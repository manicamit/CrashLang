#include "crashlang/ast.hpp"

#include <sstream>

namespace crashlang {

// ── Helpers ────────────────────────────────────────────────────────────────────

static std::string indent_str(int level) {
    return std::string(static_cast<size_t>(level * 2), ' ');
}

// ── format_expr ────────────────────────────────────────────────────────────────

std::string format_expr(const Expr& expr, int indent) {
    std::string pad = indent_str(indent);

    return std::visit([&](auto&& node) -> std::string {
        using T = std::decay_t<decltype(node)>;

        if constexpr (std::is_same_v<T, LiteralExpr>) {
            switch (node.value.type) {
                case TokenType::IntLiteral:
                    return pad + "Int(" + node.value.lexeme + ")";
                case TokenType::FloatLiteral:
                    return pad + "Float(" + node.value.lexeme + ")";
                case TokenType::StringLiteral:
                    return pad + "String(" + node.value.lexeme + ")";
                case TokenType::True:
                    return pad + "Bool(true)";
                case TokenType::False:
                    return pad + "Bool(false)";
                case TokenType::Nil:
                    return pad + "Nil";
                default:
                    return pad + "Literal(?)";
            }
        }
        else if constexpr (std::is_same_v<T, IdentifierExpr>) {
            return pad + "Ident(" + node.name.lexeme + ")";
        }
        else if constexpr (std::is_same_v<T, UnaryExpr>) {
            std::string result = pad + "Unary(" + node.op.lexeme + ")\n";
            result += format_expr(*node.operand, indent + 1);
            return result;
        }
        else if constexpr (std::is_same_v<T, BinaryExpr>) {
            std::string result = pad + "Binary(" + node.op.lexeme + ")\n";
            result += format_expr(*node.left, indent + 1) + "\n";
            result += format_expr(*node.right, indent + 1);
            return result;
        }
        else if constexpr (std::is_same_v<T, CallExpr>) {
            std::string result = pad + "Call\n";
            result += format_expr(*node.callee, indent + 1) + "\n";
            result += pad + "  Args:";
            for (const auto& arg : node.arguments) {
                result += "\n" + format_expr(*arg, indent + 2);
            }
            return result;
        }
        else if constexpr (std::is_same_v<T, IndexExpr>) {
            std::string result = pad + "Index\n";
            result += format_expr(*node.object, indent + 1) + "\n";
            result += format_expr(*node.index, indent + 1);
            return result;
        }
        else if constexpr (std::is_same_v<T, FieldAccessExpr>) {
            std::string result = pad + "FieldAccess(." + node.name.lexeme + ")\n";
            result += format_expr(*node.object, indent + 1);
            return result;
        }
        else if constexpr (std::is_same_v<T, AssignExpr>) {
            std::string result = pad + "Assign\n";
            result += format_expr(*node.target, indent + 1) + "\n";
            result += format_expr(*node.value, indent + 1);
            return result;
        }
        else if constexpr (std::is_same_v<T, ArrayExpr>) {
            std::string result = pad + "Array";
            for (const auto& elem : node.elements) {
                result += "\n" + format_expr(*elem, indent + 1);
            }
            return result;
        }
        else if constexpr (std::is_same_v<T, NewExpr>) {
            std::string result = pad + "New(" + node.type_name.lexeme + ")";
            for (const auto& [name, value] : node.fields) {
                result += "\n" + indent_str(indent + 1) + name.lexeme + ":";
                result += "\n" + format_expr(*value, indent + 2);
            }
            return result;
        }
        else if constexpr (std::is_same_v<T, RefExpr>) {
            std::string result = pad + "Ref\n";
            result += format_expr(*node.operand, indent + 1);
            return result;
        }
        else if constexpr (std::is_same_v<T, DerefExpr>) {
            std::string result = pad + "Deref\n";
            result += format_expr(*node.operand, indent + 1);
            return result;
        }
        else if constexpr (std::is_same_v<T, MoveExpr>) {
            std::string result = pad + "Move\n";
            result += format_expr(*node.operand, indent + 1);
            return result;
        }
        else if constexpr (std::is_same_v<T, LambdaExpr>) {
            std::string result = pad + "Lambda(";
            for (size_t i = 0; i < node.params.size(); ++i) {
                if (i > 0) result += ", ";
                result += node.params[i].lexeme;
            }
            result += ")\n";
            result += format_stmt(*node.body, indent + 1);
            return result;
        }
        else {
            return pad + "<unknown expr>";
        }
    }, expr.data);
}

// ── format_stmt ────────────────────────────────────────────────────────────────

std::string format_stmt(const Stmt& stmt, int indent) {
    std::string pad = indent_str(indent);

    return std::visit([&](auto&& node) -> std::string {
        using T = std::decay_t<decltype(node)>;

        if constexpr (std::is_same_v<T, ExprStmt>) {
            return pad + "ExprStmt\n" + format_expr(*node.expression, indent + 1);
        }
        else if constexpr (std::is_same_v<T, LetStmt>) {
            std::string result = pad + "Let(" + node.name.lexeme + ")";
            if (node.initializer) {
                result += "\n" + format_expr(*node.initializer, indent + 1);
            }
            return result;
        }
        else if constexpr (std::is_same_v<T, BlockStmt>) {
            std::string result = pad + "Block";
            for (const auto& s : node.statements) {
                result += "\n" + format_stmt(*s, indent + 1);
            }
            return result;
        }
        else if constexpr (std::is_same_v<T, IfStmt>) {
            std::string result = pad + "If\n";
            result += pad + "  Cond:\n" + format_expr(*node.condition, indent + 2) + "\n";
            result += pad + "  Then:\n" + format_stmt(*node.then_branch, indent + 2);
            if (node.else_branch) {
                result += "\n" + pad + "  Else:\n" + format_stmt(*node.else_branch, indent + 2);
            }
            return result;
        }
        else if constexpr (std::is_same_v<T, WhileStmt>) {
            std::string result = pad + "While\n";
            result += pad + "  Cond:\n" + format_expr(*node.condition, indent + 2) + "\n";
            result += pad + "  Body:\n" + format_stmt(*node.body, indent + 2);
            return result;
        }
        else if constexpr (std::is_same_v<T, ForStmt>) {
            std::string result = pad + "For(" + node.variable.lexeme + ")\n";
            result += pad + "  In:\n" + format_expr(*node.iterable, indent + 2) + "\n";
            result += pad + "  Body:\n" + format_stmt(*node.body, indent + 2);
            return result;
        }
        else if constexpr (std::is_same_v<T, FnStmt>) {
            std::string result = pad + "Fn(" + node.name.lexeme + ")";
            result += "(";
            for (size_t i = 0; i < node.params.size(); ++i) {
                if (i > 0) result += ", ";
                result += node.params[i].lexeme;
            }
            result += ")\n";
            result += format_stmt(*node.body, indent + 1);
            return result;
        }
        else if constexpr (std::is_same_v<T, ReturnStmt>) {
            std::string result = pad + "Return";
            if (node.value) {
                result += "\n" + format_expr(*node.value, indent + 1);
            }
            return result;
        }
        else if constexpr (std::is_same_v<T, BreakStmt>) {
            return pad + "Break";
        }
        else if constexpr (std::is_same_v<T, ContinueStmt>) {
            return pad + "Continue";
        }
        else if constexpr (std::is_same_v<T, FreeStmt>) {
            std::string result = pad + "Free\n";
            result += format_expr(*node.argument, indent + 1);
            return result;
        }
        else {
            return pad + "<unknown stmt>";
        }
    }, stmt.data);
}

// ── format_ast ─────────────────────────────────────────────────────────────────

std::string format_ast(const Program& program) {
    std::string result = "Program";
    for (const auto& s : program.statements) {
        result += "\n" + format_stmt(*s, 1);
    }
    return result;
}

} // namespace crashlang
