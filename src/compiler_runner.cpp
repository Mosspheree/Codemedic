#include "compiler_runner.h"
#include <sstream>
#include <cstdio>
#include <array>
#include <stdexcept>
#include <fstream>
#include <filesystem>

namespace fs = std::filesystem;

static std::string exec_command(const std::string& cmd, int& exit_code) {
    std::string result;
    std::string full_cmd = cmd + " 2>&1";
    FILE* pipe = popen(full_cmd.c_str(), "r");
    if (!pipe) throw std::runtime_error("popen() failed");
    std::array<char, 512> buf;
    while (fgets(buf.data(), buf.size(), pipe) != nullptr)
        result += buf.data();
    int status = pclose(pipe);
    exit_code = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
    return result;
}

std::string CompilerRunner::build_command(const std::string& file,
                                          const std::vector<std::string>& extra) const {
    std::ostringstream cmd;
    cmd << cfg_.compiler;
    for (auto& f : cfg_.compiler_flags) cmd << " " << f;
    for (auto& f : extra)              cmd << " " << f;
    cmd << " -fdiagnostics-color=never";
    cmd << " -fno-caret-diagnostics";
    cmd << " " << file;
    cmd << " -o /dev/null";
    return cmd.str();
}

CompileOutput CompilerRunner::run(const std::string& source_file,
                                  const std::vector<std::string>& extra_flags) const {
    CompileOutput out;
    out.stderr_text = exec_command(build_command(source_file, extra_flags), out.exit_code);
    return out;
}

CompileOutput CompilerRunner::run_content(const std::string& source_content,
                                          const std::string& filename_hint) const {
    fs::path tmp = fs::temp_directory_path() / ("fixcc_" + filename_hint);
    { std::ofstream f(tmp); f << source_content; }
    auto result = run(tmp.string());
    auto replace_all = [](std::string s, const std::string& from, const std::string& to) {
        size_t pos = 0;
        while ((pos = s.find(from, pos)) != std::string::npos) {
            s.replace(pos, from.size(), to); pos += to.size();
        }
        return s;
    };
    result.stderr_text = replace_all(result.stderr_text, tmp.string(), filename_hint);
    fs::remove(tmp);
    return result;
}
