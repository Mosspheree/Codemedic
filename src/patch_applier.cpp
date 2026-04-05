#include "patch_applier.h"
#include "compiler_runner.h"
#include <fstream>
#include <sstream>
#include <cstdio>
#include <filesystem>
#include <array>

namespace fs = std::filesystem;

static std::string exec_capture(const std::string& cmd, int& code) {
    std::string out;
    FILE* p = popen((cmd + " 2>&1").c_str(), "r");
    if (!p) { code = -1; return "popen failed"; }
    std::array<char,512> buf;
    while (fgets(buf.data(), buf.size(), p)) out += buf.data();
    int status = pclose(p);
    code = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
    return out;
}

PatchApplier::ApplyResult PatchApplier::apply_and_verify(
        const Fix& fix, const std::string& source_file) const {
    ApplyResult result;
    fs::path patch_file = fs::temp_directory_path() / "fixcc_patch.diff";
    { std::ofstream pf(patch_file); if (!pf) { result.error_message = "Cannot write patch file"; return result; } pf << fix.patch; }
    int patch_exit = 0;
    std::string patch_out = exec_capture(
        "patch --backup --forward -p0 " + source_file + " < " + patch_file.string(), patch_exit);
    fs::remove(patch_file);
    if (patch_exit != 0) { result.success = false; result.error_message = "patch failed:\n" + patch_out; return result; }
    result.success = true;
    std::ifstream sf(source_file);
    std::ostringstream ss; ss << sf.rdbuf();
    result.patched_source = ss.str();
    CompilerRunner runner(cfg_);
    auto compile = runner.run(source_file);
    result.compiles       = compile.success();
    result.compile_output = compile.stderr_text;
    return result;
}

bool PatchApplier::backup(const std::string& source_file) const {
    try { fs::copy_file(source_file, source_file + ".bak", fs::copy_options::overwrite_existing); return true; }
    catch (...) { return false; }
}

bool PatchApplier::write_back(const std::string& source_file, const std::string& patched_source) const {
    std::ofstream f(source_file);
    if (!f) return false;
    f << patched_source;
    return true;
}
