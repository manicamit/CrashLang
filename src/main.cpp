/// CrashLang entry point.
/// Currently: lex a file, parse it, and print the AST.

#include "crashlang/source.hpp"
#include "crashlang/lexer.hpp"
#include "crashlang/parser.hpp"
#include "crashlang/ast.hpp"

#include <cstring>
#include <iostream>

static void print_usage() {
    std::cout << "CrashLang v0.1.0\n\n"
              << "Usage:\n"
              << "  crashlang <file.cl>              Run a CrashLang program\n"
              << "  crashlang --tokens <file.cl>     Dump token stream\n"
              << "  crashlang --ast <file.cl>        Dump AST\n"
              << "  crashlang --help                 Show this help\n";
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        print_usage();
        return 0;
    }

    // Parse flags.
    bool show_tokens = false;
    bool show_ast    = false;
    const char* filename = nullptr;

    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--tokens") == 0) {
            show_tokens = true;
        } else if (std::strcmp(argv[i], "--ast") == 0) {
            show_ast = true;
        } else if (std::strcmp(argv[i], "--help") == 0 || std::strcmp(argv[i], "-h") == 0) {
            print_usage();
            return 0;
        } else {
            filename = argv[i];
        }
    }

    if (!filename) {
        std::cerr << "error: no input file\n";
        return 1;
    }

    // Load source.
    auto source = crashlang::load_source_file(filename);
    if (!source) return 1;

    // Lex.
    crashlang::Lexer lexer(*source);
    auto tokens = lexer.scan_tokens();

    if (show_tokens) {
        std::cout << "── Tokens ─────────────────────────────────────────\n";
        for (const auto& tok : tokens) {
            std::cout << "  " << tok << '\n';
        }
        std::cout << '\n';
    }

    if (lexer.had_errors()) {
        std::cerr << "Lexer errors encountered. Aborting.\n";
        return 1;
    }

    // Parse.
    crashlang::Parser parser(std::move(tokens), source.get());
    auto program = parser.parse();

    if (parser.had_errors()) {
        std::cerr << parser.format_errors();
        return 1;
    }

    if (show_ast) {
        std::cout << "── AST ────────────────────────────────────────────\n";
        std::cout << crashlang::format_ast(program) << '\n';
        std::cout << '\n';
    }

    // For now, just report success.
    std::cout << "Parsed " << program.statements.size() << " top-level statements.\n";
    return 0;
}
