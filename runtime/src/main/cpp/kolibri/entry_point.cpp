#include "entry_point.h"

#include "env.h"
#include "NativeObject.h"
#include "binary/BinaryBuffer.h"

namespace expo::kolibri {
  JNIEnv* onLoad(JavaVM* vm) {
    initVM(vm);
    JNIEnv* env = getEnv();
    JNativeObject::registerNatives(env);
    binary::BinaryBuffer::registerNatives(env);
    return env;
  }
}
