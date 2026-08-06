#include <atomic>

#include <kolibri/NativeObject.h>
#include <kolibri/native_method.h>

#include "expect.h"
#include "test_helpers.h"

namespace expo::kolibri::tests {
  namespace {
    struct TestPeer : NativeObject<TestPeer> {
      static constexpr std::string_view descriptor = "io/github/expo/kolibri/tests/TestPeer";

      static std::atomic<int> liveCount;

      explicit TestPeer(jint value) : value_(value) { liveCount.fetch_add(1); }

      ~TestPeer() override { liveCount.fetch_sub(1); }

      jint getValue() noexcept { return value_; }

      void setValue(jint value) noexcept { value_ = value; }

      jint value_;
    };

    std::atomic<int> TestPeer::liveCount{0};
  } // namespace

  void registerNativeObjectTests(JNIEnv* env) {
    registerNative<TestPeer>(env)
        .method("nativeCreate", [](jint value) -> jlong {
          return reinterpret_cast<jlong>(new TestPeer(value));
        })
        .method("liveCount", []() noexcept -> jint { return TestPeer::liveCount.load(); })
        .method<&TestPeer::getValue>("nativeGetValue")
        .method<&TestPeer::setValue>("nativeSetValue")
        .commit();

    registerNative(env, "io/github/expo/kolibri/tests/NativeObjectTests")
        .method(
          "nativeThisValue",
          JniSignatureFor<jint(Ref<TestPeer>)>::value(),
          [](JNIEnv* e, jobject peer) -> jint {
            TestPeer* native = JNativeObject::nativeThis<TestPeer>(e, peer);
            KOLIBRI_EXPECT(native != nullptr);
            return native->value_;
          }
        )
        .commit();
  }
} // namespace expo::kolibri::tests
