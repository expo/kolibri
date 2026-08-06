#include <kolibri/NativeObject.h>

#include <kolibri/Ref.h>
#include <kolibri/native_method.h>

namespace expo::kolibri {
  jfieldID JNativeObject::sPointerField_ = nullptr;

  void JNativeObject::registerNatives(JNIEnv* env) {
    const Ref<> clazz = registerNative<JNativeObject>(env)
      .method("nativeDestroy", [](jlong pointer) noexcept -> void {
        delete reinterpret_cast<JNativeObject*>(pointer);
      })
      .commit();

    sPointerField_ = env->GetFieldID(reinterpret_cast<jclass>(clazz.get()), "nativePointer", "J");
  }
} // namespace expo::kolibri
