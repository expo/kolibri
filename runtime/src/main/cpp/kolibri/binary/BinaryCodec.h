#pragma once

#include <bit>
#include <cstdint>
#include <cstring>
#include <optional>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>

#include <kolibri/binary/BinaryBuffer.h>
#include <kolibri/binary/BinaryFormat.h>
#include <kolibri/binary/BinaryReader.h>
#include <kolibri/binary/BinaryString.h>
#include <kolibri/defines.h>

namespace expo::kolibri::binary {
  namespace detail {
    template<typename T>
    inline constexpr bool isRawScalar =
        std::is_same_v<T, int32_t> || std::is_same_v<T, int64_t> ||
        std::is_same_v<T, float> || std::is_same_v<T, double>;
  } // namespace detail

  template<typename T>
  struct BinaryConverter;

  namespace detail {
    /**
     * A converter that can also decode into an existing object, `decodeInto(in, out)`. Containers
     * use it to fill their elements where they live, instead of decoding each one into a
     * temporary and moving it in.
     */
    template<typename T>
    concept DecodesInPlace = requires(Reader& in, T& out) {
      BinaryConverter<T>::decodeInto(in, out);
    };
  } // namespace detail

  template<typename T>
  T fromBinary(const uint8_t* data, size_t size) {
    Reader in{data, data + size};
    T value = BinaryConverter<std::decay_t<T>>::decode(in);
    if constexpr (kCheckedCodec) {
      if (!in.isOnEnd()) {
        throw std::runtime_error("Trailing bytes in binary payload");
      }
    }
    return value;
  }

  template<typename T>
  [[nodiscard]] bool toBinary(const T& value, BinaryBuffer& out) {
    out.clear();
    try {
      BinaryConverter<std::decay_t<T>>::encode(out, value);
      return true;
    } catch (const BufferOverflow&) {
      out.clear();
      return false;
    }
  }

  template<>
  struct BinaryConverter<bool> {
    static bool decode(Reader& in) { return in.read<uint8_t>() != 0; }

    static void encode(BinaryBuffer& out, bool value) { out.write<uint8_t>(value ? 1 : 0); }
  };

  namespace detail {
    template<typename T>
    struct ScalarConverter {
      static T decode(Reader& in) { return in.read<T>(); }

      static void encode(BinaryBuffer& out, T value) { out.write(value); }
    };
  } // namespace detail

  // clang-format off
  template<> struct BinaryConverter<int32_t> : detail::ScalarConverter<int32_t> {};
  template<> struct BinaryConverter<int64_t> : detail::ScalarConverter<int64_t> {};
  template<> struct BinaryConverter<float>   : detail::ScalarConverter<float> {};
  template<> struct BinaryConverter<double>  : detail::ScalarConverter<double> {};
  // clang-format on

  template<>
  struct BinaryConverter<std::string> {
    static std::string decode(Reader& in) { return readString(in); }

    static void encode(BinaryBuffer& out, const std::string& value) {
      writeString(out, value);
    }
  };

  template<typename T>
  struct BinaryConverter<std::optional<T>> {
    static std::optional<T> decode(Reader& in) {
      std::optional<T> result;
      decodeInto(in, result);
      return result;
    }

    // Always inlined, so that a container of optionals checks each presence byte in its own loop.
    ALWAYS_INLINE static void decodeInto(Reader& in, std::optional<T>& out) {
      if (!in.readPresence()) {
        out.reset();
      } else if constexpr (std::is_same_v<T, std::string>) {
        // An ASCII string is constructed in place: cheaper than assigning to an empty one.
        readAdaptiveString(
          in,
          [&](const char* bytes, size_t length) { out.emplace(bytes, length); },
          [&](const char16_t* units, size_t length) { out = utf16ToUtf8(units, length); }
        );
      } else if constexpr (detail::DecodesInPlace<T>) {
        BinaryConverter<T>::decodeInto(in, out.emplace());
      } else {
        out = BinaryConverter<T>::decode(in);
      }
    }

    // Always inlined, like decodeInto.
    ALWAYS_INLINE static void encode(BinaryBuffer& out, const std::optional<T>& value) {
      out.write<uint8_t>(value.has_value() ? 1 : 0);
      if (value.has_value()) {
        BinaryConverter<T>::encode(out, *value);
      }
    }
  };

  template<typename T>
  struct BinaryConverter<std::vector<T>> {
    static std::vector<T> decode(Reader& in) {
      const size_t count = in.readCount();
      if constexpr (detail::isRawScalar<T>) {
        std::vector<T> result(count);
        const uint8_t* bytes = in.readBytes(count * sizeof(T));
        if (count > 0) {
          std::memcpy(result.data(), bytes, count * sizeof(T));
        }
        return result;
      } else if constexpr (std::is_same_v<T, bool>) {
        // One byte per element, under one bounds check for the run. Writing an element of a
        // vector<bool> rewrites the word holding its bit, so the vector starts all false and only
        // the true elements are written, found eight bytes at a time.
        std::vector<T> result(count);
        const uint8_t* bytes = in.readBytes(count);
        size_t i = 0;
        if constexpr (std::endian::native == std::endian::little) {
          for (; i + 8 <= count; i += 8) {
            uint64_t word;
            std::memcpy(&word, bytes + i, sizeof(word));
            // The high bit of each nonzero byte; adding 0x7F to the low seven bits cannot carry out
            // of the byte.
            uint64_t set = (((word & 0x7F7F7F7F7F7F7F7FULL) + 0x7F7F7F7F7F7F7F7FULL) | word) &
                           0x8080808080808080ULL;
            for (; set != 0; set &= set - 1) {
              result[i + static_cast<size_t>(std::countr_zero(set) / 8)] = true;
            }
          }
        }
        for (; i < count; i++) {
          if (bytes[i] != 0) {
            result[i] = true;
          }
        }
        return result;
      } else if constexpr (std::is_same_v<T, std::string>) {
        // Each ASCII string is constructed in place, not decoded into a temporary and moved in.
        std::vector<T> result;
        result.reserve(count);
        for (size_t i = 0; i < count; i++) {
          readAdaptiveString(
            in,
            [&](const char* bytes, size_t length) { result.emplace_back(bytes, length); },
            [&](const char16_t* units, size_t length) {
              result.push_back(utf16ToUtf8(units, length));
            }
          );
        }
        return result;
      } else if constexpr (detail::DecodesInPlace<T>) {
        std::vector<T> result;
        result.reserve(count);
        for (size_t i = 0; i < count; i++) {
          BinaryConverter<T>::decodeInto(in, result.emplace_back());
        }
        return result;
      } else {
        std::vector<T> result;
        result.reserve(count);
        for (size_t i = 0; i < count; i++) {
          result.push_back(BinaryConverter<T>::decode(in));
        }
        return result;
      }
    }

    static void encode(BinaryBuffer& out, const std::vector<T>& values) {
      out.write<int32_t>(static_cast<int32_t>(values.size()));
      if constexpr (detail::isRawScalar<T>) {
        if (!values.empty()) {
          out.writeBytes(values.data(), values.size() * sizeof(T));
        }
      } else if constexpr (std::is_same_v<T, bool>) {
        uint8_t* bytes = out.advance(values.size());
        for (const bool value: values) {
          *bytes++ = value ? 1 : 0;
        }
      } else {
        for (const T& value: values) {
          BinaryConverter<T>::encode(out, value);
        }
      }
    }
  };

  /** Raw bytes: Kotlin ByteArray / JS ArrayBuffer. */
  template<>
  struct BinaryConverter<std::vector<uint8_t>> {
    static std::vector<uint8_t> decode(Reader& in) {
      const size_t size = in.readCount();
      const uint8_t* bytes = in.readBytes(size);
      return {bytes, bytes + size};
    }

    static void encode(BinaryBuffer& out, const std::vector<uint8_t>& bytes) {
      out.write<int32_t>(static_cast<int32_t>(bytes.size()));
      if (!bytes.empty()) {
        out.writeBytes(bytes.data(), bytes.size());
      }
    }
  };

  template<typename T>
  struct BinaryConverter<std::unordered_map<std::string, T>> {
    static std::unordered_map<std::string, T> decode(Reader& in) {
      const size_t count = in.readCount();
      std::unordered_map<std::string, T> result;
      result.reserve(count);
      for (size_t i = 0; i < count; i++) {
        // An ASCII key is built in place in its node, not decoded into a temporary and moved in.
        // Either way the value is decoded after the key's bytes have been consumed.
        readAdaptiveString(
          in,
          [&](const char* bytes, size_t length) {
            result.emplace(
              std::piecewise_construct,
              std::forward_as_tuple(bytes, length),
              std::forward_as_tuple(BinaryConverter<T>::decode(in))
            );
          },
          [&](const char16_t* units, size_t length) {
            result.emplace(utf16ToUtf8(units, length), BinaryConverter<T>::decode(in));
          }
        );
      }
      return result;
    }

    static void encode(BinaryBuffer& out, const std::unordered_map<std::string, T>& values) {
      out.write<int32_t>(static_cast<int32_t>(values.size()));
      for (const auto& [key, value]: values) {
        writeString(out, key);
        BinaryConverter<T>::encode(out, value);
      }
    }
  };
} // namespace expo::kolibri::binary
