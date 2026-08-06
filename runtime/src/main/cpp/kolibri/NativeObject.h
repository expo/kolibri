#pragma once

#include <jni.h>
#include <string_view>
#include <type_traits>

#include <kolibri/JavaClass.h>

namespace expo::kolibri {
  struct JNativeObject {
    static constexpr std::string_view descriptor = "io/github/expo/kolibri/NativeObject";

    virtual ~JNativeObject() = default;

    static void registerNatives(JNIEnv* env);

    template<typename T>
    static T* nativeThis(JNIEnv* env, jobject handle) {
      static_assert(
        std::is_base_of_v<JNativeObject, T>,
        "nativeThis<T> requires a NativeObject subclass"
      );
      return reinterpret_cast<T*>(env->GetLongField(handle, sPointerField_));
    }

  private:
    static jfieldID sPointerField_;
  };

  template<typename Derived>
  struct NativeObject : JNativeObject, JavaClass<Derived> {
  };
} // namespace expo::kolibri
