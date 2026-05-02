
#include "crashlang/ast.hpp"
#include "crashlang/codegen.hpp"
#include "crashlang/compiler.hpp"
#include "crashlang/diagnostics.hpp"
#include "crashlang/errors.hpp"
#include "crashlang/interpreter.hpp"
#include "crashlang/ir.hpp"
#include "crashlang/ir_builder.hpp"
#include "crashlang/ir_passes.hpp"
#include "crashlang/lexer.hpp"
#include "crashlang/optimizer.hpp"
#include "crashlang/parser.hpp"
#include "crashlang/source.hpp"
#include "crashlang/vm.hpp"

#include <cstring>
#include <iostream>

static const char* VERSION = "0.4.0";

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
        "  crashlang --vm <file.cl>          Execute via bytecode VM\n"
        "  crashlang --disasm <file.cl>      Dump compiled bytecode\n"
        "  crashlang --optimize <file.cl>    Run with constant folding\n"
        "  crashlang --emit-ir <file.cl>      Dump register-based IR\n"
        "  crashlang --emit-asm <file.cl>     Emit register-allocated assembly\n"
        "  crashlang --version               Show version\n"
        "  crashlang --help                  Show this help\n";
}
static int run_repl(bool no_color) {
    std::cout << "CrashLang " << VERSION << " — interactive mode\n";
    std::cout << "Type expressions or statements. Use :quit or Ctrl-D to exit.\n\n";

    crashlang::DiagnosticsEngine diag(!no_color);
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
static int run_file(const char* filename,
                     bool show_tokens, bool show_ast,
                     bool no_color, bool check_leaks,
                     bool use_vm, bool show_disasm,
                     bool optimize, bool emit_ir, bool emit_asm)
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

    // ── Optimizer pass ─────────────────────────────────────────────────────────
    if (optimize) {
        crashlang::Optimizer opt;
        opt.optimize(program);
        if (opt.optimizations_applied() > 0) {
            std::cerr << "[optimizer] " << opt.optimizations_applied()
                      << " constant folding(s) applied.\n";
        }
    }

    // IR / assembly pipeline.
    if (emit_ir || emit_asm) {
        crashlang::IRBuilder builder;
        auto ir_module = builder.build(program);

        // Run DCE on each function.
        for (auto& fn : ir_module.functions) {
            dead_code_elimination(fn);
        }

        if (emit_ir) {
            std::cout << ir_module.dump();
            // Also show liveness info.
            for (const auto& fn : ir_module.functions) {
                auto liveness = compute_liveness(fn);
                std::cout << "\n" << liveness.dump(fn);
                auto alloc = allocate_registers(fn);
                std::cout << "\n" << alloc.dump();
            }
            return 0;
        }

        std::cout << emit_assembly(ir_module);
        return 0;
    }

    // ── VM path ────────────────────────────────────────────────────────────────
    if (use_vm || show_disasm) {
        crashlang::Compiler compiler(*source);
        auto chunk = compiler.compile(program);

        if (compiler.had_errors()) {
            std::cerr << "Compilation failed.\n";
            return 1;
        }

        if (show_disasm) {
            std::cout << chunk.disassemble();
            return 0;
        }

        crashlang::VM vm(*source);
        try {
            vm.execute(chunk);
        } catch (const crashlang::CrashError& err) {
            std::cerr << diag.format_crash(err, *source, vm.heap(), vm.ownership());
            return 1;
        }

        if (check_leaks) {
            auto leak_report = diag.format_leaks(vm.heap(), vm.ownership(), *source);
            if (!leak_report.empty()) {
                std::cerr << leak_report;
            }
        }
        return 0;
    }

    // ── Tree-walk path (default) ───────────────────────────────────────────────
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
int main(int argc, char* argv[]) {
    bool        show_tokens = false;
    bool        show_ast    = false;
    bool        no_color    = false;
    bool        check_leaks = false;
    bool        use_vm      = false;
    bool        show_disasm = false;
    bool        optimize    = false;
    bool        emit_ir     = false;
    bool        emit_asm    = false;
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
        } else if (std::strcmp(argv[i], "--vm") == 0) {
            use_vm = true;
        } else if (std::strcmp(argv[i], "--disasm") == 0) {
            show_disasm = true;
        } else if (std::strcmp(argv[i], "--optimize") == 0) {
            optimize = true;
        } else if (std::strcmp(argv[i], "--emit-ir") == 0) {
            emit_ir = true;
        } else if (std::strcmp(argv[i], "--emit-asm") == 0) {
            emit_asm = true;
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

    return run_file(filename, show_tokens, show_ast, no_color, check_leaks, use_vm, show_disasm, optimize, emit_ir, emit_asm);
}
