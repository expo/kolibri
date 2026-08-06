#pragma once

#include <jni.h>
#include <stdexcept>
#include <string>

namespace expo::kolibri {
  class JavaException : public std::runtime_error {
  public:
    explicit JavaException(const std::string& message) : std::runtime_error(message) {
    }
  };

  void throwPending(JNIEnv* env);

  inline void checkAndThrowPending(JNIEnv* env) {
    if (!env->ExceptionCheck()) [[likely]] {
      return;
    }
    throwPending(env);
  }

  void throwJavaRuntimeException(JNIEnv* env, const char* message);

  [[noreturn]] void throwWithPending(JNIEnv* env, const std::string& message);
}
