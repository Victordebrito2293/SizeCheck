#include "report.hpp"

#include "formatter.hpp"

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#if defined(_WIN32)
#include <io.h>
#else
#include <unistd.h>
#endif

namespace sc {

namespace {

constexpr std::string_view kReset = "\x1b[0m";
constexpr std::string_view kBold = "\x1b[1m";
constexpr std::string_view kRed = "\x1b[31m";
constexpr std::string_view kDim = "\x1b[2m";

std::string paint(std::string_view text, std::string_view code, bool color) {
  if (!color) {
    return std::string(text);
  }
  return std::string(code) + std::string(text) + std::string(kReset);
}

std::string path_text(const std::filesystem::path &path, bool trailing_slash) {
  std::string text = path.generic_string();
  if (trailing_slash && !text.empty() && text.back() != '/') {
    text.push_back('/');
  }
  return text;
}

std::size_t max_path_width(const std::vector<FileEntry> &entries,
                           bool trailing_slash) {
  std::size_t width = 0;
  for (const FileEntry &entry : entries) {
    width = std::max(width, path_text(entry.relative_path, trailing_slash).size());
  }
  return width;
}

void pad_to(std::string &text, std::size_t width) {
  if (text.size() < width) {
    text.append(width - text.size(), ' ');
  }
}

} // namespace

bool stdout_is_terminal() noexcept {
#if defined(_WIN32)
  return static_cast<bool>(_isatty(_fileno(stdout)));
#else
  return ::isatty(::fileno(stdout)) != 0;
#endif
}

std::string render_text_report(const ScanSummary &summary,
                               const Analysis &analysis,
                               const std::string &requested_root,
                               const TextReportOptions &options) {
  const bool color = options.color;

  std::ostringstream out;

  out << paint("SizeCheck", kBold, color) << "\n\n";
  out << "Scanning: " << requested_root << "\n\n";

  out << "Files:        " << format_count(summary.stats.files) << "\n";
  out << "Directories:  " << format_count(summary.stats.directories) << "\n";
  out << "Total size:   "
      << paint(format_bytes(summary.stats.total_bytes), kBold, color) << "\n";

  // Largest directories.
  out << "\n" << paint("Largest directories:", kBold, color) << "\n";
  if (analysis.largest_directories.empty()) {
    out << "(none)\n";
  } else {
    constexpr bool kTrailingSlash = true;
    const std::size_t path_width =
        max_path_width(analysis.largest_directories, kTrailingSlash);
    std::size_t index = 1;
    for (const FileEntry &entry : analysis.largest_directories) {
      std::string left = std::to_string(index) + ". " +
                         path_text(entry.relative_path, kTrailingSlash);
      pad_to(left, path_width + 5);
      out << left << format_bytes(entry.size) << "\n";
      ++index;
    }
  }

  // Largest files.
  out << "\n" << paint("Largest files:", kBold, color) << "\n";
  if (analysis.largest_files.empty()) {
    out << "(none)\n";
  } else {
    for (const FileEntry &entry : analysis.largest_files) {
      out << format_bytes(entry.size) << "  "
          << path_text(entry.relative_path, false) << "\n";
    }
  }

  for (const ErrorDetail &detail : analysis.error_details) {
    out << "\n"
        << paint("Could not access: ", kRed, color) << detail.path << "\n"
        << paint("Reason: ", kRed, color) << detail.reason << "\n";
  }
  const std::size_t hidden = summary.stats.access_errors > analysis.error_details.size()
                                 ? (summary.stats.access_errors -
                                    static_cast<std::uint64_t>(analysis.error_details.size()))
                                 : 0;
  if (hidden > 0) {
    out << "\n"
        << "  ... and " << hidden << " more access errors.\n";
  }
  if (summary.stats.access_errors > 0) {
    out << "\n"
        << paint(std::to_string(summary.stats.access_errors) + " access error(s)",
                 kRed, color)
        << "\n";
  }

  out << "\n" << paint("Scan completed in ", kDim, color)
      << format_duration(options.duration_seconds) << "\n";

  return out.str();
}

std::string render_json_report(const ScanSummary &summary,
                               const Analysis &analysis,
                               const std::string &requested_root,
                               double duration_seconds) {
  std::ostringstream out;
  out << "{\n";

  // requested_root is echoed only because the user explicitly typed it.
  out << "  \"path\": " << json_escape(requested_root) << ",\n";
  out << "  \"files\": " << summary.stats.files << ",\n";
  out << "  \"directories\": " << summary.stats.directories << ",\n";
  out << "  \"total_bytes\": " << summary.stats.total_bytes << ",\n";

  out << "  \"largest_files\": [";
  if (!analysis.largest_files.empty()) {
    out << "\n";
    const auto end = analysis.largest_files.end();
    for (auto it = analysis.largest_files.begin(); it != end; ++it) {
      out << "    {\n";
      out << "      \"path\": "
          << json_escape(path_text(it->relative_path, false)) << ",\n";
      out << "      \"bytes\": " << it->size << "\n";
      out << "    }";
      if (std::next(it) != end) {
        out << ",";
      }
      out << "\n";
    }
    out << "  ]";
  } else {
    out << "]";
  }
  out << ",\n";

  out << "  \"largest_directories\": [";
  if (!analysis.largest_directories.empty()) {
    out << "\n";
    const auto end = analysis.largest_directories.end();
    for (auto it = analysis.largest_directories.begin(); it != end; ++it) {
      out << "    {\n";
      out << "      \"path\": "
          << json_escape(path_text(it->relative_path, false)) << ",\n";
      out << "      \"bytes\": " << it->size << "\n";
      out << "    }";
      if (std::next(it) != end) {
        out << ",";
      }
      out << "\n";
    }
    out << "  ]";
  } else {
    out << "]";
  }
  out << ",\n";

  out << "  \"errors\": " << summary.stats.access_errors << ",\n";
  out << "  \"scan_duration_ms\": "
      << static_cast<long long>(duration_seconds * 1000.0 + 0.5) << "\n";
  out << "}";
  return out.str();
}

} // namespace sc