#pragma once

#include <jni.h>
#include <type_traits>

namespace expo::kolibri {
  template<typename T>
  inline constexpr bool is_jni_object_v = std::is_pointer_v<std::remove_cvref_t<T>> &&
                                          std::is_convertible_v<std::remove_cvref_t<T>, jobject>;
}
