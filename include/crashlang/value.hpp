#ifndef CRASHLANG_VALUE_HPP
#define CRASHLANG_VALUE_HPP

/// Runtime value representation for CrashLang.
///
/// Every variable, expression result, and function argument is one of these.
/// The variant is kept small — heap-allocated objects are accessed by ID
/// through the HeapSimulator, not stored inline.

#include "crashlang/common.hpp"
#include "crashlang/source.hpp"

#include <functional>
#include <memory>
#include <string>
#include <variant>
#include <vector>

namespace crashlang {

// Forward declaration for upvalue support in the VM.
struct ObjUpvalue;

// ── Forward declarations ───────────────────────────────────────────────────────

struct Value;
struct Stmt;
struct Scope;
struct Chunk;

// ── Value types ────────────────────────────────────────────────────────────────

/// Sentinel type for nil / uninitialized.
struct NilType {
    bool operator==(const NilType&) const { return true; }
    bool operator!=(const NilType&) const { return false; }
};

/// A reference into the simulated heap.
/// Carries the object ID plus where the ref was created (for diagnostics).
struct HeapRef {
    HeapID id;
    Span   origin; // Where ref() was called — used in crash timelines.

    bool operator==(const HeapRef& o) const { return id == o.id; }
    bool operator!=(const HeapRef& o) const { return id != o.id; }
};

/// A CrashLang array (heap-allocated, reference-counted for easy passing).
struct ArrayValue {
    std::shared_ptr<std::vector<Value>> elements;

    bool operator==(const ArrayValue& o) const { return elements == o.elements; }
    bool operator!=(const ArrayValue& o) const { return elements != o.elements; }
};

/// A user-defined function value (supports closures).
struct FunctionValue {
    std::string              name;
    std::vector<std::string> params;
    const Stmt*              body = nullptr; // Non-owning pointer into the AST (tree-walk).
    std::shared_ptr<Scope>   closure;        // Captured environment at definition.
    std::shared_ptr<Chunk>   compiled;       // Compiled bytecode (VM path). Null for tree-walk.
    std::shared_ptr<std::vector<ObjUpvalue*>> vm_upvalues; // Captured upvalues (VM path).

    bool operator==(const FunctionValue& o) const { return name == o.name && body == o.body; }
    bool operator!=(const FunctionValue& o) const { return !(*this == o); }
};

/// A native/built-in function.
struct BuiltinValue {
    std::string name;
    int         arity; // -1 means variadic.
    std::function<Value(std::vector<Value>, Span)> fn;

    bool operator==(const BuiltinValue& o) const { return name == o.name; }
    bool operator!=(const BuiltinValue& o) const { return name != o.name; }
};

// ── Value variant ──────────────────────────────────────────────────────────────

using ValueData = std::variant<
    NilType,
    bool,
    int64_t,
    double,
    std::string,
    HeapRef,
    ArrayValue,
    FunctionValue,
    BuiltinValue
>;

struct Value {
    ValueData data;

    // ── Constructors ───────────────────────────────────────────────────────────
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

    // ── Type predicates ────────────────────────────────────────────────────────
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

    // ── Accessors (unchecked — caller ensures correct type) ───────────────────
    bool              as_bool()     const { return std::get<bool>(data); }
    int64_t           as_int()      const { return std::get<int64_t>(data); }
    double            as_float()    const { return std::get<double>(data); }
    const std::string& as_string()  const { return std::get<std::string>(data); }
    const HeapRef&    as_heap_ref() const { return std::get<HeapRef>(data); }
    const ArrayValue& as_array()    const { return std::get<ArrayValue>(data); }
    ArrayValue&       as_array()          { return std::get<ArrayValue>(data); }
    const FunctionValue& as_function() const { return std::get<FunctionValue>(data); }
    const BuiltinValue&  as_builtin()  const { return std::get<BuiltinValue>(data); }

    // ── Numeric coercion ───────────────────────────────────────────────────────
    /// Return value as double (works for both int and float).
    double to_float() const;

    bool operator==(const Value& other) const;
    bool operator!=(const Value& other) const;
};

// ── Value utilities ────────────────────────────────────────────────────────────

/// Human-readable type name: "int", "float", "string", "bool", "nil", etc.
const char* value_type_name(const Value& v);

/// Display representation: what `println` would print.
std::string value_to_string(const Value& v);

/// Truthiness rules: nil and false are falsy, everything else is truthy.
bool value_is_truthy(const Value& v);

/// Are two values equal? (For == / != operators.)
bool values_equal(const Value& a, const Value& b);

} // namespace crashlang

#endif // CRASHLANG_VALUE_HPP
