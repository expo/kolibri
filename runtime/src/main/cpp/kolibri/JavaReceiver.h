#pragma once
#include <jni.h>

namespace expo::kolibri {
  struct JavaReceiver {
    jobject self;
    [[nodiscard]] jclass asClass() const { return reinterpret_cast<jclass>(self); }
  };
}
