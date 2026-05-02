#ifndef CRASHLANG_OWNERSHIP_HPP
#define CRASHLANG_OWNERSHIP_HPP

/// Ownership tracking for CrashLang's memory model.
///
/// Records every significant event in the lifetime of a heap object:
/// allocation, free, move, ref, deref, field access, function pass.
/// This timeline is what powers the "Object lifetime" section of
/// crash reports — the feature that makes CrashLang unique.

#include "crashlang/common.hpp"
#include "crashlang/source.hpp"

#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace crashlang {

// ── OwnershipEvent ─────────────────────────────────────────────────────────────

struct OwnershipEvent {
    enum Kind {
        Allocated,        // new Typename { ... }
        Freed,            // free(x)
        Moved,            // let y = move(x)
        Referenced,       // let r = ref(x)
        Dereferenced,     // let v = deref(r)
        FieldRead,        // x.field
        FieldWrite,       // x.field = val
        PassedToFunction, // foo(x)
    };

    Kind        kind;
    Span        location;
    std::string description;     // "buf allocated as Buffer"
    std::string function_name;   // Which function this happened in.
};

/// Human-readable name for an event kind.
const char* ownership_event_kind_name(OwnershipEvent::Kind kind);

// ── OwnershipRecord ────────────────────────────────────────────────────────────

/// The full lifetime record for a single heap object.
struct OwnershipRecord {
    HeapID      id;
    std::string owner;           // Current owner variable name.
    std::string type_name;       // The type at allocation.

    bool                         moved = false;
    std::optional<std::string>   moved_to;     // Destination variable.
    std::optional<Span>          moved_at;     // Where the move happened.

    /// Chronological event log.
    std::vector<OwnershipEvent>  timeline;
};

// ── OwnershipTracker ───────────────────────────────────────────────────────────

class OwnershipTracker {
public:
    OwnershipTracker() = default;

    /// Record a new allocation.
    void record_allocation(HeapID id,
                           std::string var_name,
                           std::string type_name,
                           Span location,
                           std::string function_name);

    /// Record a free.
    void record_free(HeapID id, Span location, std::string function_name);

    /// Record a move: ownership transfers from `from_var` to `to_var`.
    /// Marks the source variable's record as moved.
    void record_move(HeapID id,
                     std::string from_var,
                     std::string to_var,
                     Span location,
                     std::string function_name);

    /// Record a ref() operation.
    void record_reference(HeapID id,
                          std::string ref_var,
                          Span location,
                          std::string function_name);

    /// Record a deref() operation.
    void record_dereference(HeapID id,
                            Span location,
                            std::string function_name);

    /// Record a field read or write.
    void record_field_access(HeapID id,
                             const std::string& field,
                             bool is_write,
                             Span location,
                             std::string function_name);

    /// Record that the value was passed into a function call.
    void record_pass_to_function(HeapID id,
                                 const std::string& callee_name,
                                 Span location,
                                 std::string function_name);

    /// Get the full timeline for an object.
    const std::vector<OwnershipEvent>& get_timeline(HeapID id) const;

    /// Get the current owner variable name.
    std::string get_owner(HeapID id) const;

    /// Is this object's ownership moved away?
    bool is_moved(HeapID id) const;

    /// Check if a record exists for this ID.
    bool has_record(HeapID id) const;

    /// Get the full record (for diagnostics).
    const OwnershipRecord& get_record(HeapID id) const;

    /// Resolve which HeapID a variable currently owns (if any).
    std::optional<HeapID> resolve_variable(const std::string& var_name) const;

    /// Register a variable → HeapID mapping.
    void bind_variable(const std::string& var_name, HeapID id);

    /// Retroactively set the owner name (resolves the <pending> placeholder
    /// set during allocation, before the let binding is known).
    void set_owner(HeapID id, const std::string& var_name);

    /// Remove a variable binding (when a variable goes out of scope or is moved).
    void unbind_variable(const std::string& var_name);

private:
    std::unordered_map<HeapID, OwnershipRecord> records_;

    /// Maps variable name → HeapID for quick ownership lookups.
    std::unordered_map<std::string, HeapID> var_to_heap_;

    /// Empty timeline sentinel for missing records.
    static const std::vector<OwnershipEvent> empty_timeline_;
};

} // namespace crashlang

#endif // CRASHLANG_OWNERSHIP_HPP
