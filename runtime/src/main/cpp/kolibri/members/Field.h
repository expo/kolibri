#pragma once

#include <jni.h>
#include <utility>

#include <kolibri/meta/CTString.h>
#include <kolibri/Token.h>
#include <kolibri/java_invoker.h>
#include <kolibri/unwrap_ref.h>

namespace expo::kolibri {
  template<typename Class, meta::CTString Name, typename T>
  struct Field : FieldToken<Class, Name, T> {
    static jfieldID id(JNIEnv* env) {
      static jfieldID fieldId = getFieldId<Field>(env);
      return fieldId;
    }

    template<typename Receiver>
    T operator()(JNIEnv* env, Receiver&& receiver) const {
      return kolibri::get(env, unwrapRef(std::forward<Receiver>(receiver)), *this);
    }


    template<typename Receiver, typename NewValue>
    void set(JNIEnv* env, Receiver&& receiver, NewValue&& value) const {
      kolibri::set(
        env,
        unwrapRef(std::forward<Receiver>(receiver)),
        *this,
        std::forward<NewValue>(value)
      );
    }
  };
}
