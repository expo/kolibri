#pragma once

#include <jni.h>
#include <type_traits>

#include <kolibri/meta/CTString.h>
#include <kolibri/env.h>
#include <kolibri/Ref.h>
#include <kolibri/utils.h>
#include <kolibri/members/Constructor.h>
#include <kolibri/members/Method.h>
#include <kolibri/members/StaticMethod.h>
#include <kolibri/members/Field.h>
#include <kolibri/members/StaticField.h>

namespace expo::kolibri {
  class NativeMethodsBuilder;

  namespace detail {
    template<typename _, typename = void>
    struct class_accessors {
      using type = ref_accessor_base;
    };

    template<typename Base>
    struct class_accessors<Base, std::void_t<typename Base::Accessors>> {
      using type = Base::Accessors;
    };

    template<typename Base>
    using class_accessors_t = class_accessors<Base>::type;
  }

  template<typename Derived, typename Base = void>
  struct JavaClass {
    struct BaseAccessors : detail::class_accessors_t<Base> {
      using Owner = Derived;
    };

    template<meta::CTString Name, typename Signature>
    using Method = Method<Derived, Name, Signature>;

    template<meta::CTString Name, typename Signature>
    using StaticMethod = StaticMethod<Derived, Name, Signature>;

    template<meta::CTString Name, typename T>
    using Field = Field<Derived, Name, T>;

    template<meta::CTString Name, typename T>
    using StaticField = StaticField<Derived, Name, T>;

    template<typename Signature>
    using Constructor = Constructor<Derived, Signature>;

    static NativeMethodsBuilder registerNatives(JNIEnv* env);

    static jclass javaClass(JNIEnv* env = getEnv()) {
      static const jclass clazz = reinterpret_cast<jclass>(
        findGlobalClass(env, Derived::descriptor).release()
      );
      return clazz;
    }
  };
}
