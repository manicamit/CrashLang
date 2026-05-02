#ifndef CRASHLANG_AST_HPP
#define CRASHLANG_AST_HPP

#include "crashlang/common.hpp"
#include "crashlang/source.hpp"
#include "crashlang/token.hpp"

#include <memory>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace crashlang {

struct Expr;
struct Stmt;

using ExprPtr = std::unique_ptr<Expr>;
using StmtPtr = std::unique_ptr<Stmt>;

// --- Expression nodes ---

struct LiteralExpr    { Token value; };
struct IdentifierExpr { Token name; };

struct UnaryExpr {
    Token   op;
    ExprPtr operand;
};

struct BinaryExpr {
    ExprPtr left;
    Token   op;
    ExprPtr right;
};

struct CallExpr {
    ExprPtr              callee;
    Token                paren;
    std::vector<ExprPtr> arguments;
};

struct IndexExpr {
    ExprPtr object;
    Token   bracket;
    ExprPtr index;
};

struct FieldAccessExpr {
    ExprPtr object;
    Token   name;
};

struct AssignExpr {
    ExprPtr target;
    Token   eq;
    ExprPtr value;
};

struct ArrayExpr {
    Token                bracket;
    std::vector<ExprPtr> elements;
};

struct NewExpr {
    Token                                    keyword;
    Token                                    type_name;
    std::vector<std::pair<Token, ExprPtr>>   fields;
};

struct RefExpr   { Token keyword; ExprPtr operand; };
struct DerefExpr { Token keyword; ExprPtr operand; };
struct MoveExpr  { Token keyword; ExprPtr operand; };

struct LambdaExpr {
    Token              keyword;
    std::vector<Token> params;
    StmtPtr            body;
};

struct MatchArm {
    ExprPtr pattern;       // nullptr for wildcard
    ExprPtr body;
    bool    is_wildcard = false;
};

struct MatchExpr {
    Token                  keyword;
    ExprPtr                target;
    std::vector<MatchArm>  arms;
};

using ExprData = std::variant<
    LiteralExpr, IdentifierExpr, UnaryExpr, BinaryExpr,
    CallExpr, IndexExpr, FieldAccessExpr, AssignExpr,
    ArrayExpr, NewExpr, RefExpr, DerefExpr, MoveExpr,
    LambdaExpr, MatchExpr
>;

struct Expr {
    ExprData data;
    Span     span;

    template<typename T>
    static ExprPtr make(T&& node, Span s) {
        auto e = std::make_unique<Expr>();
        e->data = std::forward<T>(node);
        e->span = s;
        return e;
    }
};

// --- Statement nodes ---

struct ExprStmt     { ExprPtr expression; };
struct LetStmt      { Token name; ExprPtr initializer; };
struct BlockStmt    { std::vector<StmtPtr> statements; };

struct IfStmt {
    ExprPtr condition;
    StmtPtr then_branch;
    StmtPtr else_branch;   // nullptr if no else
};

struct WhileStmt { ExprPtr condition; StmtPtr body; };

struct ForStmt {
    Token   variable;
    ExprPtr iterable;
    StmtPtr body;
};

struct FnStmt {
    Token              name;
    std::vector<Token> params;
    StmtPtr            body;
};

struct ReturnStmt   { Token keyword; ExprPtr value; };
struct BreakStmt    { Token keyword; };
struct ContinueStmt { Token keyword; };
struct FreeStmt     { Token keyword; ExprPtr argument; };

using StmtData = std::variant<
    ExprStmt, LetStmt, BlockStmt, IfStmt,
    WhileStmt, ForStmt, FnStmt, ReturnStmt,
    BreakStmt, ContinueStmt, FreeStmt
>;

struct Stmt {
    StmtData data;
    Span     span;

    template<typename T>
    static StmtPtr make(T&& node, Span s) {
        auto st = std::make_unique<Stmt>();
        st->data = std::forward<T>(node);
        st->span = s;
        return st;
    }
};

struct Program {
    std::vector<StmtPtr> statements;
};

std::string format_ast(const Program& program);
std::string format_expr(const Expr& expr, int indent = 0);
std::string format_stmt(const Stmt& stmt, int indent = 0);

} // namespace crashlang

#endif
