// report.hpp -- Human-readable and machine-readable report rendering.

#ifndef SIZECHECK_REPORT_HPP
#define SIZECHECK_REPORT_HPP

#include "analyzer.hpp"
#include "scanner.hpp"

#include <string>

namespace sc {

// How the text report should be rendered.
struct TextReportOptions {
  bool color = false;
  double duration_seconds = 0.0;
};

// True when SizeCheck is writing to a terminal (stdout).  Used to decide
// whether ANSI color codes are appropriate; consumers may override `color`.
[[nodiscard]] bool stdout_is_terminal() noexcept;

// Render a human-readable report.  All paths in `analysis` are relative to
// the scanned root; `requested_root` is echoed verbatim because the user
// explicitly typed it.
[[nodiscard]] std::string render_text_report(const ScanSummary &summary,
                                             const Analysis &analysis,
                                             const std::string &requested_root,
                                             const TextReportOptions &options);

// Render a JSON report.  No ANSI escapes are ever emitted, even when writing
// to a terminal.
[[nodiscard]] std::string render_json_report(
    const ScanSummary &summary, const Analysis &analysis,
    const std::string &requested_root, double duration_seconds);

} // namespace sc

#endif // SIZECHECK_REPORT_HPP
