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

  void Reader::throwTruncated() {
    throw std::runtime_error("Truncated binary payload");
  }

  void Reader::throwInvalid(const char* message) {
    throw std::runtime_error(message);
  }

  std::string Reader::readString() {
    return binary::readString(*this);
  }
}
