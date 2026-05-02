#ifndef CRASHLANG_OPTIMIZER_HPP
#define CRASHLANG_OPTIMIZER_HPP

/// AST-level optimizer for CrashLang.
///
/// Performs source-level optimizations on the parsed AST before compilation.
/// Currently implements:
///   1. Constant folding — evaluate binary/unary expressions with literal
///      operands at compile time (e.g., 3 + 4 → 7).
///   2. Dead branch elimination — simplify `if true { ... }` to just the body.

#include "crashlang/ast.hpp"

namespace crashlang {

class Optimizer {
public:
    /// Optimize a fully parsed program in-place.
    void optimize(Program& program);

    /// Count of optimizations performed (for diagnostics).
    int optimizations_applied() const { return count_; }

private:
    int count_ = 0;

    void optimize_stmt(StmtPtr& stmt);
    void optimize_expr(ExprPtr& expr);

    /// Try to fold a binary expression with literal operands.
    /// Returns true if the expression was replaced.
    bool try_fold_binary(ExprPtr& expr, const BinaryExpr& bin, const Span& span);

    /// Try to fold a unary expression with a literal operand.
    bool try_fold_unary(ExprPtr& expr, const UnaryExpr& un, const Span& span);
};

} // namespace crashlang

#endif // CRASHLANG_OPTIMIZER_HPP
