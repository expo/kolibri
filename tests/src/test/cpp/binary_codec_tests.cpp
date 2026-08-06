#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include <kolibri/binary/BinaryBuffer.h>
#include <kolibri/binary/BinaryCodec.h>
#include <kolibri/binary/BinaryFormat.h>
#include <kolibri/binary/BinaryReader.h>
#include <kolibri/binary/BinaryString.h>
#include <kolibri/native_method.h>

#include "expect.h"
#include "test_helpers.h"

namespace expo::kolibri::tests {
  namespace {
    using namespace expo::kolibri::binary;

    int readTagByte(Reader& in) { return static_cast<int>(in.readTag()); }

    jboolean verifyKotlinPayload(JNIEnv*, jint length) {
      const BinaryBuffer& buffer = BinaryBuffer::shared();
      Reader in{buffer.data(), buffer.data() + static_cast<size_t>(length)};

      KOLIBRI_EXPECT_EQ(readTagByte(in), static_cast<int>(Tag::LIST));
      KOLIBRI_EXPECT_EQ(in.read<int32_t>(), 12);
      KOLIBRI_EXPECT_EQ(readTagByte(in), static_cast<int>(Tag::TAGGED));

      KOLIBRI_EXPECT_EQ(readTagByte(in), static_cast<int>(Tag::NULLTAG));

      KOLIBRI_EXPECT_EQ(readTagByte(in), static_cast<int>(Tag::BOOLEAN));
      KOLIBRI_EXPECT_EQ(static_cast<int>(in.read<uint8_t>()), 1);

      KOLIBRI_EXPECT_EQ(readTagByte(in), static_cast<int>(Tag::INT));
      KOLIBRI_EXPECT_EQ(in.read<int32_t>(), 42);

      KOLIBRI_EXPECT_EQ(readTagByte(in), static_cast<int>(Tag::LONG));
      KOLIBRI_EXPECT_EQ(in.read<int64_t>(), int64_t{1} << 40);

      KOLIBRI_EXPECT_EQ(readTagByte(in), static_cast<int>(Tag::DOUBLE));
      KOLIBRI_EXPECT_EQ(in.read<double>(), 2.25);

      KOLIBRI_EXPECT_EQ(readTagByte(in), static_cast<int>(Tag::STRING));
      KOLIBRI_EXPECT_EQ(readString(in), std::string("ascii"));

      KOLIBRI_EXPECT_EQ(readTagByte(in), static_cast<int>(Tag::STRING));
      KOLIBRI_EXPECT_EQ(readString(in), std::string("odd"));

      KOLIBRI_EXPECT_EQ(readTagByte(in), static_cast<int>(Tag::STRING));
      KOLIBRI_EXPECT_EQ(readString(in), std::string("caf\xC3\xA9"));

      KOLIBRI_EXPECT_EQ(readTagByte(in), static_cast<int>(Tag::FLOAT));
      KOLIBRI_EXPECT_EQ(in.read<float>(), 1.5f);

      KOLIBRI_EXPECT_EQ(readTagByte(in), static_cast<int>(Tag::BYTE_ARRAY));
      KOLIBRI_EXPECT_EQ(in.read<int32_t>(), 3);
      const uint8_t* bytes = in.readBytes(3);
      KOLIBRI_EXPECT_EQ(static_cast<int>(bytes[0]), 1);
      KOLIBRI_EXPECT_EQ(static_cast<int>(bytes[1]), 2);
      KOLIBRI_EXPECT_EQ(static_cast<int>(bytes[2]), 3);

      KOLIBRI_EXPECT_EQ(readTagByte(in), static_cast<int>(Tag::LIST));
      KOLIBRI_EXPECT_EQ(in.read<int32_t>(), 3);
      KOLIBRI_EXPECT_EQ(readTagByte(in), static_cast<int>(Tag::INT));
      KOLIBRI_EXPECT_EQ(in.read<int32_t>(), 1);
      KOLIBRI_EXPECT_EQ(in.read<int32_t>(), 2);
      KOLIBRI_EXPECT_EQ(in.read<int32_t>(), 3);

      KOLIBRI_EXPECT_EQ(readTagByte(in), static_cast<int>(Tag::MAP));
      KOLIBRI_EXPECT_EQ(in.read<int32_t>(), 1);
      KOLIBRI_EXPECT_EQ(readString(in), std::string("key"));
      KOLIBRI_EXPECT_EQ(readTagByte(in), static_cast<int>(Tag::STRING));
      KOLIBRI_EXPECT_EQ(readString(in), std::string("value"));

      KOLIBRI_EXPECT(in.isOnEnd());
      return JNI_TRUE;
    }

    jboolean verifyDeepList(JNIEnv*, jint length, jint expectedDepth, jint innermost) {
      const BinaryBuffer& buffer = BinaryBuffer::shared();
      Reader in{buffer.data(), buffer.data() + static_cast<size_t>(length)};

      int listLevels = 0;
      while (true) {
        KOLIBRI_EXPECT_EQ(readTagByte(in), static_cast<int>(Tag::LIST));
        KOLIBRI_EXPECT_EQ(in.read<int32_t>(), 1);
        listLevels++;
        const int elementTag = readTagByte(in);
        if (elementTag == static_cast<int>(Tag::TAGGED)) {
          continue;
        }
        KOLIBRI_EXPECT_EQ(elementTag, static_cast<int>(Tag::INT));
        KOLIBRI_EXPECT_EQ(in.read<int32_t>(), innermost);
        break;
      }
      KOLIBRI_EXPECT_EQ(listLevels, expectedDepth);
      KOLIBRI_EXPECT(in.isOnEnd());
      return JNI_TRUE;
    }

    jint encodePayload(JNIEnv*) {
      BinaryBuffer& buffer = BinaryBuffer::shared();
      buffer.clear();
      buffer.writeTag(Tag::LIST);
      buffer.write<int32_t>(7);
      buffer.writeTag(Tag::TAGGED);

      buffer.writeTag(Tag::NULLTAG);
      buffer.writeTag(Tag::BOOLEAN);
      buffer.write<uint8_t>(1);
      buffer.writeTag(Tag::INT);
      buffer.write<int32_t>(-7);
      buffer.writeTag(Tag::DOUBLE);
      buffer.write<double>(3.5);
      buffer.writeTag(Tag::STRING);
      writeString(buffer, std::string("hello"));
      buffer.writeTag(Tag::STRING);
      writeString(buffer, std::string("za\xC5\xBC\xC3\xB3\xC5\x82\xC4\x87")); // "zażółć"
      buffer.writeTag(Tag::STRING);
      // The UTF-16 overload, with a surrogate pair. "gęś 🦆"
      writeString(buffer, std::u16string(u"gęś \U0001F986"));
      return static_cast<jint>(buffer.size());
    }

    jlong bufferAddress(JNIEnv*) {
      return reinterpret_cast<jlong>(BinaryBuffer::shared().data());
    }

    jboolean claimSemantics(JNIEnv*) {
      BinaryBuffer& buffer = BinaryBuffer::shared();
      {
        const BinaryBuffer::Claim outer(buffer);
        KOLIBRI_EXPECT(static_cast<bool>(outer));
        const BinaryBuffer::Claim nested(buffer);
        KOLIBRI_EXPECT(!nested);
      }
      const BinaryBuffer::Claim reclaimed(buffer);
      KOLIBRI_EXPECT(static_cast<bool>(reclaimed));
      return JNI_TRUE;
    }

    template<typename T>
    void expectConverterRoundTrip(BinaryBuffer& buffer, const T& value) {
      KOLIBRI_EXPECT(toBinary(value, buffer));
      const T decoded = fromBinary<T>(buffer.data(), buffer.size());
      KOLIBRI_EXPECT(decoded == value);
    }

    jboolean cppRoundTrips(JNIEnv*) {
      BinaryBuffer& buffer = BinaryBuffer::shared();
      expectConverterRoundTrip(buffer, true);
      expectConverterRoundTrip(buffer, int32_t{-42});
      expectConverterRoundTrip(buffer, int64_t{1} << 40);
      expectConverterRoundTrip(buffer, 1.5f);
      expectConverterRoundTrip(buffer, 2.25);
      expectConverterRoundTrip(buffer, std::string("ascii and za\xC5\xBC\xC3\xB3\xC5\x82"));
      expectConverterRoundTrip(buffer, std::vector<double>{1.5, -2.25, 0.0});
      expectConverterRoundTrip(buffer, std::vector<int32_t>{1, -2, 3});
      expectConverterRoundTrip(buffer, std::vector<bool>{true, false, true});
      expectConverterRoundTrip(buffer, std::vector<uint8_t>{0x00, 0x7F, 0xFF});
      expectConverterRoundTrip(
        buffer,
        std::unordered_map<std::string, int32_t>{{"a", 1}, {"b", 2}}
      );
      expectConverterRoundTrip(buffer, std::optional<std::string>{"present"});
      expectConverterRoundTrip(buffer, std::optional<std::string>{});
      expectConverterRoundTrip(buffer, std::vector<std::string>{"x", "café", ""});
      return JNI_TRUE;
    }

    jboolean overflowReturnsFalse(JNIEnv*) {
      BinaryBuffer& buffer = BinaryBuffer::shared();
      // 40k doubles = 320 KiB > kBufferCapacity (256 KiB). Never grows: reports and clears.
      const std::vector<double> tooBig(40'000);
      KOLIBRI_EXPECT(!toBinary(tooBig, buffer));
      KOLIBRI_EXPECT_EQ(buffer.size(), size_t{0});

      // require() past the capacity throws BufferOverflow directly.
      bool threw = false;
      try {
        buffer.require(kBufferCapacity + 1);
      } catch (const BufferOverflow&) {
        threw = true;
      }
      KOLIBRI_EXPECT(threw);

      // The buffer stays usable after an overflow.
      expectConverterRoundTrip(buffer, std::vector<double>(1000, 1.0));
      return JNI_TRUE;
    }
  } // namespace

  void registerBinaryCodecTests(JNIEnv* env) {
    registerNative(env, "io/github/expo/kolibri/tests/BinaryCodecTests")
        .method<&verifyKotlinPayload>("nativeVerifyKotlinPayload")
        .method<&verifyDeepList>("nativeVerifyDeepList")
        .method<&encodePayload>("nativeEncodePayload")
        .method<&bufferAddress>("nativeBufferAddress")
        .method<&claimSemantics>("nativeClaimSemantics")
        .method<&cppRoundTrips>("nativeCppRoundTrips")
        .method<&overflowReturnsFalse>("nativeOverflowReturnsFalse")
        .commit();
  }
} // namespace expo::kolibri::tests
