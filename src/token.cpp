#include "crashlang/token.hpp"

#include <iomanip>

namespace crashlang {

// ── token_type_name ────────────────────────────────────────────────────────────

const char* token_type_name(TokenType type) {
    switch (type) {
        case TokenType::IntLiteral:    return "IntLiteral";
        case TokenType::FloatLiteral:  return "FloatLiteral";
        case TokenType::StringLiteral: return "StringLiteral";

        case TokenType::True:          return "True";
        case TokenType::False:         return "False";
        case TokenType::Nil:           return "Nil";
        case TokenType::Let:           return "Let";
        case TokenType::Fn:            return "Fn";
        case TokenType::Return:        return "Return";
        case TokenType::If:            return "If";
        case TokenType::Else:          return "Else";
        case TokenType::While:         return "While";
        case TokenType::For:           return "For";
        case TokenType::In:            return "In";
        case TokenType::Break:         return "Break";
        case TokenType::Continue:      return "Continue";
        case TokenType::And:           return "And";
        case TokenType::Or:            return "Or";
        case TokenType::Not:           return "Not";
        case TokenType::New:           return "New";
        case TokenType::Free:          return "Free";
        case TokenType::Ref:           return "Ref";
        case TokenType::Deref:         return "Deref";
        case TokenType::Move:          return "Move";

        case TokenType::Identifier:    return "Identifier";

        case TokenType::Plus:          return "Plus";
        case TokenType::Minus:         return "Minus";
        case TokenType::Star:          return "Star";
        case TokenType::Slash:         return "Slash";
        case TokenType::Percent:       return "Percent";
        case TokenType::Eq:            return "Eq";
        case TokenType::EqEq:          return "EqEq";
        case TokenType::BangEq:        return "BangEq";
        case TokenType::Lt:            return "Lt";
        case TokenType::LtEq:          return "LtEq";
        case TokenType::Gt:            return "Gt";
        case TokenType::GtEq:          return "GtEq";
        case TokenType::Dot:           return "Dot";

        case TokenType::LParen:        return "LParen";
        case TokenType::RParen:        return "RParen";
        case TokenType::LBrace:        return "LBrace";
        case TokenType::RBrace:        return "RBrace";
        case TokenType::LBracket:      return "LBracket";
        case TokenType::RBracket:      return "RBracket";
        case TokenType::Comma:         return "Comma";
        case TokenType::Colon:         return "Colon";
        case TokenType::Semicolon:     return "Semicolon";

        case TokenType::Eof:           return "Eof";
        case TokenType::Error:         return "Error";
    }
    return "Unknown";
}

// ── token_type_symbol ──────────────────────────────────────────────────────────

const char* token_type_symbol(TokenType type) {
    switch (type) {
        case TokenType::Plus:      return "+";
        case TokenType::Minus:     return "-";
        case TokenType::Star:      return "*";
        case TokenType::Slash:     return "/";
        case TokenType::Percent:   return "%";
        case TokenType::Eq:        return "=";
        case TokenType::EqEq:      return "==";
        case TokenType::BangEq:    return "!=";
        case TokenType::Lt:        return "<";
        case TokenType::LtEq:      return "<=";
        case TokenType::Gt:        return ">";
        case TokenType::GtEq:      return ">=";
        case TokenType::Dot:       return ".";
        case TokenType::LParen:    return "(";
        case TokenType::RParen:    return ")";
        case TokenType::LBrace:    return "{";
        case TokenType::RBrace:    return "}";
        case TokenType::LBracket:  return "[";
        case TokenType::RBracket:  return "]";
        case TokenType::Comma:     return ",";
        case TokenType::Colon:     return ":";
        case TokenType::Semicolon: return ";";

        case TokenType::Let:       return "let";
        case TokenType::Fn:        return "fn";
        case TokenType::Return:    return "return";
        case TokenType::If:        return "if";
        case TokenType::Else:      return "else";
        case TokenType::While:     return "while";
        case TokenType::For:       return "for";
        case TokenType::In:        return "in";
        case TokenType::Break:     return "break";
        case TokenType::Continue:  return "continue";
        case TokenType::And:       return "and";
        case TokenType::Or:        return "or";
        case TokenType::Not:       return "not";
        case TokenType::True:      return "true";
        case TokenType::False:     return "false";
        case TokenType::Nil:       return "nil";
        case TokenType::New:       return "new";
        case TokenType::Free:      return "free";
        case TokenType::Ref:       return "ref";
        case TokenType::Deref:     return "deref";
        case TokenType::Move:      return "move";

        default:                   return nullptr;
    }
}

// ── Token factories ────────────────────────────────────────────────────────────

Token Token::make(TokenType type, std::string lexeme, Span span) {
    return Token{type, std::move(lexeme), span, std::monostate{}};
}

Token Token::make_int(int64_t value, std::string lexeme, Span span) {
    return Token{TokenType::IntLiteral, std::move(lexeme), span, value};
}

Token Token::make_float(double value, std::string lexeme, Span span) {
    return Token{TokenType::FloatLiteral, std::move(lexeme), span, value};
}

Token Token::make_string(std::string value, std::string lexeme, Span span) {
    return Token{TokenType::StringLiteral, std::move(lexeme), span, std::move(value)};
}

Token Token::make_error(std::string message, Span span) {
    return Token{TokenType::Error, std::move(message), span, std::monostate{}};
}

Token Token::make_eof(Span span) {
    return Token{TokenType::Eof, "", span, std::monostate{}};
}

// ── operator<< ─────────────────────────────────────────────────────────────────

std::ostream& operator<<(std::ostream& os, const Token& token) {
    os << std::left << std::setw(14) << token_type_name(token.type);
    os << " " << std::setw(5) << token.span.to_string();

    if (!token.lexeme.empty()) {
        os << "  '" << token.lexeme << "'";
    }

    // Print literal value if present.
    std::visit([&](auto&& val) {
        using T = std::decay_t<decltype(val)>;
        if constexpr (std::is_same_v<T, int64_t>) {
            os << "  (int: " << val << ")";
        } else if constexpr (std::is_same_v<T, double>) {
            os << "  (float: " << val << ")";
        } else if constexpr (std::is_same_v<T, std::string>) {
            os << "  (str: \"" << val << "\")";
        }
    }, token.literal);

    return os;
}

} // namespace crashlang
