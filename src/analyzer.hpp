// analyzer.hpp -- Aggregates scanner output: top-N files, top-N directories
// and a bounded list of access errors.

#ifndef SIZECHECK_ANALYZER_HPP
#define SIZECHECK_ANALYZER_HPP

#include "errors.hpp"
#include "scanner.hpp"

#include <cstddef>
#include <string>
#include <vector>

namespace sc {

// One reported access failure, with the path relative to the scanned root.
struct ErrorDetail {
  std::string path;
  std::string reason;
};

// Final derived data handed to the report layer.
struct Analysis {
  std::vector<FileEntry> largest_files;
  std::vector<FileEntry> largest_directories;
  std::vector<ErrorDetail> error_details;
};

// Turns scanner callbacks into the top-N lists and error details used by the
// report layer.  Not thread-safe; use one instance per scan.
class Analyzer final : public ScanVisitor {
public:
  explicit Analyzer(std::size_t top_n);

  void on_file(const FileEntry &entry) override;
  void on_directory(const FileEntry &entry) override;
  void on_error(const std::string &relative_path, const Error &error) override;

  [[nodiscard]] const std::vector<FileEntry> &largest_files() const noexcept {
    return largest_files_;
  }
  [[nodiscard]] const std::vector<FileEntry> &largest_directories() const
      noexcept {
    return largest_directories_;
  }
  [[nodiscard]] const std::vector<ErrorDetail> &error_details() const noexcept {
    return error_details_;
  }

  [[nodiscard]] Analysis collect() const;

private:
  std::size_t limit_ = 0;
  std::vector<FileEntry> largest_files_;
  std::vector<FileEntry> largest_directories_;
  std::vector<ErrorDetail> error_details_;
};

} // namespace sc

#endif // SIZECHECK_ANALYZER_HPP