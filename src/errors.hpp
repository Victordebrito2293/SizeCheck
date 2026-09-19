// errors.hpp -- Minimal, exception-free error handling for SizeCheck.
//
// SizeCheck treats error codes as first-class values.  All functions that can
// fail in ordinary operation (filesystem access, argument parsing) return a
// `sc::Result<T>` instead of throwing.  This keeps behavior deterministic and
// portable across platforms and compilers, and it removes the temptation to
// use exceptions for control flow.
//
// A small result type is provided here so the project does not depend on a
// specific compiler or standard library version of `std::expected` (which
// landed in C++23).

#ifndef SIZECHECK_ERRORS_HPP
#define SIZECHECK_ERRORS_HPP

#include <string>
#include <system_error>
#include <utility>

namespace sc {

// A recoverable failure.  `code` is optional; message always carries a
// human-readable explanation.
struct Error {
  std::error_code code;
  std::string message;
};

[[nodiscard]] inline Error make_error_message(std::string message) {
  return Error{{}, std::move(message)};
}

[[nodiscard]] inline Error make_error(std::error_code code,
                                      std::string message = {}) {
  Error err;
  err.code = code;
  if (message.empty()) {
    err.message = code.message();
  } else {
    err.message = std::move(message);
  }
  return err;
}

// A result that holds either a value of type T or an Error.
template <typename T> class Result {
public:
  Result(T value) : ok_(true), value_(std::move(value)) {}
  Result(Error error) : ok_(false), error_(std::move(error)) {}

  [[nodiscard]] static Result ok(T value) { return Result(std::move(value)); }
  [[nodiscard]] static Result err(Error error) {
    return Result(std::move(error));
  }
  [[nodiscard]] static Result err_message(std::string message) {
    return Result(Error{{}, std::move(message)});
  }

  [[nodiscard]] bool has_value() const noexcept { return ok_; }
  [[nodiscard]] explicit operator bool() const noexcept { return ok_; }

  [[nodiscard]] const T &value() const noexcept {
    // Only valid when has_value() is true.
    return value_;
  }
  [[nodiscard]] T &value() noexcept { return value_; }

  [[nodiscard]] T value_or(T fallback) const {
    return ok_ ? value_ : std::move(fallback);
  }

  [[nodiscard]] const Error &error() const noexcept {
    // Only valid when has_value() is false.
    return error_;
  }

private:
  bool ok_;
  T value_;
  Error error_;
};

template <> class Result<void> {
public:
  Result() noexcept : ok_(true) {}
  Result(Error error) : ok_(false), error_(std::move(error)) {}

  [[nodiscard]] static Result ok() noexcept { return Result(); }
  [[nodiscard]] static Result err(Error error) {
    return Result(std::move(error));
  }
  [[nodiscard]] static Result err_message(std::string message) {
    return Result(Error{{}, std::move(message)});
  }

  [[nodiscard]] bool has_value() const noexcept { return ok_; }
  [[nodiscard]] explicit operator bool() const noexcept { return ok_; }
  [[nodiscard]] const Error &error() const noexcept { return error_; }

private:
  bool ok_;
  Error error_;
};

} // namespace sc

#endif // SIZECHECK_ERRORS_HPP