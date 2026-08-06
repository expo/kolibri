#pragma once
#include <cstddef>
#include <string_view>

namespace expo::meta {
  template<std::size_t N>
  struct CTString {
    char value[N]{};

    constexpr CTString(const char (&str)[N]) {
      for (std::size_t i = 0; i < N; i++) {
        value[i] = str[i];
      }
    }

    [[nodiscard]] constexpr std::string_view view() const { return std::string_view(value, N - 1); }
  };
} // namespace expo::meta
