#pragma once
#include <string>
#include <vector>
#include <optional>

struct Diagnostic {
    enum class Severity { Note, Warning, Error, Fatal };
    Severity    severity;
    std::string file;
    int         line   = 0;
    int         column = 0;
    std::string message;
    std::string code;
    std::vector<std::string> context_lines;
    int context_start_line = 0;
    std::vector<std::string> notes;
};

struct Fix {
    std::string explanation;
    std::string patch;
    std::string patch_summary;
    bool        verified = false;
};

struct FixResult {
    Diagnostic  diagnostic;
    Fix         fix;
    std::string recompile_output;
};

struct Config {
    std::string api_key;
    std::string model = "claude-sonnet-4-20250514";
    std::string compiler = "clang++";
    std::vector<std::string> compiler_flags;
    std::string source_file;
    bool        auto_apply = false;
    bool        verbose    = false;
};
