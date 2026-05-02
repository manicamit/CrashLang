#ifndef CRASHLANG_BUILTINS_HPP
#define CRASHLANG_BUILTINS_HPP

/// Built-in function registry for CrashLang.
///
/// Built-ins are registered into the global scope at interpreter startup.
/// Each is a BuiltinValue with a name, arity, and a C++ lambda.

#include "crashlang/environment.hpp"

namespace crashlang {

/// Register all built-in functions into the given environment's global scope.
void register_builtins(Environment& env);

} // namespace crashlang

#endif // CRASHLANG_BUILTINS_HPP
