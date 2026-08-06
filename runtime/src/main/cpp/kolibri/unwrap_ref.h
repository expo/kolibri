#pragma once

#include <cstddef>
#include <jni.h>
#include <stdexcept>
#include <type_traits>

#include <kolibri/jni_traits.h>
#include <kolibri/Ref.h>
#include <kolibri/meta/always_false.h>

namespace expo::kolibri {
  template<typename Receiver>
  jobject unwrapRef(Receiver&& receiver) {
    using Value = std::remove_cvref_t<Receiver>;

    if constexpr (concepts::BasedRef<Value>) {
      return receiver.get();
    } else if constexpr (is_jni_object_v<Value>) {
      return receiver;
    } else if constexpr (std::is_same_v<Value, std::nullptr_t>) {
      return nullptr;
    } else {
      static_assert(meta::always_false_v<Value>, "Expected a JNI object or Java ref receiver");
      throw std::runtime_error("Expected a JNI object or Java ref receiver");
    }
  }
}
