#pragma once

#include <tuple>
#include <type_traits>
#include <utility>

namespace expo::meta {
  namespace detail {
    template<typename RemoveT, typename Tuple>
    struct remove_by_type_impl;

    template<typename RemoveT, typename... Args>
    struct remove_by_type_impl<RemoveT, std::tuple<Args...> > {
      using type = decltype(std::tuple_cat(
        std::declval<
          std::conditional_t<std::is_same_v<Args, RemoveT>, std::tuple<>, std::tuple<Args> > >()...
      ));
    };
  } // namespace detail

  template<typename RemoveT, typename Tuple>
  using remove_by_type_t = detail::remove_by_type_impl<RemoveT, Tuple>::type;

  namespace detail {
    template<typename T, typename... Candidates>
    inline constexpr bool is_any_of = (std::is_same_v<T, Candidates> || ...);

    template<typename Tuple, typename... Remove>
    struct remove_by_types_impl;

    template<typename... Args, typename... Remove>
    struct remove_by_types_impl<std::tuple<Args...>, Remove...> {
      using type = decltype(std::tuple_cat(
        std::declval<
          std::conditional_t<is_any_of<Args, Remove...>, std::tuple<>, std::tuple<Args> > >()...
      ));
    };
  } // namespace detail

  template<typename Tuple, typename... Remove>
  using remove_by_types_t = detail::remove_by_types_impl<Tuple, Remove...>::type;

  namespace detail {
    template<typename Head, typename Tuple>
    struct prepend_impl;

    template<typename Head, typename... Args>
    struct prepend_impl<Head, std::tuple<Args...> > {
      using type = std::tuple<Head, Args...>;
    };
  } // namespace detail

  template<typename Head, typename Tuple>
  using prepend_t = detail::prepend_impl<Head, Tuple>::type;

  template<typename T, typename ExpectedType>
  inline constexpr bool is_pointer_of =
      std::is_pointer_v<T> && std::is_base_of_v<ExpectedType, std::remove_pointer_t<T> >;
} // namespace expo::meta
