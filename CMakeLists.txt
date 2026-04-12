cmake_minimum_required(VERSION 3.16)
project(codemedic VERSION 3.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Warnings
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wno-unused-parameter)
endif()

find_package(CURL REQUIRED)
find_package(Threads REQUIRED)

include(FetchContent)
FetchContent_Declare(
  nlohmann_json
  URL "https://github.com/nlohmann/json/releases/download/v3.11.3/json.zip"
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
  src/session_logger.cpp
  src/config_loader.cpp
)

add_executable(codemedic ${SOURCES})

target_include_directories(codemedic PRIVATE include)

target_link_libraries(codemedic PRIVATE
  CURL::libcurl
  nlohmann_json::nlohmann_json
  Threads::Threads
)

target_compile_definitions(codemedic PRIVATE
  CODEMEDIC_VERSION="${PROJECT_VERSION}"
)

install(TARGETS codemedic DESTINATION bin)

# Install shell completions (optional)
install(FILES completions/codemedic.bash
  DESTINATION share/bash-completion/completions
  RENAME codemedic OPTIONAL)
install(FILES completions/codemedic.zsh
  DESTINATION share/zsh/site-functions
  RENAME _codemedic OPTIONAL)
