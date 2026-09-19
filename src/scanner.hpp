// scanner.hpp -- Filesystem traversal for SizeCheck.
//
// The scanner walks a directory tree (or a single file) and reports each
// regular file, each finished directory subtree, and every access failure
// through a visitor.  It never follows directory symlinks and only counts a
// file symlink's target when the resolved target stays inside the scanned
// tree, so it is safe against symlink loops, double counting and escapes.

#ifndef SIZECHECK_SCANNER_HPP
#define SIZECHECK_SCANNER_HPP

#include "errors.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <optional>
#include <string>
#include <vector>

namespace sc {

using byte_count = std::uintmax_t;

// A single entry reported to a visitor.  `relative_path` is always stored in
// forward-slash form and relative to the requested root ("" for the root
// itself).
struct FileEntry {
  std::filesystem::path relative_path;
  byte_count size = 0;
};

// Aggregate counters produced by a completed scan.
struct ScanStats {
  std::uint64_t files = 0;
  std::uint64_t directories = 0;
  byte_count total_bytes = 0;
  std::uint64_t access_errors = 0;

  void add(const ScanStats &other) {
    files += other.files;
    directories += other.directories;
    total_bytes += other.total_bytes;
    access_errors += other.access_errors;
  }
};

// Where and how to scan.
struct ScanOptions {
  std::filesystem::path root;
  bool include_hidden = false;
  std::size_t max_depth = std::numeric_limits<std::size_t>::max();
  std::vector<std::string> exclude_names;
};

// Summary returned on success.  `canonical_root` is the requested root
// normalized to an absolute path (useful for symlink containment checks).
struct ScanSummary {
  std::filesystem::path canonical_root;
  ScanStats stats;
};

// Receives the results of a traversal.  All methods may be called from a
// single thread only.
class ScanVisitor {
public:
  virtual ~ScanVisitor() = default;

  // A regular file was counted.  The root is never reported via this method.
  virtual void on_file(const FileEntry &entry) = 0;

  // A directory subtree finished; `entry.size` is the total bytes of all
  // counted files below it.  The root is never reported via this method.
  virtual void on_directory(const FileEntry &entry) = 0;

  // An entry listed by the filesystem could not be inspected or opened.
  virtual void on_error(const std::string &relative_path, const Error &error) = 0;
};

class Scanner {
public:
  Scanner() = delete;

  static Result<ScanSummary> scan(const ScanOptions &options,
                                  ScanVisitor &visitor);
};

// --- Testable policy helpers -------------------------------------------------

// True when the leaf name denotes a hidden file ("."-prefixed).  Applies on
// every platform for predictability; on POSIX this matches the shell
// convention.
[[nodiscard]] bool is_hidden_leaf(const std::filesystem::path &leaf);

// Component-wise containment test.  Requires both paths to be absolute and
// normalized (e.g. `weakly_canonical`).  `candidate == root` returns true.
[[nodiscard]] bool is_path_within(const std::filesystem::path &candidate,
                                  const std::filesystem::path &root);

// Size accounting for a symbolic link under the symlink policy described in
// the header comment.  `canonical_root` must be an absolute, normalized form
// of the scanned root.  On success the optional byte count means:
//   * a contained size   -> the link is counted as a regular file entry;
//   * `std::nullopt`     -> the link is skipped entirely (its target is a
//                           directory: never descended into, never counted);
// On failure the link is broken or looped and should be reported as an
// access error.
[[nodiscard]] Result<std::optional<byte_count>> symlink_counted_size(
    const std::filesystem::path &link,
    const std::filesystem::path &canonical_root);

} // namespace sc

#endif // SIZECHECK_SCANNER_HPP