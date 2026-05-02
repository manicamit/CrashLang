#ifndef CRASHLANG_HEAP_HPP
#define CRASHLANG_HEAP_HPP

#include "crashlang/common.hpp"
#include "crashlang/source.hpp"
#include "crashlang/value.hpp"

#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace crashlang {

struct HeapObject {
    HeapID                                 id;
    std::string                            type_name;
    std::unordered_map<std::string, Value> fields;
    bool alive = true;

    Span                       allocation_site;
    std::optional<Span>        deallocation_site;
    std::string                allocated_in_function;
    std::optional<std::string> freed_in_function;
};

class HeapSimulator {
public:
    HeapSimulator() = default;

    HeapID allocate(std::string type_name,
                    std::unordered_map<std::string, Value> fields,
                    Span site, std::string function_name);
    void free(HeapID id, Span site, std::string function_name);
    Value read_field(HeapID id, const std::string& field, Span site) const;
    void write_field(HeapID id, const std::string& field, Value value, Span site);

    bool is_alive(HeapID id) const;
    bool exists(HeapID id) const;
    const HeapObject& get_object(HeapID id) const;
    std::vector<const HeapObject*> get_all_leaked() const;
    size_t total_allocations() const { return next_id_ - 1; }

private:
    std::unordered_map<HeapID, HeapObject> objects_;
    HeapID                                 next_id_ = 1;

    [[noreturn]] void throw_use_after_free(HeapID id, Span site,
                                            const std::string& action) const;
};

} // namespace crashlang

#endif
