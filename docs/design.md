# CrashLang — Design Document

> A programming language where every runtime error explains itself.

---

## The Problem

Most languages fail you with something like:

```
Segmentation fault (core dumped)
```

No context. No cause. No fix. CrashLang is built around a single principle: **a crash should explain itself**. Instead of an opaque signal, you get a structured report with source context, an object lifetime timeline, an explanation of why it's unsafe, and a concrete fix suggestion.

---

## Architecture

CrashLang is implemented in ~8,800 lines of C++17. It has two execution paths:

```
Source → Lexer → Parser → AST → [Optimizer] → Tree-walk Interpreter (default)
                                             └→ Compiler → Bytecode → Stack VM (--vm)
```

### Frontend (shared)

| Component | File | Role |
|---|---|---|
| Source model | `source.hpp/cpp` | File loading, line lookup by number |
| Lexer | `lexer.hpp/cpp` | Scanner with source position tracking on every token |
| Parser | `parser.hpp/cpp` | Hand-written recursive descent, 10-level operator precedence |
| AST | `ast.hpp/cpp` | `std::variant`-based node types, no inheritance hierarchy |
| Optimizer | `optimizer.hpp/cpp` | Constant folding (int/float/bool/string), dead branch elimination |

### Backend 1: Tree-walk Interpreter

| Component | File | Role |
|---|---|---|
| Interpreter | `interpreter.hpp/cpp` | Recursive AST evaluation |
| Environment | `environment.hpp/cpp` | Lexical scope chain with parent pointers |
| Builtins | `builtins.hpp/cpp` | 30+ built-in functions (I/O, collections, strings, math) |

### Backend 2: Bytecode Compiler + Stack VM

| Component | File | Role |
|---|---|---|
| Bytecode | `bytecode.hpp/cpp` | Opcode enum (40+), `Chunk` with constants pool, disassembler |
| Compiler | `compiler.hpp/cpp` | Single-pass AST-to-bytecode with upvalue resolution |
| VM | `vm.hpp/cpp` | Stack-based dispatch loop with flat closure support |

### Shared runtime

| Component | File | Role |
|---|---|---|
| Value | `value.hpp/cpp` | `std::variant`-based runtime value (int, float, string, array, function, heap ref) |
| Heap | `heap.hpp/cpp` | Simulated heap with explicit `new`/`free`, lifetime tracking |
| Ownership | `ownership.hpp/cpp` | Object lifetime event log for timeline-based diagnostics |
| Diagnostics | `diagnostics.hpp/cpp` | Structured crash report formatter (4-section output) |
| Errors | `errors.hpp/cpp` | `CrashError` with kind, location, metadata, call stack |

---

## Language Features

### Types and values
- Integers (`int64_t`), floats (`double`), booleans, strings, nil
- Arrays with dynamic sizing
- Heap-allocated structs via `new TypeName { field: value }`
- First-class functions, lambdas, closures with mutable upvalue capture

### Control flow
- `if`/`else` with optional chaining
- `while` loops with `break`/`continue`
- `for`/`in` loops over arrays and strings
- `match` expressions with `when` arms and `_` wildcard

### Memory model
- `new` — allocate a struct on the simulated heap
- `free` — explicitly deallocate
- `ref` — create a reference (tracked pointer)
- `deref` — follow a reference
- `move` — transfer ownership (source becomes nil)

### Error detection (12 categories)

| Error | Detection |
|---|---|
| UseAfterFree | Accessing a freed heap object |
| DoubleFree | Freeing an already-freed object |
| NullDereference | Dereferencing nil |
| OutOfBounds | Array/string index past bounds |
| OwnershipViolation | Using a moved value |
| DivisionByZero | Integer or float division by zero |
| TypeMismatch | Incompatible operand types |
| UndefinedVariable | Variable not in scope (with "did you mean?" suggestions) |
| StackOverflow | Recursion depth exceeded |
| ArityMismatch | Wrong argument count |
| InvalidFieldAccess | Nonexistent struct field |
| MemoryLeak | Allocation never freed (via `--check-leaks`) |

---

## Compiler Internals

### Upvalue resolution (closures)

The compiler implements the "Crafting Interpreters" upvalue algorithm:

1. When an inner function references a variable from an enclosing scope, the compiler marks that local as `is_captured`.
2. `resolve_upvalue` walks the scope chain to find the variable — either as a direct local in the parent (an "open" upvalue) or transitively through the parent's upvalue table.
3. At code generation, `OP_CLOSURE` emits the function constant followed by inline upvalue descriptors: `(is_local, index)` pairs.
4. The VM maintains an `ObjUpvalue` linked list. Open upvalues point directly into the stack. When `OP_CLOSE_UPVALUE` fires (at scope exit), the value is promoted to the heap.

### Constant folding optimizer

The optimizer walks the AST bottom-up:
- **Binary folding**: `3 + 4` → `7`, `"a" + "b"` → `"ab"`, `true and false` → `false`
- **Unary folding**: `-42` → `-42` (literal), `not true` → `false`
- **Dead branch elimination**: `if true { body }` → `body`, `if false { body }` → removed

### Bytecode format

Instructions are variable-width. Most are 1 byte (opcode) + 1 byte (operand). Wide operands use 2 bytes (big-endian). `OP_CLOSURE` has variable trailing data for upvalue descriptors.

---

## Diagnostics Engine

Every crash report has four sections:

1. **What happened** — plain-English description of the error
2. **Object lifetime** — chronological trace from allocation to crash, sourced from the ownership tracker
3. **Why this is a problem** — explanation of why the operation is unsafe
4. **Suggested fix** — concrete, actionable recommendation with example code

The design philosophy mirrors Rust's compiler errors and Clang's diagnostics: errors should explain themselves.

---

## Build Phases

| Phase | Scope |
|---|---|
| 1–2 | Core language: lexer, parser, interpreter, scoping |
| 3 | Memory model: heap, ownership, `new`/`free`/`ref`/`deref`/`move` |
| 4 | Diagnostics engine: structured crash reports |
| 5 | Advanced features: string ops, lambdas, HOFs, builtins |
| 6 | Examples and test suite |
| 7 | Formal grammar specification (EBNF) |
| 8 | Bytecode compiler + stack VM |
| 9 | Higher-order function support in VM |
| 10 | CI pipeline (GCC + Clang + ASan) |
| 11 | Closures (upvalues), `match` expressions, `break`/`continue`, constant folding optimizer |

---

## Build and Run

```
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
./build/crashlang examples/01_hello_world.cl      # Tree-walk
./build/crashlang --vm examples/12_vm_test.cl      # Bytecode VM
./build/crashlang --optimize --vm program.cl       # With optimizer
bash tests/run_tests.sh                            # Full test suite
```

---

*Built to demonstrate that a language's runtime can be as helpful as its compiler.*
