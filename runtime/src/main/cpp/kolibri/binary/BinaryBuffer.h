#pragma once

#include <cassert>
#include <cstring>
#include <jni.h>
#include <memory>
#include <type_traits>

#include <kolibri/binary/BinaryFormat.h>
#include <kolibri/binary/BinaryReader.h>
#include <kolibri/defines.h>
#include <kolibri/Ref.h>

namespace expo::kolibri::binary {
  struct BufferOverflow : std::exception {
  };

  class BinaryBuffer {
  public:
    class [[nodiscard]] Claim {
    public:
      explicit Claim(BinaryBuffer& buffer = shared()) : buffer_(buffer.inUse_ ? nullptr : &buffer) {
        if (buffer_ != nullptr) {
          buffer_->inUse_ = true;
        }
      }

      ~Claim() {
        if (buffer_ != nullptr) {
          buffer_->inUse_ = false;
        }
      }

      Claim(const Claim&) = delete;

      Claim& operator=(const Claim&) = delete;

      explicit operator bool() const { return buffer_ != nullptr; }

      BinaryBuffer& buffer() const { return *buffer_; }

      Reader reader(size_t size = 0) const;

    private:
      BinaryBuffer* buffer_;
    };

    static BinaryBuffer& shared();

    // Registers the natives of the Kotlin `io.github.expo.kolibri.binary.BinaryBuffer` endpoint,
    // through which consumers obtain the calling thread's buffer. Call once from JNI_OnLoad.
    static void registerNatives(JNIEnv* env);

    BinaryBuffer(const BinaryBuffer&) = delete;

    BinaryBuffer& operator=(const BinaryBuffer&) = delete;

    [[nodiscard]] uint8_t* data() { return data_.get(); }

    [[nodiscard]] const uint8_t* data() const { return data_.get(); }

    // TODO(@lukmccall): rename to current position
    [[nodiscard]] size_t size() const { return size_; }

    void clear() { size_ = 0; }

    void setSize(size_t size) {
      assert(size <= kBufferCapacity);
      size_ = size;
    }

    void require(const size_t n) const {
      if (kBufferCapacity - size_ < n) [[unlikely]] {
        throwOverflow();
      }
    }

    void writeBytes(const void* src, size_t n) {
      require(n);
      std::memcpy(data_.get() + size_, src, n);
      size_ += n;
    }

    template<typename T>
    void write(T value) requires(std::is_arithmetic_v<T>) {
      writeBytes(&value, sizeof(value));
    }

    template<typename T>
    void writeAt(size_t offset, T value) requires(std::is_arithmetic_v<T>) {
      assert(offset + sizeof(value) <= kBufferCapacity);
      std::memcpy(data_.get() + offset, &value, sizeof(value));
    }

    void writeTag(Tag tag) { write(static_cast<uint8_t>(tag)); }

    [[nodiscard]] jobject javaByteBuffer(JNIEnv* env) const;

  private:
    NORETURN NOINLINE COLD static void throwOverflow();

    // Deliberately NOT zero-initialized (make_unique_for_overwrite): zeroing would touch every
    // page and make the whole capacity resident up front. Left untouched, the allocation is
    // virtual address space only, and physical pages commit lazily as payloads are written — a
    // thread's real memory cost is its payload high-water mark (4 KiB granularity), not
    // kBufferCapacity. Safe because the binary format never reads bytes it did not write: every
    // reader is bounded by the payload length the writer reported.
    BinaryBuffer() : data_(std::make_unique_for_overwrite<uint8_t[]>(kBufferCapacity)) {
    }

    std::unique_ptr<uint8_t[]> data_;
    size_t size_ = 0;
    bool inUse_ = false;
  };
} // namespace expo::kolibri::binary
