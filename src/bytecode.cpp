#include "crashlang/bytecode.hpp"

#include <iomanip>
#include <sstream>

namespace crashlang {

// ── Opcode metadata ────────────────────────────────────────────────────────────

const char* opcode_name(OpCode op) {
    switch (op) {
        case OpCode::Constant:     return "OP_CONSTANT";
        case OpCode::Nil:          return "OP_NIL";
        case OpCode::True:         return "OP_TRUE";
        case OpCode::False:        return "OP_FALSE";
        case OpCode::Add:          return "OP_ADD";
        case OpCode::Subtract:     return "OP_SUBTRACT";
        case OpCode::Multiply:     return "OP_MULTIPLY";
        case OpCode::Divide:       return "OP_DIVIDE";
        case OpCode::Modulo:       return "OP_MODULO";
        case OpCode::Negate:       return "OP_NEGATE";
        case OpCode::Equal:        return "OP_EQUAL";
        case OpCode::NotEqual:     return "OP_NOT_EQUAL";
        case OpCode::Less:         return "OP_LESS";
        case OpCode::LessEqual:    return "OP_LESS_EQUAL";
        case OpCode::Greater:      return "OP_GREATER";
        case OpCode::GreaterEqual: return "OP_GREATER_EQUAL";
        case OpCode::Not:          return "OP_NOT";
        case OpCode::GetLocal:     return "OP_GET_LOCAL";
        case OpCode::SetLocal:     return "OP_SET_LOCAL";
        case OpCode::GetGlobal:    return "OP_GET_GLOBAL";
        case OpCode::SetGlobal:    return "OP_SET_GLOBAL";
        case OpCode::DefineGlobal: return "OP_DEFINE_GLOBAL";
        case OpCode::Jump:         return "OP_JUMP";
        case OpCode::JumpIfFalse:  return "OP_JUMP_IF_FALSE";
        case OpCode::Loop:         return "OP_LOOP";
        case OpCode::Call:         return "OP_CALL";
        case OpCode::Return:       return "OP_RETURN";
        case OpCode::Closure:      return "OP_CLOSURE";
        case OpCode::GetUpvalue:   return "OP_GET_UPVALUE";
        case OpCode::SetUpvalue:   return "OP_SET_UPVALUE";
        case OpCode::CloseUpvalue: return "OP_CLOSE_UPVALUE";
        case OpCode::Array:        return "OP_ARRAY";
        case OpCode::Index:        return "OP_INDEX";
        case OpCode::SetIndex:     return "OP_SET_INDEX";
        case OpCode::NewStruct:    return "OP_NEW_STRUCT";
        case OpCode::GetField:     return "OP_GET_FIELD";
        case OpCode::SetField:     return "OP_SET_FIELD";
        case OpCode::Ref:          return "OP_REF";
        case OpCode::Deref:        return "OP_DEREF";
        case OpCode::Move:         return "OP_MOVE";
        case OpCode::Free:         return "OP_FREE";
        case OpCode::Pop:          return "OP_POP";
        case OpCode::PopN:         return "OP_POP_N";
        case OpCode::Print:        return "OP_PRINT";
        case OpCode::Println:      return "OP_PRINTLN";
        case OpCode::Halt:         return "OP_HALT";
    }
    return "OP_UNKNOWN";
}

int opcode_operand_width(OpCode op) {
    switch (op) {
        // No operands.
        case OpCode::Nil:
        case OpCode::True:
        case OpCode::False:
        case OpCode::Add:
        case OpCode::Subtract:
        case OpCode::Multiply:
        case OpCode::Divide:
        case OpCode::Modulo:
        case OpCode::Negate:
        case OpCode::Equal:
        case OpCode::NotEqual:
        case OpCode::Less:
        case OpCode::LessEqual:
        case OpCode::Greater:
        case OpCode::GreaterEqual:
        case OpCode::Not:
        case OpCode::Index:
        case OpCode::SetIndex:
        case OpCode::Ref:
        case OpCode::Deref:
        case OpCode::Move:
        case OpCode::Free:
        case OpCode::Pop:
        case OpCode::Return:
        case OpCode::CloseUpvalue:
        case OpCode::Halt:
            return 0;

        // One-byte operand.
        case OpCode::GetLocal:
        case OpCode::SetLocal:
        case OpCode::GetUpvalue:
        case OpCode::SetUpvalue:
        case OpCode::Call:
        case OpCode::PopN:
        case OpCode::Print:
        case OpCode::Println:
            return 1;

        // Two-byte operand (16-bit).
        case OpCode::Constant:
        case OpCode::GetGlobal:
        case OpCode::SetGlobal:
        case OpCode::DefineGlobal:
        case OpCode::Jump:
        case OpCode::JumpIfFalse:
        case OpCode::Loop:
        case OpCode::Closure:
        case OpCode::Array:
        case OpCode::GetField:
        case OpCode::SetField:
            return 2;

        // Special: 3 bytes (2-byte type name index + 1-byte field count).
        case OpCode::NewStruct:
            return 3;
    }
    return 0;
}

// ── Chunk methods ──────────────────────────────────────────────────────────────

void Chunk::emit(OpCode op, uint32_t line) {
    code.push_back(static_cast<uint8_t>(op));
    lines.push_back(line);
}

void Chunk::emit(OpCode op, uint8_t operand, uint32_t line) {
    code.push_back(static_cast<uint8_t>(op));
    lines.push_back(line);
    code.push_back(operand);
    lines.push_back(line);
}

void Chunk::emit_wide(OpCode op, uint16_t operand, uint32_t line) {
    code.push_back(static_cast<uint8_t>(op));
    lines.push_back(line);
    code.push_back(static_cast<uint8_t>((operand >> 8) & 0xFF));
    lines.push_back(line);
    code.push_back(static_cast<uint8_t>(operand & 0xFF));
    lines.push_back(line);
}

uint16_t Chunk::add_constant(Value value) {
    constants.push_back(std::move(value));
    return static_cast<uint16_t>(constants.size() - 1);
}

void Chunk::emit_constant(Value value, uint32_t line) {
    uint16_t idx = add_constant(std::move(value));
    emit_wide(OpCode::Constant, idx, line);
}

size_t Chunk::emit_jump(OpCode op, uint32_t line) {
    size_t offset = code.size();
    emit_wide(op, 0xFFFF, line); // Placeholder.
    return offset;
}

void Chunk::patch_jump(size_t offset) {
    // The jump operand starts at offset+1 (after the opcode byte).
    size_t jump = code.size() - offset - 3; // 3 = opcode + 2 operand bytes
    if (jump > 0xFFFF) {
        // In a real compiler we'd handle this. For our purposes this is fine.
        return;
    }
    code[offset + 1] = static_cast<uint8_t>((jump >> 8) & 0xFF);
    code[offset + 2] = static_cast<uint8_t>(jump & 0xFF);
}

void Chunk::emit_loop(size_t loop_start, uint32_t line) {
    size_t offset = code.size() - loop_start + 3; // +3 for the loop instruction itself
    emit_wide(OpCode::Loop, static_cast<uint16_t>(offset), line);
}

// ── Disassembly ────────────────────────────────────────────────────────────────

std::string Chunk::disassemble() const {
    std::ostringstream out;
    out << "== " << name << " ==\n";

    std::string body;
    size_t offset = 0;
    while (offset < code.size()) {
        offset = disassemble_instruction(body, offset);
    }
    out << body;
    return out.str();
}

size_t Chunk::disassemble_instruction(std::string& out, size_t offset) const {
    std::ostringstream line;

    // Offset.
    line << std::setw(4) << std::setfill('0') << offset << "  ";

    // Line number (show '|' if same as previous).
    if (offset > 0 && lines[offset] == lines[offset - 1]) {
        line << "   | ";
    } else {
        line << std::setw(4) << std::setfill(' ') << lines[offset] << " ";
    }

    auto op = static_cast<OpCode>(code[offset]);
    line << std::left << std::setw(20) << std::setfill(' ') << opcode_name(op);

    int width = opcode_operand_width(op);

    if (width == 0) {
        line << "\n";
        out += line.str();
        return offset + 1;
    }

    if (width == 1) {
        uint8_t operand = code[offset + 1];
        line << (int)operand;

        // For get/set local, show the slot number.
        line << "\n";
        out += line.str();
        return offset + 2;
    }

    if (width == 2) {
        uint16_t operand = static_cast<uint16_t>((code[offset + 1] << 8) | code[offset + 2]);
        line << operand;

        // For constants, show the value.
        if (op == OpCode::Constant || op == OpCode::GetGlobal
            || op == OpCode::SetGlobal || op == OpCode::DefineGlobal
            || op == OpCode::Closure || op == OpCode::GetField
            || op == OpCode::SetField)
        {
            if (operand < constants.size()) {
                line << "    ; " << value_to_string(constants[operand]);
            }
        }

        // For jumps, show the target offset.
        if (op == OpCode::Jump || op == OpCode::JumpIfFalse) {
            line << "    -> " << (offset + 3 + operand);
        }
        if (op == OpCode::Loop) {
            line << "    -> " << (offset + 3 - operand);
        }

        line << "\n";
        out += line.str();
        return offset + 3;
    }

    if (width == 3) {
        // NewStruct: 2-byte type name index + 1-byte field count.
        uint16_t type_idx = static_cast<uint16_t>((code[offset + 1] << 8) | code[offset + 2]);
        uint8_t  field_count = code[offset + 3];
        line << type_idx << " fields=" << (int)field_count;
        if (type_idx < constants.size()) {
            line << "    ; " << value_to_string(constants[type_idx]);
        }
        line << "\n";
        out += line.str();
        return offset + 4;
    }

    line << "\n";
    out += line.str();
    return offset + 1;
}

} // namespace crashlang
