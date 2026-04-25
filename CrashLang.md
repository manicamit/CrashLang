# CrashLang

> A programming language designed so every runtime error gives a human-readable explanation.

---

## The Problem

Every language you've used has failed you with something like this:

```
Segmentation fault (core dumped)
```

No context. No cause. No fix. Just a process exit code and a shrug.

CrashLang is built around a single principle: **a crash should explain itself**. Instead of an opaque signal, you get this:

```
RuntimeError: UseAfterFree at line 19, in function parse_tokens()

  What happened:
    You accessed the object 'buf' after it was freed.

  Timeline:
    line 12  →  buf was allocated with new()
    line 15  →  buf was passed into tokenize() by reference
    line 17  →  free(buf) was called inside tokenize()
    line 19  →  YOU ARE HERE — buf.data was read after its lifetime ended

  Why this is a problem:
    The memory at that address may have been reclaimed or overwritten.
    Reading it produces undefined behavior — or in CrashLang, this error.

  Suggested fix:
    Don't free buf before its last use, or clone it before passing it
    into a function that takes ownership.
```

That is the entire point of this project.

---

## What You're Building

CrashLang is a fully implemented interpreted language with five core components:

### 1. Lexer
Converts raw source text into a token stream. Every token records its exact line number, column, and character span in the source file. This position metadata flows through the entire pipeline — it's what makes error messages point at real code instead of abstract offsets.

Tokens cover: integer and float literals, strings, booleans, identifiers, all operators, delimiters, and a complete keyword set including `new`, `free`, `ref`, `deref`, and `move`.

### 2. Parser and AST
A hand-written recursive descent parser that produces a typed Abstract Syntax Tree. Every AST node carries the source location of the construct it represents. The grammar supports:

- Variable declarations (`let x = expr`)
- Arithmetic and boolean expressions with correct precedence
- Function definitions and calls
- If/else branches
- While loops
- Memory operations (`new`, `free`, `ref`, `deref`)
- Struct-like object construction

The AST is the data structure that every later phase operates on. Getting it right — clean node types, accurate source spans, no ambiguity — is what makes the rest of the system possible.

### 3. Interpreter
A tree-walk interpreter that evaluates AST nodes recursively. It maintains an environment stack (a linked chain of scopes) that correctly implements lexical scoping. Function calls push a new scope frame; returns pop it. Every evaluation step is wrapped in a diagnostic context object that records the current call chain, so when something fails mid-execution, the interpreter knows exactly where it is in the program.

### 4. Memory Model
A simulated heap with explicit allocation and deallocation semantics. Every call to `new` registers an object in the heap with a unique ID. Every call to `free` marks it as dead and records the source location of the deallocation. References (`ref`) are tracked as pointers back into the heap — the model knows which reference points to which heap object and whether that object is still alive.

The memory model detects:

| Error | Description |
|---|---|
| `UseAfterFree` | Accessing an object after `free()` was called on it |
| `DoubleFree` | Calling `free()` on an already-freed object |
| `NullDereference` | Dereferencing a reference that was never assigned |
| `OutOfBounds` | Accessing an array index outside its allocated range |
| `OwnershipViolation` | Using a moved value after ownership was transferred |
| `DivisionByZero` | Dividing by zero at runtime |
| `TypeMismatch` | Applying an operator to incompatible types |
| `UndefinedVariable` | Reading a variable that isn't in scope |
| `StackOverflow` | Unbounded recursion exceeding the call depth limit |

### 5. Ownership Tracker
A scope-aware table that tracks every heap-allocated object throughout its lifetime. For each object it records: who allocated it, which scope owns it, whether it has been moved (and to where), whether it has been freed, and every reference that points to it. This is the data source for the "Timeline" section of crash messages — the ownership tracker is what lets CrashLang say *"this object was freed at line 17 inside tokenize()"* rather than just *"invalid memory access"*.

### 6. Diagnostics Engine
The component that makes this project unique. The diagnostics engine maps every internal error code to a structured human-readable report with four sections:

- **What happened** — a plain-English statement of the error
- **Timeline** — a chronological trace of the relevant object's lifetime, sourced from the ownership tracker
- **Why this is a problem** — a brief explanation of why the operation is unsafe or undefined
- **Suggested fix** — a concrete, actionable recommendation

This is the same philosophy behind Rust's compiler error messages and Clang's diagnostics — production tools are praised specifically because they explain failures rather than just reporting them.

---

## Language Syntax

```
// Variable declaration
let x = 42;
let name = "crashlang";
let flag = true;

// Functions
fn add(a, b) {
    return a + b;
}

// Control flow
if x > 10 {
    let msg = "big";
} else {
    let msg = "small";
}

while x > 0 {
    x = x - 1;
}

// Memory operations
let p = new Point { x: 1, y: 2 };
let r = ref(p);
let val = deref(r);
free(p);

// Ownership transfer
let q = move(p);   // p is no longer valid after this
```

---

## Project Structure

```
crashlang/
├── src/
│   ├── lexer.py          # Tokenizer with source position tracking
│   ├── parser.py         # Recursive descent parser
│   ├── ast_nodes.py      # AST node definitions (dataclasses)
│   ├── interpreter.py    # Tree-walk evaluator
│   ├── environment.py    # Scope chain and variable resolution
│   ├── memory.py         # Heap simulator and allocation table
│   ├── ownership.py      # Ownership tracker and lifetime records
│   ├── diagnostics.py    # Error types and human-readable reporters
│   └── errors.py         # CrashLang exception hierarchy
├── examples/
│   ├── hello_world.cl
│   ├── use_after_free.cl
│   ├── double_free.cl
│   ├── null_deref.cl
│   ├── out_of_bounds.cl
│   ├── ownership_transfer.cl
│   ├── stack_overflow.cl
│   └── type_mismatch.cl
├── tests/
│   ├── test_lexer.py
│   ├── test_parser.py
│   ├── test_interpreter.py
│   └── test_memory.py
├── repl.py               # Interactive REPL
├── cli.py                # File-mode runner: `crashlang run file.cl`
└── README.md
```

---

## Build Phases

### Phase 1 — Core language
Implement the lexer, parser, and AST. Support integer arithmetic, string literals, variable declarations, if/else, while loops, and function definitions. Goal: get a working interpreter that can run simple programs without any memory features yet.

### Phase 2 — Scope and environment
Build the variable environment as a linked scope chain. Implement proper lexical scoping so inner functions can read outer variables but not the reverse. Add the `UndefinedVariable` diagnostic with a message that shows the closest in-scope name (useful "did you mean X?" behavior).

### Phase 3 — Memory model
Introduce `new`, `free`, `ref`, `deref`, and `move`. Build the heap simulator and the ownership table. Every allocation gets a unique object ID, a birth location, an owner, and a death location. Start detecting `UseAfterFree` and `DoubleFree`.

### Phase 4 — Full diagnostics engine
Write the crash reporter. For each error kind, produce the four-section structured message. Source the Timeline from the ownership tracker. Make the output readable at a glance — clear labels, consistent indentation, the offending source line quoted inline.

### Phase 5 — REPL and CLI
Build `repl.py` for interactive use and `cli.py` for running `.cl` files. Add a `--trace` flag that prints the ownership table state after every statement. Add a `--ast` flag that dumps the parsed AST before running. These flags are invaluable for demos and debugging.

### Phase 6 — Examples and tests
Write one example program per error type in `examples/`. Each should be a minimal, clear reproduction of that specific crash. These become your test suite inputs and your portfolio showcase — a visitor can run any example and see exactly what CrashLang produces.

---

## Compiler Concepts Covered

This project is a hands-on implementation of the following topics from compiler theory:

- **Lexical analysis** — scanning, tokenization, source position tracking
- **Recursive descent parsing** — hand-written top-down parser, operator precedence
- **Abstract Syntax Trees** — node design, tree construction, source span annotation
- **Scope resolution** — lexical scoping, symbol tables, shadowing rules
- **Semantic analysis** — type checking, ownership validation before execution
- **Tree-walk interpretation** — recursive evaluation, environment management
- **Call stack simulation** — frame push/pop, stack depth tracking, overflow detection
- **Heap simulation** — manual allocation, deallocation, reference tracking
- **Ownership semantics** — move semantics, lifetime analysis (simplified Rust model)
- **Diagnostic messages** — structured error reporting, source-location attribution

This is roughly the first half of a compiler course, plus a runtime systems component that most compiler courses don't cover in depth.

---

## Why This Stands Out

Most "build your own language" projects end at a working calculator. The parts that make CrashLang different:

**The diagnostics engine is production-quality thinking.** Rust's compiler became famous because of its error messages. The same design philosophy — *errors should explain themselves* — is what this project is built around from the start, not bolted on afterward.

**Ownership tracking is non-trivial.** Implementing even a simplified version of Rust's ownership model requires understanding lifetimes, scopes, and move semantics at a mechanical level. It shows you know how memory-safe languages work under the hood, not just how to use them.

**It's self-demonstrating.** A recruiter or interviewer can clone the repo, run `python cli.py run examples/use_after_free.cl`, and immediately see something impressive without reading any documentation. The output *is* the explanation.

**It's extensible.** Natural follow-on features — a bytecode compiler, a type inference pass, a borrow checker that catches errors at parse time rather than runtime, a VS Code language extension — each add another layer of systems depth without starting over.

---

## Suggested Stack

| Component | Recommendation |
|---|---|
| Implementation language | Python 3.11+ (fast iteration, clean dataclasses for AST nodes) |
| Testing | `pytest` with one test file per module |
| CLI | `argparse` or `click` |
| Optional v2 | Port the interpreter core to Go or C for performance |

---

## Example: What a Full Crash Looks Like

**Source file** (`examples/use_after_free.cl`):

```
fn parse_tokens(buf) {
    let result = buf.data;
    free(buf);
    return result;
}

let buf = new Buffer { data: "hello" };
let tokens = parse_tokens(buf);
let second = buf.data;   // line 9 — crash here
```

**CrashLang output:**

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
    line 9   READ         ← you are here (after free)

  Call stack at crash:
    <main>  line 9
      → parse_tokens()  line 11

  Why this is a problem:
    The memory backing 'buf' was released at line 11.
    Accessing it afterward reads memory that may have been
    reclaimed or overwritten by another allocation.

  Suggested fix:
    Read buf.data before passing buf into parse_tokens(),
    or restructure parse_tokens() so it doesn't free its argument.

━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
```

---

*Built to demonstrate that a language's runtime can be as helpful as its compiler.*
