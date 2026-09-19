// formatter.cpp -- Human-friendly rendering of byte counts and numbers.

#include "formatter.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <cstddef>
#include <string_view>

namespace sc {

namespace {

constexpr std::array<const char *, 7> kUnits = {"B", "KB", "MB", "GB",
                                                  "TB", "PB", "EB"};

// Build "N" or "N.MM" without <cstdio>.  Used only as a fallback if
// std::to_chars for doubles is unavailable on the toolchain.
std::string error_fallback(double value) {
  long long whole = static_cast<long long>(value);
  std::string out = std::to_string(whole);
  long long frac =
      static_cast<long long>((value - static_cast<double>(whole)) * 100.0);
  if (frac != 0) {
    out.push_back('.');
    if (frac < 10) {
      out.push_back('0');
    }
    out.append(std::to_string(frac));
  }
  return out;
}

// Fixed two-decimal rendering with trailing zeros trimmed.
void append_fixed_trimmed(std::string &out, double value) {
  char buffer[64];
  const auto result =
      std::to_chars(buffer, buffer + sizeof(buffer), value,
                    std::chars_format::fixed, 2);
  if (result.ec != std::errc{}) {
    out.append(error_fallback(value));
    return;
  }
  out.append(buffer, result.ptr);

  // Trim trailing zeros and a possible trailing dot ("3.50" -> "3.5",
  // "842.00" -> "842").
  const auto dot = out.find_last_of('.');
  if (dot != std::string::npos) {
    auto end = out.size();
    while (end > dot + 1 && out[end - 1] == '0') {
      --end;
    }
    if (end == dot + 1) {
      --end; // "x.0" -> "x"
    }
    out.resize(end);
  }
}

} // namespace

std::string format_bytes(byte_count bytes) {
  constexpr byte_count kUnit = 1024;

  std::size_t unit_index = 0;
  double value = static_cast<double>(bytes);
  while (value >= static_cast<double>(kUnit) &&
         unit_index + 1 < kUnits.size()) {
    value /= static_cast<double>(kUnit);
    ++unit_index;
  }

  if (unit_index == 0) {
    return std::to_string(bytes) + " B";
  }

  std::string out;
  append_fixed_trimmed(out, value);
  out.append(" ");
  out.append(kUnits[unit_index]);
  return out;
}

std::string format_count(std::uint64_t count) {
  std::string digits = std::to_string(count);
  std::string out;
  out.reserve(digits.size() + digits.size() / 3);
  const std::size_t first_group = digits.size() % 3;
  for (std::size_t i = 0; i < digits.size(); ++i) {
    if (i != 0 && (i % 3) == first_group) {
      out.push_back(',');
    }
    out.push_back(digits[i]);
  }
  return out;
}

std::string format_duration(double seconds) {
  const long long total_ms = static_cast<long long>(seconds * 1000.0 + 0.5);
  const long long whole = total_ms / 1000;
  const long long hundredths = (total_ms % 1000) / 10;
  return std::to_string(whole) + "." + (hundredths < 10 ? "0" : "") +
         std::to_string(hundredths) + "s";
}

std::string json_escape(std::string_view text) {
  std::string out;
  out.reserve(text.size() + 8);
  out.push_back('"');
  for (const char c : text) {
    switch (c) {
    case '"':
      out.append("\\\"");
      break;
    case '\\':
      out.append("\\\\");
      break;
    case '\b':
      out.append("\\b");
      break;
    case '\f':
      out.append("\\f");
      break;
    case '\n':
      out.append("\\n");
      break;
    case '\r':
      out.append("\\r");
      break;
    case '\t':
      out.append("\\t");
      break;
    default:
      if (static_cast<unsigned char>(c) < 0x20) {
        out.append("\\u00");
        const auto nibble = [](unsigned v) -> char {
          return v < 10 ? static_cast<char>('0' + v)
                        : static_cast<char>('a' + v - 10);
        };
        out.push_back(nibble(static_cast<unsigned>(c) >> 4));
        out.push_back(nibble(static_cast<unsigned>(c) & 0x0F));
      } else {
        out.push_back(c);
      }
      break;
    }
  }
  out.push_back('"');
  return out;
}

} // namespace sc