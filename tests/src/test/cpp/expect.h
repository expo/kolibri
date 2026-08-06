#pragma once

#include <sstream>
#include <stdexcept>
#include <string>

#include <kolibri/string_utils.h>

// Test-side assertions, kolibri's answer to fbjni's test/jni/expect.h. A failed expectation
// throws std::runtime_error; every non-noexcept test native runs under the trampoline guard
// (native_method_invoker.h), which converts the throw into a Java RuntimeException — so the JUnit
// failure message carries the C++ file:line of the failing expectation.
namespace expo::kolibri::tests {
  [[noreturn]] inline void failExpectation(const char* file, int line, const std::string& what) {
    throw std::runtime_error(std::string(file) + ":" + std::to_string(line) + ": " + what);
  }

  template<typename T>
  std::string debugString(const T& value) {
    std::ostringstream out;
    out << value;
    return out.str();
  }

  inline std::string debugString(const std::u16string& value) {
    return utf16ToUtf8(value.data(), value.size());
  }
}

#define KOLIBRI_EXPECT(cond)                                                                      \
  do {                                                                                            \
    if (!(cond)) {                                                                                \
      ::expo::kolibri::tests::failExpectation(__FILE__, __LINE__, "expectation failed: " #cond);  \
    }                                                                                             \
  } while (false)

#define KOLIBRI_EXPECT_EQ(actual, expected)                                                       \
  do {                                                                                            \
    const auto& kolibriActual = (actual);                                                         \
    const auto& kolibriExpected = (expected);                                                     \
    if (!(kolibriActual == kolibriExpected)) {                                                    \
      ::expo::kolibri::tests::failExpectation(                                                    \
        __FILE__,                                                                                 \
        __LINE__,                                                                                 \
        "expected " #actual " == " #expected ": got "                                             \
          + ::expo::kolibri::tests::debugString(kolibriActual) + ", expected "                    \
          + ::expo::kolibri::tests::debugString(kolibriExpected)                                  \
      );                                                                                          \
    }                                                                                             \
  } while (false)
