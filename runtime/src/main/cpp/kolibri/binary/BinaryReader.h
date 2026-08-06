#pragma once

#include <cstring>
#include <string>

#include <kolibri/binary/BinaryFormat.h>

namespace expo::kolibri::binary {
  class BinaryBuffer;

  struct Reader {
    const uint8_t* position;
    const uint8_t* end;

    static Reader fromBuffer(BinaryBuffer& buffer, size_t size = 0);

    void require([[maybe_unused]] size_t n) const;

    template<typename T>
    T read() {
      require(sizeof(T));
      T value;
      std::memcpy(&value, position, sizeof(T));
      position += sizeof(T);
      return value;
    }

    Tag readTag();

    [[nodiscard]] bool readPresence();

    [[nodiscard]] size_t readCount();

    [[nodiscard]] const uint8_t* readBytes(size_t n);

    [[nodiscard]] bool isOnEnd() const;

    [[nodiscard]] std::string readString();
  };
}
