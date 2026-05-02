#ifndef CRASHLANG_VM_HPP
#define CRASHLANG_VM_HPP

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

// Captured variable. Points to a live stack slot or a heap-promoted copy.
struct ObjUpvalue {
    size_t stack_index = 0;
    Value  closed;
    bool   is_closed = false;
    ObjUpvalue* next = nullptr;
};

struct VMFrame {
    const Chunk*  chunk;
    size_t        ip = 0;
    size_t        base = 0;
    std::string   name;
    std::vector<ObjUpvalue*> upvalues;
};

class VM {
public:
    static constexpr size_t STACK_MAX   = 1024;
    static constexpr size_t FRAMES_MAX  = 256;

    explicit VM(const SourceFile& source);
    ~VM();

    void execute(Chunk& chunk);

    const HeapSimulator&    heap() const { return heap_; }
    const OwnershipTracker& ownership() const { return ownership_; }

private:
    const SourceFile& source_;
    HeapSimulator     heap_;
    OwnershipTracker  ownership_;

    std::vector<Value>   stack_;
    std::vector<VMFrame> frames_;
    std::unordered_map<std::string, Value> globals_;

    // Open upvalue chain, sorted by stack index (descending).
    ObjUpvalue* open_upvalues_ = nullptr;
    std::vector<std::unique_ptr<ObjUpvalue>> all_upvalues_;

    void push(Value value);
    Value pop();
    Value& peek(size_t distance = 0);
    void pop_n(size_t count);

    ObjUpvalue* capture_upvalue(size_t stack_index);
    void close_upvalues(size_t last);

    void run();
    uint8_t read_byte();
    uint16_t read_short();
    const Value& read_constant();
    const std::string& read_string();
    VMFrame& frame() { return frames_.back(); }

    CrashError make_error(CrashKind kind, const std::string& detail) const;
    uint32_t current_line() const;
    Span current_span() const;
    void register_builtins();
};

} // namespace crashlang

#endif
