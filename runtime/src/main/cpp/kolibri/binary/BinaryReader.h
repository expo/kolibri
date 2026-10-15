#pragma once

#include <cstdint>
#include <cstring>
#include <string>

#include <kolibri/binary/BinaryFormat.h>
#include <kolibri/defines.h>

namespace expo::kolibri::binary {
  class BinaryBuffer;

  struct Reader {
    const uint8_t* position;
    const uint8_t* end;

    static Reader fromBuffer(BinaryBuffer& buffer, size_t size = 0);

    void require([[maybe_unused]] size_t n) const {
      if constexpr (kCheckedCodec) {
        if (static_cast<size_t>(end - position) < n) {
          throwTruncated();
        }
      }
    }

    template<typename T>
    T read() {
      require(sizeof(T));
      T value;
      std::memcpy(&value, position, sizeof(T));
      position += sizeof(T);
      return value;
    }

    Tag readTag() { return static_cast<Tag>(read<uint8_t>()); }

    [[nodiscard]] bool readPresence() {
      const uint8_t present = read<uint8_t>();
      if constexpr (kCheckedCodec) {
        if (present > 1) {
          throwInvalid("Invalid presence marker in binary payload");
        }
      }
      return present != 0;
    }

    [[nodiscard]] size_t readCount() {
      const auto count = read<int32_t>();
      if constexpr (kCheckedCodec) {
        if (count < 0) {
          throwInvalid("Negative count in binary payload");
        }
      }
      return static_cast<size_t>(count);
    }

    [[nodiscard]] const uint8_t* readBytes(const size_t n) {
      require(n);
      const uint8_t* bytes = position;
      position += n;
      return bytes;
    }

    [[nodiscard]] bool isOnEnd() const { return position == end; }

    [[nodiscard]] std::string readString();

  private:
    NORETURN NOINLINE COLD static void throwTruncated();

    NORETURN NOINLINE COLD static void throwInvalid(const char* message);
  };
}
