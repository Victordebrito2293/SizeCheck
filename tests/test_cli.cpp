// test_cli.cpp -- Unit tests for command line argument parsing.

#include "cli.hpp"

#include "test_harness.hpp"

#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace {

sc::Result<sc::CliOptions> parse(std::initializer_list<const char *> list) {
  std::vector<std::string_view> args;
  args.reserve(list.size());
  for (const char *arg : list) {
    args.emplace_back(arg);
  }
  return sc::parse_args(args);
}

std::string error_message(const sc::Result<sc::CliOptions> &result) {
  return result.error().message;
}

} // namespace

TEST("cli defaults") {
  const auto result = parse({});
  CHECK(result.has_value());
  CHECK_EQ(result.value().root.generic_string(), std::string("."));
  CHECK_EQ(result.value().top, static_cast<std::size_t>(10));
  CHECK_EQ(result.value().max_depth, static_cast<std::size_t>(-1));
  CHECK_EQ(result.value().json, false);
  CHECK_EQ(result.value().color, true);
  CHECK_EQ(result.value().include_hidden, false);
  CHECK_EQ(result.value().help, false);
  CHECK_EQ(result.value().version, false);
  CHECK(result.value().exclude_names.empty());
}

TEST("cli positional path") {
  const auto result = parse({"./project"});
  CHECK(result.has_value());
  CHECK_EQ(result.value().root.generic_string(), std::string("./project"));
}

TEST("cli options") {
  const auto result = parse({"./project", "--top", "20", "--depth", "3",
                             "--json", "--no-color", "--hidden",
                             "--exclude", "node_modules",
                             "--exclude", ".git"});
  CHECK(result.has_value());
  CHECK_EQ(result.value().top, static_cast<std::size_t>(20));
  CHECK_EQ(result.value().max_depth, static_cast<std::size_t>(3));
  CHECK_EQ(result.value().json, true);
  CHECK_EQ(result.value().color, false);
  CHECK_EQ(result.value().include_hidden, true);
  CHECK_EQ(result.value().exclude_names.size(), static_cast<std::size_t>(2));
  CHECK_EQ(result.value().exclude_names[0], std::string("node_modules"));
  CHECK_EQ(result.value().exclude_names[1], std::string(".git"));
}

TEST("cli inline option values") {
  const auto result = parse({"--top=5", "--depth=1", "--exclude=build",
                             "--json"});
  CHECK(result.has_value());
  CHECK_EQ(result.value().top, static_cast<std::size_t>(5));
  CHECK_EQ(result.value().max_depth, static_cast<std::size_t>(1));
  CHECK_EQ(result.value().exclude_names.size(), static_cast<std::size_t>(1));
  CHECK_EQ(result.value().exclude_names[0], std::string("build"));
}

TEST("cli help and version flags") {
  const auto help = parse({"--help"});
  CHECK(help.has_value());
  CHECK_EQ(help.value().help, true);

  const auto version = parse({"-V"});
  CHECK(version.has_value());
  CHECK_EQ(version.value().version, true);
}

TEST("cli help and version output") {
  CHECK_CONTAINS(sc::help_text(), "Usage:");
  CHECK_CONTAINS(sc::help_text(), "--top");
  CHECK_CONTAINS(sc::help_text(), "--exclude");
  CHECK_EQ(sc::version_text(), std::string("sizecheck ") + SIZECHECK_VERSION);
}

TEST("cli unknown option") {
  const auto result = parse({"--delete"});
  CHECK(!result.has_value());
  CHECK_CONTAINS(error_message(result), "unknown option");
}

TEST("cli too many positional arguments") {
  const auto result = parse({"a", "b"});
  CHECK(!result.has_value());
  CHECK_CONTAINS(error_message(result), "unexpected argument");
}

TEST("cli top zero is rejected") {
  const auto result = parse({"--top", "0"});
  CHECK(!result.has_value());
  CHECK_CONTAINS(error_message(result), "--top");
}

TEST("cli top non-numeric rejected") {
  const auto result = parse({"--top", "abc"});
  CHECK(!result.has_value());
  CHECK_CONTAINS(error_message(result), "invalid value");

  const auto negative = parse({"--top", "-5"});
  CHECK(!negative.has_value());
}

TEST("cli depth overflow rejected") {
  const auto result = parse({"--depth", "99999999999999999999999"});
  CHECK(!result.has_value());
  CHECK_CONTAINS(error_message(result), "invalid value");
}

TEST("cli depth negative rejected") {
  const auto result = parse({"--depth", "-1"});
  CHECK(!result.has_value());
}

TEST("cli missing value rejected") {
  const auto top = parse({"--top"});
  CHECK(!top.has_value());
  CHECK_CONTAINS(error_message(top), "requires a value");

  const auto depth = parse({"--depth"});
  CHECK(!depth.has_value());

  const auto exclude = parse({"--exclude"});
  CHECK(!exclude.has_value());
}

TEST("cli empty exclude rejected") {
  const auto result = parse({"--exclude="});
  CHECK(!result.has_value());
  CHECK_CONTAINS(error_message(result), "requires a non-empty value");
}

TEST("cli double dash ends options") {
  const auto result = parse({"--", "./-odd-name"});
  CHECK(result.has_value());
  CHECK_EQ(result.value().root.generic_string(), std::string("./-odd-name"));
}

TEST("cli single dash is a path") {
  const auto result = parse({"-"});
  CHECK(result.has_value());
  CHECK_EQ(result.value().root.generic_string(), std::string("-"));
}