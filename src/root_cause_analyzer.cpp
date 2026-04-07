#include "root_cause_analyzer.h"
#include <regex>
#include <algorithm>
#include <sstream>

// ── Symbol extraction patterns ─────────────────────────────────────────────────

static const std::vector<std::pair<std::regex, int>> kSymbolPatterns = {
    // 'foo' was not declared in this scope  →  foo
    { std::regex(R"('(\w+)' was not declared)"), 1 },
    // use of undeclared identifier 'foo'  →  foo
    { std::regex(R"(use of undeclared identifier '(\w+)')"), 1 },
    // unknown type name 'Foo'  →  Foo
    { std::regex(R"(unknown type name '(\w+)')"), 1 },
    // no member named 'foo' in 'Bar'  →  foo
    { std::regex(R"(no member named '(\w+)')"), 1 },
    // 'Foo' does not name a type  →  Foo
    { std::regex(R"('(\w+)' does not name a type)"), 1 },
    // cannot open source file "foo.h"  →  foo.h
    { std::regex(R"(cannot open source file "(.+?)")"), 1 },
    // No such file or directory 'foo.h'  →  foo.h
    { std::regex(R"('(.+?)': No such file or directory)"), 1 },
    // fatal error: foo.h: No such file  →  foo.h
    { std::regex(R"(fatal error: (.+?): No such file)"), 1 },
};

std::string RootCauseAnalyzer::extract_symbol(const std::string& message) const {
    for (auto& [pattern, group] : kSymbolPatterns) {
        std::smatch m;
        if (std::regex_search(message, m, pattern) && m.size() > (size_t)group)
            return m[group].str();
    }
    // Fallback: first quoted token
    std::smatch m;
    if (std::regex_search(message, m, std::regex(R"('(\w+)')")))
        return m[1].str();
    return "";
}

bool RootCauseAnalyzer::is_undeclared_error(const std::string& msg) const {
    static const std::vector<std::string> patterns = {
        "was not declared", "undeclared identifier", "undeclared",
        "is not declared", "not declared in this scope"
    };
    for (auto& p : patterns)
        if (msg.find(p) != std::string::npos) return true;
    return false;
}

bool RootCauseAnalyzer::is_type_error(const std::string& msg) const {
    static const std::vector<std::string> patterns = {
        "unknown type", "does not name a type", "no type named",
        "incomplete type", "is not a type"
    };
    for (auto& p : patterns)
        if (msg.find(p) != std::string::npos) return true;
    return false;
}

bool RootCauseAnalyzer::is_include_error(const std::string& msg) const {
    static const std::vector<std::string> patterns = {
        "No such file or directory", "cannot open source file",
        "file not found", "no such file"
    };
    for (auto& p : patterns)
        if (msg.find(p) != std::string::npos) return true;
    return false;
}

bool RootCauseAnalyzer::is_cascade(const Diagnostic& root,
                                     const Diagnostic& candidate) const {
    // An include error causes ALL undeclared/type errors in same file
    if (is_include_error(root.message) &&
        (is_undeclared_error(candidate.message) || is_type_error(candidate.message)))
        return true;

    // Same missing symbol appearing again
    std::string root_sym = extract_symbol(root.message);
    if (!root_sym.empty()) {
        std::string cand_sym = extract_symbol(candidate.message);
        if (root_sym == cand_sym && candidate.line > root.line)
            return true;
    }

    // Type error cascades: if a type is unknown, any member access on it is cascade
    if (is_type_error(root.message)) {
        std::string root_sym2 = extract_symbol(root.message);
        if (!root_sym2.empty() && candidate.message.find(root_sym2) != std::string::npos
            && candidate.line > root.line)
            return true;
    }

    return false;
}

RootCauseAnalyzer::AnalysisResult
RootCauseAnalyzer::analyze(const std::vector<Diagnostic>& diagnostics) const {
    AnalysisResult result;
    result.total_errors = (int)diagnostics.size();
    result.suppressed = 0;

    if (diagnostics.empty()) return result;

    // Single error - trivially a root cause
    if (diagnostics.size() == 1) {
        result.root_causes = diagnostics;
        return result;
    }

    // Sort by file, then line (process in order)
    std::vector<Diagnostic> sorted = diagnostics;
    std::sort(sorted.begin(), sorted.end(), [](const Diagnostic& a, const Diagnostic& b) {
        if (a.file != b.file) return a.file < b.file;
        return a.line < b.line;
    });

    // Mark which diagnostics are cascade dependents
    std::vector<bool> is_dependent(sorted.size(), false);

    for (size_t i = 0; i < sorted.size(); ++i) {
        if (is_dependent[i]) continue;
        for (size_t j = i + 1; j < sorted.size(); ++j) {
            if (!is_dependent[j] && is_cascade(sorted[i], sorted[j])) {
                is_dependent[j] = true;
            }
        }
    }

    // Build cluster map (symbol → lines where it appears)
    for (auto& d : sorted) {
        std::string sym = extract_symbol(d.message);
        if (!sym.empty())
            result.clusters[sym].push_back(d.line);
    }

    // Separate roots from dependents
    for (size_t i = 0; i < sorted.size(); ++i) {
        if (is_dependent[i]) {
            result.dependents.push_back(sorted[i]);
            ++result.suppressed;
        } else {
            result.root_causes.push_back(sorted[i]);
        }
    }

    return result;
}

std::string RootCauseAnalyzer::AnalysisResult::summary() const {
    if (suppressed == 0) return "";
    std::ostringstream ss;
    ss << "  \033[33m⚡ Root cause analysis: " << root_causes.size()
       << " root cause" << (root_causes.size() != 1 ? "s" : "")
       << " identified from " << total_errors << " total errors. "
       << suppressed << " likely cascade error"
       << (suppressed != 1 ? "s" : "")
       << " will be suppressed — fixing root causes should eliminate them.\033[0m\n";
    return ss.str();
}
