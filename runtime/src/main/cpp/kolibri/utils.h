#pragma once

#include <jni.h>
#include <span>
#include <string>
#include <string_view>

#include <kolibri/Ref.h>

namespace expo::kolibri {
  void registerNative(
    JNIEnv* env,
    jclass clazz,
    std::span<const JNINativeMethod> methods
  );

  template<concepts::BasedRef RefT>
  void registerNative(
    JNIEnv* env,
    const RefT& clazz,
    const std::span<const JNINativeMethod> methods
  ) {
    registerNative(env, reinterpret_cast<jclass>(clazz.get()), methods);
  }

  Ref<> registerNative(
    JNIEnv* env,
    std::string_view descriptor,
    std::span<const JNINativeMethod> methods
  );

  Ref<> findClass(JNIEnv* env, std::string_view descriptor);

  GlobalRef<> findGlobalClass(JNIEnv* env, std::string_view descriptor);

  /** The class's Java name (e.g. `java.lang.String`), for readable error messages. */
  std::string describeClass(JNIEnv* env, jclass clazz);
} // namespace expo::kolibri
