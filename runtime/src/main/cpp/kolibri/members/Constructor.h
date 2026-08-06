#pragma once

#include <jni.h>

#include <kolibri/Token.h>
#include <kolibri/java_invoker.h>
#include <kolibri/Ref.h>

namespace expo::kolibri {
  template<typename Class, typename Signature>
  struct Constructor;

  template<typename Class, typename... Args>
  struct Constructor<Class, void(Args...)>
      : FunctionToken<Class, "<init>", void, Args...> {
    static jmethodID id(JNIEnv* env) {
      static jmethodID methodId = getMethodId<Constructor>(env);
      return methodId;
    }

    template<typename... CallArgs>
    Ref<Class> operator()(JNIEnv* env, CallArgs&&... args) const {
      return kolibri::newObject(env, *this, std::forward<CallArgs>(args)...);
    }

    Ref<Class> operator()(JNIEnv* env, Args... args) const {
      return kolibri::newObject(env, *this, args...);
    }

    template<typename... CallArgs>
    jobject createRaw(JNIEnv* env, CallArgs&&... args) const {
      return kolibri::newRawObject(env, *this, std::forward<CallArgs>(args)...);
    }

    jobject createRaw(JNIEnv* env, Args... args) const {
      return kolibri::newRawObject(env, *this, args...);
    }
  };
}
