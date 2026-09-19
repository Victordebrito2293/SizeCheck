// test_analyzer.cpp -- Unit tests for the top-N aggregation logic.

#include "analyzer.hpp"

#include "test_harness.hpp"

#include <cstddef>
#include <string>

namespace {

sc::FileEntry entry(const char *path, sc::byte_count size) {
  sc::FileEntry e;
  e.relative_path = std::filesystem::path(path);
  e.size = size;
  return e;
}

} // namespace

TEST("analyzer keeps the top files sorted descending") {
  sc::Analyzer analyzer(3);
  analyzer.on_file(entry("a", 100));
  analyzer.on_file(entry("b", 300));
  analyzer.on_file(entry("c", 200));
  analyzer.on_file(entry("d", 50));
  analyzer.on_file(entry("e", 250));

  const sc::Analysis analysis = analyzer.collect();

  CHECK_EQ(analysis.largest_files.size(), static_cast<std::size_t>(3));
  CHECK_EQ(analysis.largest_files[0].size, static_cast<sc::byte_count>(300));
  CHECK_EQ(analysis.largest_files[1].size, static_cast<sc::byte_count>(250));
  CHECK_EQ(analysis.largest_files[2].size, static_cast<sc::byte_count>(200));
}

TEST("analyzer fewer entries than the limit") {
  sc::Analyzer analyzer(10);
  analyzer.on_file(entry("only", 42));

  const sc::Analysis analysis = analyzer.collect();

  CHECK_EQ(analysis.largest_files.size(), static_cast<std::size_t>(1));
  CHECK_EQ(analysis.largest_files[0].size, static_cast<sc::byte_count>(42));
}

TEST("analyzer equal sizes keep stable ordering by size") {
  sc::Analyzer analyzer(4);
  for (sc::byte_count i = 1; i <= 10; ++i) {
    analyzer.on_file(entry("same.bin", 100));
  }
  const sc::Analysis analysis = analyzer.collect();
  CHECK_EQ(analysis.largest_files.size(), static_cast<std::size_t>(4));
  for (const auto &file : analysis.largest_files) {
    CHECK_EQ(file.size, static_cast<sc::byte_count>(100));
  }
}

TEST("analyzer ignores empty directories") {
  sc::Analyzer analyzer(4);
  analyzer.on_file(entry("f", 1));
  analyzer.on_directory(entry("empty", 0));
  analyzer.on_directory(entry("with_content", 500));

  const sc::Analysis analysis = analyzer.collect();

  CHECK_EQ(analysis.largest_directories.size(), static_cast<std::size_t>(1));
  CHECK_EQ(analysis.largest_directories[0].relative_path.generic_string(),
           std::string("with_content"));
}

TEST("analyzer caps stored error details") {
  sc::Analyzer analyzer(4);
  for (int i = 0; i < 100; ++i) {
    analyzer.on_error("path/" + std::to_string(i),
                      sc::make_error_message("perm"));
  }
  const sc::Analysis analysis = analyzer.collect();
  CHECK(analysis.error_details.size() <= static_cast<std::size_t>(32));
  CHECK_EQ(analysis.error_details[0].reason, std::string("perm"));
}

TEST("analyzer top zero treated as one") {
  sc::Analyzer analyzer(0);
  analyzer.on_file(entry("a", 10));
  analyzer.on_file(entry("b", 20));
  const sc::Analysis analysis = analyzer.collect();
  CHECK_EQ(analysis.largest_files.size(), static_cast<std::size_t>(1));
}