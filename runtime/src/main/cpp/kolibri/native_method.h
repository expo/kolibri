#pragma once

#include <jni.h>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include <kolibri/NativeObject.h>
#include <kolibri/defines.h>
#include <kolibri/meta/func_traits.h>
#include <kolibri/JavaReceiver.h>
#include <kolibri/native_method_invoker.h>
#include <kolibri/meta/traits.h>
#include <kolibri/utils.h>

namespace expo::kolibri {
  namespace detail {
    template<bool RequireEnv, bool RequireSelf, typename Target, typename... A>
    decltype(auto) callFree(JNIEnv* env, const jobject self, Target target, A&&... a) {
      if constexpr (RequireEnv && RequireSelf) {
        return target(env, JavaReceiver{self}, std::forward<A>(a)...);
      } else if constexpr (RequireEnv) {
        return target(env, std::forward<A>(a)...);
      } else if constexpr (RequireSelf) {
        return target(JavaReceiver{self}, std::forward<A>(a)...);
      } else {
        return target(std::forward<A>(a)...);
      }
    }

    template<auto Func>
    struct CallFunction {
      using func_traits = meta::func_traits<decltype(Func)>;

      using Return = func_traits::return_type;
      using Args = meta::remove_by_types_t<typename func_traits::params, JNIEnv*, JavaReceiver>;

      static constexpr bool RequireGuard = !func_traits::is_noexcept;
      static constexpr bool RequireEnv = func_traits::template nth_arg_is<0, JNIEnv*>();
      static constexpr bool RequireSelf =
          func_traits::template nth_arg_is<RequireEnv ? 1 : 0, JavaReceiver>();

      template<typename... A>
      ALWAYS_INLINE static decltype(auto) call(JNIEnv* env, jobject self, A&&... a) {
        return callFree<RequireEnv, RequireSelf>(env, self, Func, std::forward<A>(a)...);
      }
    };

    template<typename Lambda>
    struct CallLambda {
      using func_traits = meta::func_traits<Lambda>;

      using Return = func_traits::return_type;
      using Args = meta::remove_by_types_t<typename func_traits::params, JNIEnv*, JavaReceiver>;

      static constexpr bool RequireGuard = !func_traits::is_noexcept;
      static constexpr bool RequireEnv = func_traits::template nth_arg_is<0, JNIEnv*>();
      static constexpr bool RequireSelf =
          func_traits::template nth_arg_is<RequireEnv ? 1 : 0, JavaReceiver>();

      template<typename... A>
      ALWAYS_INLINE static decltype(auto) call(JNIEnv* env, jobject self, A&&... a) {
        return callFree<RequireEnv, RequireSelf>(env, self, Lambda{}, std::forward<A>(a)...);
      }
    };

    template<auto Method>
    struct CallMember {
      using func_traits = meta::func_traits<decltype(Method)>;

      using ClassType = func_traits::class_type;
      using Return = func_traits::return_type;
      using Args =
      meta::prepend_t<ClassType*, meta::remove_by_type_t<JNIEnv*, typename func_traits::params>>;

      static_assert(
        std::is_base_of_v<JNativeObject, ClassType>,
        "makeNativeMethod member target must be a member of a NativeObject subclass"
      );

      static constexpr bool RequireGuard = !func_traits::is_noexcept;
      static constexpr bool RequireEnv = func_traits::template nth_arg_is<0, JNIEnv*>();

      template<typename... A>
      ALWAYS_INLINE static decltype(auto) call(JNIEnv* env, jobject, ClassType* self, A&&... a) {
        if constexpr (RequireEnv) {
          return (self->*Method)(env, std::forward<A>(a)...);
        } else {
          return (self->*Method)(std::forward<A>(a)...);
        }
      }
    };

    template<auto Method>
    using MemberMethod = Binding<CallMember<Method>>;
    template<auto Fn>
    using FunctionMethod = Binding<CallFunction<Fn>>;
    template<typename Lambda>
    using LambdaMethod = Binding<CallLambda<Lambda>>;

    template<auto Callable>
    constexpr auto selectBinding() {
      using T = decltype(Callable);
      if constexpr (std::is_member_function_pointer_v<T>) {
        return std::type_identity<MemberMethod<Callable>>{};
      } else if constexpr (std::is_pointer_v<T>) {
        static_assert(
          std::is_function_v<std::remove_pointer_t<T>>,
          "makeNativeMethod pointer target must be a function pointer."
        );
        return std::type_identity<FunctionMethod<Callable>>{};
      } else {
        static_assert(
          std::is_empty_v<T>,
          "makeNativeMethod requires a member function of a NativeObject subclass, a plain "
          "function pointer, or a captureless lambda (a capturing one has state and cannot "
          "be re-materialised inside the JNI trampoline)."
        );
        return std::type_identity<LambdaMethod<T>>{};
      }
    }

    template<auto Callable>
    using BindingFor = decltype(selectBinding<Callable>())::type;

    template<typename Func>
    JNINativeMethod makeNativeMethod(const char* name, const char* signature, Func* fn) {
      return JNINativeMethod{
        const_cast<char*>(name),
        const_cast<char*>(signature),
        reinterpret_cast<void*>(fn)
      };
    }

    template<auto Callable>
    JNINativeMethod makeNativeMethod(const char* name, const char* signature) {
      using Binding = BindingFor<Callable>;
      return makeNativeMethod(name, signature, &Binding::trampoline);
    }
  } // namespace detail

  template<auto Callable>
  JNINativeMethod makeNativeMethod(const char* name) {
    return detail::makeNativeMethod<Callable>(name, detail::BindingFor<Callable>::jniSignature());
  }

  template<auto Callable, typename Signature> requires std::is_function_v<Signature>
  JNINativeMethod makeNativeMethod(const char* name) {
    return detail::makeNativeMethod<Callable>(name, JniSignatureFor<Signature>::value());
  }

  template<auto Callable>
  JNINativeMethod makeNativeMethod(const char* name, const char* signature) {
    return detail::makeNativeMethod<Callable>(name, signature);
  }

  template<typename Lambda>
  JNINativeMethod makeNativeMethod(const char* name, Lambda) {
    static_assert(
      std::is_empty_v<Lambda>,
      "makeNativeMethod requires a captureless lambda (a capturing one has state and "
      "cannot be re-materialised inside the JNI trampoline)."
    );

    using Binding = detail::LambdaMethod<Lambda>;
    return detail::makeNativeMethod(name, Binding::jniSignature(), &Binding::trampoline);
  }

  template<typename Lambda, std::enable_if_t<!std::is_pointer_v<Lambda>, int> = 0>
  JNINativeMethod makeNativeMethod(const char* name, const char* signature, Lambda) {
    static_assert(
      std::is_empty_v<Lambda>,
      "makeNativeMethod requires a captureless lambda (a capturing one has state and "
      "cannot be re-materialised inside the JNI trampoline)."
    );

    return detail::makeNativeMethod(name, signature, &detail::LambdaMethod<Lambda>::trampoline);
  }

  class NativeMethodsBuilder {
  public:
    NativeMethodsBuilder(JNIEnv* env, const std::string_view descriptor)
      : env_(env), descriptor_(descriptor) {
    }

    // clang-format off
    NativeMethodsBuilder(const NativeMethodsBuilder&) = delete;
    NativeMethodsBuilder& operator=(const NativeMethodsBuilder&) = delete;
    NativeMethodsBuilder(NativeMethodsBuilder&&) = default;
    NativeMethodsBuilder& operator=(NativeMethodsBuilder&&) = default;
    // clang-format on

    template<auto Callable>
    NativeMethodsBuilder& method(const char* name) {
      methods_.push_back(makeNativeMethod<Callable>(name));
      return *this;
    }

    template<auto Callable, typename Signature> requires std::is_function_v<Signature>
    NativeMethodsBuilder& method(const char* name) {
      methods_.push_back(makeNativeMethod<Callable, Signature>(name));
      return *this;
    }

    template<auto Callable>
    NativeMethodsBuilder& method(const char* name, const char* signature) {
      methods_.push_back(makeNativeMethod<Callable>(name, signature));
      return *this;
    }

    template<typename Lambda>
    NativeMethodsBuilder& method(const char* name, Lambda lambda) {
      methods_.push_back(makeNativeMethod(name, lambda));
      return *this;
    }

    template<typename Lambda>
    NativeMethodsBuilder& method(const char* name, const char* signature, Lambda lambda) {
      methods_.push_back(makeNativeMethod(name, signature, lambda));
      return *this;
    }

    Ref<> commit() {
      return registerNative(
        env_,
        descriptor_,
        std::span<const JNINativeMethod>{methods_}
      );
    }

  private:
    JNIEnv* env_;
    std::string_view descriptor_;
    std::vector<JNINativeMethod> methods_;
  };

  inline NativeMethodsBuilder registerNative(
    JNIEnv* env,
    const std::string_view descriptor
  ) {
    return NativeMethodsBuilder{env, descriptor};
  }

  template<typename T> requires requires { std::string_view{T::descriptor}; }
  NativeMethodsBuilder registerNative(JNIEnv* env) {
    return registerNative(env, std::string_view{T::descriptor});
  }

  template<typename Derived, typename Base>
  NativeMethodsBuilder JavaClass<Derived, Base>::registerNatives(JNIEnv* env) {
    return registerNative<Derived>(env);
  }

  typedef jlong NativePointer;
} // namespace expo::kolibri
