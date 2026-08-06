#include <kolibri/binary/BinaryReader.h>

#include <kolibri/string_utils.h>
#include <kolibri/binary/BinaryBuffer.h>
#include <kolibri/binary/BinaryString.h>

#include <stdexcept>
#include <vector>

namespace expo::kolibri::binary {
  Reader Reader::fromBuffer(BinaryBuffer& buffer, size_t size) {
    if (size != 0) {
      return {
        .position = buffer.data(),
        .end = buffer.data() + size
      };
    }

    // If size wasn't provided, we are going to read it from the buffer
    int32_t payloadSize;
    std::memcpy(&payloadSize, buffer.data(), sizeof(payloadSize));

    // We're going to add an offset, so the reader will skip the payload size
    uint8_t* start = buffer.data() + sizeof(payloadSize);
    return {
      .position = start,
      .end =  start + payloadSize - sizeof(payloadSize)
    };
  }

  void Reader::require(size_t n) const {
    if constexpr (kCheckedCodec) {
      if (static_cast<size_t>(end - position) < n) {
        throw std::runtime_error("Truncated binary payload");
      }
    }
  }

  Tag Reader::readTag() { return static_cast<Tag>(read<uint8_t>()); }

  bool Reader::readPresence() {
    const uint8_t present = read<uint8_t>();
    if constexpr (kCheckedCodec) {
      if (present > 1) {
        throw std::runtime_error("Invalid presence marker in binary payload");
      }
    }
    return present != 0;
  }

  size_t Reader::readCount() {
    const auto count = read<int32_t>();
    if constexpr (kCheckedCodec) {
      if (count < 0) {
        throw std::runtime_error("Negative count in binary payload");
      }
    }
    return static_cast<size_t>(count);
  }

  const uint8_t* Reader::readBytes(size_t n) {
    require(n);
    const uint8_t* bytes = position;
    position += n;
    return bytes;
  }

  bool Reader::isOnEnd() const { return position == end; }

  std::string Reader::readString() {
    return readAdaptiveString(
      *this,
      [](const char* bytes, size_t length) { return std::string{bytes, length}; },
      [](const char16_t* units, size_t count) { return utf16ToUtf8(units, count); }
    );
  }
}
