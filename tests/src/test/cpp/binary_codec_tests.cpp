#include <cstdint>
#include <cstring>
#include <optional>
#include <stdexcept>
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

    // Bool runs are written and read as one block of bytes, not one byte at a time; the format
    // must stay one 0/1 byte per element behind the count.
    jboolean boolRuns(JNIEnv*) {
      BinaryBuffer& buffer = BinaryBuffer::shared();
      for (const size_t count: {0, 1, 7, 8, 9, 63, 64, 65, 1000}) {
        std::vector<bool> values(count);
        for (size_t i = 0; i < count; i++) {
          values[i] = i % 3 == 0;
        }
        expectConverterRoundTrip(buffer, values);
        expectConverterRoundTrip(buffer, std::vector<bool>(count, true));

        KOLIBRI_EXPECT(toBinary(values, buffer));
        KOLIBRI_EXPECT_EQ(buffer.size(), sizeof(int32_t) + count);
        for (size_t i = 0; i < count; i++) {
          KOLIBRI_EXPECT_EQ(static_cast<int>(buffer.data()[sizeof(int32_t) + i]), values[i] ? 1 : 0);
        }
      }

      // Decoding finds the true elements a word of bytes at a time: every byte value, at every
      // position in a word and in the tail, reads back as whether it is non-zero.
      for (const size_t count: {255, 256, 257, 260}) {
        for (size_t shift = 0; shift < 8; shift++) {
          std::vector<uint8_t> payload(sizeof(int32_t) + count);
          const auto prefix = static_cast<int32_t>(count);
          std::memcpy(payload.data(), &prefix, sizeof(prefix));
          for (size_t i = 0; i < count; i++) {
            payload[sizeof(int32_t) + i] = static_cast<uint8_t>((i + shift) * 37);
          }
          const auto decoded = fromBinary<std::vector<bool>>(payload.data(), payload.size());
          KOLIBRI_EXPECT_EQ(decoded.size(), count);
          for (size_t i = 0; i < count; i++) {
            KOLIBRI_EXPECT_EQ(decoded[i], payload[sizeof(int32_t) + i] != 0);
          }
        }
      }

      // Any non-zero byte reads back as true, as before.
      const uint8_t payload[] = {3, 0, 0, 0, 0, 2, 0xFF};
      KOLIBRI_EXPECT(
        fromBinary<std::vector<bool>>(payload, sizeof(payload)) ==
        (std::vector<bool>{false, true, true})
      );

      if constexpr (kCheckedCodec) {
        // A run cut short by the end of the payload is still rejected, now by one check up front.
        bool threw = false;
        try {
          (void) fromBinary<std::vector<bool>>(payload, sizeof(payload) - 1);
        } catch (const std::runtime_error&) {
          threw = true;
        }
        KOLIBRI_EXPECT(threw);
      }
      return JNI_TRUE;
    }

    // A run of `T`s decoded from each of the eight alignments its payload can start at: aligned
    // runs and unaligned ones are copied differently. Values are random bit patterns (NaNs with
    // payloads included for the floating types) and must come back bit for bit.
    template<typename T>
    void expectScalarRunsAtEveryOffset() {
      uint64_t state = 0x9E3779B97F4A7C15ULL;
      for (const size_t count: {0, 1, 2, 3, 7, 8, 9, 64, 1000}) {
        std::vector<T> values(count);
        for (T& value: values) {
          state = state * 6364136223846793005ULL + 1442695040888963407ULL;
          std::memcpy(&value, &state, sizeof(T));
        }
        const auto prefix = static_cast<int32_t>(count);
        const size_t size = sizeof(prefix) + count * sizeof(T);
        std::vector<uint8_t> storage(8 + size);
        for (size_t offset = 0; offset < 8; offset++) {
          uint8_t* payload = storage.data() + offset;
          std::memcpy(payload, &prefix, sizeof(prefix));
          if (count > 0) {
            std::memcpy(payload + sizeof(prefix), values.data(), count * sizeof(T));
          }
          const auto decoded = fromBinary<std::vector<T>>(payload, size);
          KOLIBRI_EXPECT_EQ(decoded.size(), count);
          if (count > 0) {
            KOLIBRI_EXPECT(std::memcmp(decoded.data(), values.data(), count * sizeof(T)) == 0);
          }
        }
      }
    }

    jboolean scalarRuns(JNIEnv*) {
      expectScalarRunsAtEveryOffset<int32_t>();
      expectScalarRunsAtEveryOffset<int64_t>();
      expectScalarRunsAtEveryOffset<float>();
      expectScalarRunsAtEveryOffset<double>();

      // Runs nested behind presence bytes land at shifting offsets within one payload.
      BinaryBuffer& buffer = BinaryBuffer::shared();
      expectConverterRoundTrip(
        buffer,
        std::vector<std::optional<std::vector<double>>>{
          std::vector<double>{1.5, -2.25},
          std::nullopt,
          std::vector<double>{},
          std::vector<double>{0.5},
          std::vector<double>(100, 3.0),
        }
      );
      expectConverterRoundTrip(
        buffer,
        std::vector<std::optional<std::vector<int64_t>>>{
          std::nullopt, std::vector<int64_t>{int64_t{1} << 40, -1}, std::nullopt,
          std::vector<int64_t>{7},
        }
      );

      if constexpr (kCheckedCodec) {
        // A run cut short by the end of the payload is still rejected.
        KOLIBRI_EXPECT(toBinary(std::vector<double>{1.0, 2.0}, buffer));
        bool threw = false;
        try {
          (void) fromBinary<std::vector<double>>(buffer.data(), buffer.size() - 1);
        } catch (const std::runtime_error&) {
          threw = true;
        }
        KOLIBRI_EXPECT(threw);
      }
      return JNI_TRUE;
    }

    // Strings take the ASCII form or the UTF-16 one; the check that picks reads 8 bytes at a time,
    // so a non-ASCII byte has to be found in a full word, in the tail, and at the edges of both.
    // UTF-16 behind a presence byte lands at an odd offset and is realigned, on the stack up to
    // kStackUnits and on the heap past it.
    jboolean stringForms(JNIEnv*) {
      BinaryBuffer& buffer = BinaryBuffer::shared();
      for (size_t length = 1; length <= 24; length++) {
        // é as a whole two-byte sequence at every position it fits: valid UTF-8 round-trips.
        for (size_t at = 0; at + 1 < length; at++) {
          std::string text(length, 'a');
          text[at] = static_cast<char>(0xC3);
          text[at + 1] = static_cast<char>(0xA9);
          KOLIBRI_EXPECT(toBinary(text, buffer));
          int32_t prefix;
          std::memcpy(&prefix, buffer.data(), sizeof(prefix));
          KOLIBRI_EXPECT(prefix < 0);
          KOLIBRI_EXPECT_EQ(fromBinary<std::string>(buffer.data(), buffer.size()), text);
        }

        const std::string ascii(length, 'z');
        KOLIBRI_EXPECT(toBinary(ascii, buffer));
        KOLIBRI_EXPECT_EQ(buffer.size(), sizeof(int32_t) + length);
        KOLIBRI_EXPECT_EQ(fromBinary<std::string>(buffer.data(), buffer.size()), ascii);
      }

      std::string longNonAscii;
      for (size_t i = 0; i < kStackUnits + 100; i++) {
        longNonAscii += "\xC5\xBC";
      }
      const std::vector<std::optional<std::string>> optionals{
        "caf\xC3\xA9",
        std::nullopt,
        "\xE6\x97\xA5\xE6\x9C\xAC",
        "\xF0\x9F\x98\x80 pair",
        longNonAscii,
        std::string(kStackUnits + 100, 'a'),
        "",
      };
      expectConverterRoundTrip(buffer, optionals);

      expectConverterRoundTrip(
        buffer,
        std::unordered_map<std::string, std::optional<std::string>>{
          {"za\xC5\xBC\xC3\xB3\xC5\x82\xC4\x87", "g\xC4\x99\xC5\x9B"},
          {"ascii", std::nullopt},
        }
      );
      return JNI_TRUE;
    }

    // Containers construct their strings and optionals in place while decoding; every shape has
    // to come out as it went in, and a map keeps the first of two equal keys, as it did.
    jboolean inPlaceDecoding(JNIEnv*) {
      BinaryBuffer& buffer = BinaryBuffer::shared();
      expectConverterRoundTrip(buffer, std::vector<std::optional<double>>{1.5, std::nullopt, -0.0});
      expectConverterRoundTrip(
        buffer,
        std::vector<std::optional<std::string>>{
          "", std::nullopt, "ascii", "za\xC5\xBC\xC3\xB3\xC5\x82\xC4\x87", std::nullopt,
          std::string(kStackUnits + 1, 'x'),
        }
      );
      expectConverterRoundTrip(buffer, std::optional<std::vector<std::string>>{{"a", "b\xC3\xA9"}});
      expectConverterRoundTrip(buffer, std::optional<std::vector<std::string>>{});
      expectConverterRoundTrip(buffer, std::vector<std::string>{});
      expectConverterRoundTrip(buffer, std::vector<std::string>{"", "\xF0\x9F\x98\x80", "x"});

      // Two entries under the key "a" (ASCII form), values 1 then 2: the first one is kept.
      const uint8_t duplicates[] = {
        2, 0, 0, 0,
        1, 0, 0, 0, 'a', 1, 0, 0, 0,
        1, 0, 0, 0, 'a', 2, 0, 0, 0,
      };
      const auto decoded =
        fromBinary<std::unordered_map<std::string, int32_t>>(duplicates, sizeof(duplicates));
      KOLIBRI_EXPECT_EQ(decoded.size(), size_t{1});
      KOLIBRI_EXPECT_EQ(decoded.at("a"), 1);

      // The same with the key in the UTF-16 form (é), which takes the other path.
      const uint8_t utf16Duplicates[] = {
        2, 0, 0, 0,
        0xFF, 0xFF, 0xFF, 0xFF, 0xE9, 0x00, 7, 0, 0, 0,
        0xFF, 0xFF, 0xFF, 0xFF, 0xE9, 0x00, 8, 0, 0, 0,
      };
      const auto decodedUtf16 =
        fromBinary<std::unordered_map<std::string, int32_t>>(utf16Duplicates, sizeof(utf16Duplicates));
      KOLIBRI_EXPECT_EQ(decodedUtf16.size(), size_t{1});
      KOLIBRI_EXPECT_EQ(decodedUtf16.at("\xC3\xA9"), 7);

      if constexpr (kCheckedCodec) {
        // A string list cut off inside its last string is still rejected.
        KOLIBRI_EXPECT(toBinary(std::vector<std::string>{"first", "second"}, buffer));
        bool threw = false;
        try {
          (void) fromBinary<std::vector<std::string>>(buffer.data(), buffer.size() - 1);
        } catch (const std::runtime_error&) {
          threw = true;
        }
        KOLIBRI_EXPECT(threw);
      }
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
        .method<&boolRuns>("nativeBoolRuns")
        .method<&scalarRuns>("nativeScalarRuns")
        .method<&stringForms>("nativeStringForms")
        .method<&inPlaceDecoding>("nativeInPlaceDecoding")
        .commit();
  }
} // namespace expo::kolibri::tests
