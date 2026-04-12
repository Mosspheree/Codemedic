#include "llm_client.h"
#include <nlohmann/json.hpp>
#include <curl/curl.h>
#include <sstream>
#include <stdexcept>

using json = nlohmann::json;

// ── Provider endpoints ────────────────────────────────────────────────────────
static const std::string GROQ_URL      = "https://api.groq.com/openai/v1/chat/completions";
static const std::string OPENAI_URL    = "https://api.openai.com/v1/chat/completions";
static const std::string ANTHROPIC_URL = "https://api.anthropic.com/v1/messages";
static const std::string OLLAMA_URL    = "http://localhost:11434/api/chat";

// ── Default models per provider ───────────────────────────────────────────────
static const std::string GROQ_MODEL      = "llama-3.3-70b-versatile";
static const std::string OPENAI_MODEL    = "gpt-4o";
static const std::string ANTHROPIC_MODEL = "claude-sonnet-4-20250514";
static const std::string OLLAMA_MODEL    = "llama3";

static size_t write_cb(char* ptr, size_t size, size_t nmemb, void* userdata) {
    auto* s = static_cast<std::string*>(userdata);
    s->append(ptr, size * nmemb);
    return size * nmemb;
}

std::string LLMClient::post_json(
        const std::string& url,
        const std::string& body,
        const std::vector<std::pair<std::string, std::string>>& headers) const {
    CURL* curl = curl_easy_init();
    if (!curl) throw std::runtime_error("curl_easy_init failed");

    std::string response;
    struct curl_slist* hdr_list = nullptr;
    hdr_list = curl_slist_append(hdr_list, "Content-Type: application/json");
    for (auto& [key, val] : headers)
        hdr_list = curl_slist_append(hdr_list, (key + ": " + val).c_str());

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, hdr_list);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 120L);

    CURLcode res = curl_easy_perform(curl);

    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

    curl_slist_free_all(hdr_list);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK)
        throw std::runtime_error(std::string("curl error: ") + curl_easy_strerror(res));

    if (http_code >= 400) {
        std::string detail = response.substr(0, 300);
        throw std::runtime_error("HTTP " + std::to_string(http_code) + ": " + detail);
    }

    return response;
}

std::string LLMClient::build_system_prompt() const {
    return R"(You are an expert C/C++ compiler error analyst and code repair tool.

Your job: given a compiler error and the surrounding source code, you must:
1. Explain the error in clear, plain English (2-4 sentences max)
2. Produce a minimal unified diff patch that fixes it

CRITICAL RULES for the patch:
- Output a valid unified diff (--- a/file, +++ b/file, @@ ... @@ format)
- Change as few lines as possible — minimal fix only
- The patch MUST compile successfully when applied
- Do NOT refactor, rename, or improve unrelated code
- Do NOT add comments unless they are the fix itself

Respond ONLY with valid JSON in this exact schema, no markdown, no preamble:
{
  "explanation": "Plain English explanation of what the error means and why it occurs.",
  "patch_summary": "One line: what the patch does.",
  "patch": "--- a/filename.cpp\n+++ b/filename.cpp\n@@ ... @@\n..."
}
)";
}

std::string LLMClient::build_prompt(const Diagnostic& diag,
                                     const std::string& full_source) const {
    std::ostringstream p;

    p << "COMPILER ERROR:\n";
    p << diag.file << ":" << diag.line << ":" << diag.column << ": ";

    switch (diag.severity) {
        case Diagnostic::Severity::Error: p << "error: "; break;
        case Diagnostic::Severity::Fatal: p << "fatal error: "; break;
        default: p << "warning: "; break;
    }
    p << diag.message << "\n";

    if (!diag.code.empty())
        p << "Error code: " << diag.code << "\n";

    if (!diag.notes.empty()) {
        p << "\nCOMPILER NOTES:\n";
        for (auto& n : diag.notes) p << "  " << n << "\n";
    }

    p << "\nSOURCE CONTEXT (around line " << diag.line << "):\n";
    int lineno = diag.context_start_line;
    for (auto& line : diag.context_lines) {
        bool is_error_line = (lineno == diag.line);
        p << (is_error_line ? ">>> " : "    ");
        p << lineno << ": " << line << "\n";
        ++lineno;
    }

    p << "\nFULL SOURCE FILE (" << diag.file << "):\n```\n";
    p << full_source << "\n```\n";

    p << "\nFilename for the patch: " << diag.file << "\n";
    p << "Produce the JSON response now.";

    return p.str();
}

// ── OpenAI/Groq-compatible body (chat completions) ───────────────────────────
std::string LLMClient::build_openai_body(const Diagnostic& diag,
                                          const std::string& full_source) const {
    std::string model = cfg_.resolved_model();

    json body = {
        {"model",      model},
        {"max_tokens", 2048},
        {"temperature", 0.2},
        {"messages", json::array({
            {{"role", "system"}, {"content", build_system_prompt()}},
            {{"role", "user"},   {"content", build_prompt(diag, full_source)}}
        })}
    };

    return body.dump();
}

// ── Anthropic Messages API body ──────────────────────────────────────────────
std::string LLMClient::build_anthropic_body(const Diagnostic& diag,
                                             const std::string& full_source) const {
    std::string model = cfg_.resolved_model();

    json body = {
        {"model",      model},
        {"max_tokens", 2048},
        {"system",     build_system_prompt()},
        {"messages", json::array({
            {{"role", "user"}, {"content", build_prompt(diag, full_source)}}
        })}
    };

    return body.dump();
}

// ── Parse OpenAI/Groq/Ollama response ────────────────────────────────────────
Fix LLMClient::parse_openai_response(const std::string& response_body) const {
    auto j = json::parse(response_body);

    std::string text;
    if (j.contains("choices") && j["choices"].is_array()) {
        text = j["choices"][0]["message"]["content"].get<std::string>();
    } else if (j.contains("message") && j["message"].contains("content")) {
        // Ollama format
        text = j["message"]["content"].get<std::string>();
    } else if (j.contains("error")) {
        throw std::runtime_error("API error: " + j["error"]["message"].get<std::string>());
    } else {
        throw std::runtime_error("Unexpected API response: " + response_body.substr(0, 200));
    }

    auto strip = [](std::string s) {
        auto start = s.find('{');
        auto end   = s.rfind('}');
        if (start != std::string::npos && end != std::string::npos)
            return s.substr(start, end - start + 1);
        return s;
    };

    auto inner = json::parse(strip(text));

    Fix fix;
    fix.explanation   = inner.value("explanation", "");
    fix.patch_summary = inner.value("patch_summary", "");
    fix.patch         = inner.value("patch", "");
    return fix;
}

// ── Parse Anthropic Messages API response ────────────────────────────────────
Fix LLMClient::parse_anthropic_response(const std::string& response_body) const {
    auto j = json::parse(response_body);

    if (j.contains("error")) {
        std::string msg = j["error"].value("message", "Unknown error");
        throw std::runtime_error("Anthropic API error: " + msg);
    }

    std::string text;
    if (j.contains("content") && j["content"].is_array() && !j["content"].empty()) {
        text = j["content"][0].value("text", "");
    } else {
        throw std::runtime_error("Unexpected Anthropic response: " + response_body.substr(0, 200));
    }

    auto strip = [](std::string s) {
        auto start = s.find('{');
        auto end   = s.rfind('}');
        if (start != std::string::npos && end != std::string::npos)
            return s.substr(start, end - start + 1);
        return s;
    };

    auto inner = json::parse(strip(text));

    Fix fix;
    fix.explanation   = inner.value("explanation", "");
    fix.patch_summary = inner.value("patch_summary", "");
    fix.patch         = inner.value("patch", "");
    return fix;
}

// ── Main get_fix dispatcher ──────────────────────────────────────────────────
Fix LLMClient::get_fix(const Diagnostic& diag, const std::string& full_source) const {
    switch (cfg_.provider) {
        case LLMProvider::Anthropic: {
            std::string url = cfg_.provider_url.empty() ? ANTHROPIC_URL : cfg_.provider_url;
            std::string body = build_anthropic_body(diag, full_source);
            std::vector<std::pair<std::string, std::string>> headers = {
                {"x-api-key",         cfg_.api_key},
                {"anthropic-version", "2023-06-01"}
            };
            std::string resp = post_json(url, body, headers);
            return parse_anthropic_response(resp);
        }
        case LLMProvider::OpenAI: {
            std::string url = cfg_.provider_url.empty() ? OPENAI_URL : cfg_.provider_url;
            std::string body = build_openai_body(diag, full_source);
            std::vector<std::pair<std::string, std::string>> headers = {
                {"Authorization", "Bearer " + cfg_.api_key}
            };
            std::string resp = post_json(url, body, headers);
            return parse_openai_response(resp);
        }
        case LLMProvider::Groq: {
            std::string url = cfg_.provider_url.empty() ? GROQ_URL : cfg_.provider_url;
            std::string body = build_openai_body(diag, full_source);
            std::vector<std::pair<std::string, std::string>> headers = {
                {"Authorization", "Bearer " + cfg_.api_key}
            };
            std::string resp = post_json(url, body, headers);
            return parse_openai_response(resp);
        }
        case LLMProvider::Ollama: {
            std::string url = cfg_.provider_url.empty() ? OLLAMA_URL : cfg_.provider_url;
            std::string model = cfg_.resolved_model();
            json body = {
                {"model", model},
                {"stream", false},
                {"messages", json::array({
                    {{"role", "system"}, {"content", build_system_prompt()}},
                    {{"role", "user"},   {"content", build_prompt(diag, full_source)}}
                })}
            };
            // Ollama doesn't need auth headers
            std::vector<std::pair<std::string, std::string>> headers;
            std::string resp = post_json(url, body.dump(), headers);
            return parse_openai_response(resp);
        }
    }

    throw std::runtime_error("Unknown LLM provider");
}

Fix LLMClient::get_fix_streaming(const Diagnostic& diag,
                                  const std::string& full_source,
                                  std::function<void(const std::string&)> on_token) const {
    (void)on_token;
    return get_fix(diag, full_source);
}
