#pragma once

#include <array>
#include <cstddef>
#include <jni.h>
#include <string_view>

#include <kolibri/Ref.h>
#include <kolibri/meta/meta.h>

namespace expo::kolibri {
  namespace detail {
    template<typename T, typename = void>
    struct jni_descriptor {
      static_assert(sizeof(T) == 0,
                    "No default JNI descriptor for this type; "
                    "pass an explicit signature to makeNativeMethod.");
    };

    // clang-format off
    template <> struct jni_descriptor<void>     { static constexpr std::string_view value = "V"; };
    template <> struct jni_descriptor<jboolean> { static constexpr std::string_view value = "Z"; };
    template <> struct jni_descriptor<jbyte>    { static constexpr std::string_view value = "B"; };
    template <> struct jni_descriptor<jchar>    { static constexpr std::string_view value = "C"; };
    template <> struct jni_descriptor<jshort>   { static constexpr std::string_view value = "S"; };
    template <> struct jni_descriptor<jint>     { static constexpr std::string_view value = "I"; };
    template <> struct jni_descriptor<jlong>    { static constexpr std::string_view value = "J"; };
    template <> struct jni_descriptor<jfloat>   { static constexpr std::string_view value = "F"; };
    template <> struct jni_descriptor<jdouble>  { static constexpr std::string_view value = "D"; };
    template <> struct jni_descriptor<jstring>  { static constexpr std::string_view value = "Ljava/lang/String;"; };
    template <> struct jni_descriptor<jobject>  { static constexpr std::string_view value = "Ljava/lang/Object;"; };

    template <> struct jni_descriptor<jbooleanArray>{ static constexpr std::string_view value = "[Z"; };
    template <> struct jni_descriptor<jbyteArray>   { static constexpr std::string_view value = "[B"; };
    template <> struct jni_descriptor<jcharArray>   { static constexpr std::string_view value = "[C"; };
    template <> struct jni_descriptor<jshortArray>  { static constexpr std::string_view value = "[S"; };
    template <> struct jni_descriptor<jintArray>    { static constexpr std::string_view value = "[I"; };
    template <> struct jni_descriptor<jlongArray>   { static constexpr std::string_view value = "[J"; };
    template <> struct jni_descriptor<jfloatArray>  { static constexpr std::string_view value = "[F"; };
    template <> struct jni_descriptor<jdoubleArray> { static constexpr std::string_view value = "[D"; };
    template <> struct jni_descriptor<jobjectArray> { static constexpr std::string_view value = "[Ljava/lang/Object;"; };
    // clang-format on

    template<typename T>
    concept has_descriptor = requires { T::descriptor; };

    template<typename T>
    concept has_array_descriptor =
        has_descriptor<T> && std::string_view{T::descriptor}.starts_with('[');

    template<has_descriptor T>
    struct jni_descriptor<T, void> {
    private:
      static constexpr std::string_view name = T::descriptor;
      static constexpr auto storage = meta::wrap<name, "L", ";">();

    public:
      static constexpr std::string_view value{storage.data(), storage.size()};
    };

    template<has_array_descriptor T>
    struct jni_descriptor<T, void> {
      static constexpr std::string_view value = T::descriptor;
    };

    template<concepts::BasedRef T>
    struct jni_descriptor<T, void> {
      static constexpr std::string_view value =
          jni_descriptor<typename T::InnerType>::value;
    };
  }

  template<typename T>
  inline constexpr std::string_view jni_descriptor_v =
      detail::jni_descriptor<T>::value;

  template<typename T>
  std::string jni_descriptor_string_v() {
    constexpr std::string_view desc = jni_descriptor_v<T>;
    return {desc.data(), desc.size()};
  }

  namespace detail {
    template<typename R, typename... Params>
    constexpr std::size_t signature_length() {
      // "(" + parameter descriptors + ")" + return descriptor.
      return 2 + (jni_descriptor<Params>::value.size() + ... + 0) +
             jni_descriptor<R>::value.size();
    }
  }

  template<typename R, typename... Params>
  consteval std::array<char, detail::signature_length<R, Params...>() + 1> jni_signature() {
    std::array<char, detail::signature_length<R, Params...>() + 1> buf{};
    auto put = [&buf, i = 0](const std::string_view s) mutable {
      for (char c: s) {
        buf[i++] = c;
      }
    };
    put("(");
    (put(jni_descriptor_v<Params>), ...);
    put(")");
    put(jni_descriptor_v<R>);
    return buf;
  }

  template<typename Signature>
  struct JniSignatureFor;

  template<typename Return, typename... Args>
  struct JniSignatureFor<Return(Args...)> {
    static const char* value() {
      static constexpr auto signature = jni_signature<Return, Args...>();
      return signature.data();
    }
  };

  template<typename Return, typename... Args>
  struct JniSignatureFor<Return(Args...) noexcept> : JniSignatureFor<Return(Args...)> {
  };
}
