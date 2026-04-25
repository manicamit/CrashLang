#ifndef CRASHLANG_COMMON_HPP
#define CRASHLANG_COMMON_HPP

/// CrashLang — common type aliases and forward declarations.
///
/// This header is included by nearly every other header. Keep it light:
/// only type aliases, constants, and forward declarations belong here.

#include <cstdint>
#include <string>

namespace crashlang {

// ── Numeric aliases ────────────────────────────────────────────────────────────

/// Unique identifier for a heap-allocated object.
using HeapID = uint64_t;

/// Source position types. 1-indexed (line 1, column 1 is the first character).
using LineNo = uint32_t;
using ColNo  = uint32_t;

// ── Sentinel values ────────────────────────────────────────────────────────────

/// Invalid / uninitialized heap ID.
constexpr HeapID INVALID_HEAP_ID = 0;

// ── Forward declarations ───────────────────────────────────────────────────────

struct SourceFile;
struct SourceLocation;
struct Span;

struct Token;

// AST — defined as variant types in ast.hpp
struct LiteralExpr;
struct IdentifierExpr;
struct UnaryExpr;
struct BinaryExpr;
struct CallExpr;
struct IndexExpr;
struct FieldAccessExpr;
struct AssignExpr;
struct ArrayExpr;
struct StructExpr;
struct NewExpr;
struct RefExpr;
struct DerefExpr;
struct MoveExpr;

struct ExprStmt;
struct LetStmt;
struct BlockStmt;
struct IfStmt;
struct WhileStmt;
struct ForStmt;
struct FnStmt;
struct ReturnStmt;
struct BreakStmt;
struct ContinueStmt;
struct FreeStmt;

class Lexer;
class Parser;
class Interpreter;
class Environment;
class HeapSimulator;
class OwnershipTracker;
class DiagnosticsEngine;

} // namespace crashlang

#endif // CRASHLANG_COMMON_HPP
