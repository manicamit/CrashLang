#ifndef CRASHLANG_HEAP_HPP
#define CRASHLANG_HEAP_HPP

/// Simulated heap for CrashLang memory modeling.
///
/// Tracks all allocations and their lifetimes. When an object is freed,
/// it is NOT removed from the map — it's marked dead so that subsequent
/// accesses produce accurate diagnostics (UseAfterFree, DoubleFree).

#include "crashlang/common.hpp"
#include "crashlang/source.hpp"
#include "crashlang/value.hpp"

#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace crashlang {

// ── HeapObject ─────────────────────────────────────────────────────────────────

struct HeapObject {
    HeapID                                 id;
    std::string                            type_name;  // e.g., "Buffer", "Point"
    std::unordered_map<std::string, Value> fields;

    bool alive = true;

    // Metadata for diagnostics.
    Span                       allocation_site;
    std::optional<Span>        deallocation_site;
    std::string                allocated_in_function;
    std::optional<std::string> freed_in_function;
};

// ── HeapSimulator ──────────────────────────────────────────────────────────────

class HeapSimulator {
public:
    HeapSimulator() = default;

    /// Allocate a new object on the simulated heap. Returns its HeapID.
    HeapID allocate(std::string type_name,
                    std::unordered_map<std::string, Value> fields,
                    Span site,
                    std::string function_name);

    /// Free an object. Throws DoubleFree if already dead.
    void free(HeapID id, Span site, std::string function_name);

    /// Read a field. Throws UseAfterFree or InvalidFieldAccess.
    Value read_field(HeapID id, const std::string& field, Span site) const;

    /// Write a field. Throws UseAfterFree or InvalidFieldAccess.
    void write_field(HeapID id, const std::string& field, Value value, Span site);

    /// Is the object alive?
    bool is_alive(HeapID id) const;

    /// Does this HeapID exist at all?
    bool exists(HeapID id) const;

    /// Get read-only reference to an object (for diagnostics).
    const HeapObject& get_object(HeapID id) const;

    /// Return all objects still alive at program exit (potential leaks).
    std::vector<const HeapObject*> get_all_leaked() const;

    /// Total number of allocations made (for heap stats).
    size_t total_allocations() const { return next_id_ - 1; }

private:
    std::unordered_map<HeapID, HeapObject> objects_;
    HeapID                                 next_id_ = 1;

    /// Throw a UseAfterFree CrashError with full metadata.
    [[noreturn]] void throw_use_after_free(HeapID id,
                                            Span site,
                                            const std::string& action) const;
};

} // namespace crashlang

#endif // CRASHLANG_HEAP_HPP
