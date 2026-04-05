#pragma once
#include "types.h"
#include <string>
#include <vector>

class ContextExtractor {
public:
    void enrich(std::vector<Diagnostic>& diagnostics,
                const std::string& source_file,
                int context_radius = 8) const;
    static std::string read_file(const std::string& path);
    static std::vector<std::string> split_lines(const std::string& content);
};
