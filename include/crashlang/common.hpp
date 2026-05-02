#ifndef CRASHLANG_COMMON_HPP
#define CRASHLANG_COMMON_HPP

#include <cstdint>
#include <string>

namespace crashlang {

using HeapID = uint64_t;
using LineNo = uint32_t;
using ColNo  = uint32_t;

constexpr HeapID INVALID_HEAP_ID = 0;

// Forward declarations.

struct SourceFile;
struct SourceLocation;
struct Span;

struct Token;

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
struct LambdaExpr;
struct MatchExpr;

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

#endif
