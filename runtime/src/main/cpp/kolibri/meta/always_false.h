#pragma once

namespace expo::meta {
  template<typename...>
  inline constexpr bool always_false_v = false;
}
