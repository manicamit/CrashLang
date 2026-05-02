#include "crashlang/lexer.hpp"

#include <cctype>
#include <charconv>
#include <cstdlib>

namespace crashlang {
const std::unordered_map<std::string, TokenType> Lexer::keywords_ = {
    {"true",     TokenType::True},
    {"false",    TokenType::False},
    {"nil",      TokenType::Nil},
    {"let",      TokenType::Let},
    {"fn",       TokenType::Fn},
    {"return",   TokenType::Return},
    {"if",       TokenType::If},
    {"else",     TokenType::Else},
    {"while",    TokenType::While},
    {"for",      TokenType::For},
    {"in",       TokenType::In},
    {"break",    TokenType::Break},
    {"continue", TokenType::Continue},
    {"and",      TokenType::And},
    {"or",       TokenType::Or},
    {"not",      TokenType::Not},
    {"new",      TokenType::New},
    {"free",     TokenType::Free},
    {"ref",      TokenType::Ref},
    {"deref",    TokenType::Deref},
    {"move",     TokenType::Move},
    {"match",    TokenType::Match},
    {"when",     TokenType::When},
};
Lexer::Lexer(const SourceFile& source)
    : source_(source)
    , text_(source.source)
{}
bool Lexer::is_at_end() const {
    return current_ >= text_.size();
}

char Lexer::peek() const {
    if (is_at_end()) return '\0';
    return text_[current_];
}

char Lexer::peek_next() const {
    if (current_ + 1 >= text_.size()) return '\0';
    return text_[current_ + 1];
}

char Lexer::advance() {
    return text_[current_++];
}

bool Lexer::match(char expected) {
    if (is_at_end() || text_[current_] != expected) return false;
    ++current_;
    return true;
}
void Lexer::begin_token() {
    token_start_ = current_;
}

Span Lexer::current_span() const {
    auto start = source_.location_from_offset(token_start_);
    // End points at the last character consumed (current_ - 1), but if we
    // haven't advanced past token_start_, use token_start_ itself.
    size_t end_offset = (current_ > token_start_) ? current_ - 1 : token_start_;
    auto end = source_.location_from_offset(end_offset);
    return Span{start, end};
}

std::string Lexer::current_lexeme() const {
    return std::string(text_.substr(token_start_, current_ - token_start_));
}

void Lexer::add_token(TokenType type) {
    tokens_.push_back(Token::make(type, current_lexeme(), current_span()));
}

void Lexer::add_error(const std::string& message) {
    had_errors_ = true;
    tokens_.push_back(Token::make_error(message, current_span()));
}
std::vector<Token> Lexer::scan_tokens() {
    tokens_.clear();
    current_ = 0;
    had_errors_ = false;

    while (!is_at_end()) {
        begin_token();
        scan_token();
    }

    // Emit EOF.
    begin_token();
    auto eof_loc = source_.location_from_offset(text_.size() > 0 ? text_.size() - 1 : 0);
    tokens_.push_back(Token::make_eof(Span{eof_loc, eof_loc}));

    return tokens_;
}
void Lexer::scan_token() {
    char c = advance();

    switch (c) {
        // ── Single-character tokens ────────────────────────────────────────────
        case '(': add_token(TokenType::LParen);    return;
        case ')': add_token(TokenType::RParen);    return;
        case '{': add_token(TokenType::LBrace);    return;
        case '}': add_token(TokenType::RBrace);    return;
        case '[': add_token(TokenType::LBracket);  return;
        case ']': add_token(TokenType::RBracket);  return;
        case ',': add_token(TokenType::Comma);     return;
        case ':': add_token(TokenType::Colon);     return;
        case ';': add_token(TokenType::Semicolon); return;
        case '.': add_token(TokenType::Dot);       return;
        case '+': add_token(TokenType::Plus);      return;
        case '-': add_token(TokenType::Minus);     return;
        case '*': add_token(TokenType::Star);      return;
        case '%': add_token(TokenType::Percent);   return;

        // ── Two-character tokens ───────────────────────────────────────────────
        case '=':
            if (match('>')) {
                add_token(TokenType::Arrow);
            } else {
                add_token(match('=') ? TokenType::EqEq : TokenType::Eq);
            }
            return;

        case '!':
            if (match('=')) {
                add_token(TokenType::BangEq);
            } else {
                add_error("unexpected character '!'. Did you mean '!='?");
            }
            return;

        case '<':
            add_token(match('=') ? TokenType::LtEq : TokenType::Lt);
            return;

        case '>':
            add_token(match('=') ? TokenType::GtEq : TokenType::Gt);
            return;

        // ── Slash: division or comment ─────────────────────────────────────────
        case '/':
            if (match('/')) {
                skip_line_comment();
            } else if (match('*')) {
                skip_block_comment();
            } else {
                add_token(TokenType::Slash);
            }
            return;

        // ── String literal ─────────────────────────────────────────────────────
        case '"':
            scan_string();
            return;

        // ── Whitespace ─────────────────────────────────────────────────────────
        case ' ':
        case '\t':
        case '\r':
        case '\n':
            // Skip silently.
            return;

        default:
            // ── Numbers ────────────────────────────────────────────────────────
            if (std::isdigit(static_cast<unsigned char>(c))) {
                scan_number();
                return;
            }

            // ── Identifiers / keywords ─────────────────────────────────────────
            if (std::isalpha(static_cast<unsigned char>(c)) || c == '_') {
                scan_identifier();
                return;
            }

            // ── Unknown character ──────────────────────────────────────────────
            add_error(std::string("unexpected character '") + c + "'");
            return;
    }
}
void Lexer::scan_string() {
    std::string value;

    while (!is_at_end() && peek() != '"') {
        char c = advance();

        if (c == '\n') {
            // Strings can span multiple lines in CrashLang.
            value += '\n';
            continue;
        }

        if (c == '\\') {
            // Escape sequence.
            if (is_at_end()) {
                add_error("unterminated escape sequence in string");
                return;
            }

            char escaped = advance();
            switch (escaped) {
                case 'n':  value += '\n'; break;
                case 't':  value += '\t'; break;
                case 'r':  value += '\r'; break;
                case '\\': value += '\\'; break;
                case '"':  value += '"';  break;
                case '0':  value += '\0'; break;
                default:
                    add_error(std::string("unknown escape sequence '\\") + escaped + "'");
                    return;
            }
        } else {
            value += c;
        }
    }

    if (is_at_end()) {
        add_error("unterminated string literal");
        return;
    }

    // Consume the closing quote.
    advance();

    auto lexeme = current_lexeme();
    auto span   = current_span();
    tokens_.push_back(Token::make_string(std::move(value), std::move(lexeme), span));
}
void Lexer::scan_number() {
    // Consume integer part.
    while (!is_at_end() && std::isdigit(static_cast<unsigned char>(peek()))) {
        advance();
    }

    bool is_float = false;

    // Check for fractional part.
    if (peek() == '.' && std::isdigit(static_cast<unsigned char>(peek_next()))) {
        is_float = true;
        advance(); // Consume '.'.

        while (!is_at_end() && std::isdigit(static_cast<unsigned char>(peek()))) {
            advance();
        }
    }

    auto lexeme = current_lexeme();
    auto span   = current_span();

    if (is_float) {
        char* end = nullptr;
        double value = std::strtod(lexeme.c_str(), &end);
        if (end != lexeme.c_str() + lexeme.size()) {
            add_error("invalid float literal '" + lexeme + "'");
            return;
        }
        tokens_.push_back(Token::make_float(value, std::move(lexeme), span));
    } else {
        // Parse integer. Use strtoll for range checking.
        char* end = nullptr;
        long long value = std::strtoll(lexeme.c_str(), &end, 10);
        if (end != lexeme.c_str() + lexeme.size()) {
            add_error("invalid integer literal '" + lexeme + "'");
            return;
        }
        tokens_.push_back(Token::make_int(static_cast<int64_t>(value), std::move(lexeme), span));
    }
}
void Lexer::scan_identifier() {
    while (!is_at_end()) {
        char c = peek();
        if (std::isalnum(static_cast<unsigned char>(c)) || c == '_') {
            advance();
        } else {
            break;
        }
    }

    auto lexeme = current_lexeme();
    auto span   = current_span();

    // Check if it's a keyword.
    auto it = keywords_.find(lexeme);
    if (it != keywords_.end()) {
        tokens_.push_back(Token::make(it->second, std::move(lexeme), span));
    } else {
        tokens_.push_back(Token::make(TokenType::Identifier, std::move(lexeme), span));
    }
}
void Lexer::skip_line_comment() {
    while (!is_at_end() && peek() != '\n') {
        advance();
    }
    // Don't consume the newline — let the main loop handle it.
}

void Lexer::skip_block_comment() {
    int depth = 1; // Support nested block comments.

    while (!is_at_end() && depth > 0) {
        if (peek() == '/' && peek_next() == '*') {
            advance(); advance();
            ++depth;
        } else if (peek() == '*' && peek_next() == '/') {
            advance(); advance();
            --depth;
        } else {
            advance();
        }
    }

    if (depth > 0) {
        add_error("unterminated block comment");
    }
}

} // namespace crashlang
