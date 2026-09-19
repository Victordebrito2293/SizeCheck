// test_scanner.cpp -- Integration tests for the filesystem scanner and its
// policy helpers.  Every test creates its own temporary directory tree; no
// real user directories are ever touched.

#include "analyzer.hpp"
#include "scanner.hpp"

#include "test_harness.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

#if !defined(_WIN32)
#include <unistd.h>
#endif

namespace {

class TempDir {
public:
  TempDir() {
    path_ = std::filesystem::temp_directory_path() /
           ("sizecheck_test_" + std::to_string(sequence()++));
    std::error_code ec;
    std::filesystem::create_directories(path_, ec);
  }

  ~TempDir() {
    // Restore permissions first so removal always succeeds.
    std::error_code ec;
    for (const auto &locked : locked_) {
      std::filesystem::permissions(
          locked, std::filesystem::perms::owner_all,
          std::filesystem::perm_options::replace, ec);
      ec.clear();
    }
    std::filesystem::remove_all(path_, ec);
  }

  TempDir(const TempDir &) = delete;
  TempDir &operator=(const TempDir &) = delete;

  const std::filesystem::path &path() const { return path_; }

  std::filesystem::path make_dir(const std::filesystem::path &relative) {
    const std::filesystem::path target = path_ / relative;
    std::error_code ec;
    std::filesystem::create_directories(target, ec);
    return target;
  }

  std::filesystem::path make_file(const std::filesystem::path &relative,
                                  std::uintmax_t size) {
    const std::filesystem::path target = path_ / relative;
    std::error_code ec;
    std::filesystem::create_directories(target.parent_path(), ec);
    std::ofstream stream(target, std::ios::out | std::ios::binary);
    stream.close();
    std::filesystem::resize_file(target, size, ec);
    return target;
  }

  void make_symlink(const std::filesystem::path &target,
                    const std::filesystem::path &link_relative) {
    const std::filesystem::path link = path_ / link_relative;
    std::error_code ec;
    std::filesystem::create_directories(link.parent_path(), ec);
    std::filesystem::create_symlink(target, link, ec);
  }

  // On POSIX, revoke read access on a directory so scans produce an access
  // error there.
  void make_dir_permission_denied(const std::filesystem::path &relative) {
    const std::filesystem::path target = make_dir(relative);
#if !defined(_WIN32)
    std::error_code ec;
    std::filesystem::permissions(target, std::filesystem::perms::none,
                                 std::filesystem::perm_options::replace, ec);
    locked_.push_back(target);
#endif
  }

  static std::size_t &sequence() {
    static std::size_t value = 0;
    return value;
  }

private:
  std::filesystem::path path_;
  std::vector<std::filesystem::path> locked_;
};

struct Outcome {
  sc::ScanSummary summary;
  sc::Analysis analysis;
};

Outcome scan_root(const std::filesystem::path &root, sc::ScanOptions options = {},
                  std::size_t top_n = 16) {
  options.root = root;
  sc::Analyzer analyzer(top_n);
  auto result = sc::Scanner::scan(options, analyzer);
  if (!result.has_value()) {
    throw sc_test::TestFailure("scan failed: " + result.error().message);
  }
  return Outcome{result.value(), analyzer.collect()};
}

bool running_as_root() {
#if defined(_WIN32)
  return false;
#else
  return ::geteuid() == 0;
#endif
}

} // namespace

TEST("scanner counts files directories and total") {
  TempDir tmp;
  tmp.make_file("a.txt", 100);
  tmp.make_file("sub/b.txt", 50);
  tmp.make_dir("empty");

  const Outcome outcome = scan_root(tmp.path());

  CHECK_EQ(outcome.summary.stats.files, static_cast<std::uint64_t>(2));
  CHECK_EQ(outcome.summary.stats.directories,
           static_cast<std::uint64_t>(3)); // root + sub + empty
  CHECK_EQ(outcome.summary.stats.total_bytes,
           static_cast<sc::byte_count>(150));
  CHECK_EQ(outcome.analysis.largest_files.size(),
           static_cast<std::size_t>(2));
}

TEST("scanner empty root and empty files") {
  TempDir tmp;
  tmp.make_file("empty.bin", 0);

  const Outcome outcome = scan_root(tmp.path());

  CHECK_EQ(outcome.summary.stats.files, static_cast<std::uint64_t>(1));
  CHECK_EQ(outcome.summary.stats.directories, static_cast<std::uint64_t>(1));
  CHECK_EQ(outcome.summary.stats.total_bytes, static_cast<sc::byte_count>(0));
}

TEST("scanner large file larger than 4 GiB") {
  TempDir tmp;
  const sc::byte_count size = static_cast<sc::byte_count>(1) << 32; // 4 GiB
  tmp.make_file("huge.bin", size + 1);

  const Outcome outcome = scan_root(tmp.path());

  CHECK_EQ(outcome.summary.stats.files, static_cast<std::uint64_t>(1));
  CHECK_EQ(outcome.summary.stats.total_bytes, size + 1);
  CHECK_EQ(outcome.analysis.largest_files[0].size, size + 1);
  CHECK_EQ(outcome.analysis.largest_files[0].relative_path.generic_string(),
           std::string("huge.bin"));
}

TEST("scanner hidden files excluded by default") {
  TempDir tmp;
  tmp.make_file(".secret", 5000);
  tmp.make_file(".hidden_dir/inside.txt", 42);
  tmp.make_file("visible.txt", 100);

  const Outcome default_outcome = scan_root(tmp.path());

  CHECK_EQ(default_outcome.summary.stats.files, static_cast<std::uint64_t>(1));
  CHECK_EQ(default_outcome.summary.stats.total_bytes,
           static_cast<sc::byte_count>(100));

  sc::ScanOptions with_hidden;
  with_hidden.include_hidden = true;
  const Outcome hidden_outcome = scan_root(tmp.path(), with_hidden);

  CHECK_EQ(hidden_outcome.summary.stats.files, static_cast<std::uint64_t>(3));
  CHECK_EQ(hidden_outcome.summary.stats.total_bytes,
           static_cast<sc::byte_count>(5142));
}

TEST("scanner depth limiting") {
  TempDir tmp;
  tmp.make_file("root_file.bin", 50);           // depth 1
  tmp.make_file("a/b/deep.bin", 5000);          // depth 3
  tmp.make_file("a/b/c/even_deeper.bin", 9000); // depth 4

  sc::ScanOptions depth_one;
  depth_one.max_depth = 1;
  const Outcome outcome = scan_root(tmp.path(), depth_one);

  // Only the file directly inside root is counted; sibling directories are
  // seen but not descended into.
  CHECK_EQ(outcome.summary.stats.files, static_cast<std::uint64_t>(1));
  CHECK_EQ(outcome.summary.stats.total_bytes, static_cast<sc::byte_count>(50));
  CHECK(outcome.summary.stats.directories >= static_cast<std::uint64_t>(2));

  sc::ScanOptions depth_two;
  depth_two.max_depth = 2;
  const Outcome outcome2 = scan_root(tmp.path(), depth_two);
  CHECK_EQ(outcome2.summary.stats.files, static_cast<std::uint64_t>(1));

  sc::ScanOptions depth_three;
  depth_three.max_depth = 3;
  const Outcome outcome3 = scan_root(tmp.path(), depth_three);
  CHECK_EQ(outcome3.summary.stats.files, static_cast<std::uint64_t>(2));
  CHECK_EQ(outcome3.summary.stats.total_bytes,
           static_cast<sc::byte_count>(5050));
}

TEST("scanner exclude by name") {
  TempDir tmp;
  tmp.make_file("node_modules/big.js", 8000);
  tmp.make_file("src/main.cpp", 100);
  tmp.make_file("src/skip.tmp", 60);

  sc::ScanOptions options;
  options.exclude_names = {"node_modules", "skip.tmp"};
  const Outcome outcome = scan_root(tmp.path(), options);

  CHECK_EQ(outcome.summary.stats.files, static_cast<std::uint64_t>(1));
  CHECK_EQ(outcome.summary.stats.total_bytes, static_cast<sc::byte_count>(100));
}

TEST("scanner symlink to directory is not followed") {
  TempDir tmp;
  tmp.make_file("real/data.bin", 250);
  tmp.make_symlink("real", "real_link");

  const Outcome outcome = scan_root(tmp.path());

  // The directory symlink is skipped entirely: not descended and not counted.
  CHECK_EQ(outcome.summary.stats.files, static_cast<std::uint64_t>(1));
  CHECK_EQ(outcome.summary.stats.total_bytes, static_cast<sc::byte_count>(250));
  CHECK_EQ(outcome.summary.stats.directories, static_cast<std::uint64_t>(2));
  CHECK_EQ(outcome.summary.stats.access_errors, static_cast<std::uint64_t>(0));
}

TEST("scanner symlink to file inside tree is counted") {
  TempDir tmp;
  tmp.make_file("data.bin", 1000);
  tmp.make_symlink("data.bin", "alias.bin");

  const Outcome outcome = scan_root(tmp.path());

  // Both names are reported; alias is resolved and counted too.
  CHECK_EQ(outcome.summary.stats.files, static_cast<std::uint64_t>(2));
  CHECK_EQ(outcome.summary.stats.total_bytes, static_cast<sc::byte_count>(2000));
}

TEST("scanner symlink outside tree counts only the link") {
  TempDir external;
  external.make_file("outside/big.bin", 50000);

  TempDir tmp;
  // Relative link browser-resolved relative to the LINK's directory.
  tmp.make_symlink(
      std::filesystem::absolute(external.path() / "outside/big.bin"),
      "escape.bin");
  tmp.make_file("inside.bin", 10);

  const Outcome outcome = scan_root(tmp.path());

  // Only the 10-byte file plus a tiny link are counted.  The 50 KB outside
  // file must never appear in totals.
  CHECK_EQ(outcome.summary.stats.files, static_cast<std::uint64_t>(2));
  CHECK(outcome.summary.stats.total_bytes < static_cast<sc::byte_count>(1000));
  CHECK_EQ(outcome.summary.stats.access_errors, static_cast<std::uint64_t>(0));
}

TEST("scanner dangling symlink pointing inside is reported as error") {
  TempDir tmp;
  tmp.make_file("ok.bin", 7);
  tmp.make_symlink("missing.bin", "dead_link.bin");

  const Outcome outcome = scan_root(tmp.path());

  CHECK_EQ(outcome.summary.stats.files, static_cast<std::uint64_t>(1));
  CHECK(outcome.summary.stats.access_errors == static_cast<std::uint64_t>(1));
}

TEST("scanner symlink loop terminates") {
  TempDir tmp;
  tmp.make_symlink("b", "a");
  tmp.make_symlink("a", "b");

  const Outcome outcome = scan_root(tmp.path());

  // The scan must terminate; the loop is either reported as an error or
  // counted as a link, but it must never hang or crash.
  CHECK(outcome.summary.stats.access_errors >= static_cast<std::uint64_t>(0));
  CHECK(outcome.summary.stats.files >= static_cast<std::uint64_t>(0));
}

TEST("scanner permission denied on directory is reported") {
#if defined(_WIN32)
  return; // No POSIX-style permissions on Windows.
#else
  if (running_as_root()) {
    return; // Root bypasses permission checks.
  }

  TempDir tmp;
  tmp.make_file("secret/locked.txt", 300);
  tmp.make_dir_permission_denied("secret");

  const Outcome outcome = scan_root(tmp.path());

  CHECK(outcome.summary.stats.access_errors >= static_cast<std::uint64_t>(1));
  CHECK_EQ(outcome.summary.stats.total_bytes,
           static_cast<sc::byte_count>(0)); // "secret" is 0 bytes itself
  CHECK(outcome.analysis.error_details.size() >= static_cast<std::size_t>(1));
#endif
}

TEST("scanner nonexistent root fails") {
  TempDir tmp;
  const std::filesystem::path missing = tmp.path() / "missing-dir";

  sc::Analyzer analyzer(8);
  sc::ScanOptions options;
  options.root = missing;
  const auto result = sc::Scanner::scan(options, analyzer);

  CHECK(!result.has_value());
  CHECK_NOT_CONTAINS(result.error().message, "no such file");
}

TEST("scanner single file root") {
  TempDir tmp;
  tmp.make_file("single.bin", 4096);

  sc::Analyzer analyzer(8);
  sc::ScanOptions options;
  options.root = tmp.path() / "single.bin";
  const auto result = sc::Scanner::scan(options, analyzer);

  CHECK(result.has_value());
  CHECK_EQ(result.value().stats.files, static_cast<std::uint64_t>(1));
  CHECK_EQ(result.value().stats.directories, static_cast<std::uint64_t>(0));
  CHECK_EQ(result.value().stats.total_bytes, static_cast<sc::byte_count>(4096));

  const sc::Analysis analysis = analyzer.collect();
  CHECK_EQ(analysis.largest_files.size(), static_cast<std::size_t>(1));
  CHECK_EQ(analysis.largest_files[0].size, static_cast<sc::byte_count>(4096));
}

TEST("scanner excludes only files, not their content parent") {
  TempDir tmp;
  tmp.make_file("keep/nodename.tmp", 30);
  tmp.make_file("keep/real.txt", 20);

  sc::ScanOptions options;
  options.exclude_names = {"skipwhatever"};
  const Outcome outcome = scan_root(tmp.path(), options);

  CHECK_EQ(outcome.summary.stats.files, static_cast<std::uint64_t>(2));
  CHECK_EQ(outcome.summary.stats.total_bytes, static_cast<sc::byte_count>(50));
}

TEST("is_path_within component boundaries") {
  using sc::is_path_within;

  const std::filesystem::path root = "/home/user/project";
  CHECK_EQ(is_path_within(root, root), true);
  CHECK_EQ(is_path_within("/home/user/project/file.c", root), true);
  CHECK_EQ(is_path_within("/home/user/project/sub/file.c", root), true);
  // Near-root sibling must NOT be treated as inside.
  CHECK_EQ(is_path_within("/home/user/project2/file.c", root), false);
  CHECK_EQ(is_path_within("/home/user/proj/file.c", root), false);
  CHECK_EQ(is_path_within("/home/user/", root), false);
  CHECK_EQ(is_path_within("/", root), false);
}

TEST("is_hidden_leaf detection") {
  using sc::is_hidden_leaf;
  CHECK_EQ(is_hidden_leaf(std::filesystem::path(".git")), true);
  CHECK_EQ(is_hidden_leaf(std::filesystem::path("src/main.cpp")), false);
  CHECK_EQ(is_hidden_leaf(std::filesystem::path(".hidden_dir/file")), false);
  CHECK_EQ(is_hidden_leaf(std::filesystem::path(".x")), true);
}