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
#include "session_logger.h"
#include "config_loader.h"

#include <nlohmann/json.hpp>

#include <iostream>
#include <cstdlib>
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;
using json = nlohmann::json;

static const char* VERSION = "3.0.0";

// ── Config helper methods ─────────────────────────────────────────────────────
std::string Config::resolved_url() const {
    if (!provider_url.empty()) return provider_url;
    switch (provider) {
        case LLMProvider::Anthropic: return "https://api.anthropic.com/v1/messages";
        case LLMProvider::OpenAI:    return "https://api.openai.com/v1/chat/completions";
        case LLMProvider::Groq:      return "https://api.groq.com/openai/v1/chat/completions";
        case LLMProvider::Ollama:    return "http://localhost:11434/api/chat";
    }
    return "";
}

std::string Config::resolved_model() const {
    if (!model.empty()) return model;
    switch (provider) {
        case LLMProvider::Anthropic: return "claude-sonnet-4-20250514";
        case LLMProvider::OpenAI:    return "gpt-4o";
        case LLMProvider::Groq:      return "llama-3.3-70b-versatile";
        case LLMProvider::Ollama:    return "llama3";
    }
    return model;
}

std::string Config::provider_name() const {
    switch (provider) {
        case LLMProvider::Anthropic: return "anthropic";
        case LLMProvider::OpenAI:    return "openai";
        case LLMProvider::Groq:      return "groq";
        case LLMProvider::Ollama:    return "ollama";
    }
    return "unknown";
}

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
              << "  -m <model>          LLM model (default: auto per provider)\n"
              << "\n"
              << "  --json              Machine-readable JSON output (for CI/CD)\n"
              << "  --diff              Preview patches as diffs without applying\n"
              << "  --undo              Restore source files from .bak backups\n"
              << "  --provider <name>   LLM provider: anthropic|openai|groq|ollama (default: groq)\n"
              << "  --provider-url <u>  Custom API endpoint URL\n"
              << "  --log <file>        Write fix history to JSON log file\n"
              << "  --config <file>     Path to .codemedic.yaml config file\n"
              << "  --version           Show version information\n"
              << "  -h, --help          Show this help\n"
              << "\n"
              << "Environment variables:\n"
              << "  ANTHROPIC_API_KEY   API key (works for Anthropic/Groq)\n"
              << "  OPENAI_API_KEY      API key for OpenAI provider\n"
              << "  GROQ_API_KEY        API key for Groq provider\n"
              << "\n"
              << "Examples:\n"
              << "  codemedic broken.cpp\n"
              << "  codemedic -y main.cpp -- -std=c++17 -I./include\n"
              << "  codemedic -e broken.cpp                     # explain only, no patching\n"
              << "  codemedic -b ./src/                         # fix all files in directory\n"
              << "  codemedic -y -g main.cpp                    # fix and auto-commit to git\n"
              << "  codemedic --provider anthropic broken.cpp   # use Claude directly\n"
              << "  codemedic --provider ollama broken.cpp      # use local Ollama\n"
              << "  codemedic --json broken.cpp                 # JSON output for CI\n"
              << "  codemedic --diff broken.cpp                 # preview diffs only\n"
              << "  codemedic --undo broken.cpp                 # restore from backup\n"
              << "  codemedic --log fixes.json broken.cpp       # log fix history\n"
              << "  codemedic -c gcc broken.c -- -Wall\n";
}

static void print_version() {
    std::cout << "codemedic v" << VERSION << "\n"
              << "C/C++ compiler error fixer with LLM-powered patching\n"
              << "Built with: libcurl, nlohmann/json\n"
              << "Providers: Anthropic (Claude), OpenAI (GPT), Groq (Llama), Ollama (local)\n";
}

static LLMProvider parse_provider_arg(const std::string& name) {
    std::string lower = name;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    if (lower == "anthropic" || lower == "claude") return LLMProvider::Anthropic;
    if (lower == "openai"    || lower == "gpt")    return LLMProvider::OpenAI;
    if (lower == "groq"      || lower == "llama")  return LLMProvider::Groq;
    if (lower == "ollama"    || lower == "local")   return LLMProvider::Ollama;
    std::cerr << "Unknown provider: " << name << ". Using groq.\n";
    return LLMProvider::Groq;
}

static Config parse_args(int argc, char** argv) {
    Config cfg;

    bool past_dashdash = false;
    bool provider_set = false;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (past_dashdash)                          { cfg.compiler_flags.push_back(arg); continue; }
        if (arg == "--")                            { past_dashdash = true; continue; }
        if (arg == "-h" || arg == "--help")         { print_usage(argv[0]); std::exit(0); }
        if (arg == "--version")                     { cfg.show_version = true; continue; }
        if (arg == "-y" || arg == "--yes")          { cfg.auto_apply    = true; continue; }
        if (arg == "-v" || arg == "--verbose")      { cfg.verbose       = true; continue; }
        if (arg == "-w" || arg == "--warnings")     { cfg.fix_warnings  = true; continue; }
        if (arg == "-e" || arg == "--explain-only") { cfg.explain_only  = true; continue; }
        if (arg == "-b" || arg == "--batch")        { cfg.batch_mode    = true; continue; }
        if (arg == "-g" || arg == "--git")          { cfg.git_commit    = true; continue; }
        if (arg == "--json")                        { cfg.json_output   = true; continue; }
        if (arg == "--diff")                        { cfg.diff_only     = true; continue; }
        if (arg == "--undo")                        { cfg.undo_mode     = true; continue; }
        if (arg == "-c" && i+1 < argc)              { cfg.compiler      = argv[++i]; continue; }
        if (arg == "-m" && i+1 < argc)              { cfg.model         = argv[++i]; continue; }
        if (arg == "--provider" && i+1 < argc)      { cfg.provider = parse_provider_arg(argv[++i]); provider_set = true; continue; }
        if (arg == "--provider-url" && i+1 < argc)  { cfg.provider_url  = argv[++i]; continue; }
        if (arg == "--log" && i+1 < argc)           { cfg.log_file      = argv[++i]; continue; }
        if (arg == "--config" && i+1 < argc)        { cfg.config_file   = argv[++i]; continue; }
        if (arg[0] != '-')                          { cfg.source_file   = arg; continue; }
    }

    // Load config file (CLI flags override file values)
    Config file_cfg = cfg;  // Start with CLI-parsed values
    if (ConfigLoader::load(file_cfg, cfg.config_file)) {
        // Only apply file values for fields not set on CLI
        if (cfg.compiler == "clang++" && file_cfg.compiler != "clang++")
            cfg.compiler = file_cfg.compiler;
        if (!provider_set)
            cfg.provider = file_cfg.provider;
        if (cfg.provider_url.empty())
            cfg.provider_url = file_cfg.provider_url;
        if (cfg.log_file.empty())
            cfg.log_file = file_cfg.log_file;
    }

    // Resolve API key from environment based on provider
    auto env_or = [](const char* name, const std::string& fallback) -> std::string {
        const char* v = std::getenv(name);
        return v ? v : fallback;
    };

    switch (cfg.provider) {
        case LLMProvider::Anthropic:
            cfg.api_key = env_or("ANTHROPIC_API_KEY", "");
            break;
        case LLMProvider::OpenAI:
            cfg.api_key = env_or("OPENAI_API_KEY", env_or("ANTHROPIC_API_KEY", ""));
            break;
        case LLMProvider::Groq:
            cfg.api_key = env_or("GROQ_API_KEY", env_or("ANTHROPIC_API_KEY", ""));
            break;
        case LLMProvider::Ollama:
            cfg.api_key = "not-needed";  // Ollama runs locally
            break;
    }

    return cfg;
}

// ── Undo mode ─────────────────────────────────────────────────────────────────
static int run_undo(const Config& cfg, const TerminalUI& ui) {
    std::string bak = cfg.source_file + ".bak";
    if (!fs::exists(bak)) {
        ui.print_error("No backup file found: " + bak);
        return 1;
    }

    try {
        fs::copy_file(bak, cfg.source_file, fs::copy_options::overwrite_existing);
        fs::remove(bak);
    } catch (const fs::filesystem_error& e) {
        ui.print_error(std::string("Restore failed: ") + e.what());
        return 1;
    }

    if (cfg.json_output) {
        json out = {
            {"action", "undo"},
            {"file", cfg.source_file},
            {"backup", bak},
            {"success", true}
        };
        std::cout << out.dump(2) << "\n";
    } else {
        std::cout << "\033[32m  Restored " << cfg.source_file
                  << " from backup.\033[0m\n";
        std::cout << "  Backup file removed: " << bak << "\n\n";
    }
    return 0;
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

// ── JSON output builder ───────────────────────────────────────────────────────
static json diagnostic_to_json(const Diagnostic& d) {
    std::string sev = "error";
    switch (d.severity) {
        case Diagnostic::Severity::Note:    sev = "note"; break;
        case Diagnostic::Severity::Warning: sev = "warning"; break;
        case Diagnostic::Severity::Fatal:   sev = "fatal"; break;
        default: break;
    }
    return {
        {"file",     d.file},
        {"line",     d.line},
        {"column",   d.column},
        {"severity", sev},
        {"message",  d.message},
        {"code",     d.code}
    };
}

static json fix_to_json(const Fix& f) {
    return {
        {"explanation",   f.explanation},
        {"patch_summary", f.patch_summary},
        {"patch",         f.patch}
    };
}

// ── Single-file fix pipeline ───────────────────────────────────────────────────
static int run_single_file(const Config& cfg, const TerminalUI& ui) {
    json json_out;
    if (cfg.json_output) {
        json_out["file"] = cfg.source_file;
        json_out["provider"] = cfg.provider_name();
        json_out["model"] = cfg.resolved_model();
    }

    if (!cfg.json_output) {
        ui.print_banner();
        if (cfg.explain_only)
            std::cout << "  \033[33m[explain-only mode -- no patches will be applied]\033[0m\n\n";
        if (cfg.diff_only)
            std::cout << "  \033[33m[diff-preview mode -- patches shown but not applied]\033[0m\n\n";
        std::cout << "  Compiling " << cfg.source_file << "...\n";
        std::cout << "  Provider: \033[36m" << cfg.provider_name()
                  << "\033[0m  Model: \033[36m" << cfg.resolved_model() << "\033[0m\n\n";
    }

    // Session logging
    SessionLogger logger(cfg);

    // Step 1: Compile
    CompilerRunner runner(cfg);
    auto compile_out = runner.run(cfg.source_file);

    if (compile_out.success()) {
        if (cfg.json_output) {
            json_out["errors"] = json::array();
            json_out["summary"] = {{"total", 0}, {"fixed", 0}};
            std::cout << json_out.dump(2) << "\n";
        } else {
            std::cout << "\033[32m  No errors found -- file compiles cleanly.\033[0m\n\n";
        }
        return 0;
    }

    // Step 2: Parse
    ErrorParser parser;
    auto diagnostics = parser.parse(compile_out.stderr_text, cfg.fix_warnings);

    if (diagnostics.empty()) {
        if (cfg.json_output) {
            json_out["errors"] = json::array();
            json_out["raw_output"] = compile_out.stderr_text;
            std::cout << json_out.dump(2) << "\n";
        } else {
            std::cout << "  No parseable errors found. Raw output:\n" << compile_out.stderr_text << "\n";
        }
        return 1;
    }

    // Step 3: Root cause analysis
    RootCauseAnalyzer analyzer;
    auto analysis = analyzer.analyze(diagnostics);

    if (!cfg.json_output) {
        std::string analysis_summary = analysis.summary();
        if (!analysis_summary.empty())
            std::cout << analysis_summary << "\n";
    }

    auto& to_fix = analysis.root_causes;

    if (!cfg.json_output) {
        std::cout << "  Found \033[1m\033[31m" << diagnostics.size()
                  << " error" << (diagnostics.size() != 1 ? "s" : "") << "\033[0m";
        if (analysis.suppressed > 0)
            std::cout << " (\033[33m" << to_fix.size() << " root cause"
                      << (to_fix.size() != 1 ? "s" : "") << " to fix\033[0m)";
        std::cout << ". Asking " << cfg.provider_name() << "...\n";
    }

    // Step 4: Enrich with source context
    ContextExtractor extractor;
    extractor.enrich(to_fix, cfg.source_file);

    std::string full_source;
    try { full_source = ContextExtractor::read_file(cfg.source_file); } catch (...) {}

    // Step 5: Check git
    bool in_git = cfg.git_commit && GitIntegration::is_git_repo(cfg.source_file);
    if (!cfg.json_output) {
        if (cfg.git_commit && !in_git)
            std::cout << "  \033[33mNote: not a git repo -- --git flag ignored.\033[0m\n";
        if (in_git)
            std::cout << "  \033[32m  Git integration active (branch: "
                      << GitIntegration::current_branch() << ")\033[0m\n";
    }

    if (!cfg.json_output) std::cout << "\n";

    // Step 6: Process each root cause
    LLMClient    llm(cfg);
    PatchApplier patcher(cfg);

    int fixed = 0;
    int idx   = 0;
    bool backed_up = false;

    json json_errors = json::array();

    for (auto& diag : to_fix) {
        ++idx;
        auto start_time = std::chrono::steady_clock::now();

        if (!cfg.json_output)
            ui.print_diagnostic(diag, idx, (int)to_fix.size());

        Fix fix;
        try {
            if (!cfg.json_output) {
                Spinner spinner(ui);
                fix = llm.get_fix(diag, full_source);
                spinner.stop();
            } else {
                fix = llm.get_fix(diag, full_source);
            }
        } catch (const std::exception& e) {
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - start_time).count();
            if (cfg.json_output) {
                json_errors.push_back({
                    {"diagnostic", diagnostic_to_json(diag)},
                    {"error", e.what()},
                    {"applied", false},
                    {"verified", false}
                });
            } else {
                ui.print_failed(std::string("LLM call failed: ") + e.what());
            }

            FixRecord rec;
            rec.file = diag.file; rec.line = diag.line;
            rec.error_message = diag.message;
            rec.duration_ms = elapsed;
            rec.timestamp = SessionLogger::now_iso8601();
            logger.record_fix(rec);

            continue;
        }

        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start_time).count();

        if (!cfg.json_output) {
            ui.print_explanation(fix);
        }

        // Explain-only mode: print explanation and continue without patching
        if (cfg.explain_only) {
            if (cfg.json_output) {
                json_errors.push_back({
                    {"diagnostic", diagnostic_to_json(diag)},
                    {"fix", fix_to_json(fix)},
                    {"applied", false},
                    {"verified", false},
                    {"mode", "explain-only"}
                });
            } else {
                std::cout << "  \033[90m[explain-only: patch skipped]\033[0m\n\n";
            }

            FixRecord rec;
            rec.file = diag.file; rec.line = diag.line;
            rec.error_message = diag.message;
            rec.explanation = fix.explanation;
            rec.patch_summary = fix.patch_summary;
            rec.duration_ms = elapsed;
            rec.timestamp = SessionLogger::now_iso8601();
            logger.record_fix(rec);

            continue;
        }

        // Diff preview mode: show diff and continue
        if (cfg.diff_only) {
            if (cfg.json_output) {
                json_errors.push_back({
                    {"diagnostic", diagnostic_to_json(diag)},
                    {"fix", fix_to_json(fix)},
                    {"applied", false},
                    {"verified", false},
                    {"mode", "diff-preview"}
                });
            } else {
                ui.print_patch(fix);
                std::cout << "  \033[90m[diff-preview: patch not applied]\033[0m\n\n";
            }

            FixRecord rec;
            rec.file = diag.file; rec.line = diag.line;
            rec.error_message = diag.message;
            rec.explanation = fix.explanation;
            rec.patch_summary = fix.patch_summary;
            rec.duration_ms = elapsed;
            rec.timestamp = SessionLogger::now_iso8601();
            logger.record_fix(rec);

            continue;
        }

        if (!cfg.json_output)
            ui.print_patch(fix);

        bool do_apply = cfg.auto_apply || cfg.json_output || ui.prompt_apply();
        if (!do_apply) {
            if (!cfg.json_output) std::cout << "  Skipped.\n\n";
            continue;
        }

        if (!backed_up) { patcher.backup(cfg.source_file); backed_up = true; }

        auto result = patcher.apply_and_verify(fix, cfg.source_file);

        FixRecord rec;
        rec.file = diag.file; rec.line = diag.line;
        rec.error_message = diag.message;
        rec.explanation = fix.explanation;
        rec.patch_summary = fix.patch_summary;
        rec.applied = result.success;
        rec.verified = result.compiles;
        rec.duration_ms = elapsed;
        rec.timestamp = SessionLogger::now_iso8601();
        logger.record_fix(rec);

        if (!result.success) {
            if (cfg.json_output) {
                json_errors.push_back({
                    {"diagnostic", diagnostic_to_json(diag)},
                    {"fix", fix_to_json(fix)},
                    {"applied", false},
                    {"verified", false},
                    {"error", result.error_message}
                });
            } else {
                ui.print_failed("Could not apply patch: " + result.error_message);
            }
            continue;
        }

        if (result.compiles) {
            if (!cfg.json_output)
                ui.print_verified(result.compile_output);
            full_source = result.patched_source;
            ++fixed;

            if (cfg.json_output) {
                json_errors.push_back({
                    {"diagnostic", diagnostic_to_json(diag)},
                    {"fix", fix_to_json(fix)},
                    {"applied", true},
                    {"verified", true}
                });
            }

            // Git auto-commit
            if (in_git) {
                bool committed = GitIntegration::commit_fix(
                    cfg.source_file, diag.message, fix.patch_summary);
                if (!cfg.json_output) {
                    if (committed)
                        std::cout << "  \033[32m  Committed: " << fix.patch_summary << "\033[0m\n\n";
                    else
                        std::cout << "  \033[33m  Git commit skipped\033[0m\n\n";
                }
            }

            if (cfg.verbose && in_git && !cfg.json_output) {
                std::string diff = GitIntegration::show_diff(cfg.source_file);
                if (!diff.empty())
                    std::cout << "  \033[90m" << diff << "\033[0m\n";
            }
        } else {
            if (cfg.json_output) {
                json_errors.push_back({
                    {"diagnostic", diagnostic_to_json(diag)},
                    {"fix", fix_to_json(fix)},
                    {"applied", true},
                    {"verified", false},
                    {"compile_error", result.compile_output}
                });
            } else {
                ui.print_failed("Patch applied but file still doesn't compile:\n" +
                                 result.compile_output);
            }
        }
    }

    // Summary
    if (cfg.json_output) {
        json_out["errors"] = json_errors;
        json_out["summary"] = {
            {"total_errors",    (int)diagnostics.size()},
            {"root_causes",     (int)to_fix.size()},
            {"suppressed",      analysis.suppressed},
            {"fixed",           fixed}
        };
        std::cout << json_out.dump(2) << "\n";
    } else {
        if (analysis.suppressed > 0 && fixed > 0) {
            std::cout << "  \033[33m " << analysis.suppressed
                      << " cascade error" << (analysis.suppressed != 1 ? "s" : "")
                      << " suppressed -- recompile to check if resolved.\033[0m\n\n";
        }
        ui.print_summary(fixed, (int)to_fix.size());
    }

    // Write session log
    logger.set_session_end();
    if (!cfg.log_file.empty()) {
        if (logger.write_log()) {
            if (!cfg.json_output)
                std::cout << "  \033[90mSession log written to: " << cfg.log_file << "\033[0m\n\n";
        }
    }

    return fixed == (int)to_fix.size() ? 0 : 1;
}

// ── Main ───────────────────────────────────────────────────────────────────────
int main(int argc, char** argv) {
    TerminalUI ui;

    if (argc < 2) { print_usage(argv[0]); return 1; }

    Config cfg = parse_args(argc, argv);

    // Version flag
    if (cfg.show_version) {
        print_version();
        return 0;
    }

    if (cfg.source_file.empty()) {
        ui.print_error("No source file or directory specified.");
        return 1;
    }

    // Undo mode
    if (cfg.undo_mode)
        return run_undo(cfg, ui);

    // API key check (Ollama doesn't need one)
    if (cfg.api_key.empty() && cfg.provider != LLMProvider::Ollama) {
        std::string key_name;
        switch (cfg.provider) {
            case LLMProvider::Anthropic: key_name = "ANTHROPIC_API_KEY"; break;
            case LLMProvider::OpenAI:    key_name = "OPENAI_API_KEY"; break;
            case LLMProvider::Groq:      key_name = "GROQ_API_KEY or ANTHROPIC_API_KEY"; break;
            default: key_name = "API_KEY"; break;
        }
        ui.print_error(key_name + " environment variable not set.");
        return 1;
    }

    // Batch mode
    if (cfg.batch_mode || fs::is_directory(cfg.source_file)) {
        if (!cfg.json_output) ui.print_banner();
        BatchProcessor bp(cfg);
        auto summary = bp.run(cfg.source_file);

        if (cfg.git_commit && summary.total_fixed > 0) {
            std::vector<std::string> fixed_files;
            for (auto& r : summary.results)
                if (r.errors_fixed > 0) fixed_files.push_back(r.filepath);
            if (!fixed_files.empty() && GitIntegration::is_git_repo(fixed_files[0])) {
                GitIntegration::commit_batch(fixed_files, summary.total_fixed, summary.total_errors);
                if (!cfg.json_output)
                    std::cout << "  \033[32m  Batch changes committed to git.\033[0m\n\n";
            }
        }

        return summary.files_fully_fixed == summary.files_with_errors ? 0 : 1;
    }

    return run_single_file(cfg, ui);
}
