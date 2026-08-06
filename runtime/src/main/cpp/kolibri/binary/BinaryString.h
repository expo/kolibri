#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include <kolibri/binary/BinaryBuffer.h>
#include <kolibri/binary/BinaryReader.h>
#include <kolibri/string_utils.h>

namespace expo::kolibri::binary {
  inline bool isAllAscii(
    const char* data,
    const size_t size
  ) {
    for (size_t i = 0; i < size; i++) {
      if (static_cast<unsigned char>(data[i]) >= 0x80) {
        return false;
      }
    }
    return true;
  }

  inline void writeString(
    BinaryBuffer& out,
    const std::string& value
  ) {
    if (isAllAscii(value.data(), value.size())) {
      out.write<int32_t>(static_cast<int32_t>(value.size()));
      out.writeBytes(value.data(), value.size());
      return;
    }
    // Convert into a stack buffer for typical short strings (no heap); fall back to a heap
    // buffer only for very long ones. UTF-16 never has more units than the UTF-8 byte count.
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

  inline void writeString(
    BinaryBuffer& out,
    const std::u16string_view value
  ) {
    out.write<int32_t>(-static_cast<int32_t>(value.size()));
    out.writeBytes(value.data(), value.size() * sizeof(char16_t));
  }

  inline std::string readString(Reader& in) {
    return in.readString();
  }

  template<typename MakeAscii, typename MakeUtf16>
  auto readAdaptiveString(Reader& in, MakeAscii&& makeAscii, MakeUtf16&& makeUtf16) {
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

    if ((reinterpret_cast<uintptr_t>(bytes) & 1) == 0) {
      return makeUtf16(reinterpret_cast<const char16_t*>(bytes), units);
    }
    // Tag and presence bytes can leave the payload odd-aligned; realign before reading units.
    std::vector<char16_t> aligned(units);
    std::memcpy(aligned.data(), bytes, units * sizeof(char16_t));
    return makeUtf16(aligned.data(), units);
  }
}
