#pragma once
#include "types.h"
#include <string>
#include <vector>

class CompilerRunner {
public:
    explicit CompilerRunner(const Config& cfg) : cfg_(cfg) {}
    CompileOutput run(const std::string& source_file,
                      const std::vector<std::string>& extra_flags = {}) const;
    CompileOutput run_content(const std::string& source_content,
                              const std::string& filename_hint) const;
private:
    const Config& cfg_;
    std::string build_command(const std::string& file,
                              const std::vector<std::string>& extra) const;
};
