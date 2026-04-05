#include "terminal_ui.h"
#include <iostream>
#include <sstream>
#include <cstdio>

#define RST  "\033[0m"
#define BOLD "\033[1m"
#define DIM  "\033[2m"
#define RED  "\033[31m"
#define GRN  "\033[32m"
#define YEL  "\033[33m"
#define CYN  "\033[36m"
#define WHT  "\033[97m"

void TerminalUI::print_divider(char ch, int width) const {
    std::cout << DIM; for (int i = 0; i < width; ++i) std::cout << ch; std::cout << RST << "\n";
}

void TerminalUI::print_banner() const {
    std::cout << "\n"
              << BOLD CYN "  ╔══════════════════════════════════╗\n" RST
              << BOLD CYN "  ║  " WHT "fixcc" CYN "  — compiler error fixer  ║\n" RST
              << BOLD CYN "  ╚══════════════════════════════════╝\n" RST << "\n";
}

void TerminalUI::print_diagnostic(const Diagnostic& d, int index, int total) const {
    print_divider();
    std::cout << BOLD RED "  Error " << index << "/" << total << RST
              << "  " << DIM << d.file << ":" << d.line << ":" << d.column << RST << "\n";
    std::cout << "  " << BOLD WHT << d.message << RST << "\n\n";
    if (!d.context_lines.empty()) {
        int lineno = d.context_start_line;
        for (auto& line : d.context_lines) {
            bool is_err = (lineno == d.line);
            if (is_err) std::cout << RED "  ▶ " << BOLD << lineno << " │ " << line << RST << "\n";
            else        std::cout << DIM "    " << lineno << " │ " RST << line << "\n";
            ++lineno;
        }
    }
    std::cout << "\n";
}

void TerminalUI::print_thinking() const { std::cout << CYN "  ✦ Asking Claude for a fix..." RST "\n"; }

void TerminalUI::print_explanation(const Fix& fix) const {
    std::cout << "\n" BOLD WHT "  Explanation\n" RST;
    print_divider('.');
    std::istringstream words(fix.explanation);
    std::string word; int col = 2;
    std::cout << "  ";
    while (words >> word) {
        if (col + (int)word.size() > 72) { std::cout << "\n  "; col = 2; }
        std::cout << word << " "; col += word.size() + 1;
    }
    std::cout << "\n\n";
}

void TerminalUI::print_patch(const Fix& fix) const {
    std::cout << BOLD WHT "  Patch  " RST DIM "─ " RST << fix.patch_summary << "\n";
    print_divider('.');
    std::istringstream lines(fix.patch); std::string line;
    while (std::getline(lines, line)) {
        if (line.empty())                { std::cout << "\n"; continue; }
        if (line[0] == '+')              std::cout << GRN << line << RST << "\n";
        else if (line[0] == '-')         std::cout << RED << line << RST << "\n";
        else if (line.substr(0,2)=="@@") std::cout << CYN << line << RST << "\n";
        else                             std::cout << DIM << line << RST << "\n";
    }
    std::cout << "\n";
}

void TerminalUI::print_verified(const std::string&) const {
    std::cout << BOLD GRN "  ✓ Patch applied and verified — file compiles cleanly.\n" RST "\n";
}

void TerminalUI::print_failed(const std::string& reason) const {
    std::cout << BOLD RED "  ✗ " RST RED << reason << RST "\n\n";
}

void TerminalUI::print_summary(int fixed, int total) const {
    print_divider();
    std::cout << "\n  " BOLD "Summary: " RST;
    if (fixed == total) std::cout << GRN BOLD << fixed << "/" << total << " errors fixed." RST "\n";
    else                std::cout << YEL BOLD << fixed << "/" << total << " errors fixed." RST "\n";
    std::cout << "\n";
}

void TerminalUI::print_error(const std::string& msg) const {
    std::cout << BOLD RED "  Error: " RST RED << msg << RST "\n";
}

bool TerminalUI::prompt_apply() const {
    std::cout << "  Apply this patch? " BOLD "[y/N] " RST;
    std::string ans; std::getline(std::cin, ans);
    return !ans.empty() && (ans[0] == 'y' || ans[0] == 'Y');
}

static const char* kSpinner[] = {"⠋","⠙","⠹","⠸","⠼","⠴","⠦","⠧","⠇","⠏"};
void TerminalUI::spin(int tick) const { std::cout << "\r  " CYN << kSpinner[tick % 10] << RST " thinking..." << std::flush; }
void TerminalUI::clear_line() const  { std::cout << "\r                          \r" << std::flush; }
