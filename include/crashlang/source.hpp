#ifndef CRASHLANG_SOURCE_HPP
#define CRASHLANG_SOURCE_HPP

#include "crashlang/common.hpp"

#include <string>
#include <string_view>
#include <vector>
#include <memory>

namespace crashlang {

struct SourceLocation {
    const SourceFile* file = nullptr;
    LineNo line   = 0;
    ColNo  column = 0;

    std::string to_string() const;

    bool operator==(const SourceLocation& other) const;
    bool operator!=(const SourceLocation& other) const;
};

// A range in source code. Every token and AST node carries one.
struct Span {
    SourceLocation start;
    SourceLocation end;

    static Span merge(const Span& a, const Span& b);
    std::string to_string() const;
    bool is_valid() const;
};

struct SourceFile {
    std::string filename;
    std::string source;

    SourceFile(std::string filename, std::string source);

    std::string_view get_line(LineNo line) const;
    LineNo line_count() const;
    SourceLocation location_from_offset(size_t offset) const;

private:
    std::vector<size_t> line_offsets_;
    void build_line_index();
};

std::unique_ptr<SourceFile> load_source_file(const std::string& path);

} // namespace crashlang

#endif
