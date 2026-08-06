#pragma once

#include <jni.h>

namespace expo::kolibri {
  JNIEnv* onLoad(JavaVM* vm);
}
