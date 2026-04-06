# codemedic (compiler error fixer)

A C/C++ compiler error explainer that synthesizes verified patches using LLMs.

Point it at a broken source file. It compiles it, parses every error, asks Claude
to explain each one in plain English and produce a minimal fix, applies the patch,
recompiles to verify it works, and reports the result all in your terminal without having the hassle to copy/paste and ask AI to do it for you!

```
  ╔══════════════════════════════════╗
  ║  codemedic  — compiler error fixer  ║
  ╚══════════════════════════════════╝

  Found 3 errors. Asking Claude to fix them...

  ────────────────────────────────────────────────────────────────────────
  Error 1/3  demo_bugs.cpp:15:18
  'multiplier' was not declared in this scope

      13 │ int compute(int x) {
  ▶   14 │     int result = x * multiplier;
      15 │     return result;

  Explanation
  ···········································
  The variable 'multiplier' is used on line 14 but has never been declared
  or defined anywhere in scope. The compiler cannot find any variable,
  constant, or parameter by that name.

  Patch  ─ declare multiplier as a local constant
  ···········································
  --- a/demo_bugs.cpp
  +++ b/demo_bugs.cpp
  @@ -12,6 +12,7 @@
   int compute(int x) {
  +    const int multiplier = 1;
       int result = x * multiplier;

  Apply this patch? [y/N]
```

## Why it's novel

Existing tools explain errors. **codemedic** explains, patches, recompiles, and
verifies — closing the loop automatically. The verified recompile is the new part.

## Requirements

- Linux (uses `ptrace`-style process control and `patch` command)
- clang or gcc
- CMake ≥ 3.16
- LLVM/Clang dev libraries
- libcurl
- An Anthropic API key

### Install dependencies (Ubuntu/Debian)

```bash
sudo apt install cmake clang llvm-dev libclang-dev libcurl4-openssl-dev patch
```

### Install dependencies (Fedora)

```bash
sudo dnf install cmake clang llvm-devel clang-devel libcurl-devel patch
```

## Build

```bash
export ANTHROPIC_API_KEY=sk-ant-...
./scripts/build.sh
```

Or manually:

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
```

## Usage

```bash
# Basic usage
./build/codemedic broken.cpp

# Auto-apply all patches without prompting (great for demos)
./build/codemedic -y broken.cpp

# Pass compiler flags after --
./build/codemedic broken.cpp -- -std=c++17 -I./include -Wall

# Fix a C file
./build/codemedic -c clang broken.c

# Use a different model
./build/codemedic -m claude-opus-4-20250514 tricky.cpp
```

## Architecture

```
Source file
    │
    ▼
CompilerRunner          invokes clang/gcc, captures stderr
    │
    ▼
ErrorParser             parses "file:line:col: error: msg" into Diagnostic structs
    │
    ▼
ContextExtractor        reads source file, attaches surrounding lines to each Diagnostic
    │
    ▼
LLMClient               builds a structured prompt, calls Anthropic API
    │                   receives JSON: { explanation, patch_summary, patch }
    ▼
PatchApplier            applies unified diff via `patch`, recompiles to verify
    │
    ▼
TerminalUI              colored output, spinner, apply prompt, summary
```

## Project structure

```
codemedic/
├── CMakeLists.txt
├── README.md
├── scripts/
│   └── build.sh
├── include/
│   ├── types.h               — Diagnostic, Fix, Config structs
│   ├── compiler_runner.h
│   ├── error_parser.h
│   ├── context_extractor.h
│   ├── llm_client.h
│   ├── patch_applier.h
│   └── terminal_ui.h
├── src/
│   ├── main.cpp
│   ├── compiler_runner.cpp
│   ├── error_parser.cpp
│   ├── context_extractor.cpp
│   ├── llm_client.cpp
│   ├── patch_applier.cpp
│   └── terminal_ui.cpp
└── tests/
    ├── demo_bugs.cpp         — intentional C++ errors for demo
    └── demo_c_bugs.c         — intentional C errors for demo
```

## Stretch goals (week 5)

- Stream LLM tokens to terminal in real time (partial JSON reconstruction)
- Support Python and Rust via tree-sitter
- Web dashboard showing fix history across a project
- Git integration: auto-commit verified fixes with descriptive message
- `--explain-only` mode: explain without patching (good for learning)
- Batch mode: fix every `.cpp` in a directory

## Demo script (60 seconds)

```bash
# 1. Show the broken file
cat tests/demo_bugs.cpp

# 2. Show it fails to compile
clang++ tests/demo_bugs.cpp

# 3. Run codemedic with auto-apply
./build/codemedic -y tests/demo_bugs.cpp

# 4. Show it now compiles
clang++ tests/demo_bugs.cpp && echo "Compiles cleanly."
```
