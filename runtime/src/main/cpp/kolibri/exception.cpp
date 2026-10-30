#include <kolibri/exception.h>

#include <atomic>
#include <string>

#include <kolibri/Ref.h>
#include <kolibri/string_utils.h>
#include <kolibri/utils.h>

namespace expo::kolibri {
  namespace {
    // The lookups below run while one exception is being turned into another, so they never
    // throw: a failed one returns null with the VM's own exception pending, and is retried on
    // the next call. A success is cached for the process.

    // Throwable.toString(). Called on any throwable it dispatches to the override, so the
    // throwable's own class never needs looking up. Throwable is never unloaded, so the ID
    // stays valid without holding the class.
    jmethodID throwableToString(JNIEnv* env) {
      static constinit std::atomic<jmethodID> slot{nullptr};
      jmethodID id = slot.load(std::memory_order_acquire);
      if (id == nullptr) [[unlikely]] {
        const Ref<> throwable = Ref<>::adopt(env, env->FindClass("java/lang/Throwable"));
        if (!throwable) {
          return nullptr;
        }
        id = env->GetMethodID(
          reinterpret_cast<jclass>(throwable.get()),
          "toString",
          "()Ljava/lang/String;"
        );
        slot.store(id, std::memory_order_release);
      }
      return id;
    }

    // Held by a global ref for the process; two threads racing here keep the first one stored.
    jclass runtimeExceptionClass(JNIEnv* env) {
      static constinit std::atomic<jclass> slot{nullptr};
      jclass clazz = slot.load(std::memory_order_acquire);
      if (clazz != nullptr) [[likely]] {
        return clazz;
      }
      const Ref<> local = Ref<>::adopt(env, env->FindClass("java/lang/RuntimeException"));
      if (!local) {
        return nullptr;
      }
      const auto created = reinterpret_cast<jclass>(env->NewGlobalRef(local.get()));
      if (created == nullptr) {
        return nullptr;
      }
      if (slot.compare_exchange_strong(clazz, created, std::memory_order_acq_rel)) {
        return created;
      }
      env->DeleteGlobalRef(created);
      return clazz;
    }
  } // namespace

  void throwPending(JNIEnv* env) {
    const Ref<> throwable = Ref<>::adopt(env, env->ExceptionOccurred());
    env->ExceptionClear();

    std::string message = "Java exception";
    const jmethodID toString = throwableToString(env);
    if (toString == nullptr) {
      // The lookup failed with its own exception pending; the generic message has to do.
      env->ExceptionClear();
    } else {
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
    // If the lookup fails, its own exception (e.g. an OutOfMemoryError) is what propagates.
    const jclass cls = runtimeExceptionClass(env);
    if (cls != nullptr) {
      env->ThrowNew(cls, message);
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

  void throwWithPending(JNIEnv* env, const char* message) {
    throwWithPending(env, std::string(message));
  }
} // namespace expo::kolibri
