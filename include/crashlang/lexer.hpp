#ifndef CRASHLANG_LEXER_HPP
#define CRASHLANG_LEXER_HPP

#include "crashlang/token.hpp"

#include <string>
#include <string_view>
#include <vector>
#include <unordered_map>

namespace crashlang {

class Lexer {
public:
    explicit Lexer(const SourceFile& source);

    std::vector<Token> scan_tokens();
    bool had_errors() const { return had_errors_; }

private:
    const SourceFile& source_;
    std::string_view  text_;
    size_t current_    = 0;
    size_t token_start_ = 0;
    std::vector<Token> tokens_;
    bool had_errors_   = false;

    bool is_at_end() const;
    char peek() const;
    char peek_next() const;
    char advance();
    bool match(char expected);

    void begin_token();
    Span current_span() const;
    std::string current_lexeme() const;
    void add_token(TokenType type);
    void add_error(const std::string& message);

    void scan_token();
    void scan_string();
    void scan_number();
    void scan_identifier();
    void skip_line_comment();
    void skip_block_comment();

    static const std::unordered_map<std::string, TokenType> keywords_;
};

} // namespace crashlang

#endif
