// tests/fuzz/fuzz_cli.cpp -- libFuzzer harness for sc::parse_args.
//
// The argument parser must never crash, hang, or read out of bounds no matter
// what bytes it is handed.  Each input is split on NUL bytes into "tokens" and
// fed to parse_args as though they were command line arguments.
//
// Build with: -DSIZECHECK_BUILD_FUZZERS=ON and a Clang toolchain.

#include "cli.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data,
                                      std::size_t size) {
  std::vector<std::string> tokens;
  std::size_t start = 0;
  for (std::size_t i = 0; i < size; ++i) {
    if (data[i] == 0) {
      tokens.emplace_back(reinterpret_cast<const char *>(data + start),
                          i - start);
      start = i + 1;
    }
  }
  if (start < size) {
    tokens.emplace_back(reinterpret_cast<const char *>(data + start),
                        size - start);
  }

  std::vector<std::string_view> args;
  args.reserve(tokens.size());
  for (const std::string &token : tokens) {
    args.emplace_back(token);
  }
  (void)sc::parse_args(args);
  return 0;
}