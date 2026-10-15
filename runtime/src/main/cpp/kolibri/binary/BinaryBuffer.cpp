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

  void BinaryBuffer::throwOverflow() {
    throw BufferOverflow{};
  }

  jobject BinaryBuffer::javaByteBuffer(JNIEnv* env) const {
    // It's a local ref that has to be store on the Java side.
    jobject buffer = env->NewDirectByteBuffer(data_.get(), kBufferCapacity);
    if (buffer == nullptr) {
      throwWithPending(env, "NewDirectByteBuffer failed for the binary bridge buffer");
    }
    return buffer;
  }
} // namespace expo::kolibri::binary
