#ifndef CRASHLANG_ENVIRONMENT_HPP
#define CRASHLANG_ENVIRONMENT_HPP

/// Scope chain for CrashLang's lexical scoping.
///
/// Each Scope holds one level of variable bindings and a pointer to its
/// enclosing scope. Variable lookup walks the chain outward until it finds
/// the name or exhausts the chain.
///
/// Scopes are reference-counted so function closures can safely capture
/// their enclosing environment even after the creating scope is exited.

#include "crashlang/value.hpp"

#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace crashlang {

// ── Scope ──────────────────────────────────────────────────────────────────────

class Scope {
public:
    explicit Scope(std::shared_ptr<Scope> parent = nullptr, std::string name = "<block>")
        : parent_(std::move(parent))
        , name_(std::move(name))
    {}

    /// Define a new variable in this scope. Always succeeds (shadows any outer binding).
    void define(const std::string& name, Value value);

    /// Look up a variable, walking outward through parent scopes.
    /// Returns the Value if found, nullopt otherwise.
    std::optional<Value> get(const std::string& name) const;

    /// Assign to an existing variable. Walks the chain to find it.
    /// Returns true if the variable was found and updated.
    bool assign(const std::string& name, Value value);

    /// Check if a name is defined anywhere in the chain.
    bool has(const std::string& name) const;

    /// Check if a name is defined in this scope only (not parents).
    bool has_local(const std::string& name) const;

    /// Find the closest variable name to `name` using Levenshtein distance.
    /// Used for "did you mean X?" suggestions in UndefinedVariable errors.
    /// Returns nullopt if no close match found (threshold: distance <= 3).
    std::optional<std::string> find_closest(const std::string& name) const;

    /// Collect all variable names visible from this scope (for suggestions).
    std::vector<std::string> all_names() const;

    /// The human-readable name of this scope (function name or "<block>").
    const std::string& name() const { return name_; }

    /// The parent scope, or nullptr if this is the global scope.
    const std::shared_ptr<Scope>& parent() const { return parent_; }

private:
    std::unordered_map<std::string, Value> bindings_;
    std::shared_ptr<Scope>                 parent_;
    std::string                            name_;
};

// ── Environment ────────────────────────────────────────────────────────────────

/// Manages the active scope stack during interpretation.
/// Wraps the Scope chain with a more ergonomic interface for the interpreter.
class Environment {
public:
    Environment();

    /// The current (innermost) scope.
    std::shared_ptr<Scope> current() const { return current_; }

    /// Push a new scope (entering a block or function call).
    void push_scope(std::string name = "<block>");

    /// Push a pre-built scope (used for function calls with closures).
    void push_existing_scope(std::shared_ptr<Scope> scope);

    /// Pop the current scope (leaving a block or function call).
    void pop_scope();

    /// Define a variable in the current scope.
    void define(const std::string& name, Value value);

    /// Look up a variable through the scope chain.
    std::optional<Value> get(const std::string& name) const;

    /// Assign to an existing variable through the scope chain.
    bool assign(const std::string& name, Value value);

    /// Find the closest variable name for "did you mean?" suggestions.
    std::optional<std::string> find_closest(const std::string& name) const;

    /// How many scopes deep are we? (Useful for debugging.)
    size_t depth() const;

private:
    std::shared_ptr<Scope> current_;
};

} // namespace crashlang

#endif // CRASHLANG_ENVIRONMENT_HPP
