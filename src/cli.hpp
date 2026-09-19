// cli.hpp -- Arguments parsing for the sizecheck command line.

#ifndef SIZECHECK_CLI_HPP
#define SIZECHECK_CLI_HPP

#include "errors.hpp"

#include <cstddef>
#include <filesystem>
#include <span>
#include <string>
#include <vector>

namespace sc {

struct CliOptions {
  std::filesystem::path root = "."; // positional argument, defaults to "."
  std::size_t top = 10;             // entries per top-N list
  std::size_t max_depth =
      static_cast<std::size_t>(-1); // unlimited traversal depth
  bool json = false;
  bool color = true;
  bool include_hidden = false;
  std::vector<std::string> exclude_names;
  bool help = false;
  bool version = false;
};

// Parse the argument vector (excluding the program name).  On success the
// options are filled in; on failure an Error explaining the problem is
// returned.  Strict parsing: numbers are validated conservatively and
// unexpected arguments are rejected.
[[nodiscard]] Result<CliOptions> parse_args(std::span<const std::string_view> args);

// Plain-text usage message for --help.
[[nodiscard]] std::string help_text();

// Version string, e.g. "sizecheck 1.0.0".
[[nodiscard]] std::string version_text();

} // namespace sc

#endif // SIZECHECK_CLI_HPP