#pragma once

#include <jni.h>

#include <kolibri/Token.h>
#include <kolibri/java_invoker.h>

namespace expo::kolibri {
  template<typename Class, meta::CTString Name, typename R, bool NoThrow, typename... Args>
  struct StaticMethodBase : FunctionToken<Class, Name, R, Args...> {
    static constexpr bool noThrow = NoThrow;

    static jmethodID id(JNIEnv* env) {
      static jmethodID methodId = getStaticMethodId<StaticMethodBase>(env);
      return methodId;
    }

    template<typename... CallArgs>
    R operator()(JNIEnv* env, CallArgs&&... args) const {
      return kolibri::callStatic(env, *this, std::forward<CallArgs>(args)...);
    }

    R operator()(JNIEnv* env, Args... args) const {
      return kolibri::callStatic(env, *this, args...);
    }
  };

  // clang-format off
  template<typename Class, meta::CTString Name, typename Signature>           struct StaticMethod;
  template<typename Class, meta::CTString Name, typename R, typename... Args> struct StaticMethod<Class, Name, R(Args...)>          : StaticMethodBase<Class, Name, R, false, Args...> {};
  template<typename Class, meta::CTString Name, typename R, typename... Args> struct StaticMethod<Class, Name, R(Args...) noexcept> : StaticMethodBase<Class, Name, R, true, Args...> {};
  // clang-format on
} // namespace expo::kolibri
