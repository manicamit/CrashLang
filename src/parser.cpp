#include "crashlang/parser.hpp"

#include <sstream>

namespace crashlang {

// ── Constructor ────────────────────────────────────────────────────────────────

Parser::Parser(std::vector<Token> tokens, const SourceFile* source)
    : tokens_(std::move(tokens))
    , source_(source)
{}

// ── Token navigation ───────────────────────────────────────────────────────────

const Token& Parser::peek() const {
    return tokens_[current_];
}

const Token& Parser::previous() const {
    return tokens_[current_ - 1];
}

bool Parser::check(TokenType type) const {
    return !is_at_end() && peek().type == type;
}

bool Parser::is_at_end() const {
    return peek().type == TokenType::Eof;
}

const Token& Parser::advance() {
    if (!is_at_end()) ++current_;
    return previous();
}

bool Parser::match(TokenType type) {
    if (check(type)) {
        advance();
        return true;
    }
    return false;
}

bool Parser::match_any(std::initializer_list<TokenType> types) {
    for (auto t : types) {
        if (check(t)) {
            advance();
            return true;
        }
    }
    return false;
}

const Token& Parser::expect(TokenType type, const std::string& message) {
    if (check(type)) {
        return advance();
    }
    error(message);
    // Return current token as a fallback so callers don't crash.
    return peek();
}

void Parser::error(const std::string& message) {
    error_at(peek().span, message);
}

void Parser::error_at(Span span, const std::string& message) {
    errors_.push_back({span, message});
}

void Parser::synchronize() {
    advance(); // Skip the problematic token.

    while (!is_at_end()) {
        // If the previous token was a semicolon, we're at a new statement.
        if (previous().type == TokenType::Semicolon) return;

        // If the current token starts a new statement, stop.
        switch (peek().type) {
            case TokenType::Let:
            case TokenType::Fn:
            case TokenType::If:
            case TokenType::While:
            case TokenType::For:
            case TokenType::Return:
            case TokenType::Break:
            case TokenType::Continue:
            case TokenType::Free:
                return;
            default:
                break;
        }

        advance();
    }
}

// ── Error formatting ───────────────────────────────────────────────────────────

std::string Parser::format_errors() const {
    std::ostringstream os;
    for (const auto& err : errors_) {
        os << err.span.to_string() << ": error: " << err.message << '\n';

        // Show the source line if we can.
        if (source_ && err.span.start.line > 0) {
            auto line = source_->get_line(err.span.start.line);
            os << "  " << err.span.start.line << " | " << line << '\n';

            // Caret pointing at the error column.
            os << "  " << std::string(std::to_string(err.span.start.line).size(), ' ')
               << " | ";
            if (err.span.start.column > 1) {
                os << std::string(err.span.start.column - 1, ' ');
            }
            os << "^\n";
        }
    }
    return os.str();
}

// ── Top-level: parse() ────────────────────────────────────────────────────────

Program Parser::parse() {
    Program program;

    while (!is_at_end()) {
        auto stmt = parse_declaration();
        if (stmt) {
            program.statements.push_back(std::move(stmt));
        }
    }

    return program;
}

// ── Declaration parsing ────────────────────────────────────────────────────────

StmtPtr Parser::parse_declaration() {
    try {
        if (check(TokenType::Let))  return parse_let_stmt();
        if (check(TokenType::Fn))   return parse_fn_stmt();
        return parse_statement();
    } catch (...) {
        // This shouldn't happen since we use error() + synchronize() instead
        // of exceptions, but just in case.
        synchronize();
        return nullptr;
    }
}

StmtPtr Parser::parse_let_stmt() {
    auto let_tok = advance(); // Consume 'let'.
    auto& name = expect(TokenType::Identifier, "expected variable name after 'let'");

    ExprPtr initializer = nullptr;
    if (match(TokenType::Eq)) {
        initializer = parse_expression();
    } else {
        error("expected '=' after variable name in 'let' declaration");
        synchronize();
        return nullptr;
    }

    expect(TokenType::Semicolon, "expected ';' after variable declaration");

    auto span = Span::merge(let_tok.span, previous().span);
    LetStmt node;
    node.name = name;
    node.initializer = std::move(initializer);
    return Stmt::make(std::move(node), span);
}

StmtPtr Parser::parse_fn_stmt() {
    auto fn_tok = advance(); // Consume 'fn'.
    auto& name = expect(TokenType::Identifier, "expected function name after 'fn'");

    expect(TokenType::LParen, "expected '(' after function name");

    std::vector<Token> params;
    if (!check(TokenType::RParen)) {
        do {
            auto& param = expect(TokenType::Identifier, "expected parameter name");
            params.push_back(param);
        } while (match(TokenType::Comma));
    }

    expect(TokenType::RParen, "expected ')' after parameter list");

    auto body = parse_block();
    if (!body) return nullptr;

    auto span = Span::merge(fn_tok.span, previous().span);
    FnStmt node;
    node.name = name;
    node.params = std::move(params);
    node.body = std::move(body);
    return Stmt::make(std::move(node), span);
}

// ── Statement parsing ──────────────────────────────────────────────────────────

StmtPtr Parser::parse_statement() {
    if (check(TokenType::If))       return parse_if_stmt();
    if (check(TokenType::While))    return parse_while_stmt();
    if (check(TokenType::For))      return parse_for_stmt();
    if (check(TokenType::Return))   return parse_return_stmt();
    if (check(TokenType::Break))    return parse_break_stmt();
    if (check(TokenType::Continue)) return parse_continue_stmt();
    if (check(TokenType::Free))     return parse_free_stmt();
    if (check(TokenType::LBrace))   return parse_block();
    return parse_expr_stmt();
}

StmtPtr Parser::parse_if_stmt() {
    auto if_tok = advance(); // Consume 'if'.

    auto condition = parse_expression();
    auto then_branch = parse_block();
    if (!then_branch) return nullptr;

    StmtPtr else_branch = nullptr;
    if (match(TokenType::Else)) {
        if (check(TokenType::If)) {
            // else-if chain: parse as another IfStmt.
            else_branch = parse_if_stmt();
        } else {
            else_branch = parse_block();
        }
    }

    auto span = Span::merge(if_tok.span, previous().span);
    IfStmt node;
    node.condition = std::move(condition);
    node.then_branch = std::move(then_branch);
    node.else_branch = std::move(else_branch);
    return Stmt::make(std::move(node), span);
}

StmtPtr Parser::parse_while_stmt() {
    auto while_tok = advance(); // Consume 'while'.

    auto condition = parse_expression();
    auto body = parse_block();
    if (!body) return nullptr;

    auto span = Span::merge(while_tok.span, previous().span);
    WhileStmt node;
    node.condition = std::move(condition);
    node.body = std::move(body);
    return Stmt::make(std::move(node), span);
}

StmtPtr Parser::parse_for_stmt() {
    auto for_tok = advance(); // Consume 'for'.

    auto& var = expect(TokenType::Identifier, "expected loop variable after 'for'");
    expect(TokenType::In, "expected 'in' after loop variable");

    auto iterable = parse_expression();
    auto body = parse_block();
    if (!body) return nullptr;

    auto span = Span::merge(for_tok.span, previous().span);
    ForStmt node;
    node.variable = var;
    node.iterable = std::move(iterable);
    node.body = std::move(body);
    return Stmt::make(std::move(node), span);
}

StmtPtr Parser::parse_return_stmt() {
    auto ret_tok = advance(); // Consume 'return'.

    ExprPtr value = nullptr;
    if (!check(TokenType::Semicolon)) {
        value = parse_expression();
    }

    expect(TokenType::Semicolon, "expected ';' after return value");

    auto span = Span::merge(ret_tok.span, previous().span);
    ReturnStmt node;
    node.keyword = ret_tok;
    node.value = std::move(value);
    return Stmt::make(std::move(node), span);
}

StmtPtr Parser::parse_break_stmt() {
    auto tok = advance(); // Consume 'break'.
    expect(TokenType::Semicolon, "expected ';' after 'break'");

    auto span = Span::merge(tok.span, previous().span);
    BreakStmt node;
    node.keyword = tok;
    return Stmt::make(std::move(node), span);
}

StmtPtr Parser::parse_continue_stmt() {
    auto tok = advance(); // Consume 'continue'.
    expect(TokenType::Semicolon, "expected ';' after 'continue'");

    auto span = Span::merge(tok.span, previous().span);
    ContinueStmt node;
    node.keyword = tok;
    return Stmt::make(std::move(node), span);
}

StmtPtr Parser::parse_free_stmt() {
    auto free_tok = advance(); // Consume 'free'.
    expect(TokenType::LParen, "expected '(' after 'free'");

    auto argument = parse_expression();

    expect(TokenType::RParen, "expected ')' after free argument");
    expect(TokenType::Semicolon, "expected ';' after 'free(...)'");

    auto span = Span::merge(free_tok.span, previous().span);
    FreeStmt node;
    node.keyword = free_tok;
    node.argument = std::move(argument);
    return Stmt::make(std::move(node), span);
}

StmtPtr Parser::parse_block() {
    auto open = expect(TokenType::LBrace, "expected '{'");

    std::vector<StmtPtr> stmts;
    while (!check(TokenType::RBrace) && !is_at_end()) {
        auto s = parse_declaration();
        if (s) stmts.push_back(std::move(s));
    }

    expect(TokenType::RBrace, "expected '}' to close block");

    auto span = Span::merge(open.span, previous().span);
    BlockStmt node;
    node.statements = std::move(stmts);
    return Stmt::make(std::move(node), span);
}

StmtPtr Parser::parse_expr_stmt() {
    auto expr = parse_expression();
    if (!expr) {
        // Couldn't parse an expression. Skip this token and try again.
        error("unexpected token '" + peek().lexeme + "'");
        synchronize();
        return nullptr;
    }

    expect(TokenType::Semicolon, "expected ';' after expression");

    auto span = expr->span;
    ExprStmt node;
    node.expression = std::move(expr);
    return Stmt::make(std::move(node), span);
}

// ── Expression parsing ─────────────────────────────────────────────────────────
//
// Precedence (lowest to highest):
//   Assignment  =
//   Or          or
//   And         and
//   Equality    ==  !=
//   Comparison  <  <=  >  >=
//   Addition    +  -
//   Multiply    *  /  %
//   Unary       -  not
//   Postfix     ()  []  .
//   Primary     literals, identifiers, grouped, arrays, new, ref, deref, move

ExprPtr Parser::parse_expression() {
    return parse_assignment();
}

ExprPtr Parser::parse_assignment() {
    auto expr = parse_or();
    if (!expr) return nullptr;

    if (match(TokenType::Eq)) {
        auto eq = previous();
        auto value = parse_assignment(); // Right-associative.

        // Verify the left side is a valid assignment target.
        bool valid = std::visit([](auto&& n) -> bool {
            using T = std::decay_t<decltype(n)>;
            return std::is_same_v<T, IdentifierExpr>
                || std::is_same_v<T, IndexExpr>
                || std::is_same_v<T, FieldAccessExpr>;
        }, expr->data);

        if (!valid) {
            error_at(eq.span, "invalid assignment target");
        }

        auto span = Span::merge(expr->span, value ? value->span : eq.span);
        AssignExpr node;
        node.target = std::move(expr);
        node.eq = eq;
        node.value = std::move(value);
        return Expr::make(std::move(node), span);
    }

    return expr;
}

ExprPtr Parser::parse_or() {
    auto left = parse_and();
    if (!left) return nullptr;

    while (match(TokenType::Or)) {
        auto op = previous();
        auto right = parse_and();
        auto span = Span::merge(left->span, right ? right->span : op.span);
        BinaryExpr node;
        node.left = std::move(left);
        node.op = op;
        node.right = std::move(right);
        left = Expr::make(std::move(node), span);
    }

    return left;
}

ExprPtr Parser::parse_and() {
    auto left = parse_equality();
    if (!left) return nullptr;

    while (match(TokenType::And)) {
        auto op = previous();
        auto right = parse_equality();
        auto span = Span::merge(left->span, right ? right->span : op.span);
        BinaryExpr node;
        node.left = std::move(left);
        node.op = op;
        node.right = std::move(right);
        left = Expr::make(std::move(node), span);
    }

    return left;
}

ExprPtr Parser::parse_equality() {
    auto left = parse_comparison();
    if (!left) return nullptr;

    while (match_any({TokenType::EqEq, TokenType::BangEq})) {
        auto op = previous();
        auto right = parse_comparison();
        auto span = Span::merge(left->span, right ? right->span : op.span);
        BinaryExpr node;
        node.left = std::move(left);
        node.op = op;
        node.right = std::move(right);
        left = Expr::make(std::move(node), span);
    }

    return left;
}

ExprPtr Parser::parse_comparison() {
    auto left = parse_addition();
    if (!left) return nullptr;

    while (match_any({TokenType::Lt, TokenType::LtEq, TokenType::Gt, TokenType::GtEq})) {
        auto op = previous();
        auto right = parse_addition();
        auto span = Span::merge(left->span, right ? right->span : op.span);
        BinaryExpr node;
        node.left = std::move(left);
        node.op = op;
        node.right = std::move(right);
        left = Expr::make(std::move(node), span);
    }

    return left;
}

ExprPtr Parser::parse_addition() {
    auto left = parse_multiplication();
    if (!left) return nullptr;

    while (match_any({TokenType::Plus, TokenType::Minus})) {
        auto op = previous();
        auto right = parse_multiplication();
        auto span = Span::merge(left->span, right ? right->span : op.span);
        BinaryExpr node;
        node.left = std::move(left);
        node.op = op;
        node.right = std::move(right);
        left = Expr::make(std::move(node), span);
    }

    return left;
}

ExprPtr Parser::parse_multiplication() {
    auto left = parse_unary();
    if (!left) return nullptr;

    while (match_any({TokenType::Star, TokenType::Slash, TokenType::Percent})) {
        auto op = previous();
        auto right = parse_unary();
        auto span = Span::merge(left->span, right ? right->span : op.span);
        BinaryExpr node;
        node.left = std::move(left);
        node.op = op;
        node.right = std::move(right);
        left = Expr::make(std::move(node), span);
    }

    return left;
}

ExprPtr Parser::parse_unary() {
    if (match_any({TokenType::Minus, TokenType::Not})) {
        auto op = previous();
        auto operand = parse_unary(); // Right-recursive for chaining: --x, not not x.
        auto span = Span::merge(op.span, operand ? operand->span : op.span);
        UnaryExpr node;
        node.op = op;
        node.operand = std::move(operand);
        return Expr::make(std::move(node), span);
    }

    return parse_postfix();
}

ExprPtr Parser::parse_postfix() {
    auto expr = parse_primary();
    if (!expr) return nullptr;

    // Loop for chained postfix: foo(1)(2), arr[0][1], obj.x.y, etc.
    for (;;) {
        if (match(TokenType::LParen)) {
            // Function call.
            auto paren = previous();
            auto args = parse_arguments();
            auto close = expect(TokenType::RParen, "expected ')' after arguments");
            auto span = Span::merge(expr->span, close.span);
            CallExpr node;
            node.callee = std::move(expr);
            node.paren = paren;
            node.arguments = std::move(args);
            expr = Expr::make(std::move(node), span);
        }
        else if (match(TokenType::LBracket)) {
            // Index access.
            auto bracket = previous();
            auto index = parse_expression();
            auto close = expect(TokenType::RBracket, "expected ']' after index");
            auto span = Span::merge(expr->span, close.span);
            IndexExpr node;
            node.object = std::move(expr);
            node.bracket = bracket;
            node.index = std::move(index);
            expr = Expr::make(std::move(node), span);
        }
        else if (match(TokenType::Dot)) {
            // Field access.
            auto& name = expect(TokenType::Identifier, "expected field name after '.'");
            auto span = Span::merge(expr->span, name.span);
            FieldAccessExpr node;
            node.object = std::move(expr);
            node.name = name;
            expr = Expr::make(std::move(node), span);
        }
        else {
            break;
        }
    }

    return expr;
}

std::vector<ExprPtr> Parser::parse_arguments() {
    std::vector<ExprPtr> args;
    if (!check(TokenType::RParen)) {
        do {
            args.push_back(parse_expression());
        } while (match(TokenType::Comma));
    }
    return args;
}

ExprPtr Parser::parse_primary() {
    // ── Literals ───────────────────────────────────────────────────────────────
    if (match_any({TokenType::IntLiteral, TokenType::FloatLiteral,
                   TokenType::StringLiteral, TokenType::True,
                   TokenType::False, TokenType::Nil})) {
        auto& tok = previous();
        LiteralExpr node;
        node.value = tok;
        return Expr::make(std::move(node), tok.span);
    }

    // ── Identifier ─────────────────────────────────────────────────────────────
    if (match(TokenType::Identifier)) {
        auto& tok = previous();
        IdentifierExpr node;
        node.name = tok;
        return Expr::make(std::move(node), tok.span);
    }

    // ── Grouped expression: (expr) ─────────────────────────────────────────────
    if (match(TokenType::LParen)) {
        auto open = previous();
        auto expr = parse_expression();
        expect(TokenType::RParen, "expected ')' after expression");
        // Preserve the span of the parenthesized expression itself, not the parens.
        return expr;
    }

    // ── Array literal: [expr, expr, ...] ───────────────────────────────────────
    if (match(TokenType::LBracket)) {
        auto bracket = previous();
        std::vector<ExprPtr> elements;
        if (!check(TokenType::RBracket)) {
            do {
                elements.push_back(parse_expression());
            } while (match(TokenType::Comma));
        }
        auto close = expect(TokenType::RBracket, "expected ']' to close array literal");
        auto span = Span::merge(bracket.span, close.span);
        ArrayExpr node;
        node.bracket = bracket;
        node.elements = std::move(elements);
        return Expr::make(std::move(node), span);
    }

    // ── new TypeName { field: value, ... } ─────────────────────────────────────
    if (match(TokenType::New)) {
        auto kw = previous();
        auto& type_name = expect(TokenType::Identifier, "expected type name after 'new'");

        expect(TokenType::LBrace, "expected '{' after type name in 'new' expression");

        std::vector<std::pair<Token, ExprPtr>> fields;
        if (!check(TokenType::RBrace)) {
            do {
                auto& fname = expect(TokenType::Identifier, "expected field name");
                expect(TokenType::Colon, "expected ':' after field name");
                auto fvalue = parse_expression();
                fields.emplace_back(fname, std::move(fvalue));
            } while (match(TokenType::Comma));
        }

        auto close = expect(TokenType::RBrace, "expected '}' to close struct literal");
        auto span = Span::merge(kw.span, close.span);
        NewExpr node;
        node.keyword = kw;
        node.type_name = type_name;
        node.fields = std::move(fields);
        return Expr::make(std::move(node), span);
    }

    // ── ref(expr) ──────────────────────────────────────────────────────────────
    if (match(TokenType::Ref)) {
        auto kw = previous();
        expect(TokenType::LParen, "expected '(' after 'ref'");
        auto operand = parse_expression();
        auto close = expect(TokenType::RParen, "expected ')' after ref argument");
        auto span = Span::merge(kw.span, close.span);
        RefExpr node;
        node.keyword = kw;
        node.operand = std::move(operand);
        return Expr::make(std::move(node), span);
    }

    // ── deref(expr) ────────────────────────────────────────────────────────────
    if (match(TokenType::Deref)) {
        auto kw = previous();
        expect(TokenType::LParen, "expected '(' after 'deref'");
        auto operand = parse_expression();
        auto close = expect(TokenType::RParen, "expected ')' after deref argument");
        auto span = Span::merge(kw.span, close.span);
        DerefExpr node;
        node.keyword = kw;
        node.operand = std::move(operand);
        return Expr::make(std::move(node), span);
    }

    // ── move(expr) ─────────────────────────────────────────────────────────────
    if (match(TokenType::Move)) {
        auto kw = previous();
        expect(TokenType::LParen, "expected '(' after 'move'");
        auto operand = parse_expression();
        auto close = expect(TokenType::RParen, "expected ')' after move argument");
        auto span = Span::merge(kw.span, close.span);
        MoveExpr node;
        node.keyword = kw;
        node.operand = std::move(operand);
        return Expr::make(std::move(node), span);
    }

    // ── Error ──────────────────────────────────────────────────────────────────
    error("expected expression, got '" + peek().lexeme + "'");
    synchronize();
    return nullptr;
}

} // namespace crashlang
