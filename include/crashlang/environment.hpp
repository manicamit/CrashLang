#ifndef CRASHLANG_ENVIRONMENT_HPP
#define CRASHLANG_ENVIRONMENT_HPP

#include "crashlang/value.hpp"

#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace crashlang {

class Scope {
public:
    explicit Scope(std::shared_ptr<Scope> parent = nullptr, std::string name = "<block>")
        : parent_(std::move(parent))
        , name_(std::move(name))
    {}

    void define(const std::string& name, Value value);
    std::optional<Value> get(const std::string& name) const;
    bool assign(const std::string& name, Value value);
    bool has(const std::string& name) const;
    bool has_local(const std::string& name) const;

    // Levenshtein-based suggestion for "did you mean?" errors.
    std::optional<std::string> find_closest(const std::string& name) const;
    std::vector<std::string> all_names() const;

    const std::string& name() const { return name_; }
    const std::shared_ptr<Scope>& parent() const { return parent_; }

private:
    std::unordered_map<std::string, Value> bindings_;
    std::shared_ptr<Scope>                 parent_;
    std::string                            name_;
};

class Environment {
public:
    Environment();

    std::shared_ptr<Scope> current() const { return current_; }
    void push_scope(std::string name = "<block>");
    void push_existing_scope(std::shared_ptr<Scope> scope);
    void pop_scope();

    void define(const std::string& name, Value value);
    std::optional<Value> get(const std::string& name) const;
    bool assign(const std::string& name, Value value);
    std::optional<std::string> find_closest(const std::string& name) const;
    size_t depth() const;

private:
    std::shared_ptr<Scope> current_;
};

} // namespace crashlang

#endif
