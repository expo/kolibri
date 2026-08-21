#pragma once

#include <concepts>
#include <jni.h>
#include <tuple>
#include <type_traits>
#include <utility>

#include <kolibri/NativeObject.h>
#include <kolibri/Ref.h>
#include <kolibri/defines.h>
#include <kolibri/exception.h>
#include <kolibri/jni_traits.h>
#include <kolibri/signature.h>
#include <kolibri/meta/traits.h>

namespace expo::kolibri {
  namespace detail {
    template<typename Func>
    ALWAYS_INLINE auto guardJni(JNIEnv* env, Func&& fn) -> std::invoke_result_t<Func> {
      using Return = std::invoke_result_t<Func>;
      try {
        return fn();
      } catch (const std::exception& e) {
        throwJavaRuntimeException(env, e.what());
        if constexpr (std::is_void_v<Return>) {
          return;
        } else {
          return Return{};
        }
      }
    }
  }

  template<bool Guard, typename Func>
  ALWAYS_INLINE auto runBound(JNIEnv* env, Func&& fn) -> std::invoke_result_t<Func> {
    if constexpr (Guard) {
      return detail::guardJni(env, std::forward<Func>(fn));
    } else {
      return fn();
    }
  }

  namespace detail {
    template<typename R>
    struct raw_return {
      using type = R;
    };

    template<concepts::BasedRef R>
    struct raw_return<R> {
    private:
      using Inner = std::remove_cvref_t<R>::InnerType;

    public:
      using type = std::conditional_t<is_jni_object_v<Inner>, Inner, jobject>;
    };

    template<typename R>
    using raw_return_t = raw_return<std::remove_cvref_t<R>>::type;

    template<typename R>
    ALWAYS_INLINE raw_return_t<R> lowerReturn(JNIEnv* env, R&& value) {
      using Value = std::remove_cvref_t<R>;
      using Raw = raw_return_t<R>;
      static_assert(
        !(concepts::OwnedRef<Value> && std::is_lvalue_reference_v<R>),
        "native methods must return owned refs (Ref/GlobalRef) by value"
      );

      if constexpr (concepts::UnownedRef<Value>) {
        return reinterpret_cast<Raw>(value.get());
      } else if constexpr (concepts::Ref<Value>) {
        return reinterpret_cast<Raw>(value.release());
      } else if constexpr (concepts::OwnedRef<Value>) {
        jobject raw = value.get();
        return reinterpret_cast<Raw>(raw ? env->NewLocalRef(raw) : nullptr);
      } else {
        return std::forward<R>(value);
      }
    }

    template<typename P, typename ArgsTuple>
    inline constexpr bool policyCallable = false;

    template<typename P, typename... Args>
    inline constexpr bool policyCallable<P, std::tuple<Args...>> = requires
    {
      {
        P::call(std::declval<JNIEnv*>(), std::declval<jobject>(), std::declval<Args>()...)
      } -> std::convertible_to<typename P::Return>;
    };
  } // namespace detail

  template<typename P>
  concept CallPolicy = requires
  {
    typename P::Return;
    typename P::Args;
    { P::RequireGuard } -> std::convertible_to<bool>;
  } && detail::policyCallable<P, typename P::Args>;

  template<CallPolicy Invoker, typename ArgsTuple = Invoker::Args>
  struct Binding;

  template<CallPolicy Invoker, typename... Args>
  struct Binding<Invoker, std::tuple<Args...>> {
  private:
    using Return = Invoker::Return;

    template<typename A>
    using RawArg = std::conditional_t<meta::is_pointer_of<A, JNativeObject>, jlong, A>;

    template<typename A>
    static constexpr A liftArg(JNIEnv*, RawArg<A> raw) {
      if constexpr (meta::is_pointer_of<A, JNativeObject>) {
        return reinterpret_cast<A>(raw);
      } else {
        return raw;
      }
    }

  public:
    static const char* jniSignature() {
      return JniSignatureFor<Return(RawArg<Args>...)>::value();
    }

    static detail::raw_return_t<Return> trampoline(JNIEnv* env, jobject self, RawArg<Args>... raw) {
      return kolibri::runBound<Invoker::RequireGuard>(
        env,
        [&] {
          if constexpr (std::is_void_v<Return>) {
            return Invoker::call(env, self, liftArg<Args>(env, raw)...);
          } else {
            return detail::lowerReturn(env, Invoker::call(env, self, liftArg<Args>(env, raw)...));
          }
        }
      );
    }
  };
}
