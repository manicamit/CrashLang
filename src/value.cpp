#include "crashlang/value.hpp"

#include <sstream>
#include <iomanip>

namespace crashlang {
double Value::to_float() const {
    if (is_int())   return static_cast<double>(as_int());
    if (is_float()) return as_float();
    return 0.0;
}
bool Value::operator==(const Value& other) const {
    return values_equal(*this, other);
}

bool Value::operator!=(const Value& other) const {
    return !(*this == other);
}
const char* value_type_name(const Value& v) {
    return std::visit([](auto&& val) -> const char* {
        using T = std::decay_t<decltype(val)>;
        if constexpr (std::is_same_v<T, NilType>)         return "nil";
        else if constexpr (std::is_same_v<T, bool>)        return "bool";
        else if constexpr (std::is_same_v<T, int64_t>)     return "int";
        else if constexpr (std::is_same_v<T, double>)      return "float";
        else if constexpr (std::is_same_v<T, std::string>) return "string";
        else if constexpr (std::is_same_v<T, HeapRef>)     return "ref";
        else if constexpr (std::is_same_v<T, ArrayValue>)  return "array";
        else if constexpr (std::is_same_v<T, FunctionValue>) return "function";
        else if constexpr (std::is_same_v<T, BuiltinValue>)  return "builtin";
        else return "unknown";
    }, v.data);
}
std::string value_to_string(const Value& v) {
    return std::visit([](auto&& val) -> std::string {
        using T = std::decay_t<decltype(val)>;

        if constexpr (std::is_same_v<T, NilType>) {
            return "nil";
        }
        else if constexpr (std::is_same_v<T, bool>) {
            return val ? "true" : "false";
        }
        else if constexpr (std::is_same_v<T, int64_t>) {
            return std::to_string(val);
        }
        else if constexpr (std::is_same_v<T, double>) {
            // Avoid trailing zeros: 3.0 → "3.0", 3.14 → "3.14".
            std::ostringstream oss;
            oss << std::setprecision(15) << val;
            std::string s = oss.str();
            // Ensure there is always a decimal point so it reads as float.
            if (s.find('.') == std::string::npos && s.find('e') == std::string::npos) {
                s += ".0";
            }
            return s;
        }
        else if constexpr (std::is_same_v<T, std::string>) {
            return val;
        }
        else if constexpr (std::is_same_v<T, HeapRef>) {
            return "<ref heap#" + std::to_string(val.id) + ">";
        }
        else if constexpr (std::is_same_v<T, ArrayValue>) {
            std::string result = "[";
            const auto& elems = *val.elements;
            for (size_t i = 0; i < elems.size(); ++i) {
                if (i > 0) result += ", ";
                // Quote strings in array display.
                if (elems[i].is_string()) {
                    result += '"' + elems[i].as_string() + '"';
                } else {
                    result += value_to_string(elems[i]);
                }
            }
            result += "]";
            return result;
        }
        else if constexpr (std::is_same_v<T, FunctionValue>) {
            return "<fn " + val.name + ">";
        }
        else if constexpr (std::is_same_v<T, BuiltinValue>) {
            return "<builtin " + val.name + ">";
        }
        else {
            return "<unknown>";
        }
    }, v.data);
}
bool value_is_truthy(const Value& v) {
    return std::visit([](auto&& val) -> bool {
        using T = std::decay_t<decltype(val)>;
        if constexpr (std::is_same_v<T, NilType>)  return false;
        else if constexpr (std::is_same_v<T, bool>) return val;
        // Everything else is truthy: 0 is truthy, "" is truthy.
        // CrashLang keeps it simple — only nil and false are falsy.
        else return true;
    }, v.data);
}
bool values_equal(const Value& a, const Value& b) {
    // Nil == Nil.
    if (a.is_nil() && b.is_nil()) return true;

    // int == float and float == int comparisons.
    if (a.is_int() && b.is_float()) return static_cast<double>(a.as_int()) == b.as_float();
    if (a.is_float() && b.is_int()) return a.as_float() == static_cast<double>(b.as_int());

    // Same-type comparisons.
    return std::visit([&](auto&& lhs) -> bool {
        using T = std::decay_t<decltype(lhs)>;
        if constexpr (std::is_same_v<T, NilType>)          return b.is_nil();
        else if constexpr (std::is_same_v<T, bool>)        return b.is_bool()   && lhs == b.as_bool();
        else if constexpr (std::is_same_v<T, int64_t>)     return b.is_int()    && lhs == b.as_int();
        else if constexpr (std::is_same_v<T, double>)      return b.is_float()  && lhs == b.as_float();
        else if constexpr (std::is_same_v<T, std::string>) return b.is_string() && lhs == b.as_string();
        else if constexpr (std::is_same_v<T, HeapRef>)     return b.is_heap_ref() && lhs == b.as_heap_ref();
        // Arrays, functions, builtins: compare by identity.
        else if constexpr (std::is_same_v<T, ArrayValue>)    return b.is_array()    && lhs == b.as_array();
        else if constexpr (std::is_same_v<T, FunctionValue>) return b.is_function() && lhs == b.as_function();
        else if constexpr (std::is_same_v<T, BuiltinValue>)  return b.is_builtin()  && lhs == b.as_builtin();
        else return false;
    }, a.data);
}

} // namespace crashlang
