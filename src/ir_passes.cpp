#include "crashlang/ir_passes.hpp"

#include <algorithm>
#include <map>
#include <sstream>

namespace crashlang {

// Collect all registers read by an instruction.
static std::set<Reg> instr_uses(const IRInstr& instr) {
    std::set<Reg> uses;
    switch (instr.op) {
    case IROp::IAdd: case IROp::ISub: case IROp::IMul: case IROp::IDiv: case IROp::IMod:
    case IROp::FAdd: case IROp::FSub: case IROp::FMul: case IROp::FDiv:
    case IROp::Eq: case IROp::Neq: case IROp::Lt: case IROp::Le:
    case IROp::Gt: case IROp::Ge:
        if (instr.src1 != NO_REG) uses.insert(instr.src1);
        if (instr.src2 != NO_REG) uses.insert(instr.src2);
        break;

    case IROp::INeg: case IROp::FNeg: case IROp::Not: case IROp::Copy:
        if (instr.src1 != NO_REG) uses.insert(instr.src1);
        break;

    case IROp::Ret:
        if (instr.src1 != NO_REG) uses.insert(instr.src1);
        break;

    case IROp::BrIf:
        if (instr.src1 != NO_REG) uses.insert(instr.src1);
        break;

    case IROp::Call:
        for (uint32_t i = 0; i < instr.call_argc; ++i) {
            if (instr.src1 + i != NO_REG)
                uses.insert(instr.src1 + i);
        }
        break;

    default:
        break;
    }
    return uses;
}

static Reg instr_def(const IRInstr& instr) {
    switch (instr.op) {
    case IROp::Ret: case IROp::Br: case IROp::BrIf:
        return NO_REG;
    default:
        return instr.dst;
    }
}

// --- Dead Code Elimination ---

DCEResult dead_code_elimination(IRFunction& fn) {
    DCEResult result;
    bool changed = true;

    while (changed) {
        changed = false;

        // Collect all used registers.
        std::unordered_set<Reg> used;
        for (const auto& bb : fn.blocks) {
            for (const auto& instr : bb.instrs) {
                auto u = instr_uses(instr);
                used.insert(u.begin(), u.end());
            }
        }

        // Remove instructions that define unused registers,
        // but never remove terminators or calls (side effects).
        for (auto& bb : fn.blocks) {
            auto it = bb.instrs.begin();
            while (it != bb.instrs.end()) {
                Reg d = instr_def(*it);
                bool is_side_effect = (it->op == IROp::Call || it->op == IROp::Ret ||
                                       it->op == IROp::Br || it->op == IROp::BrIf);
                if (d != NO_REG && !is_side_effect && used.find(d) == used.end()) {
                    it = bb.instrs.erase(it);
                    result.instructions_removed++;
                    changed = true;
                } else {
                    ++it;
                }
            }
        }
    }

    return result;
}

// --- Liveness Analysis (backward dataflow) ---

LivenessResult compute_liveness(const IRFunction& fn) {
    LivenessResult result;

    // Initialize all blocks.
    for (const auto& bb : fn.blocks) {
        result.blocks[bb.id] = LivenessInfo{};
    }

    bool changed = true;
    while (changed) {
        changed = false;

        // Process blocks in reverse order.
        for (int i = static_cast<int>(fn.blocks.size()) - 1; i >= 0; --i) {
            const auto& bb = fn.blocks[static_cast<size_t>(i)];
            auto& info = result.blocks[bb.id];

            // live_out = union of successors' live_in
            std::set<Reg> new_out;
            for (uint32_t succ : bb.successors) {
                const auto& succ_info = result.blocks[succ];
                new_out.insert(succ_info.live_in.begin(), succ_info.live_in.end());
            }

            // live_in = uses(block) ∪ (live_out - defs(block))
            std::set<Reg> new_in = new_out;

            // Walk instructions backward.
            for (int j = static_cast<int>(bb.instrs.size()) - 1; j >= 0; --j) {
                const auto& instr = bb.instrs[static_cast<size_t>(j)];
                Reg d = instr_def(instr);
                if (d != NO_REG) new_in.erase(d);
                auto u = instr_uses(instr);
                new_in.insert(u.begin(), u.end());
            }

            if (new_in != info.live_in || new_out != info.live_out) {
                info.live_in = std::move(new_in);
                info.live_out = std::move(new_out);
                changed = true;
            }
        }
    }

    return result;
}

std::string LivenessResult::dump(const IRFunction& fn) const {
    std::ostringstream out;
    out << "Liveness for @" << fn.name << ":\n";
    for (const auto& bb : fn.blocks) {
        auto it = blocks.find(bb.id);
        if (it == blocks.end()) continue;
        out << "  " << bb.label << ":\n";
        out << "    live_in:  {";
        bool first = true;
        for (Reg r : it->second.live_in) {
            if (!first) out << ", ";
            out << "%r" << r;
            first = false;
        }
        out << "}\n    live_out: {";
        first = true;
        for (Reg r : it->second.live_out) {
            if (!first) out << ", ";
            out << "%r" << r;
            first = false;
        }
        out << "}\n";
    }
    return out.str();
}

// --- Register Allocation (Linear Scan) ---

struct LiveInterval {
    Reg      vreg;
    uint32_t start;
    uint32_t end;
};

RegAllocResult allocate_registers(const IRFunction& fn, int num_physical_regs) {
    RegAllocResult result;

    // Flatten all instructions to compute live intervals.
    // Assign a global position to each instruction.
    std::unordered_map<Reg, LiveInterval> intervals;

    uint32_t pos = 0;
    for (const auto& bb : fn.blocks) {
        for (const auto& instr : bb.instrs) {
            // Record definition point.
            Reg d = instr_def(instr);
            if (d != NO_REG) {
                if (intervals.find(d) == intervals.end()) {
                    intervals[d] = {d, pos, pos};
                }
                intervals[d].start = std::min(intervals[d].start, pos);
                intervals[d].end   = std::max(intervals[d].end, pos);
            }

            // Record use points.
            auto uses = instr_uses(instr);
            for (Reg u : uses) {
                if (intervals.find(u) == intervals.end()) {
                    intervals[u] = {u, pos, pos};
                }
                intervals[u].end = std::max(intervals[u].end, pos);
            }

            ++pos;
        }
    }

    // Sort intervals by start position.
    std::vector<LiveInterval> sorted;
    sorted.reserve(intervals.size());
    for (auto& [_, iv] : intervals) {
        sorted.push_back(iv);
    }
    std::sort(sorted.begin(), sorted.end(),
              [](const LiveInterval& a, const LiveInterval& b) {
                  return a.start < b.start;
              });

    // Linear scan.
    std::vector<bool> phys_free(static_cast<size_t>(num_physical_regs), true);
    // Active intervals, sorted by end point.
    std::vector<LiveInterval> active;

    auto expire_old = [&](uint32_t current_start) {
        auto it = active.begin();
        while (it != active.end()) {
            if (it->end < current_start) {
                result.allocation.count(it->vreg);
                int preg = result.allocation[it->vreg];
                phys_free[static_cast<size_t>(preg)] = true;
                it = active.erase(it);
            } else {
                ++it;
            }
        }
    };

    for (auto& iv : sorted) {
        expire_old(iv.start);

        // Find a free physical register.
        int chosen = -1;
        for (int i = 0; i < num_physical_regs; ++i) {
            if (phys_free[static_cast<size_t>(i)]) {
                chosen = i;
                break;
            }
        }

        if (chosen >= 0) {
            result.allocation[iv.vreg] = chosen;
            phys_free[static_cast<size_t>(chosen)] = false;
            active.push_back(iv);
            result.physical_regs_used = std::max(result.physical_regs_used, chosen + 1);
        } else {
            // Spill the interval with the furthest end point.
            auto longest = std::max_element(active.begin(), active.end(),
                [](const LiveInterval& a, const LiveInterval& b) {
                    return a.end < b.end;
                });

            if (longest != active.end() && longest->end > iv.end) {
                // Spill longest, give its register to iv.
                int preg = result.allocation[longest->vreg];
                result.spilled.insert(longest->vreg);
                result.allocation.erase(longest->vreg);
                result.allocation[iv.vreg] = preg;
                active.erase(longest);
                active.push_back(iv);
            } else {
                result.spilled.insert(iv.vreg);
            }
        }
    }

    return result;
}

std::string RegAllocResult::dump() const {
    std::ostringstream out;
    out << "Register allocation (" << physical_regs_used << " physical regs used):\n";

    // Sort by virtual register for deterministic output.
    std::map<Reg, int> sorted(allocation.begin(), allocation.end());
    for (auto& [vreg, preg] : sorted) {
        out << "  %r" << vreg << " -> %p" << preg << "\n";
    }
    if (!spilled.empty()) {
        out << "  spilled: {";
        bool first = true;
        for (Reg r : spilled) {
            if (!first) out << ", ";
            out << "%r" << r;
            first = false;
        }
        out << "}\n";
    }
    return out.str();
}

} // namespace crashlang
