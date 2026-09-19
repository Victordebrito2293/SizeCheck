#include "cli.hpp"

#include <charconv>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace sc {

namespace {

Result<std::size_t> parse_count(std::string_view text, std::string_view option) {
  std::size_t value = 0;
  const auto result =
      std::from_chars(text.data(), text.data() + text.size(), value);
  if (result.ec != std::errc{} || result.ptr != text.data() + text.size()) {
    return Result<std::size_t>::err_message(
        "invalid value for " + std::string(option) + ": '" +
        std::string(text) + "' (expected a non-negative integer)");
  }
  return Result<std::size_t>::ok(value);
}

// Describes how an argument matches a value-taking option like "--top".
struct ValueMatch {
  bool matched = false;     // arg was --name or --name=value
  std::string_view inline_value; // non-empty when the inline form was used
};

[[nodiscard]] ValueMatch match_value_option(std::string_view arg,
                                            std::string_view name) {
  if (arg == name) {
    return ValueMatch{true, {}};
  }
  const std::string prefix = std::string(name) + "=";
  if (arg.size() >= prefix.size() && arg.substr(0, prefix.size()) == prefix) {
    return ValueMatch{true, arg.substr(prefix.size())};
  }
  return ValueMatch{false, {}};
}

} // namespace

Result<CliOptions> parse_args(std::span<const std::string_view> args) {
  CliOptions options;
  std::optional<std::string> positional;
  bool only_positional = false;

  std::size_t i = 0;
  while (i < args.size()) {
    const std::string_view arg = args[i];

    if (!only_positional && arg == "--") {
      only_positional = true;
      ++i;
      continue;
    }

    const bool is_option =
        !only_positional && arg.size() > 1 && arg.front() == '-' && arg != "-";

    if (!is_option) {
      if (positional.has_value()) {
        return Result<CliOptions>::err_message(
            "unexpected argument: " + std::string(arg));
      }
      positional = std::string(arg);
      ++i;
      continue;
    }

    if (arg == "--help" || arg == "-h") {
      options.help = true;
      ++i;
      continue;
    }
    if (arg == "--version" || arg == "-V") {
      options.version = true;
      ++i;
      continue;
    }
    if (arg == "--json") {
      options.json = true;
      ++i;
      continue;
    }
    if (arg == "--no-color") {
      options.color = false;
      ++i;
      continue;
    }
    if (arg == "--hidden") {
      options.include_hidden = true;
      ++i;
      continue;
    }

    // Value-taking options.
    bool handled = false;

    auto read_value = [&](std::string_view name) -> Result<std::string_view> {
      const ValueMatch match = match_value_option(arg, name);
      if (match.matched) {
        handled = true;
        if (!match.inline_value.empty()) {
          ++i;
          return Result<std::string_view>::ok(match.inline_value);
        }
        if (arg.size() > name.size() && arg[name.size()] == '=') {
          // The inline form `--name=` was used with an empty value.
          ++i;
          return Result<std::string_view>::err_message(
              std::string(name) + " requires a non-empty value");
        }
        if (i + 1 >= args.size()) {
          return Result<std::string_view>::err_message(
              std::string(name) + " requires a value");
        }
        ++i; // value argument
        ++i; // option argument
        return Result<std::string_view>::ok(args[i - 1]);
      }
      return Result<std::string_view>::err(make_error_message(""));
    };

    const auto top = read_value("--top");
    if (handled) {
      if (!top.has_value()) {
        return Result<CliOptions>::err(Error{{}, top.error().message});
      }
      auto parsed = parse_count(top.value(), "--top");
      if (!parsed.has_value()) {
        return Result<CliOptions>::err(Error{{}, parsed.error().message});
      }
      if (parsed.value() == 0) {
        return Result<CliOptions>::err_message("--top must be at least 1");
      }
      options.top = parsed.value();
      continue;
    }

    const auto depth = read_value("--depth");
    if (handled) {
      if (!depth.has_value()) {
        return Result<CliOptions>::err(Error{{}, depth.error().message});
      }
      auto parsed = parse_count(depth.value(), "--depth");
      if (!parsed.has_value()) {
        return Result<CliOptions>::err(Error{{}, parsed.error().message});
      }
      options.max_depth = parsed.value();
      continue;
    }

    const auto exclude = read_value("--exclude");
    if (handled) {
      if (!exclude.has_value()) {
        return Result<CliOptions>::err(Error{{}, exclude.error().message});
      }
      options.exclude_names.emplace_back(exclude.value());
      continue;
    }

    return Result<CliOptions>::err_message(
        "unknown option: " + std::string(arg));
  }

  if (positional.has_value()) {
    options.root = std::filesystem::path(*positional);
  }

  return Result<CliOptions>::ok(std::move(options));
}

std::string help_text() {
  return R"(SizeCheck - analyze how much disk space files and directories use

Usage:
  sizecheck [OPTIONS] [PATH]

Arguments:
  PATH                 Directory or file to analyze (default: ".")

Options:
  -h, --help           Show this help and exit
  -V, --version        Show version and exit
      --top N          Show the N largest files and directories (default: 10)
      --depth N        Only traverse N directory levels below PATH
                       (0 scans only the top level; default: unlimited)
      --exclude NAME   Skip entries whose name is NAME (can be repeated)
      --hidden         Include hidden (dot-prefixed) files and directories
      --json           Emit a machine-readable JSON report
      --no-color       Disable colored output

Examples:
  sizecheck .
  sizecheck ./project --top 20
  sizecheck ./project --depth 3
  sizecheck ./project --exclude node_modules --exclude .git
  sizecheck ./project --json

Prints "Files", "Directories", "Total size", the largest directories and the
largest files, plus the scan duration.  Never modifies, links, moves or
deletes anything; the tool only reads metadata.
)";
}

std::string version_text() {
  return std::string("sizecheck ") + SIZECHECK_VERSION;
}

} // namespace sc