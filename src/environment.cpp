#include "crashlang/environment.hpp"

#include <algorithm>
#include <limits>

namespace crashlang {

// ── Levenshtein distance helper ────────────────────────────────────────────────

static size_t levenshtein(const std::string& a, const std::string& b) {
    const size_t m = a.size();
    const size_t n = b.size();

    // Two-row DP to save memory.
    std::vector<size_t> prev(n + 1), curr(n + 1);

    for (size_t j = 0; j <= n; ++j) prev[j] = j;

    for (size_t i = 1; i <= m; ++i) {
        curr[0] = i;
        for (size_t j = 1; j <= n; ++j) {
            if (a[i - 1] == b[j - 1]) {
                curr[j] = prev[j - 1];
            } else {
                curr[j] = 1 + std::min({prev[j], curr[j - 1], prev[j - 1]});
            }
        }
        std::swap(prev, curr);
    }

    return prev[n];
}

// ── Scope ──────────────────────────────────────────────────────────────────────

void Scope::define(const std::string& name, Value value) {
    bindings_[name] = std::move(value);
}

std::optional<Value> Scope::get(const std::string& name) const {
    auto it = bindings_.find(name);
    if (it != bindings_.end()) {
        return it->second;
    }
    if (parent_) {
        return parent_->get(name);
    }
    return std::nullopt;
}

bool Scope::assign(const std::string& name, Value value) {
    auto it = bindings_.find(name);
    if (it != bindings_.end()) {
        it->second = std::move(value);
        return true;
    }
    if (parent_) {
        return parent_->assign(name, value);
    }
    return false;
}

bool Scope::has(const std::string& name) const {
    return get(name).has_value();
}

bool Scope::has_local(const std::string& name) const {
    return bindings_.count(name) > 0;
}

std::vector<std::string> Scope::all_names() const {
    std::vector<std::string> names;
    names.reserve(bindings_.size());
    for (const auto& [k, _] : bindings_) {
        names.push_back(k);
    }
    if (parent_) {
        auto parent_names = parent_->all_names();
        names.insert(names.end(), parent_names.begin(), parent_names.end());
    }
    return names;
}

std::optional<std::string> Scope::find_closest(const std::string& name) const {
    auto candidates = all_names();
    if (candidates.empty()) return std::nullopt;

    std::string best;
    size_t best_dist = std::numeric_limits<size_t>::max();

    for (const auto& candidate : candidates) {
        size_t dist = levenshtein(name, candidate);
        if (dist < best_dist) {
            best_dist = dist;
            best = candidate;
        }
    }

    // Only suggest if the edit distance is small enough to be meaningful.
    constexpr size_t MAX_SUGGEST_DIST = 3;
    if (best_dist <= MAX_SUGGEST_DIST && !best.empty()) {
        return best;
    }
    return std::nullopt;
}

// ── Environment ────────────────────────────────────────────────────────────────

Environment::Environment() {
    // Start with a global scope.
    current_ = std::make_shared<Scope>(nullptr, "<global>");
}

void Environment::push_scope(std::string name) {
    current_ = std::make_shared<Scope>(current_, std::move(name));
}

void Environment::push_existing_scope(std::shared_ptr<Scope> scope) {
    current_ = std::move(scope);
}

void Environment::pop_scope() {
    if (current_->parent()) {
        current_ = current_->parent();
    }
}

void Environment::define(const std::string& name, Value value) {
    current_->define(name, std::move(value));
}

std::optional<Value> Environment::get(const std::string& name) const {
    return current_->get(name);
}

bool Environment::assign(const std::string& name, Value value) {
    return current_->assign(name, std::move(value));
}

std::optional<std::string> Environment::find_closest(const std::string& name) const {
    return current_->find_closest(name);
}

size_t Environment::depth() const {
    size_t d = 0;
    const Scope* s = current_.get();
    while (s) {
        ++d;
        s = s->parent().get();
    }
    return d;
}

} // namespace crashlang
