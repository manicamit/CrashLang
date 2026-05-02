#include "crashlang/ir_builder.hpp"
#include "crashlang/token.hpp"

namespace crashlang {

void IRBuilder::emit(IRInstr instr) {
    block().instrs.push_back(std::move(instr));
}

IRModule IRBuilder::build(const Program& program) {
    module_ = IRModule{};

    for (const auto& stmt : program.statements) {
        if (auto* fn = std::get_if<FnStmt>(&stmt->data)) {
            lower_function(*fn);
        }
    }

    for (auto& fn : module_.functions) {
        fn.compute_predecessors();
    }
    return std::move(module_);
}

void IRBuilder::lower_function(const FnStmt& fn) {
    module_.functions.emplace_back();
    current_fn_ = &module_.functions.back();
    current_fn_->name = fn.name.lexeme;
    locals_.clear();

    // Set up parameters as registers.
    for (size_t i = 0; i < fn.params.size(); ++i) {
        Reg r = alloc();
        current_fn_->param_names.push_back(fn.params[i].lexeme);
        current_fn_->param_types.push_back(IRType::I64);
        locals_[fn.params[i].lexeme] = r;
    }

    current_fn_->return_type = IRType::I64;
    current_bb_ = current_fn_->add_block("entry");

    // Emit load.arg for each parameter.
    for (size_t i = 0; i < fn.params.size(); ++i) {
        IRInstr load;
        load.op   = IROp::LoadParam;
        load.dst  = static_cast<Reg>(i);
        load.src1 = static_cast<Reg>(i);
        load.type = IRType::I64;
        emit(load);
    }

    if (fn.body) {
        lower_stmt(*fn.body);
    }

    // Ensure every block has a terminator.
    for (auto& bb : current_fn_->blocks) {
        if (bb.instrs.empty() || (bb.instrs.back().op != IROp::Ret &&
            bb.instrs.back().op != IROp::Br && bb.instrs.back().op != IROp::BrIf)) {
            IRInstr ret;
            ret.op = IROp::Ret;
            bb.instrs.push_back(ret);
        }
    }
}

void IRBuilder::lower_stmt(const Stmt& stmt) {
    std::visit([&](auto&& node) {
        using T = std::decay_t<decltype(node)>;
        if constexpr (std::is_same_v<T, ExprStmt>)     lower_expr_stmt(node);
        else if constexpr (std::is_same_v<T, LetStmt>)  lower_let(node);
        else if constexpr (std::is_same_v<T, BlockStmt>) lower_block(node);
        else if constexpr (std::is_same_v<T, IfStmt>)   lower_if(node);
        else if constexpr (std::is_same_v<T, WhileStmt>) lower_while(node);
        else if constexpr (std::is_same_v<T, ReturnStmt>) lower_return(node);
    }, stmt.data);
}

void IRBuilder::lower_block(const BlockStmt& stmt) {
    for (const auto& s : stmt.statements) {
        lower_stmt(*s);
    }
}

void IRBuilder::lower_let(const LetStmt& stmt) {
    Reg r;
    if (stmt.initializer) {
        r = lower_expr(*stmt.initializer);
    } else {
        r = alloc();
        IRInstr load;
        load.op   = IROp::LoadConst;
        load.dst  = r;
        load.type = IRType::I64;
        load.imm  = int64_t(0);
        emit(load);
    }
    locals_[stmt.name.lexeme] = r;
}

void IRBuilder::lower_if(const IfStmt& stmt) {
    Reg cond = lower_expr(*stmt.condition);

    uint32_t then_bb = current_fn_->add_block("if.then");
    uint32_t else_bb = stmt.else_branch
        ? current_fn_->add_block("if.else") : 0;
    uint32_t merge_bb = current_fn_->add_block("if.end");

    if (!else_bb) else_bb = merge_bb;

    IRInstr br;
    br.op = IROp::BrIf;
    br.src1 = cond;
    br.branch_target = then_bb;
    br.src2 = else_bb;
    emit(br);
    block().successors = {then_bb, else_bb};

    // Then block.
    set_block(then_bb);
    lower_stmt(*stmt.then_branch);
    if (block().instrs.empty() || block().instrs.back().op != IROp::Ret) {
        IRInstr jump;
        jump.op = IROp::Br;
        jump.branch_target = merge_bb;
        emit(jump);
        block().successors = {merge_bb};
    }

    // Else block (if present).
    if (stmt.else_branch) {
        set_block(else_bb);
        lower_stmt(*stmt.else_branch);
        if (block().instrs.empty() || block().instrs.back().op != IROp::Ret) {
            IRInstr jump;
            jump.op = IROp::Br;
            jump.branch_target = merge_bb;
            emit(jump);
            block().successors = {merge_bb};
        }
    }

    set_block(merge_bb);
}

void IRBuilder::lower_while(const WhileStmt& stmt) {
    uint32_t cond_bb = current_fn_->add_block("while.cond");
    uint32_t body_bb = current_fn_->add_block("while.body");
    uint32_t exit_bb = current_fn_->add_block("while.end");

    IRInstr jump;
    jump.op = IROp::Br;
    jump.branch_target = cond_bb;
    emit(jump);
    block().successors = {cond_bb};

    // Condition block.
    set_block(cond_bb);
    Reg cond = lower_expr(*stmt.condition);
    IRInstr br;
    br.op = IROp::BrIf;
    br.src1 = cond;
    br.branch_target = body_bb;
    br.src2 = exit_bb;
    emit(br);
    block().successors = {body_bb, exit_bb};

    // Body block.
    set_block(body_bb);
    lower_stmt(*stmt.body);
    if (block().instrs.empty() || block().instrs.back().op != IROp::Ret) {
        IRInstr back;
        back.op = IROp::Br;
        back.branch_target = cond_bb;
        emit(back);
        block().successors = {cond_bb};
    }

    set_block(exit_bb);
}

void IRBuilder::lower_return(const ReturnStmt& stmt) {
    IRInstr ret;
    ret.op = IROp::Ret;
    if (stmt.value) {
        ret.src1 = lower_expr(*stmt.value);
    }
    emit(ret);
}

void IRBuilder::lower_expr_stmt(const ExprStmt& stmt) {
    lower_expr(*stmt.expression);
}

Reg IRBuilder::lower_expr(const Expr& expr) {
    return std::visit([&](auto&& node) -> Reg {
        using T = std::decay_t<decltype(node)>;
        if constexpr (std::is_same_v<T, LiteralExpr>)    return lower_literal(node);
        else if constexpr (std::is_same_v<T, IdentifierExpr>) return lower_identifier(node);
        else if constexpr (std::is_same_v<T, UnaryExpr>)  return lower_unary(node);
        else if constexpr (std::is_same_v<T, BinaryExpr>) return lower_binary(node);
        else if constexpr (std::is_same_v<T, CallExpr>)   return lower_call(node);
        else if constexpr (std::is_same_v<T, AssignExpr>) return lower_assign(node);
        else {
            // Unsupported expression types lower to a zero constant.
            Reg r = alloc();
            IRInstr load;
            load.op = IROp::LoadConst;
            load.dst = r;
            load.type = IRType::I64;
            load.imm = int64_t(0);
            emit(load);
            return r;
        }
    }, expr.data);
}

Reg IRBuilder::lower_literal(const LiteralExpr& expr) {
    Reg r = alloc();
    IRInstr load;
    load.op  = IROp::LoadConst;
    load.dst = r;

    switch (expr.value.type) {
    case TokenType::IntLiteral:
        load.type = IRType::I64;
        load.imm = std::get<int64_t>(expr.value.literal);
        break;
    case TokenType::FloatLiteral:
        load.type = IRType::F64;
        load.imm = std::get<double>(expr.value.literal);
        break;
    case TokenType::True:
        load.type = IRType::Bool;
        load.imm = true;
        break;
    case TokenType::False:
        load.type = IRType::Bool;
        load.imm = false;
        break;
    case TokenType::StringLiteral:
        load.type = IRType::Str;
        load.imm = std::get<std::string>(expr.value.literal);
        break;
    default:
        load.type = IRType::I64;
        load.imm = int64_t(0);
        break;
    }

    emit(load);
    return r;
}

Reg IRBuilder::lower_identifier(const IdentifierExpr& expr) {
    auto it = locals_.find(expr.name.lexeme);
    if (it != locals_.end()) return it->second;

    // Unknown variable — emit a zero constant as fallback.
    Reg r = alloc();
    IRInstr load;
    load.op = IROp::LoadConst;
    load.dst = r;
    load.type = IRType::I64;
    load.imm = int64_t(0);
    emit(load);
    return r;
}

Reg IRBuilder::lower_unary(const UnaryExpr& expr) {
    Reg operand = lower_expr(*expr.operand);
    Reg r = alloc();

    IRInstr instr;
    instr.dst  = r;
    instr.src1 = operand;

    switch (expr.op.type) {
    case TokenType::Minus: instr.op = IROp::INeg; break;
    case TokenType::Not:   instr.op = IROp::Not;  break;
    default:               instr.op = IROp::INeg; break;
    }

    emit(instr);
    return r;
}

Reg IRBuilder::lower_binary(const BinaryExpr& expr) {
    // Short-circuit and/or: lower to branches.
    if (expr.op.type == TokenType::And || expr.op.type == TokenType::Or) {
        Reg left = lower_expr(*expr.left);
        Reg result = alloc();

        uint32_t rhs_bb = current_fn_->add_block(
            expr.op.type == TokenType::And ? "and.rhs" : "or.rhs");
        uint32_t merge_bb = current_fn_->add_block("logic.end");

        // Copy left to result.
        IRInstr copy;
        copy.op = IROp::Copy;
        copy.dst = result;
        copy.src1 = left;
        emit(copy);

        IRInstr br;
        br.op = IROp::BrIf;
        br.src1 = left;
        if (expr.op.type == TokenType::And) {
            br.branch_target = rhs_bb;
            br.src2 = merge_bb;
        } else {
            br.branch_target = merge_bb;
            br.src2 = rhs_bb;
        }
        emit(br);
        block().successors = {rhs_bb, merge_bb};

        set_block(rhs_bb);
        Reg right = lower_expr(*expr.right);
        IRInstr copy2;
        copy2.op = IROp::Copy;
        copy2.dst = result;
        copy2.src1 = right;
        emit(copy2);

        IRInstr jump;
        jump.op = IROp::Br;
        jump.branch_target = merge_bb;
        emit(jump);
        block().successors = {merge_bb};

        set_block(merge_bb);
        return result;
    }

    Reg left  = lower_expr(*expr.left);
    Reg right = lower_expr(*expr.right);
    Reg r = alloc();

    IRInstr instr;
    instr.dst  = r;
    instr.src1 = left;
    instr.src2 = right;

    switch (expr.op.type) {
    case TokenType::Plus:    instr.op = IROp::IAdd; break;
    case TokenType::Minus:   instr.op = IROp::ISub; break;
    case TokenType::Star:    instr.op = IROp::IMul; break;
    case TokenType::Slash:   instr.op = IROp::IDiv; break;
    case TokenType::Percent: instr.op = IROp::IMod; break;
    case TokenType::EqEq:    instr.op = IROp::Eq;   break;
    case TokenType::BangEq:  instr.op = IROp::Neq;  break;
    case TokenType::Lt:      instr.op = IROp::Lt;   break;
    case TokenType::LtEq:    instr.op = IROp::Le;   break;
    case TokenType::Gt:      instr.op = IROp::Gt;   break;
    case TokenType::GtEq:    instr.op = IROp::Ge;   break;
    default:                 instr.op = IROp::IAdd;  break;
    }

    emit(instr);
    return r;
}

Reg IRBuilder::lower_call(const CallExpr& expr) {
    std::string target = "unknown";
    if (auto* id = std::get_if<IdentifierExpr>(&expr.callee->data)) {
        target = id->name.lexeme;
    }

    std::vector<Reg> arg_regs;
    for (const auto& arg : expr.arguments) {
        arg_regs.push_back(lower_expr(*arg));
    }

    Reg dst = alloc();
    IRInstr call;
    call.op = IROp::Call;
    call.dst = dst;
    call.call_target = target;
    call.call_argc = static_cast<uint32_t>(arg_regs.size());
    call.src1 = arg_regs.empty() ? NO_REG : arg_regs[0];
    emit(call);
    return dst;
}

Reg IRBuilder::lower_assign(const AssignExpr& expr) {
    Reg val = lower_expr(*expr.value);
    if (auto* id = std::get_if<IdentifierExpr>(&expr.target->data)) {
        locals_[id->name.lexeme] = val;
    }
    return val;
}

} // namespace crashlang
