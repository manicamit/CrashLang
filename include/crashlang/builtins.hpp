#ifndef CRASHLANG_BUILTINS_HPP
#define CRASHLANG_BUILTINS_HPP

/// Built-in function registry for CrashLang.
///
/// Built-ins are registered into the global scope at interpreter startup.
/// Each is a BuiltinValue with a name, arity, and a C++ lambda.
///
/// Higher-order builtins (map, filter, forEach) can call user-defined
/// functions via a callback set by the interpreter.

#include "crashlang/environment.hpp"
#include "crashlang/value.hpp"

#include <functional>

namespace crashlang {

/// Callback type that builtins use to invoke user-defined functions.
using UserFnCaller = std::function<Value(const Value& fn, std::vector<Value> args, Span span)>;

/// Set the callback that builtins use to call user functions.
void set_user_fn_caller(UserFnCaller caller);

/// Get the currently set callback.
const UserFnCaller& get_user_fn_caller();

/// Register all built-in functions into the given environment's global scope.
void register_builtins(Environment& env);

} // namespace crashlang

#endif // CRASHLANG_BUILTINS_HPP
