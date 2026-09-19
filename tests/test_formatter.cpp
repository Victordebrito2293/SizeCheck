// test_formatter.cpp -- Unit tests for byte, count, duration and JSON
// string formatting.

#include "formatter.hpp"

#include "test_harness.hpp"

#include <cstdint>
#include <limits>
#include <string>

namespace {

using sc::format_bytes;
using sc::format_count;
using sc::format_duration;
using sc::json_escape;

} // namespace

TEST("format_bytes empty") { CHECK_EQ(format_bytes(0), std::string("0 B")); }

TEST("format_bytes below kilobyte") {
  CHECK_EQ(format_bytes(1), std::string("1 B"));
  CHECK_EQ(format_bytes(1023), std::string("1023 B"));
}

TEST("format_bytes kilobyte boundary") {
  CHECK_EQ(format_bytes(1024), std::string("1 KB"));
  CHECK_EQ(format_bytes(1536), std::string("1.5 KB"));
  CHECK_EQ(format_bytes(3424), std::string("3.34 KB"));
}

TEST("format_bytes megabyte and mixed") {
  CHECK_EQ(format_bytes(1024 * 1024), std::string("1 MB"));
  CHECK_EQ(format_bytes(842 * 1024 * 1024), std::string("842 MB"));
  CHECK_EQ(format_bytes(87 * 1024 * 1024), std::string("87 MB"));
}

TEST("format_bytes gigabyte with decimals") {
  // 3.82 GB (see README example).
  const sc::byte_count value = static_cast<sc::byte_count>(4103116800ULL);
  CHECK_EQ(format_bytes(value), std::string("3.82 GB"));
  CHECK_EQ(format_bytes(1024ULL * 1024 * 1024), std::string("1 GB"));
}

TEST("format_bytes terabyte and beyond") {
  const std::uintmax_t tb =
      static_cast<std::uintmax_t>(1024) * 1024 * 1024 * 1024;
  CHECK_EQ(format_bytes(tb), std::string("1 TB"));
  // Extremely large value must not overflow or produce garbage.
  const std::uintmax_t huge = std::numeric_limits<std::uintmax_t>::max();
  CHECK_CONTAINS(format_bytes(huge), "EB");
  CHECK_NE(format_bytes(huge).empty(), true);
}

TEST("format_bytes trims trailing zeros") {
  CHECK_EQ(format_bytes(1500), std::string("1.46 KB"));
  CHECK_EQ(format_bytes(1024 + 204), std::string("1.2 KB"));
}

TEST("format_count grouping") {
  CHECK_EQ(format_count(0), std::string("0"));
  CHECK_EQ(format_count(999), std::string("999"));
  CHECK_EQ(format_count(1000), std::string("1,000"));
  CHECK_EQ(format_count(12842), std::string("12,842"));
  CHECK_EQ(format_count(999999), std::string("999,999"));
  CHECK_EQ(format_count(1000000), std::string("1,000,000"));
}

TEST("format_duration") {
  CHECK_EQ(format_duration(0.0), std::string("0.00s"));
  CHECK_EQ(format_duration(1.42), std::string("1.42s"));
  CHECK_EQ(format_duration(0.5), std::string("0.50s"));
  CHECK_EQ(format_duration(12.345), std::string("12.34s"));
}

TEST("json_escape plain string") {
  CHECK_EQ(json_escape("hello"), std::string("\"hello\""));
}

TEST("json_escape special characters") {
  CHECK_EQ(json_escape("a\"b\\c"), std::string("\"a\\\"b\\\\c\""));
  CHECK_EQ(json_escape("line\nbreak"), std::string("\"line\\nbreak\""));
  CHECK_EQ(json_escape("tab\there"), std::string("\"tab\\there\""));
  CHECK_CONTAINS(json_escape(std::string("ctl\x01", 4)), "\\u0001");
}

TEST("json_escape utf8 passthrough") {
  // Non-ASCII bytes pass through untouched.
  const std::string utf8 = "caf\xC3\xA9";
  CHECK_EQ(json_escape(utf8), std::string("\"caf\xC3\xA9\""));
}