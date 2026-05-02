#ifndef CRASHLANG_IR_HPP
#define CRASHLANG_IR_HPP

#include <cstdint>
#include <string>
#include <variant>
#include <vector>

namespace crashlang {

enum class IRType { I64, F64, Bool, Str, Void };

const char* ir_type_name(IRType t);

using Reg = uint32_t;
constexpr Reg NO_REG = UINT32_MAX;

enum class IROp {
    IAdd, ISub, IMul, IDiv, IMod,
    FAdd, FSub, FMul, FDiv,
    INeg, FNeg,
    Eq, Neq, Lt, Le, Gt, Ge,
    Not,
    LoadConst,
    LoadParam,
    Copy,
    Call,
    Ret,
    Br,
    BrIf,
};

const char* irop_name(IROp op);

using IRConstant = std::variant<int64_t, double, bool, std::string>;

struct IRInstr {
    IROp        op;
    Reg         dst    = NO_REG;
    Reg         src1   = NO_REG;
    Reg         src2   = NO_REG;
    IRType      type   = IRType::Void;

    IRConstant  imm;              // for LoadConst
    std::string call_target;      // for Call
    uint32_t    call_argc = 0;    // for Call
    uint32_t    branch_target = 0; // basic block index for Br/BrIf
};

struct BasicBlock {
    uint32_t              id;
    std::string           label;
    std::vector<IRInstr>  instrs;
    std::vector<uint32_t> successors;
    std::vector<uint32_t> predecessors;
};

struct IRFunction {
    std::string              name;
    std::vector<std::string> param_names;
    std::vector<IRType>      param_types;
    IRType                   return_type = IRType::Void;
    std::vector<BasicBlock>  blocks;
    uint32_t                 next_reg = 0;

    Reg alloc_reg() { return next_reg++; }
    uint32_t add_block(const std::string& label);
    void compute_predecessors();
};

struct IRModule {
    std::vector<IRFunction> functions;
    std::string dump() const;
};

std::string dump_function(const IRFunction& fn);
std::string dump_block(const BasicBlock& bb);
std::string dump_instr(const IRInstr& instr);
std::string ir_constant_to_string(const IRConstant& c);

} // namespace crashlang

#endif
