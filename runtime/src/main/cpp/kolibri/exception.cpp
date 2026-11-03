#include <kolibri/exception.h>

#include <string>

#include <kolibri/Ref.h>
#include <kolibri/class.h>

namespace expo::kolibri {
  // Throwable and RuntimeException are always loadable, so their JavaClass lookups are assumed
  // never to fail here.

  void throwPending(JNIEnv* env) {
    const Ref<JThrowable> throwable = Ref<JThrowable>::adopt(env, env->ExceptionOccurred());
    env->ExceptionClear();

    std::string message = "Java exception";
    // Called on any throwable it dispatches to the override, so the throwable's own class never
    // needs looking up.
    const Ref<JString> text = throwable->toString(env);
    // The noexcept token leaves the check to us; if toString faulted, keep the generic message.
    if (env->ExceptionCheck()) {
      env->ExceptionClear();
    } else if (text) {
      message = text->toStdString(env);
    }
    throw JavaException(message);
  }

  void throwJavaRuntimeException(JNIEnv* env, const char* message) {
    if (env->ExceptionCheck()) {
      // A Java exception is already pending; let it propagate rather than masking it.
      return;
    }
    env->ThrowNew(JRuntimeException::javaClass(env), message);
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

  void throwWithPending(JNIEnv* env, const char* message) {
    throwWithPending(env, std::string(message));
  }
} // namespace expo::kolibri
