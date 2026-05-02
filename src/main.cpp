/// CrashLang entry point — file runner and interactive REPL.

#include "crashlang/ast.hpp"
#include "crashlang/diagnostics.hpp"
#include "crashlang/errors.hpp"
#include "crashlang/interpreter.hpp"
#include "crashlang/lexer.hpp"
#include "crashlang/parser.hpp"
#include "crashlang/source.hpp"

#include <cstring>
#include <iostream>

static const char* VERSION = "0.3.0";

static void print_version() {
    std::cout << "CrashLang " << VERSION << '\n';
}

static void print_usage() {
    std::cout <<
        "CrashLang " << VERSION << "\n\n"
        "Usage:\n"
        "  crashlang                         Start the interactive REPL\n"
        "  crashlang <file.cl>               Run a CrashLang program\n"
        "  crashlang --tokens <file.cl>      Dump token stream and exit\n"
        "  crashlang --ast <file.cl>         Dump AST and exit\n"
        "  crashlang --no-color <file.cl>    Disable ANSI colors in output\n"
        "  crashlang --check-leaks <file.cl> Report memory leaks at exit\n"
        "  crashlang --version               Show version\n"
        "  crashlang --help                  Show this help\n";
}

// ── REPL ───────────────────────────────────────────────────────────────────────

static int run_repl(bool no_color) {
    std::cout << "CrashLang " << VERSION << " — interactive mode\n";
    std::cout << "Type expressions or statements. Use :quit or Ctrl-D to exit.\n\n";

    crashlang::DiagnosticsEngine diag(!no_color);

    // A dummy source used for each line's diagnostics.
    auto dummy = crashlang::SourceFile("<repl>", "");
    crashlang::Interpreter interp(dummy);

    std::string line;
    int line_num = 0;
    for (;;) {
        std::cout << "crash> ";
        std::cout.flush();

        if (!std::getline(std::cin, line)) {
            std::cout << "\nGoodbye.\n";
            break;
        }

        if (line.empty()) continue;

        // REPL commands.
        if (line == ":quit" || line == ":q" || line == ":exit") {
            std::cout << "Goodbye.\n";
            break;
        }
        if (line == ":help" || line == ":h") {
            std::cout
                << "  :help    Show this help\n"
                << "  :quit    Exit the REPL\n"
                << "  :leaks   Check for memory leaks\n";
            continue;
        }
        if (line == ":leaks") {
            auto report = diag.format_leaks(interp.heap(), interp.ownership(), dummy);
            if (report.empty()) {
                std::cout << "No memory leaks detected.\n";
            } else {
                std::cerr << report;
            }
            continue;
        }

        ++line_num;

        // Create a fresh SourceFile for this input line so the lexer
        // and parser work correctly.
        auto line_source = crashlang::SourceFile("<repl>", line + "\n");

        crashlang::Lexer lexer(line_source);
        auto tokens = lexer.scan_tokens();

        if (lexer.had_errors()) {
            std::cerr << "Syntax error in input.\n";
            continue;
        }

        crashlang::Parser parser(std::move(tokens), &line_source);
        auto program = parser.parse();

        if (parser.had_errors()) {
            std::cerr << parser.format_errors();
            continue;
        }

        try {
            interp.execute(program);
        } catch (const crashlang::CrashError& err) {
            std::cerr << diag.format_crash(err, line_source,
                                            interp.heap(), interp.ownership());
        }
    }

    return 0;
}

// ── File execution ─────────────────────────────────────────────────────────────

static int run_file(const char* filename,
                     bool show_tokens, bool show_ast,
                     bool no_color, bool check_leaks)
{
    auto source = crashlang::load_source_file(filename);
    if (!source) return 1;

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

    crashlang::Parser parser(std::move(tokens), source.get());
    auto program = parser.parse();

    if (parser.had_errors()) {
        std::cerr << parser.format_errors();
        return 1;
    }

    if (show_ast) {
        std::cout << "── AST ────────────────────────────────────────────\n";
        std::cout << crashlang::format_ast(program) << '\n';
        if (!show_tokens) return 0;
    }
    if (show_tokens) return 0;

    crashlang::DiagnosticsEngine diag(!no_color);
    crashlang::Interpreter interp(*source);

    try {
        interp.execute(program);
    } catch (const crashlang::CrashError& err) {
        std::cerr << diag.format_crash(err, *source, interp.heap(), interp.ownership());
        return 1;
    }

    if (check_leaks) {
        auto leak_report = diag.format_leaks(interp.heap(), interp.ownership(), *source);
        if (!leak_report.empty()) {
            std::cerr << leak_report;
        }
    }

    return 0;
}

// ── main ───────────────────────────────────────────────────────────────────────

int main(int argc, char* argv[]) {
    bool        show_tokens = false;
    bool        show_ast    = false;
    bool        no_color    = false;
    bool        check_leaks = false;
    const char* filename    = nullptr;

    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--tokens") == 0) {
            show_tokens = true;
        } else if (std::strcmp(argv[i], "--ast") == 0) {
            show_ast = true;
        } else if (std::strcmp(argv[i], "--no-color") == 0) {
            no_color = true;
        } else if (std::strcmp(argv[i], "--check-leaks") == 0) {
            check_leaks = true;
        } else if (std::strcmp(argv[i], "--help") == 0
                || std::strcmp(argv[i], "-h") == 0) {
            print_usage();
            return 0;
        } else if (std::strcmp(argv[i], "--version") == 0
                || std::strcmp(argv[i], "-v") == 0) {
            print_version();
            return 0;
        } else {
            filename = argv[i];
        }
    }

    if (!filename) {
        return run_repl(no_color);
    }

    return run_file(filename, show_tokens, show_ast, no_color, check_leaks);
}
