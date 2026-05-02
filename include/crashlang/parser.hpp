#ifndef CRASHLANG_PARSER_HPP
#define CRASHLANG_PARSER_HPP

#include "crashlang/ast.hpp"
#include "crashlang/token.hpp"

#include <functional>
#include <string>
#include <vector>

namespace crashlang {

struct ParseError {
    Span        span;
    std::string message;
};

class Parser {
public:
    Parser(std::vector<Token> tokens, const SourceFile* source);

    Program parse();
    bool had_errors() const { return !errors_.empty(); }
    const std::vector<ParseError>& errors() const { return errors_; }
    std::string format_errors() const;

private:
    std::vector<Token> tokens_;
    const SourceFile*  source_;
    size_t             current_ = 0;
    std::vector<ParseError> errors_;

    // Token access.
    const Token& peek() const;
    const Token& previous() const;
    bool check(TokenType type) const;
    bool is_at_end() const;
    const Token& advance();
    bool match(TokenType type);
    bool match_any(std::initializer_list<TokenType> types);
    const Token& expect(TokenType type, const std::string& message);
    void error(const std::string& message);
    void error_at(Span span, const std::string& message);
    void synchronize();

    // Statement parsing.
    StmtPtr parse_declaration();
    StmtPtr parse_let_stmt();
    StmtPtr parse_fn_stmt();
    StmtPtr parse_statement();
    StmtPtr parse_if_stmt();
    StmtPtr parse_while_stmt();
    StmtPtr parse_for_stmt();
    StmtPtr parse_return_stmt();
    StmtPtr parse_break_stmt();
    StmtPtr parse_continue_stmt();
    StmtPtr parse_free_stmt();
    StmtPtr parse_block();
    StmtPtr parse_expr_stmt();

    // Expression parsing (precedence climbing).
    ExprPtr parse_expression();
    ExprPtr parse_assignment();
    ExprPtr parse_or();
    ExprPtr parse_and();
    ExprPtr parse_equality();
    ExprPtr parse_comparison();
    ExprPtr parse_addition();
    ExprPtr parse_multiplication();
    ExprPtr parse_unary();
    ExprPtr parse_postfix();
    ExprPtr parse_primary();
    std::vector<ExprPtr> parse_arguments();
};

} // namespace crashlang

#endif
