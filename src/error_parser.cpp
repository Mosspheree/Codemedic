#include "error_parser.h"
#include <regex>
#include <sstream>
#include <algorithm>

// Clang format:  file.cpp:10:5: error: message [-Wflag]
// GCC format:    file.cpp:10:5: error: message
static const std::regex kDiagPattern(
    R"(^(.+?):(\d+):(\d+):\s+(note|warning|error|fatal error):\s+(.+?)(?:\s+\[(.+?)\])?$)"
);

Diagnostic::Severity ErrorParser::parse_severity(const std::string& s) const {
    if (s == "note")         return Diagnostic::Severity::Note;
    if (s == "warning")      return Diagnostic::Severity::Warning;
    if (s == "fatal error")  return Diagnostic::Severity::Fatal;
    return Diagnostic::Severity::Error;
}

std::optional<Diagnostic> ErrorParser::try_parse_line(const std::string& line) const {
    std::smatch m;
    if (!std::regex_match(line, m, kDiagPattern)) return std::nullopt;

    Diagnostic d;
    d.file     = m[1].str();
    d.line     = std::stoi(m[2].str());
    d.column   = std::stoi(m[3].str());
    d.severity = parse_severity(m[4].str());
    d.message  = m[5].str();
    if (m[6].matched) d.code = m[6].str();
    return d;
}

std::vector<Diagnostic> ErrorParser::parse(const std::string& compiler_output,
                                            bool include_warnings) const {
    std::vector<Diagnostic> results;
    std::istringstream stream(compiler_output);
    std::string line;

    Diagnostic* current = nullptr;

    while (std::getline(stream, line)) {
        auto diag = try_parse_line(line);
        if (!diag) {
            // Could be a continuation note or context — attach to current diag
            if (current && !line.empty())
                current->notes.push_back(line);
            continue;
        }

        if (diag->severity == Diagnostic::Severity::Note) {
            if (current) current->notes.push_back(diag->message);
            continue;
        }

        if (!include_warnings && diag->severity == Diagnostic::Severity::Warning)
            continue;

        results.push_back(std::move(*diag));
        current = &results.back();
    }

    // Deduplicate — clang sometimes emits the same error twice
    results.erase(
        std::unique(results.begin(), results.end(), [](const Diagnostic& a, const Diagnostic& b) {
            return a.file == b.file && a.line == b.line && a.message == b.message;
        }),
        results.end()
    );

    return results;
}
