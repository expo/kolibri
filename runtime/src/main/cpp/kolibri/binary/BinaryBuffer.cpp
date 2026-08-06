#include <kolibri/binary/BinaryBuffer.h>

#include <kolibri/JavaClass.h>
#include <kolibri/exception.h>
#include <kolibri/native_method.h>

namespace expo::kolibri::binary {
  namespace {
    struct JBinaryBuffer : JavaClass<JBinaryBuffer> {
      static constexpr std::string_view descriptor = "io/github/expo/kolibri/binary/BinaryBuffer";

      static void registerNatives(JNIEnv* env) {
        JavaClass::registerNatives(env)
          .method(
            "nativeGetBuffer",
            "()Ljava/nio/ByteBuffer;",
            [](JNIEnv* e) -> jobject { return BinaryBuffer::shared().javaByteBuffer(e); }
          )
          .commit();
      }
    };
  } // namespace

  BinaryBuffer::Claim::Claim(BinaryBuffer& buffer) : buffer_(buffer.inUse_ ? nullptr : &buffer) {
    if (buffer_ != nullptr) {
      buffer_->inUse_ = true;
    }
  }

  BinaryBuffer::Claim::~Claim() {
    if (buffer_ != nullptr) {
      buffer_->inUse_ = false;
    }
  }

  BinaryBuffer::Claim::operator bool() const { return buffer_ != nullptr; }

  BinaryBuffer& BinaryBuffer::Claim::buffer() const {
    return *buffer_;
  }

  Reader BinaryBuffer::Claim::reader(size_t size) const {
    return Reader::fromBuffer(buffer(), size);
  }

  BinaryBuffer& BinaryBuffer::shared() {
    thread_local BinaryBuffer buffer;
    return buffer;
  }

  void BinaryBuffer::registerNatives(JNIEnv* env) {
    JBinaryBuffer::registerNatives(env);
  }

  uint8_t* BinaryBuffer::data() { return data_.get(); }

  const uint8_t* BinaryBuffer::data() const { return data_.get(); }

  size_t BinaryBuffer::size() const { return size_; }

  void BinaryBuffer::clear() { size_ = 0; }

  void BinaryBuffer::setSize(size_t size) {
    assert(size <= kBufferCapacity);
    size_ = size;
  }

  void BinaryBuffer::require(size_t n) const {
    if (kBufferCapacity - size_ < n) [[unlikely]] {
      throw BufferOverflow{};
    }
  }

  void BinaryBuffer::writeBytes(const void* src, size_t n) {
    require(n);
    std::memcpy(data_.get() + size_, src, n);
    size_ += n;
  }

  void BinaryBuffer::writeTag(Tag tag) { write(static_cast<uint8_t>(tag)); }

  jobject BinaryBuffer::javaByteBuffer(JNIEnv* env) const {
    // It's a local ref that has to be store on the Java side.
    jobject buffer = env->NewDirectByteBuffer(data_.get(), kBufferCapacity);
    if (buffer == nullptr) {
      throwWithPending(env, "NewDirectByteBuffer failed for the binary bridge buffer");
    }
    return buffer;
  }
} // namespace expo::kolibri::binary
