#pragma once

#include <cmath>
#include <functional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace sinesoda::test {

struct TestCase {
  std::string name;
  std::function<void()> function;
};

inline std::vector<TestCase>& registry() {
  static std::vector<TestCase> tests;
  return tests;
}

class Registrar {
 public:
  Registrar(std::string name, std::function<void()> function) {
    registry().push_back({std::move(name), std::move(function)});
  }
};

inline void fail(const char* expression, const char* file, int line,
                 const std::string& detail = {}) {
  std::ostringstream message;
  message << file << ':' << line << ": expectation failed: " << expression;
  if (!detail.empty()) {
    message << " (" << detail << ')';
  }
  throw std::runtime_error(message.str());
}

template <typename Left, typename Right>
void expectNear(Left left, Right right, double tolerance, const char* expression, const char* file,
                int line) {
  const double difference = std::abs(static_cast<double>(left) - static_cast<double>(right));
  if (!std::isfinite(difference) || difference > tolerance) {
    std::ostringstream detail;
    detail << left << " vs " << right << ", tolerance " << tolerance;
    fail(expression, file, line, detail.str());
  }
}

}  // namespace sinesoda::test

#define SINESODA_TEST(name)                                          \
  static void name();                                                \
  static ::sinesoda::test::Registrar name##_registrar(#name, &name); \
  static void name()

#define SINESODA_EXPECT_TRUE(expression)                       \
  do {                                                         \
    if (!(expression)) {                                       \
      ::sinesoda::test::fail(#expression, __FILE__, __LINE__); \
    }                                                          \
  } while (false)

#define SINESODA_EXPECT_EQ(left, right)                                \
  do {                                                                 \
    const auto left_value = (left);                                    \
    const auto right_value = (right);                                  \
    if (!(left_value == right_value)) {                                \
      ::sinesoda::test::fail(#left " == " #right, __FILE__, __LINE__); \
    }                                                                  \
  } while (false)

#define SINESODA_EXPECT_NEAR(left, right, tolerance)                                        \
  ::sinesoda::test::expectNear((left), (right), (tolerance), #left " ~= " #right, __FILE__, \
                               __LINE__)

#define SINESODA_EXPECT_THROW(expression, exception_type)                                 \
  do {                                                                                    \
    bool caught_expected_exception = false;                                               \
    try {                                                                                 \
      static_cast<void>(expression);                                                      \
    } catch (const exception_type&) {                                                     \
      caught_expected_exception = true;                                                   \
    }                                                                                     \
    if (!caught_expected_exception) {                                                     \
      ::sinesoda::test::fail(#expression " throws " #exception_type, __FILE__, __LINE__); \
    }                                                                                     \
  } while (false)
