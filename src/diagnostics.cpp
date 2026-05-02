#include "crashlang/diagnostics.hpp"

#include <algorithm>
#include <iomanip>
#include <sstream>

namespace crashlang {

// ── ANSI escape codes ──────────────────────────────────────────────────────────

static constexpr const char* RESET     = "\033[0m";
static constexpr const char* BOLD      = "\033[1m";
static constexpr const char* DIM       = "\033[2m";
static constexpr const char* RED       = "\033[31m";
static constexpr const char* BRIGHT_RED = "\033[91m";
static constexpr const char* GREEN     = "\033[32m";
static constexpr const char* YELLOW    = "\033[33m";
static constexpr const char* CYAN      = "\033[36m";
static constexpr const char* MAGENTA   = "\033[35m";

// ── Constructor ────────────────────────────────────────────────────────────────

DiagnosticsEngine::DiagnosticsEngine(bool use_color)
    : use_color_(use_color)
{}

// ── Color helpers ──────────────────────────────────────────────────────────────

std::string DiagnosticsEngine::bold(const std::string& text) const {
    if (!use_color_) return text;
    return std::string(BOLD) + text + RESET;
}

std::string DiagnosticsEngine::red(const std::string& text) const {
    if (!use_color_) return text;
    return std::string(RED) + text + RESET;
}

std::string DiagnosticsEngine::bright_red(const std::string& text) const {
    if (!use_color_) return text;
    return std::string(BRIGHT_RED) + BOLD + text + RESET;
}

std::string DiagnosticsEngine::yellow(const std::string& text) const {
    if (!use_color_) return text;
    return std::string(YELLOW) + text + RESET;
}

std::string DiagnosticsEngine::green(const std::string& text) const {
    if (!use_color_) return text;
    return std::string(GREEN) + text + RESET;
}

std::string DiagnosticsEngine::cyan(const std::string& text) const {
    if (!use_color_) return text;
    return std::string(CYAN) + text + RESET;
}

std::string DiagnosticsEngine::dim(const std::string& text) const {
    if (!use_color_) return text;
    return std::string(DIM) + text + RESET;
}

std::string DiagnosticsEngine::magenta(const std::string& text) const {
    if (!use_color_) return text;
    return std::string(MAGENTA) + text + RESET;
}

// ── Box-drawing helpers ────────────────────────────────────────────────────────

static std::string bar_top(const std::string& title, bool color) {
    std::string line = " RuntimeError: " + title + " ";
    size_t pad_len = static_cast<size_t>(std::max(60 - static_cast<int>(line.size()), 4));
    std::string border(pad_len, '=');
    // Use simple ASCII for maximum compatibility.
    if (color) {
        return std::string(BOLD) + BRIGHT_RED + "\n+--" + line + std::string(border.size(), '-') + "--+" + RESET;
    }
    return "\n+--" + line + std::string(border.size(), '-') + "--+";
}

static std::string bar_bottom(bool color) {
    if (color) {
        return std::string(DIM) + "+" + std::string(62, '-') + "+" + RESET;
    }
    return "+" + std::string(62, '-') + "+";
}

static std::string pipe(bool color) {
    if (color) return std::string(DIM) + "|" + RESET;
    return "|";
}

// ── format_crash ───────────────────────────────────────────────────────────────

std::string DiagnosticsEngine::format_crash(const CrashError& error,
                                              const SourceFile& source,
                                              const HeapSimulator& heap,
                                              const OwnershipTracker& ownership) const
{
    std::ostringstream out;

    // Top bar.
    out << bar_top(crash_kind_name(error.kind), use_color_) << '\n';
    out << pipe(use_color_) << '\n';

    // Location.
    out << pipe(use_color_) << "  " << bold(error.location.to_string()) << '\n';
    out << pipe(use_color_) << '\n';

    // Source context.
    out << format_source_context(error, source);

    // What happened.
    out << format_what_happened(error);

    // Object timeline (only for heap-related errors).
    if (error.related_object) {
        out << format_timeline(error, heap, ownership, source);
    }

    // Call stack.
    if (!error.call_stack.empty()) {
        out << format_call_stack(error);
    }

    // Why this is a problem.
    auto why = format_why(error);
    if (!why.empty()) {
        out << why;
    }

    // Suggested fix.
    auto suggestion = format_suggestion(error);
    if (!suggestion.empty()) {
        out << suggestion;
    }

    // Bottom bar.
    out << pipe(use_color_) << '\n';
    out << bar_bottom(use_color_) << '\n';

    return out.str();
}

// ── Source context ─────────────────────────────────────────────────────────────

std::string DiagnosticsEngine::format_source_context(const CrashError& error,
                                                       const SourceFile& source) const
{
    std::ostringstream out;

    uint32_t err_line = error.location.start.line;
    uint32_t err_col  = error.location.start.column;
    uint32_t end_col  = error.location.end.column;

    // Show 2 lines before and the error line.
    uint32_t start_line = (err_line > 2) ? err_line - 2 : 1;
    uint32_t end_line   = err_line;

    // Figure out line number width for alignment.
    int gutter_width = static_cast<int>(std::to_string(end_line).size());

    for (uint32_t ln = start_line; ln <= end_line; ++ln) {
        auto line_text = source.get_line(ln);

        if (ln == err_line) {
            // Arrow line — the offending line.
            out << pipe(use_color_) << "  "
                << bright_red("->") << " "
                << dim(std::to_string(ln)) << " "
                << dim("|") << " " << line_text << '\n';

            // Caret line.
            std::string padding(static_cast<size_t>(gutter_width + 1), ' ');
            std::string carets;
            size_t caret_len = (end_col > err_col) ? end_col - err_col : 1;
            // Make sure we have at least one caret.
            if (caret_len == 0) caret_len = 1;
            carets = std::string(caret_len, '^');

            out << pipe(use_color_) << "     "
                << padding << dim("|") << " ";
            if (err_col > 1) {
                out << std::string(err_col - 1, ' ');
            }
            out << bright_red(carets) << '\n';
        } else {
            // Context line.
            out << pipe(use_color_) << "   "
                << dim(std::to_string(ln)) << " "
                << dim("|") << " " << line_text << '\n';
        }
    }

    out << pipe(use_color_) << '\n';
    return out.str();
}

// ── What happened ──────────────────────────────────────────────────────────────

std::string DiagnosticsEngine::format_what_happened(const CrashError& error) const {
    std::ostringstream out;

    out << pipe(use_color_) << "  " << bold("What happened:") << '\n';
    out << pipe(use_color_) << "    " << error.detail << '\n';
    out << pipe(use_color_) << '\n';

    return out.str();
}

// ── Object timeline ────────────────────────────────────────────────────────────

std::string DiagnosticsEngine::format_timeline(const CrashError& error,
                                                 const HeapSimulator& heap,
                                                 const OwnershipTracker& ownership,
                                                 const SourceFile& source) const
{
    if (!error.related_object) return "";
    HeapID id = *error.related_object;

    if (!ownership.has_record(id)) return "";

    const auto& record = ownership.get_record(id);
    const auto& timeline = record.timeline;

    if (timeline.empty()) return "";

    std::ostringstream out;

    out << pipe(use_color_) << "  " << bold("Object lifetime")
        << " " << dim("[heap#" + std::to_string(id) + "]") << ":" << '\n';

    for (const auto& event : timeline) {
        std::string line_str = std::to_string(event.location.start.line);
        std::string kind_str = ownership_event_kind_name(event.kind);

        // Pad for alignment.
        while (line_str.size() < 4) line_str = " " + line_str;
        while (kind_str.size() < 8) kind_str += " ";

        out << pipe(use_color_) << "    "
            << cyan("line " + line_str) << "  "
            << yellow(kind_str) << "  "
            << dim("--") << " " << event.description << '\n';
    }

    // Add the "you are here" marker for the crash site.
    std::string crash_line = std::to_string(error.location.start.line);
    while (crash_line.size() < 4) crash_line = " " + crash_line;

    // Determine what kind of action was attempted.
    std::string action = "ACCESS";
    auto it = error.metadata.find("action");
    if (it != error.metadata.end()) {
        action = it->second;
    } else if (error.kind == CrashKind::DoubleFree) {
        action = "free()  ";
    }
    while (action.size() < 8) action += " ";

    out << pipe(use_color_) << "  "
        << bright_red("->") << " "
        << bright_red("line " + crash_line) << "  "
        << bright_red(action) << "  "
        << bright_red("<- you are here (after free)") << '\n';

    out << pipe(use_color_) << '\n';

    // Suppress unused parameter warning for heap — we access it
    // through the ownership tracker's records.
    (void)heap;
    (void)source;

    return out.str();
}

// ── Call stack ──────────────────────────────────────────────────────────────────

std::string DiagnosticsEngine::format_call_stack(const CrashError& error) const {
    std::ostringstream out;

    out << pipe(use_color_) << "  " << bold("Call stack:") << '\n';

    // Print innermost last (reverse order).
    for (auto it = error.call_stack.rbegin(); it != error.call_stack.rend(); ++it) {
        out << pipe(use_color_) << "    "
            << cyan(it->function_name)
            << dim("  called at ") << it->call_site.to_string() << '\n';
    }

    out << pipe(use_color_) << '\n';
    return out.str();
}

// ── Why this is a problem ──────────────────────────────────────────────────────

std::string DiagnosticsEngine::format_why(const CrashError& error) const {
    std::ostringstream out;
    std::string reason;

    switch (error.kind) {
        case CrashKind::UseAfterFree: {
            auto free_site = error.metadata.find("free_site");
            reason = "The memory backing this object was released";
            if (free_site != error.metadata.end()) {
                reason += " at " + free_site->second;
            }
            reason += ".\n"
                "    Accessing it afterward reads memory that may have been\n"
                "    reclaimed or overwritten by another allocation.";
            break;
        }
        case CrashKind::DoubleFree: {
            auto first = error.metadata.find("first_free_location");
            reason = "This object was already freed";
            if (first != error.metadata.end()) {
                reason += " at " + first->second;
            }
            reason += ".\n"
                "    Calling free() again is undefined behavior in real systems\n"
                "    and can corrupt the heap allocator's internal state.";
            break;
        }
        case CrashKind::NullDereference:
            reason = "A nil reference points to nothing.\n"
                "    Dereferencing it would be equivalent to a null pointer\n"
                "    dereference in C/C++, causing a segmentation fault.";
            break;
        case CrashKind::OutOfBounds: {
            auto idx = error.metadata.find("index");
            auto len = error.metadata.find("length");
            reason = "Array indices must be in the range [0, length).";
            if (idx != error.metadata.end() && len != error.metadata.end()) {
                reason += "\n    You accessed index " + idx->second
                        + " but the array only has " + len->second + " elements.";
            }
            break;
        }
        case CrashKind::OwnershipViolation:
            reason = "After move(), the source variable no longer owns the object.\n"
                "    Using it would violate memory safety, because two variables\n"
                "    would think they own the same memory.";
            break;
        case CrashKind::DivisionByZero:
            reason = "Division by zero is mathematically undefined.\n"
                "    In integer arithmetic it would cause a hardware exception;\n"
                "    in floating-point it would produce infinity or NaN.";
            break;
        case CrashKind::TypeMismatch:
            reason = "The operand types don't support this operation.\n"
                "    CrashLang is dynamically typed but does not perform\n"
                "    implicit conversions between incompatible types.";
            break;
        case CrashKind::UndefinedVariable:
            reason = "The variable was not declared with 'let' in any\n"
                "    enclosing scope.";
            break;
        case CrashKind::StackOverflow:
            reason = "The maximum call depth was exceeded, likely due to\n"
                "    infinite recursion. Each function call adds a frame\n"
                "    to the stack, and the limit prevents the process from\n"
                "    running out of real stack memory.";
            break;
        case CrashKind::ArityMismatch:
            reason = "The function was called with the wrong number of arguments.\n"
                "    Check the function definition for the expected parameter count.";
            break;
        case CrashKind::InvalidFieldAccess:
            reason = "The field does not exist on this struct type.\n"
                "    Fields are defined at allocation time in the 'new' expression.";
            break;
        default:
            return ""; // No "why" section for generic errors.
    }

    out << pipe(use_color_) << "  " << bold("Why this is a problem:") << '\n';

    // Wrap each line with the pipe character.
    std::istringstream iss(reason);
    std::string line;
    while (std::getline(iss, line)) {
        out << pipe(use_color_) << "    " << line << '\n';
    }

    out << pipe(use_color_) << '\n';
    return out.str();
}

// ── Suggested fix ──────────────────────────────────────────────────────────────

std::string DiagnosticsEngine::format_suggestion(const CrashError& error) const {
    std::ostringstream out;
    std::string suggestion;

    switch (error.kind) {
        case CrashKind::UseAfterFree:
            suggestion = "Read the field before calling free(), or restructure\n"
                "    the code so the object is not accessed after being freed.";
            break;
        case CrashKind::DoubleFree:
            suggestion = "Remove the second free(), or guard it with a nil check:\n"
                "    if obj != nil { free(obj); obj = nil; }";
            break;
        case CrashKind::NullDereference:
            suggestion = "Initialize the reference before dereferencing it, or\n"
                "    add a nil check: if r != nil { let val = deref(r); }";
            break;
        case CrashKind::OutOfBounds: {
            suggestion = "Check the array length before accessing:\n"
                "    if i < len(arr) { let val = arr[i]; }";
            break;
        }
        case CrashKind::OwnershipViolation:
            suggestion = "Don't use a variable after calling move() on it.\n"
                "    If you need the value in both places, use ref() instead.";
            break;
        case CrashKind::DivisionByZero:
            suggestion = "Check for zero before dividing:\n"
                "    if divisor != 0 { let result = n / divisor; }";
            break;
        case CrashKind::TypeMismatch:
            suggestion = "Use to_string(), to_int(), or to_float() to convert\n"
                "    between types, or check the type with type_of() first.";
            break;
        case CrashKind::UndefinedVariable: {
            auto it = error.metadata.find("suggestion");
            if (it != error.metadata.end()) {
                suggestion = "Did you mean '" + it->second + "'?";
            } else {
                suggestion = "Declare the variable with 'let' before using it.";
            }
            break;
        }
        case CrashKind::StackOverflow:
            suggestion = "Add a base case to your recursive function to ensure\n"
                "    it terminates.";
            break;
        case CrashKind::ArityMismatch: {
            auto expected = error.metadata.find("expected");
            auto got = error.metadata.find("got");
            if (expected != error.metadata.end() && got != error.metadata.end()) {
                suggestion = "The function expects " + expected->second
                    + " argument(s), but you passed " + got->second + ".";
            } else {
                suggestion = "Check the function definition for the correct\n"
                    "    number of parameters.";
            }
            break;
        }
        case CrashKind::InvalidFieldAccess: {
            auto avail = error.metadata.find("available_fields");
            if (avail != error.metadata.end() && !avail->second.empty()) {
                suggestion = "Available fields: " + avail->second;
            } else {
                suggestion = "Check the 'new' expression to see which fields\n"
                    "    were defined on this object.";
            }
            break;
        }
        default:
            return "";
    }

    out << pipe(use_color_) << "  " << green(bold("Suggested fix:")) << '\n';

    std::istringstream iss(suggestion);
    std::string line;
    while (std::getline(iss, line)) {
        out << pipe(use_color_) << "    " << green(line) << '\n';
    }

    out << pipe(use_color_) << '\n';
    return out.str();
}

// ── Memory leak report ─────────────────────────────────────────────────────────

std::string DiagnosticsEngine::format_leaks(const HeapSimulator& heap,
                                              const OwnershipTracker& ownership,
                                              const SourceFile& source) const
{
    auto leaks = heap.get_all_leaked();
    if (leaks.empty()) return "";

    std::ostringstream out;

    out << '\n'
        << yellow(bold("Warning: " + std::to_string(leaks.size())
            + " object(s) were never freed (memory leak)"))
        << '\n' << '\n';

    for (const auto* obj : leaks) {
        std::string owner = "<unknown>";
        if (ownership.has_record(obj->id)) {
            owner = ownership.get_owner(obj->id);
        }

        out << "  " << yellow("heap#" + std::to_string(obj->id))
            << "  " << dim(obj->type_name)
            << "  allocated at " << obj->allocation_site.to_string()
            << "  in " << obj->allocated_in_function << '\n';

        // Show the allocation line.
        auto line_text = source.get_line(obj->allocation_site.start.line);
        out << "    "
            << dim(std::to_string(obj->allocation_site.start.line) + " | ")
            << line_text << '\n';
    }

    out << '\n'
        << dim("  Add free() calls for these objects, or restructure the")
        << '\n'
        << dim("  code so ownership is transferred to a scope that frees them.")
        << '\n';

    return out.str();
}

} // namespace crashlang
