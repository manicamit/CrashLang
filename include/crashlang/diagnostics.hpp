#ifndef CRASHLANG_DIAGNOSTICS_HPP
#define CRASHLANG_DIAGNOSTICS_HPP

#include "crashlang/errors.hpp"
#include "crashlang/heap.hpp"
#include "crashlang/ownership.hpp"
#include "crashlang/source.hpp"

#include <string>

namespace crashlang {

class DiagnosticsEngine {
public:
    explicit DiagnosticsEngine(bool use_color = true);

    std::string format_crash(const CrashError& error,
                              const SourceFile& source,
                              const HeapSimulator& heap,
                              const OwnershipTracker& ownership) const;

    std::string format_leaks(const HeapSimulator& heap,
                              const OwnershipTracker& ownership,
                              const SourceFile& source) const;

private:
    bool use_color_;

    std::string bold(const std::string& text) const;
    std::string red(const std::string& text) const;
    std::string bright_red(const std::string& text) const;
    std::string yellow(const std::string& text) const;
    std::string green(const std::string& text) const;
    std::string cyan(const std::string& text) const;
    std::string dim(const std::string& text) const;
    std::string magenta(const std::string& text) const;

    std::string format_header(const CrashError& error) const;
    std::string format_source_context(const CrashError& error, const SourceFile& source) const;
    std::string format_what_happened(const CrashError& error) const;
    std::string format_timeline(const CrashError& error, const HeapSimulator& heap,
                                 const OwnershipTracker& ownership, const SourceFile& source) const;
    std::string format_call_stack(const CrashError& error) const;
    std::string format_why(const CrashError& error) const;
    std::string format_suggestion(const CrashError& error) const;
};

} // namespace crashlang

#endif
