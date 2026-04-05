cmake_minimum_required(VERSION 3.16)
project(fixcc VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ── Find libclang ──────────────────────────────────────────────────────────────
find_package(LLVM REQUIRED CONFIG)
find_package(Clang REQUIRED CONFIG)

message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

# ── Find libcurl (for LLM API calls) ──────────────────────────────────────────
find_package(CURL REQUIRED)

# ── nlohmann/json (header-only, fetch if not found) ───────────────────────────
include(FetchContent)
FetchContent_Declare(
  nlohmann_json
  URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(nlohmann_json)

# ── Sources ───────────────────────────────────────────────────────────────────
set(SOURCES
  src/main.cpp
  src/compiler_runner.cpp
  src/error_parser.cpp
  src/context_extractor.cpp
  src/llm_client.cpp
  src/patch_applier.cpp
  src/terminal_ui.cpp
)

add_executable(fixcc ${SOURCES})

target_include_directories(fixcc PRIVATE
  include
  ${LLVM_INCLUDE_DIRS}
  ${CLANG_INCLUDE_DIRS}
)

target_link_libraries(fixcc PRIVATE
  clang-cpp
  LLVM
  CURL::libcurl
  nlohmann_json::nlohmann_json
)

target_compile_definitions(fixcc PRIVATE
  ${LLVM_DEFINITIONS}
)

# ── Install ───────────────────────────────────────────────────────────────────
install(TARGETS fixcc DESTINATION bin)
