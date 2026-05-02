#ifndef CRASHLANG_TOKEN_HPP
#define CRASHLANG_TOKEN_HPP

#include "crashlang/common.hpp"
#include "crashlang/source.hpp"

#include <string>
#include <variant>
#include <ostream>

namespace crashlang {

enum class TokenType {
    IntLiteral, FloatLiteral, StringLiteral,

    True, False, Nil,
    Let, Fn, Return, If, Else,
    While, For, In, Break, Continue,
    And, Or, Not,
    New, Free, Ref, Deref, Move,
    Match, When,

    Identifier,

    Plus, Minus, Star, Slash, Percent,
    Eq, EqEq, BangEq, Lt, LtEq, Gt, GtEq,
    Dot, Arrow,
    LParen, RParen, LBrace, RBrace, LBracket, RBracket,
    Comma, Colon, Semicolon,

    Eof, Error,
};

const char* token_type_name(TokenType type);
const char* token_type_symbol(TokenType type);

struct Token {
    TokenType   type   = TokenType::Eof;
    std::string lexeme;
    Span        span;
    std::variant<std::monostate, int64_t, double, std::string> literal;

    static Token make(TokenType type, std::string lexeme, Span span);
    static Token make_int(int64_t value, std::string lexeme, Span span);
    static Token make_float(double value, std::string lexeme, Span span);
    static Token make_string(std::string value, std::string lexeme, Span span);
    static Token make_error(std::string message, Span span);
    static Token make_eof(Span span);
};

std::ostream& operator<<(std::ostream& os, const Token& token);

} // namespace crashlang

#endif
