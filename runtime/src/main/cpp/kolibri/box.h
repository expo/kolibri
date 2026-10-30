#pragma once

#include <atomic>
#include <jni.h>
#include <string_view>
#include <type_traits>
#include <variant>

#include <kolibri/JavaClass.h>
#include <kolibri/members/Method.h>
#include <kolibri/members/StaticMethod.h>
#include <kolibri/Ref.h>
#include <kolibri/defines.h>
#include <kolibri/exception.h>
#include <kolibri/signature.h>

namespace expo::kolibri {
  struct JBoolean : JavaClass<JBoolean> {
    static constexpr std::string_view descriptor = "java/lang/Boolean";
    static constexpr StaticMethod<"valueOf", Ref<JBoolean>(jboolean)> valueOf{};
    static constexpr Method<"booleanValue", jboolean()> booleanValue{};

    struct Accessors : BaseAccessors {
      JAVA_FORWARD_ACCESSOR(booleanValue)
    };
  };

  struct JInteger : JavaClass<JInteger> {
    static constexpr std::string_view descriptor = "java/lang/Integer";
    static constexpr StaticMethod<"valueOf", Ref<JInteger>(jint)> valueOf{};
    static constexpr Method<"intValue", jint()> intValue{};

    struct Accessors : BaseAccessors {
      JAVA_FORWARD_ACCESSOR(intValue)
    };
  };

  struct JLong : JavaClass<JLong> {
    static constexpr std::string_view descriptor = "java/lang/Long";
    static constexpr StaticMethod<"valueOf", Ref<JLong>(jlong)> valueOf{};
    static constexpr Method<"longValue", jlong()> longValue{};

    struct Accessors : BaseAccessors {
      JAVA_FORWARD_ACCESSOR(longValue)
    };
  };

  struct JFloat : JavaClass<JFloat> {
    static constexpr std::string_view descriptor = "java/lang/Float";
    static constexpr StaticMethod<"valueOf", Ref<JFloat>(jfloat)> valueOf{};
    static constexpr Method<"floatValue", jfloat()> floatValue{};

    struct Accessors : BaseAccessors {
      JAVA_FORWARD_ACCESSOR(floatValue)
    };
  };

  struct JDouble : JavaClass<JDouble> {
    static constexpr std::string_view descriptor = "java/lang/Double";
    static constexpr StaticMethod<"valueOf", Ref<JDouble>(jdouble)> valueOf{};
    static constexpr Method<"doubleValue", jdouble()> doubleValue{};

    struct Accessors : BaseAccessors {
      JAVA_FORWARD_ACCESSOR(doubleValue)
    };
  };

  namespace detail {
    // valueOf(-128..127) results, held for the process. Integer.valueOf is specified to return
    // one cached object per value in that range, Boolean.valueOf returns TRUE or FALSE, and
    // Long.valueOf caches the same range on HotSpot and ART; so handing out another reference to
    // the object valueOf returned first is what valueOf itself would do, minus the call into Java.
    inline constexpr int kCachedBoxes = 256;

    // Two threads may both get here first: the loser drops its global ref and takes the winner's.
    NOINLINE inline jobject publishCachedBox(
      JNIEnv* env,
      std::atomic<jobject>& slot,
      const jobject boxed
    ) {
      const jobject created = env->NewGlobalRef(boxed);
      if (created == nullptr) {
        throwWithPending(env, "Could not hold a cached box");
      }
      jobject winner = nullptr;
      if (slot.compare_exchange_strong(winner, created, std::memory_order_acq_rel)) {
        return created;
      }
      env->DeleteGlobalRef(created);
      return winner;
    }

    template<typename Box, typename T>
    jobject cachedBox(JNIEnv* env, const T value) {
      static constinit std::atomic<jobject> slots[kCachedBoxes]{};
      std::atomic<jobject>& slot = slots[static_cast<int>(value) + 128];
      jobject boxed = slot.load(std::memory_order_acquire);
      if (boxed == nullptr) [[unlikely]] {
        boxed = publishCachedBox(env, slot, Box::valueOf(env, value).get());
      }
      return env->NewLocalRef(boxed);
    }

    template<typename T>
    constexpr bool inBoxCache(const T value) {
      return value >= -128 && value <= 127;
    }

    // The wrapper's private `value` field, read directly instead of calling xxxValue() in Java.
    // The wrappers are final, so nothing can override xxxValue(), and JNI field access ignores
    // Java access checks. Null if the VM has no such field: then the method is called instead.
    NOINLINE inline jfieldID findBoxValueField(JNIEnv* env, jclass clazz, const char* signature) {
      const jfieldID field = env->GetFieldID(clazz, "value", signature);
      if (field == nullptr) {
        env->ExceptionClear();
      }
      return field;
    }

    template<typename Box, auto GetField, typename ValueMethod>
    auto unboxValue(JNIEnv* env, const jobject boxed, const ValueMethod valueMethod) {
      using Value = decltype((env->*GetField)(boxed, jfieldID{}));
      static const jfieldID field =
        findBoxValueField(env, Box::javaClass(env), jni_descriptor_v<Value>.data());
      if (field != nullptr && boxed != nullptr) [[likely]] {
        return (env->*GetField)(boxed, field);
      }
      // The method also reports a null receiver, as before.
      return valueMethod(env, boxed);
    }
  } // namespace detail

  /**
   * Boxes a primitive into its `java.lang` wrapper. Returns a new local reference owned by the
   * current JNI frame.
   */
  [[nodiscard]] inline jobject box(JNIEnv* env, const jboolean value) {
    return detail::cachedBox<JBoolean>(env, static_cast<jboolean>(value != 0));
  }

  // A separate overload: a plain `bool` would otherwise promote to `jint` and box as an Integer.
  [[nodiscard]] inline jobject box(JNIEnv* env, const bool value) {
    return box(env, static_cast<jboolean>(value));
  }

  [[nodiscard]] inline jobject box(JNIEnv* env, const jint value) {
    if (detail::inBoxCache(value)) {
      return detail::cachedBox<JInteger>(env, value);
    }
    return JInteger::valueOf(env, value).release();
  }

  [[nodiscard]] inline jobject box(JNIEnv* env, const jlong value) {
    if (detail::inBoxCache(value)) {
      return detail::cachedBox<JLong>(env, value);
    }
    return JLong::valueOf(env, value).release();
  }

  [[nodiscard]] inline jobject box(JNIEnv* env, const jfloat value) {
    return JFloat::valueOf(env, value).release();
  }

  [[nodiscard]] inline jobject box(JNIEnv* env, const jdouble value) {
    return JDouble::valueOf(env, value).release();
  }

  using Boxed = std::variant<std::monostate, jboolean, jint, jlong, jfloat, jdouble>;

  [[nodiscard]] inline Boxed unbox(JNIEnv* env, const jobject value) {
    if (value == nullptr) {
      return std::monostate{};
    }
    if (env->IsInstanceOf(value, JInteger::javaClass(env))) {
      return detail::unboxValue<JInteger, &JNIEnv::GetIntField>(env, value, JInteger::intValue);
    }
    if (env->IsInstanceOf(value, JDouble::javaClass(env))) {
      return detail::unboxValue<JDouble, &JNIEnv::GetDoubleField>(env, value, JDouble::doubleValue);
    }
    if (env->IsInstanceOf(value, JBoolean::javaClass(env))) {
      return detail::unboxValue<JBoolean, &JNIEnv::GetBooleanField>(
        env, value, JBoolean::booleanValue
      );
    }
    if (env->IsInstanceOf(value, JLong::javaClass(env))) {
      return detail::unboxValue<JLong, &JNIEnv::GetLongField>(env, value, JLong::longValue);
    }
    if (env->IsInstanceOf(value, JFloat::javaClass(env))) {
      return detail::unboxValue<JFloat, &JNIEnv::GetFloatField>(env, value, JFloat::floatValue);
    }
    return std::monostate{};
  }

  namespace concepts {
    template<typename T>
    concept BoxedType =
      std::is_same_v<T, JBoolean> || std::is_same_v<T, JInteger> || std::is_same_v<T, JLong> ||
      std::is_same_v<T, JFloat> || std::is_same_v<T, JDouble>;
  }

  template<concepts::BasedRef R> requires concepts::BoxedType<typename std::remove_cvref_t<R>::InnerType>
  [[nodiscard]] auto unbox(JNIEnv* env, const R& ref) {
    using Class = std::remove_cvref_t<R>::InnerType;
    const jobject boxed = ref.get();
    if constexpr (std::is_same_v<Class, JBoolean>) {
      return detail::unboxValue<JBoolean, &JNIEnv::GetBooleanField>(
        env, boxed, JBoolean::booleanValue
      );
    } else if constexpr (std::is_same_v<Class, JInteger>) {
      return detail::unboxValue<JInteger, &JNIEnv::GetIntField>(env, boxed, JInteger::intValue);
    } else if constexpr (std::is_same_v<Class, JLong>) {
      return detail::unboxValue<JLong, &JNIEnv::GetLongField>(env, boxed, JLong::longValue);
    } else if constexpr (std::is_same_v<Class, JFloat>) {
      return detail::unboxValue<JFloat, &JNIEnv::GetFloatField>(env, boxed, JFloat::floatValue);
    } else {
      return detail::unboxValue<JDouble, &JNIEnv::GetDoubleField>(
        env, boxed, JDouble::doubleValue
      );
    }
  }
} // namespace expo::kolibri
