#pragma once

#include <jni.h>
#include <string>
#include <string_view>

namespace expo::kolibri {
  /**
   * UTF-16 unit count up to which string conversions use a stack buffer instead of the heap.
   */
  inline constexpr size_t kStackUnits = 512;

  /**
   * A `jstring` (real UTF-8, via UTF-16) as a `std::string`. A null `str` yields an empty string.
   */
  std::string toStdString(JNIEnv* env, jstring str);

  /**
   * A `jstring` as a `std::u16string`. A null `str` yields an empty string.
   */
  std::u16string toU16StdString(JNIEnv* env, jstring str);

  /**
   * A UTF-8 `std::string` as a fresh `jstring` local reference (real UTF-8, via UTF-16).
   * An empty string yields an empty Java string, never `nullptr`.
   */
  jstring toJString(JNIEnv* env, const std::string& str);

  /**
   * UTF-16 code units as a fresh `jstring` local reference — a direct copy, no conversion.
   * An empty view yields an empty Java string, never `nullptr`.
   */
  jstring toJString(JNIEnv* env, std::u16string_view str);

  /**
   * UTF-16 code units as UTF-8.
   */
  std::string utf16ToUtf8(const char16_t* units, size_t length);

  /**
   * Converts UTF-8 `str` into UTF-16 code units written to `out.
   */
  size_t utf8ToUtf16Into(std::string_view str, char16_t* out);
} // namespace expo::kolibri
