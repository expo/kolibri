#pragma once

#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>
#include <vector>

#include <kolibri/binary/BinaryBuffer.h>
#include <kolibri/binary/BinaryReader.h>
#include <kolibri/defines.h>
#include <kolibri/string_utils.h>

namespace expo::kolibri::binary {
  inline bool isAllAscii(
    const char* data,
    const size_t size
  ) {
    // 8 bytes per step, stopping at the first word with a high bit set.
    size_t i = 0;
    for (; i + 8 <= size; i += 8) {
      uint64_t word;
      std::memcpy(&word, data + i, sizeof(word));
      if ((word & 0x8080808080808080ULL) != 0) {
        return false;
      }
    }
    for (; i < size; i++) {
      if (static_cast<unsigned char>(data[i]) >= 0x80) {
        return false;
      }
    }
    return true;
  }

  namespace detail {
    // A non-ASCII string goes out in the UTF-16 form. Converted into a stack buffer for typical
    // short strings (no heap), and a heap buffer only for very long ones: UTF-16 never has more
    // units than the UTF-8 byte count. Out of line, so the ASCII path does not carry the buffer.
    NOINLINE inline void writeUtf16Form(BinaryBuffer& out, const std::string& value) {
      if (value.size() <= kStackUnits) {
        char16_t buf[kStackUnits];
        const size_t n = utf8ToUtf16Into(value, buf);
        out.write<int32_t>(-static_cast<int32_t>(n));
        out.writeBytes(buf, n * sizeof(char16_t));
      } else {
        std::vector<char16_t> buf(value.size());
        const size_t n = utf8ToUtf16Into(value, buf.data());
        out.write<int32_t>(-static_cast<int32_t>(n));
        out.writeBytes(buf.data(), n * sizeof(char16_t));
      }
    }
  } // namespace detail

  // Always inlined: a call per string is a large share of encoding a run of short ones.
  ALWAYS_INLINE void writeString(
    BinaryBuffer& out,
    const std::string& value
  ) {
    const size_t size = value.size();
    if (isAllAscii(value.data(), size)) {
      // The length prefix and the bytes under one bounds check.
      uint8_t* at = out.advance(sizeof(int32_t) + size);
      const auto prefix = static_cast<int32_t>(size);
      std::memcpy(at, &prefix, sizeof(prefix));
      std::memcpy(at + sizeof(prefix), value.data(), size);
      return;
    }
    detail::writeUtf16Form(out, value);
  }

  inline void writeString(
    BinaryBuffer& out,
    const std::u16string_view value
  ) {
    out.write<int32_t>(-static_cast<int32_t>(value.size()));
    out.writeBytes(value.data(), value.size() * sizeof(char16_t));
  }

  namespace detail {
    // Tag and presence bytes can leave a UTF-16 payload odd-aligned; it is realigned before its
    // units are read, on the stack for typical lengths. Out of line, so that the common aligned
    // read does not carry the buffer in its frame.
    template<typename MakeUtf16>
    NOINLINE auto readMisalignedUtf16(const uint8_t* bytes, const size_t units, MakeUtf16& makeUtf16) {
      if (units <= kStackUnits) {
        char16_t aligned[kStackUnits];
        std::memcpy(aligned, bytes, units * sizeof(char16_t));
        return makeUtf16(aligned, units);
      }
      std::vector<char16_t> aligned(units);
      std::memcpy(aligned.data(), bytes, units * sizeof(char16_t));
      return makeUtf16(aligned.data(), units);
    }
  } // namespace detail

  // Always inlined: a call per string is a large share of decoding a run of short ones.
  template<typename MakeAscii, typename MakeUtf16>
  ALWAYS_INLINE auto readAdaptiveString(Reader& in, MakeAscii&& makeAscii, MakeUtf16&& makeUtf16) {
    const auto prefix = in.read<int32_t>();
    // If ascii
    if (prefix >= 0) {
      const auto length = static_cast<size_t>(prefix);
      const auto* bytes = reinterpret_cast<const char*>(in.readBytes(length));
      return makeAscii(bytes, length);
    }

    // utf16
    const auto units = static_cast<size_t>(-static_cast<int64_t>(prefix));
    const uint8_t* bytes = in.readBytes(units * sizeof(char16_t));

    if ((reinterpret_cast<uintptr_t>(bytes) & 1) == 0) [[likely]] {
      return makeUtf16(reinterpret_cast<const char16_t*>(bytes), units);
    }
    return detail::readMisalignedUtf16(bytes, units, makeUtf16);
  }

  // Inline, so decoding a run of strings does not pay a call per string.
  inline std::string readString(Reader& in) {
    return readAdaptiveString(
      in,
      [](const char* bytes, size_t length) { return std::string{bytes, length}; },
      [](const char16_t* units, size_t count) { return utf16ToUtf8(units, count); }
    );
  }
}
