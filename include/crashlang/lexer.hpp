#ifndef CRASHLANG_LEXER_HPP
#define CRASHLANG_LEXER_HPP

/// Lexer / Scanner for CrashLang.
///
/// Converts raw source text into a flat vector of tokens. The scanner is
/// character-by-character with one character of lookahead. Every token
/// records its exact source Span for downstream diagnostics.
///
/// Error handling: malformed input produces Error tokens rather than
/// throwing exceptions. The caller can decide how to handle them.

#include "crashlang/token.hpp"

#include <string>
#include <string_view>
#include <vector>
#include <unordered_map>

namespace crashlang {

class Lexer {
public:
    /// Construct a lexer for the given source file.
    /// The SourceFile must outlive the Lexer.
    explicit Lexer(const SourceFile& source);

    /// Scan the entire source and return all tokens (ending with Eof).
    std::vector<Token> scan_tokens();

    /// Were any errors encountered during scanning?
    bool had_errors() const { return had_errors_; }

private:
    // ── Source traversal ───────────────────────────────────────────────────────

    /// The source file being scanned.
    const SourceFile& source_;

    /// Full source text (convenience reference).
    std::string_view text_;

    /// Current position in the source text.
    size_t current_ = 0;

    /// Start of the current token being scanned.
    size_t token_start_ = 0;

    /// Collected tokens.
    std::vector<Token> tokens_;

    /// Error flag.
    bool had_errors_ = false;

    // ── Character helpers ──────────────────────────────────────────────────────

    /// Is the scanner at the end of input?
    bool is_at_end() const;

    /// Peek at the current character without consuming it.
    char peek() const;

    /// Peek at the next character (one ahead of current).
    char peek_next() const;

    /// Consume and return the current character.
    char advance();

    /// Consume the current character if it matches `expected`.
    bool match(char expected);

    // ── Token production ───────────────────────────────────────────────────────

    /// Mark the start of a new token.
    void begin_token();

    /// Build a Span from token_start_ to current_.
    Span current_span() const;

    /// Extract the lexeme text from token_start_ to current_.
    std::string current_lexeme() const;

    /// Add a simple token (no literal value).
    void add_token(TokenType type);

    /// Add an error token.
    void add_error(const std::string& message);

    // ── Scanning routines ──────────────────────────────────────────────────────

    /// Scan one token starting at current_.
    void scan_token();

    /// Scan a string literal (opening quote already consumed).
    void scan_string();

    /// Scan a numeric literal (first digit already consumed).
    void scan_number();

    /// Scan an identifier or keyword (first char already consumed).
    void scan_identifier();

    /// Skip a line comment (// already consumed).
    void skip_line_comment();

    /// Skip a block comment (/* already consumed). Handles nesting.
    void skip_block_comment();

    // ── Keyword table ──────────────────────────────────────────────────────────

    /// Map of keyword strings to their TokenType.
    static const std::unordered_map<std::string, TokenType> keywords_;
};

} // namespace crashlang

#endif // CRASHLANG_LEXER_HPP
