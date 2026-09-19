// test_report.cpp -- Tests for the text and JSON report renderers.

#include "report.hpp"

#include "test_harness.hpp"

#include <string>

namespace {

sc::ScanSummary make_summary() {
  sc::ScanSummary summary;
  summary.stats.files = 12842;
  summary.stats.directories = 1284;
  summary.stats.total_bytes = static_cast<sc::byte_count>(4103116800ULL);
  summary.stats.access_errors = 0;
  return summary;
}

sc::Analysis make_analysis() {
  sc::Analysis analysis;
  analysis.largest_files.push_back(
      sc::FileEntry{std::filesystem::path("assets/video.mp4"), 148897792});
  analysis.largest_directories.push_back(
      sc::FileEntry{std::filesystem::path("node_modules"),
                    842ULL * 1024ULL * 1024ULL});
  return analysis;
}

} // namespace

TEST("text report contains aggregate rows") {
  const std::string report = sc::render_text_report(
      make_summary(), make_analysis(), "./project",
      sc::TextReportOptions{false, 1.42});

  CHECK_CONTAINS(report, "SizeCheck");
  CHECK_CONTAINS(report, "Scanning: ./project");
  CHECK_CONTAINS(report, "Files:" + std::string(8, ' ') + "12,842");
  CHECK_CONTAINS(report, "Directories:" + std::string(2, ' ') + "1,284");
  CHECK_CONTAINS(report, "Total size:" + std::string(3, ' ') + "3.82 GB");
  CHECK_CONTAINS(report, "Scan completed in 1.42s");
}

TEST("text report lists directories and files") {
  const std::string report = sc::render_text_report(
      make_summary(), make_analysis(), "./project",
      sc::TextReportOptions{false, 1.42});

  CHECK_CONTAINS(report, "Largest directories:");
  CHECK_CONTAINS(report, "node_modules/");
  CHECK_CONTAINS(report, "842 MB");
  CHECK_CONTAINS(report, "Largest files:");
  CHECK_CONTAINS(report, "assets/video.mp4");
  CHECK_CONTAINS(report, "142 MB");
}

TEST("text report no color mode contains no escape codes") {
  const std::string report = sc::render_text_report(
      make_summary(), make_analysis(), ".",
      sc::TextReportOptions{false, 0.5});
  CHECK_NOT_CONTAINS(report, "\x1b[");
}

TEST("text report color mode uses escape codes") {
  const std::string report = sc::render_text_report(
      make_summary(), make_analysis(), ".",
      sc::TextReportOptions{true, 0.5});
  CHECK_CONTAINS(report, "\x1b[");
}

TEST("text report empty lists") {
  sc::ScanSummary summary = make_summary();
  summary.stats.files = 0;
  summary.stats.directories = 0;
  summary.stats.total_bytes = 0;
  const sc::Analysis empty_analysis;

  const std::string report = sc::render_text_report(
      summary, empty_analysis, ".", sc::TextReportOptions{false, 0.5});

  CHECK_CONTAINS(report, "(none)");
  CHECK_CONTAINS(report, "0 B");
}

TEST("text report shows access errors and reasons") {
  sc::ScanSummary summary = make_summary();
  summary.stats.access_errors = 2;
  sc::Analysis analysis = make_analysis();
  analysis.error_details.push_back(
      sc::ErrorDetail{"private/data", "Permission denied"});

  const std::string report = sc::render_text_report(
      summary, analysis, ".", sc::TextReportOptions{false, 1.0});

  CHECK_CONTAINS(report, "Could not access: private/data");
  CHECK_CONTAINS(report, "Reason: Permission denied");
  CHECK_CONTAINS(report, "2 access error(s)");
}

TEST("json report contains expected keys and values") {
  const std::string json = sc::render_json_report(
      make_summary(), make_analysis(), ".", 1.42);

  CHECK_CONTAINS(json, "\"path\": \".\"");
  CHECK_CONTAINS(json, "\"files\": 12842");
  CHECK_CONTAINS(json, "\"directories\": 1284");
  CHECK_CONTAINS(json, "\"total_bytes\": 4103116800");
  CHECK_CONTAINS(json, "\"errors\": 0");
  CHECK_CONTAINS(json, "\"scan_duration_ms\": 1420");
  CHECK_CONTAINS(json, "\"largest_files\"");
  CHECK_CONTAINS(json, "\"largest_directories\"");
  CHECK_CONTAINS(json, "assets/video.mp4");
  CHECK_CONTAINS(json, "148897792");
}

TEST("json report escapes special path characters") {
  sc::ScanSummary summary = make_summary();
  sc::Analysis analysis;
  sc::FileEntry weird;
  weird.relative_path = std::filesystem::path("file\"name.txt");
  weird.size = 5;
  analysis.largest_files.push_back(weird);

  const std::string json = sc::render_json_report(summary, analysis, ".", 0.1);

  CHECK_CONTAINS(json, "file\\\"name.txt");
}

TEST("json report paths do not leak absolute information") {
  sc::ScanSummary summary = make_summary();
  sc::Analysis analysis;
  sc::FileEntry file;
  file.relative_path = std::filesystem::path("src/main.cpp");
  file.size = 10;
  analysis.largest_files.push_back(file);

  const std::string json = sc::render_json_report(summary, analysis, ".", 0.1);

  // The requested root is echoed verbatim only if the user typed it (here ".")
  // and entry paths stay relative.
  CHECK_CONTAINS(json, "\"path\": \".\"");
  CHECK_CONTAINS(json, "src/main.cpp");
  CHECK_NOT_CONTAINS(json, "Users/");
  CHECK_NOT_CONTAINS(json, "C:\\");
}