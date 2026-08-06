#include <string>

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
        .commit();
  }
} // namespace expo::kolibri::tests
