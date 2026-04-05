#include "types.h"
#include "compiler_runner.h"
#include "error_parser.h"
#include "context_extractor.h"
#include "llm_client.h"
#include "patch_applier.h"
#include "terminal_ui.h"

#include <iostream>
#include <cstdlib>
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <chrono>

// ── CLI argument parsing ───────────────────────────────────────────────────────
static void print_usage(const char* prog) {
    std::cerr << "Usage: " << prog << " [options] <source_file> [-- compiler_flags...]\n"
              << "\n"
              << "Options:\n"
              << "  -y, --yes         Auto-apply patches without prompting\n"
              << "  -w, --warnings    Also fix warnings\n"
              << "  -v, --verbose     Verbose output\n"
              << "  -c <compiler>     Compiler to use (default: clang++)\n"
              << "  -m <model>        LLM model (default: claude-sonnet-4-20250514)\n"
              << "  -h, --help        Show this help\n"
              << "\n"
              << "Environment variables:\n"
              << "  ANTHROPIC_API_KEY  Your Anthropic API key (required)\n"
              << "\n"
              << "Examples:\n"
              << "  fixcc broken.cpp\n"
              << "  fixcc -y main.cpp -- -std=c++17 -I./include\n"
              << "  fixcc -c gcc broken.c -- -Wall\n";
}

static Config parse_args(int argc, char** argv) {
    Config cfg;

    cfg.api_key = std::getenv("ANTHROPIC_API_KEY") ? std::getenv("ANTHROPIC_API_KEY") : "";

    bool past_dashdash = false;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (past_dashdash) {
            cfg.compiler_flags.push_back(arg);
            continue;
        }
        if (arg == "--") { past_dashdash = true; continue; }
        if (arg == "-h" || arg == "--help")     { print_usage(argv[0]); std::exit(0); }
        if (arg == "-y" || arg == "--yes")      { cfg.auto_apply = true; continue; }
        if (arg == "-v" || arg == "--verbose")  { cfg.verbose    = true; continue; }
        if (arg == "-c" && i+1 < argc)          { cfg.compiler   = argv[++i]; continue; }
        if (arg == "-m" && i+1 < argc)          { cfg.model      = argv[++i]; continue; }
        if (arg[0] != '-')                       { cfg.source_file = arg; continue; }
    }

    return cfg;
}

// ── Spinner thread helper ──────────────────────────────────────────────────────
struct Spinner {
    std::atomic<bool> running{true};
    std::thread       thread;
    const TerminalUI& ui;

    explicit Spinner(const TerminalUI& u) : ui(u) {
        thread = std::thread([this](){
            int tick = 0;
            while (running.load()) {
                ui.spin(tick++);
                std::this_thread::sleep_for(std::chrono::milliseconds(80));
            }
            ui.clear_line();
        });
    }
    ~Spinner() { stop(); }
    void stop() {
        running = false;
        if (thread.joinable()) thread.join();
    }
};

// ── Main ───────────────────────────────────────────────────────────────────────
int main(int argc, char** argv) {
    TerminalUI ui;

    if (argc < 2) { print_usage(argv[0]); return 1; }

    Config cfg = parse_args(argc, argv);

    if (cfg.source_file.empty()) {
        ui.print_error("No source file specified.");
        return 1;
    }
    if (cfg.api_key.empty()) {
        ui.print_error("ANTHROPIC_API_KEY environment variable not set.");
        return 1;
    }

    ui.print_banner();
    std::cout << "  Compiling " << cfg.source_file << "...\n\n";

    // ── Step 1: Compile and capture errors ──────────────────────────────────
    CompilerRunner runner(cfg);
    auto compile_out = runner.run(cfg.source_file);

    if (compile_out.success()) {
        std::cout << "\033[32m  ✓ No errors found — file compiles cleanly.\033[0m\n\n";
        return 0;
    }

    // ── Step 2: Parse errors ─────────────────────────────────────────────────
    ErrorParser parser;
    auto diagnostics = parser.parse(compile_out.stderr_text);

    if (diagnostics.empty()) {
        std::cout << "  No parseable errors found. Raw output:\n";
        std::cout << compile_out.stderr_text << "\n";
        return 1;
    }

    // ── Step 3: Enrich with source context ───────────────────────────────────
    ContextExtractor extractor;
    extractor.enrich(diagnostics, cfg.source_file);

    std::string full_source;
    try { full_source = ContextExtractor::read_file(cfg.source_file); }
    catch (...) {}

    std::cout << "  Found \033[1m\033[31m" << diagnostics.size()
              << " error" << (diagnostics.size() != 1 ? "s" : "")
              << "\033[0m. Asking Claude to fix them...\n";

    // ── Step 4: For each error, get fix + apply ───────────────────────────────
    LLMClient    llm(cfg);
    PatchApplier patcher(cfg);

    int fixed = 0;
    int idx   = 0;

    for (auto& diag : diagnostics) {
        ++idx;
        ui.print_diagnostic(diag, idx, (int)diagnostics.size());

        Fix fix;
        try {
            Spinner spinner(ui);
            fix = llm.get_fix(diag, full_source);
            spinner.stop();
        } catch (const std::exception& e) {
            ui.print_failed(std::string("LLM call failed: ") + e.what());
            continue;
        }

        ui.print_explanation(fix);
        ui.print_patch(fix);

        bool do_apply = cfg.auto_apply || ui.prompt_apply();

        if (!do_apply) {
            std::cout << "  Skipped.\n\n";
            continue;
        }

        // Back up file before first patch
        if (idx == 1) patcher.backup(cfg.source_file);

        auto result = patcher.apply_and_verify(fix, cfg.source_file);

        if (!result.success) {
            ui.print_failed("Could not apply patch: " + result.error_message);
            continue;
        }

        if (result.compiles) {
            ui.print_verified(result.compile_output);
            // Update full_source for subsequent patches
            full_source = result.patched_source;
            ++fixed;
        } else {
            ui.print_failed("Patch applied but file still doesn't compile:\n" +
                             result.compile_output);
        }
    }

    ui.print_summary(fixed, (int)diagnostics.size());
    return fixed == (int)diagnostics.size() ? 0 : 1;
}
