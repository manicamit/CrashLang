# CrashLang

A programming language where every runtime error explains itself.

Most languages fail you with something like `Segmentation fault (core dumped)`. No context, no cause, no fix. CrashLang is built around a different idea: when your program crashes, the crash report should tell you exactly what went wrong, where, why, and how to fix it.

```
+-- RuntimeError: UseAfterFree ----------------------------------+
|
|  examples/05_use_after_free.cl:9:15
|
|   7 |
|   8 | // This should crash with UseAfterFree
|  -> 9 | let val = buf.data;
|       |               ^^^
|
|  What happened:
|    tried to read field 'data' on 'Buffer' (heap#1), but it was already freed
|
|  Object lifetime [heap#1]:
|    line    3  new()     -- buf allocated as Buffer
|    line    4  READ      -- buf.data read
|    line    6  free()    -- buf freed
|    line    9  READ      -- buf.data read
|  -> line    9  read field 'data'  <- you are here (after free)
|
|  Why this is a problem:
|    The memory backing this object was released at line 6.
|    Accessing it afterward reads memory that may have been
|    reclaimed or overwritten by another allocation.
|
|  Suggested fix:
|    Read the field before calling free(), or restructure
|    the code so the object is not accessed after being freed.
|
|
+--------------------------------------------------------------+
```

That is the entire point of this project.

## What is this

CrashLang is an interpreted language implemented in C++17. It has a simulated heap with explicit `new`/`free` semantics, an ownership tracker that records every object's full lifetime, and a diagnostics engine that turns runtime errors into structured, human-readable crash reports.

The same design philosophy behind Rust's compiler errors and Clang's diagnostics -- errors should explain themselves -- applied from the ground up to a language runtime.

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

Run a program:

```
./build/crashlang examples/01_hello_world.cl
```

Start the interactive REPL:

```
./build/crashlang
```

```
CrashLang 0.3.0 — interactive mode
Type expressions or statements. Use :quit or Ctrl-D to exit.

crash> let x = 42;
crash> println(x * 2);
84
crash> :quit
Goodbye.
```

Check for memory leaks at exit:

```
./build/crashlang --check-leaks examples/10_memory_leak.cl
```

Other flags:

```
--tokens <file>     Dump token stream
--ast <file>        Dump AST
--no-color <file>   Disable ANSI colors
--version           Show version
--help              Show help
```

## Language syntax

```
// Variables and types
let x = 42;
let name = "crashlang";
let flag = true;
let items = [1, 2, 3];

// Functions
fn add(a, b) {
    return a + b;
}

// Lambdas (anonymous functions)
let double = fn(x) { return x * 2; };

// Closures
fn make_adder(n) {
    return fn(x) { return x + n; };
}

// Control flow
if x > 10 {
    println("big");
} else {
    println("small");
}

while x > 0 {
    x = x - 1;
}

for item in items {
    println(item);
}

// String iteration
for ch in "hello" {
    print(ch);
}

// String indexing
let first = name[0];

// Memory operations
let p = new Point { x: 1, y: 2 };
println(p.x);
p.x = 10;
let r = ref(p);
let val = deref(r);
free(p);

// Ownership transfer
let q = move(p);
// p is now nil -- using it will crash with a diagnostic
```

## Built-in functions

| Function | Description |
|---|---|
| `println(args...)` | Print values with newline |
| `print(args...)` | Print values without newline |
| `len(val)` | Length of string or array |
| `type_of(val)` | Type name as string |
| `to_string(val)` | Convert to string |
| `to_int(val)` | Convert to integer |
| `to_float(val)` | Convert to float |
| `assert(cond, msg?)` | Crash if condition is false |
| `push(arr, val)` | Append to array |
| `pop(arr)` | Remove and return last element |
| `range(start, end)` | Generate integer array |
| `input(prompt?)` | Read a line from stdin |
| `clock()` | Seconds since epoch |
| `sqrt(n)`, `abs(n)` | Math functions |
| `max(a, b)`, `min(a, b)` | Numeric comparison |
| `upper(s)`, `lower(s)` | Case conversion |
| `trim(s)` | Strip whitespace |
| `contains(s, sub)` | Substring check |
| `starts_with(s, pre)` | Prefix check |
| `ends_with(s, suf)` | Suffix check |
| `replace(s, old, new)` | Replace all occurrences |
| `substr(s, start, len?)` | Extract substring |
| `split(s, delim)` | Split string into array |
| `join(arr, sep)` | Join array into string |
| `chars(s)` | String to char array |
| `reverse(val)` | Reverse string or array |

## Error detection

All of these are detected at runtime with structured crash reports:

| Error | What it catches |
|---|---|
| UseAfterFree | Accessing an object after `free()` was called on it |
| DoubleFree | Calling `free()` on an already-freed object |
| NullDereference | Dereferencing a reference that was never assigned |
| OutOfBounds | Array or string index outside valid range |
| OwnershipViolation | Using a moved value after ownership transfer |
| DivisionByZero | Dividing by zero |
| TypeMismatch | Operator applied to incompatible types |
| UndefinedVariable | Variable not in scope (with "did you mean?" suggestions) |
| StackOverflow | Unbounded recursion past the call depth limit |
| MemoryLeak | Allocations never freed (reported with `--check-leaks`) |

Each crash report includes:
- Source context with the offending line highlighted
- Plain-English "What happened" description
- Object lifetime timeline showing every event from allocation to crash
- Technical "Why this is a problem" explanation
- Actionable "Suggested fix" recommendation

## Running tests

```
bash tests/run_tests.sh
```

## Project structure

```
CrashLang/
├── CMakeLists.txt
├── include/crashlang/
│   ├── common.hpp          # Type aliases, forward declarations
│   ├── source.hpp          # Source file model, locations, spans
│   ├── token.hpp           # Token types and Token struct
│   ├── lexer.hpp           # Scanner
│   ├── ast.hpp             # AST node types (variant-based)
│   ├── parser.hpp          # Recursive descent parser
│   ├── value.hpp           # Runtime value representation
│   ├── environment.hpp     # Lexical scope chain
│   ├── errors.hpp          # Error kinds and CrashError struct
│   ├── interpreter.hpp     # Tree-walk interpreter
│   ├── builtins.hpp        # Built-in function registry
│   ├── heap.hpp            # Simulated heap with lifetime tracking
│   ├── ownership.hpp       # Ownership and event timeline tracking
│   └── diagnostics.hpp     # Crash report formatter
├── src/
│   ├── main.cpp            # Entry point, CLI, and REPL
│   ├── source.cpp          # Source file implementation
│   ├── token.cpp           # Token utilities
│   ├── lexer.cpp           # Scanner implementation
│   ├── ast.cpp             # AST pretty-printer
│   ├── parser.cpp          # Parser implementation
│   ├── value.cpp           # Value display and coercion
│   ├── environment.cpp     # Scope management
│   ├── errors.cpp          # Error utilities
│   ├── interpreter.cpp     # Interpreter implementation
│   ├── builtins.cpp        # Built-in function implementations
│   ├── heap.cpp            # Heap simulator
│   ├── ownership.cpp       # Ownership tracker
│   └── diagnostics.cpp     # Diagnostics engine
├── examples/
│   ├── 01_hello_world.cl   # Fibonacci, arrays, closures, sorting
│   ├── 03_error_test.cl    # Deliberate type mismatch across call stack
│   ├── 04_heap_basic.cl    # Heap allocation, fields, ref, deref, free
│   ├── 05_use_after_free.cl# Use-after-free detection
│   ├── 06_double_free.cl   # Double-free detection
│   ├── 07_ownership.cl     # Use-after-move detection
│   ├── 08_null_deref.cl    # Null dereference detection
│   ├── 09_flagship.cl      # Cross-function use-after-free
│   ├── 10_memory_leak.cl   # Memory leak detection
│   └── 11_advanced.cl      # Lambdas, string ops, closures
└── tests/
    └── run_tests.sh        # Automated test runner (16 tests)
```

## License

MIT
