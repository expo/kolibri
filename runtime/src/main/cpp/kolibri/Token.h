#pragma once

#include <concepts>
#include <string>
#include <tuple>
#include <type_traits>

#include <kolibri/meta/CTString.h>
#include <kolibri/signature.h>
#include <kolibri/meta/meta.h>

namespace expo::kolibri {
  namespace detail {
    // Empty marker bases the token types
    // clang-format off
    struct TokenTag {};
    struct FunctionTokenTag {};
    struct FieldTokenTag {};
    // clang-format on
  }

  template<typename Class, meta::CTString Name, auto Descriptor, typename ReturnType,
    typename... Args>
  struct SignatureToken : detail::TokenTag {
    using ClassType = Class;
    using Return = ReturnType;
    using ArgsTuple = std::tuple<Args...>;

    static constexpr const char* name() { return Name.value; }

    static const char* signature() { return Descriptor.data(); }
  };

  template<typename Class, meta::CTString Name, typename ReturnType, typename... Args>
  struct FunctionToken
      : SignatureToken<Class, Name, jni_signature<ReturnType, Args...>(), ReturnType, Args...>,
        detail::FunctionTokenTag {
  };

  template<typename Class, meta::CTString Name, typename T>
  struct FieldToken
      : SignatureToken<Class, Name, meta::store_as_array<jni_descriptor_v<T>>(), T>,
        detail::FieldTokenTag {
    using Value = T;
  };

  namespace concepts {
    template<typename TokenCandidate>
    concept Token = requires
    {
      typename TokenCandidate::ClassType;
      typename TokenCandidate::Return;
      typename TokenCandidate::ArgsTuple;
      { TokenCandidate::name() } -> std::convertible_to<const char*>;
      { TokenCandidate::signature() } -> std::convertible_to<const char*>;
    } && std::is_base_of_v<detail::TokenTag, TokenCandidate>;

    template<typename TokenCandidate>
    concept FunctionToken =
        Token<TokenCandidate> && std::is_base_of_v<detail::FunctionTokenTag, TokenCandidate>;

    template<typename TokenCandidate>
    concept FieldToken =
        Token<TokenCandidate> && std::is_base_of_v<detail::FieldTokenTag, TokenCandidate>;
  }

  template<concepts::Token T>
  std::string tokenDebugDescription() {
    return std::string(T::ClassType::descriptor) + "." + std::string(T::name()) + T::signature();
  }
}
