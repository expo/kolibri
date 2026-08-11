#include <kolibri/string_utils.h>

#include <cstdint>
#include <string>
#include <vector>

namespace expo::kolibri {
  namespace {
    // --- UTF-8 <-> UTF-16 ----------------------------------------------------------------------
    // The JNI `*UTF*` calls use *modified* UTF-8 (no real 4-byte sequences, U+0000 encoded as two
    // bytes). To round-trip arbitrary JS strings faithfully we convert against UTF-16 ourselves and
    // use NewString / GetStringChars, which speak genuine UTF-16.

    // NewString requires a valid (possibly zero-length) unit pointer; never hand it null.
    jstring emptyJString(JNIEnv* env) {
      static constexpr jchar kNoUnits[1] = {0};
      return env->NewString(kNoUnits, 0);
    }

    void appendUtf8(std::string& out, uint32_t cp) {
      if (cp <= 0x7F) {
        out.push_back(static_cast<char>(cp));
      } else if (cp <= 0x7FF) {
        out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
      } else if (cp <= 0xFFFF) {
        out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
      } else {
        out.push_back(static_cast<char>(0xF0 | (cp >> 18)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
      }
    }
  } // namespace

  std::string utf16ToUtf8(const char16_t* units, size_t length) {
    std::string out;
    out.reserve(length);
    for (size_t i = 0; i < length; i++) {
      uint32_t unit = units[i];
      if (unit >= 0xD800 && unit <= 0xDBFF && i + 1 < length) {
        const uint32_t low = units[i + 1];
        if (low >= 0xDC00 && low <= 0xDFFF) {
          unit = 0x10000 + ((unit - 0xD800) << 10) + (low - 0xDC00);
          i++;
        }
      }
      appendUtf8(out, unit);
    }
    return out;
  }

  size_t utf8ToUtf16Into(std::string_view str, char16_t* out) {
    size_t n = 0;
    const auto* bytes = reinterpret_cast<const unsigned char*>(str.data());
    const size_t size = str.size();
    for (size_t i = 0; i < size;) {
      const unsigned char b = bytes[i];
      uint32_t cp;
      size_t extra;
      if (b < 0x80) {
        cp = b;
        extra = 0;
      } else if ((b >> 5) == 0x6) {
        cp = b & 0x1F;
        extra = 1;
      } else if ((b >> 4) == 0xE) {
        cp = b & 0x0F;
        extra = 2;
      } else if ((b >> 3) == 0x1E) {
        cp = b & 0x07;
        extra = 3;
      } else {
        // Invalid lead byte; emit the replacement character and resync.
        out[n++] = 0xFFFD;
        i++;
        continue;
      }
      if (i + extra >= size) {
        out[n++] = 0xFFFD;
        break;
      }
      bool valid = true;
      for (size_t k = 1; k <= extra; k++) {
        const unsigned char cont = bytes[i + k];
        if ((cont & 0xC0) != 0x80) {
          valid = false;
          break;
        }
        cp = (cp << 6) | (cont & 0x3F);
      }
      if (!valid) {
        // Malformed continuation: emit the replacement character and resync right after the lead
        // byte, so the offending byte is reconsidered as a new lead.
        out[n++] = 0xFFFD;
        i++;
        continue;
      }
      i += extra + 1;
      if (cp > 0x10FFFF) {
        // Out of range for UTF-16. Surrogate halves (0xD800..0xDFFF) pass through deliberately:
        // this is WTF-8-style decoding, so lone surrogates in JS strings round-trip faithfully.
        out[n++] = 0xFFFD;
        continue;
      }
      if (cp <= 0xFFFF) {
        out[n++] = static_cast<char16_t>(cp);
      } else {
        cp -= 0x10000;
        out[n++] = static_cast<char16_t>(0xD800 + (cp >> 10));
        out[n++] = static_cast<char16_t>(0xDC00 + (cp & 0x3FF));
      }
    }
    return n;
  }

  std::string toStdString(JNIEnv* env, jstring str) {
    if (str == nullptr) {
      return {};
    }
    const jsize length = env->GetStringLength(str);
    const jchar* units = env->GetStringChars(str, nullptr);
    if (units == nullptr) {
      // OOM: GetStringChars failed with an exception pending; the caller's next pending-exception
      // check surfaces it.
      return {};
    }
    std::string result =
        utf16ToUtf8(reinterpret_cast<const char16_t*>(units), static_cast<size_t>(length));
    env->ReleaseStringChars(str, units);
    return result;
  }

  std::u16string toU16StdString(JNIEnv* env, jstring str) {
    if (str == nullptr) {
      return {};
    }
    const jsize length = env->GetStringLength(str);
    const jchar* units = env->GetStringChars(str, nullptr);
    if (units == nullptr) {
      // OOM: GetStringChars failed with an exception pending; the caller's next pending-exception
      // check surfaces it.
      return {};
    }
    std::u16string result{reinterpret_cast<const char16_t*>(units), static_cast<size_t>(length)};
    env->ReleaseStringChars(str, units);
    return result;
  }

  jstring toJString(JNIEnv* env, const std::string& str) {
    if (str.empty()) {
      return emptyJString(env);
    }
    // Convert into a stack buffer for typical short strings (no heap); fall back to a heap buffer
    // only for very long ones. UTF-16 never has more units than the UTF-8 byte count.
    if (str.size() <= kStackUnits) {
      char16_t buf[kStackUnits];
      const size_t n = utf8ToUtf16Into(str, buf);
      return env->NewString(reinterpret_cast<const jchar*>(buf), static_cast<jsize>(n));
    }
    std::vector<char16_t> buf(str.size());
    const size_t n = utf8ToUtf16Into(str, buf.data());
    return env->NewString(reinterpret_cast<const jchar*>(buf.data()), static_cast<jsize>(n));
  }

  jstring toJString(JNIEnv* env, const std::u16string_view str) {
    if (str.empty()) {
      return emptyJString(env);
    }
    return env->NewString(reinterpret_cast<const jchar*>(str.data()), static_cast<jsize>(str.size()));
  }
} // namespace expo::kolibri
