#include "crashlang/ownership.hpp"

namespace crashlang {
const std::vector<OwnershipEvent> OwnershipTracker::empty_timeline_;
const char* ownership_event_kind_name(OwnershipEvent::Kind kind) {
    switch (kind) {
        case OwnershipEvent::Allocated:        return "new()";
        case OwnershipEvent::Freed:            return "free()";
        case OwnershipEvent::Moved:            return "move()";
        case OwnershipEvent::Referenced:       return "ref()";
        case OwnershipEvent::Dereferenced:     return "deref()";
        case OwnershipEvent::FieldRead:        return "READ";
        case OwnershipEvent::FieldWrite:       return "WRITE";
        case OwnershipEvent::PassedToFunction: return "call";
    }
    return "???";
}
void OwnershipTracker::record_allocation(HeapID id,
                                          std::string var_name,
                                          std::string type_name,
                                          Span location,
                                          std::string function_name)
{
    OwnershipRecord record;
    record.id        = id;
    record.owner     = var_name;
    record.type_name = type_name;

    OwnershipEvent event;
    event.kind          = OwnershipEvent::Allocated;
    event.location      = location;
    event.description   = var_name + " allocated as " + type_name;
    event.function_name = std::move(function_name);
    record.timeline.push_back(std::move(event));

    records_.emplace(id, std::move(record));
    var_to_heap_[var_name] = id;
}
void OwnershipTracker::record_free(HeapID id,
                                    Span location,
                                    std::string function_name)
{
    auto it = records_.find(id);
    if (it == records_.end()) return;

    OwnershipEvent event;
    event.kind          = OwnershipEvent::Freed;
    event.location      = location;
    event.description   = it->second.owner + " freed";
    event.function_name = std::move(function_name);
    it->second.timeline.push_back(std::move(event));
}
void OwnershipTracker::record_move(HeapID id,
                                    std::string from_var,
                                    std::string to_var,
                                    Span location,
                                    std::string function_name)
{
    auto it = records_.find(id);
    if (it == records_.end()) return;

    auto& rec = it->second;

    OwnershipEvent event;
    event.kind          = OwnershipEvent::Moved;
    event.location      = location;
    event.description   = "ownership moved from " + from_var + " to " + to_var;
    event.function_name = std::move(function_name);
    rec.timeline.push_back(std::move(event));

    rec.moved    = true;
    rec.moved_to = to_var;
    rec.moved_at = location;
    rec.owner    = to_var;

    // Update variable bindings.
    var_to_heap_.erase(from_var);
    var_to_heap_[to_var] = id;
}
void OwnershipTracker::record_reference(HeapID id,
                                         std::string ref_var,
                                         Span location,
                                         std::string function_name)
{
    auto it = records_.find(id);
    if (it == records_.end()) return;

    OwnershipEvent event;
    event.kind          = OwnershipEvent::Referenced;
    event.location      = location;
    event.description   = ref_var + " = ref(" + it->second.owner + ")";
    event.function_name = std::move(function_name);
    it->second.timeline.push_back(std::move(event));
}
void OwnershipTracker::record_dereference(HeapID id,
                                           Span location,
                                           std::string function_name)
{
    auto it = records_.find(id);
    if (it == records_.end()) return;

    OwnershipEvent event;
    event.kind          = OwnershipEvent::Dereferenced;
    event.location      = location;
    event.description   = "deref(heap#" + std::to_string(id) + ")";
    event.function_name = std::move(function_name);
    it->second.timeline.push_back(std::move(event));
}
void OwnershipTracker::record_field_access(HeapID id,
                                            const std::string& field,
                                            bool is_write,
                                            Span location,
                                            std::string function_name)
{
    auto it = records_.find(id);
    if (it == records_.end()) return;

    OwnershipEvent event;
    event.kind          = is_write ? OwnershipEvent::FieldWrite : OwnershipEvent::FieldRead;
    event.location      = location;
    event.description   = it->second.owner + "." + field
                        + (is_write ? " written" : " read");
    event.function_name = std::move(function_name);
    it->second.timeline.push_back(std::move(event));
}
void OwnershipTracker::record_pass_to_function(HeapID id,
                                                const std::string& callee_name,
                                                Span location,
                                                std::string function_name)
{
    auto it = records_.find(id);
    if (it == records_.end()) return;

    OwnershipEvent event;
    event.kind          = OwnershipEvent::PassedToFunction;
    event.location      = location;
    event.description   = it->second.owner + " passed into " + callee_name + "()";
    event.function_name = std::move(function_name);
    it->second.timeline.push_back(std::move(event));
}
const std::vector<OwnershipEvent>& OwnershipTracker::get_timeline(HeapID id) const {
    auto it = records_.find(id);
    if (it == records_.end()) return empty_timeline_;
    return it->second.timeline;
}

std::string OwnershipTracker::get_owner(HeapID id) const {
    auto it = records_.find(id);
    if (it == records_.end()) return "<unknown>";
    return it->second.owner;
}

bool OwnershipTracker::is_moved(HeapID id) const {
    auto it = records_.find(id);
    return it != records_.end() && it->second.moved;
}

bool OwnershipTracker::has_record(HeapID id) const {
    return records_.count(id) > 0;
}

const OwnershipRecord& OwnershipTracker::get_record(HeapID id) const {
    return records_.at(id);
}

std::optional<HeapID> OwnershipTracker::resolve_variable(const std::string& var_name) const {
    auto it = var_to_heap_.find(var_name);
    if (it == var_to_heap_.end()) return std::nullopt;
    return it->second;
}

void OwnershipTracker::bind_variable(const std::string& var_name, HeapID id) {
    var_to_heap_[var_name] = id;
}

void OwnershipTracker::set_owner(HeapID id, const std::string& var_name) {
    auto it = records_.find(id);
    if (it == records_.end()) return;

    auto& rec = it->second;
    std::string old_name = rec.owner;
    rec.owner = var_name;

    // Rewrite all timeline descriptions that used the old name.
    for (auto& event : rec.timeline) {
        size_t pos = 0;
        while ((pos = event.description.find(old_name, pos)) != std::string::npos) {
            event.description.replace(pos, old_name.size(), var_name);
            pos += var_name.size();
        }
    }

    // Update the variable binding map.
    var_to_heap_.erase(old_name);
    var_to_heap_[var_name] = id;
}

void OwnershipTracker::unbind_variable(const std::string& var_name) {
    var_to_heap_.erase(var_name);
}

} // namespace crashlang
