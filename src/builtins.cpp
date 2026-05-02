#include "crashlang/builtins.hpp"
#include "crashlang/errors.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <iostream>
#include <sstream>

namespace crashlang {

// ── Helper macros ──────────────────────────────────────────────────────────────

/// Register a built-in with fixed arity.
#define BUILTIN(name_, arity_, fn_body_) \
    env.define(name_, Value(BuiltinValue{ \
        name_, arity_, \
        [](std::vector<Value> args, Span span) -> Value fn_body_ \
    }))

/// Suppress unused-variable warnings for builtins that don't use span or args.
#define UNUSED_SPAN (void)span
#define UNUSED_ARGS (void)args

/// Throw a runtime error from a built-in function.
#define BUILTIN_ERROR(kind_, msg_) \
    do { \
        CrashError _e(kind_, span, msg_); \
        throw _e; \
    } while (false)

// ── Built-in implementations ───────────────────────────────────────────────────

void register_builtins(Environment& env) {

    // print(args...) — print values space-separated, no trailing newline.
    BUILTIN("print", -1, {
        UNUSED_SPAN;
        for (size_t i = 0; i < args.size(); ++i) {
            if (i > 0) std::cout << ' ';
            std::cout << value_to_string(args[i]);
        }
        return Value(NilType{});
    });

    // println(args...) — print values space-separated, with trailing newline.
    BUILTIN("println", -1, {
        UNUSED_SPAN;
        for (size_t i = 0; i < args.size(); ++i) {
            if (i > 0) std::cout << ' ';
            std::cout << value_to_string(args[i]);
        }
        std::cout << '\n';
        return Value(NilType{});
    });

    // len(val) — length of a string or array.
    BUILTIN("len", 1, {
        if (args[0].is_string())
            return Value(static_cast<int64_t>(args[0].as_string().size()));
        if (args[0].is_array())
            return Value(static_cast<int64_t>(args[0].as_array().elements->size()));
        BUILTIN_ERROR(CrashKind::TypeMismatch,
            "len() expects a string or array, got " + std::string(value_type_name(args[0])));
    });

    // type_of(val) — return the type name as a string.
    BUILTIN("type_of", 1, {
        UNUSED_SPAN;
        return Value(std::string(value_type_name(args[0])));
    });

    // to_string(val) — convert any value to its string representation.
    BUILTIN("to_string", 1, {
        UNUSED_SPAN;
        return Value(value_to_string(args[0]));
    });

    // to_int(val) — parse a string to int, or truncate a float to int.
    BUILTIN("to_int", 1, {
        if (args[0].is_int())   return args[0];
        if (args[0].is_float()) return Value(static_cast<int64_t>(args[0].as_float()));
        if (args[0].is_string()) {
            try {
                size_t pos = 0;
                int64_t v = std::stoll(args[0].as_string(), &pos);
                if (pos != args[0].as_string().size()) {
                    BUILTIN_ERROR(CrashKind::InvalidOperation,
                        "to_int(): '" + args[0].as_string() + "' is not a valid integer");
                }
                return Value(v);
            } catch (...) {
                BUILTIN_ERROR(CrashKind::InvalidOperation,
                    "to_int(): '" + args[0].as_string() + "' is not a valid integer");
            }
        }
        BUILTIN_ERROR(CrashKind::TypeMismatch,
            "to_int() expects int, float, or string, got "
            + std::string(value_type_name(args[0])));
    });

    // to_float(val) — convert int or string to float.
    BUILTIN("to_float", 1, {
        if (args[0].is_float()) return args[0];
        if (args[0].is_int())   return Value(static_cast<double>(args[0].as_int()));
        if (args[0].is_string()) {
            try {
                size_t pos = 0;
                double v = std::stod(args[0].as_string(), &pos);
                if (pos != args[0].as_string().size()) {
                    BUILTIN_ERROR(CrashKind::InvalidOperation,
                        "to_float(): '" + args[0].as_string() + "' is not a valid number");
                }
                return Value(v);
            } catch (...) {
                BUILTIN_ERROR(CrashKind::InvalidOperation,
                    "to_float(): '" + args[0].as_string() + "' is not a valid number");
            }
        }
        BUILTIN_ERROR(CrashKind::TypeMismatch,
            "to_float() expects int, float, or string, got "
            + std::string(value_type_name(args[0])));
    });

    // assert(cond) or assert(cond, message) — crash if condition is false.
    BUILTIN("assert", -1, {
        if (args.empty() || args.size() > 2) {
            BUILTIN_ERROR(CrashKind::ArityMismatch,
                "assert() takes 1 or 2 arguments, got " + std::to_string(args.size()));
        }
        if (!value_is_truthy(args[0])) {
            std::string msg = args.size() == 2
                ? "assertion failed: " + value_to_string(args[1])
                : "assertion failed";
            BUILTIN_ERROR(CrashKind::AssertionFailed, msg);
        }
        return Value(NilType{});
    });

    // push(arr, val) — append val to array. Returns nil.
    BUILTIN("push", 2, {
        if (!args[0].is_array()) {
            BUILTIN_ERROR(CrashKind::TypeMismatch,
                "push() expects an array as the first argument, got "
                + std::string(value_type_name(args[0])));
        }
        args[0].as_array().elements->push_back(std::move(args[1]));
        return Value(NilType{});
    });

    // pop(arr) — remove and return the last element. Crashes on empty array.
    BUILTIN("pop", 1, {
        if (!args[0].is_array()) {
            BUILTIN_ERROR(CrashKind::TypeMismatch,
                "pop() expects an array, got " + std::string(value_type_name(args[0])));
        }
        auto& elements = *args[0].as_array().elements;
        if (elements.empty()) {
            BUILTIN_ERROR(CrashKind::OutOfBounds, "pop() called on an empty array");
        }
        Value last = std::move(elements.back());
        elements.pop_back();
        return last;
    });

    // range(start, end) — returns [start, start+1, ..., end-1].
    BUILTIN("range", 2, {
        if (!args[0].is_int() || !args[1].is_int()) {
            BUILTIN_ERROR(CrashKind::TypeMismatch,
                "range() expects two int arguments");
        }
        int64_t start = args[0].as_int();
        int64_t end   = args[1].as_int();
        auto elements = std::make_shared<std::vector<Value>>();
        for (int64_t i = start; i < end; ++i) {
            elements->push_back(Value(i));
        }
        ArrayValue arr;
        arr.elements = std::move(elements);
        return Value(std::move(arr));
    });

    // input(prompt?) — read a line from stdin, optionally printing a prompt.
    BUILTIN("input", -1, {
        if (args.size() > 1) {
            BUILTIN_ERROR(CrashKind::ArityMismatch,
                "input() takes 0 or 1 arguments");
        }
        if (args.size() == 1) {
            std::cout << value_to_string(args[0]);
            std::cout.flush();
        }
        std::string line;
        if (!std::getline(std::cin, line)) {
            return Value(NilType{});
        }
        return Value(std::move(line));
    });

    // clock() — seconds since epoch as a float (useful for benchmarks).
    BUILTIN("clock", 0, {
        UNUSED_SPAN; UNUSED_ARGS;
        auto now = std::chrono::steady_clock::now();
        auto ns  = std::chrono::duration_cast<std::chrono::nanoseconds>(
                       now.time_since_epoch()).count();
        return Value(static_cast<double>(ns) / 1e9);
    });

    // sqrt(n) — square root.
    BUILTIN("sqrt", 1, {
        if (!args[0].is_numeric()) {
            BUILTIN_ERROR(CrashKind::TypeMismatch,
                "sqrt() expects a number, got " + std::string(value_type_name(args[0])));
        }
        return Value(std::sqrt(args[0].to_float()));
    });

    // abs(n) — absolute value.
    BUILTIN("abs", 1, {
        if (args[0].is_int())   return Value(args[0].as_int() < 0 ? -args[0].as_int() : args[0].as_int());
        if (args[0].is_float()) return Value(std::abs(args[0].as_float()));
        BUILTIN_ERROR(CrashKind::TypeMismatch,
            "abs() expects a number, got " + std::string(value_type_name(args[0])));
    });

    // max(a, b) and min(a, b) — numeric comparison.
    BUILTIN("max", 2, {
        if (!args[0].is_numeric() || !args[1].is_numeric()) {
            BUILTIN_ERROR(CrashKind::TypeMismatch, "max() expects numeric arguments");
        }
        bool left_bigger = args[0].is_int() && args[1].is_int()
            ? args[0].as_int() >= args[1].as_int()
            : args[0].to_float() >= args[1].to_float();
        return left_bigger ? args[0] : args[1];
    });

    BUILTIN("min", 2, {
        if (!args[0].is_numeric() || !args[1].is_numeric()) {
            BUILTIN_ERROR(CrashKind::TypeMismatch, "min() expects numeric arguments");
        }
        bool left_smaller = args[0].is_int() && args[1].is_int()
            ? args[0].as_int() <= args[1].as_int()
            : args[0].to_float() <= args[1].to_float();
        return left_smaller ? args[0] : args[1];
    });

    // ── Higher-order functions ──────────────────────────────────────────────────

    // map(arr, fn) — apply fn to each element, return new array.
    BUILTIN("map", 2, {
        if (!args[0].is_array()) {
            BUILTIN_ERROR(CrashKind::TypeMismatch,
                "map() expects an array as the first argument, got "
                + std::string(value_type_name(args[0])));
        }
        if (!args[1].is_callable()) {
            BUILTIN_ERROR(CrashKind::TypeMismatch,
                "map() expects a function as the second argument, got "
                + std::string(value_type_name(args[1])));
        }
        const auto& src = *args[0].as_array().elements;
        auto result_elems = std::make_shared<std::vector<Value>>();
        result_elems->reserve(src.size());
        for (const auto& elem : src) {
            std::vector<Value> call_args = {elem};
            if (args[1].is_builtin()) {
                result_elems->push_back(args[1].as_builtin().fn(std::move(call_args), span));
            } else {
                // For user functions, we use a simplified direct call.
                // The interpreter's call_function handles scope, but we don't
                // have access to it here. Instead, we throw a special signal.
                BUILTIN_ERROR(CrashKind::InvalidOperation,
                    "map() with user-defined functions requires using a for loop instead. "
                    "Use: for item in arr { ... }");
            }
        }
        ArrayValue arr;
        arr.elements = std::move(result_elems);
        return Value(std::move(arr));
    });

    // filter(arr, fn) — keep elements where fn returns truthy.
    BUILTIN("filter", 2, {
        if (!args[0].is_array()) {
            BUILTIN_ERROR(CrashKind::TypeMismatch,
                "filter() expects an array as the first argument");
        }
        if (!args[1].is_callable()) {
            BUILTIN_ERROR(CrashKind::TypeMismatch,
                "filter() expects a function as the second argument");
        }
        const auto& src = *args[0].as_array().elements;
        auto result_elems = std::make_shared<std::vector<Value>>();
        for (const auto& elem : src) {
            std::vector<Value> call_args = {elem};
            if (args[1].is_builtin()) {
                Value test = args[1].as_builtin().fn(std::move(call_args), span);
                if (value_is_truthy(test)) {
                    result_elems->push_back(elem);
                }
            } else {
                BUILTIN_ERROR(CrashKind::InvalidOperation,
                    "filter() with user-defined functions requires using a for loop instead");
            }
        }
        ArrayValue arr;
        arr.elements = std::move(result_elems);
        return Value(std::move(arr));
    });

    // forEach(arr, fn) — call fn for each element, discard results.
    BUILTIN("forEach", 2, {
        if (!args[0].is_array()) {
            BUILTIN_ERROR(CrashKind::TypeMismatch,
                "forEach() expects an array as the first argument");
        }
        if (!args[1].is_callable()) {
            BUILTIN_ERROR(CrashKind::TypeMismatch,
                "forEach() expects a function as the second argument");
        }
        const auto& src = *args[0].as_array().elements;
        for (const auto& elem : src) {
            std::vector<Value> call_args = {elem};
            if (args[1].is_builtin()) {
                args[1].as_builtin().fn(std::move(call_args), span);
            } else {
                BUILTIN_ERROR(CrashKind::InvalidOperation,
                    "forEach() with user-defined functions requires using a for loop instead");
            }
        }
        return Value(NilType{});
    });

    // ── String utilities ───────────────────────────────────────────────────────

    // substr(str, start, length?) — extract a substring.
    BUILTIN("substr", -1, {
        if (args.size() < 2 || args.size() > 3) {
            BUILTIN_ERROR(CrashKind::ArityMismatch,
                "substr() takes 2 or 3 arguments, got " + std::to_string(args.size()));
        }
        if (!args[0].is_string()) {
            BUILTIN_ERROR(CrashKind::TypeMismatch,
                "substr() expects a string as the first argument");
        }
        if (!args[1].is_int()) {
            BUILTIN_ERROR(CrashKind::TypeMismatch,
                "substr() expects an int start index");
        }
        const auto& str = args[0].as_string();
        int64_t start = args[1].as_int();
        if (start < 0) start = 0;
        if (static_cast<size_t>(start) >= str.size()) {
            return Value(std::string(""));
        }
        if (args.size() == 3) {
            if (!args[2].is_int()) {
                BUILTIN_ERROR(CrashKind::TypeMismatch,
                    "substr() expects an int length");
            }
            int64_t length = args[2].as_int();
            if (length < 0) length = 0;
            return Value(str.substr(static_cast<size_t>(start), static_cast<size_t>(length)));
        }
        return Value(str.substr(static_cast<size_t>(start)));
    });

    // split(str, delimiter) — split string into an array of strings.
    BUILTIN("split", 2, {
        if (!args[0].is_string() || !args[1].is_string()) {
            BUILTIN_ERROR(CrashKind::TypeMismatch,
                "split() expects two string arguments");
        }
        const auto& str = args[0].as_string();
        const auto& delim = args[1].as_string();
        auto elements = std::make_shared<std::vector<Value>>();

        if (delim.empty()) {
            // Split into individual characters.
            for (char ch : str) {
                elements->push_back(Value(std::string(1, ch)));
            }
        } else {
            size_t pos = 0;
            size_t found;
            while ((found = str.find(delim, pos)) != std::string::npos) {
                elements->push_back(Value(str.substr(pos, found - pos)));
                pos = found + delim.size();
            }
            elements->push_back(Value(str.substr(pos)));
        }

        ArrayValue arr;
        arr.elements = std::move(elements);
        return Value(std::move(arr));
    });

    // join(arr, separator) — join array of strings with a separator.
    BUILTIN("join", 2, {
        if (!args[0].is_array()) {
            BUILTIN_ERROR(CrashKind::TypeMismatch,
                "join() expects an array as the first argument");
        }
        if (!args[1].is_string()) {
            BUILTIN_ERROR(CrashKind::TypeMismatch,
                "join() expects a string separator as the second argument");
        }
        const auto& elems = *args[0].as_array().elements;
        const auto& sep = args[1].as_string();
        std::string result;
        for (size_t i = 0; i < elems.size(); ++i) {
            if (i > 0) result += sep;
            result += value_to_string(elems[i]);
        }
        return Value(std::move(result));
    });

    // contains(str, substr) — check if string contains a substring.
    BUILTIN("contains", 2, {
        UNUSED_SPAN;
        if (!args[0].is_string() || !args[1].is_string()) {
            BUILTIN_ERROR(CrashKind::TypeMismatch,
                "contains() expects two string arguments");
        }
        return Value(args[0].as_string().find(args[1].as_string()) != std::string::npos);
    });

    // starts_with(str, prefix) — check if string starts with prefix.
    BUILTIN("starts_with", 2, {
        UNUSED_SPAN;
        if (!args[0].is_string() || !args[1].is_string()) {
            BUILTIN_ERROR(CrashKind::TypeMismatch,
                "starts_with() expects two string arguments");
        }
        const auto& str = args[0].as_string();
        const auto& prefix = args[1].as_string();
        return Value(str.size() >= prefix.size()
            && str.compare(0, prefix.size(), prefix) == 0);
    });

    // ends_with(str, suffix) — check if string ends with suffix.
    BUILTIN("ends_with", 2, {
        UNUSED_SPAN;
        if (!args[0].is_string() || !args[1].is_string()) {
            BUILTIN_ERROR(CrashKind::TypeMismatch,
                "ends_with() expects two string arguments");
        }
        const auto& str = args[0].as_string();
        const auto& suffix = args[1].as_string();
        return Value(str.size() >= suffix.size()
            && str.compare(str.size() - suffix.size(), suffix.size(), suffix) == 0);
    });

    // replace(str, old, new) — replace all occurrences of old with new.
    BUILTIN("replace", 3, {
        UNUSED_SPAN;
        if (!args[0].is_string() || !args[1].is_string() || !args[2].is_string()) {
            BUILTIN_ERROR(CrashKind::TypeMismatch,
                "replace() expects three string arguments");
        }
        std::string result = args[0].as_string();
        const auto& from = args[1].as_string();
        const auto& to = args[2].as_string();
        if (!from.empty()) {
            size_t pos = 0;
            while ((pos = result.find(from, pos)) != std::string::npos) {
                result.replace(pos, from.size(), to);
                pos += to.size();
            }
        }
        return Value(std::move(result));
    });

    // trim(str) — remove leading and trailing whitespace.
    BUILTIN("trim", 1, {
        UNUSED_SPAN;
        if (!args[0].is_string()) {
            BUILTIN_ERROR(CrashKind::TypeMismatch,
                "trim() expects a string, got " + std::string(value_type_name(args[0])));
        }
        std::string s = args[0].as_string();
        size_t start = s.find_first_not_of(" \t\n\r");
        if (start == std::string::npos) return Value(std::string(""));
        size_t end = s.find_last_not_of(" \t\n\r");
        return Value(s.substr(start, end - start + 1));
    });

    // upper(str) — convert to uppercase.
    BUILTIN("upper", 1, {
        UNUSED_SPAN;
        if (!args[0].is_string()) {
            BUILTIN_ERROR(CrashKind::TypeMismatch,
                "upper() expects a string");
        }
        std::string s = args[0].as_string();
        std::transform(s.begin(), s.end(), s.begin(), ::toupper);
        return Value(std::move(s));
    });

    // lower(str) — convert to lowercase.
    BUILTIN("lower", 1, {
        UNUSED_SPAN;
        if (!args[0].is_string()) {
            BUILTIN_ERROR(CrashKind::TypeMismatch,
                "lower() expects a string");
        }
        std::string s = args[0].as_string();
        std::transform(s.begin(), s.end(), s.begin(), ::tolower);
        return Value(std::move(s));
    });

    // chars(str) — convert string to array of single-char strings.
    BUILTIN("chars", 1, {
        UNUSED_SPAN;
        if (!args[0].is_string()) {
            BUILTIN_ERROR(CrashKind::TypeMismatch,
                "chars() expects a string");
        }
        const auto& str = args[0].as_string();
        auto elements = std::make_shared<std::vector<Value>>();
        for (char ch : str) {
            elements->push_back(Value(std::string(1, ch)));
        }
        ArrayValue arr;
        arr.elements = std::move(elements);
        return Value(std::move(arr));
    });

    // reverse(val) — reverse a string or array.
    BUILTIN("reverse", 1, {
        UNUSED_SPAN;
        if (args[0].is_string()) {
            std::string s = args[0].as_string();
            std::reverse(s.begin(), s.end());
            return Value(std::move(s));
        }
        if (args[0].is_array()) {
            auto elements = std::make_shared<std::vector<Value>>(
                *args[0].as_array().elements);
            std::reverse(elements->begin(), elements->end());
            ArrayValue arr;
            arr.elements = std::move(elements);
            return Value(std::move(arr));
        }
        BUILTIN_ERROR(CrashKind::TypeMismatch,
            "reverse() expects a string or array, got "
            + std::string(value_type_name(args[0])));
    });

    #undef BUILTIN
    #undef BUILTIN_ERROR
    #undef UNUSED_SPAN
    #undef UNUSED_ARGS
}

} // namespace crashlang
