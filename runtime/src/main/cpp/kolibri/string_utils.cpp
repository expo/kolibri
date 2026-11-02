#include <kolibri/string_utils.h>

#include <kolibri/defines.h>

#include <cstdint>
#include <cstring>
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

    // Whether every byte is in 0x01..0x7F. Word-wise, stopping at the first word that is not.
    bool isAsciiWithoutNul(std::string_view str) {
      constexpr uint64_t kLowBits = 0x0101010101010101ULL;
      constexpr uint64_t kHighBits = 0x8080808080808080ULL;
      const auto* bytes = reinterpret_cast<const unsigned char*>(str.data());
      const size_t size = str.size();
      size_t i = 0;
      for (; i + 8 <= size; i += 8) {
        uint64_t word;
        std::memcpy(&word, bytes + i, sizeof(word));
        // High bit set in any byte, or (with none set) any byte zero.
        if (((word | ((word - kLowBits) & ~word)) & kHighBits) != 0) {
          return false;
        }
      }
      for (; i < size; i++) {
        if (bytes[i] == 0 || bytes[i] >= 0x80) {
          return false;
        }
      }
      return true;
    }

    bool isHighSurrogate(uint32_t unit) { return (unit & 0xFC00) == 0xD800; }

    bool isLowSurrogate(uint32_t unit) { return (unit & 0xFC00) == 0xDC00; }

    // Units before the first 64-unit block holding a non-ASCII unit; all of them for an ASCII
    // string. No early exit within a block, so each block's check vectorizes.
    size_t asciiPrefixLength(const char16_t* units, size_t length) {
      constexpr size_t kBlock = 64;
      size_t i = 0;
      for (; i + kBlock <= length; i += kBlock) {
        uint32_t bits = 0;
        for (size_t k = 0; k < kBlock; k++) {
          bits |= units[i + k];
        }
        if (bits >= 0x80) {
          return i;
        }
      }
      uint32_t bits = 0;
      for (size_t k = i; k < length; k++) {
        bits |= units[k];
      }
      return bits >= 0x80 ? i : length;
    }

    // UTF-8 bytes `units` encode to. Each unit alone takes 1-3 bytes (a lone surrogate takes 3,
    // as WTF-8); only a valid surrogate pair is cheaper than its halves, 4 bytes instead of 6.
    // Without surrogates the loop has no cross-unit dependency, so it vectorizes.
    size_t utf8Length(const char16_t* units, size_t length) {
      size_t bytes = length;
      uint32_t surrogates = 0;
      for (size_t i = 0; i < length; i++) {
        const uint32_t unit = units[i];
        bytes += (unit >= 0x80) + (unit >= 0x800);
        surrogates |= (unit & 0xF800) == 0xD800;
      }
      if (surrogates != 0) [[unlikely]] {
        for (size_t i = 0; i + 1 < length; i++) {
          if (isHighSurrogate(units[i]) && isLowSurrogate(units[i + 1])) {
            bytes -= 2;
            i++;
          }
        }
      }
      return bytes;
    }

    // Encodes `units` into `out`, which must have room for utf8Length(units, length) bytes, and
    // returns the end of what it wrote. Kept out of line: inlined next to the ASCII paths, it
    // compiles to a slower loop.
    NOINLINE char* encodeUtf8(const char16_t* units, size_t length, char* out) {
      // Checked most common first: one byte, two, three, and only then the surrogates.
      for (size_t i = 0; i < length;) {
        uint32_t cp = units[i++];
        if (cp <= 0x7F) {
          *out++ = static_cast<char>(cp);
        } else if (cp <= 0x7FF) {
          *out++ = static_cast<char>(0xC0 | (cp >> 6));
          *out++ = static_cast<char>(0x80 | (cp & 0x3F));
        } else if ((cp & 0xF800) != 0xD800 ||
                   !(isHighSurrogate(cp) && i < length && isLowSurrogate(units[i]))) {
          // Also a lone surrogate, as WTF-8.
          *out++ = static_cast<char>(0xE0 | (cp >> 12));
          *out++ = static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
          *out++ = static_cast<char>(0x80 | (cp & 0x3F));
        } else {
          cp = 0x10000 + ((cp - 0xD800) << 10) + (units[i++] - 0xDC00);
          *out++ = static_cast<char>(0xF0 | (cp >> 18));
          *out++ = static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
          *out++ = static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
          *out++ = static_cast<char>(0x80 | (cp & 0x3F));
        }
      }
      return out;
    }

    // Up to this many units, a non-ASCII string is encoded straight into a stack buffer and
    // copied out, instead of being measured first: for short strings the extra pass costs more
    // than the copy.
    constexpr size_t kShortUnits = 64;
  } // namespace

  std::string utf16ToUtf8(const char16_t* units, size_t length) {
    // The ASCII prefix narrows unit for unit, so only the rest needs measuring and encoding;
    // for an all-ASCII string that is nothing. Sized exactly up front, so the conversion writes
    // through a pointer instead of growing the string a byte at a time.
    const size_t ascii = asciiPrefixLength(units, length);
    if (ascii != length && length <= kShortUnits) {
      // Each unit takes at most 3 bytes (a pair takes 4 for 2 units).
      char buf[3 * kShortUnits];
      char* end = encodeUtf8(units, length, buf);
      return std::string(buf, end);
    }
    const size_t rest = length - ascii;
    std::string out(ascii + (rest == 0 ? 0 : utf8Length(units + ascii, rest)), '\0');
    char* p = out.data();

    for (size_t i = 0; i < ascii; i++) {
      p[i] = static_cast<char>(units[i]);
    }
    if (rest != 0) {
      encodeUtf8(units + ascii, rest, p + ascii);
    }
    return out;
  }

  size_t utf8ToUtf16Into(std::string_view str, char16_t* out) {
    size_t n = 0;
    const auto* bytes = reinterpret_cast<const unsigned char*>(str.data());
    const size_t size = str.size();
    // A leading ASCII run goes 8 bytes at a time; past it, ASCII is decoded a byte at a time, so
    // mixed text pays nothing for the word-wise path.
    size_t i = 0;
    for (; i + 8 <= size; i += 8) {
      uint64_t word;
      std::memcpy(&word, bytes + i, sizeof(word));
      if ((word & 0x8080808080808080ULL) != 0) {
        break;
      }
      for (size_t k = 0; k < 8; k++) {
        out[n + k] = bytes[i + k];
      }
      n += 8;
    }
    while (i < size) {
      const unsigned char b = bytes[i];
      if (b < 0x80) {
        out[n++] = b;
        i++;
        continue;
      }

      // A sequence cut short by the end of the input decodes to U+FFFD and ends the string. A
      // malformed continuation decodes to U+FFFD and resyncs right after the lead byte, so the
      // offending byte is reconsidered as a new lead.
      if ((b >> 5) == 0x6) {
        if (i + 1 >= size) {
          out[n++] = 0xFFFD;
          break;
        }
        const unsigned char c1 = bytes[i + 1];
        if ((c1 & 0xC0) != 0x80) {
          out[n++] = 0xFFFD;
          i++;
          continue;
        }
        out[n++] = static_cast<char16_t>(((b & 0x1F) << 6) | (c1 & 0x3F));
        i += 2;
        continue;
      }
      if ((b >> 4) == 0xE) {
        if (i + 2 >= size) {
          out[n++] = 0xFFFD;
          break;
        }
        const unsigned char c1 = bytes[i + 1];
        const unsigned char c2 = bytes[i + 2];
        // A non-short-circuiting `|`, so the checks need no branch each; on ints, since Clang
        // warns about `|` between bools.
        if (static_cast<int>((c1 & 0xC0) != 0x80) | static_cast<int>((c2 & 0xC0) != 0x80)) {
          out[n++] = 0xFFFD;
          i++;
          continue;
        }
        out[n++] = static_cast<char16_t>(((b & 0x0F) << 12) | ((c1 & 0x3F) << 6) | (c2 & 0x3F));
        i += 3;
        continue;
      }
      if ((b >> 3) != 0x1E) {
        // Invalid lead byte; emit the replacement character and resync.
        out[n++] = 0xFFFD;
        i++;
        continue;
      }

      if (i + 3 >= size) {
        out[n++] = 0xFFFD;
        break;
      }
      const unsigned char c1 = bytes[i + 1];
      const unsigned char c2 = bytes[i + 2];
      const unsigned char c3 = bytes[i + 3];
      if (static_cast<int>((c1 & 0xC0) != 0x80) | static_cast<int>((c2 & 0xC0) != 0x80) |
          static_cast<int>((c3 & 0xC0) != 0x80)) {
        out[n++] = 0xFFFD;
        i++;
        continue;
      }
      uint32_t cp = ((b & 0x07) << 18) | ((c1 & 0x3F) << 12) | ((c2 & 0x3F) << 6) | (c3 & 0x3F);
      i += 4;
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

  // Both read the units with GetStringRegion, a plain copy into a buffer we own. GetStringChars
  // copies as well (a compact Latin-1 string has no UTF-16 units to lend), but into a buffer the
  // VM allocates and needs a third call to free.
  std::string toStdString(JNIEnv* env, jstring str) {
    if (str == nullptr) {
      return {};
    }
    const jsize length = env->GetStringLength(str);
    if (static_cast<size_t>(length) <= kStackUnits) {
      jchar buf[kStackUnits];
      env->GetStringRegion(str, 0, length, buf);
      return utf16ToUtf8(reinterpret_cast<const char16_t*>(buf), static_cast<size_t>(length));
    }
    std::vector<jchar> buf(static_cast<size_t>(length));
    env->GetStringRegion(str, 0, length, buf.data());
    return utf16ToUtf8(reinterpret_cast<const char16_t*>(buf.data()), buf.size());
  }

  std::u16string toU16StdString(JNIEnv* env, jstring str) {
    if (str == nullptr) {
      return {};
    }
    const jsize length = env->GetStringLength(str);
    std::u16string result(static_cast<size_t>(length), u'\0');
    env->GetStringRegion(str, 0, length, reinterpret_cast<jchar*>(result.data()));
    return result;
  }

  jstring toJString(JNIEnv* env, const std::string& str) {
    if (str.empty()) {
      return emptyJString(env);
    }
    // ASCII without NUL reads the same as modified UTF-8, so the VM can take the bytes as they
    // are: it builds a Latin-1 string straight from them, with no UTF-16 detour.
    if (isAsciiWithoutNul(str)) {
      return env->NewStringUTF(str.c_str());
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
