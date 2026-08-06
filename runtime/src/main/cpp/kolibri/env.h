#pragma once

#include <jni.h>

namespace expo::kolibri {
  /** Caches the `JavaVM*`. Call once from `JNI_OnLoad`. */
  void initVM(JavaVM* vm);

  /**
   * Returns the `JNIEnv*` for the current thread, attaching it to the VM (as a daemon thread, so
   * it never blocks JVM shutdown) if necessary. The bridge is driven from the JS thread (already
   * attached); the attach path exists for the odd native destructor that may run elsewhere.
   */
  JNIEnv* getEnv();

  /**
   * Returns the `JNIEnv*` for the current thread only if it is already attached, else nullptr —
   * never attaches. For code that may run during thread teardown (e.g. `thread_local`
   * destructors), where the JVM may have already detached the thread and re-attaching crashes.
   */
  JNIEnv* getEnvIfAttached();
} // namespace expo::kolibri
