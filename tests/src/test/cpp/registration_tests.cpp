#include <string>

#include <kolibri/JavaReceiver.h>
#include <kolibri/Ref.h>
#include <kolibri/native_method.h>
#include <kolibri/string_utils.h>

#include "expect.h"
#include "test_helpers.h"

namespace expo::kolibri::tests {
  namespace {
    jint freeEcho(jint value) noexcept { return value; }

    jlong freeEchoLong(jlong value) noexcept { return value; }
  } // namespace

  void registerRegistrationTests(JNIEnv* env) {
    registerNative(env, "io/github/expo/kolibri/tests/RegistrationTests")
        .method<&freeEcho>("freeFunctionEcho")
        .method<&freeEchoLong, jlong(jlong)>("freeFunctionWithSignatureTemplate")
        .method(
          "lambdaConcat",
          [](JNIEnv* e, jstring a, jstring b) -> jstring {
            return toJString(e, toStdString(e, a) + toStdString(e, b));
          }
        )
        .method("lambdaWithExplicitSignature", "(I)I", [](jint value) -> jint { return value + 1; })
        .method("envOnly", [](JNIEnv* e) -> jboolean { return e != nullptr ? JNI_TRUE : JNI_FALSE; })
        .method(
          "receiverOnly",
          [](JavaReceiver self) -> jboolean { return self.self != nullptr ? JNI_TRUE : JNI_FALSE; }
        )
        .method(
          "envAndReceiver",
          [](JNIEnv* e, JavaReceiver self) -> jboolean {
            KOLIBRI_EXPECT(e != nullptr);
            KOLIBRI_EXPECT(self.self != nullptr);
            return JNI_TRUE;
          }
        )
        .method("noEnvNoReceiver", [](jint a, jint b) noexcept -> jint { return a + b; })
        .method(
          "refReturn",
          [](JNIEnv* e) -> Ref<jstring> {
            return Ref<jstring>::adopt(e, toJString(e, std::string("owned local")));
          }
        )
        .method(
          "nullableRefReturn",
          [](JNIEnv* e, jboolean returnNull) -> Ref<jstring> {
            if (returnNull == JNI_TRUE) {
              return {};
            }
            return Ref<jstring>::adopt(e, toJString(e, std::string("present")));
          }
        )
        .method(
          "globalRefReturn",
          [](JNIEnv* e) -> GlobalRef<jstring> {
            const auto local =
                Ref<jstring>::adopt(e, toJString(e, std::string("from global")));
            return GlobalRef<jstring>::make(e, local.get());
          }
        )
        .commit();
  }
} // namespace expo::kolibri::tests
