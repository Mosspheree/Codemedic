#include "batch_processor.h"
#include "compiler_runner.h"
#include "error_parser.h"
#include "context_extractor.h"
#include "llm_client.h"
#include "patch_applier.h"
#include "root_cause_analyzer.h"
#include "terminal_ui.h"

#include <filesystem>
#include <iostream>
#include <algorithm>

namespace fs = std::filesystem;

const std::vector<std::string> BatchProcessor::kExtensions = {
    ".cpp", ".cxx", ".cc", ".c", ".C"
};

std::vector<std::string>
BatchProcessor::find_source_files(const std::string& dir) const {
    std::vector<std::string> files;
    try {
        for (auto& entry : fs::recursive_directory_iterator(dir)) {
            if (!entry.is_regular_file()) continue;
            std::string ext = entry.path().extension().string();
            for (auto& e : kExtensions) {
                if (ext == e) {
                    files.push_back(entry.path().string());
                    break;
                }
            }
        }
    } catch (const fs::filesystem_error& e) {
        std::cerr << "  Warning: " << e.what() << "\n";
    }
    std::sort(files.begin(), files.end());
    return files;
}

BatchProcessor::FileResult
BatchProcessor::process_file(const std::string& filepath) const {
    FileResult result;
    result.filepath = filepath;
    result.skipped = false;

    try {
        CompilerRunner runner(cfg_);
        auto compile_out = runner.run(filepath);

        if (compile_out.success()) {
            result.skipped = true;
            result.errors_found = 0;
            result.errors_fixed = 0;
            return result;
        }

        ErrorParser parser;
        auto diagnostics = parser.parse(compile_out.stderr_text);

        // Root cause analysis
        RootCauseAnalyzer analyzer;
        auto analysis = analyzer.analyze(diagnostics);
        auto& to_fix = analysis.root_causes;

        result.errors_found = (int)diagnostics.size();
        result.errors_fixed = 0;

        if (to_fix.empty()) return result;

        ContextExtractor extractor;
        extractor.enrich(const_cast<std::vector<Diagnostic>&>(to_fix), filepath);

        std::string full_source;
        try { full_source = ContextExtractor::read_file(filepath); } catch (...) {}

        LLMClient llm(cfg_);
        PatchApplier patcher(cfg_);
        patcher.backup(filepath);

        for (auto& diag : to_fix) {
            try {
                Fix fix = llm.get_fix(diag, full_source);
                auto patch_result = patcher.apply_and_verify(fix, filepath);
                if (patch_result.success && patch_result.compiles) {
                    full_source = patch_result.patched_source;
                    ++result.errors_fixed;
                }
            } catch (...) {
                // Continue with next error
            }
        }

    } catch (const std::exception& e) {
        result.error = e.what();
        result.errors_found = 0;
        result.errors_fixed = 0;
    }

    return result;
}

BatchProcessor::BatchSummary
BatchProcessor::run(const std::string& directory, ProgressCallback on_file_done) {
    BatchSummary summary;
    summary.files_scanned = 0;
    summary.files_with_errors = 0;
    summary.files_fully_fixed = 0;
    summary.files_partially_fixed = 0;
    summary.total_errors = 0;
    summary.total_fixed = 0;

    auto files = find_source_files(directory);
    summary.files_scanned = (int)files.size();

    std::cout << "\n  \033[1mBatch mode:\033[0m Found " << files.size()
              << " source file" << (files.size() != 1 ? "s" : "")
              << " in " << directory << "\n\n";

    for (auto& f : files) {
        std::cout << "  \033[90m→ " << f << "\033[0m ";
        std::cout.flush();

        auto result = process_file(f);
        summary.results.push_back(result);

        if (result.skipped) {
            std::cout << "\033[32m✓ clean\033[0m\n";
        } else if (!result.error.empty()) {
            std::cout << "\033[31m✗ error: " << result.error << "\033[0m\n";
        } else {
            summary.files_with_errors++;
            summary.total_errors += result.errors_found;
            summary.total_fixed  += result.errors_fixed;

            if (result.errors_fixed == result.errors_found) {
                summary.files_fully_fixed++;
                std::cout << "\033[32m✓ fixed " << result.errors_fixed
                          << "/" << result.errors_found << "\033[0m\n";
            } else if (result.errors_fixed > 0) {
                summary.files_partially_fixed++;
                std::cout << "\033[33m~ fixed " << result.errors_fixed
                          << "/" << result.errors_found << "\033[0m\n";
            } else {
                std::cout << "\033[31m✗ 0/" << result.errors_found
                          << " fixed\033[0m\n";
            }
        }

        if (on_file_done) on_file_done(result);
    }

    summary.print();
    return summary;
}

void BatchProcessor::BatchSummary::print() const {
    std::cout << "\n  ─────────────────────────────────────────────\n";
    std::cout << "  \033[1mBatch Summary\033[0m\n\n";
    std::cout << "  Files scanned:      " << files_scanned << "\n";
    std::cout << "  Files with errors:  " << files_with_errors << "\n";
    std::cout << "  Fully fixed:        \033[32m" << files_fully_fixed << "\033[0m\n";
    if (files_partially_fixed > 0)
        std::cout << "  Partially fixed:    \033[33m" << files_partially_fixed << "\033[0m\n";
    std::cout << "  Total errors found: " << total_errors << "\n";
    std::cout << "  Total errors fixed: \033[32m" << total_fixed << "\033[0m\n";
    std::cout << "  ─────────────────────────────────────────────\n\n";
}
