#pragma once
#include "types.h"
#include <string>
#include <vector>

class ErrorParser {
public:
    std::vector<Diagnostic> parse(const std::string& compiler_output,
                                  bool include_warnings = false) const;
private:
    std::optional<Diagnostic> try_parse_line(const std::string& line) const;
    Diagnostic::Severity parse_severity(const std::string& s) const;
};
