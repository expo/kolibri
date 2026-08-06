#include <kolibri/env.h>

#include <pthread.h>

namespace expo::kolibri {
  namespace {
    JavaVM* gVm = nullptr;

    thread_local JNIEnv* tCachedEnv = nullptr;

    pthread_key_t gDetachKey;
    pthread_once_t gDetachKeyOnce = PTHREAD_ONCE_INIT;

    void detachThread(void*) {
      tCachedEnv = nullptr;
      gVm->DetachCurrentThread();
    }

    void makeDetachKey() { pthread_key_create(&gDetachKey, detachThread); }
  }

  void initVM(JavaVM* vm) { gVm = vm; }

  JNIEnv* getEnv() {
    if (tCachedEnv != nullptr) {
      return tCachedEnv;
    }

    JNIEnv* env = nullptr;
    const jint status = gVm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6);
    if (status == JNI_EDETACHED) {
      // Daemon attach: threads reaching this path are foreign background threads (GC executors,
      // JS thread pools) that live for the whole process — a non-daemon attach would make
      // DestroyJavaVM wait on them forever, hanging JVM shutdown.
#ifdef __ANDROID__
      gVm->AttachCurrentThreadAsDaemon(&env, nullptr);
#else
      gVm->AttachCurrentThreadAsDaemon(reinterpret_cast<void**>(&env), nullptr);
#endif

      pthread_once(&gDetachKeyOnce, makeDetachKey);
      pthread_setspecific(gDetachKey, reinterpret_cast<void*>(1));
    }

    tCachedEnv = env;
    return env;
  }

  JNIEnv* getEnvIfAttached() {
    if (tCachedEnv != nullptr) {
      return tCachedEnv;
    }

    JNIEnv* env = nullptr;
    if (gVm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK) {
      return nullptr;
    }
    return env;
  }
}
