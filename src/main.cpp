#include "types.h"
#include "compiler_runner.h"
#include "error_parser.h"
#include "context_extractor.h"
#include "llm_client.h"
#include "patch_applier.h"
#include "terminal_ui.h"
#include "root_cause_analyzer.h"
#include "git_integration.h"
#include "batch_processor.h"

#include <iostream>
#include <cstdlib>
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <chrono>
#include <filesystem>

namespace fs = std::filesystem;

// ── CLI argument parsing ───────────────────────────────────────────────────────
static void print_usage(const char* prog) {
    std::cerr << "Usage: " << prog << " [options] <source_file|directory> [-- compiler_flags...]\n"
              << "\n"
              << "Options:\n"
              << "  -y, --yes           Auto-apply patches without prompting\n"
              << "  -w, --warnings      Also fix warnings\n"
              << "  -v, --verbose       Verbose output\n"
              << "  -e, --explain-only  Explain errors without applying patches\n"
              << "  -b, --batch         Batch mode: fix all .cpp/.c files in directory\n"
              << "  -g, --git           Auto-commit verified fixes to git\n"
              << "  -c <compiler>       Compiler to use (default: clang++)\n"
              << "  -m <model>          LLM model (default: claude-sonnet-4-20250514)\n"
              << "  -h, --help          Show this help\n"
              << "\n"
              << "Environment variables:\n"
              << "  ANTHROPIC_API_KEY   Your Anthropic API key (required)\n"
              << "\n"
              << "Examples:\n"
              << "  codemedic broken.cpp\n"
              << "  codemedic -y main.cpp -- -std=c++17 -I./include\n"
              << "  codemedic -e broken.cpp                  # explain only, no patching\n"
              << "  codemedic -b ./src/                      # fix all files in directory\n"
              << "  codemedic -y -g main.cpp                 # fix and auto-commit to git\n"
              << "  codemedic -c gcc broken.c -- -Wall\n";
}

static Config parse_args(int argc, char** argv) {
    Config cfg;
    cfg.api_key = std::getenv("ANTHROPIC_API_KEY") ? std::getenv("ANTHROPIC_API_KEY") : "";

    bool past_dashdash = false;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (past_dashdash)                          { cfg.compiler_flags.push_back(arg); continue; }
        if (arg == "--")                            { past_dashdash = true; continue; }
        if (arg == "-h" || arg == "--help")         { print_usage(argv[0]); std::exit(0); }
        if (arg == "-y" || arg == "--yes")          { cfg.auto_apply    = true; continue; }
        if (arg == "-v" || arg == "--verbose")      { cfg.verbose       = true; continue; }
        if (arg == "-w" || arg == "--warnings")     { cfg.fix_warnings  = true; continue; }
        if (arg == "-e" || arg == "--explain-only") { cfg.explain_only  = true; continue; }
        if (arg == "-b" || arg == "--batch")        { cfg.batch_mode    = true; continue; }
        if (arg == "-g" || arg == "--git")          { cfg.git_commit    = true; continue; }
        if (arg == "-c" && i+1 < argc)              { cfg.compiler      = argv[++i]; continue; }
        if (arg == "-m" && i+1 < argc)              { cfg.model         = argv[++i]; continue; }
        if (arg[0] != '-')                          { cfg.source_file   = arg; continue; }
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

// ── Single-file fix pipeline ───────────────────────────────────────────────────
static int run_single_file(const Config& cfg, const TerminalUI& ui) {
    ui.print_banner();

    if (cfg.explain_only)
        std::cout << "  \033[33m[explain-only mode — no patches will be applied]\033[0m\n\n";

    std::cout << "  Compiling " << cfg.source_file << "...\n\n";

    // Step 1: Compile
    CompilerRunner runner(cfg);
    auto compile_out = runner.run(cfg.source_file);

    if (compile_out.success()) {
        std::cout << "\033[32m  ✓ No errors found — file compiles cleanly.\033[0m\n\n";
        return 0;
    }

    // Step 2: Parse
    ErrorParser parser;
    auto diagnostics = parser.parse(compile_out.stderr_text, cfg.fix_warnings);

    if (diagnostics.empty()) {
        std::cout << "  No parseable errors found. Raw output:\n" << compile_out.stderr_text << "\n";
        return 1;
    }

    // Step 3: Root cause analysis
    RootCauseAnalyzer analyzer;
    auto analysis = analyzer.analyze(diagnostics);

    std::string analysis_summary = analysis.summary();
    if (!analysis_summary.empty())
        std::cout << analysis_summary << "\n";

    auto& to_fix = analysis.root_causes;

    std::cout << "  Found \033[1m\033[31m" << diagnostics.size()
              << " error" << (diagnostics.size() != 1 ? "s" : "") << "\033[0m";

    if (analysis.suppressed > 0)
        std::cout << " (\033[33m" << to_fix.size() << " root cause"
                  << (to_fix.size() != 1 ? "s" : "") << " to fix\033[0m)";

    std::cout << ". Asking Claude...\n";

    // Step 4: Enrich with source context
    ContextExtractor extractor;
    extractor.enrich(to_fix, cfg.source_file);

    std::string full_source;
    try { full_source = ContextExtractor::read_file(cfg.source_file); } catch (...) {}

    // Step 5: Check git
    bool in_git = cfg.git_commit && GitIntegration::is_git_repo(cfg.source_file);
    if (cfg.git_commit && !in_git)
        std::cout << "  \033[33mNote: not a git repo — --git flag ignored.\033[0m\n";
    if (in_git)
        std::cout << "  \033[32m⎇  Git integration active (branch: "
                  << GitIntegration::current_branch() << ")\033[0m\n";

    std::cout << "\n";

    // Step 6: Process each root cause
    LLMClient    llm(cfg);
    PatchApplier patcher(cfg);

    int fixed = 0;
    int idx   = 0;
    bool backed_up = false;

    for (auto& diag : to_fix) {
        ++idx;
        ui.print_diagnostic(diag, idx, (int)to_fix.size());

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

        // Explain-only mode: print explanation and continue without patching
        if (cfg.explain_only) {
            std::cout << "  \033[90m[explain-only: patch skipped]\033[0m\n\n";
            continue;
        }

        ui.print_patch(fix);

        bool do_apply = cfg.auto_apply || ui.prompt_apply();
        if (!do_apply) { std::cout << "  Skipped.\n\n"; continue; }

        if (!backed_up) { patcher.backup(cfg.source_file); backed_up = true; }

        auto result = patcher.apply_and_verify(fix, cfg.source_file);

        if (!result.success) {
            ui.print_failed("Could not apply patch: " + result.error_message);
            continue;
        }

        if (result.compiles) {
            ui.print_verified(result.compile_output);
            full_source = result.patched_source;
            ++fixed;

            // Git auto-commit
            if (in_git) {
                bool committed = GitIntegration::commit_fix(
                    cfg.source_file, diag.message, fix.patch_summary);
                if (committed)
                    std::cout << "  \033[32m⎇  Committed: " << fix.patch_summary << "\033[0m\n\n";
                else
                    std::cout << "  \033[33m⎇  Git commit skipped\033[0m\n\n";
            }

            if (cfg.verbose && in_git) {
                std::string diff = GitIntegration::show_diff(cfg.source_file);
                if (!diff.empty())
                    std::cout << "  \033[90m" << diff << "\033[0m\n";
            }
        } else {
            ui.print_failed("Patch applied but file still doesn't compile:\n" +
                             result.compile_output);
        }
    }

    if (analysis.suppressed > 0 && fixed > 0) {
        std::cout << "  \033[33m⚡ " << analysis.suppressed
                  << " cascade error" << (analysis.suppressed != 1 ? "s" : "")
                  << " suppressed — recompile to check if resolved.\033[0m\n\n";
    }

    ui.print_summary(fixed, (int)to_fix.size());
    return fixed == (int)to_fix.size() ? 0 : 1;
}

// ── Main ───────────────────────────────────────────────────────────────────────
int main(int argc, char** argv) {
    TerminalUI ui;

    if (argc < 2) { print_usage(argv[0]); return 1; }

    Config cfg = parse_args(argc, argv);

    if (cfg.source_file.empty()) {
        ui.print_error("No source file or directory specified.");
        return 1;
    }
    if (cfg.api_key.empty()) {
        ui.print_error("ANTHROPIC_API_KEY environment variable not set.");
        return 1;
    }

    // Batch mode
    if (cfg.batch_mode || fs::is_directory(cfg.source_file)) {
        ui.print_banner();
        BatchProcessor bp(cfg);
        auto summary = bp.run(cfg.source_file);

        if (cfg.git_commit && summary.total_fixed > 0) {
            std::vector<std::string> fixed_files;
            for (auto& r : summary.results)
                if (r.errors_fixed > 0) fixed_files.push_back(r.filepath);
            if (GitIntegration::is_git_repo(fixed_files[0])) {
                GitIntegration::commit_batch(fixed_files, summary.total_fixed, summary.total_errors);
                std::cout << "  \033[32m⎇  Batch changes committed to git.\033[0m\n\n";
            }
        }

        return summary.files_fully_fixed == summary.files_with_errors ? 0 : 1;
    }

    return run_single_file(cfg, ui);
}
