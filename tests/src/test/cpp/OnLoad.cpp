#include <jni.h>

#include <kolibri/entry_point.h>

#include "test_helpers.h"

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void*) {
  using namespace expo::kolibri;
  JNIEnv* env = onLoad(vm);

  tests::registerRefTests(env);
  tests::registerMemberTests(env);
  tests::registerRegistrationTests(env);
  tests::registerExceptionTests(env);
  tests::registerStringTests(env);
  tests::registerArrayTests(env);
  tests::registerNativeObjectTests(env);
  tests::registerScopedNativeObjectTests(env);
  tests::registerBinaryCodecTests(env);
  tests::registerEnvTests(env);
  return JNI_VERSION_1_6;
}
