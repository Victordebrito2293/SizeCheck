cmake_minimum_required(VERSION 3.20)

project(
  SizeCheck
  VERSION 1.0.0
  DESCRIPTION "A small, fast, safe, offline CLI that analyzes disk usage"
  HOMEPAGE_URL "https://github.com/Victordebrito2293/SizeCheck"
  LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ---------------------------------------------------------------------------
# Project options
# ---------------------------------------------------------------------------
option(SIZECHECK_BUILD_TESTS "Build the test suite" ON)
option(SIZECHECK_BUILD_FUZZERS "Build libFuzzer harnesses (Clang only)" OFF)
option(SIZECHECK_SANITIZE "Build with AddressSanitizer + UndefinedBehaviorSanitizer" OFF)
option(SIZECHECK_WERROR "Treat compiler warnings as errors" OFF)
option(SIZECHECK_ENABLE_CLANG_TIDY
       "Run clang-tidy as part of the build (requires clang-tidy)" OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE "Release"
      CACHE STRING "Build type" FORCE)
  set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS
               "Debug;Release;RelWithDebInfo;MinSizeRel")
endif()

# ---------------------------------------------------------------------------
# Compiler warnings
# ---------------------------------------------------------------------------
function(sizecheck_set_warnings target)
  if(MSVC)
    target_compile_options(${target} PRIVATE /W4 /permissive- /utf-8)
    if(SIZECHECK_WERROR)
      target_compile_options(${target} PRIVATE /WX)
    endif()
  else()
    target_compile_options(${target} PRIVATE
                           -Wall
                           -Wextra
                           -Wpedantic
                           -Wshadow
                           -Wconversion
                           -Wsign-conversion
                           -Wnull-dereference
                           -Wfloat-equal
                           -Wcast-qual
                           -Wold-style-cast)
    if(SIZECHECK_WERROR)
      target_compile_options(${target} PRIVATE -Werror)
    endif()
  endif()
endfunction()

# ---------------------------------------------------------------------------
# Sanitizers (GCC / Clang)
# ---------------------------------------------------------------------------
function(sizecheck_setup_sanitizers target)
  if(SIZECHECK_SANITIZE AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(${target} PRIVATE
                           -fsanitize=address,undefined
                           -fno-omit-frame-pointer)
    target_link_options(${target} PRIVATE
                        -fsanitize=address,undefined)
  endif()
endfunction()

# ---------------------------------------------------------------------------
# Core library
# ---------------------------------------------------------------------------
add_library(sizecheck_core STATIC
  src/cli.cpp
  src/analyzer.cpp
  src/formatter.cpp
  src/report.cpp
  src/scanner.cpp)

target_include_directories(sizecheck_core PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
  $<INSTALL_INTERFACE:include>)

target_compile_features(sizecheck_core PUBLIC cxx_std_20)
target_compile_definitions(sizecheck_core PUBLIC
  SIZECHECK_VERSION="${PROJECT_VERSION}")

sizecheck_set_warnings(sizecheck_core)
sizecheck_setup_sanitizers(sizecheck_core)

set_target_properties(sizecheck_core PROPERTIES
  POSITION_INDEPENDENT_CODE ON)

# Export compile commands for clang-tidy / IDE consumers.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ---------------------------------------------------------------------------
# Executable
# ---------------------------------------------------------------------------
add_executable(sizecheck src/main.cpp)
target_link_libraries(sizecheck PRIVATE sizecheck_core)
sizecheck_set_warnings(sizecheck)
sizecheck_setup_sanitizers(sizecheck)

install(TARGETS sizecheck
  RUNTIME DESTINATION bin)

# ---------------------------------------------------------------------------
# clang-tidy integration
# ---------------------------------------------------------------------------
if(SIZECHECK_ENABLE_CLANG_TIDY)
  find_program(SIZECHECK_CLANG_TIDY_EXECUTABLE NAMES clang-tidy
                                             clang-tidy-18 clang-tidy-19)
  if(SIZECHECK_CLANG_TIDY_EXECUTABLE)
    set(CMAKE_CXX_CLANG_TIDY "${SIZECHECK_CLANG_TIDY_EXECUTABLE}"
        "--config-file=${CMAKE_CURRENT_SOURCE_DIR}/.clang-tidy")
    message(STATUS "clang-tidy: ${SIZECHECK_CLANG_TIDY_EXECUTABLE}")
  else()
    message(WARNING "SIZECHECK_ENABLE_CLANG_TIDY was requested but clang-tidy was not found.")
  endif()
endif()

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------
include(CTest)

if(SIZECHECK_BUILD_TESTS)
  add_subdirectory(tests)
endif()

# ---------------------------------------------------------------------------
# Fuzzing harnesses (Clang + libFuzzer only)
# ---------------------------------------------------------------------------
if(SIZECHECK_BUILD_FUZZERS)
  if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    add_executable(fuzz_cli tests/fuzz/fuzz_cli.cpp)
    target_link_libraries(fuzz_cli PRIVATE sizecheck_core)
    target_compile_options(fuzz_cli PRIVATE -fsanitize=fuzzer,address,undefined)
    target_compile_features(fuzz_cli PRIVATE cxx_std_20)
  else()
    message(FATAL_ERROR
            "SIZECHECK_BUILD_FUZZERS requires a Clang compiler (libFuzzer).")
  endif()
endif()

# ---------------------------------------------------------------------------
# Packaging
# ---------------------------------------------------------------------------
include(GNUInstallDirs)
install(TARGETS sizecheck RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})