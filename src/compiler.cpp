#include "crashlang/compiler.hpp"
#include "crashlang/token.hpp"

#include <iostream>

namespace crashlang {

// ── Constructor ────────────────────────────────────────────────────────────────

Compiler::Compiler(const SourceFile& source)
    : source_(source)
{}

// ── Top-level compile ──────────────────────────────────────────────────────────

Chunk Compiler::compile(const Program& program) {
    CompilerScope top_scope;
    top_scope.chunk.name = "<script>";
    current_ = &top_scope;

    for (const auto& stmt : program.statements) {
        compile_stmt(*stmt);
    }

    current_chunk().emit(OpCode::Halt, 0);
    return std::move(current_->chunk);
}

// ── Helpers ────────────────────────────────────────────────────────────────────

void Compiler::error(const std::string& message) {
    std::cerr << "Compiler error: " << message << "\n";
    had_error_ = true;
}

uint16_t Compiler::identifier_constant(const std::string& name) {
    return current_chunk().add_constant(Value(name));
}

// ── Scope management ──────────────────────────────────────────────────────────

void Compiler::begin_scope() {
    current_->scope_depth++;
}

void Compiler::end_scope(uint32_t line) {
    current_->scope_depth--;
    while (!current_->locals.empty()
           && current_->locals.back().depth > current_->scope_depth)
    {
        if (current_->locals.back().is_captured) {
            current_chunk().emit(OpCode::CloseUpvalue, line);
        } else {
            current_chunk().emit(OpCode::Pop, line);
        }
        current_->locals.pop_back();
    }
}

void Compiler::declare_local(const std::string& name) {
    // Check for redeclaration in the same scope.
    for (int i = static_cast<int>(current_->locals.size()) - 1; i >= 0; --i) {
        if (current_->locals[i].depth < current_->scope_depth) break;
        if (current_->locals[i].name == name) {
            error("variable '" + name + "' already declared in this scope");
            return;
        }
    }
    add_local(name);
}

void Compiler::mark_initialized() {
    if (!current_->locals.empty()) {
        current_->locals.back().initialized = true;
    }
}

void Compiler::add_local(const std::string& name) {
    Local local;
    local.name  = name;
    local.depth = current_->scope_depth;
    local.initialized = false;
    current_->locals.push_back(std::move(local));
}

int Compiler::resolve_local(const std::string& name) const {
    for (int i = static_cast<int>(current_->locals.size()) - 1; i >= 0; --i) {
        if (current_->locals[i].name == name) {
            return i;
        }
    }
    return -1;
}

// ── Upvalue resolution ────────────────────────────────────────────────────────

int Compiler::add_upvalue(CompilerScope* scope, uint8_t index, bool is_local) {
    // Check if we already have this upvalue.
    for (int i = 0; i < scope->upvalue_count; ++i) {
        if (scope->upvalues[i].index == index && scope->upvalues[i].is_local == is_local) {
            return i;
        }
    }
    CompilerUpvalue uv;
    uv.index = index;
    uv.is_local = is_local;
    scope->upvalues.push_back(uv);
    return scope->upvalue_count++;
}

int Compiler::resolve_upvalue(CompilerScope* scope, const std::string& name) {
    if (scope->enclosing == nullptr) return -1;

    // Check if it's a local in the enclosing scope.
    for (int i = static_cast<int>(scope->enclosing->locals.size()) - 1; i >= 0; --i) {
        if (scope->enclosing->locals[i].name == name) {
            scope->enclosing->locals[i].is_captured = true;
            return add_upvalue(scope, static_cast<uint8_t>(i), true);
        }
    }

    // Recursively check outer scopes.
    int upvalue = resolve_upvalue(scope->enclosing, name);
    if (upvalue != -1) {
        return add_upvalue(scope, static_cast<uint8_t>(upvalue), false);
    }

    return -1;
}

// ── Statement dispatch ─────────────────────────────────────────────────────────

void Compiler::compile_stmt(const Stmt& stmt) {
    std::visit([this](auto&& node) {
        using T = std::decay_t<decltype(node)>;
        if constexpr (std::is_same_v<T, ExprStmt>)      compile_expr_stmt(node);
        else if constexpr (std::is_same_v<T, LetStmt>)   compile_let(node);
        else if constexpr (std::is_same_v<T, BlockStmt>) compile_block(node);
        else if constexpr (std::is_same_v<T, IfStmt>)    compile_if(node);
        else if constexpr (std::is_same_v<T, WhileStmt>) compile_while(node);
        else if constexpr (std::is_same_v<T, ForStmt>)   compile_for(node);
        else if constexpr (std::is_same_v<T, FnStmt>)    compile_fn(node);
        else if constexpr (std::is_same_v<T, ReturnStmt>) compile_return(node);
        else if constexpr (std::is_same_v<T, FreeStmt>)  compile_free(node);
        else if constexpr (std::is_same_v<T, BreakStmt>) {
            compile_break(node);
        }
        else if constexpr (std::is_same_v<T, ContinueStmt>) {
            compile_continue(node);
        }
    }, stmt.data);
}

void Compiler::compile_expr_stmt(const ExprStmt& stmt) {
    compile_expr(*stmt.expression);
    uint32_t line = stmt.expression->span.start.line;
    current_chunk().emit(OpCode::Pop, line);
}

void Compiler::compile_let(const LetStmt& stmt) {
    uint32_t line = stmt.name.span.start.line;

    if (current_->scope_depth > 0) {
        // Local variable.
        declare_local(stmt.name.lexeme);
        if (stmt.initializer) {
            compile_expr(*stmt.initializer);
        } else {
            current_chunk().emit(OpCode::Nil, line);
        }
        mark_initialized();
    } else {
        // Global variable.
        uint16_t name_idx = identifier_constant(stmt.name.lexeme);
        if (stmt.initializer) {
            compile_expr(*stmt.initializer);
        } else {
            current_chunk().emit(OpCode::Nil, line);
        }
        current_chunk().emit_wide(OpCode::DefineGlobal, name_idx, line);
    }
}

void Compiler::compile_block(const BlockStmt& stmt) {
    begin_scope();
    for (const auto& s : stmt.statements) {
        compile_stmt(*s);
    }
    uint32_t line = 0;
    if (!stmt.statements.empty()) {
        // Use the line of the last statement for scope cleanup.
        line = stmt.statements.back()->span.start.line;
    }
    end_scope(line);
}

void Compiler::compile_if(const IfStmt& stmt) {
    uint32_t line = stmt.condition->span.start.line;

    compile_expr(*stmt.condition);
    size_t then_jump = current_chunk().emit_jump(OpCode::JumpIfFalse, line);
    current_chunk().emit(OpCode::Pop, line); // Pop condition.

    compile_stmt(*stmt.then_branch);

    if (stmt.else_branch) {
        size_t else_jump = current_chunk().emit_jump(OpCode::Jump, line);
        current_chunk().patch_jump(then_jump);
        current_chunk().emit(OpCode::Pop, line); // Pop condition.
        compile_stmt(*stmt.else_branch);
        current_chunk().patch_jump(else_jump);
    } else {
        current_chunk().patch_jump(then_jump);
        current_chunk().emit(OpCode::Pop, line); // Pop condition.
    }
}

void Compiler::compile_while(const WhileStmt& stmt) {
    uint32_t line = stmt.condition->span.start.line;

    size_t loop_start = current_chunk().current_offset();

    // Push loop context.
    loop_stack_.push_back({loop_start, {}, current_->scope_depth});

    compile_expr(*stmt.condition);
    size_t exit_jump = current_chunk().emit_jump(OpCode::JumpIfFalse, line);
    current_chunk().emit(OpCode::Pop, line);

    compile_stmt(*stmt.body);
    current_chunk().emit_loop(loop_start, line);

    current_chunk().patch_jump(exit_jump);
    current_chunk().emit(OpCode::Pop, line);

    // Patch all break jumps.
    for (size_t patch : loop_stack_.back().break_patches) {
        current_chunk().patch_jump(patch);
    }
    loop_stack_.pop_back();
}

void Compiler::compile_for(const ForStmt& stmt) {
    uint32_t line = stmt.variable.span.start.line;

    // Compile the iterable, store in a hidden local.
    begin_scope();
    compile_expr(*stmt.iterable);
    add_local("$iter");
    mark_initialized();

    // Index counter — another hidden local.
    current_chunk().emit_constant(Value(int64_t(0)), line);
    add_local("$idx");
    mark_initialized();

    int iter_slot = resolve_local("$iter");
    int idx_slot  = resolve_local("$idx");

    // Loop header: check idx < len(iter).
    size_t loop_start = current_chunk().current_offset();

    // Push idx, then compute len(iter), then compare: idx < len_result
    current_chunk().emit(OpCode::GetLocal, static_cast<uint8_t>(idx_slot), line);
    // Call len(iter) — pushes len function, then iter arg, then Call.
    uint16_t len_name = identifier_constant("len");
    current_chunk().emit_wide(OpCode::GetGlobal, len_name, line);
    current_chunk().emit(OpCode::GetLocal, static_cast<uint8_t>(iter_slot), line);
    current_chunk().emit(OpCode::Call, 1, line);
    // Stack: ... idx len_result
    current_chunk().emit(OpCode::Less, line);

    size_t exit_jump = current_chunk().emit_jump(OpCode::JumpIfFalse, line);
    current_chunk().emit(OpCode::Pop, line);

    // Get current element: iter[idx].
    begin_scope();
    current_chunk().emit(OpCode::GetLocal, static_cast<uint8_t>(iter_slot), line);
    current_chunk().emit(OpCode::GetLocal, static_cast<uint8_t>(idx_slot), line);
    current_chunk().emit(OpCode::Index, line);
    add_local(stmt.variable.lexeme);
    mark_initialized();

    // Compile body.
    compile_stmt(*stmt.body);

    end_scope(line);

    // Push loop context.
    loop_stack_.push_back({loop_start, {}, current_->scope_depth});

    // Increment idx.
    current_chunk().emit(OpCode::GetLocal, static_cast<uint8_t>(idx_slot), line);
    current_chunk().emit_constant(Value(int64_t(1)), line);
    current_chunk().emit(OpCode::Add, line);
    current_chunk().emit(OpCode::SetLocal, static_cast<uint8_t>(idx_slot), line);
    current_chunk().emit(OpCode::Pop, line);

    current_chunk().emit_loop(loop_start, line);

    current_chunk().patch_jump(exit_jump);
    current_chunk().emit(OpCode::Pop, line);

    // Patch all break jumps.
    for (size_t patch : loop_stack_.back().break_patches) {
        current_chunk().patch_jump(patch);
    }
    loop_stack_.pop_back();

    end_scope(line);
}

void Compiler::compile_fn(const FnStmt& stmt) {
    uint32_t line = stmt.name.span.start.line;

    // compile_function_body emits OP_CLOSURE which pushes the closure to the stack.
    compile_function_body(stmt.name.lexeme, stmt.params, *stmt.body);

    if (current_->scope_depth > 0) {
        declare_local(stmt.name.lexeme);
        mark_initialized();
    } else {
        uint16_t name_idx = identifier_constant(stmt.name.lexeme);
        current_chunk().emit_wide(OpCode::DefineGlobal, name_idx, line);
    }
}

void Compiler::compile_return(const ReturnStmt& stmt) {
    uint32_t line = stmt.keyword.span.start.line;
    if (stmt.value) {
        compile_expr(*stmt.value);
    } else {
        current_chunk().emit(OpCode::Nil, line);
    }
    current_chunk().emit(OpCode::Return, line);
}

void Compiler::compile_free(const FreeStmt& stmt) {
    uint32_t line = stmt.keyword.span.start.line;
    compile_expr(*stmt.argument);
    current_chunk().emit(OpCode::Free, line);
}

void Compiler::compile_break(const BreakStmt& stmt) {
    uint32_t line = stmt.keyword.span.start.line;
    if (loop_stack_.empty()) {
        error("'break' outside of loop");
        return;
    }
    // Emit a jump placeholder — patched at loop end.
    size_t jump = current_chunk().emit_jump(OpCode::Jump, line);
    loop_stack_.back().break_patches.push_back(jump);
}

void Compiler::compile_continue(const ContinueStmt& stmt) {
    uint32_t line = stmt.keyword.span.start.line;
    if (loop_stack_.empty()) {
        error("'continue' outside of loop");
        return;
    }
    // Jump back to loop start.
    current_chunk().emit_loop(loop_stack_.back().start_offset, line);
}

Value Compiler::compile_function_body(const std::string& name,
                                       const std::vector<Token>& params,
                                       const Stmt& body)
{
    CompilerScope fn_scope;
    fn_scope.chunk.name = name;
    fn_scope.scope_depth = 0;
    fn_scope.enclosing = current_;
    current_ = &fn_scope;

    begin_scope();

    // Slot 0 is reserved for the callee (function value itself on the VM stack).
    add_local("");
    mark_initialized();

    // Declare parameters as locals (starting at slot 1).
    for (const auto& param : params) {
        declare_local(param.lexeme);
        mark_initialized();
    }

    // Compile the body — function bodies are always BlockStmt.
    if (auto* block = std::get_if<BlockStmt>(&body.data)) {
        for (const auto& s : block->statements) {
            compile_stmt(*s);
        }
    }

    // Implicit return nil if no explicit return.
    current_chunk().emit(OpCode::Nil, 0);
    current_chunk().emit(OpCode::Return, 0);

    Chunk compiled_chunk = std::move(fn_scope.chunk);
    current_ = fn_scope.enclosing;

    // Create a FunctionValue holding the compiled chunk.
    FunctionValue fn;
    fn.name = name;
    for (const auto& p : params) {
        fn.params.push_back(p.lexeme);
    }
    fn.body = nullptr; // VM functions use compiled_chunk, not AST body.
    fn.closure = nullptr;

    // Store the chunk in the constants pool of the FunctionValue.
    fn.compiled = std::make_shared<Chunk>(std::move(compiled_chunk));

    Value fn_val(std::move(fn));

    // Emit OP_CLOSURE: push the function constant, then emit upvalue descriptors.
    uint16_t fn_idx = current_chunk().add_constant(std::move(fn_val));
    current_chunk().emit_wide(OpCode::Closure, fn_idx, 0);
    // Emit upvalue count + descriptor pairs.
    current_chunk().code.push_back(static_cast<uint8_t>(fn_scope.upvalue_count));
    current_chunk().lines.push_back(0);
    for (int i = 0; i < fn_scope.upvalue_count; ++i) {
        current_chunk().code.push_back(fn_scope.upvalues[i].is_local ? 1 : 0);
        current_chunk().lines.push_back(0);
        current_chunk().code.push_back(fn_scope.upvalues[i].index);
        current_chunk().lines.push_back(0);
    }

    // Return a placeholder — the actual value is on the stack via OP_CLOSURE.
    return Value(NilType{});
}

// ── Expression dispatch ────────────────────────────────────────────────────────

void Compiler::compile_expr(const Expr& expr) {
    std::visit([this, &expr](auto&& node) {
        using T = std::decay_t<decltype(node)>;
        auto& span = expr.span;
        if constexpr (std::is_same_v<T, LiteralExpr>)       compile_literal(node, span);
        else if constexpr (std::is_same_v<T, IdentifierExpr>) compile_identifier(node, span);
        else if constexpr (std::is_same_v<T, UnaryExpr>)     compile_unary(node, span);
        else if constexpr (std::is_same_v<T, BinaryExpr>)    compile_binary(node, span);
        else if constexpr (std::is_same_v<T, CallExpr>)      compile_call(node, span);
        else if constexpr (std::is_same_v<T, IndexExpr>)     compile_index(node, span);
        else if constexpr (std::is_same_v<T, FieldAccessExpr>) compile_field_access(node, span);
        else if constexpr (std::is_same_v<T, AssignExpr>)    compile_assign(node, span);
        else if constexpr (std::is_same_v<T, ArrayExpr>)     compile_array(node, span);
        else if constexpr (std::is_same_v<T, NewExpr>)       compile_new(node, span);
        else if constexpr (std::is_same_v<T, RefExpr>)       compile_ref(node, span);
        else if constexpr (std::is_same_v<T, DerefExpr>)     compile_deref(node, span);
        else if constexpr (std::is_same_v<T, MoveExpr>)      compile_move(node, span);
        else if constexpr (std::is_same_v<T, LambdaExpr>)    compile_lambda(node, span);
        else if constexpr (std::is_same_v<T, MatchExpr>)     compile_match(node, span);
        else error("unsupported expression type in compiler");
    }, expr.data);
}

void Compiler::compile_literal(const LiteralExpr& expr, const Span& span) {
    uint32_t line = span.start.line;
    switch (expr.value.type) {
        case TokenType::Nil:
            current_chunk().emit(OpCode::Nil, line);
            break;
        case TokenType::True:
            current_chunk().emit(OpCode::True, line);
            break;
        case TokenType::False:
            current_chunk().emit(OpCode::False, line);
            break;
        case TokenType::IntLiteral: {
            int64_t val = std::stoll(expr.value.lexeme);
            current_chunk().emit_constant(Value(val), line);
            break;
        }
        case TokenType::FloatLiteral: {
            double val = std::stod(expr.value.lexeme);
            current_chunk().emit_constant(Value(val), line);
            break;
        }
        case TokenType::StringLiteral: {
            // Strip surrounding quotes and process escapes.
            std::string raw = expr.value.lexeme;
            // The lexer stores the raw lexeme including quotes.
            if (raw.size() >= 2 && raw.front() == '"' && raw.back() == '"') {
                raw = raw.substr(1, raw.size() - 2);
            }
            // Process escape sequences.
            std::string processed;
            for (size_t i = 0; i < raw.size(); ++i) {
                if (raw[i] == '\\' && i + 1 < raw.size()) {
                    switch (raw[i + 1]) {
                        case 'n':  processed += '\n'; ++i; break;
                        case 't':  processed += '\t'; ++i; break;
                        case 'r':  processed += '\r'; ++i; break;
                        case '\\': processed += '\\'; ++i; break;
                        case '"':  processed += '"';  ++i; break;
                        case '0':  processed += '\0'; ++i; break;
                        default:   processed += raw[i]; break;
                    }
                } else {
                    processed += raw[i];
                }
            }
            current_chunk().emit_constant(Value(std::move(processed)), line);
            break;
        }
        default:
            error("unexpected literal type");
            break;
    }
}

void Compiler::compile_identifier(const IdentifierExpr& expr, const Span& span) {
    uint32_t line = span.start.line;
    int slot = resolve_local(expr.name.lexeme);
    if (slot != -1) {
        current_chunk().emit(OpCode::GetLocal, static_cast<uint8_t>(slot), line);
    } else {
        int upvalue = resolve_upvalue(current_, expr.name.lexeme);
        if (upvalue != -1) {
            current_chunk().emit(OpCode::GetUpvalue, static_cast<uint8_t>(upvalue), line);
        } else {
            uint16_t idx = identifier_constant(expr.name.lexeme);
            current_chunk().emit_wide(OpCode::GetGlobal, idx, line);
        }
    }
}

void Compiler::compile_unary(const UnaryExpr& expr, const Span& span) {
    uint32_t line = span.start.line;
    compile_expr(*expr.operand);
    if (expr.op.type == TokenType::Minus) {
        current_chunk().emit(OpCode::Negate, line);
    } else if (expr.op.type == TokenType::Not) {
        current_chunk().emit(OpCode::Not, line);
    }
}

void Compiler::compile_binary(const BinaryExpr& expr, const Span& span) {
    uint32_t line = span.start.line;

    // Short-circuit: 'and' / 'or' compile to jumps.
    if (expr.op.type == TokenType::And) {
        compile_expr(*expr.left);
        size_t end_jump = current_chunk().emit_jump(OpCode::JumpIfFalse, line);
        current_chunk().emit(OpCode::Pop, line);
        compile_expr(*expr.right);
        current_chunk().patch_jump(end_jump);
        return;
    }
    if (expr.op.type == TokenType::Or) {
        compile_expr(*expr.left);
        size_t else_jump = current_chunk().emit_jump(OpCode::JumpIfFalse, line);
        size_t end_jump  = current_chunk().emit_jump(OpCode::Jump, line);
        current_chunk().patch_jump(else_jump);
        current_chunk().emit(OpCode::Pop, line);
        compile_expr(*expr.right);
        current_chunk().patch_jump(end_jump);
        return;
    }

    compile_expr(*expr.left);
    compile_expr(*expr.right);

    switch (expr.op.type) {
        case TokenType::Plus:    current_chunk().emit(OpCode::Add, line); break;
        case TokenType::Minus:   current_chunk().emit(OpCode::Subtract, line); break;
        case TokenType::Star:    current_chunk().emit(OpCode::Multiply, line); break;
        case TokenType::Slash:   current_chunk().emit(OpCode::Divide, line); break;
        case TokenType::Percent: current_chunk().emit(OpCode::Modulo, line); break;
        case TokenType::EqEq:    current_chunk().emit(OpCode::Equal, line); break;
        case TokenType::BangEq:  current_chunk().emit(OpCode::NotEqual, line); break;
        case TokenType::Lt:      current_chunk().emit(OpCode::Less, line); break;
        case TokenType::LtEq:    current_chunk().emit(OpCode::LessEqual, line); break;
        case TokenType::Gt:      current_chunk().emit(OpCode::Greater, line); break;
        case TokenType::GtEq:    current_chunk().emit(OpCode::GreaterEqual, line); break;
        default:
            error("unsupported binary operator: " + expr.op.lexeme);
            break;
    }
}

void Compiler::compile_call(const CallExpr& expr, const Span& span) {
    uint32_t line = span.start.line;

    // Special-case: println/print calls compile to dedicated opcodes.
    bool is_print = false;
    bool is_println = false;
    if (auto* id = std::get_if<IdentifierExpr>(&expr.callee->data)) {
        if (id->name.lexeme == "print")   is_print = true;
        if (id->name.lexeme == "println") is_println = true;
    }

    if (is_print || is_println) {
        for (const auto& arg : expr.arguments) {
            compile_expr(*arg);
        }
        uint8_t argc = static_cast<uint8_t>(expr.arguments.size());
        if (is_println) {
            current_chunk().emit(OpCode::Println, argc, line);
        } else {
            current_chunk().emit(OpCode::Print, argc, line);
        }
        // print/println return nil — push it.
        current_chunk().emit(OpCode::Nil, line);
        return;
    }

    // General function call.
    compile_expr(*expr.callee);
    for (const auto& arg : expr.arguments) {
        compile_expr(*arg);
    }
    uint8_t argc = static_cast<uint8_t>(expr.arguments.size());
    current_chunk().emit(OpCode::Call, argc, line);
}

void Compiler::compile_index(const IndexExpr& expr, const Span& span) {
    uint32_t line = span.start.line;
    compile_expr(*expr.object);
    compile_expr(*expr.index);
    current_chunk().emit(OpCode::Index, line);
}

void Compiler::compile_field_access(const FieldAccessExpr& expr, const Span& span) {
    uint32_t line = span.start.line;
    compile_expr(*expr.object);
    uint16_t name_idx = identifier_constant(expr.name.lexeme);
    current_chunk().emit_wide(OpCode::GetField, name_idx, line);
}

void Compiler::compile_assign(const AssignExpr& expr, const Span& span) {
    uint32_t line = span.start.line;
    compile_expr(*expr.value);

    // Determine the assignment target.
    if (auto* id = std::get_if<IdentifierExpr>(&expr.target->data)) {
        int slot = resolve_local(id->name.lexeme);
        if (slot != -1) {
            current_chunk().emit(OpCode::SetLocal, static_cast<uint8_t>(slot), line);
        } else {
            int upvalue = resolve_upvalue(current_, id->name.lexeme);
            if (upvalue != -1) {
                current_chunk().emit(OpCode::SetUpvalue, static_cast<uint8_t>(upvalue), line);
            } else {
                uint16_t idx = identifier_constant(id->name.lexeme);
                current_chunk().emit_wide(OpCode::SetGlobal, idx, line);
            }
        }
    } else if (auto* field = std::get_if<FieldAccessExpr>(&expr.target->data)) {
        compile_expr(*field->object);
        uint16_t name_idx = identifier_constant(field->name.lexeme);
        current_chunk().emit_wide(OpCode::SetField, name_idx, line);
    } else if (std::holds_alternative<IndexExpr>(expr.target->data)) {
        auto& idx_expr = std::get<IndexExpr>(expr.target->data);
        compile_expr(*idx_expr.object);
        compile_expr(*idx_expr.index);
        current_chunk().emit(OpCode::SetIndex, line);
    } else {
        error("invalid assignment target");
    }
}

void Compiler::compile_array(const ArrayExpr& expr, const Span& span) {
    uint32_t line = span.start.line;
    for (const auto& elem : expr.elements) {
        compile_expr(*elem);
    }
    current_chunk().emit_wide(OpCode::Array,
        static_cast<uint16_t>(expr.elements.size()), line);
}

void Compiler::compile_new(const NewExpr& expr, const Span& span) {
    uint32_t line = span.start.line;
    // Push field values, then field names as constants.
    for (const auto& [name_tok, value_expr] : expr.fields) {
        compile_expr(*value_expr);
    }
    // Push field names.
    for (const auto& [name_tok, value_expr] : expr.fields) {
        current_chunk().emit_constant(Value(name_tok.lexeme), line);
    }
    uint16_t type_idx = identifier_constant(expr.type_name.lexeme);
    current_chunk().emit_wide(OpCode::NewStruct, type_idx, line);
    current_chunk().code.push_back(static_cast<uint8_t>(expr.fields.size()));
    current_chunk().lines.push_back(line);
}

void Compiler::compile_ref(const RefExpr& expr, const Span& span) {
    uint32_t line = span.start.line;
    compile_expr(*expr.operand);
    current_chunk().emit(OpCode::Ref, line);
}

void Compiler::compile_deref(const DerefExpr& expr, const Span& span) {
    uint32_t line = span.start.line;
    compile_expr(*expr.operand);
    current_chunk().emit(OpCode::Deref, line);
}

void Compiler::compile_move(const MoveExpr& expr, const Span& span) {
    uint32_t line = span.start.line;
    compile_expr(*expr.operand);
    current_chunk().emit(OpCode::Move, line);
}

void Compiler::compile_lambda(const LambdaExpr& expr, const Span& span) {
    // compile_function_body emits OP_CLOSURE which pushes the closure to the stack.
    compile_function_body("<lambda>", expr.params, *expr.body);
}

void Compiler::compile_match(const MatchExpr& expr, const Span& span) {
    uint32_t line = span.start.line;

    // Compile the target expression (the value being matched).
    compile_expr(*expr.target);

    std::vector<size_t> end_jumps;

    for (size_t i = 0; i < expr.arms.size(); ++i) {
        const auto& arm = expr.arms[i];

        if (arm.is_wildcard) {
            // Wildcard: pop the target value, evaluate the body.
            current_chunk().emit(OpCode::Pop, line); // Pop the match target.
            compile_expr(*arm.body);
            break;
        }

        // Duplicate the match target for comparison (it stays on stack for next arm).
        // We emit GetLocal for the TOS. But actually, we can't do that easily.
        // Instead, re-evaluate isn't possible, so we use a different approach:
        // compare and jump.

        // Duplicate TOS: push the same value again.
        // We don't have a DUP opcode, so we emit a temporary local pattern:
        // The match target is at TOS. We need it for the next comparison too.
        // Approach: store in a temporary local on the first arm, then use GetLocal.
        if (i == 0) {
            // Store match target as a hidden local.
            add_local("$match");
            mark_initialized();
        }

        // Push the match target again for comparison.
        int match_slot = resolve_local("$match");
        current_chunk().emit(OpCode::GetLocal, static_cast<uint8_t>(match_slot), line);

        // Push the pattern value.
        compile_expr(*arm.pattern);

        // Compare.
        current_chunk().emit(OpCode::Equal, line);

        // If not equal, jump to next arm.
        size_t next_arm = current_chunk().emit_jump(OpCode::JumpIfFalse, line);
        current_chunk().emit(OpCode::Pop, line); // Pop comparison result (true).

        // Match! Compile the body expression.
        compile_expr(*arm.body);

        // Jump to end of match.
        end_jumps.push_back(current_chunk().emit_jump(OpCode::Jump, line));

        // Patch the "not equal" jump.
        current_chunk().patch_jump(next_arm);
        current_chunk().emit(OpCode::Pop, line); // Pop comparison result (false).
    }

    // If no arm matched and no wildcard, push nil.
    bool has_wildcard = !expr.arms.empty() && expr.arms.back().is_wildcard;
    if (!has_wildcard) {
        // Pop the hidden $match local.
        current_chunk().emit(OpCode::Nil, line);
    }

    // Patch all end jumps to here.
    for (size_t jump : end_jumps) {
        current_chunk().patch_jump(jump);
    }

    // The hidden $match local will be cleaned up by end_scope.
    // But since we're in an expression context, we need to swap:
    // The result is on TOS, and $match is below it.
    // We'll handle this by using a scope around the match.
}

} // namespace crashlang
