#include <exception>
#include <iostream>

#include "TestSupport.h"

int main() {
  int failures = 0;
  for (const auto& test : sinesoda::test::registry()) {
    try {
      test.function();
      std::cout << "[PASS] " << test.name << '\n';
    } catch (const std::exception& error) {
      ++failures;
      std::cerr << "[FAIL] " << test.name << ": " << error.what() << '\n';
    } catch (...) {
      ++failures;
      std::cerr << "[FAIL] " << test.name << ": unknown exception\n";
    }
  }
  std::cout << sinesoda::test::registry().size() - static_cast<std::size_t>(failures) << '/'
            << sinesoda::test::registry().size() << " tests passed\n";
  return failures == 0 ? 0 : 1;
}
