#pragma once

#include <jni.h>
#include <utility>

#include <kolibri/Token.h>
#include <kolibri/java_invoker.h>

namespace expo::kolibri {
  template<typename Class, meta::CTString Name, typename T>
  struct StaticField : FieldToken<Class, Name, T> {
    static jfieldID id(JNIEnv* env) {
      static jfieldID fieldId = getStaticFieldId<StaticField>(env);
      return fieldId;
    }

    T operator()(JNIEnv* env) const {
      return kolibri::getStatic(env, *this);
    }

    template<typename NewValue>
    void set(JNIEnv* env, NewValue&& value) const {
      kolibri::setStatic(env, *this, std::forward<NewValue>(value));
    }
  };
}
