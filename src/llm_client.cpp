#include "llm_client.h"
#include <nlohmann/json.hpp>
#include <curl/curl.h>
#include <sstream>
#include <stdexcept>

using json = nlohmann::json;

// ── curl write callback ────────────────────────────────────────────────────────
static size_t write_cb(char* ptr, size_t size, size_t nmemb, void* userdata) {
    auto* s = static_cast<std::string*>(userdata);
    s->append(ptr, size * nmemb);
    return size * nmemb;
}

std::string LLMClient::post_json(const std::string& url,
                                  const std::string& body,
                                  const std::string& auth_header) const {
    CURL* curl = curl_easy_init();
    if (!curl) throw std::runtime_error("curl_easy_init failed");

    std::string response;
    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    headers = curl_slist_append(headers, auth_header.c_str());
    headers = curl_slist_append(headers, "anthropic-version: 2023-06-01");

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 60L);

    CURLcode res = curl_easy_perform(curl);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK)
        throw std::runtime_error(std::string("curl error: ") + curl_easy_strerror(res));

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

Fix LLMClient::parse_llm_response(const std::string& response_body) const {
    auto j = json::parse(response_body);

    // Anthropic response: j["content"][0]["text"]
    std::string text;
    if (j.contains("content") && j["content"].is_array()) {
        text = j["content"][0]["text"].get<std::string>();
    } else if (j.contains("choices")) {
        // OpenAI fallback
        text = j["choices"][0]["message"]["content"].get<std::string>();
    } else if (j.contains("error")) {
        throw std::runtime_error("API error: " + j["error"]["message"].get<std::string>());
    } else {
        throw std::runtime_error("Unexpected API response: " + response_body.substr(0, 200));
    }

    // Strip any accidental markdown fences
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

Fix LLMClient::get_fix(const Diagnostic& diag, const std::string& full_source) const {
    json body = {
        {"model", cfg_.model},
        {"max_tokens", 2048},
        {"system", build_system_prompt()},
        {"messages", json::array({
            {{"role", "user"}, {"content", build_prompt(diag, full_source)}}
        })}
    };

    std::string auth = "x-api-key: " + cfg_.api_key;
    std::string resp = post_json("https://api.anthropic.com/v1/messages",
                                  body.dump(), auth);
    return parse_llm_response(resp);
}

Fix LLMClient::get_fix_streaming(const Diagnostic& diag,
                                  const std::string& full_source,
                                  std::function<void(const std::string&)> on_token) const {
    // For now delegate to non-streaming; streaming can be added as stretch goal
    (void)on_token;
    return get_fix(diag, full_source);
}
