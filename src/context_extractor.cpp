#include "context_extractor.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <stdexcept>

std::string ContextExtractor::read_file(const std::string& path) {
    std::ifstream f(path);
    if (!f) throw std::runtime_error("Cannot open file: " + path);
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

std::vector<std::string> ContextExtractor::split_lines(const std::string& content) {
    std::vector<std::string> lines;
    std::istringstream stream(content);
    std::string line;
    while (std::getline(stream, line)) lines.push_back(line);
    return lines;
}

void ContextExtractor::enrich(std::vector<Diagnostic>& diagnostics,
                               const std::string& source_file,
                               int context_radius) const {
    std::string content;
    try { content = read_file(source_file); } catch (...) { return; }
    auto lines = split_lines(content);
    int total  = static_cast<int>(lines.size());
    for (auto& diag : diagnostics) {
        int center = diag.line - 1;
        int start  = std::max(0, center - context_radius);
        int end    = std::min(total - 1, center + context_radius);
        diag.context_start_line = start + 1;
        diag.context_lines.clear();
        for (int i = start; i <= end; ++i)
            diag.context_lines.push_back(lines[i]);
    }
}
