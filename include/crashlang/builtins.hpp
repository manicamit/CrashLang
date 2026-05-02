#ifndef CRASHLANG_BUILTINS_HPP
#define CRASHLANG_BUILTINS_HPP

#include "crashlang/environment.hpp"
#include "crashlang/value.hpp"

#include <functional>

namespace crashlang {

// Callback for builtins that need to invoke user-defined functions (e.g. map, filter).
using UserFnCaller = std::function<Value(const Value& fn, std::vector<Value> args, Span span)>;

void set_user_fn_caller(UserFnCaller caller);
const UserFnCaller& get_user_fn_caller();
void register_builtins(Environment& env);

} // namespace crashlang

#endif
