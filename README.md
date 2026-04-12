<p align="center">
  <img src="https://img.shields.io/badge/lang-C%2B%2B17-blue?logo=cplusplus" alt="C++17">
  <img src="https://img.shields.io/badge/build-CMake-064F8C?logo=cmake" alt="CMake">
  <img src="https://img.shields.io/badge/docker-ready-2496ED?logo=docker" alt="Docker">
  <img src="https://img.shields.io/badge/CI-GitHub%20Actions-2088FF?logo=githubactions&logoColor=white" alt="CI">
  <img src="https://img.shields.io/badge/license-MIT-green" alt="License">
  <img src="https://img.shields.io/badge/version-3.0.0-orange" alt="Version">
</p>

# codemedic

> **AI-powered C/C++ compiler error fixer** that compiles, diagnoses, patches, and *verifies* -- fully automated from your terminal.

Point it at a broken source file. It compiles it, parses every error, asks an LLM to explain each one in plain English and produce a minimal fix, applies the patch, recompiles to verify it works, and reports the result -- all without copy-pasting into a chatbot.

```
  ╔══════════════════════════════════╗
  ║  codemedic  — compiler error fixer  ║
  ╚══════════════════════════════════╝

  Compiling demo_bugs.cpp...
  Provider: groq  Model: llama-3.3-70b-versatile

  Found 3 errors (2 root causes to fix). Asking groq...

  ────────────────────────────────────────────────────────────────────────
  Error 1/2  demo_bugs.cpp:15:18
  'multiplier' was not declared in this scope

  ▶  14 │     int result = x * multiplier;

  Explanation
  ··········································
  The variable 'multiplier' is used but has never been declared or
  defined anywhere in scope.

  Patch  ─ declare multiplier as a local constant
  ··········································
  --- a/demo_bugs.cpp
  +++ b/demo_bugs.cpp
  @@ -12,6 +12,7 @@
   int compute(int x) {
  +    const int multiplier = 1;
       int result = x * multiplier;

  ✓ Patch applied and verified — file compiles cleanly.
```

## What makes it different

Most tools explain errors. **codemedic** closes the loop:

1. **Compile** -- invokes clang/gcc, captures stderr
2. **Parse** -- extracts structured diagnostics with file/line/column
3. **Analyze** -- identifies root causes, suppresses cascade errors
4. **Fix** -- asks an LLM for a minimal unified diff patch
5. **Verify** -- applies the patch and recompiles to prove it works
6. **Commit** -- optionally auto-commits verified fixes to git

The **verified recompile** step is key -- it guarantees every applied patch actually compiles.

## Features

| Feature | Flag | Description |
|---------|------|-------------|
| **Multi-provider LLM** | `--provider` | Anthropic (Claude), OpenAI (GPT), Groq (Llama), Ollama (local) |
| **Auto-apply** | `-y` | Apply all patches without prompting |
| **Explain-only** | `-e` | Explain errors without patching (learning mode) |
| **Batch mode** | `-b` | Fix all `.cpp/.c` files in a directory recursively |
| **Git integration** | `-g` | Auto-commit each verified fix with descriptive messages |
| **Root cause analysis** | automatic | Suppresses cascade errors, fixes root causes first |
| **JSON output** | `--json` | Machine-readable output for CI/CD integration |
| **Diff preview** | `--diff` | Preview patches without applying them |
| **Undo/rollback** | `--undo` | Restore files from `.bak` backups |
| **Session logging** | `--log` | Write fix history to structured JSON log |
| **Config file** | `--config` | Project or global `.codemedic.yaml` configuration |
| **Warning fixes** | `-w` | Also fix compiler warnings |
| **Shell completions** | -- | Bash and Zsh tab completion |
| **Docker support** | -- | Multi-stage Dockerfile + Compose with Ollama |

## Requirements

- Linux (uses `patch` command and POSIX process control)
- clang or gcc (as the compiler to fix errors from)
- CMake >= 3.16
- libcurl
- An API key (Anthropic, OpenAI, or Groq) -- *or* local Ollama

### Install dependencies

**Ubuntu/Debian:**
```bash
sudo apt install cmake clang libcurl4-openssl-dev patch
```

**Fedora/RHEL:**
```bash
sudo dnf install cmake clang libcurl-devel patch
```

## Build

```bash
# Quick build
./scripts/build.sh

# Or manually
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)

# Install system-wide
./scripts/install.sh
```

### Docker

```bash
# Build and run
docker build -t codemedic .
docker run --rm -v $(pwd):/workspace -e ANTHROPIC_API_KEY codemedic broken.cpp

# With docker-compose (includes Ollama for local LLM)
docker compose run codemedic -y broken.cpp
docker compose run codemedic-local broken.cpp   # uses local Ollama
```

## Usage

```bash
# Basic -- fix a broken file
codemedic broken.cpp

# Auto-apply all patches
codemedic -y main.cpp

# Pass compiler flags after --
codemedic broken.cpp -- -std=c++17 -I./include -Wall

# Explain errors without patching (great for learning)
codemedic -e broken.cpp

# Preview diffs without applying
codemedic --diff broken.cpp

# Batch fix an entire directory
codemedic -b ./src/

# Fix and auto-commit to git
codemedic -y -g main.cpp
```

### LLM Providers

```bash
# Groq (default, free tier available)
export GROQ_API_KEY=gsk_...
codemedic broken.cpp

# Anthropic (Claude)
export ANTHROPIC_API_KEY=sk-ant-...
codemedic --provider anthropic broken.cpp

# OpenAI (GPT-4o)
export OPENAI_API_KEY=sk-...
codemedic --provider openai broken.cpp

# Ollama (fully local, no API key)
ollama pull llama3
codemedic --provider ollama broken.cpp

# Custom endpoint (e.g., Azure OpenAI, vLLM, LM Studio)
codemedic --provider openai --provider-url http://localhost:8080/v1/chat/completions broken.cpp
```

### CI/CD Integration

```bash
# JSON output for automated pipelines
codemedic --json -y broken.cpp | jq '.summary'

# Log fix history for auditing
codemedic --log fixes.json -y broken.cpp

# Exit code: 0 = all fixed, 1 = some remaining
codemedic -y broken.cpp && echo "All fixed!" || echo "Issues remain"
```

### Configuration

Create `.codemedic.yaml` in your project root:

```yaml
compiler: clang++
provider: groq
auto_apply: false
fix_warnings: true
compiler_flags:
  - -std=c++17
  - -Wall
  - -I./include
```

Config file search order:
1. `--config <path>` (explicit)
2. `.codemedic.yaml` in source file's directory (walk up to root)
3. `~/.config/codemedic/config.yaml` (user global)

CLI flags always override config file values.

## Architecture

```
Source file
    │
    ▼
CompilerRunner          invokes clang/gcc, captures stderr
    │
    ▼
ErrorParser             regex-based parsing into Diagnostic structs
    │
    ▼
RootCauseAnalyzer       dependency graph, cascade suppression
    │
    ▼
ContextExtractor        reads source file, attaches surrounding lines
    │
    ▼
LLMClient               multi-provider dispatcher (Anthropic/OpenAI/Groq/Ollama)
    │                   structured prompt → JSON: { explanation, patch, patch_summary }
    ▼
PatchApplier            applies unified diff, recompiles to verify
    │
    ▼
SessionLogger           records fix history to JSON log
    │
    ▼
TerminalUI / JSON       colored terminal output or machine-readable JSON
```

## Project structure

```
codemedic/
├── CMakeLists.txt
├── Dockerfile                     # Multi-stage Docker build
├── docker-compose.yml             # Compose with Ollama support
├── .codemedic.yaml                # Example config file
├── .clang-format                  # Code formatting rules
├── .clang-tidy                    # Static analysis config
├── .github/
│   └── workflows/
│       ├── build.yml              # CI: build matrix (clang/gcc × Debug/Release)
│       └── release.yml            # CD: auto-release on tag push
├── scripts/
│   ├── build.sh                   # Quick build script
│   └── install.sh                 # System-wide installer
├── completions/
│   ├── codemedic.bash             # Bash tab completion
│   └── codemedic.zsh              # Zsh tab completion
├── include/
│   ├── types.h                    # Diagnostic, Fix, Config, LLMProvider structs
│   ├── compiler_runner.h          # Compile invocation interface
│   ├── error_parser.h             # Parse compiler output into diagnostics
│   ├── context_extractor.h        # Source context enrichment
│   ├── llm_client.h               # Multi-provider LLM client
│   ├── patch_applier.h            # Apply + verify patches
│   ├── terminal_ui.h              # Terminal UI with colors/spinners
│   ├── root_cause_analyzer.h      # Cascade error detection
│   ├── git_integration.h          # Git auto-commit integration
│   ├── batch_processor.h          # Recursive directory processing
│   ├── session_logger.h           # JSON fix history logging
│   └── config_loader.h            # YAML config file loader
├── src/
│   ├── main.cpp                   # CLI parsing, orchestration
│   ├── compiler_runner.cpp
│   ├── error_parser.cpp
│   ├── context_extractor.cpp
│   ├── llm_client.cpp             # Anthropic/OpenAI/Groq/Ollama dispatching
│   ├── patch_applier.cpp
│   ├── terminal_ui.cpp
│   ├── root_cause_analyzer.cpp
│   ├── git_integration.cpp
│   ├── batch_processor.cpp
│   ├── session_logger.cpp
│   └── config_loader.cpp
└── tests/
    ├── demo_bugs.cpp              # Intentional C++ errors for demo
    ├── demo_c_bugs.c              # Intentional C errors for demo
    └── cascade_test.cpp           # Cascade suppression demo
```

## Demo (60 seconds)

```bash
# 1. Show the broken file
cat tests/demo_bugs.cpp

# 2. Show it fails to compile
clang++ tests/demo_bugs.cpp

# 3. Run codemedic with auto-apply
./build/codemedic -y tests/demo_bugs.cpp

# 4. Show it now compiles
clang++ tests/demo_bugs.cpp && echo "Compiles cleanly!"

# 5. Undo if needed
./build/codemedic --undo tests/demo_bugs.cpp
```

## JSON output example

```json
{
  "file": "broken.cpp",
  "provider": "groq",
  "model": "llama-3.3-70b-versatile",
  "errors": [
    {
      "diagnostic": {
        "file": "broken.cpp",
        "line": 5,
        "severity": "error",
        "message": "'cout' was not declared in this scope"
      },
      "fix": {
        "explanation": "The program uses cout without including <iostream>.",
        "patch_summary": "Add #include <iostream>",
        "patch": "--- a/broken.cpp\n+++ b/broken.cpp\n..."
      },
      "applied": true,
      "verified": true
    }
  ],
  "summary": {
    "total_errors": 3,
    "root_causes": 1,
    "suppressed": 2,
    "fixed": 1
  }
}
```

## Session log example

```json
{
  "session": {
    "start": "2025-06-15T10:30:00Z",
    "end": "2025-06-15T10:30:45Z",
    "provider": "groq",
    "model": "llama-3.3-70b-versatile",
    "compiler": "clang++"
  },
  "fixes": [
    {
      "file": "broken.cpp",
      "line": 5,
      "error": "'cout' was not declared in this scope",
      "applied": true,
      "verified": true,
      "duration_ms": 1234.5
    }
  ],
  "summary": {
    "total_attempts": 1,
    "total_applied": 1,
    "total_verified": 1,
    "total_duration_ms": 1234.5
  }
}
```
