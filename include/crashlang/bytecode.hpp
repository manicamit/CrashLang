#ifndef CRASHLANG_BYTECODE_HPP
#define CRASHLANG_BYTECODE_HPP

#include "crashlang/value.hpp"
#include "crashlang/source.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace crashlang {

enum class OpCode : uint8_t {
    // Constants
    Constant,       // [idx_hi] [idx_lo]
    Nil, True, False,

    // Arithmetic
    Add, Subtract, Multiply, Divide, Modulo, Negate,

    // Comparison
    Equal, NotEqual, Less, LessEqual, Greater, GreaterEqual,

    // Logic (and/or compile to jumps)
    Not,

    // Variables
    GetLocal,       // [slot]
    SetLocal,       // [slot]
    GetGlobal,      // [idx_hi] [idx_lo]
    SetGlobal,      // [idx_hi] [idx_lo]
    DefineGlobal,   // [idx_hi] [idx_lo]

    // Control flow
    Jump,           // [off_hi] [off_lo]
    JumpIfFalse,    // [off_hi] [off_lo]
    Loop,           // [off_hi] [off_lo]  (backward)

    // Functions
    Call,           // [argc]
    Return,
    Closure,        // [idx_hi] [idx_lo] [upvalue_count] [is_local, index]...

    // Upvalues
    GetUpvalue,     // [slot]
    SetUpvalue,     // [slot]
    CloseUpvalue,

    // Collections
    Array,          // [count_hi] [count_lo]
    Index,
    SetIndex,

    // Heap / memory
    NewStruct,      // [type_idx_hi] [type_idx_lo] [field_count]
    GetField,       // [name_idx_hi] [name_idx_lo]
    SetField,       // [name_idx_hi] [name_idx_lo]
    Ref, Deref, Move, Free,

    // Stack
    Pop,
    PopN,           // [count]

    // I/O
    Print,          // [argc]
    Println,        // [argc]

    Halt,
};

const char* opcode_name(OpCode op);
int opcode_operand_width(OpCode op);

// A compiled unit of bytecode — one per function/script.
struct Chunk {
    std::vector<uint8_t>  code;
    std::vector<Value>    constants;
    std::vector<uint32_t> lines;       // source line per byte
    std::vector<Span>     spans;
    std::string           name;

    void emit(OpCode op, uint32_t line);
    void emit(OpCode op, uint8_t operand, uint32_t line);
    void emit_wide(OpCode op, uint16_t operand, uint32_t line);

    uint16_t add_constant(Value value);
    void emit_constant(Value value, uint32_t line);

    size_t current_offset() const { return code.size(); }
    size_t emit_jump(OpCode op, uint32_t line);
    void patch_jump(size_t offset);
    void emit_loop(size_t loop_start, uint32_t line);

    std::string disassemble() const;
    size_t disassemble_instruction(std::string& out, size_t offset) const;
};

} // namespace crashlang

#endif
