#include "crashlang/ir.hpp"

#include <sstream>

namespace crashlang {

const char* ir_type_name(IRType t) {
    switch (t) {
        case IRType::I64:  return "i64";
        case IRType::F64:  return "f64";
        case IRType::Bool: return "pred";
        case IRType::Str:  return "str";
        case IRType::Void: return "void";
    }
    return "?";
}

const char* irop_name(IROp op) {
    switch (op) {
        case IROp::IAdd:      return "add.i64";
        case IROp::ISub:      return "sub.i64";
        case IROp::IMul:      return "mul.i64";
        case IROp::IDiv:      return "div.i64";
        case IROp::IMod:      return "rem.i64";
        case IROp::FAdd:      return "add.f64";
        case IROp::FSub:      return "sub.f64";
        case IROp::FMul:      return "mul.f64";
        case IROp::FDiv:      return "div.f64";
        case IROp::INeg:      return "neg.i64";
        case IROp::FNeg:      return "neg.f64";
        case IROp::Eq:        return "cmp.eq";
        case IROp::Neq:       return "cmp.ne";
        case IROp::Lt:        return "cmp.lt";
        case IROp::Le:        return "cmp.le";
        case IROp::Gt:        return "cmp.gt";
        case IROp::Ge:        return "cmp.ge";
        case IROp::Not:       return "not.pred";
        case IROp::LoadConst: return "mov";
        case IROp::LoadParam: return "load.arg";
        case IROp::Copy:      return "mov";
        case IROp::Call:      return "call";
        case IROp::Ret:       return "ret";
        case IROp::Br:        return "br";
        case IROp::BrIf:      return "br.if";
    }
    return "???";
}

std::string ir_constant_to_string(const IRConstant& c) {
    if (auto* i = std::get_if<int64_t>(&c))     return std::to_string(*i);
    if (auto* f = std::get_if<double>(&c))       return std::to_string(*f);
    if (auto* b = std::get_if<bool>(&c))         return *b ? "1" : "0";
    if (auto* s = std::get_if<std::string>(&c))  return "\"" + *s + "\"";
    return "?";
}

static std::string reg(Reg r) {
    if (r == NO_REG) return "_";
    return "%r" + std::to_string(r);
}

std::string dump_instr(const IRInstr& instr) {
    std::ostringstream out;
    out << "    ";

    switch (instr.op) {
    case IROp::IAdd: case IROp::ISub: case IROp::IMul: case IROp::IDiv: case IROp::IMod:
    case IROp::FAdd: case IROp::FSub: case IROp::FMul: case IROp::FDiv:
    case IROp::Eq: case IROp::Neq: case IROp::Lt: case IROp::Le:
    case IROp::Gt: case IROp::Ge:
        out << reg(instr.dst) << " = " << irop_name(instr.op)
            << " " << reg(instr.src1) << ", " << reg(instr.src2);
        break;

    case IROp::INeg: case IROp::FNeg: case IROp::Not:
        out << reg(instr.dst) << " = " << irop_name(instr.op)
            << " " << reg(instr.src1);
        break;

    case IROp::LoadConst:
        out << reg(instr.dst) << " = " << irop_name(instr.op) << "."
            << ir_type_name(instr.type) << " " << ir_constant_to_string(instr.imm);
        break;

    case IROp::LoadParam:
        out << reg(instr.dst) << " = " << irop_name(instr.op) << "."
            << ir_type_name(instr.type) << " " << instr.src1;
        break;

    case IROp::Copy:
        out << reg(instr.dst) << " = " << irop_name(instr.op) << " " << reg(instr.src1);
        break;

    case IROp::Call:
        if (instr.dst != NO_REG)
            out << reg(instr.dst) << " = ";
        out << "call @" << instr.call_target << "(";
        for (uint32_t i = 0; i < instr.call_argc; ++i) {
            if (i > 0) out << ", ";
            out << reg(instr.src1 + i);
        }
        out << ")";
        break;

    case IROp::Ret:
        out << "ret";
        if (instr.src1 != NO_REG) out << " " << reg(instr.src1);
        break;

    case IROp::Br:
        out << "br bb" << instr.branch_target;
        break;

    case IROp::BrIf:
        out << "br.if " << reg(instr.src1) << ", bb" << instr.branch_target
            << ", bb" << instr.src2;
        break;
    }

    return out.str();
}

std::string dump_block(const BasicBlock& bb) {
    std::ostringstream out;
    out << "  " << bb.label << ":\n";
    for (const auto& instr : bb.instrs) {
        out << dump_instr(instr) << "\n";
    }
    return out.str();
}

std::string dump_function(const IRFunction& fn) {
    std::ostringstream out;
    out << "func @" << fn.name << "(";
    for (size_t i = 0; i < fn.param_names.size(); ++i) {
        if (i > 0) out << ", ";
        out << "%r" << i << ": " << ir_type_name(fn.param_types[i]);
    }
    out << ") -> " << ir_type_name(fn.return_type) << " {\n";
    for (const auto& bb : fn.blocks) {
        out << dump_block(bb);
    }
    out << "}\n";
    return out.str();
}

std::string IRModule::dump() const {
    std::ostringstream out;
    for (size_t i = 0; i < functions.size(); ++i) {
        if (i > 0) out << "\n";
        out << dump_function(functions[i]);
    }
    return out.str();
}

uint32_t IRFunction::add_block(const std::string& label) {
    uint32_t id = static_cast<uint32_t>(blocks.size());
    BasicBlock bb;
    bb.id = id;
    bb.label = label;
    blocks.push_back(std::move(bb));
    return id;
}

void IRFunction::compute_predecessors() {
    for (auto& bb : blocks) bb.predecessors.clear();
    for (const auto& bb : blocks) {
        for (uint32_t succ : bb.successors) {
            if (succ < blocks.size()) {
                blocks[succ].predecessors.push_back(bb.id);
            }
        }
    }
}

} // namespace crashlang
