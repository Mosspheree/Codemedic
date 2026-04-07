#pragma once
#include <string>
#include <vector>
#include <optional>

// ── Diagnostic ─────────────────────────────────────────────────────────────────
struct Diagnostic {
    enum class Severity { Note, Warning, Error, Fatal };

    std::string file;
    int         line   = 0;
    int         column = 0;
    Severity    severity = Severity::Error;
    std::string message;
    std::string code;   // e.g. "-Wunused-variable"

    // Source context (filled by ContextExtractor)
    std::vector<std::string> context_lines;
    int context_start_line = 0;

    // Notes from the compiler (lines following the main diagnostic)
    std::vector<std::string> notes;
};

// ── Fix ────────────────────────────────────────────────────────────────────────
struct Fix {
    std::string explanation;    // Plain-English explanation
    std::string patch_summary;  // One-line description of the patch
    std::string patch;          // Unified diff
};

// ── PatchResult ────────────────────────────────────────────────────────────────
struct PatchResult {
    bool        success        = false;
    bool        compiles       = false;
    std::string error_message;
    std::string compile_output;
    std::string patched_source;  // Content of file after patch (for chaining)
};

// ── CompileOutput ──────────────────────────────────────────────────────────────
struct CompileOutput {
    int         exit_code   = -1;
    std::string stdout_text;
    std::string stderr_text;

    bool success() const { return exit_code == 0; }
};

// ── Config ─────────────────────────────────────────────────────────────────────
struct Config {
    std::string source_file;
    std::string api_key;
    std::string compiler    = "clang++";
    std::string model       = "claude-sonnet-4-20250514";

    std::vector<std::string> compiler_flags;

    bool auto_apply   = false;  // -y: apply all patches without prompting
    bool verbose      = false;  // -v: extra output
    bool fix_warnings = false;  // -w: also fix warnings
    bool explain_only = false;  // -e: explain errors, don't patch
    bool batch_mode   = false;  // -b: process entire directory
    bool git_commit   = false;  // -g: auto-commit verified fixes to git
};
