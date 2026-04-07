#include "root_cause_analyzer.h"
#include <regex>
#include <algorithm>
#include <sstream>

struct Pattern { const char* pat; int group; };

static const Pattern kSymbolPatterns[] = {
    { "'(\\w+)' was not declared", 1 },
    { "use of undeclared identifier", 1 },
    { "unknown type name", 1 },
    { "no member named", 1 },
    { "does not name a type", 1 },
    { "No such file or directory", 0 },
    { "file not found", 0 },
};

std::string RootCauseAnalyzer::extract_symbol(const std::string& message) const {
    for (auto& p : kSymbolPatterns) {
        std::regex re(p.pat);
        std::smatch m;
        if (std::regex_search(message, m, re)) {
            if (p.group > 0 && m.size() > (size_t)p.group)
                return m[p.group].str();
            return message.substr(0, 40);
        }
    }
    std::smatch m;
    if (std::regex_search(message, m, std::regex("'(\\w+)'")))
        return m[1].str();
    return "";
}

bool RootCauseAnalyzer::is_undeclared_error(const std::string& msg) const {
    return msg.find("was not declared") != std::string::npos
        || msg.find("undeclared identifier") != std::string::npos
        || msg.find("not declared in this scope") != std::string::npos;
}

bool RootCauseAnalyzer::is_type_error(const std::string& msg) const {
    return msg.find("unknown type") != std::string::npos
        || msg.find("does not name a type") != std::string::npos
        || msg.find("incomplete type") != std::string::npos;
}

bool RootCauseAnalyzer::is_include_error(const std::string& msg) const {
    return msg.find("No such file or directory") != std::string::npos
        || msg.find("file not found") != std::string::npos
        || msg.find("fatal error") != std::string::npos;
}

bool RootCauseAnalyzer::is_cascade(const Diagnostic& root,
                                    const Diagnostic& candidate) const {
    if (is_include_error(root.message) &&
        (is_undeclared_error(candidate.message) || is_type_error(candidate.message)))
        return true;
    std::string root_sym = extract_symbol(root.message);
    if (!root_sym.empty()) {
        std::string cand_sym = extract_symbol(candidate.message);
        if (root_sym == cand_sym && candidate.line > root.line)
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
    if (diagnostics.size() == 1) { result.root_causes = diagnostics; return result; }
    std::vector<Diagnostic> sorted = diagnostics;
    std::sort(sorted.begin(), sorted.end(), [](const Diagnostic& a, const Diagnostic& b) {
        if (a.file != b.file) return a.file < b.file;
        return a.line < b.line;
    });
    std::vector<bool> is_dependent(sorted.size(), false);
    for (size_t i = 0; i < sorted.size(); ++i) {
        if (is_dependent[i]) continue;
        for (size_t j = i + 1; j < sorted.size(); ++j) {
            if (!is_dependent[j] && is_cascade(sorted[i], sorted[j]))
                is_dependent[j] = true;
        }
    }
    for (auto& d : sorted) {
        std::string sym = extract_symbol(d.message);
        if (!sym.empty()) result.clusters[sym].push_back(d.line);
    }
    for (size_t i = 0; i < sorted.size(); ++i) {
        if (is_dependent[i]) { result.dependents.push_back(sorted[i]); ++result.suppressed; }
        else result.root_causes.push_back(sorted[i]);
    }
    return result;
}

std::string RootCauseAnalyzer::AnalysisResult::summary() const {
    if (suppressed == 0) return "";
    std::ostringstream ss;
    ss << "  \033[33m⚡ Root cause analysis: " << root_causes.size()
       << " root cause" << (root_causes.size() != 1 ? "s" : "")
       << " from " << total_errors << " total errors. "
       << suppressed << " cascade error"
       << (suppressed != 1 ? "s" : "") << " suppressed.\033[0m\n";
    return ss.str();
}
