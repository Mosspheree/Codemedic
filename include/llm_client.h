#pragma once
#include "types.h"
#include <string>
#include <functional>

class LLMClient {
public:
    explicit LLMClient(const Config& cfg) : cfg_(cfg) {}
    Fix get_fix(const Diagnostic& diag, const std::string& full_source) const;
    Fix get_fix_streaming(const Diagnostic& diag,
                          const std::string& full_source,
                          std::function<void(const std::string&)> on_token) const;
private:
    const Config& cfg_;
    std::string build_prompt(const Diagnostic& diag, const std::string& full_source) const;
    std::string build_system_prompt() const;
    std::string post_json(const std::string& url, const std::string& body,
                          const std::string& auth_header) const;
    Fix parse_llm_response(const std::string& response_body) const;
};
