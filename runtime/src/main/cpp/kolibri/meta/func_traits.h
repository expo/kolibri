#pragma once

#include <cstddef>
#include <tuple>

namespace expo::meta {
  template<typename T>
  struct func_traits : func_traits<decltype(&T::operator())> {
  };

  template<typename R, typename... A>
  struct func_traits<R(A...)> {
    using return_type = R;
    using params = std::tuple<A...>;

    static constexpr bool is_noexcept = false;
    static constexpr std::size_t arity = sizeof...(A);

    template<std::size_t N>
    struct arg {
      static_assert(N < arity, "Error: invalid parameter index.");
      using type = std::tuple_element_t<N, std::tuple<A...> >;
    };

    template<std::size_t Index, typename Type>
    static consteval bool nth_arg_is() {
      if constexpr (Index >= arity) {
        return false;
      } else {
        return std::is_same_v<Type, typename arg<Index>::type>;
      }
    }
  };

  // clang-format off
  template<typename R, typename... A>
  struct func_traits<R(A...) noexcept>                                         : func_traits<R(A...)> { static constexpr bool is_noexcept = true; };
  template<typename R, typename... A> struct func_traits<R (*)(A...)>          : func_traits<R(A...)> {};
  template<typename R, typename... A> struct func_traits<R (*)(A...) noexcept> : func_traits<R(A...) noexcept> {};

  template<typename C, typename R, typename... A> struct func_traits<R (C::*)(A...)>                : func_traits<R(A...)>          { using class_type = C; };
  template<typename C, typename R, typename... A> struct func_traits<R (C::*)(A...) noexcept>       : func_traits<R(A...) noexcept> { using class_type = C; };
  template<typename C, typename R, typename... A> struct func_traits<R (C::*)(A...) const>          : func_traits<R(A...)>          { using class_type = C; };
  template<typename C, typename R, typename... A> struct func_traits<R (C::*)(A...) const noexcept> : func_traits<R(A...) noexcept> { using class_type = C; };
  // clang-format on
} // namespace expo::meta
