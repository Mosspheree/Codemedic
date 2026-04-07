#pragma once
#include "types.h"
#include <vector>
#include <unordered_map>
#include <string>

/**
 * RootCauseAnalyzer
 *
 * Many compilers emit dozens of errors when there are really only 1-3 root
 * causes. For example, a missing #include can cascade into 50+ "undeclared
 * identifier" errors all pointing at different lines but sharing the same
 * root cause.
 *
 * This class builds a dependency graph of diagnostics and identifies which
 * errors are root causes vs. cascading symptoms. Fixing root causes first
 * often eliminates the rest automatically on recompile.
 *
 * Algorithm:
 *   1. Group errors by message pattern (undeclared X, unknown type X, etc.)
 *   2. Cluster errors that share the same "missing symbol" or "unknown type"
 *   3. Mark the earliest-line occurrence as the root; rest as dependents
 *   4. Return root causes sorted by line number (fix order matters)
 */
class RootCauseAnalyzer {
public:
    struct AnalysisResult {
        std::vector<Diagnostic> root_causes;      // Errors to actually fix
        std::vector<Diagnostic> dependents;       // Likely cascade from roots
        std::unordered_map<std::string, std::vector<int>> clusters; // symbol -> line numbers
        int total_errors;
        int suppressed;  // dependents not shown

        std::string summary() const;
    };

    AnalysisResult analyze(const std::vector<Diagnostic>& diagnostics) const;

private:
    // Extract the "key symbol" from an error message (the thing that's missing/wrong)
    std::string extract_symbol(const std::string& message) const;

    // Determine if error B is likely a cascade of error A
    bool is_cascade(const Diagnostic& root, const Diagnostic& candidate) const;

    // Pattern matching for common cascade error types
    bool is_undeclared_error(const std::string& msg) const;
    bool is_type_error(const std::string& msg) const;
    bool is_include_error(const std::string& msg) const;
};
