#include "crashlang/interpreter.hpp"
#include "crashlang/builtins.hpp"

#include <sstream>

namespace crashlang {
Interpreter::Interpreter(const SourceFile& source)
    : source_(source)
{
    // Register all built-in functions into the global scope.
    register_builtins(env_);

    // Allow builtins like map/filter/forEach to call user-defined functions
    // by routing through this interpreter's call_function method.
    set_user_fn_caller([this](const Value& fn_val, std::vector<Value> args, Span span) -> Value {
        if (fn_val.is_function()) {
            return call_function(fn_val.as_function(), std::move(args), span);
        }
        auto err = make_error(CrashKind::NotCallable, span,
            "expected a function, got " + std::string(value_type_name(fn_val)));
        throw err;
    });
}
void Interpreter::execute(const Program& program) {
    try {
        for (const auto& stmt : program.statements) {
            execute_stmt(*stmt);
        }
    } catch (const CrashError& err) {
        crashed_ = true;
        throw; // Re-throw — main.cpp handles formatting and display.
    }
}
void Interpreter::execute_stmt(const Stmt& stmt) {
    std::visit([this, &stmt](auto&& node) {
        using T = std::decay_t<decltype(node)>;

        if constexpr (std::is_same_v<T, ExprStmt>)      { evaluate(*node.expression); }
        else if constexpr (std::is_same_v<T, LetStmt>)  { execute_let(node); }
        else if constexpr (std::is_same_v<T, BlockStmt>) { execute_block(node); }
        else if constexpr (std::is_same_v<T, IfStmt>)   { execute_if(node); }
        else if constexpr (std::is_same_v<T, WhileStmt>) { execute_while(node); }
        else if constexpr (std::is_same_v<T, ForStmt>)   { execute_for(node); }
        else if constexpr (std::is_same_v<T, FnStmt>)    { execute_fn(node); }
        else if constexpr (std::is_same_v<T, ReturnStmt>) { execute_return(node); }
        else if constexpr (std::is_same_v<T, BreakStmt>)    { execute_break(node); }
        else if constexpr (std::is_same_v<T, ContinueStmt>) { execute_continue(node); }
        else if constexpr (std::is_same_v<T, FreeStmt>)     { execute_free(node); }
    }, stmt.data);
}
void Interpreter::execute_block(const BlockStmt& block) {
    env_.push_scope();
    try {
        for (const auto& s : block.statements) {
            execute_stmt(*s);
        }
    } catch (...) {
        env_.pop_scope();
        throw;
    }
    env_.pop_scope();
}

void Interpreter::execute_let(const LetStmt& stmt) {
    Value val;
    if (stmt.initializer) {
        val = evaluate(*stmt.initializer);
    }

    // If the value is a heap ref, bind the variable name to it in the
    // ownership tracker so we can trace ownership transfers.
    if (val.is_heap_ref()) {
        HeapID id = val.as_heap_ref().id;
        ownership_.bind_variable(stmt.name.lexeme, id);
        ownership_.set_owner(id, stmt.name.lexeme);
    }

    env_.define(stmt.name.lexeme, std::move(val));
}

void Interpreter::execute_if(const IfStmt& stmt) {
    Value cond = evaluate(*stmt.condition);
    if (value_is_truthy(cond)) {
        execute_stmt(*stmt.then_branch);
    } else if (stmt.else_branch) {
        execute_stmt(*stmt.else_branch);
    }
}

void Interpreter::execute_while(const WhileStmt& stmt) {
    while (true) {
        Value cond = evaluate(*stmt.condition);
        if (!value_is_truthy(cond)) break;

        try {
            execute_stmt(*stmt.body);
        } catch (const BreakSignal&) {
            break;
        } catch (const ContinueSignal&) {
            continue;
        }
    }
}

void Interpreter::execute_for(const ForStmt& stmt) {
    Value iterable = evaluate(*stmt.iterable);

    // String iteration: for ch in "hello" { ... }
    if (iterable.is_string()) {
        const auto& str = iterable.as_string();
        for (size_t i = 0; i < str.size(); ++i) {
            env_.push_scope();
            env_.define(stmt.variable.lexeme, Value(std::string(1, str[i])));

            try {
                execute_stmt(*stmt.body);
            } catch (const BreakSignal&) {
                env_.pop_scope();
                return;
            } catch (const ContinueSignal&) {
                env_.pop_scope();
                continue;
            } catch (...) {
                env_.pop_scope();
                throw;
            }

            env_.pop_scope();
        }
        return;
    }

    if (!iterable.is_array()) {
        throw make_error(CrashKind::TypeMismatch, stmt.variable.span,
            "for-in loop requires an array or string, got " + std::string(value_type_name(iterable)));
    }

    const auto& elements = *iterable.as_array().elements;
    for (const auto& elem : elements) {
        env_.push_scope();
        env_.define(stmt.variable.lexeme, elem);

        try {
            execute_stmt(*stmt.body);
        } catch (const BreakSignal&) {
            env_.pop_scope();
            return;
        } catch (const ContinueSignal&) {
            env_.pop_scope();
            continue;
        } catch (...) {
            env_.pop_scope();
            throw;
        }

        env_.pop_scope();
    }
}

void Interpreter::execute_fn(const FnStmt& stmt) {
    // Capture the current scope as the closure environment.
    FunctionValue fn;
    fn.name    = stmt.name.lexeme;
    fn.body    = stmt.body.get();
    fn.closure = env_.current();

    for (const auto& param : stmt.params) {
        fn.params.push_back(param.lexeme);
    }

    env_.define(stmt.name.lexeme, Value(std::move(fn)));
}

void Interpreter::execute_return(const ReturnStmt& stmt) {
    Value val;
    if (stmt.value) {
        val = evaluate(*stmt.value);
    }
    throw ReturnSignal{std::move(val), stmt.keyword.span};
}

void Interpreter::execute_break(const BreakStmt& stmt) {
    throw BreakSignal{stmt.keyword.span};
}

void Interpreter::execute_continue(const ContinueStmt& stmt) {
    throw ContinueSignal{stmt.keyword.span};
}

void Interpreter::execute_free(const FreeStmt& stmt) {
    Value val = evaluate(*stmt.argument);

    if (!val.is_heap_ref()) {
        throw make_error(CrashKind::TypeMismatch, stmt.keyword.span,
            "free() expects a heap reference, got " + std::string(value_type_name(val)));
    }

    HeapID id = val.as_heap_ref().id;
    auto fn_name = current_function_name();

    // HeapSimulator::free() throws DoubleFree if already dead.
    heap_.free(id, stmt.keyword.span, fn_name);
    ownership_.record_free(id, stmt.keyword.span, fn_name);
}
Value Interpreter::evaluate(const Expr& expr) {
    return std::visit([this](auto&& node) -> Value {
        using T = std::decay_t<decltype(node)>;

        if constexpr (std::is_same_v<T, LiteralExpr>)      return eval_literal(node);
        else if constexpr (std::is_same_v<T, IdentifierExpr>)  return eval_identifier(node);
        else if constexpr (std::is_same_v<T, UnaryExpr>)       return eval_unary(node);
        else if constexpr (std::is_same_v<T, BinaryExpr>)      return eval_binary(node);
        else if constexpr (std::is_same_v<T, CallExpr>)        return eval_call(node);
        else if constexpr (std::is_same_v<T, IndexExpr>)       return eval_index(node);
        else if constexpr (std::is_same_v<T, FieldAccessExpr>) return eval_field_access(node);
        else if constexpr (std::is_same_v<T, AssignExpr>)      return eval_assign(node);
        else if constexpr (std::is_same_v<T, ArrayExpr>)       return eval_array(node);
        else if constexpr (std::is_same_v<T, NewExpr>)         return eval_new(node);
        else if constexpr (std::is_same_v<T, RefExpr>)         return eval_ref(node);
        else if constexpr (std::is_same_v<T, DerefExpr>)       return eval_deref(node);
        else if constexpr (std::is_same_v<T, MoveExpr>)        return eval_move(node);
        else if constexpr (std::is_same_v<T, LambdaExpr>)      return eval_lambda(node);
        else if constexpr (std::is_same_v<T, MatchExpr>)       return eval_match(node);
        else return Value{};
    }, expr.data);
}
Value Interpreter::eval_literal(const LiteralExpr& expr) {
    switch (expr.value.type) {
        case TokenType::IntLiteral:
            return Value(std::get<int64_t>(expr.value.literal));
        case TokenType::FloatLiteral:
            return Value(std::get<double>(expr.value.literal));
        case TokenType::StringLiteral:
            return Value(std::get<std::string>(expr.value.literal));
        case TokenType::True:
            return Value(true);
        case TokenType::False:
            return Value(false);
        case TokenType::Nil:
            return Value(NilType{});
        default:
            return Value{};
    }
}
Value Interpreter::eval_identifier(const IdentifierExpr& expr) {
    auto val = env_.get(expr.name.lexeme);
    if (!val) {
        auto err = make_error(CrashKind::UndefinedVariable, expr.name.span,
            "variable '" + expr.name.lexeme + "' is not defined");

        err.metadata["variable"] = expr.name.lexeme;

        auto suggestion = env_.find_closest(expr.name.lexeme);
        if (suggestion) {
            err.metadata["suggestion"] = *suggestion;
            err.detail += " (did you mean '" + *suggestion + "'?)";
        }
        throw err;
    }
    return *val;
}
Value Interpreter::eval_unary(const UnaryExpr& expr) {
    Value operand = evaluate(*expr.operand);

    if (expr.op.type == TokenType::Minus) {
        if (operand.is_int())   return Value(-operand.as_int());
        if (operand.is_float()) return Value(-operand.as_float());
        type_error("-", operand, "int or float", expr.op.span);
    }

    if (expr.op.type == TokenType::Not) {
        return Value(!value_is_truthy(operand));
    }

    return Value{};
}
Value Interpreter::eval_binary(const BinaryExpr& expr) {
    // Short-circuit logical operators.
    if (expr.op.type == TokenType::And) {
        Value left = evaluate(*expr.left);
        if (!value_is_truthy(left)) return Value(false);
        Value right = evaluate(*expr.right);
        return Value(value_is_truthy(right));
    }
    if (expr.op.type == TokenType::Or) {
        Value left = evaluate(*expr.left);
        if (value_is_truthy(left)) return Value(true);
        Value right = evaluate(*expr.right);
        return Value(value_is_truthy(right));
    }

    Value left  = evaluate(*expr.left);
    Value right = evaluate(*expr.right);
    Span  span  = expr.op.span;

    switch (expr.op.type) {
        // ── Arithmetic ─────────────────────────────────────────────────────────
        case TokenType::Plus:
            if (left.is_int() && right.is_int())
                return Value(left.as_int() + right.as_int());
            if (left.is_numeric() && right.is_numeric())
                return Value(left.to_float() + right.to_float());
            if (left.is_string() && right.is_string())
                return Value(left.as_string() + right.as_string());
            // int + string or string + int → concatenation via to_string
            if (left.is_string())
                return Value(left.as_string() + value_to_string(right));
            if (right.is_string())
                return Value(value_to_string(left) + right.as_string());
            binary_type_error("+", left, right, span);

        case TokenType::Minus:
            if (left.is_int() && right.is_int())
                return Value(left.as_int() - right.as_int());
            if (left.is_numeric() && right.is_numeric())
                return Value(left.to_float() - right.to_float());
            binary_type_error("-", left, right, span);

        case TokenType::Star:
            if (left.is_int() && right.is_int())
                return Value(left.as_int() * right.as_int());
            if (left.is_numeric() && right.is_numeric())
                return Value(left.to_float() * right.to_float());
            binary_type_error("*", left, right, span);

        case TokenType::Slash: {
            if (left.is_numeric() && right.is_numeric()) {
                if (right.is_int() && right.as_int() == 0)
                    throw make_error(CrashKind::DivisionByZero, span,
                        "division by zero");
                if (right.is_float() && right.as_float() == 0.0)
                    throw make_error(CrashKind::DivisionByZero, span,
                        "division by zero");

                if (left.is_int() && right.is_int())
                    return Value(left.as_int() / right.as_int());
                return Value(left.to_float() / right.to_float());
            }
            binary_type_error("/", left, right, span);
        }

        case TokenType::Percent: {
            if (left.is_int() && right.is_int()) {
                if (right.as_int() == 0)
                    throw make_error(CrashKind::DivisionByZero, span,
                        "modulo by zero");
                return Value(left.as_int() % right.as_int());
            }
            binary_type_error("%", left, right, span);
        }

        // ── Comparison ─────────────────────────────────────────────────────────
        case TokenType::EqEq:  return Value(values_equal(left, right));
        case TokenType::BangEq: return Value(!values_equal(left, right));

        case TokenType::Lt:
            if (left.is_int()    && right.is_int())    return Value(left.as_int() < right.as_int());
            if (left.is_numeric() && right.is_numeric()) return Value(left.to_float() < right.to_float());
            if (left.is_string() && right.is_string()) return Value(left.as_string() < right.as_string());
            binary_type_error("<", left, right, span);

        case TokenType::LtEq:
            if (left.is_int()    && right.is_int())    return Value(left.as_int() <= right.as_int());
            if (left.is_numeric() && right.is_numeric()) return Value(left.to_float() <= right.to_float());
            if (left.is_string() && right.is_string()) return Value(left.as_string() <= right.as_string());
            binary_type_error("<=", left, right, span);

        case TokenType::Gt:
            if (left.is_int()    && right.is_int())    return Value(left.as_int() > right.as_int());
            if (left.is_numeric() && right.is_numeric()) return Value(left.to_float() > right.to_float());
            if (left.is_string() && right.is_string()) return Value(left.as_string() > right.as_string());
            binary_type_error(">", left, right, span);

        case TokenType::GtEq:
            if (left.is_int()    && right.is_int())    return Value(left.as_int() >= right.as_int());
            if (left.is_numeric() && right.is_numeric()) return Value(left.to_float() >= right.to_float());
            if (left.is_string() && right.is_string()) return Value(left.as_string() >= right.as_string());
            binary_type_error(">=", left, right, span);

        default:
            return Value{};
    }
}
Value Interpreter::eval_call(const CallExpr& expr) {
    Value callee = evaluate(*expr.callee);

    std::vector<Value> args;
    args.reserve(expr.arguments.size());
    for (const auto& arg : expr.arguments) {
        args.push_back(evaluate(*arg));
    }

    if (callee.is_function()) {
        return call_function(callee.as_function(), std::move(args), expr.paren.span);
    }
    if (callee.is_builtin()) {
        return call_builtin(callee.as_builtin(), std::move(args), expr.paren.span);
    }

    throw make_error(CrashKind::NotCallable, expr.paren.span,
        "'" + value_to_string(callee) + "' is not callable (type: "
        + value_type_name(callee) + ")");
}

Value Interpreter::call_function(const FunctionValue& fn,
                                  std::vector<Value> args,
                                  Span call_site)
{
    // Arity check.
    if (args.size() != fn.params.size()) {
        auto err = make_error(CrashKind::ArityMismatch, call_site,
            "'" + fn.name + "' expects " + std::to_string(fn.params.size())
            + " argument(s), got " + std::to_string(args.size()));
        err.metadata["function"]  = fn.name;
        err.metadata["expected"]  = std::to_string(fn.params.size());
        err.metadata["got"]       = std::to_string(args.size());
        throw err;
    }

    // Stack depth check.
    if (call_stack_.size() >= MAX_CALL_DEPTH) {
        throw make_error(CrashKind::StackOverflow, call_site,
            "maximum call depth of " + std::to_string(MAX_CALL_DEPTH) + " exceeded");
    }

    // Push call frame.
    call_stack_.push_back({fn.name, call_site});

    // Create function scope, child of the closure environment.
    auto fn_scope = std::make_shared<Scope>(fn.closure, fn.name);
    for (size_t i = 0; i < fn.params.size(); ++i) {
        fn_scope->define(fn.params[i], std::move(args[i]));
    }

    // Execute body.
    auto previous_scope = env_.current();
    env_.push_existing_scope(fn_scope);

    Value result;
    try {
        execute_stmt(*fn.body);
    } catch (const ReturnSignal& ret) {
        result = ret.value;
    } catch (...) {
        env_.push_existing_scope(previous_scope);
        call_stack_.pop_back();
        throw;
    }

    env_.push_existing_scope(previous_scope);
    call_stack_.pop_back();
    return result;
}

Value Interpreter::call_builtin(const BuiltinValue& builtin,
                                 std::vector<Value> args,
                                 Span call_site)
{
    // Arity check (arity == -1 means variadic).
    if (builtin.arity >= 0
        && static_cast<size_t>(builtin.arity) != args.size())
    {
        auto err = make_error(CrashKind::ArityMismatch, call_site,
            "'" + builtin.name + "' expects "
            + std::to_string(builtin.arity) + " argument(s), got "
            + std::to_string(args.size()));
        err.metadata["function"] = builtin.name;
        throw err;
    }

    return builtin.fn(std::move(args), call_site);
}
Value Interpreter::eval_index(const IndexExpr& expr) {
    Value obj   = evaluate(*expr.object);
    Value index = evaluate(*expr.index);

    // String indexing: str[i] → single-char string.
    if (obj.is_string()) {
        if (!index.is_int()) {
            type_error("string index", index, "int", expr.bracket.span);
        }
        const auto& str = obj.as_string();
        int64_t idx = index.as_int();
        int64_t len = static_cast<int64_t>(str.size());

        if (idx < 0 || idx >= len) {
            auto err = make_error(CrashKind::OutOfBounds, expr.bracket.span,
                "string index " + std::to_string(idx)
                + " is out of bounds (length: " + std::to_string(len) + ")");
            err.metadata["index"]  = std::to_string(idx);
            err.metadata["length"] = std::to_string(len);
            throw err;
        }

        return Value(std::string(1, str[static_cast<size_t>(idx)]));
    }

    // Array indexing.
    if (!obj.is_array()) {
        type_error("index ([])", obj, "array or string", expr.bracket.span);
    }
    if (!index.is_int()) {
        type_error("array index", index, "int", expr.bracket.span);
    }

    const auto& elements = *obj.as_array().elements;
    int64_t     idx      = index.as_int();
    int64_t     len      = static_cast<int64_t>(elements.size());

    if (idx < 0 || idx >= len) {
        auto err = make_error(CrashKind::OutOfBounds, expr.bracket.span,
            "array index " + std::to_string(idx)
            + " is out of bounds (length: " + std::to_string(len) + ")");
        err.metadata["index"]  = std::to_string(idx);
        err.metadata["length"] = std::to_string(len);
        throw err;
    }

    return elements[static_cast<size_t>(idx)];
}
Value Interpreter::eval_field_access(const FieldAccessExpr& expr) {
    Value obj = evaluate(*expr.object);

    if (!obj.is_heap_ref()) {
        throw make_error(CrashKind::TypeMismatch, expr.name.span,
            "field access requires a heap object, got " + std::string(value_type_name(obj)));
    }

    HeapID id = obj.as_heap_ref().id;
    auto fn_name = current_function_name();

    // Record the field read in the ownership timeline.
    ownership_.record_field_access(id, expr.name.lexeme, false, expr.name.span, fn_name);

    // HeapSimulator::read_field() throws UseAfterFree or InvalidFieldAccess.
    return heap_.read_field(id, expr.name.lexeme, expr.name.span);
}
Value Interpreter::eval_assign(const AssignExpr& expr) {
    Value val = evaluate(*expr.value);

    std::visit([&](auto&& target) {
        using T = std::decay_t<decltype(target)>;

        if constexpr (std::is_same_v<T, IdentifierExpr>) {
            if (!env_.assign(target.name.lexeme, val)) {
                // Variable doesn't exist — define it in current scope.
                // (Assignment to undefined is an error in strict mode, but
                //  we'll be lenient for now and treat it as define.)
                auto err = make_error(CrashKind::UndefinedVariable,
                    target.name.span,
                    "cannot assign to undefined variable '"
                    + target.name.lexeme + "'");
                err.metadata["variable"] = target.name.lexeme;
                throw err;
            }
        }
        else if constexpr (std::is_same_v<T, IndexExpr>) {
            Value obj   = evaluate(*target.object);
            Value index = evaluate(*target.index);

            if (!obj.is_array())  type_error("index assign", obj, "array", target.bracket.span);
            if (!index.is_int())  type_error("array index", index, "int", target.bracket.span);

            auto& elements = *obj.as_array().elements;
            int64_t idx = index.as_int();
            int64_t len = static_cast<int64_t>(elements.size());

            if (idx < 0 || idx >= len) {
                auto err = make_error(CrashKind::OutOfBounds, target.bracket.span,
                    "array index " + std::to_string(idx) + " out of bounds (length: "
                    + std::to_string(len) + ")");
                err.metadata["index"]  = std::to_string(idx);
                err.metadata["length"] = std::to_string(len);
                throw err;
            }

            elements[static_cast<size_t>(idx)] = val;
        }
        else if constexpr (std::is_same_v<T, FieldAccessExpr>) {
            Value obj = evaluate(*target.object);
            if (!obj.is_heap_ref()) {
                type_error("field assign", obj, "heap object", target.name.span);
            }
            HeapID id = obj.as_heap_ref().id;
            ownership_.record_field_access(id, target.name.lexeme, true,
                                            target.name.span, current_function_name());
            heap_.write_field(id, target.name.lexeme, val, target.name.span);
        }
        else {
            throw make_error(CrashKind::InvalidOperation, expr.eq.span,
                "invalid assignment target");
        }
    }, expr.target->data);

    return val;
}
Value Interpreter::eval_array(const ArrayExpr& expr) {
    auto elements = std::make_shared<std::vector<Value>>();
    elements->reserve(expr.elements.size());
    for (const auto& elem : expr.elements) {
        elements->push_back(evaluate(*elem));
    }
    ArrayValue arr;
    arr.elements = std::move(elements);
    return Value(std::move(arr));
}
Value Interpreter::eval_new(const NewExpr& expr) {
    auto fn_name = current_function_name();

    // Evaluate field initializers.
    std::unordered_map<std::string, Value> fields;
    for (const auto& [name_tok, value_expr] : expr.fields) {
        fields[name_tok.lexeme] = evaluate(*value_expr);
    }

    // Allocate on the heap.
    HeapID id = heap_.allocate(
        expr.type_name.lexeme,
        std::move(fields),
        expr.keyword.span,
        fn_name
    );

    // Record the allocation in the ownership tracker.
    // The variable name will be bound by execute_let when the result is stored.
    ownership_.record_allocation(id, "<pending>", expr.type_name.lexeme,
                                  expr.keyword.span, fn_name);

    // Return a HeapRef.
    HeapRef ref;
    ref.id     = id;
    ref.origin = expr.keyword.span;
    return Value(ref);
}
Value Interpreter::eval_ref(const RefExpr& expr) {
    Value operand = evaluate(*expr.operand);

    if (!operand.is_heap_ref()) {
        throw make_error(CrashKind::TypeMismatch, expr.keyword.span,
            "ref() expects a heap object, got " + std::string(value_type_name(operand)));
    }

    HeapID id = operand.as_heap_ref().id;

    // Check that the object is still alive.
    if (!heap_.is_alive(id)) {
        auto err = make_error(CrashKind::UseAfterFree, expr.keyword.span,
            "cannot create a reference to a freed object (heap#"
            + std::to_string(id) + ")");
        err.related_object = id;
        throw err;
    }

    ownership_.record_reference(id, "<ref>", expr.keyword.span,
                                 current_function_name());

    // Return a new HeapRef pointing to the same object.
    HeapRef ref;
    ref.id     = id;
    ref.origin = expr.keyword.span;
    return Value(ref);
}
Value Interpreter::eval_deref(const DerefExpr& expr) {
    Value operand = evaluate(*expr.operand);

    if (operand.is_nil()) {
        throw make_error(CrashKind::NullDereference, expr.keyword.span,
            "tried to dereference a nil reference");
    }

    if (!operand.is_heap_ref()) {
        throw make_error(CrashKind::TypeMismatch, expr.keyword.span,
            "deref() expects a reference, got " + std::string(value_type_name(operand)));
    }

    HeapID id = operand.as_heap_ref().id;

    // Check alive. read_field would catch this too, but deref on a freed
    // object should get its own clear error.
    if (!heap_.is_alive(id)) {
        auto err = make_error(CrashKind::UseAfterFree, expr.keyword.span,
            "tried to dereference a reference to a freed object (heap#"
            + std::to_string(id) + ")");
        err.related_object = id;
        throw err;
    }

    ownership_.record_dereference(id, expr.keyword.span, current_function_name());

    // Return the HeapRef itself — field access on it still goes through
    // the heap simulator. deref() is about validating the reference is
    // alive, not extracting a copy.
    return operand;
}
Value Interpreter::eval_move(const MoveExpr& expr) {
    Value operand = evaluate(*expr.operand);

    if (!operand.is_heap_ref()) {
        throw make_error(CrashKind::TypeMismatch, expr.keyword.span,
            "move() expects a heap object, got " + std::string(value_type_name(operand)));
    }

    HeapID id = operand.as_heap_ref().id;

    // Check if already moved.
    if (ownership_.is_moved(id)) {
        auto err = make_error(CrashKind::OwnershipViolation, expr.keyword.span,
            "cannot move an object that has already been moved");
        err.related_object = id;
        throw err;
    }

    // Check alive.
    if (!heap_.is_alive(id)) {
        auto err = make_error(CrashKind::UseAfterFree, expr.keyword.span,
            "cannot move a freed object (heap#" + std::to_string(id) + ")");
        err.related_object = id;
        throw err;
    }

    // Determine the source variable name from the operand expression.
    std::string from_var = "<expr>";
    if (std::holds_alternative<IdentifierExpr>(expr.operand->data)) {
        from_var = std::get<IdentifierExpr>(expr.operand->data).name.lexeme;
    }

    // Record the move. The destination variable name will be resolved by
    // execute_let when the returned value is bound.
    ownership_.record_move(id, from_var, "<pending>",
                            expr.keyword.span, current_function_name());

    // Invalidate the source variable by setting it to nil in the environment.
    if (from_var != "<expr>") {
        env_.assign(from_var, Value(NilType{}));
    }

    return operand;
}
Value Interpreter::eval_lambda(const LambdaExpr& expr) {
    FunctionValue fn;
    fn.name    = "<lambda>";
    fn.body    = expr.body.get();
    fn.closure = env_.current();

    for (const auto& param : expr.params) {
        fn.params.push_back(param.lexeme);
    }

    return Value(std::move(fn));
}
CrashError Interpreter::make_error(CrashKind kind,
                                     Span location,
                                     std::string detail) const
{
    CrashError err(kind, location, std::move(detail));
    err.call_stack = call_stack_;
    return err;
}

void Interpreter::type_error(const std::string& op,
                               const Value& got,
                               const char* expected,
                               Span location)
{
    auto err = make_error(CrashKind::TypeMismatch, location,
        "operator '" + op + "' expected " + expected
        + ", got " + value_type_name(got));
    err.metadata["operator"] = op;
    err.metadata["got"]      = value_type_name(got);
    err.metadata["expected"] = expected;
    throw err;
}

void Interpreter::binary_type_error(const std::string& op,
                                     const Value& left,
                                     const Value& right,
                                     Span location)
{
    auto err = make_error(CrashKind::TypeMismatch, location,
        "operator '" + op + "' cannot be applied to "
        + value_type_name(left) + " and " + value_type_name(right));
    err.metadata["operator"] = op;
    err.metadata["left"]     = value_type_name(left);
    err.metadata["right"]    = value_type_name(right);
    throw err;
}

std::string Interpreter::current_function_name() const {
    if (call_stack_.empty()) return "<main>";
    return call_stack_.back().function_name;
}
Value Interpreter::eval_match(const MatchExpr& expr) {
    Value target = evaluate(*expr.target);

    for (const auto& arm : expr.arms) {
        if (arm.is_wildcard) {
            return evaluate(*arm.body);
        }
        Value pattern = evaluate(*arm.pattern);
        if (values_equal(target, pattern)) {
            return evaluate(*arm.body);
        }
    }

    return Value(NilType{});
}

} // namespace crashlang
