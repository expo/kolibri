#include <kolibri/LocalFrame.h>

namespace expo::kolibri {
  LocalFrame::LocalFrame(JNIEnv* env, size_t size) : env_(env), ref_(nullptr) {
    env->PushLocalFrame(size);
  }

  LocalFrame::~LocalFrame() {
    env_->PopLocalFrame(ref_);
  }

  void LocalFrame::setReturn(const jobject ref) {
    ref_ = ref;
  }
}
