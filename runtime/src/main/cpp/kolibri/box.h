#pragma once

#include <atomic>
#include <jni.h>
#include <string_view>
#include <type_traits>
#include <variant>

#include <kolibri/JavaClass.h>
#include <kolibri/members/Method.h>
#include <kolibri/members/StaticField.h>
#include <kolibri/members/StaticMethod.h>
#include <kolibri/Ref.h>
#include <kolibri/defines.h>
#include <kolibri/exception.h>
#include <kolibri/signature.h>

namespace expo::kolibri {
  struct JBoolean : JavaClass<JBoolean> {
    static constexpr std::string_view descriptor = "java/lang/Boolean";
    static constexpr StaticMethod<"valueOf", Ref<JBoolean>(jboolean)> valueOf{};
    static constexpr StaticField<"TRUE", Ref<JBoolean>> boxedTrue{};
    static constexpr StaticField<"FALSE", Ref<JBoolean>> boxedFalse{};
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
    // valueOf(-128..127) results. Integer.valueOf is specified to return one cached object per
    // value in that range, and Long.valueOf caches the same range on HotSpot and ART; so handing
    // out another reference to the object valueOf returned first is what valueOf itself would do,
    // minus the call into Java. The boxes live in a Java array, one per wrapper type, so the whole
    // cache holds a single global reference instead of one per value.
    inline constexpr jsize kCachedBoxes = 256;

    // Two threads may both get here first: the loser drops its array and takes the winner's.
    NOINLINE inline jobjectArray publishBoxCache(
      JNIEnv* env,
      std::atomic<jobjectArray>& slot,
      const jclass boxClass
    ) {
      const auto local = Ref<>::adopt(env, env->NewObjectArray(kCachedBoxes, boxClass, nullptr));
      if (!local) {
        throwWithPending(env, "Could not create a box cache");
      }
      const auto created = static_cast<jobjectArray>(env->NewGlobalRef(local.get()));
      if (created == nullptr) {
        throwWithPending(env, "Could not hold a box cache");
      }
      jobjectArray winner = nullptr;
      if (slot.compare_exchange_strong(winner, created, std::memory_order_acq_rel)) {
        return created;
      }
      env->DeleteGlobalRef(created);
      return winner;
    }

    // Threads racing on an empty slot each store what valueOf gave them; any of those is a correct
    // box, and for Integer (and Long on HotSpot and ART) they are the same object anyway.
    template<typename Box, typename T>
    NOINLINE jobject fillCachedBox(
      JNIEnv* env,
      const jobjectArray cache,
      const jsize index,
      const T value
    ) {
      jobject boxed = Box::valueOf(env, value).release();
      env->SetObjectArrayElement(cache, index, boxed);
      return boxed;
    }

    template<typename Box, typename T>
    jobject cachedBox(JNIEnv* env, const T value) {
      static constinit std::atomic<jobjectArray> slot{nullptr};
      jobjectArray cache = slot.load(std::memory_order_acquire);
      if (cache == nullptr) [[unlikely]] {
        cache = publishBoxCache(env, slot, Box::javaClass(env));
      }
      const auto index = static_cast<jsize>(value + 128);
      const jobject boxed = env->GetObjectArrayElement(cache, index);
      if (boxed == nullptr) [[unlikely]] {
        return fillCachedBox<Box>(env, cache, index, value);
      }
      return boxed;
    }

    template<typename T>
    constexpr bool inBoxCache(const T value) {
      return value >= -128 && value <= 127;
    }

#ifdef __ANDROID__
    // On Android, xxxValue() is always called. ART treats the wrappers' private `value` fields as
    // non-SDK interfaces that aren't on any allow list. Since Android 9, apps can't look them up
    // (GetFieldID throws NoSuchFieldError), and each lookup logs an "Accessing hidden field"
    // warning and reports a StrictMode NonSdkApiUsedViolation, which crashes apps that use
    // penaltyDeath(). So don't try.
    template<typename Box, auto GetField, typename ValueMethod>
    auto unboxValue(JNIEnv* env, const jobject boxed, const ValueMethod valueMethod) {
      return valueMethod(env, boxed);
    }
#else
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
#endif
  } // namespace detail

  /**
   * Boxes a primitive into its `java.lang` wrapper. Returns a new local reference owned by the
   * current JNI frame.
   */
  [[nodiscard]] inline jobject box(JNIEnv* env, const jboolean value) {
    // Boolean.valueOf returns one of these two, so read them straight off the class.
    if (value != 0) {
      return JBoolean::boxedTrue(env).release();
    }
    return JBoolean::boxedFalse(env).release();
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
