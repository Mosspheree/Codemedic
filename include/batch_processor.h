#pragma once
#include "types.h"
#include <string>
#include <vector>
#include <functional>

/**
 * BatchProcessor
 *
 * Recursively finds all C/C++ source files in a directory and runs
 * the full codemedic pipeline on each one.
 *
 * Usage:
 *   BatchProcessor bp(cfg);
 *   bp.run("./src/", on_progress);
 *
 * The on_progress callback is called after each file with the result.
 */
class BatchProcessor {
public:
    struct FileResult {
        std::string filepath;
        int errors_found;
        int errors_fixed;
        bool skipped;       // compiled cleanly, nothing to do
        std::string error;  // if processing itself failed
    };

    struct BatchSummary {
        int files_scanned;
        int files_with_errors;
        int files_fully_fixed;
        int files_partially_fixed;
        int total_errors;
        int total_fixed;
        std::vector<FileResult> results;

        void print() const;
    };

    using ProgressCallback = std::function<void(const FileResult&)>;

    explicit BatchProcessor(const Config& cfg) : cfg_(cfg) {}

    BatchSummary run(const std::string& directory,
                     ProgressCallback on_file_done = nullptr);

private:
    Config cfg_;

    std::vector<std::string> find_source_files(const std::string& dir) const;
    FileResult process_file(const std::string& filepath) const;

    static const std::vector<std::string> kExtensions;
};
