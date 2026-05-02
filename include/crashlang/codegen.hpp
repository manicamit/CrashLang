#ifndef CRASHLANG_CODEGEN_HPP
#define CRASHLANG_CODEGEN_HPP

#include "crashlang/ir.hpp"
#include "crashlang/ir_passes.hpp"

#include <string>

namespace crashlang {

// Emit register-based assembly from IR, using physical register allocation.
std::string emit_assembly(const IRModule& module);
std::string emit_function_asm(const IRFunction& fn, const RegAllocResult& alloc);

} // namespace crashlang

#endif
