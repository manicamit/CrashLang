/// CrashLang entry point.
/// Currently supports: load a file, tokenize it, and print the token stream.

#include "crashlang/source.hpp"
#include "crashlang/lexer.hpp"

#include <iostream>

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cout << "CrashLang v0.1.0\n";
        std::cout << "Usage: crashlang <file.cl>\n";
        return 0;
    }

    // Load source file.
    auto source = crashlang::load_source_file(argv[1]);
    if (!source) {
        return 1;
    }

    std::cout << "── Source ─────────────────────────────────────────\n";
    for (crashlang::LineNo i = 1; i <= source->line_count(); ++i) {
        std::cout << "  " << i << " │ " << source->get_line(i) << '\n';
    }

    // Tokenize.
    crashlang::Lexer lexer(*source);
    auto tokens = lexer.scan_tokens();

    std::cout << "\n── Tokens ─────────────────────────────────────────\n";
    for (const auto& tok : tokens) {
        std::cout << "  " << tok << '\n';
    }

    if (lexer.had_errors()) {
        std::cout << "\n[!] Lexer encountered errors.\n";
        return 1;
    }

    std::cout << "\n[✓] " << tokens.size() << " tokens produced.\n";
    return 0;
}
