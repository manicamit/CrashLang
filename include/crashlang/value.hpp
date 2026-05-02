#ifndef CRASHLANG_VALUE_HPP
#define CRASHLANG_VALUE_HPP

#include "crashlang/common.hpp"
#include "crashlang/source.hpp"

#include <functional>
#include <memory>
#include <string>
#include <variant>
#include <vector>

namespace crashlang {

struct ObjUpvalue;
struct Value;
struct Stmt;
struct Scope;
struct Chunk;

struct NilType {
    bool operator==(const NilType&) const { return true; }
    bool operator!=(const NilType&) const { return false; }
};

struct HeapRef {
    HeapID id;
    Span   origin;

    bool operator==(const HeapRef& o) const { return id == o.id; }
    bool operator!=(const HeapRef& o) const { return id != o.id; }
};

struct ArrayValue {
    std::shared_ptr<std::vector<Value>> elements;

    bool operator==(const ArrayValue& o) const { return elements == o.elements; }
    bool operator!=(const ArrayValue& o) const { return elements != o.elements; }
};

struct FunctionValue {
    std::string              name;
    std::vector<std::string> params;
    const Stmt*              body = nullptr;     // tree-walk path
    std::shared_ptr<Scope>   closure;
    std::shared_ptr<Chunk>   compiled;           // VM path (null for tree-walk)
    std::shared_ptr<std::vector<ObjUpvalue*>> vm_upvalues;

    bool operator==(const FunctionValue& o) const { return name == o.name && body == o.body; }
    bool operator!=(const FunctionValue& o) const { return !(*this == o); }
};

struct BuiltinValue {
    std::string name;
    int         arity; // -1 = variadic
    std::function<Value(std::vector<Value>, Span)> fn;

    bool operator==(const BuiltinValue& o) const { return name == o.name; }
    bool operator!=(const BuiltinValue& o) const { return name != o.name; }
};

using ValueData = std::variant<
    NilType, bool, int64_t, double, std::string,
    HeapRef, ArrayValue, FunctionValue, BuiltinValue
>;

struct Value {
    ValueData data;

    Value() : data(NilType{}) {}
    explicit Value(NilType v)        : data(v) {}
    explicit Value(bool v)           : data(v) {}
    explicit Value(int64_t v)        : data(v) {}
    explicit Value(double v)         : data(v) {}
    explicit Value(std::string v)    : data(std::move(v)) {}
    explicit Value(HeapRef v)        : data(v) {}
    explicit Value(ArrayValue v)     : data(std::move(v)) {}
    explicit Value(FunctionValue v)  : data(std::move(v)) {}
    explicit Value(BuiltinValue v)   : data(std::move(v)) {}

    bool is_nil()      const { return std::holds_alternative<NilType>(data); }
    bool is_bool()     const { return std::holds_alternative<bool>(data); }
    bool is_int()      const { return std::holds_alternative<int64_t>(data); }
    bool is_float()    const { return std::holds_alternative<double>(data); }
    bool is_string()   const { return std::holds_alternative<std::string>(data); }
    bool is_heap_ref() const { return std::holds_alternative<HeapRef>(data); }
    bool is_array()    const { return std::holds_alternative<ArrayValue>(data); }
    bool is_function() const { return std::holds_alternative<FunctionValue>(data); }
    bool is_builtin()  const { return std::holds_alternative<BuiltinValue>(data); }
    bool is_numeric()  const { return is_int() || is_float(); }
    bool is_callable() const { return is_function() || is_builtin(); }

    bool              as_bool()     const { return std::get<bool>(data); }
    int64_t           as_int()      const { return std::get<int64_t>(data); }
    double            as_float()    const { return std::get<double>(data); }
    const std::string& as_string()  const { return std::get<std::string>(data); }
    const HeapRef&    as_heap_ref() const { return std::get<HeapRef>(data); }
    const ArrayValue& as_array()    const { return std::get<ArrayValue>(data); }
    ArrayValue&       as_array()          { return std::get<ArrayValue>(data); }
    const FunctionValue& as_function() const { return std::get<FunctionValue>(data); }
    const BuiltinValue&  as_builtin()  const { return std::get<BuiltinValue>(data); }

    double to_float() const;

    bool operator==(const Value& other) const;
    bool operator!=(const Value& other) const;
};

const char* value_type_name(const Value& v);
std::string value_to_string(const Value& v);
bool value_is_truthy(const Value& v);
bool values_equal(const Value& a, const Value& b);

} // namespace crashlang

#endif
