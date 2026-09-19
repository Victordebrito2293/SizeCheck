#include "analyzer.hpp"
#include "cli.hpp"
#include "report.hpp"
#include "scanner.hpp"

#include <chrono>
#include <cstddef>
#include <iostream>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace {

int run_scan(const sc::CliOptions &options) {
  sc::ScanOptions scan_options;
  scan_options.root = options.root;
  scan_options.include_hidden = options.include_hidden;
  scan_options.max_depth = options.max_depth;
  scan_options.exclude_names = options.exclude_names;

  sc::Analyzer analyzer(options.top);

  const auto start = std::chrono::steady_clock::now();
  auto scan_result = sc::Scanner::scan(scan_options, analyzer);
  const auto finish = std::chrono::steady_clock::now();
  const double duration_seconds =
      std::chrono::duration<double>(finish - start).count();

  if (!scan_result.has_value()) {
    std::cerr << "sizecheck: " << scan_result.error().message << "\n";
    return 1;
  }

  const sc::Analysis analysis = analyzer.collect();
  const std::string requested_root = options.root.generic_string();

  if (options.json) {
    std::cout << sc::render_json_report(scan_result.value(), analysis,
                                        requested_root, duration_seconds)
              << "\n";
    return 0;
  }

  const bool color = options.color && sc::stdout_is_terminal();
  std::cout << sc::render_text_report(scan_result.value(), analysis,
                                      requested_root,
                                      sc::TextReportOptions{color,
                                                           duration_seconds});
  return 0;
}

} // namespace

int main(int argc, char **argv) {
  std::vector<std::string_view> args;
  args.reserve(argc > 0 ? static_cast<std::size_t>(argc) - 1 : 0);
  for (int i = 1; i < argc; ++i) {
    args.emplace_back(argv[i]);
  }

  auto parse_result = sc::parse_args(args);
  if (!parse_result.has_value()) {
    std::cerr << "sizecheck: " << parse_result.error().message << "\n"
              << "Try 'sizecheck --help' for more information.\n";
    return 2;
  }

  const sc::CliOptions &options = parse_result.value();

  if (options.help) {
    std::cout << sc::help_text();
    return 0;
  }
  if (options.version) {
    std::cout << sc::version_text() << "\n";
    return 0;
  }

  return run_scan(options);
}