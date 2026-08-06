#pragma once

#include <jni.h>

namespace expo::kolibri {
  class LocalFrame {
  public:
    explicit LocalFrame(JNIEnv* env, size_t size);

    ~LocalFrame();

    void setReturn(jobject ref);

  private:
    JNIEnv* env_;
    jobject ref_;
  };
}
