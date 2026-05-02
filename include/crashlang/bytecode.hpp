#ifndef CRASHLANG_BYTECODE_HPP
#define CRASHLANG_BYTECODE_HPP

/// Bytecode instruction set and Chunk representation for CrashLang.
///
/// Each instruction is a single-byte opcode, optionally followed by operand
/// bytes. The Chunk stores bytecode, a constants pool, and a line-number
/// table that maps every instruction back to its source line (for diagnostics).

#include "crashlang/value.hpp"
#include "crashlang/source.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace crashlang {

// ── Opcodes ────────────────────────────────────────────────────────────────────

enum class OpCode : uint8_t {
    // ── Constants & literals ───────────────────────────────────────────────────
    Constant,       // [idx_hi] [idx_lo]  — push constants[idx]
    Nil,            //                     — push nil
    True,           //                     — push true
    False,          //                     — push false

    // ── Arithmetic ─────────────────────────────────────────────────────────────
    Add,            // pop b, pop a, push a+b
    Subtract,       // pop b, pop a, push a-b
    Multiply,       // pop b, pop a, push a*b
    Divide,         // pop b, pop a, push a/b
    Modulo,         // pop b, pop a, push a%b
    Negate,         // pop a, push -a

    // ── Comparison ─────────────────────────────────────────────────────────────
    Equal,          // pop b, pop a, push a==b
    NotEqual,       // pop b, pop a, push a!=b
    Less,           // pop b, pop a, push a<b
    LessEqual,      // pop b, pop a, push a<=b
    Greater,        // pop b, pop a, push a>b
    GreaterEqual,   // pop b, pop a, push a>=b

    // ── Logic ──────────────────────────────────────────────────────────────────
    Not,            // pop a, push !a
    // And/Or are compiled to jumps, not opcodes.

    // ── Variables ──────────────────────────────────────────────────────────────
    GetLocal,       // [slot]              — push stack[frame_base + slot]
    SetLocal,       // [slot]              — stack[frame_base + slot] = peek()
    GetGlobal,      // [idx_hi] [idx_lo]   — push globals[name at constants[idx]]
    SetGlobal,      // [idx_hi] [idx_lo]   — globals[name] = peek()
    DefineGlobal,   // [idx_hi] [idx_lo]   — define globals[name] = pop()

    // ── Control flow ───────────────────────────────────────────────────────────
    Jump,           // [off_hi] [off_lo]   — ip += offset
    JumpIfFalse,    // [off_hi] [off_lo]   — if !peek(): ip += offset
    Loop,           // [off_hi] [off_lo]   — ip -= offset (backwards jump)

    // ── Functions ──────────────────────────────────────────────────────────────
    Call,           // [argc]              — call value at stack[top - argc]
    Return,         //                     — return from current frame
    Closure,        // [idx_hi] [idx_lo] [upvalue_count] [is_local, index]...
                    //                     — create closure from constants[idx]

    // ── Upvalues (closure capture) ─────────────────────────────────────────────
    GetUpvalue,     // [slot]              — push upvalue at slot
    SetUpvalue,     // [slot]              — set upvalue at slot = peek()
    CloseUpvalue,   //                     — close the topmost open upvalue

    // ── Collections ────────────────────────────────────────────────────────────
    Array,          // [count_hi] [count_lo] — pop count values, push array
    Index,          //                     — pop index, pop obj, push obj[index]
    SetIndex,       //                     — pop val, pop index, pop arr, set arr[index]=val

    // ── Heap / memory ──────────────────────────────────────────────────────────
    NewStruct,      // [type_idx_hi] [type_idx_lo] [field_count]
    GetField,       // [name_idx_hi] [name_idx_lo]
    SetField,       // [name_idx_hi] [name_idx_lo]
    Ref,            //                     — pop obj, push ref(obj)
    Deref,          //                     — pop ref, push deref(ref)
    Move,           //                     — pop obj, push moved obj, nil source
    Free,           //                     — pop obj, free it

    // ── Stack management ───────────────────────────────────────────────────────
    Pop,            //                     — discard TOS
    PopN,           // [count]             — discard top N values

    // ── I/O ────────────────────────────────────────────────────────────────────
    Print,          // [argc]              — pop argc values, print space-separated
    Println,        // [argc]              — same but with trailing newline

    // ── Halt ───────────────────────────────────────────────────────────────────
    Halt,           //                     — stop execution
};

/// Human-readable name for an opcode.
const char* opcode_name(OpCode op);

/// How many operand bytes follow this opcode.
int opcode_operand_width(OpCode op);

// ── Chunk ──────────────────────────────────────────────────────────────────────

/// A compiled unit of bytecode — one per function (plus one for top-level).
struct Chunk {
    /// The raw bytecode stream.
    std::vector<uint8_t> code;

    /// Constants pool — values referenced by Constant instructions.
    std::vector<Value> constants;

    /// Source line for each byte in `code` (for diagnostics).
    /// lines[i] is the source line that produced code[i].
    std::vector<uint32_t> lines;

    /// Source spans for each instruction start (indexed by instruction offset).
    /// Not every byte has a span — only the first byte of each instruction.
    std::vector<Span> spans;

    /// Name of the function this chunk belongs to (or "<script>" for top-level).
    std::string name;

    // ── Emit helpers ───────────────────────────────────────────────────────────

    /// Emit a single-byte instruction.
    void emit(OpCode op, uint32_t line);

    /// Emit an instruction with a single byte operand.
    void emit(OpCode op, uint8_t operand, uint32_t line);

    /// Emit an instruction with a 16-bit operand (big-endian).
    void emit_wide(OpCode op, uint16_t operand, uint32_t line);

    /// Add a constant and return its index.
    uint16_t add_constant(Value value);

    /// Emit OpConstant for a value.
    void emit_constant(Value value, uint32_t line);

    /// Current offset (for jump patching).
    size_t current_offset() const { return code.size(); }

    /// Emit a jump placeholder, return the offset to patch later.
    size_t emit_jump(OpCode op, uint32_t line);

    /// Patch a previously emitted jump to land at the current offset.
    void patch_jump(size_t offset);

    /// Emit a backwards jump (loop).
    void emit_loop(size_t loop_start, uint32_t line);

    // ── Disassembly ────────────────────────────────────────────────────────────

    /// Pretty-print the entire chunk (for --disasm).
    std::string disassemble() const;

    /// Disassemble a single instruction at the given offset.
    /// Returns the offset of the next instruction.
    size_t disassemble_instruction(std::string& out, size_t offset) const;
};

} // namespace crashlang

#endif // CRASHLANG_BYTECODE_HPP
