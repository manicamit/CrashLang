#ifndef CRASHLANG_OWNERSHIP_HPP
#define CRASHLANG_OWNERSHIP_HPP

#include "crashlang/common.hpp"
#include "crashlang/source.hpp"

#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace crashlang {

struct OwnershipEvent {
    enum Kind {
        Allocated, Freed, Moved, Referenced,
        Dereferenced, FieldRead, FieldWrite, PassedToFunction,
    };

    Kind        kind;
    Span        location;
    std::string description;
    std::string function_name;
};

const char* ownership_event_kind_name(OwnershipEvent::Kind kind);

struct OwnershipRecord {
    HeapID      id;
    std::string owner;
    std::string type_name;

    bool                         moved = false;
    std::optional<std::string>   moved_to;
    std::optional<Span>          moved_at;

    std::vector<OwnershipEvent>  timeline;
};

class OwnershipTracker {
public:
    OwnershipTracker() = default;

    void record_allocation(HeapID id, std::string var_name, std::string type_name,
                           Span location, std::string function_name);
    void record_free(HeapID id, Span location, std::string function_name);
    void record_move(HeapID id, std::string from_var, std::string to_var,
                     Span location, std::string function_name);
    void record_reference(HeapID id, std::string ref_var,
                          Span location, std::string function_name);
    void record_dereference(HeapID id, Span location, std::string function_name);
    void record_field_access(HeapID id, const std::string& field, bool is_write,
                             Span location, std::string function_name);
    void record_pass_to_function(HeapID id, const std::string& callee_name,
                                 Span location, std::string function_name);

    const std::vector<OwnershipEvent>& get_timeline(HeapID id) const;
    std::string get_owner(HeapID id) const;
    bool is_moved(HeapID id) const;
    bool has_record(HeapID id) const;
    const OwnershipRecord& get_record(HeapID id) const;

    std::optional<HeapID> resolve_variable(const std::string& var_name) const;
    void bind_variable(const std::string& var_name, HeapID id);
    void set_owner(HeapID id, const std::string& var_name);
    void unbind_variable(const std::string& var_name);

private:
    std::unordered_map<HeapID, OwnershipRecord> records_;
    std::unordered_map<std::string, HeapID> var_to_heap_;
    static const std::vector<OwnershipEvent> empty_timeline_;
};

} // namespace crashlang

#endif
