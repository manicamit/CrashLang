#include "crashlang/errors.hpp"

namespace crashlang {

const char* crash_kind_name(CrashKind kind) {
    switch (kind) {
        case CrashKind::UseAfterFree:       return "UseAfterFree";
        case CrashKind::DoubleFree:         return "DoubleFree";
        case CrashKind::NullDereference:    return "NullDereference";
        case CrashKind::OutOfBounds:        return "OutOfBounds";
        case CrashKind::OwnershipViolation: return "OwnershipViolation";
        case CrashKind::DivisionByZero:     return "DivisionByZero";
        case CrashKind::TypeMismatch:       return "TypeMismatch";
        case CrashKind::UndefinedVariable:  return "UndefinedVariable";
        case CrashKind::StackOverflow:      return "StackOverflow";
        case CrashKind::AssertionFailed:    return "AssertionFailed";
        case CrashKind::ArityMismatch:      return "ArityMismatch";
        case CrashKind::InvalidFieldAccess: return "InvalidFieldAccess";
        case CrashKind::NotCallable:        return "NotCallable";
        case CrashKind::InvalidOperation:   return "InvalidOperation";
    }
    return "UnknownError";
}

} // namespace crashlang
