#ifndef CRASHLANG_SOURCE_HPP
#define CRASHLANG_SOURCE_HPP

/// Source file representation and location tracking.
///
/// Every token and AST node in CrashLang carries a Span — a pair of
/// SourceLocations marking where it starts and ends in the source file.
/// This metadata flows through the entire pipeline and is what allows the
/// diagnostics engine to point at real code in crash reports.

#include "crashlang/common.hpp"

#include <string>
#include <string_view>
#include <vector>
#include <memory>

namespace crashlang {

// ── SourceLocation ─────────────────────────────────────────────────────────────

/// A position in source code: file, line number, column number.
/// All values are 1-indexed (the first character of a file is line 1, col 1).
struct SourceLocation {
    const SourceFile* file = nullptr;
    LineNo line   = 0;
    ColNo  column = 0;

    /// Format as "filename:line:column" for display.
    std::string to_string() const;

    bool operator==(const SourceLocation& other) const;
    bool operator!=(const SourceLocation& other) const;
};

// ── Span ───────────────────────────────────────────────────────────────────────

/// A range in source code from `start` to `end` (inclusive).
/// Every token and every AST node carries one of these.
struct Span {
    SourceLocation start;
    SourceLocation end;

    /// Create a span that covers both `a` and `b`.
    static Span merge(const Span& a, const Span& b);

    /// Format as "filename:line:col" (uses start position).
    std::string to_string() const;

    /// Check if this span has valid location data.
    bool is_valid() const;
};

// ── SourceFile ─────────────────────────────────────────────────────────────────

/// Holds the full contents of a source file along with a precomputed
/// line-offset index for O(1) access to any line's text.
///
/// Immutable after construction — shared across all phases of compilation.
struct SourceFile {
    std::string filename;
    std::string source;

    /// Construct from a filename and its full text content.
    SourceFile(std::string filename, std::string source);

    /// Get the text of a specific line (1-indexed). Returns empty if out of range.
    std::string_view get_line(LineNo line) const;

    /// Total number of lines in the source.
    LineNo line_count() const;

    /// Create a SourceLocation for a given offset into the source string.
    SourceLocation location_from_offset(size_t offset) const;

private:
    /// Byte offsets where each line starts. line_offsets_[0] is line 1's start.
    std::vector<size_t> line_offsets_;

    /// Build the line offset index from `source`.
    void build_line_index();
};

/// Load a source file from disk. Returns nullptr and prints to stderr on failure.
std::unique_ptr<SourceFile> load_source_file(const std::string& path);

} // namespace crashlang

#endif // CRASHLANG_SOURCE_HPP
