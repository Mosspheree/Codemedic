cmake_minimum_required(VERSION 3.16)
project(codemedic VERSION 2.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(LLVM REQUIRED CONFIG)
find_package(Clang REQUIRED CONFIG)
find_package(CURL REQUIRED)

include(FetchContent)
FetchContent_Declare(
  nlohmann_json
  URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(nlohmann_json)

set(SOURCES
  src/main.cpp
  src/compiler_runner.cpp
  src/error_parser.cpp
  src/context_extractor.cpp
  src/llm_client.cpp
  src/patch_applier.cpp
  src/terminal_ui.cpp
  src/root_cause_analyzer.cpp
  src/git_integration.cpp
  src/batch_processor.cpp
)

add_executable(codemedic ${SOURCES})

target_include_directories(codemedic PRIVATE
  include
  ${LLVM_INCLUDE_DIRS}
  ${CLANG_INCLUDE_DIRS}
)

target_link_libraries(codemedic PRIVATE
  clang-cpp
  LLVM
  CURL::libcurl
  nlohmann_json::nlohmann_json
)

target_compile_definitions(codemedic PRIVATE
  ${LLVM_DEFINITIONS}
)

install(TARGETS codemedic DESTINATION bin)
