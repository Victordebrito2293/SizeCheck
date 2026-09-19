#include "analyzer.hpp"

#include <algorithm>
#include <utility>

namespace sc {

namespace {

// Heap comparator used to keep only the largest elements: the heap root is
// the smallest tracked element, the one to evict when the heap overflows.
struct EvictSmallest {
  bool operator()(const FileEntry &a, const FileEntry &b) const {
    return a.size > b.size;
  }
};

struct LargestFirst {
  bool operator()(const FileEntry &a, const FileEntry &b) const {
    return a.size > b.size;
  }
};

} // namespace

Analyzer::Analyzer(std::size_t top_n) : limit_(top_n == 0 ? 1 : top_n) {}

void Analyzer::on_file(const FileEntry &entry) {
  largest_files_.push_back(entry);
  std::push_heap(largest_files_.begin(), largest_files_.end(),
                 EvictSmallest{});
  if (largest_files_.size() > limit_) {
    std::pop_heap(largest_files_.begin(), largest_files_.end(),
                  EvictSmallest{});
    largest_files_.pop_back();
  }
}

void Analyzer::on_directory(const FileEntry &entry) {
  if (entry.size == 0) {
    // Empty directories are noise in a "largest directories" list.
    return;
  }
  largest_directories_.push_back(entry);
  std::push_heap(largest_directories_.begin(), largest_directories_.end(),
                 EvictSmallest{});
  if (largest_directories_.size() > limit_) {
    std::pop_heap(largest_directories_.begin(), largest_directories_.end(),
                  EvictSmallest{});
    largest_directories_.pop_back();
  }
}

void Analyzer::on_error(const std::string &relative_path, const Error &error) {
  constexpr std::size_t kMaxStoredErrors = 32;
  if (error_details_.size() < kMaxStoredErrors) {
    error_details_.push_back(ErrorDetail{relative_path, error.message});
  }
}

Analysis Analyzer::collect() const {
  std::vector<FileEntry> files = largest_files_;
  std::vector<FileEntry> dirs = largest_directories_;
  std::sort(files.begin(), files.end(), LargestFirst{});
  std::sort(dirs.begin(), dirs.end(), LargestFirst{});
  return Analysis{std::move(files), std::move(dirs), error_details_};
}

} // namespace sc