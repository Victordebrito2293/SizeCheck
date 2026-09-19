// test_harness.hpp -- Minimal, dependency-free test framework for SizeCheck.
//
// Tests register themselves with TEST(name) { ... } and use CHECK / CHECK_EQ /
// REQUIRE / CHECK_CONTAINS.  Failures are counted and summarized; a non-zero
// exit code is returned when any test fails.

#ifndef SIZECHECK_TEST_HARNESS_HPP
#define SIZECHECK_TEST_HARNESS_HPP

#include <functional>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace sc_test {

struct TestCase {
  const char *name;
  const char *file;
  int line;
  std::function<void()> fn;
};

inline std::vector<TestCase> &registry() {
  static std::vector<TestCase> tests;
  return tests;
}

struct Registrar {
  Registrar(const char *name, const char *file, int line,
            std::function<void()> fn) {
    registry().push_back(TestCase{name, file, line, std::move(fn)});
  }
};

class TestFailure {
public:
  explicit TestFailure(std::string message) : message_(std::move(message)) {}
  const std::string &message() const { return message_; }

private:
  std::string message_;
};

inline std::string to_string_impl(const std::string &value) { return value; }
inline std::string to_string_impl(std::string_view value) {
  return std::string(value);
}
inline std::string to_string_impl(const char *value) {
  return value == nullptr ? std::string("(null)") : std::string(value);
}
inline std::string to_string_impl(bool value) {
  return value ? std::string("true") : std::string("false");
}

template <typename T> std::string to_string_impl(const T &value) {
  std::ostringstream out;
  out << value;
  return out.str();
}

inline std::string to_string(const auto &value) {
  return to_string_impl(value);
}

inline bool check_impl(bool condition, const char *expr, const char *file,
                       int line, const std::string &extra = {}) {
  if (!condition) {
    std::ostringstream message;
    message << file << ":" << line << ": CHECK failed: " << expr;
    if (!extra.empty()) {
      message << " [" << extra << "]";
    }
    throw TestFailure(message.str());
  }
  return true;
}

#define CHECK(expr) ::sc_test::check_impl(!!(expr), #expr, __FILE__, __LINE__)

#define CHECK_EQ(lhs, rhs)                                                     \
  ::sc_test::check_impl(                                                        \
      (lhs) == (rhs), #lhs " == " #rhs, __FILE__, __LINE__,                    \
      "lhs=" + ::sc_test::to_string(lhs) + ", rhs=" + ::sc_test::to_string(rhs))

#define CHECK_NE(lhs, rhs)                                                     \
  ::sc_test::check_impl(                                                        \
      (lhs) != (rhs), #lhs " != " #rhs, __FILE__, __LINE__,                    \
      "lhs=" + ::sc_test::to_string(lhs) + ", rhs=" + ::sc_test::to_string(rhs))

#define CHECK_CONTAINS(haystack, needle)                                       \
  ::sc_test::check_impl(                                                       \
      ::sc_test::contains_impl((haystack), (needle)),                          \
      "CONTAINS(" #haystack ", " #needle ")", __FILE__, __LINE__,              \
      "haystack='" + ::sc_test::to_string(haystack) +                          \
          "', needle='" + ::sc_test::to_string(needle) + "'")

#define CHECK_NOT_CONTAINS(haystack, needle)                                   \
  ::sc_test::check_impl(                                                       \
      !::sc_test::contains_impl((haystack), (needle)),                         \
      "NOT_CONTAINS(" #haystack ", " #needle ")", __FILE__, __LINE__,          \
      "haystack='" + ::sc_test::to_string(haystack) +                          \
          "', needle='" + ::sc_test::to_string(needle) + "'")

inline bool contains_impl(const std::string &haystack,
                          const std::string &needle) {
  return haystack.find(needle) != std::string::npos;
}

// Two-level paste so an operand like `__LINE__` is expanded before being glued.
// Writing `sc_test_fn##__LINE__` directly makes some preprocessors paste the
// literal token `__LINE__` (or the whole counter) instead of its value.
#define SC_TEST_CAT_IMPL(a, b) a##b
#define SC_TEST_CAT(a, b) SC_TEST_CAT_IMPL(a, b)

#define TEST(name)                                                             \
  static void SC_TEST_CAT(sc_test_fn_, __LINE__)();                            \
  static ::sc_test::Registrar SC_TEST_CAT(sc_test_reg_, __LINE__)(             \
      name, __FILE__, __LINE__, &SC_TEST_CAT(sc_test_fn_, __LINE__));          \
  static void SC_TEST_CAT(sc_test_fn_, __LINE__)()

} // namespace sc_test

#endif // SIZECHECK_TEST_HARNESS_HPP