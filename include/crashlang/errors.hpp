#ifndef CRASHLANG_ERRORS_HPP
#define CRASHLANG_ERRORS_HPP

/// Runtime error types for CrashLang.
///
/// The interpreter throws CrashError when something goes wrong at runtime.
/// The top-level runner catches it and passes it to the DiagnosticsEngine
/// which formats it into a human-readable crash report.
///
/// Control-flow signals (return, break, continue) use separate exception
/// types so they don't get mixed with actual errors.

#include "crashlang/common.hpp"
#include "crashlang/source.hpp"
#include "crashlang/value.hpp"

#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace crashlang {

// ── CrashKind ──────────────────────────────────────────────────────────────────

enum class CrashKind {
    UseAfterFree,         // Accessing a heap object after free()
    DoubleFree,           // Calling free() on an already-freed object
    NullDereference,      // Dereferencing a nil reference
    OutOfBounds,          // Array index out of range
    OwnershipViolation,   // Using a moved value
    DivisionByZero,       // Division or modulo by zero
    TypeMismatch,         // Operator applied to wrong types
    UndefinedVariable,    // Variable not in scope
    StackOverflow,        // Call depth exceeded
    AssertionFailed,      // assert() failed
    ArityMismatch,        // Wrong number of arguments to a function
    InvalidFieldAccess,   // Accessing a field that doesn't exist on a struct
    NotCallable,          // Calling something that isn't a function
    InvalidOperation,     // Catch-all for other invalid operations
};

/// Human-readable name for a CrashKind (shown in the error header).
const char* crash_kind_name(CrashKind kind);

// ── CallFrame ─────────────────────────────────────────────────────────────────

/// One frame in the call stack at the point of a crash.
struct CallFrame {
    std::string function_name;
    Span        call_site;     // Where this function was called from.
};

// ── CrashError ────────────────────────────────────────────────────────────────

/// A runtime error. Thrown by the interpreter, caught by the runner.
struct CrashError : public std::runtime_error {
    CrashKind   kind;
    Span        location;      // The source location of the failing operation.
    std::string detail;        // Short plain-English statement of what happened.

    /// The heap object involved, if any (for UseAfterFree, DoubleFree, etc.).
    std::optional<HeapID> related_object;

    /// Call stack at the point of the crash (innermost last).
    std::vector<CallFrame> call_stack;

    /// Extra key-value metadata for the diagnostics engine.
    /// Examples: {"variable": "buf"}, {"index": "5", "length": "3"}
    std::unordered_map<std::string, std::string> metadata;

    CrashError(CrashKind kind_, Span location_, std::string detail_)
        : std::runtime_error(detail_)
        , kind(kind_)
        , location(location_)
        , detail(std::move(detail_))
    {}
};

// ── Control-flow signals ───────────────────────────────────────────────────────
// These are NOT errors — they're how return/break/continue propagate up
// the call stack cleanly. They are thrown and caught within the interpreter.

/// Thrown by a return statement.
struct ReturnSignal {
    Value value;   // The returned value (nil if bare return).
    Span  span;    // Where the return statement is (for stack overflow detection).
};

/// Thrown by a break statement.
struct BreakSignal {
    Span span;
};

/// Thrown by a continue statement.
struct ContinueSignal {
    Span span;
};

} // namespace crashlang

#endif // CRASHLANG_ERRORS_HPP
