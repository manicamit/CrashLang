#ifndef CRASHLANG_ERRORS_HPP
#define CRASHLANG_ERRORS_HPP

#include "crashlang/common.hpp"
#include "crashlang/source.hpp"
#include "crashlang/value.hpp"

#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace crashlang {

enum class CrashKind {
    UseAfterFree,
    DoubleFree,
    NullDereference,
    OutOfBounds,
    OwnershipViolation,
    DivisionByZero,
    TypeMismatch,
    UndefinedVariable,
    StackOverflow,
    AssertionFailed,
    ArityMismatch,
    InvalidFieldAccess,
    NotCallable,
    InvalidOperation,
};

const char* crash_kind_name(CrashKind kind);

struct CallFrame {
    std::string function_name;
    Span        call_site;
};

struct CrashError : public std::runtime_error {
    CrashKind   kind;
    Span        location;
    std::string detail;

    std::optional<HeapID> related_object;
    std::vector<CallFrame> call_stack;
    std::unordered_map<std::string, std::string> metadata;

    CrashError(CrashKind kind_, Span location_, std::string detail_)
        : std::runtime_error(detail_)
        , kind(kind_)
        , location(location_)
        , detail(std::move(detail_))
    {}
};

// Control-flow signals (not errors). Thrown and caught within the interpreter
// to propagate return/break/continue up the call stack.

struct ReturnSignal   { Value value; Span span; };
struct BreakSignal    { Span span; };
struct ContinueSignal { Span span; };

} // namespace crashlang

#endif
