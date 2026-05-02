#include "crashlang/heap.hpp"
#include "crashlang/errors.hpp"

namespace crashlang {
HeapID HeapSimulator::allocate(std::string type_name,
                                std::unordered_map<std::string, Value> fields,
                                Span site,
                                std::string function_name)
{
    HeapID id = next_id_++;

    HeapObject obj;
    obj.id                    = id;
    obj.type_name             = std::move(type_name);
    obj.fields                = std::move(fields);
    obj.alive                 = true;
    obj.allocation_site       = site;
    obj.allocated_in_function = std::move(function_name);

    objects_.emplace(id, std::move(obj));
    return id;
}
void HeapSimulator::free(HeapID id, Span site, std::string function_name) {
    auto it = objects_.find(id);
    if (it == objects_.end()) {
        // Freeing something that was never allocated. Treat as an invalid op.
        CrashError err(CrashKind::InvalidOperation, site,
            "free() called on an invalid heap reference (heap#"
            + std::to_string(id) + ")");
        throw err;
    }

    auto& obj = it->second;
    if (!obj.alive) {
        // Double free. Build a rich error with metadata.
        CrashError err(CrashKind::DoubleFree, site,
            "'" + obj.type_name + "' (heap#" + std::to_string(id)
            + ") was already freed");

        err.related_object = id;

        if (obj.deallocation_site) {
            err.metadata["first_free_location"] = obj.deallocation_site->to_string();
        }
        if (obj.freed_in_function) {
            err.metadata["first_free_function"] = *obj.freed_in_function;
        }
        err.metadata["type"] = obj.type_name;
        err.metadata["allocation_site"] = obj.allocation_site.to_string();

        throw err;
    }

    obj.alive              = false;
    obj.deallocation_site  = site;
    obj.freed_in_function  = std::move(function_name);
}
Value HeapSimulator::read_field(HeapID id, const std::string& field, Span site) const {
    auto it = objects_.find(id);
    if (it == objects_.end()) {
        CrashError err(CrashKind::InvalidOperation, site,
            "read on invalid heap reference (heap#" + std::to_string(id) + ")");
        throw err;
    }

    const auto& obj = it->second;
    if (!obj.alive) {
        throw_use_after_free(id, site, "read field '" + field + "'");
    }

    auto field_it = obj.fields.find(field);
    if (field_it == obj.fields.end()) {
        CrashError err(CrashKind::InvalidFieldAccess, site,
            "'" + obj.type_name + "' has no field '" + field + "'");
        err.related_object = id;
        err.metadata["type"] = obj.type_name;
        err.metadata["field"] = field;

        // Collect actual field names for "did you mean?" style suggestions.
        std::string available;
        for (const auto& [k, _] : obj.fields) {
            if (!available.empty()) available += ", ";
            available += k;
        }
        err.metadata["available_fields"] = available;

        throw err;
    }

    return field_it->second;
}
void HeapSimulator::write_field(HeapID id, const std::string& field, Value value, Span site) {
    auto it = objects_.find(id);
    if (it == objects_.end()) {
        CrashError err(CrashKind::InvalidOperation, site,
            "write on invalid heap reference (heap#" + std::to_string(id) + ")");
        throw err;
    }

    auto& obj = it->second;
    if (!obj.alive) {
        throw_use_after_free(id, site, "write field '" + field + "'");
    }

    auto field_it = obj.fields.find(field);
    if (field_it == obj.fields.end()) {
        CrashError err(CrashKind::InvalidFieldAccess, site,
            "'" + obj.type_name + "' has no field '" + field + "'");
        err.related_object = id;
        err.metadata["type"] = obj.type_name;
        err.metadata["field"] = field;
        throw err;
    }

    field_it->second = std::move(value);
}
bool HeapSimulator::is_alive(HeapID id) const {
    auto it = objects_.find(id);
    return it != objects_.end() && it->second.alive;
}

bool HeapSimulator::exists(HeapID id) const {
    return objects_.count(id) > 0;
}
const HeapObject& HeapSimulator::get_object(HeapID id) const {
    return objects_.at(id);
}
std::vector<const HeapObject*> HeapSimulator::get_all_leaked() const {
    std::vector<const HeapObject*> leaks;
    for (const auto& [_, obj] : objects_) {
        if (obj.alive) {
            leaks.push_back(&obj);
        }
    }
    return leaks;
}
void HeapSimulator::throw_use_after_free(HeapID id,
                                          Span site,
                                          const std::string& action) const
{
    const auto& obj = objects_.at(id);

    CrashError err(CrashKind::UseAfterFree, site,
        "tried to " + action + " on '" + obj.type_name
        + "' (heap#" + std::to_string(id) + "), but it was already freed");

    err.related_object = id;
    err.metadata["type"]            = obj.type_name;
    err.metadata["action"]          = action;
    err.metadata["allocation_site"] = obj.allocation_site.to_string();

    if (obj.deallocation_site) {
        err.metadata["free_site"] = obj.deallocation_site->to_string();
    }
    if (obj.freed_in_function) {
        err.metadata["freed_in"] = *obj.freed_in_function;
    }

    throw err;
}

} // namespace crashlang
