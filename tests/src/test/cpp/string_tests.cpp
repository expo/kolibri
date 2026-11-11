#include <cstdint>
#include <iterator>
#include <random>
#include <string>
#include <string_view>

#include <kolibri/Ref.h>
#include <kolibri/native_method.h>
#include <kolibri/string_utils.h>

#include "expect.h"
#include "test_helpers.h"

namespace expo::kolibri::tests {
  namespace {
    jstring roundTripUtf8(JNIEnv* env, jstring value) {
      const std::string utf8 = toStdString(env, value);
      return toJString(env, utf8);
    }

    jstring roundTripUtf16(JNIEnv* env, jstring value) {
      const std::u16string utf16 = toU16StdString(env, value);
      return toJString(env, std::u16string_view{utf16});
    }

    std::u16string utf8ToUtf16(const std::string_view str) {
      // UTF-16 never has more units than the UTF-8 byte count.
      std::u16string out(str.size(), u'\0');
      out.resize(utf8ToUtf16Into(str, out.data()));
      return out;
    }

    jboolean utf8Conversions(JNIEnv*) {
      // Two-byte sequence: é = U+00E9.
      KOLIBRI_EXPECT_EQ(utf16ToUtf8(u"héllo", 5), std::string("h\xC3\xA9llo"));
      KOLIBRI_EXPECT_EQ(utf8ToUtf16("h\xC3\xA9llo"), std::u16string(u"héllo"));

      // Three-byte sequence: あ = U+3042.
      KOLIBRI_EXPECT_EQ(utf16ToUtf8(u"あ", 1), std::string("\xE3\x81\x82"));
      KOLIBRI_EXPECT_EQ(utf8ToUtf16("\xE3\x81\x82"), std::u16string(u"あ"));

      // Non-BMP: 😀 = U+1F600, a surrogate pair in UTF-16, a real 4-byte sequence in UTF-8
      // (modified UTF-8 would encode the pair as two 3-byte sequences instead).
      const char16_t grinning[] = {0xD83D, 0xDE00};
      KOLIBRI_EXPECT_EQ(utf16ToUtf8(grinning, 2), std::string("\xF0\x9F\x98\x80"));
      KOLIBRI_EXPECT_EQ(utf8ToUtf16("\xF0\x9F\x98\x80"), std::u16string(grinning, 2));

      // Embedded NUL is one real 0x00 byte (modified UTF-8 would use 0xC0 0x80).
      const char16_t withNul[] = {u'a', u'\0', u'b'};
      const std::string nulUtf8 = utf16ToUtf8(withNul, 3);
      KOLIBRI_EXPECT_EQ(nulUtf8, std::string("a\0b", 3));
      KOLIBRI_EXPECT_EQ(utf8ToUtf16(std::string_view("a\0b", 3)), std::u16string(withNul, 3));

      // Lone surrogate: WTF-8-style, encoded as a 3-byte sequence and decoded back faithfully.
      const char16_t loneHigh[] = {0xD800};
      const std::string wtf8 = utf16ToUtf8(loneHigh, 1);
      KOLIBRI_EXPECT_EQ(wtf8, std::string("\xED\xA0\x80"));
      KOLIBRI_EXPECT_EQ(utf8ToUtf16(wtf8), std::u16string(loneHigh, 1));

      // Malformed input decodes to U+FFFD instead of crashing or desyncing.
      KOLIBRI_EXPECT_EQ(utf8ToUtf16("\xFF"), std::u16string(u"�"));
      KOLIBRI_EXPECT_EQ(utf8ToUtf16("\xE2\x82"), std::u16string(u"�"));

      KOLIBRI_EXPECT_EQ(utf8ToUtf16(""), std::u16string());
      KOLIBRI_EXPECT_EQ(utf16ToUtf8(nullptr, 0), std::string());
      return JNI_TRUE;
    }

    // --- Reference conversions -----------------------------------------------------------------
    // One code point at a time with no fast paths, written for clarity, not speed: the library's
    // word- and block-wise paths have to agree with them on every input.

    std::string referenceUtf16ToUtf8(std::u16string_view units) {
      std::string out;
      for (size_t i = 0; i < units.size(); i++) {
        uint32_t cp = units[i];
        const bool high = cp >= 0xD800 && cp <= 0xDBFF;
        if (high && i + 1 < units.size() && units[i + 1] >= 0xDC00 && units[i + 1] <= 0xDFFF) {
          cp = 0x10000 + ((cp - 0xD800) << 10) + (units[i + 1] - 0xDC00);
          i++;
        }
        if (cp < 0x80) {
          out += static_cast<char>(cp);
        } else if (cp < 0x800) {
          out += static_cast<char>(0xC0 | (cp >> 6));
          out += static_cast<char>(0x80 | (cp & 0x3F));
        } else if (cp < 0x10000) {
          // Lone surrogates included (WTF-8).
          out += static_cast<char>(0xE0 | (cp >> 12));
          out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
          out += static_cast<char>(0x80 | (cp & 0x3F));
        } else {
          out += static_cast<char>(0xF0 | (cp >> 18));
          out += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
          out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
          out += static_cast<char>(0x80 | (cp & 0x3F));
        }
      }
      return out;
    }

    // The decoding rules: an invalid lead byte, or a lead followed by a non-continuation byte,
    // becomes U+FFFD and decoding resumes right after the lead; a sequence cut off by the end of
    // the input becomes U+FFFD and ends it; anything past U+10FFFF becomes U+FFFD.
    std::u16string referenceUtf8ToUtf16(std::string_view text) {
      std::u16string out;
      const auto* bytes = reinterpret_cast<const unsigned char*>(text.data());
      size_t i = 0;
      while (i < text.size()) {
        const unsigned char lead = bytes[i];
        size_t extra;
        uint32_t cp;
        if (lead < 0x80) {
          out += static_cast<char16_t>(lead);
          i++;
          continue;
        } else if (lead >= 0xC0 && lead < 0xE0) {
          extra = 1;
          cp = lead & 0x1F;
        } else if (lead >= 0xE0 && lead < 0xF0) {
          extra = 2;
          cp = lead & 0x0F;
        } else if (lead >= 0xF0 && lead < 0xF8) {
          extra = 3;
          cp = lead & 0x07;
        } else {
          out += u'�';
          i++;
          continue;
        }
        if (i + extra >= text.size()) {
          out += u'�';
          break;
        }
        bool valid = true;
        for (size_t k = 1; k <= extra; k++) {
          valid = valid && (bytes[i + k] & 0xC0) == 0x80;
          cp = (cp << 6) | (bytes[i + k] & 0x3F);
        }
        if (!valid) {
          out += u'�';
          i++;
          continue;
        }
        i += extra + 1;
        if (cp > 0x10FFFF) {
          out += u'�';
        } else if (cp < 0x10000) {
          out += static_cast<char16_t>(cp);
        } else {
          out += static_cast<char16_t>(0xD800 + ((cp - 0x10000) >> 10));
          out += static_cast<char16_t>(0xDC00 + ((cp - 0x10000) & 0x3FF));
        }
      }
      return out;
    }

    // UTF-16 units and UTF-8 bytes that sit on the edges the converters branch on.
    constexpr char16_t kEdgeUnits[] = {
      0x0000, 0x0041, 0x007F, 0x0080, 0x00E9, 0x07FF, 0x0800, 0x3042, 0xD7FF,
      0xD800, 0xDBFF, 0xDC00, 0xDFFF, 0xE000, 0xFFFD, 0xFFFF, 0xD83D, 0xDE00,
    };
    constexpr unsigned char kEdgeBytes[] = {
      0x00, 0x41, 0x7F, 0x80, 0xBF, 0xC0, 0xC2, 0xC3, 0xA9, 0xDF, 0xE0, 0xE3,
      0xED, 0xA0, 0xEF, 0xF0, 0xF4, 0xF5, 0xF8, 0xFF, 0x9F, 0x98,
    };

    void expectBothDirections(std::u16string_view units, std::string_view bytes) {
      KOLIBRI_EXPECT_EQ(utf16ToUtf8(units.data(), units.size()), referenceUtf16ToUtf8(units));
      KOLIBRI_EXPECT_EQ(utf8ToUtf16(bytes), referenceUtf8ToUtf16(bytes));
    }

    jboolean conversionsMatchReference(JNIEnv*) {
      // An ASCII run of every length up to a few blocks, then one edge unit/byte and a tail: the
      // hand-over from the fast paths to the general ones at every offset, including a surrogate
      // pair split across the end of the run.
      for (size_t run = 0; run <= 200; run++) {
        for (const char16_t unit: kEdgeUnits) {
          std::u16string units(run, u'a');
          units += unit;
          units += u"tail\xD83D\xDE00";
          expectBothDirections(units, referenceUtf16ToUtf8(units));
        }
        for (const unsigned char byte: kEdgeBytes) {
          std::string bytes(run, 'a');
          bytes += static_cast<char>(byte);
          bytes += "\x80tail";
          expectBothDirections(referenceUtf8ToUtf16(bytes), bytes);
        }
      }

      // Pseudo-random text from the edge alphabets, seeded so a failure reproduces.
      std::mt19937 rng(20261030);
      for (int round = 0; round < 3000; round++) {
        const size_t length = rng() % 160;
        std::u16string units(length, u'\0');
        std::string bytes(length, '\0');
        for (size_t i = 0; i < length; i++) {
          const bool ascii = rng() % 4 != 0;
          units[i] = ascii ? static_cast<char16_t>(rng() % 0x80)
                           : kEdgeUnits[rng() % std::size(kEdgeUnits)];
          bytes[i] = static_cast<char>(
            ascii ? rng() % 0x80 : kEdgeBytes[rng() % std::size(kEdgeBytes)]
          );
        }
        expectBothDirections(units, bytes);
      }

      // Runs of two- and three-byte sequences are decoded in loops of their own. Whatever ends a
      // run (another length, a malformed or truncated sequence, a stray byte) hands back to the
      // general path, which has to pick up exactly where the run stopped.
      for (const std::string_view sequence: {std::string_view("\xD0\x96"), std::string_view("\xE6\x97\xA5")}) {
        for (size_t run = 0; run <= 9; run++) {
          std::string prefix;
          for (size_t k = 0; k < run; k++) {
            prefix += sequence;
          }
          for (const unsigned char byte: kEdgeBytes) {
            for (const std::string_view after: {std::string_view(), sequence, std::string_view("\x80tail")}) {
              std::string bytes = prefix;
              bytes += static_cast<char>(byte);
              bytes += after;
              KOLIBRI_EXPECT_EQ(utf8ToUtf16(bytes), referenceUtf8ToUtf16(bytes));
            }
          }
          // Cut off inside the sequence after the run.
          for (size_t cut = 1; cut < sequence.size(); cut++) {
            const std::string bytes = prefix + std::string(sequence.substr(0, cut));
            KOLIBRI_EXPECT_EQ(utf8ToUtf16(bytes), referenceUtf8ToUtf16(bytes));
          }
        }
      }

      // Long text is measured in blocks of 4096 units: a surrogate pair (or a lone half) on
      // either side of a block boundary, in text of one, two and three bytes a unit.
      for (const size_t length: {4095, 4096, 4097, 8193}) {
        for (const char16_t fill: {u'a', u'é', u'あ'}) {
          for (const size_t at: {size_t{0}, size_t{4094}, size_t{4095}, size_t{4096}, length - 2, length - 1}) {
            std::u16string units(length, fill);
            units[at] = 0xD83D;
            if (at + 1 < length) {
              units[at + 1] = 0xDE00;
            }
            expectBothDirections(units, referenceUtf16ToUtf8(units));
            units[at] = 0xDC00;
            KOLIBRI_EXPECT_EQ(utf16ToUtf8(units.data(), units.size()), referenceUtf16ToUtf8(units));
          }
        }
      }
      return JNI_TRUE;
    }

    // toJString hands NUL-free ASCII to NewStringUTF and everything else to the UTF-16 path; a NUL
    // or a non-ASCII byte at any position, inside an 8-byte word or in the tail, must still be
    // noticed. toStdString has to bring the same bytes back.
    jboolean toJStringChecksEveryByte(JNIEnv* env) {
      for (size_t length = 1; length <= 40; length++) {
        for (size_t at = 0; at < length; at++) {
          for (const char special: {'\0', '\x7F', static_cast<char>(0xC3)}) {
            std::string text(length, 'a');
            text[at] = special;
            if (special == static_cast<char>(0xC3) && at + 1 < length) {
              text[at + 1] = static_cast<char>(0xA9);
            }
            const Ref<jstring> str = Ref<jstring>::adopt(env, toJString(env, text));
            const std::u16string expected = referenceUtf8ToUtf16(text);
            KOLIBRI_EXPECT_EQ(toU16StdString(env, static_cast<jstring>(str.get())), expected);
            KOLIBRI_EXPECT_EQ(
              toStdString(env, static_cast<jstring>(str.get())),
              referenceUtf16ToUtf8(expected)
            );
          }
        }
      }
      return JNI_TRUE;
    }

    jboolean emptyStringIsNotNull(JNIEnv* env) {
      const jstring fromUtf8 = toJString(env, std::string{});
      KOLIBRI_EXPECT(fromUtf8 != nullptr);
      KOLIBRI_EXPECT_EQ(env->GetStringLength(fromUtf8), 0);

      const jstring fromUtf16 = toJString(env, std::u16string_view{});
      KOLIBRI_EXPECT(fromUtf16 != nullptr);
      KOLIBRI_EXPECT_EQ(env->GetStringLength(fromUtf16), 0);

      // A null jstring converts to an empty C++ string rather than faulting.
      KOLIBRI_EXPECT_EQ(toStdString(env, nullptr), std::string());
      KOLIBRI_EXPECT_EQ(toU16StdString(env, nullptr), std::u16string());
      return JNI_TRUE;
    }
  } // namespace

  void registerStringTests(JNIEnv* env) {
    registerNative(env, "io/github/expo/kolibri/tests/StringTests")
        .method<&roundTripUtf8>("nativeRoundTripUtf8")
        .method<&roundTripUtf16>("nativeRoundTripUtf16")
        .method<&utf8Conversions>("nativeUtf8Conversions")
        .method<&emptyStringIsNotNull>("nativeEmptyStringIsNotNull")
        .method<&conversionsMatchReference>("nativeConversionsMatchReference")
        .method<&toJStringChecksEveryByte>("nativeToJStringChecksEveryByte")
        .commit();
  }
} // namespace expo::kolibri::tests
