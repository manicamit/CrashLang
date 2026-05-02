#ifndef CRASHLANG_IR_BUILDER_HPP
#define CRASHLANG_IR_BUILDER_HPP

#include "crashlang/ast.hpp"
#include "crashlang/ir.hpp"

#include <string>
#include <unordered_map>

namespace crashlang {

class IRBuilder {
public:
    IRModule build(const Program& program);

private:
    IRModule  module_;
    IRFunction* current_fn_ = nullptr;
    uint32_t    current_bb_ = 0;

    // variable name -> register mapping, per function
    std::unordered_map<std::string, Reg> locals_;

    void emit(IRInstr instr);
    BasicBlock& block() { return current_fn_->blocks[current_bb_]; }
    void set_block(uint32_t id) { current_bb_ = id; }
    Reg alloc() { return current_fn_->alloc_reg(); }

    void lower_function(const FnStmt& fn);
    void lower_stmt(const Stmt& stmt);
    void lower_block(const BlockStmt& stmt);
    void lower_let(const LetStmt& stmt);
    void lower_if(const IfStmt& stmt);
    void lower_while(const WhileStmt& stmt);
    void lower_return(const ReturnStmt& stmt);
    void lower_expr_stmt(const ExprStmt& stmt);

    Reg lower_expr(const Expr& expr);
    Reg lower_literal(const LiteralExpr& expr);
    Reg lower_identifier(const IdentifierExpr& expr);
    Reg lower_unary(const UnaryExpr& expr);
    Reg lower_binary(const BinaryExpr& expr);
    Reg lower_call(const CallExpr& expr);
    Reg lower_assign(const AssignExpr& expr);
};

} // namespace crashlang

#endif
