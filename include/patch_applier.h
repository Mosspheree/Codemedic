#pragma once
#include "types.h"
#include <string>

class PatchApplier {
public:
    explicit PatchApplier(const Config& cfg) : cfg_(cfg) {}
    struct ApplyResult {
        bool        success;
        bool        compiles;
        std::string patched_source;
        std::string compile_output;
        std::string error_message;
    };
    ApplyResult apply_and_verify(const Fix& fix, const std::string& source_file) const;
    bool write_back(const std::string& source_file, const std::string& patched_source) const;
    bool backup(const std::string& source_file) const;
private:
    const Config& cfg_;
};
