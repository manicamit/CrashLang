#ifndef CRASHLANG_COMPILER_HPP
#define CRASHLANG_COMPILER_HPP

#include "crashlang/ast.hpp"
#include "crashlang/bytecode.hpp"
#include "crashlang/source.hpp"

#include <string>
#include <vector>
#include <memory>

namespace crashlang {

struct Local {
    std::string name;
    int         depth;
    bool        initialized = false;
    bool        is_captured = false;
};

struct CompilerUpvalue {
    uint8_t index;
    bool    is_local;
};

struct LoopContext {
    size_t              start_offset;
    std::vector<size_t> break_patches;
    int                 scope_depth;
};

struct CompilerScope {
    Chunk                        chunk;
    std::vector<Local>           locals;
    std::vector<CompilerUpvalue> upvalues;
    int                          scope_depth = 0;
    int                          upvalue_count = 0;
    CompilerScope*               enclosing = nullptr;
};

class Compiler {
public:
    explicit Compiler(const SourceFile& source);

    Chunk compile(const Program& program);
    bool had_errors() const { return had_error_; }

private:
    const SourceFile&          source_;
    CompilerScope*             current_ = nullptr;
    bool                       had_error_ = false;
    std::vector<LoopContext>   loop_stack_;

    void begin_scope();
    void end_scope(uint32_t line);
    void declare_local(const std::string& name);
    void mark_initialized();
    int resolve_local(const std::string& name) const;
    void add_local(const std::string& name);

    int resolve_upvalue(CompilerScope* scope, const std::string& name);
    int add_upvalue(CompilerScope* scope, uint8_t index, bool is_local);

    void compile_stmt(const Stmt& stmt);
    void compile_block(const BlockStmt& stmt);
    void compile_let(const LetStmt& stmt);
    void compile_if(const IfStmt& stmt);
    void compile_while(const WhileStmt& stmt);
    void compile_for(const ForStmt& stmt);
    void compile_fn(const FnStmt& stmt);
    void compile_return(const ReturnStmt& stmt);
    void compile_free(const FreeStmt& stmt);
    void compile_expr_stmt(const ExprStmt& stmt);
    void compile_break(const BreakStmt& stmt);
    void compile_continue(const ContinueStmt& stmt);

    void compile_expr(const Expr& expr);
    void compile_literal(const LiteralExpr& expr, const Span& span);
    void compile_identifier(const IdentifierExpr& expr, const Span& span);
    void compile_unary(const UnaryExpr& expr, const Span& span);
    void compile_binary(const BinaryExpr& expr, const Span& span);
    void compile_call(const CallExpr& expr, const Span& span);
    void compile_index(const IndexExpr& expr, const Span& span);
    void compile_field_access(const FieldAccessExpr& expr, const Span& span);
    void compile_assign(const AssignExpr& expr, const Span& span);
    void compile_array(const ArrayExpr& expr, const Span& span);
    void compile_new(const NewExpr& expr, const Span& span);
    void compile_ref(const RefExpr& expr, const Span& span);
    void compile_deref(const DerefExpr& expr, const Span& span);
    void compile_move(const MoveExpr& expr, const Span& span);
    void compile_lambda(const LambdaExpr& expr, const Span& span);
    void compile_match(const MatchExpr& expr, const Span& span);

    Value compile_function_body(const std::string& name,
                                const std::vector<Token>& params,
                                const Stmt& body);

    Chunk& current_chunk() { return current_->chunk; }
    void error(const std::string& message);
    uint16_t identifier_constant(const std::string& name);
};

} // namespace crashlang

#endif
