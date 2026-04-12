#pragma once
#include "types.h"
#include <string>

/**
 * ConfigLoader
 *
 * Loads configuration from .codemedic.yaml files.
 * Search order:
 *   1. Explicit --config path
 *   2. .codemedic.yaml in source file's directory
 *   3. .codemedic.yaml in current working directory
 *   4. ~/.config/codemedic/config.yaml (user global)
 *
 * Config file values are overridden by CLI flags.
 *
 * Example .codemedic.yaml:
 *   compiler: clang++
 *   model: claude-sonnet-4-20250514
 *   provider: anthropic
 *   auto_apply: false
 *   fix_warnings: true
 *   compiler_flags:
 *     - -std=c++17
 *     - -Wall
 */
class ConfigLoader {
public:
    // Load config from file, merging with existing CLI config
    // CLI flags take precedence over file values
    static bool load(Config& cfg, const std::string& config_path = "");

    // Find the nearest .codemedic.yaml
    static std::string find_config(const std::string& start_dir = ".");

private:
    static bool parse_yaml_simple(Config& cfg, const std::string& path);
    static LLMProvider parse_provider(const std::string& name);
    static std::string trim(const std::string& s);
};
