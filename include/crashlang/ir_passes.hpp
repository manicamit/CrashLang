#ifndef CRASHLANG_IR_PASSES_HPP
#define CRASHLANG_IR_PASSES_HPP

#include "crashlang/ir.hpp"

#include <set>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace crashlang {

// --- Dead Code Elimination ---

struct DCEResult {
    int instructions_removed = 0;
};

DCEResult dead_code_elimination(IRFunction& fn);

// --- Liveness Analysis ---

struct LivenessInfo {
    std::set<Reg> live_in;
    std::set<Reg> live_out;
};

struct LivenessResult {
    std::unordered_map<uint32_t, LivenessInfo> blocks;  // block id -> info
    std::string dump(const IRFunction& fn) const;
};

LivenessResult compute_liveness(const IRFunction& fn);

// --- Register Allocation (Linear Scan) ---

struct RegAllocResult {
    std::unordered_map<Reg, int> allocation;  // virtual reg -> physical reg
    std::unordered_set<Reg>      spilled;
    int physical_regs_used = 0;
    std::string dump() const;
};

RegAllocResult allocate_registers(const IRFunction& fn, int num_physical_regs = 16);

} // namespace crashlang

#endif
