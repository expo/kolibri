#pragma once

#include <jni.h>

namespace expo::kolibri::tests {
  // clang-format off
  void registerRefTests(JNIEnv* env);
  void registerMemberTests(JNIEnv* env);
  void registerRegistrationTests(JNIEnv* env);
  void registerExceptionTests(JNIEnv* env);
  void registerStringTests(JNIEnv* env);
  void registerArrayTests(JNIEnv* env);
  void registerNativeObjectTests(JNIEnv* env);
  void registerScopedNativeObjectTests(JNIEnv* env);
  void registerBinaryCodecTests(JNIEnv* env);
  void registerEnvTests(JNIEnv* env);
  // clang-format on
}
