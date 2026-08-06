#pragma once

#include <jni.h>

#include <kolibri/Token.h>
#include <kolibri/java_invoker.h>
#include <kolibri/unwrap_ref.h>

namespace expo::kolibri {
  template<typename Class, meta::CTString Name, typename R, bool NoThrow, typename... Args>
  struct MethodBase : FunctionToken<Class, Name, R, Args...> {
    static constexpr bool noThrow = NoThrow;

    static jmethodID id(JNIEnv* env) {
      static jmethodID methodId = getMethodId<MethodBase>(env);
      return methodId;
    }

    template<typename Receiver, typename... CallArgs>
    R operator()(JNIEnv* env, Receiver&& receiver, CallArgs&&... args) const {
      return kolibri::call(
        env,
        unwrapRef(std::forward<Receiver>(receiver)),
        *this,
        std::forward<CallArgs>(args)...
      );
    }

    R operator()(JNIEnv* env, jobject receiver, Args... args) const {
      return kolibri::call(env, receiver, *this, args...);
    }
  };

  // clang-format off
  template<typename Class, meta::CTString Name, typename Signature>           struct Method;
  template<typename Class, meta::CTString Name, typename R, typename... Args> struct Method<Class, Name, R(Args...)>          : MethodBase<Class, Name, R, false, Args...> {};
  template<typename Class, meta::CTString Name, typename R, typename... Args> struct Method<Class, Name, R(Args...) noexcept> : MethodBase<Class, Name, R, true, Args...> {};
  // clang-format on
}
