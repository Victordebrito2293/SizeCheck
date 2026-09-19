#include "test_harness.hpp"

#include <iostream>
#include <string>

namespace sc_test {

// Implemented in test_main so the harness header stays header-only.
inline std::string &filter_ref() {
  static std::string filter;
  return filter;
}

void set_filter(const std::string &filter) { filter_ref() = filter; }
bool test_matches(const TestCase &test) {
  if (filter_ref().empty()) {
    return true;
  }
  return std::string(test.name).find(filter_ref()) != std::string::npos;
}

} // namespace sc_test

int main(int argc, char **argv) {
  if (argc > 1) {
    sc_test::set_filter(std::string(argv[1]));
  }

  std::size_t passed = 0;
  std::size_t failed = 0;
  std::vector<std::string> failures;

  for (const sc_test::TestCase &test : sc_test::registry()) {
    if (!sc_test::test_matches(test)) {
      continue;
    }
    try {
      test.fn();
      passed += 1;
    } catch (const sc_test::TestFailure &failure) {
      failed += 1;
      failures.push_back(failure.message());
      std::cout << "[FAIL] " << test.name << "\n";
    } catch (const std::exception &ex) {
      failed += 1;
      failures.push_back(std::string("unexpected exception: ") + ex.what());
      std::cout << "[FAIL] " << test.name << " (unexpected exception)\n";
    }
  }

  for (const std::string &failure : failures) {
    std::cout << "  " << failure << "\n";
  }

  std::cout << "\n" << passed << " passed, " << failed << " failed";
  if (!sc_test::filter_ref().empty()) {
    std::cout << " (filtered by '" << sc_test::filter_ref() << "')";
  }
  std::cout << "\n";

  return failed == 0 ? 0 : 1;
}