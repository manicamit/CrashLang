# CrashLang

A programming language where every runtime error explains itself.

Most languages fail you with something like `Segmentation fault (core dumped)`. No context, no cause, no fix. CrashLang is built around a different idea: when your program crashes, the crash report should tell you exactly what went wrong, where, why, and how to fix it.

```
RuntimeError: UseAfterFree
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

  What happened:
    You tried to read field 'data' on object 'buf',
    but 'buf' was already freed.

  Object lifetime:
    line 7   new()        buf allocated (id: heap#003)
    line 8   call         buf passed into parse_tokens()
    line 11  free()       buf freed inside parse_tokens()
    line 9   READ         <- you are here (after free)

  Why this is a problem:
    The memory backing 'buf' was released at line 11.
    Accessing it afterward reads memory that may have been
    reclaimed or overwritten by another allocation.

  Suggested fix:
    Read buf.data before passing buf into parse_tokens(),
    or restructure parse_tokens() so it doesn't free its
    argument.

━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
```

That is the entire point of this project.

## What is this

CrashLang is an interpreted language implemented in C++17. It has a simulated heap with explicit `new`/`free` semantics, an ownership tracker that records every object's full lifetime, and a diagnostics engine that turns internal errors into structured, human-readable crash reports.

The same design philosophy behind Rust's compiler errors and Clang's diagnostics — errors should explain themselves — applied from the ground up to a language runtime.

## Current status

This is early. The lexer and source infrastructure are done. Parser and interpreter are next.

**What works right now:**
- Full tokenizer with position tracking for every token
- Source file model with O(1) line lookup (for pointing at code in error messages)
- All operators, keywords, literals (int, float, string with escapes), identifiers
- Line comments (`//`), nested block comments (`/* ... /* ... */ ... */`)
- Error recovery in the lexer (malformed input produces error tokens, doesn't crash the scanner)

**What's coming:**
- Recursive descent parser with Pratt expression parsing
- Tree-walk interpreter with lexical scoping
- Simulated heap with allocation/deallocation tracking
- Ownership tracker (simplified Rust-style move semantics)
- The diagnostics engine (the whole point)
- REPL, CLI flags (`--ast`, `--trace`, `--tokens`), example programs

## Building

Requires a C++17 compiler and CMake 3.20+.

```
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
```

To enable AddressSanitizer:

```
cmake -B build -DCMAKE_BUILD_TYPE=Debug -DSANITIZE=ON
cmake --build build
```

## Usage

Right now you can tokenize a file:

```
./build/crashlang examples/01_hello_world.cl
```

This prints the source with line numbers, then the full token stream with types, positions, and literal values. Not very exciting yet — it will be.

## Language syntax

```
let x = 42;
let name = "crashlang";
let flag = true;

fn add(a, b) {
    return a + b;
}

if x > 10 {
    println("big");
} else {
    println("small");
}

while x > 0 {
    x = x - 1;
}

// Memory operations
let p = new Point { x: 1, y: 2 };
let r = ref(p);
let val = deref(r);
free(p);

// Ownership transfer — p is no longer valid after this
let q = move(p);
```

## Error types

The diagnostics engine will detect and explain all of these at runtime:

| Error | What it catches |
|---|---|
| UseAfterFree | Accessing an object after `free()` was called on it |
| DoubleFree | Calling `free()` on an already-freed object |
| NullDereference | Dereferencing a reference that was never assigned |
| OutOfBounds | Array index outside allocated range |
| OwnershipViolation | Using a moved value after ownership transfer |
| DivisionByZero | Dividing by zero |
| TypeMismatch | Operator applied to incompatible types |
| UndefinedVariable | Variable not in scope (with "did you mean?" suggestions) |
| StackOverflow | Unbounded recursion past the call depth limit |
| MemoryLeak | Allocations never freed (reported at program exit) |

## Project structure

```
CrashLang/
├── CMakeLists.txt
├── include/crashlang/
│   ├── common.hpp          # Type aliases, forward declarations
│   ├── source.hpp          # Source file model, locations, spans
│   ├── token.hpp           # Token types and Token struct
│   └── lexer.hpp           # Scanner
├── src/
│   ├── main.cpp            # Entry point
│   ├── source.cpp          # Source file implementation
│   ├── token.cpp           # Token utilities
│   └── lexer.cpp           # Scanner implementation
└── examples/
    ├── 00_lexer_test.cl    # Exercises all token types
    └── 01_hello_world.cl   # Basic program
```

More components (parser, interpreter, heap, ownership, diagnostics) will be added as separate commits as the project progresses.

## License

MIT
