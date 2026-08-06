#pragma once

#include <cstdint>
#include <cstring>
#include <optional>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <vector>

#include <kolibri/binary/BinaryBuffer.h>
#include <kolibri/binary/BinaryFormat.h>
#include <kolibri/binary/BinaryReader.h>
#include <kolibri/binary/BinaryString.h>

namespace expo::kolibri::binary {
  namespace detail {
    template<typename T>
    inline constexpr bool isRawScalar =
        std::is_same_v<T, int32_t> || std::is_same_v<T, int64_t> ||
        std::is_same_v<T, float> || std::is_same_v<T, double>;
  } // namespace detail

  template<typename T>
  struct BinaryConverter;

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
      if (!in.readPresence()) {
        return std::nullopt;
      }
      return BinaryConverter<T>::decode(in);
    }

    static void encode(BinaryBuffer& out, const std::optional<T>& value) {
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
        // Raw scalar run: one bounds check (checked builds only) and one memcpy.
        std::vector<T> result(count);
        const uint8_t* bytes = in.readBytes(count * sizeof(T));
        if (count > 0) {
          std::memcpy(result.data(), bytes, count * sizeof(T));
        }
        return result;
      } else if constexpr (std::is_same_v<T, bool>) {
        std::vector<T> result(count);
        for (size_t i = 0; i < count; i++) {
          result[i] = in.read<uint8_t>() != 0;
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
        for (const bool value: values) {
          out.write<uint8_t>(value ? 1 : 0);
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
        std::string key = readString(in);
        result.emplace(std::move(key), BinaryConverter<T>::decode(in));
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
