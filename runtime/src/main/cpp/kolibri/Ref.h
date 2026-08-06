#pragma once

#include <cstdio>
#include <jni.h>
#include <string>
#include <type_traits>
#include <utility>

#include <kolibri/env.h>
#include <kolibri/jni_traits.h>
#include <kolibri/string_utils.h>

namespace expo::kolibri {
  struct JNativeObject;

  template<typename T>
  struct UnownedRef;

  namespace detail {
    template<typename T>
    concept RefTarget = is_jni_object_v<T> || std::is_class_v<T>;

    struct LocalRefTraits {
      /**
       * Whether it will be removed from the same thread as it was created.
       */
      static constexpr bool threadConfined = true;

      static jobject newRef(JNIEnv* env, jobject jobj) {
        return jobj ? env->NewLocalRef(jobj) : nullptr;
      }

      static void deleteRef(JNIEnv* env, jobject jobj) { env->DeleteLocalRef(jobj); }
    };

    struct GlobalRefTraits {
      static constexpr bool threadConfined = false;

      static jobject newRef(JNIEnv* env, jobject jobj) {
        return jobj ? env->NewGlobalRef(jobj) : nullptr;
      }

      static void deleteRef(JNIEnv* env, jobject jobj) { env->DeleteGlobalRef(jobj); }
    };

    struct ref_accessor_base {
      jobject handle_;

    protected:
      // Forwards to a class token with the bound object inserted as the receiver, threading the
      // caller-supplied env: `it->next(env)` -> `JIterator::next(env, handle_)`.
      template<typename Token, typename... A>
      decltype(auto) callToken(JNIEnv* env, Token token, A&&... a) const {
        return token(env, handle_, std::forward<A>(a)...);
      }
    };

    template<typename T>
    struct ref_accessor_view : T::Accessors {
      explicit ref_accessor_view(jobject handle) : T::Accessors{handle} {
      }

      const ref_accessor_view* operator->() const {
        return this;
      }
    };

    template<typename T>
    concept has_ref_accessors = requires { typename T::Accessors; };

    template<typename Derived, typename T>
    class BasedRef {
      [[nodiscard]] jobject handle() const { return static_cast<const Derived*>(this)->get(); }

    public:
      using InnerType = T;

      ref_accessor_view<T> operator->() const requires has_ref_accessors<T> {
        return ref_accessor_view<T>(handle());
      }

      T* native(JNIEnv* env) const
        requires std::is_base_of_v<JNativeObject, T> {
        return T::template nativeThis<T>(env, handle());
      }

      template<RefTarget U>
      [[nodiscard]] UnownedRef<U> staticCast() const {
        return UnownedRef<U>(handle());
      }

      [[nodiscard]] std::string toStdString(JNIEnv* env) const
        requires std::is_same_v<T, jstring> {
        return kolibri::toStdString(env, static_cast<jstring>(handle()));
      }
    };
  }

  template<typename Traits, typename T = void>
  class OwnedRef : public detail::BasedRef<OwnedRef<Traits, T>, T> {
  public:
    static constexpr bool IsLocalRef = Traits::threadConfined;

    OwnedRef() = default;

    ~OwnedRef() {
      if (ref_) {
        Traits::deleteRef(deleteEnv(), ref_);
      }
    }

    static OwnedRef adopt(JNIEnv* env, jobject existing) {
      return OwnedRef(env, existing);
    }

    static OwnedRef make(JNIEnv* env, jobject borrowed) {
      return OwnedRef(env, Traits::newRef(env, borrowed));
    }

    OwnedRef(OwnedRef&& other) noexcept : env_(other.env_), ref_(other.ref_) {
      other.ref_ = nullptr;
    }

    OwnedRef& operator=(OwnedRef&& other) noexcept {
      if (this != &other) {
        reset();
        env_ = other.env_;
        ref_ = other.ref_;
        other.ref_ = nullptr;
      }
      return *this;
    }

    OwnedRef(const OwnedRef&) = delete;

    OwnedRef& operator=(const OwnedRef&) = delete;

    [[nodiscard]] jobject get() const { return ref_; }
    operator jobject() const { return ref_; }
    explicit operator bool() const { return ref_ != nullptr; }

    [[nodiscard]] jobject release() {
      jobject r = ref_;
      ref_ = nullptr;
      return r;
    }

    void reset() {
      if (ref_) {
        Traits::deleteRef(deleteEnv(), ref_);
        ref_ = nullptr;
      }
    }

    OwnedRef clone() const { return make(liveEnv(), ref_); }

    using detail::BasedRef<OwnedRef, T>::staticCast;

    template<detail::RefTarget U>
    [[nodiscard]] OwnedRef<Traits, U> staticCast() && {
      return OwnedRef<Traits, U>::adopt(env_, release());
    }

  private:
    explicit OwnedRef(JNIEnv* env, jobject adopted) : env_(env), ref_(adopted) {
    }

    /** Env for creating refs: attaches the thread if needed, never null. */
    JNIEnv* liveEnv() const {
      if constexpr (Traits::threadConfined) {
        return env_;
      } else {
        return getEnv();
      }
    }

    /**
     * Env for releasing refs. A global ref released after its thread was detached from the JVM
     * (typically a thread_local destructor running during thread teardown) is a lifecycle bug:
     * the owner must be released explicitly while the thread is still attached. Aborts loudly —
     * re-attaching mid-teardown crashes inside the JVM anyway, and leaking would hide the bug.
     */
    JNIEnv* deleteEnv() const {
      if constexpr (Traits::threadConfined) {
        return env_;
      } else {
        JNIEnv* env = getEnvIfAttached();
        if (env == nullptr) {
          std::fprintf(
            stderr,
            "kolibri: fatal: global reference released after its thread detached from the JVM; "
            "release the owning object explicitly while the thread is attached\n"
          );
          std::abort();
        }
        return env;
      }
    }

    JNIEnv* env_ = nullptr;
    jobject ref_ = nullptr;
  };

  template<typename T = jobject>
  using Ref = OwnedRef<detail::LocalRefTraits, T>;

  template<typename T = jobject>
  using GlobalRef = OwnedRef<detail::GlobalRefTraits, T>;

  template<typename T>
  struct UnownedRef : detail::BasedRef<UnownedRef<T>, T> {
    UnownedRef() = default;

    UnownedRef(jobject r) : ref(r) {
    }

    [[nodiscard]] jobject get() const { return ref; }
    operator jobject() const { return ref; }
    explicit operator bool() const { return ref != nullptr; }

  private:
    jobject ref = nullptr;
  };

  namespace concepts {
    template<typename T>
    concept BasedRef =
      requires { typename std::remove_cvref_t<T>::InnerType; } &&
      std::is_base_of_v<
        detail::BasedRef<std::remove_cvref_t<T>, typename std::remove_cvref_t<T>::InnerType>,
        std::remove_cvref_t<T>
      >;

    template<typename T>
    concept OwnedRef = BasedRef<T> && requires { std::remove_cvref_t<T>::IsLocalRef; };

    template<typename T>
    concept Ref = OwnedRef<T> && std::remove_cvref_t<T>::IsLocalRef;

    template<typename T>
    concept GlobalRef = OwnedRef<T> && !std::remove_cvref_t<T>::IsLocalRef;

    template<typename T>
    concept UnownedRef = BasedRef<T> && !OwnedRef<T>;
  }
}

#define JAVA_FORWARD_ACCESSOR(name)                                                               \
  template<typename... ArgsT>                                                                     \
  decltype(auto) name(JNIEnv* env, ArgsT&&... args) const {                                       \
    return callToken(env, Owner::name, std::forward<ArgsT>(args)...);                             \
  }
