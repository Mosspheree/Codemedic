#include "config_loader.h"
#include <fstream>
#include <sstream>
#include <filesystem>
#include <algorithm>
#include <iostream>

namespace fs = std::filesystem;

std::string ConfigLoader::trim(const std::string& s) {
    auto start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    auto end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

LLMProvider ConfigLoader::parse_provider(const std::string& name) {
    std::string lower = name;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    if (lower == "anthropic" || lower == "claude") return LLMProvider::Anthropic;
    if (lower == "openai"    || lower == "gpt")    return LLMProvider::OpenAI;
    if (lower == "groq"      || lower == "llama")  return LLMProvider::Groq;
    if (lower == "ollama"    || lower == "local")   return LLMProvider::Ollama;
    return LLMProvider::Groq;
}

std::string ConfigLoader::find_config(const std::string& start_dir) {
    // 1. Check current directory and parents
    try {
        fs::path dir = fs::absolute(start_dir);
        for (int depth = 0; depth < 10 && !dir.empty(); ++depth) {
            fs::path candidate = dir / ".codemedic.yaml";
            if (fs::exists(candidate)) return candidate.string();
            candidate = dir / ".codemedic.yml";
            if (fs::exists(candidate)) return candidate.string();
            auto parent = dir.parent_path();
            if (parent == dir) break;
            dir = parent;
        }
    } catch (...) {}

    // 2. Check ~/.config/codemedic/config.yaml
    const char* home = std::getenv("HOME");
    if (home) {
        fs::path global = fs::path(home) / ".config" / "codemedic" / "config.yaml";
        if (fs::exists(global)) return global.string();
    }

    return "";
}

// Simple YAML-like parser (handles flat key: value pairs and simple lists)
bool ConfigLoader::parse_yaml_simple(Config& cfg, const std::string& path) {
    std::ifstream f(path);
    if (!f) return false;

    std::string line;
    std::string current_list_key;
    bool collecting_list = false;

    while (std::getline(f, line)) {
        std::string trimmed = trim(line);

        // Skip comments and empty lines
        if (trimmed.empty() || trimmed[0] == '#') {
            collecting_list = false;
            continue;
        }

        // List item (starts with -)
        if (trimmed[0] == '-' && collecting_list) {
            std::string val = trim(trimmed.substr(1));
            // Remove surrounding quotes
            if (val.size() >= 2 && val.front() == '"' && val.back() == '"')
                val = val.substr(1, val.size() - 2);
            if (current_list_key == "compiler_flags")
                cfg.compiler_flags.push_back(val);
            continue;
        }
        collecting_list = false;

        // Key: value pairs
        auto colon = trimmed.find(':');
        if (colon == std::string::npos) continue;

        std::string key = trim(trimmed.substr(0, colon));
        std::string val = trim(trimmed.substr(colon + 1));

        // Remove surrounding quotes
        if (val.size() >= 2 && val.front() == '"' && val.back() == '"')
            val = val.substr(1, val.size() - 2);

        // Handle empty value (start of a list)
        if (val.empty()) {
            current_list_key = key;
            collecting_list = true;
            continue;
        }

        auto tobool = [](const std::string& v) {
            return v == "true" || v == "yes" || v == "1" || v == "on";
        };

        if      (key == "compiler")      cfg.compiler = val;
        else if (key == "model")         cfg.model = val;
        else if (key == "provider")      cfg.provider = parse_provider(val);
        else if (key == "provider_url")  cfg.provider_url = val;
        else if (key == "auto_apply")    cfg.auto_apply = tobool(val);
        else if (key == "verbose")       cfg.verbose = tobool(val);
        else if (key == "fix_warnings")  cfg.fix_warnings = tobool(val);
        else if (key == "explain_only")  cfg.explain_only = tobool(val);
        else if (key == "git_commit")    cfg.git_commit = tobool(val);
        else if (key == "json_output")   cfg.json_output = tobool(val);
        else if (key == "log_file")      cfg.log_file = val;
    }

    return true;
}

bool ConfigLoader::load(Config& cfg, const std::string& config_path) {
    std::string path = config_path;

    if (path.empty())
        path = find_config(".");

    if (path.empty())
        return false;

    return parse_yaml_simple(cfg, path);
}
