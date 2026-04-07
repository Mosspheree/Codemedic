#include "git_integration.h"
#include <array>
#include <cstdio>
#include <stdexcept>
#include <filesystem>
#include <sstream>
#include <ctime>

namespace fs = std::filesystem;

// ── Shell helper ───────────────────────────────────────────────────────────────
std::string GitIntegration::run_git(const std::string& args, const std::string& cwd) {
    std::string cmd;
    if (!cwd.empty())
        cmd = "cd " + cwd + " && ";
    cmd += "git " + args + " 2>&1";

    std::array<char, 256> buf;
    std::string result;
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) return "";
    while (fgets(buf.data(), buf.size(), pipe))
        result += buf.data();
    pclose(pipe);
    return result;
}

std::string GitIntegration::file_dir(const std::string& filepath) {
    try {
        return fs::absolute(filepath).parent_path().string();
    } catch (...) {
        return ".";
    }
}

// ── Public API ─────────────────────────────────────────────────────────────────
bool GitIntegration::is_git_repo(const std::string& filepath) {
    std::string dir = file_dir(filepath);
    std::string out = run_git("rev-parse --is-inside-work-tree", dir);
    return out.find("true") != std::string::npos;
}

bool GitIntegration::commit_fix(const std::string& filepath,
                                 const std::string& error_message,
                                 const std::string& patch_summary) {
    std::string dir = file_dir(filepath);
    std::string rel = fs::path(filepath).filename().string();

    // Stage the file
    run_git("add " + fs::absolute(filepath).string(), dir);

    // Build commit message
    // Format: "fix(codemedic): <patch_summary>\n\nOriginal error: <error>"
    std::string short_error = error_message.substr(0, 80);
    // Escape single quotes for shell
    auto esc = [](std::string s) {
        std::string r;
        for (char c : s) {
            if (c == '\'') r += "'\\''";
            else r += c;
        }
        return r;
    };

    std::string subject = "fix(" + rel + "): " + patch_summary;
    std::string body    = "Original error: " + short_error;
    std::string body2   = "Fixed by codemedic (AI-verified patch)";

    std::string msg = subject + "\\n\\n" + body + "\\n" + body2;
    std::string out = run_git("commit -m '" + esc(msg) + "'", dir);

    return out.find("master") != std::string::npos
        || out.find("main") != std::string::npos
        || out.find("1 file") != std::string::npos
        || out.find("[") != std::string::npos;
}

bool GitIntegration::commit_batch(const std::vector<std::string>& filepaths,
                                   int num_fixed, int num_total) {
    if (filepaths.empty()) return false;

    std::string dir = file_dir(filepaths[0]);

    // Stage all files
    for (auto& f : filepaths)
        run_git("add " + fs::absolute(f).string(), dir);

    std::ostringstream msg;
    msg << "fix: codemedic batch — " << num_fixed << "/" << num_total
        << " files fixed\\n\\nAI-verified patches applied by codemedic.";

    std::string out = run_git("commit -m '" + msg.str() + "'", dir);
    return out.find("[") != std::string::npos;
}

std::string GitIntegration::show_diff(const std::string& filepath) {
    std::string dir = file_dir(filepath);
    return run_git("diff HEAD " + fs::absolute(filepath).string(), dir);
}

std::string GitIntegration::current_branch() {
    return run_git("rev-parse --abbrev-ref HEAD");
}
