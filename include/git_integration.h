#pragma once
#include <string>
#include <vector>

/**
 * GitIntegration
 *
 * Detects if a file is inside a git repository and provides helpers for:
 *   - Checking git availability
 *   - Auto-committing verified fixes with descriptive messages
 *   - Creating a WIP branch for batch fixes
 *   - Showing a clean diff of what was changed
 */
class GitIntegration {
public:
    // Returns true if `git` is available and the file is inside a repo
    static bool is_git_repo(const std::string& filepath);

    // Commit the current state of filepath with a descriptive message
    // Returns true on success
    static bool commit_fix(const std::string& filepath,
                           const std::string& error_message,
                           const std::string& patch_summary);

    // Commit multiple files (batch mode)
    static bool commit_batch(const std::vector<std::string>& filepaths,
                             int num_fixed, int num_total);

    // Show colored git diff of what changed (for verbose mode)
    static std::string show_diff(const std::string& filepath);

    // Get current branch name
    static std::string current_branch();

private:
    static std::string run_git(const std::string& args,
                                const std::string& cwd = "");
    static std::string file_dir(const std::string& filepath);
};
