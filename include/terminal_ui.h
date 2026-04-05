#pragma once
#include "types.h"
#include <string>

class TerminalUI {
public:
    void print_banner() const;
    void print_diagnostic(const Diagnostic& d, int index, int total) const;
    void print_thinking() const;
    void print_explanation(const Fix& fix) const;
    void print_patch(const Fix& fix) const;
    void print_verified(const std::string& compile_output) const;
    void print_failed(const std::string& reason) const;
    void print_summary(int fixed, int total) const;
    void print_error(const std::string& msg) const;
    bool prompt_apply() const;
    void spin(int tick) const;
    void clear_line() const;
private:
    void print_divider(char ch = '-', int width = 72) const;
};
