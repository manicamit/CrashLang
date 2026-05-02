#ifndef CRASHLANG_VM_HPP
#define CRASHLANG_VM_HPP

/// Stack-based virtual machine for CrashLang.
///
/// Executes compiled bytecode from the Compiler. Integrates with the
/// existing HeapSimulator and OwnershipTracker so that memory safety
/// errors produce the same CrashError exceptions consumed by the
/// DiagnosticsEngine.
///
/// Closures capture variables via ObjUpvalue: when a function accesses
/// a variable from an enclosing scope, the compiler emits GetUpvalue/
/// SetUpvalue instructions. Open upvalues point to stack slots; when
/// the enclosing frame returns, CloseUpvalue promotes them to the heap.

#include "crashlang/bytecode.hpp"
#include "crashlang/errors.hpp"
#include "crashlang/heap.hpp"
#include "crashlang/ownership.hpp"
#include "crashlang/source.hpp"
#include "crashlang/value.hpp"

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace crashlang {

/// A captured variable (upvalue). Points either to a live stack slot
/// or to a heap-promoted copy (when the slot goes out of scope).
struct ObjUpvalue {
    /// Index into the VM's value stack (only valid when `closed` is nil/unset).
    size_t stack_index = 0;

    /// When the variable is closed over (stack frame returns), the value
    /// is moved here. After closing, this holds the actual value.
    Value closed;
    bool is_closed = false;

    /// Linked list for the VM's open upvalue chain.
    ObjUpvalue* next = nullptr;
};

/// One frame on the VM call stack.
struct VMFrame {
    const Chunk*  chunk;       // The bytecode being executed.
    size_t        ip = 0;      // Instruction pointer into chunk->code.
    size_t        base = 0;    // Stack base for this frame's locals.
    std::string   name;        // Function name (for diagnostics).

    /// Upvalues captured by this closure invocation.
    std::vector<ObjUpvalue*> upvalues;
};

class VM {
public:
    static constexpr size_t STACK_MAX   = 1024;
    static constexpr size_t FRAMES_MAX  = 256;

    explicit VM(const SourceFile& source);
    ~VM();

    /// Execute a compiled top-level chunk.
    void execute(Chunk& chunk);

    /// Access heap/ownership for diagnostics (same as Interpreter).
    const HeapSimulator&    heap() const { return heap_; }
    const OwnershipTracker& ownership() const { return ownership_; }

private:
    const SourceFile& source_;
    HeapSimulator     heap_;
    OwnershipTracker  ownership_;

    /// Value stack.
    std::vector<Value> stack_;

    /// Call frame stack.
    std::vector<VMFrame> frames_;

    /// Global variables.
    std::unordered_map<std::string, Value> globals_;

    /// Linked list of all open upvalues (sorted by stack_index descending).
    ObjUpvalue* open_upvalues_ = nullptr;

    /// All allocated upvalues (for memory cleanup).
    std::vector<std::unique_ptr<ObjUpvalue>> all_upvalues_;

    // ── Stack operations ───────────────────────────────────────────────────────

    void push(Value value);
    Value pop();
    Value& peek(size_t distance = 0);
    void pop_n(size_t count);

    // ── Upvalue operations ─────────────────────────────────────────────────────

    /// Capture an upvalue pointing at the given stack index.
    ObjUpvalue* capture_upvalue(size_t stack_index);

    /// Close all upvalues pointing at stack slots >= last.
    void close_upvalues(size_t last);

    // ── Dispatch ───────────────────────────────────────────────────────────────

    void run();
    uint8_t read_byte();
    uint16_t read_short();
    const Value& read_constant();
    const std::string& read_string();
    VMFrame& frame() { return frames_.back(); }

    // ── Helpers ────────────────────────────────────────────────────────────────

    CrashError make_error(CrashKind kind, const std::string& detail) const;
    uint32_t current_line() const;
    Span current_span() const;
    void register_builtins();
};

} // namespace crashlang

#endif // CRASHLANG_VM_HPP
