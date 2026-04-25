#ifndef CRASHLANG_TOKEN_HPP
#define CRASHLANG_TOKEN_HPP

/// Token types and the Token structure.
///
/// The lexer produces a flat vector of these tokens. Every token carries
/// its source Span so downstream phases can always point back to the
/// exact text that produced it.

#include "crashlang/common.hpp"
#include "crashlang/source.hpp"

#include <string>
#include <variant>
#include <ostream>

namespace crashlang {

// ── TokenType ──────────────────────────────────────────────────────────────────

enum class TokenType {
    // ── Literals ───────────────────────────────────────────────────────────────
    IntLiteral,       // 42
    FloatLiteral,     // 3.14
    StringLiteral,    // "hello"

    // ── Keywords ───────────────────────────────────────────────────────────────
    True,             // true
    False,            // false
    Nil,              // nil
    Let,              // let
    Fn,               // fn
    Return,           // return
    If,               // if
    Else,             // else
    While,            // while
    For,              // for
    In,               // in
    Break,            // break
    Continue,         // continue
    And,              // and
    Or,               // or
    Not,              // not
    New,              // new
    Free,             // free
    Ref,              // ref
    Deref,            // deref
    Move,             // move

    // ── Identifiers ────────────────────────────────────────────────────────────
    Identifier,       // foo, bar, my_var

    // ── Operators ──────────────────────────────────────────────────────────────
    Plus,             // +
    Minus,            // -
    Star,             // *
    Slash,            // /
    Percent,          // %

    Eq,               // =
    EqEq,             // ==
    BangEq,           // !=
    Lt,               // <
    LtEq,             // <=
    Gt,               // >
    GtEq,             // >=

    Dot,              // .

    // ── Delimiters ─────────────────────────────────────────────────────────────
    LParen,           // (
    RParen,           // )
    LBrace,           // {
    RBrace,           // }
    LBracket,         // [
    RBracket,         // ]
    Comma,            // ,
    Colon,            // :
    Semicolon,        // ;

    // ── Special ────────────────────────────────────────────────────────────────
    Eof,              // End of file
    Error,            // Malformed token — lexer error recovery
};

/// Human-readable name for a token type (used in error messages).
const char* token_type_name(TokenType type);

/// Punctuation/keyword string for a token type, or nullptr if not applicable.
/// E.g. TokenType::Plus → "+", TokenType::Let → "let".
const char* token_type_symbol(TokenType type);

// ── Token ──────────────────────────────────────────────────────────────────────

/// A single token produced by the lexer.
struct Token {
    TokenType   type   = TokenType::Eof;
    std::string lexeme;                    // The exact source text of this token.
    Span        span;                      // Where it lives in the source file.

    /// Literal value for numeric/string tokens. Monostate for non-literals.
    std::variant<std::monostate, int64_t, double, std::string> literal;

    /// Convenience constructors.
    static Token make(TokenType type, std::string lexeme, Span span);
    static Token make_int(int64_t value, std::string lexeme, Span span);
    static Token make_float(double value, std::string lexeme, Span span);
    static Token make_string(std::string value, std::string lexeme, Span span);
    static Token make_error(std::string message, Span span);
    static Token make_eof(Span span);
};

/// Print a token for debugging (used by --tokens flag).
std::ostream& operator<<(std::ostream& os, const Token& token);

} // namespace crashlang

#endif // CRASHLANG_TOKEN_HPP
