#include <string>

#include <kolibri/JavaClass.h>
#include <kolibri/exception.h>
#include <kolibri/native_method.h>
#include <kolibri/string_utils.h>

#include "expect.h"
#include "test_helpers.h"

namespace expo::kolibri::tests {
  namespace {
    struct JExceptionTests : JavaClass<JExceptionTests> {
      static constexpr std::string_view descriptor = "io/github/expo/kolibri/tests/ExceptionTests";
      static constexpr StaticMethod<"throwingCallback", void()> throwingCallback{};
    };

    void throwJavaExceptionImpl(JNIEnv* env, jstring message) {
      throw JavaException(toStdString(env, message));
    }

    void throwRuntimeDirect(JNIEnv* env, jstring message) noexcept {
      const std::string text = toStdString(env, message);
      throwJavaRuntimeException(env, text.c_str());
    }

    jboolean rethrowFromCallback(JNIEnv* env) {
      try {
        // The non-noexcept token checks the pending state and rethrows it as a C++ JavaException.
        JExceptionTests::throwingCallback(env);
      } catch (const JavaException& e) {
        const std::string message = e.what();
        KOLIBRI_EXPECT(message.find("IllegalArgumentException") != std::string::npos);
        KOLIBRI_EXPECT(message.find("callback failed") != std::string::npos);
        // throwPending cleared the pending state; the JVM is usable again.
        KOLIBRI_EXPECT(!env->ExceptionCheck());
        return JNI_TRUE;
      }
      KOLIBRI_EXPECT(false);
      return JNI_FALSE;
    }

    void propagateFromCallback(JNIEnv* env) {
      JExceptionTests::throwingCallback(env);
    }

    void throwWithPendingImpl(JNIEnv* env, jstring context) {
      const std::string text = toStdString(env, context);
      throwJavaRuntimeException(env, "pending failure");
      throwWithPending(env, text);
    }
  } // namespace

  void registerExceptionTests(JNIEnv* env) {
    registerNative(env, "io/github/expo/kolibri/tests/ExceptionTests")
      .method<&throwJavaExceptionImpl>("nativeThrowJavaException")
      .method<&throwRuntimeDirect>("nativeThrowRuntimeDirect")
      .method<&rethrowFromCallback>("nativeRethrowFromCallback")
      .method<&propagateFromCallback>("nativePropagateFromCallback")
      .method<&throwWithPendingImpl>("nativeThrowWithPending")
      .commit();
  }
} // namespace expo::kolibri::tests
