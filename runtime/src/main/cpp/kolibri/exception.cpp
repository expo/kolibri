#include <kolibri/exception.h>

#include <string>

#include <kolibri/Ref.h>
#include <kolibri/string_utils.h>
#include <kolibri/utils.h>

namespace expo::kolibri {
  void throwPending(JNIEnv* env) {
    const Ref<> throwable = Ref<>::adopt(env, env->ExceptionOccurred());
    env->ExceptionClear();

    std::string message = "Java exception";
    const Ref<> throwableClass =
        Ref<>::adopt(env, env->GetObjectClass(throwable));
    jmethodID toString = env->GetMethodID(
      reinterpret_cast<jclass>(throwableClass.get()),
      "toString",
      "()Ljava/lang/String;"
    );
    if (toString != nullptr) {
      // CallObjectMethod is variadic, so the receiver must be the raw jobject.
      const Ref<> text =
          Ref<>::adopt(env, env->CallObjectMethod(throwable.get(), toString));
      // CallObjectMethod can itself fault; if so, fall back to the generic message.
      if (env->ExceptionCheck()) {
        env->ExceptionClear();
      } else if (text) {
        message = toStdString(env, reinterpret_cast<jstring>(text.get()));
      }
    }
    throw JavaException(message);
  }

  void throwJavaRuntimeException(JNIEnv* env, const char* message) {
    if (env->ExceptionCheck()) {
      // A Java exception is already pending; let it propagate rather than masking it.
      return;
    }
    const Ref<> cls =
        Ref<>::adopt(env, env->FindClass("java/lang/RuntimeException"));
    if (cls) {
      env->ThrowNew(reinterpret_cast<jclass>(cls.get()), message);
    }
  }

  void throwWithPending(JNIEnv* env, const std::string& message) {
    if (env->ExceptionCheck()) {
      try {
        throwPending(env);
      } catch (const std::exception& e) {
        throw std::runtime_error(message + ": " + e.what());
      }
    }
    throw std::runtime_error(message);
  }
} // namespace expo::kolibri
