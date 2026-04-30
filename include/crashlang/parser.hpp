#ifndef CRASHLANG_PARSER_HPP
#define CRASHLANG_PARSER_HPP

/// Recursive descent parser with Pratt (precedence climbing) expression parsing.
///
/// Consumes the token stream from the lexer and produces a typed AST.
/// Error recovery: on a parse error, the parser records the error and
/// synchronizes to the next statement boundary, then continues. This
/// allows reporting multiple errors in a single pass.

#include "crashlang/ast.hpp"
#include "crashlang/token.hpp"

#include <functional>
#include <string>
#include <vector>

namespace crashlang {

/// A parse error with location and message.
struct ParseError {
    Span        span;
    std::string message;
};

class Parser {
public:
    /// Construct a parser for the given token stream.
    /// The SourceFile pointer is used for error messages.
    Parser(std::vector<Token> tokens, const SourceFile* source);

    /// Parse the entire token stream into a Program.
    Program parse();

    /// Were there any parse errors?
    bool had_errors() const { return !errors_.empty(); }

    /// Get all parse errors.
    const std::vector<ParseError>& errors() const { return errors_; }

    /// Format all errors as a human-readable string.
    std::string format_errors() const;

private:
    // ── Token stream navigation ────────────────────────────────────────────────

    std::vector<Token> tokens_;
    const SourceFile*  source_;
    size_t             current_ = 0;

    /// Collected parse errors.
    std::vector<ParseError> errors_;

    /// Peek at the current token without consuming.
    const Token& peek() const;

    /// Peek at the previous token (the one just consumed).
    const Token& previous() const;

    /// Is the current token of the given type?
    bool check(TokenType type) const;

    /// Are we at EOF?
    bool is_at_end() const;

    /// Consume and return the current token.
    const Token& advance();

    /// Consume the current token if it matches `type`. Returns true if consumed.
    bool match(TokenType type);

    /// Consume the current token if it matches any of the given types.
    bool match_any(std::initializer_list<TokenType> types);

    /// Expect the current token to be `type`. Consume it if so; emit error if not.
    const Token& expect(TokenType type, const std::string& message);

    /// Record a parse error at the current token.
    void error(const std::string& message);

    /// Record a parse error at a specific span.
    void error_at(Span span, const std::string& message);

    /// Synchronize after an error: skip tokens until we find a statement boundary.
    void synchronize();

    // ── Statement parsing ──────────────────────────────────────────────────────

    /// Parse a declaration (let, fn) or a statement.
    StmtPtr parse_declaration();

    /// Parse `let name = expr;`.
    StmtPtr parse_let_stmt();

    /// Parse `fn name(params...) { body }`.
    StmtPtr parse_fn_stmt();

    /// Parse a non-declaration statement.
    StmtPtr parse_statement();

    /// Parse `if expr { ... } else { ... }`.
    StmtPtr parse_if_stmt();

    /// Parse `while expr { ... }`.
    StmtPtr parse_while_stmt();

    /// Parse `for name in expr { ... }`.
    StmtPtr parse_for_stmt();

    /// Parse `return expr;` or `return;`.
    StmtPtr parse_return_stmt();

    /// Parse `break;`.
    StmtPtr parse_break_stmt();

    /// Parse `continue;`.
    StmtPtr parse_continue_stmt();

    /// Parse `free(expr);`.
    StmtPtr parse_free_stmt();

    /// Parse `{ stmts... }`. Assumes the opening '{' has NOT been consumed.
    StmtPtr parse_block();

    /// Parse an expression followed by ';'.
    StmtPtr parse_expr_stmt();

    // ── Expression parsing (Pratt / precedence climbing) ───────────────────────

    /// Parse an expression. Entry point.
    ExprPtr parse_expression();

    /// Parse an assignment expression (lowest precedence).
    ExprPtr parse_assignment();

    /// Parse an `or` expression.
    ExprPtr parse_or();

    /// Parse an `and` expression.
    ExprPtr parse_and();

    /// Parse an equality expression: `==`, `!=`.
    ExprPtr parse_equality();

    /// Parse a comparison: `<`, `<=`, `>`, `>=`.
    ExprPtr parse_comparison();

    /// Parse addition/subtraction: `+`, `-`.
    ExprPtr parse_addition();

    /// Parse multiplication/division/modulo: `*`, `/`, `%`.
    ExprPtr parse_multiplication();

    /// Parse a unary expression: `-expr`, `not expr`.
    ExprPtr parse_unary();

    /// Parse call expressions, indexing, and field access (postfix operators).
    ExprPtr parse_postfix();

    /// Parse a primary expression (literals, identifiers, grouped expressions,
    /// arrays, new, ref, deref, move).
    ExprPtr parse_primary();

    /// Parse the argument list of a call: `(expr, expr, ...)`.
    /// Assumes '(' has just been consumed.
    std::vector<ExprPtr> parse_arguments();
};

} // namespace crashlang

#endif // CRASHLANG_PARSER_HPP
