#pragma once

#include <jni.h>
#include <string_view>
#include <type_traits>
#include <variant>

#include <kolibri/JavaClass.h>
#include <kolibri/members/Method.h>
#include <kolibri/members/StaticMethod.h>
#include <kolibri/Ref.h>

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

  /**
   * Boxes a primitive into its `java.lang` wrapper. Returns a new local reference owned by the
   * current JNI frame.
   */
  [[nodiscard]] inline jobject box(JNIEnv* env, const jboolean value) {
    return JBoolean::valueOf(env, value).release();
  }

  // A separate overload: a plain `bool` would otherwise promote to `jint` and box as an Integer.
  [[nodiscard]] inline jobject box(JNIEnv* env, const bool value) {
    return box(env, static_cast<jboolean>(value));
  }

  [[nodiscard]] inline jobject box(JNIEnv* env, const jint value) {
    return JInteger::valueOf(env, value).release();
  }

  [[nodiscard]] inline jobject box(JNIEnv* env, const jlong value) {
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
      return JInteger::intValue(env, value);
    }
    if (env->IsInstanceOf(value, JDouble::javaClass(env))) {
      return JDouble::doubleValue(env, value);
    }
    if (env->IsInstanceOf(value, JBoolean::javaClass(env))) {
      return JBoolean::booleanValue(env, value);
    }
    if (env->IsInstanceOf(value, JLong::javaClass(env))) {
      return JLong::longValue(env, value);
    }
    if (env->IsInstanceOf(value, JFloat::javaClass(env))) {
      return JFloat::floatValue(env, value);
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
    if constexpr (std::is_same_v<Class, JBoolean>) {
      return ref->booleanValue(env);
    } else if constexpr (std::is_same_v<Class, JInteger>) {
      return ref->intValue(env);
    } else if constexpr (std::is_same_v<Class, JLong>) {
      return ref->longValue(env);
    } else if constexpr (std::is_same_v<Class, JFloat>) {
      return ref->floatValue(env);
    } else {
      return ref->doubleValue(env);
    }
  }
} // namespace expo::kolibri
