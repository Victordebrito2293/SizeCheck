// formatter.hpp -- Human-friendly rendering of byte counts and numbers.

#ifndef SIZECHECK_FORMATTER_HPP
#define SIZECHECK_FORMATTER_HPP

#include <cstdint>
#include <string>
#include <string_view>

namespace sc {

using byte_count = std::uintmax_t;

// Format a byte count with binary factors (1024).  Examples:
//   0            -> "0 B"
//   842          -> "842 B"
//   1024         -> "1 KB"
//   4103116800   -> "3.82 GB"
// Trailing zeros are trimmed: 3424 -> "3.32 KB" (not "3.30 KB").
[[nodiscard]] std::string format_bytes(byte_count bytes);

// Format a non-negative integer with thousands separators:
// 12842 -> "12,842".
[[nodiscard]] std::string format_count(std::uint64_t count);

// Format a duration in seconds with two decimals: 1.42 -> "1.42s".
[[nodiscard]] std::string format_duration(double seconds);

// Escape a string for embedding inside a JSON string literal.  The returned
// value includes the surrounding double quotes.
[[nodiscard]] std::string json_escape(std::string_view text);

} // namespace sc

#endif // SIZECHECK_FORMATTER_HPP