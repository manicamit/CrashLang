#include "crashlang/source.hpp"

#include <fstream>
#include <iostream>
#include <sstream>

namespace crashlang {

// ── SourceLocation ─────────────────────────────────────────────────────────────

std::string SourceLocation::to_string() const {
    std::string result;
    if (file) {
        result += file->filename;
        result += ':';
    }
    result += std::to_string(line);
    result += ':';
    result += std::to_string(column);
    return result;
}

bool SourceLocation::operator==(const SourceLocation& other) const {
    return file == other.file && line == other.line && column == other.column;
}

bool SourceLocation::operator!=(const SourceLocation& other) const {
    return !(*this == other);
}

// ── Span ───────────────────────────────────────────────────────────────────────

Span Span::merge(const Span& a, const Span& b) {
    return Span{a.start, b.end};
}

std::string Span::to_string() const {
    return start.to_string();
}

bool Span::is_valid() const {
    return start.line > 0 && start.column > 0;
}

// ── SourceFile ─────────────────────────────────────────────────────────────────

SourceFile::SourceFile(std::string filename_, std::string source_)
    : filename(std::move(filename_))
    , source(std::move(source_))
{
    build_line_index();
}

void SourceFile::build_line_index() {
    line_offsets_.clear();
    line_offsets_.push_back(0); // Line 1 starts at offset 0.

    for (size_t i = 0; i < source.size(); ++i) {
        if (source[i] == '\n') {
            line_offsets_.push_back(i + 1);
        }
    }
}

std::string_view SourceFile::get_line(LineNo line) const {
    if (line == 0 || line > line_count()) {
        return {};
    }

    size_t idx   = static_cast<size_t>(line - 1);
    size_t start = line_offsets_[idx];

    // End is either the next newline or end-of-file.
    size_t end = (idx + 1 < line_offsets_.size())
                     ? line_offsets_[idx + 1] - 1  // Exclude the '\n' itself.
                     : source.size();

    // Handle \r\n line endings.
    if (end > start && end <= source.size() && source[end - 1] == '\r') {
        --end;
    }

    return std::string_view(source.data() + start, end - start);
}

LineNo SourceFile::line_count() const {
    return static_cast<LineNo>(line_offsets_.size());
}

SourceLocation SourceFile::location_from_offset(size_t offset) const {
    // Binary search for the line containing this offset.
    LineNo lo = 0;
    LineNo hi = static_cast<LineNo>(line_offsets_.size());

    while (lo < hi) {
        LineNo mid = lo + (hi - lo) / 2;
        if (line_offsets_[mid] <= offset) {
            lo = mid + 1;
        } else {
            hi = mid;
        }
    }

    // `lo` is now the first line whose offset is > `offset`,
    // so the offset belongs to line `lo` (1-indexed).
    LineNo line = lo;
    ColNo col   = static_cast<ColNo>(offset - line_offsets_[line - 1] + 1);

    return SourceLocation{this, line, col};
}

// ── File loading ───────────────────────────────────────────────────────────────

std::unique_ptr<SourceFile> load_source_file(const std::string& path) {
    std::ifstream file(path, std::ios::in | std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "error: could not open file: " << path << '\n';
        return nullptr;
    }

    std::ostringstream buf;
    buf << file.rdbuf();

    if (file.bad()) {
        std::cerr << "error: failed to read file: " << path << '\n';
        return nullptr;
    }

    return std::make_unique<SourceFile>(path, buf.str());
}

} // namespace crashlang
