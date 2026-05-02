#include "crashlang/codegen.hpp"

#include <sstream>

namespace crashlang {

static std::string preg(Reg vreg, const RegAllocResult& alloc) {
    auto it = alloc.allocation.find(vreg);
    if (it != alloc.allocation.end()) {
        return "%p" + std::to_string(it->second);
    }
    if (alloc.spilled.count(vreg)) {
        return "[spill_%r" + std::to_string(vreg) + "]";
    }
    return "%r" + std::to_string(vreg);
}

static std::string type_suffix(IRType t) {
    switch (t) {
    case IRType::I64:  return ".s64";
    case IRType::F64:  return ".f64";
    case IRType::Bool: return ".pred";
    case IRType::Str:  return ".str";
    case IRType::Void: return "";
    }
    return "";
}

std::string emit_function_asm(const IRFunction& fn, const RegAllocResult& alloc) {
    std::ostringstream out;

    out << ".func " << fn.name << "(";
    for (size_t i = 0; i < fn.param_names.size(); ++i) {
        if (i > 0) out << ", ";
        out << type_suffix(fn.param_types[i]) << " " << preg(static_cast<Reg>(i), alloc);
    }
    out << ") -> " << type_suffix(fn.return_type) << " {\n";

    // Declare physical registers used.
    if (alloc.physical_regs_used > 0) {
        out << "    .reg .s64 %p<" << alloc.physical_regs_used << ">;\n";
    }
    out << "\n";

    for (const auto& bb : fn.blocks) {
        out << bb.label << ":\n";

        for (const auto& instr : bb.instrs) {
            out << "    ";

            switch (instr.op) {
            case IROp::IAdd: case IROp::ISub: case IROp::IMul:
            case IROp::IDiv: case IROp::IMod:
            case IROp::FAdd: case IROp::FSub: case IROp::FMul: case IROp::FDiv:
            case IROp::Eq: case IROp::Neq: case IROp::Lt: case IROp::Le:
            case IROp::Gt: case IROp::Ge:
                out << irop_name(instr.op) << " "
                    << preg(instr.dst, alloc) << ", "
                    << preg(instr.src1, alloc) << ", "
                    << preg(instr.src2, alloc) << ";";
                break;

            case IROp::INeg: case IROp::FNeg: case IROp::Not:
                out << irop_name(instr.op) << " "
                    << preg(instr.dst, alloc) << ", "
                    << preg(instr.src1, alloc) << ";";
                break;

            case IROp::LoadConst:
                out << "mov" << type_suffix(instr.type) << " "
                    << preg(instr.dst, alloc) << ", "
                    << ir_constant_to_string(instr.imm) << ";";
                break;

            case IROp::LoadParam:
                out << "load.arg" << type_suffix(instr.type) << " "
                    << preg(instr.dst, alloc) << ", [arg"
                    << instr.src1 << "];";
                break;

            case IROp::Copy:
                out << "mov " << preg(instr.dst, alloc) << ", "
                    << preg(instr.src1, alloc) << ";";
                break;

            case IROp::Call:
                if (instr.dst != NO_REG)
                    out << preg(instr.dst, alloc) << " = ";
                out << "call @" << instr.call_target << "(";
                for (uint32_t i = 0; i < instr.call_argc; ++i) {
                    if (i > 0) out << ", ";
                    out << preg(instr.src1 + i, alloc);
                }
                out << ");";
                break;

            case IROp::Ret:
                if (instr.src1 != NO_REG) {
                    out << "ret " << preg(instr.src1, alloc) << ";";
                } else {
                    out << "ret;";
                }
                break;

            case IROp::Br:
                out << "br " << fn.blocks[instr.branch_target].label << ";";
                break;

            case IROp::BrIf:
                out << "br.if " << preg(instr.src1, alloc) << ", "
                    << fn.blocks[instr.branch_target].label << ", "
                    << fn.blocks[instr.src2].label << ";";
                break;
            }

            out << "\n";
        }
    }

    out << "}\n";
    return out.str();
}

std::string emit_assembly(const IRModule& module) {
    std::ostringstream out;
    out << "; CrashLang assembly output\n";
    out << "; target: register-based ISA\n\n";

    for (const auto& fn : module.functions) {
        auto alloc = allocate_registers(fn);
        out << emit_function_asm(fn, alloc);
        out << "\n";
    }

    return out.str();
}

} // namespace crashlang
