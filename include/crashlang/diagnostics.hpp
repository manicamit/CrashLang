#ifndef CRASHLANG_DIAGNOSTICS_HPP
#define CRASHLANG_DIAGNOSTICS_HPP

/// Diagnostics engine for CrashLang.
///
/// Takes a CrashError, the source file, the heap state, and the ownership
/// timeline, then produces a structured, ANSI-colored crash report with:
///
///   1. Error header (kind + location)
///   2. Source context with caret highlighting
///   3. "What happened" — plain-English explanation
///   4. Object lifetime timeline (for heap-related errors)
///   5. Call stack
///   6. "Why this is a problem" — technical explanation
///   7. "Suggested fix" — actionable recommendation
///
/// This is the feature that makes CrashLang unique.

#include "crashlang/errors.hpp"
#include "crashlang/heap.hpp"
#include "crashlang/ownership.hpp"
#include "crashlang/source.hpp"

#include <string>

namespace crashlang {

class DiagnosticsEngine {
public:
    /// Controls whether ANSI escape codes are emitted.
    explicit DiagnosticsEngine(bool use_color = true);

    /// Format a crash report from a CrashError.
    std::string format_crash(const CrashError& error,
                              const SourceFile& source,
                              const HeapSimulator& heap,
                              const OwnershipTracker& ownership) const;

    /// Format a memory leak warning (called at program exit).
    std::string format_leaks(const HeapSimulator& heap,
                              const OwnershipTracker& ownership,
                              const SourceFile& source) const;

private:
    bool use_color_;

    // ── ANSI color helpers ─────────────────────────────────────────────────────

    std::string bold(const std::string& text) const;
    std::string red(const std::string& text) const;
    std::string bright_red(const std::string& text) const;
    std::string yellow(const std::string& text) const;
    std::string green(const std::string& text) const;
    std::string cyan(const std::string& text) const;
    std::string dim(const std::string& text) const;
    std::string magenta(const std::string& text) const;

    // ── Report sections ────────────────────────────────────────────────────────

    std::string format_header(const CrashError& error) const;

    std::string format_source_context(const CrashError& error,
                                       const SourceFile& source) const;

    std::string format_what_happened(const CrashError& error) const;

    std::string format_timeline(const CrashError& error,
                                 const HeapSimulator& heap,
                                 const OwnershipTracker& ownership,
                                 const SourceFile& source) const;

    std::string format_call_stack(const CrashError& error) const;

    std::string format_why(const CrashError& error) const;

    std::string format_suggestion(const CrashError& error) const;
};

} // namespace crashlang

#endif // CRASHLANG_DIAGNOSTICS_HPP
