#include "scanner.hpp"

#include <algorithm>
#include <memory>
#include <optional>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#if !defined(_WIN32)
#include <sys/stat.h>
#endif

namespace sc {

namespace {

bool is_hidden_name(std::string_view name) {
  return !name.empty() && name.front() == '.';
}

bool is_excluded_name(std::string_view name,
                      const std::vector<std::string> &excludes) {
  return std::find(excludes.begin(), excludes.end(), name) != excludes.end();
}

std::filesystem::path child_relative(const std::filesystem::path &base,
                                     std::string_view leaf) {
  if (base.empty()) {
    return std::filesystem::path(leaf);
  }
  return base / std::filesystem::path(leaf);
}

bool file_status_error(const std::filesystem::file_status &status) {
  return status.type() == std::filesystem::file_type::none ||
         status.type() == std::filesystem::file_type::unknown;
}

// The byte count of the link itself (the length of its target string on
// POSIX); 0 where the platform gives no portable way to read it.
byte_count symlink_self_size(const std::filesystem::path &link) noexcept {
#if !defined(_WIN32)
  struct ::stat info {};
  if (::lstat(link.c_str(), &info) == 0) {
    return static_cast<byte_count>(info.st_size);
  }
#endif
  return 0;
}

} // namespace

bool is_hidden_leaf(const std::filesystem::path &leaf) {
  return is_hidden_name(leaf.filename().generic_string());
}

bool is_path_within(const std::filesystem::path &candidate,
                    const std::filesystem::path &root) {
  auto candidate_it = candidate.begin();
  auto root_it = root.begin();
  for (; candidate_it != candidate.end() && root_it != root.end();
       ++candidate_it, ++root_it) {
    if (*candidate_it != *root_it) {
      return false;
    }
  }
  return root_it == root.end();
}

// Some platforms (notably Windows reparse points) do not fully resolve
// certain directory links through weak_canonical/status.  When resolution
// fails, re-read the raw link text: a link that points into a real directory
// is still skipped, and only genuinely broken or looping links are errors.
Result<std::optional<byte_count>> link_from_raw_target_or_error(
    const std::filesystem::path &link) {
  std::error_code ec;
  const std::filesystem::path raw = std::filesystem::read_symlink(link, ec);
  if (ec) {
    return Result<std::optional<byte_count>>::err(make_error(ec));
  }

  std::filesystem::path candidate = raw;
  if (candidate.is_relative()) {
    candidate = link.parent_path() / candidate;
  }
  const std::filesystem::file_status candidate_status =
      std::filesystem::status(candidate, ec);
  if (!ec &&
      candidate_status.type() == std::filesystem::file_type::directory) {
    return Result<std::optional<byte_count>>::ok(std::nullopt);
  }
  return Result<std::optional<byte_count>>::err(
      make_error_message("broken symbolic link"));
}

Result<std::optional<byte_count>> symlink_counted_size(
    const std::filesystem::path &link,
    const std::filesystem::path &canonical_root) {
  std::error_code ec;
  const std::filesystem::path resolved =
      std::filesystem::weakly_canonical(link, ec);
  if (ec) {
    // A dangling or looping link: report an access error, unless the raw
    // target turns out to be a skipped directory on this platform.
    return link_from_raw_target_or_error(link);
  }

  const std::filesystem::file_status target_status =
      std::filesystem::status(resolved, ec);
  const auto target_type = target_status.type();
  if (ec || target_type == std::filesystem::file_type::not_found ||
      target_type == std::filesystem::file_type::none) {
    return link_from_raw_target_or_error(link);
  }

  if (target_type == std::filesystem::file_type::directory) {
    // Directory symlinks are neither descended into nor counted.
    return Result<std::optional<byte_count>>::ok(std::nullopt);
  }

  if (is_path_within(resolved, canonical_root) &&
      target_type == std::filesystem::file_type::regular) {
    // A regular file inside the tree: count the target's bytes.
    ec.clear();
    const byte_count size = std::filesystem::file_size(link, ec);
    if (ec) {
      return Result<std::optional<byte_count>>::err(make_error(ec));
    }
    return Result<std::optional<byte_count>>::ok(size);
  }

  // Outside the scanned tree, or pointing at a special file: count only the
  // link itself, never its target.
  return Result<std::optional<byte_count>>::ok(symlink_self_size(link));
}

class Recorder {
public:
  Recorder(ScanVisitor &visitor, ScanStats &stats)
      : visitor_(visitor), stats_(stats) {}

  void file(const FileEntry &entry) {
    stats_.files += 1;
    stats_.total_bytes += entry.size;
    visitor_.on_file(entry);
  }

  void directory(const FileEntry &entry) { visitor_.on_directory(entry); }

  void error(const std::string &relative_path, const Error &error) {
    stats_.access_errors += 1;
    visitor_.on_error(relative_path, error);
  }

private:
  ScanVisitor &visitor_;
  ScanStats &stats_;
};

namespace {

struct Frame {
  std::filesystem::directory_iterator it;
  std::filesystem::directory_iterator end;
  std::filesystem::path relative; // "" == root
  std::size_t depth = 0;
  byte_count subtree_bytes = 0;
};

} // namespace

Result<ScanSummary> Scanner::scan(const ScanOptions &options,
                                  ScanVisitor &visitor) {
  std::error_code ec;

  const std::filesystem::file_status root_status =
      std::filesystem::status(options.root, ec);
  if (ec || file_status_error(root_status)) {
    std::string message = "'" + options.root.generic_string() +
                          "' does not exist or is not accessible: " +
                          (ec ? ec.message() : std::string("no such file"));
    return Result<ScanSummary>::err(make_error_message(std::move(message)));
  }

  std::filesystem::path canonical_root =
      std::filesystem::weakly_canonical(options.root, ec);
  if (ec) {
    canonical_root = std::filesystem::absolute(options.root, ec);
  }

  ScanStats stats;
  Recorder recorder(visitor, stats);

  if (std::filesystem::is_regular_file(root_status)) {
    // The requested root is a single file: report it directly.
    const byte_count size = std::filesystem::file_size(options.root, ec);
    if (ec) {
      std::string message =
          "'" + options.root.generic_string() + "': " + ec.message();
      return Result<ScanSummary>::err(make_error_message(std::move(message)));
    }
    FileEntry entry;
    entry.relative_path = options.root.filename().generic_string();
    entry.size = size;
    recorder.file(entry);
    return Result<ScanSummary>::ok(
        ScanSummary{std::move(canonical_root), std::move(stats)});
  }

  if (!std::filesystem::is_directory(root_status)) {
    std::string message = "'" + options.root.generic_string() +
                          "' is neither a regular file nor a directory";
    return Result<ScanSummary>::err(make_error_message(std::move(message)));
  }

  std::vector<std::unique_ptr<Frame>> stack;
  auto root_frame = std::make_unique<Frame>();
  root_frame->it = std::filesystem::directory_iterator(options.root, ec);
  if (ec) {
    std::string message =
        "'" + options.root.generic_string() + "': " + ec.message();
    return Result<ScanSummary>::err(make_error_message(std::move(message)));
  }
  root_frame->depth = 0;
  stats.directories = 1;
  stack.push_back(std::move(root_frame));

  while (!stack.empty()) {
    Frame &frame = *stack.back();

    if (frame.it == frame.end) {
      const byte_count subtree_bytes = frame.subtree_bytes;
      if (!frame.relative.empty()) {
        FileEntry entry;
        entry.relative_path = frame.relative;
        entry.size = subtree_bytes;
        recorder.directory(entry);
      }
      stack.pop_back();
      if (!stack.empty()) {
        stack.back()->subtree_bytes += subtree_bytes;
      }
      continue;
    }

    const std::filesystem::directory_entry entry = *frame.it;

    ec.clear();
    frame.it.increment(ec);
    if (ec) {
      // The listing advanced past this entry but got an error for a sibling.
      recorder.error(frame.relative.generic_string(), make_error(ec));
      continue;
    }

    const std::string leaf = entry.path().filename().generic_string();

    if (!options.include_hidden && is_hidden_name(leaf)) {
      continue;
    }
    if (is_excluded_name(leaf, options.exclude_names)) {
      continue;
    }

    ec.clear();
    const std::filesystem::file_status status = entry.symlink_status(ec);
    if (ec) {
      recorder.error(child_relative(frame.relative, leaf).generic_string(),
                     make_error(ec));
      continue;
    }

    if (status.type() == std::filesystem::file_type::symlink) {
      const auto size_result = symlink_counted_size(entry.path(), canonical_root);
      if (!size_result.has_value()) {
        recorder.error(child_relative(frame.relative, leaf).generic_string(),
                       size_result.error());
        continue;
      }
      const auto counted_size = size_result.value();
      if (!counted_size.has_value()) {
        // A symlink to a directory: neither descended into nor counted.
        continue;
      }
      const byte_count size = counted_size.value();
      FileEntry file_entry;
      file_entry.relative_path = child_relative(frame.relative, leaf);
      file_entry.size = size;
      frame.subtree_bytes += size;
      recorder.file(file_entry);
      continue;
    }

    if (status.type() == std::filesystem::file_type::directory) {
      stats.directories += 1;
      const std::size_t child_depth = frame.depth + 1;
      if (child_depth < options.max_depth) {
        std::error_code child_ec;
        std::filesystem::directory_iterator child_it(entry.path(), child_ec);
        if (child_ec) {
          recorder.error(child_relative(frame.relative, leaf).generic_string(),
                         make_error(child_ec));
        } else {
          auto child_frame = std::make_unique<Frame>();
          child_frame->it = std::move(child_it);
          child_frame->relative = child_relative(frame.relative, leaf);
          child_frame->depth = child_depth;
          stack.push_back(std::move(child_frame));
        }
      }
      continue;
    }

    if (status.type() == std::filesystem::file_type::regular) {
      ec.clear();
      const byte_count size = std::filesystem::file_size(entry.path(), ec);
      if (ec) {
        recorder.error(child_relative(frame.relative, leaf).generic_string(),
                       make_error(ec));
        continue;
      }
      FileEntry file_entry;
      file_entry.relative_path = child_relative(frame.relative, leaf);
      file_entry.size = size;
      frame.subtree_bytes += size;
      recorder.file(file_entry);
      continue;
    }

    // FIFOs, sockets, block/character devices and unknown types are skipped
    // without being counted: they carry no useful size for this tool.
  }

  return Result<ScanSummary>::ok(
      ScanSummary{std::move(canonical_root), std::move(stats)});
}

} // namespace sc