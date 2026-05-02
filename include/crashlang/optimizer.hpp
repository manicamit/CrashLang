#ifndef CRASHLANG_OPTIMIZER_HPP
#define CRASHLANG_OPTIMIZER_HPP

#include "crashlang/ast.hpp"

namespace crashlang {

class Optimizer {
public:
    void optimize(Program& program);
    int optimizations_applied() const { return count_; }

private:
    int count_ = 0;

    void optimize_stmt(StmtPtr& stmt);
    void optimize_expr(ExprPtr& expr);
    bool try_fold_binary(ExprPtr& expr, const BinaryExpr& bin, const Span& span);
    bool try_fold_unary(ExprPtr& expr, const UnaryExpr& un, const Span& span);
};

} // namespace crashlang

#endif
