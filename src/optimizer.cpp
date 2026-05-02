#include "crashlang/optimizer.hpp"
#include "crashlang/token.hpp"

#include <cmath>

namespace crashlang {

// ── Public API ─────────────────────────────────────────────────────────────────

void Optimizer::optimize(Program& program) {
    for (auto& stmt : program.statements) {
        optimize_stmt(stmt);
    }
}

// ── Statement optimization ─────────────────────────────────────────────────────

void Optimizer::optimize_stmt(StmtPtr& stmt) {
    if (!stmt) return;

    std::visit([&](auto&& node) {
        using T = std::decay_t<decltype(node)>;

        if constexpr (std::is_same_v<T, ExprStmt>) {
            optimize_expr(node.expression);
        }
        else if constexpr (std::is_same_v<T, LetStmt>) {
            if (node.initializer) optimize_expr(node.initializer);
        }
        else if constexpr (std::is_same_v<T, BlockStmt>) {
            for (auto& s : node.statements) optimize_stmt(s);
        }
        else if constexpr (std::is_same_v<T, IfStmt>) {
            optimize_expr(node.condition);
            optimize_stmt(node.then_branch);
            if (node.else_branch) optimize_stmt(node.else_branch);

            // Dead branch elimination: if condition is a known literal.
            if (auto* lit = std::get_if<LiteralExpr>(&node.condition->data)) {
                if (lit->value.type == TokenType::True) {
                    // Replace the entire if statement with just the then branch.
                    stmt = std::move(node.then_branch);
                    count_++;
                } else if (lit->value.type == TokenType::False) {
                    if (node.else_branch) {
                        stmt = std::move(node.else_branch);
                    } else {
                        // Replace with empty block.
                        BlockStmt empty;
                        stmt = Stmt::make(std::move(empty), stmt->span);
                    }
                    count_++;
                }
            }
        }
        else if constexpr (std::is_same_v<T, WhileStmt>) {
            optimize_expr(node.condition);
            optimize_stmt(node.body);
        }
        else if constexpr (std::is_same_v<T, ForStmt>) {
            optimize_expr(node.iterable);
            optimize_stmt(node.body);
        }
        else if constexpr (std::is_same_v<T, FnStmt>) {
            optimize_stmt(node.body);
        }
        else if constexpr (std::is_same_v<T, ReturnStmt>) {
            if (node.value) optimize_expr(node.value);
        }
        else if constexpr (std::is_same_v<T, FreeStmt>) {
            optimize_expr(node.argument);
        }
    }, stmt->data);
}

// ── Expression optimization ────────────────────────────────────────────────────

void Optimizer::optimize_expr(ExprPtr& expr) {
    if (!expr) return;

    // First optimize children.
    std::visit([&](auto&& node) {
        using T = std::decay_t<decltype(node)>;

        if constexpr (std::is_same_v<T, UnaryExpr>) {
            optimize_expr(node.operand);
        }
        else if constexpr (std::is_same_v<T, BinaryExpr>) {
            optimize_expr(node.left);
            optimize_expr(node.right);
        }
        else if constexpr (std::is_same_v<T, CallExpr>) {
            optimize_expr(node.callee);
            for (auto& arg : node.arguments) optimize_expr(arg);
        }
        else if constexpr (std::is_same_v<T, IndexExpr>) {
            optimize_expr(node.object);
            optimize_expr(node.index);
        }
        else if constexpr (std::is_same_v<T, AssignExpr>) {
            optimize_expr(node.value);
        }
        else if constexpr (std::is_same_v<T, ArrayExpr>) {
            for (auto& elem : node.elements) optimize_expr(elem);
        }
        else if constexpr (std::is_same_v<T, MatchExpr>) {
            optimize_expr(node.target);
            for (auto& arm : node.arms) {
                if (arm.pattern) optimize_expr(arm.pattern);
                optimize_expr(arm.body);
            }
        }
    }, expr->data);

    // Now try to fold this expression itself.
    auto& span = expr->span;
    if (auto* bin = std::get_if<BinaryExpr>(&expr->data)) {
        try_fold_binary(expr, *bin, span);
    } else if (auto* un = std::get_if<UnaryExpr>(&expr->data)) {
        try_fold_unary(expr, *un, span);
    }
}

// ── Constant folding: binary ───────────────────────────────────────────────────

bool Optimizer::try_fold_binary(ExprPtr& expr, const BinaryExpr& bin, const Span& span) {
    auto* left_lit  = std::get_if<LiteralExpr>(&bin.left->data);
    auto* right_lit = std::get_if<LiteralExpr>(&bin.right->data);
    if (!left_lit || !right_lit) return false;

    auto op = bin.op.type;

    // Integer + Integer
    if (left_lit->value.type == TokenType::IntLiteral &&
        right_lit->value.type == TokenType::IntLiteral)
    {
        int64_t a = std::get<int64_t>(left_lit->value.literal);
        int64_t b = std::get<int64_t>(right_lit->value.literal);
        int64_t result = 0;
        bool is_bool = false;
        bool bool_result = false;

        switch (op) {
            case TokenType::Plus:    result = a + b; break;
            case TokenType::Minus:   result = a - b; break;
            case TokenType::Star:    result = a * b; break;
            case TokenType::Slash:   if (b == 0) return false; result = a / b; break;
            case TokenType::Percent: if (b == 0) return false; result = a % b; break;
            case TokenType::EqEq:    is_bool = true; bool_result = (a == b); break;
            case TokenType::BangEq:  is_bool = true; bool_result = (a != b); break;
            case TokenType::Lt:      is_bool = true; bool_result = (a < b); break;
            case TokenType::LtEq:    is_bool = true; bool_result = (a <= b); break;
            case TokenType::Gt:      is_bool = true; bool_result = (a > b); break;
            case TokenType::GtEq:    is_bool = true; bool_result = (a >= b); break;
            default: return false;
        }

        LiteralExpr folded;
        if (is_bool) {
            folded.value = Token::make(bool_result ? TokenType::True : TokenType::False,
                                       bool_result ? "true" : "false", span);
        } else {
            folded.value = Token::make_int(result, std::to_string(result), span);
        }
        expr = Expr::make(std::move(folded), span);
        count_++;
        return true;
    }

    // Float + Float (or mixed int/float)
    bool left_numeric = left_lit->value.type == TokenType::IntLiteral ||
                        left_lit->value.type == TokenType::FloatLiteral;
    bool right_numeric = right_lit->value.type == TokenType::IntLiteral ||
                         right_lit->value.type == TokenType::FloatLiteral;
    if (left_numeric && right_numeric &&
        (left_lit->value.type == TokenType::FloatLiteral ||
         right_lit->value.type == TokenType::FloatLiteral))
    {
        double a = (left_lit->value.type == TokenType::IntLiteral)
            ? static_cast<double>(std::get<int64_t>(left_lit->value.literal))
            : std::get<double>(left_lit->value.literal);
        double b = (right_lit->value.type == TokenType::IntLiteral)
            ? static_cast<double>(std::get<int64_t>(right_lit->value.literal))
            : std::get<double>(right_lit->value.literal);
        double result = 0;

        switch (op) {
            case TokenType::Plus:    result = a + b; break;
            case TokenType::Minus:   result = a - b; break;
            case TokenType::Star:    result = a * b; break;
            case TokenType::Slash:   if (b == 0.0) return false; result = a / b; break;
            default: return false;
        }

        LiteralExpr folded;
        folded.value = Token::make_float(result, std::to_string(result), span);
        expr = Expr::make(std::move(folded), span);
        count_++;
        return true;
    }

    // String + String concatenation.
    if (left_lit->value.type == TokenType::StringLiteral &&
        right_lit->value.type == TokenType::StringLiteral &&
        op == TokenType::Plus)
    {
        std::string a = std::get<std::string>(left_lit->value.literal);
        std::string b = std::get<std::string>(right_lit->value.literal);
        std::string result = a + b;
        LiteralExpr folded;
        folded.value = Token::make_string(result, "\"" + result + "\"", span);
        expr = Expr::make(std::move(folded), span);
        count_++;
        return true;
    }

    // Boolean logic: true and false => false, true or false => true, etc.
    if ((left_lit->value.type == TokenType::True || left_lit->value.type == TokenType::False) &&
        (right_lit->value.type == TokenType::True || right_lit->value.type == TokenType::False))
    {
        bool a = (left_lit->value.type == TokenType::True);
        bool b = (right_lit->value.type == TokenType::True);
        bool result = false;

        if (op == TokenType::And) result = a && b;
        else if (op == TokenType::Or) result = a || b;
        else if (op == TokenType::EqEq) result = (a == b);
        else if (op == TokenType::BangEq) result = (a != b);
        else return false;

        LiteralExpr folded;
        folded.value = Token::make(result ? TokenType::True : TokenType::False,
                                   result ? "true" : "false", span);
        expr = Expr::make(std::move(folded), span);
        count_++;
        return true;
    }

    return false;
}

// ── Constant folding: unary ────────────────────────────────────────────────────

bool Optimizer::try_fold_unary(ExprPtr& expr, const UnaryExpr& un, const Span& span) {
    auto* operand_lit = std::get_if<LiteralExpr>(&un.operand->data);
    if (!operand_lit) return false;

    auto op = un.op.type;

    // Negate integer: -(42) => -42
    if (op == TokenType::Minus && operand_lit->value.type == TokenType::IntLiteral) {
        int64_t val = std::get<int64_t>(operand_lit->value.literal);
        LiteralExpr folded;
        folded.value = Token::make_int(-val, std::to_string(-val), span);
        expr = Expr::make(std::move(folded), span);
        count_++;
        return true;
    }

    // Negate float: -(3.14) => -3.14
    if (op == TokenType::Minus && operand_lit->value.type == TokenType::FloatLiteral) {
        double val = std::get<double>(operand_lit->value.literal);
        LiteralExpr folded;
        folded.value = Token::make_float(-val, std::to_string(-val), span);
        expr = Expr::make(std::move(folded), span);
        count_++;
        return true;
    }

    // Boolean not: not true => false
    if (op == TokenType::Not &&
        (operand_lit->value.type == TokenType::True || operand_lit->value.type == TokenType::False))
    {
        bool val = (operand_lit->value.type == TokenType::True);
        LiteralExpr folded;
        folded.value = Token::make(!val ? TokenType::True : TokenType::False,
                                   !val ? "true" : "false", span);
        expr = Expr::make(std::move(folded), span);
        count_++;
        return true;
    }

    return false;
}

} // namespace crashlang
