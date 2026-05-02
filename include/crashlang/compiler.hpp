#ifndef CRASHLANG_COMPILER_HPP
#define CRASHLANG_COMPILER_HPP

/// AST-to-bytecode compiler for CrashLang.
///
/// Walks the AST produced by the parser and emits bytecode instructions
/// into Chunk objects. Each function body compiles to its own Chunk.
/// Local variable resolution is done at compile time using a scope stack,
/// so the VM can use slot-indexed access instead of name lookups.
/// Upvalue resolution enables closures to capture variables from enclosing
/// scopes via heap-promoted upvalue slots.

#include "crashlang/ast.hpp"
#include "crashlang/bytecode.hpp"
#include "crashlang/source.hpp"

#include <string>
#include <vector>
#include <memory>

namespace crashlang {

/// A local variable tracked during compilation.
struct Local {
    std::string name;
    int         depth;    // Scope depth where this local was declared.
    bool        initialized = false;
    bool        is_captured = false; // True if an inner closure captures this local.
};

/// An upvalue descriptor — identifies a captured variable.
struct CompilerUpvalue {
    uint8_t index;    // Slot in the enclosing scope (local or upvalue index).
    bool    is_local; // True = captured from immediate enclosing locals, false = from upvalue.
};

/// Loop context for break/continue compilation.
struct LoopContext {
    size_t              start_offset;    // Where to jump for continue.
    std::vector<size_t> break_patches;   // Jump offsets to patch for break.
    int                 scope_depth;     // Scope depth at loop start (for popping locals).
};

/// Compiler state for one function scope.
struct CompilerScope {
    Chunk                        chunk;
    std::vector<Local>           locals;
    std::vector<CompilerUpvalue> upvalues;
    int                          scope_depth = 0;
    int                          upvalue_count = 0;

    /// The enclosing compiler scope (nullptr for top-level).
    CompilerScope*               enclosing = nullptr;
};

class Compiler {
public:
    explicit Compiler(const SourceFile& source);

    /// Compile a fully parsed program. Returns the top-level chunk.
    Chunk compile(const Program& program);

    /// Were there compilation errors?
    bool had_errors() const { return had_error_; }

private:
    const SourceFile&          source_;
    CompilerScope*             current_ = nullptr;    // Active compilation scope.
    bool                       had_error_ = false;
    std::vector<LoopContext>   loop_stack_;            // Active loop contexts.

    // ── Scope management ───────────────────────────────────────────────────────

    void begin_scope();
    void end_scope(uint32_t line);
    void declare_local(const std::string& name);
    void mark_initialized();
    int resolve_local(const std::string& name) const;
    void add_local(const std::string& name);

    // ── Upvalue resolution ─────────────────────────────────────────────────────

    /// Resolve a variable name as an upvalue. Returns -1 if not captured.
    int resolve_upvalue(CompilerScope* scope, const std::string& name);

    /// Add an upvalue to the current scope's upvalue array.
    int add_upvalue(CompilerScope* scope, uint8_t index, bool is_local);

    // ── Statement compilation ──────────────────────────────────────────────────

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

    // ── Expression compilation ─────────────────────────────────────────────────

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

    // ── Function compilation ───────────────────────────────────────────────────

    Value compile_function_body(const std::string& name,
                                const std::vector<Token>& params,
                                const Stmt& body);

    // ── Helpers ────────────────────────────────────────────────────────────────

    Chunk& current_chunk() { return current_->chunk; }
    void error(const std::string& message);
    uint16_t identifier_constant(const std::string& name);
};

} // namespace crashlang

#endif // CRASHLANG_COMPILER_HPP
