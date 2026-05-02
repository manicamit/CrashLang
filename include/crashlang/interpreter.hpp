#ifndef CRASHLANG_INTERPRETER_HPP
#define CRASHLANG_INTERPRETER_HPP

/// Tree-walk interpreter for CrashLang.
///
/// Recursively evaluates AST nodes. Maintains an Environment for variable
/// bindings, a HeapSimulator for memory modeling, and an OwnershipTracker
/// for lifetime event recording. Tracks the call stack for crash reports
/// and enforces the stack depth limit for StackOverflow detection.

#include "crashlang/ast.hpp"
#include "crashlang/environment.hpp"
#include "crashlang/errors.hpp"
#include "crashlang/heap.hpp"
#include "crashlang/ownership.hpp"
#include "crashlang/source.hpp"
#include "crashlang/value.hpp"

#include <string>
#include <vector>

namespace crashlang {

class Interpreter {
public:
    /// Maximum call depth before StackOverflow is raised.
    static constexpr size_t MAX_CALL_DEPTH = 256;

    explicit Interpreter(const SourceFile& source);

    /// Execute a fully parsed program.
    void execute(const Program& program);

    /// Did execution complete without a crash?
    bool ok() const { return !crashed_; }

    /// Access the heap (for diagnostics in main.cpp).
    const HeapSimulator& heap() const { return heap_; }

    /// Access the ownership tracker (for diagnostics in main.cpp).
    const OwnershipTracker& ownership() const { return ownership_; }

private:
    const SourceFile&  source_;
    Environment        env_;
    HeapSimulator      heap_;
    OwnershipTracker   ownership_;
    bool               crashed_ = false;

    /// Active call stack (grows on function calls, shrinks on return).
    std::vector<CallFrame> call_stack_;

    /// Get the name of the currently executing function (for ownership events).
    std::string current_function_name() const;

    // ── Statement execution ────────────────────────────────────────────────────

    void execute_stmt(const Stmt& stmt);
    void execute_block(const BlockStmt& block);
    void execute_let(const LetStmt& stmt);
    void execute_if(const IfStmt& stmt);
    void execute_while(const WhileStmt& stmt);
    void execute_for(const ForStmt& stmt);
    void execute_fn(const FnStmt& stmt);
    void execute_return(const ReturnStmt& stmt);
    void execute_break(const BreakStmt& stmt);
    void execute_continue(const ContinueStmt& stmt);
    void execute_free(const FreeStmt& stmt);

    // ── Expression evaluation ──────────────────────────────────────────────────

    Value evaluate(const Expr& expr);
    Value eval_literal(const LiteralExpr& expr);
    Value eval_identifier(const IdentifierExpr& expr);
    Value eval_unary(const UnaryExpr& expr);
    Value eval_binary(const BinaryExpr& expr);
    Value eval_call(const CallExpr& expr);
    Value eval_index(const IndexExpr& expr);
    Value eval_field_access(const FieldAccessExpr& expr);
    Value eval_assign(const AssignExpr& expr);
    Value eval_array(const ArrayExpr& expr);
    Value eval_new(const NewExpr& expr);
    Value eval_ref(const RefExpr& expr);
    Value eval_deref(const DerefExpr& expr);
    Value eval_move(const MoveExpr& expr);
    Value eval_lambda(const LambdaExpr& expr);

    // ── Function calls ─────────────────────────────────────────────────────────

    Value call_function(const FunctionValue& fn,
                        std::vector<Value> args,
                        Span call_site);

    Value call_builtin(const BuiltinValue& builtin,
                       std::vector<Value> args,
                       Span call_site);

    // ── Error helpers ──────────────────────────────────────────────────────────

    /// Build a CrashError with the current call stack attached.
    CrashError make_error(CrashKind kind, Span location, std::string detail) const;

    /// Throw a type mismatch error.
    [[noreturn]] void type_error(const std::string& op,
                                 const Value& got,
                                 const char* expected,
                                 Span location);

    /// Throw a type mismatch error for binary operators.
    [[noreturn]] void binary_type_error(const std::string& op,
                                        const Value& left,
                                        const Value& right,
                                        Span location);
};

} // namespace crashlang

#endif // CRASHLANG_INTERPRETER_HPP
