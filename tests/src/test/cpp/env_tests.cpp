#include <stdexcept>
#include <string>
#include <thread>

#include <kolibri/env.h>
#include <kolibri/native_method.h>

#include "expect.h"
#include "test_helpers.h"

namespace expo::kolibri::tests {
  namespace {
    jboolean envOnJniThread(JNIEnv* env) {
      KOLIBRI_EXPECT(getEnv() == env);
      KOLIBRI_EXPECT(getEnvIfAttached() == env);
      return JNI_TRUE;
    }

    jboolean getEnvAttachesFreshThread(JNIEnv*) {
      std::string failure;
      std::thread worker([&] {
        try {
          KOLIBRI_EXPECT(getEnvIfAttached() == nullptr);

          JNIEnv* attached = getEnv();
          KOLIBRI_EXPECT(attached != nullptr);
          KOLIBRI_EXPECT(getEnv() == attached);
          KOLIBRI_EXPECT(getEnvIfAttached() == attached);
        } catch (const std::exception& e) {
          failure = e.what();
        }
      });
      worker.join();
      if (!failure.empty()) {
        throw std::runtime_error(failure);
      }
      return JNI_TRUE;
    }
  } // namespace

  void registerEnvTests(JNIEnv* env) {
    registerNative(env, "io/github/expo/kolibri/tests/EnvTests")
        .method<&envOnJniThread>("nativeEnvOnJniThread")
        .method<&getEnvAttachesFreshThread>("nativeGetEnvAttachesFreshThread")
        .commit();
  }
} // namespace expo::kolibri::tests
