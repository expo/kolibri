#include <kolibri/utils.h>

#include <kolibri/exception.h>
#include <kolibri/string_utils.h>

namespace expo::kolibri {
  void registerNative(
    JNIEnv* env,
    const jclass clazz,
    const std::span<const JNINativeMethod> methods
  ) {
    const jint result = env->RegisterNatives(
      clazz,
      methods.data(),
      static_cast<jint>(methods.size())
    );

    if (result != JNI_OK) {
      checkAndThrowPending(env);
      throw std::runtime_error("RegisterNatives failed");
    }
  }

  Ref<> registerNative(
    JNIEnv* env,
    const std::string_view descriptor,
    const std::span<const JNINativeMethod> methods
  ) {
    auto clazz = findClass(env, descriptor);
    registerNative(env, clazz, methods);
    return clazz;
  }

  Ref<> findClass(JNIEnv* env, const std::string_view descriptor) {
    const auto className = std::string(descriptor);
    const auto local = env->FindClass(className.c_str());
    if (local == nullptr) {
      throwWithPending(env, "Could not find Java class " + className);
    }
    return Ref<>::adopt(env, local);
  }

  std::string describeClass(JNIEnv* env, jclass clazz) {
    const Ref<> classClass = Ref<>::adopt(env, env->GetObjectClass(clazz));
    jmethodID getName = env->GetMethodID(
      reinterpret_cast<jclass>(classClass.get()),
      "getName",
      "()Ljava/lang/String;"
    );
    // CallObjectMethod is variadic, so the receiver must be the raw jobject, not the wrapper.
    const Ref<> name = Ref<>::adopt(env, env->CallObjectMethod(clazz, getName));
    return toStdString(env, reinterpret_cast<jstring>(name.get()));
  }

  GlobalRef<> findGlobalClass(JNIEnv* env, const std::string_view descriptor) {
    const auto local = findClass(env, descriptor);
    auto global = GlobalRef<>::make(env, local.get());
    if (!global) {
      throwWithPending(
        env,
        "Could not create global reference for Java class " + std::string(descriptor)
      );
    }
    return global;
  }
} // namespace expo::kolibri
