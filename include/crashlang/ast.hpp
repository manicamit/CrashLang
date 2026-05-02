#ifndef CRASHLANG_AST_HPP
#define CRASHLANG_AST_HPP

/// Abstract Syntax Tree node definitions for CrashLang.
///
/// The AST uses a variant-based design: `Expr` and `Stmt` are wrapper structs
/// that hold a `std::variant` of concrete node types. Recursive references
/// use `std::unique_ptr<Expr>` / `std::unique_ptr<Stmt>`, which have fixed
/// size regardless of the variant's contents.
///
/// Every node carries a `Span` — the source range it was parsed from.
/// This metadata flows through interpretation and into crash reports.

#include "crashlang/common.hpp"
#include "crashlang/source.hpp"
#include "crashlang/token.hpp"

#include <memory>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace crashlang {

// ── Forward declarations for recursive types ───────────────────────────────────

struct Expr;
struct Stmt;

using ExprPtr = std::unique_ptr<Expr>;
using StmtPtr = std::unique_ptr<Stmt>;

// ── Expression node types ──────────────────────────────────────────────────────

/// Integer, float, string, bool, or nil literal.
struct LiteralExpr {
    Token value; // Literal payload lives in value.literal.
};

/// Variable reference: `foo`, `my_var`.
struct IdentifierExpr {
    Token name;
};

/// Unary operator: `-expr`, `not expr`.
struct UnaryExpr {
    Token   op;      // Minus or Not.
    ExprPtr operand;
};

/// Binary operator: `a + b`, `x == y`, etc.
struct BinaryExpr {
    ExprPtr left;
    Token   op;
    ExprPtr right;
};

/// Function or method call: `add(1, 2)`, `println("hello")`.
struct CallExpr {
    ExprPtr              callee;    // The thing being called.
    Token                paren;     // Opening '(' — for error positions.
    std::vector<ExprPtr> arguments;
};

/// Array index: `arr[i]`.
struct IndexExpr {
    ExprPtr object;
    Token   bracket; // Opening '[' — for error positions.
    ExprPtr index;
};

/// Field access: `point.x`, `buf.data`.
struct FieldAccessExpr {
    ExprPtr object;
    Token   name; // The field name token after '.'.
};

/// Assignment: `x = 5`, `arr[0] = 42`, `obj.field = val`.
/// The target must resolve to a valid l-value at runtime.
struct AssignExpr {
    ExprPtr target;
    Token   eq; // The '=' token.
    ExprPtr value;
};

/// Array literal: `[1, 2, 3]`.
struct ArrayExpr {
    Token                bracket; // Opening '['.
    std::vector<ExprPtr> elements;
};

/// Heap allocation: `new Point { x: 1, y: 2 }`.
struct NewExpr {
    Token                                    keyword;   // 'new'.
    Token                                    type_name; // Struct type name.
    std::vector<std::pair<Token, ExprPtr>>   fields;    // field_name: value pairs.
};

/// Create a reference: `ref(expr)`.
struct RefExpr {
    Token   keyword; // 'ref'.
    ExprPtr operand;
};

/// Dereference: `deref(expr)`.
struct DerefExpr {
    Token   keyword; // 'deref'.
    ExprPtr operand;
};

/// Ownership transfer: `move(expr)`.
struct MoveExpr {
    Token   keyword; // 'move'.
    ExprPtr operand;
};

/// Anonymous function: `fn(x, y) { x + y }`.
struct LambdaExpr {
    Token              keyword; // 'fn'.
    std::vector<Token> params;
    StmtPtr            body;   // Always a BlockStmt.
};

/// The variant holding all expression types.
using ExprData = std::variant<
    LiteralExpr,
    IdentifierExpr,
    UnaryExpr,
    BinaryExpr,
    CallExpr,
    IndexExpr,
    FieldAccessExpr,
    AssignExpr,
    ArrayExpr,
    NewExpr,
    RefExpr,
    DerefExpr,
    MoveExpr,
    LambdaExpr
>;

/// An expression node. Wraps ExprData + source Span.
struct Expr {
    ExprData data;
    Span     span;

    /// Construct an Expr on the heap from any node type.
    template<typename T>
    static ExprPtr make(T&& node, Span s) {
        auto e = std::make_unique<Expr>();
        e->data = std::forward<T>(node);
        e->span = s;
        return e;
    }
};

// ── Statement node types ───────────────────────────────────────────────────────

/// Expression used as a statement: `foo();`, `x + 1;`.
struct ExprStmt {
    ExprPtr expression;
};

/// Variable declaration: `let x = expr;`.
struct LetStmt {
    Token   name;
    ExprPtr initializer; // nullptr if no initializer (not currently allowed).
};

/// Block of statements: `{ ... }`.
struct BlockStmt {
    std::vector<StmtPtr> statements;
};

/// Conditional: `if expr { ... } else { ... }`.
struct IfStmt {
    ExprPtr condition;
    StmtPtr then_branch;  // Always a BlockStmt.
    StmtPtr else_branch;  // BlockStmt or IfStmt (else-if chain), or nullptr.
};

/// While loop: `while expr { ... }`.
struct WhileStmt {
    ExprPtr condition;
    StmtPtr body; // Always a BlockStmt.
};

/// For-in loop: `for x in expr { ... }`.
struct ForStmt {
    Token   variable;  // Loop variable name.
    ExprPtr iterable;  // The expression to iterate over.
    StmtPtr body;      // Always a BlockStmt.
};

/// Function definition: `fn name(params...) { ... }`.
struct FnStmt {
    Token              name;
    std::vector<Token> params;
    StmtPtr            body; // Always a BlockStmt.
};

/// Return statement: `return;` or `return expr;`.
struct ReturnStmt {
    Token   keyword; // 'return'.
    ExprPtr value;   // nullptr for bare `return;`.
};

/// Break out of a loop: `break;`.
struct BreakStmt {
    Token keyword;
};

/// Continue to next iteration: `continue;`.
struct ContinueStmt {
    Token keyword;
};

/// Free heap object: `free(expr);`.
struct FreeStmt {
    Token   keyword; // 'free'.
    ExprPtr argument;
};

/// The variant holding all statement types.
using StmtData = std::variant<
    ExprStmt,
    LetStmt,
    BlockStmt,
    IfStmt,
    WhileStmt,
    ForStmt,
    FnStmt,
    ReturnStmt,
    BreakStmt,
    ContinueStmt,
    FreeStmt
>;

/// A statement node. Wraps StmtData + source Span.
struct Stmt {
    StmtData data;
    Span     span;

    /// Construct a Stmt on the heap from any node type.
    template<typename T>
    static StmtPtr make(T&& node, Span s) {
        auto st = std::make_unique<Stmt>();
        st->data = std::forward<T>(node);
        st->span = s;
        return st;
    }
};

// ── Program ────────────────────────────────────────────────────────────────────

/// Top-level: a sequence of statements.
struct Program {
    std::vector<StmtPtr> statements;
};

// ── AST Printer ────────────────────────────────────────────────────────────────

/// Pretty-print an entire program's AST (for --ast flag).
std::string format_ast(const Program& program);

/// Pretty-print a single expression.
std::string format_expr(const Expr& expr, int indent = 0);

/// Pretty-print a single statement.
std::string format_stmt(const Stmt& stmt, int indent = 0);

} // namespace crashlang

#endif // CRASHLANG_AST_HPP
