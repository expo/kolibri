#pragma once

#include <array>
#include <cstddef>
#include <string_view>
#include <type_traits>

#include <kolibri/meta/CTString.h>

namespace expo::meta {
  template<const std::string_view& Value, CTString Prefix, CTString Suffix>
  constexpr auto wrap() {
    constexpr std::string_view prefix = Prefix.view();
    constexpr std::string_view suffix = Suffix.view();
    constexpr size_t size = prefix.size() + Value.size() + suffix.size();

    std::array<char, size> buf{};
    std::size_t i = 0;

    for (char c: prefix) {
      buf[i++] = c;
    }

    for (char c: Value) {
      buf[i++] = c;
    }

    for (char c: suffix) {
      buf[i++] = c;
    }

    return buf;
  }

  template<typename T>
  inline constexpr bool is_c_string_v = std::is_same_v<std::decay_t<T>, char*> ||
                                        std::is_same_v<std::decay_t<T>, const char*>;

  template<const std::string_view& Value>
  consteval auto store_as_array() {
    // +1, because we're adding null at the end
    std::array<char, Value.size() + 1> storage{};
    for (std::size_t i = 0; i < Value.size(); i++) {
      storage[i] = Value[i];
    }
    return storage;
  }
}
