#pragma once
#include <array>
#include <cstddef>
#include <jni.h>
#include <tuple>
#include <utility>

#include <kolibri/Token.h>
#include <kolibri/exception.h>
#include <kolibri/jni_traits.h>
#include <kolibri/Ref.h>
#include <kolibri/string_utils.h>
#include <kolibri/defines.h>
#include <kolibri/meta/always_false.h>
#include <kolibri/meta/meta.h>

namespace expo::kolibri {
  namespace detail {
    template<typename Declared>
    inline constexpr bool is_jni_reference_slot_v =
        !(std::is_same_v<Declared, jboolean> || std::is_same_v<Declared, jbyte> ||
          std::is_same_v<Declared, jchar> || std::is_same_v<Declared, jshort> ||
          std::is_same_v<Declared, jint> || std::is_same_v<Declared, jlong> ||
          std::is_same_v<Declared, jfloat> || std::is_same_v<Declared, jdouble>);

    template<typename Declared, typename Passed>
    inline constexpr bool is_compatible_jni_arg_v = [] {
      using P = std::remove_cvref_t<Passed>;
      if constexpr (std::is_same_v<Declared, jboolean>) {
        return std::is_same_v<P, jboolean> || std::is_same_v<P, bool>;
      } else if constexpr (std::is_same_v<Declared, jint>) {
        return std::is_same_v<P, jint> || std::is_same_v<P, int>;
      } else if constexpr (!is_jni_reference_slot_v<Declared>) {
        return std::is_same_v<P, Declared>;
      } else {
        return concepts::BasedRef<P> || is_jni_object_v<P> ||
               std::is_same_v<P, std::nullptr_t> || std::is_same_v<P, std::string> ||
               meta::is_c_string_v<P>;
      }
    }();

    template<typename DeclaredTuple, typename PassedTuple, std::size_t... I>
    constexpr bool compatible_jni_args_impl(std::index_sequence<I...>) {
      return (is_compatible_jni_arg_v<
          std::tuple_element_t<I, DeclaredTuple>,
          std::tuple_element_t<I, PassedTuple>> &&
        ...);
    }

    template<typename DeclaredTuple, typename... Passed>
    inline constexpr bool are_compatible_jni_args_v =
        sizeof...(Passed) == std::tuple_size_v<DeclaredTuple> &&
        compatible_jni_args_impl<DeclaredTuple, std::tuple<Passed...>>(
          std::index_sequence_for<Passed...>{});

    template<typename T, std::size_t N>
    jvalue toJValue(
      JNIEnv* env,
      T&& value,
      std::array<Ref<>, N>& retained,
      std::size_t index
    ) {
      using Value = std::remove_cvref_t<T>;
      jvalue out{};

      const auto retainString = [&](const std::string& str) ALWAYS_INLINE_LAMBDA {
        retained[index] = Ref<>::adopt(env, toJString(env, str));
        out.l = retained[index].get();
      };

      if constexpr (std::is_same_v<Value, jboolean> || std::is_same_v<Value, bool>) {
        out.z = static_cast<jboolean>(value);
      } else if constexpr (std::is_same_v<Value, jbyte>) {
        out.b = value;
      } else if constexpr (std::is_same_v<Value, jchar>) {
        out.c = value;
      } else if constexpr (std::is_same_v<Value, jshort>) {
        out.s = value;
      } else if constexpr (std::is_same_v<Value, jint> || std::is_same_v<Value, int>) {
        out.i = static_cast<jint>(value);
      } else if constexpr (std::is_same_v<Value, jlong>) {
        out.j = value;
      } else if constexpr (std::is_same_v<Value, jfloat>) {
        out.f = value;
      } else if constexpr (std::is_same_v<Value, jdouble>) {
        out.d = value;
      } else if constexpr (concepts::BasedRef<Value>) {
        out.l = value.get();
      } else if constexpr (is_jni_object_v<Value>) {
        out.l = value;
      } else if constexpr (std::is_same_v<Value, std::nullptr_t>) {
        out.l = nullptr;
      } else if constexpr (std::is_same_v<Value, std::string>) {
        retainString(value);
      } else if constexpr (meta::is_c_string_v<Value>) {
        const char* cstr = value;
        if (cstr != nullptr) {
          retainString(cstr);
        }
      } else {
        static_assert(meta::always_false_v<Value>, "Unsupported JNI token argument type");
      }

      return out;
    }

    template<typename R>
    R wrapObject(JNIEnv* env, jobject raw) {
      if constexpr (concepts::Ref<R>) {
        return R::adopt(env, raw);
      } else if constexpr (concepts::GlobalRef<R>) {
        R result = R::make(env, raw);
        if (raw != nullptr) {
          env->DeleteLocalRef(raw);
        }
        return result;
      } else if constexpr (std::is_same_v<R, std::string>) {
        const Ref<> str = Ref<>::adopt(env, raw);
        return toStdString(env, reinterpret_cast<jstring>(raw));
      } else if constexpr (is_jni_object_v<R>) {
        return reinterpret_cast<R>(raw);
      } else {
        static_assert(meta::always_false_v<R>, "Unsupported JNI token object return type");
        throw std::runtime_error("Unsupported JNI token object type");
      }
    }
  }

  template<std::size_t N, typename... Args>
  std::array<jvalue, sizeof...(Args)> makeJValues(
    JNIEnv* env,
    std::array<Ref<>, N>& retained,
    Args&&... args
  ) {
    std::array<jvalue, sizeof...(Args)> values{};
    if constexpr (sizeof...(Args) > 0) {
      std::size_t index = 0;
      ((values[index] = toJValue(env, std::forward<Args>(args), retained, index), ++index), ...);
    }
    return values;
  }

  template<concepts::FunctionToken Token>
  jmethodID getMethodId(JNIEnv* env) {
    jmethodID id = env->GetMethodID(
      Token::ClassType::javaClass(env),
      Token::name(),
      Token::signature()
    );
    if (id == nullptr) {
      throwWithPending(env, "Could not find method" + tokenDebugDescription<Token>());
    }
    return id;
  }

  template<concepts::FunctionToken Token>
  jmethodID getStaticMethodId(JNIEnv* env) {
    jmethodID id = env->GetStaticMethodID(
      Token::ClassType::javaClass(env),
      Token::name(),
      Token::signature()
    );
    if (id == nullptr) {
      throwWithPending(env, "Could not find static method" + tokenDebugDescription<Token>());
    }
    return id;
  }

  template<concepts::FieldToken Token>
  jfieldID getFieldId(JNIEnv* env) {
    jfieldID id = env->GetFieldID(
      Token::ClassType::javaClass(env),
      Token::name(),
      Token::signature()
    );
    if (id == nullptr) {
      throwWithPending(env, "Could not find field" + tokenDebugDescription<Token>());
    }
    return id;
  }

  template<concepts::FieldToken Token>
  jfieldID getStaticFieldId(JNIEnv* env) {
    jfieldID id = env->GetStaticFieldID(
      Token::ClassType::javaClass(env),
      Token::name(),
      Token::signature()
    );
    if (id == nullptr) {
      throwWithPending(env, "Could not find static field" + tokenDebugDescription<Token>());
    }
    return id;
  }

  template<typename R, bool NoThrow>
  R callMethod(
    JNIEnv* env,
    const jobject receiver,
    const jmethodID id,
    jvalue* args
  ) {
    const auto check = [&]() ALWAYS_INLINE_LAMBDA {
      if constexpr (!NoThrow) {
        checkAndThrowPending(env);
      }
    };

    if constexpr (std::is_void_v<R>) {
      env->CallVoidMethodA(receiver, id, args);
      check();
      return;
    } else if constexpr (std::is_same_v<R, jboolean>) {
      R result = env->CallBooleanMethodA(receiver, id, args);
      check();
      return result;
    } else if constexpr (std::is_same_v<R, bool>) {
      bool result = env->CallBooleanMethodA(receiver, id, args) != 0;
      check();
      return result;
    } else if constexpr (std::is_same_v<R, jbyte>) {
      R result = env->CallByteMethodA(receiver, id, args);
      check();
      return result;
    } else if constexpr (std::is_same_v<R, jchar>) {
      R result = env->CallCharMethodA(receiver, id, args);
      check();
      return result;
    } else if constexpr (std::is_same_v<R, jshort>) {
      R result = env->CallShortMethodA(receiver, id, args);
      check();
      return result;
    } else if constexpr (std::is_same_v<R, jint>) {
      R result = env->CallIntMethodA(receiver, id, args);
      check();
      return result;
    } else if constexpr (std::is_same_v<R, jlong>) {
      R result = env->CallLongMethodA(receiver, id, args);
      check();
      return result;
    } else if constexpr (std::is_same_v<R, jfloat>) {
      R result = env->CallFloatMethodA(receiver, id, args);
      check();
      return result;
    } else if constexpr (std::is_same_v<R, jdouble>) {
      R result = env->CallDoubleMethodA(receiver, id, args);
      check();
      return result;
    } else {
      const jobject raw = env->CallObjectMethodA(receiver, id, args);
      check();
      return detail::wrapObject<R>(env, raw);
    }
  }

  template<typename R, bool NoThrow>
  R callStaticMethod(JNIEnv* env, const jclass clazz, const jmethodID id, jvalue* args) {
    const auto check = [&]() ALWAYS_INLINE_LAMBDA {
      if constexpr (!NoThrow) {
        checkAndThrowPending(env);
      }
    };

    if constexpr (std::is_void_v<R>) {
      env->CallStaticVoidMethodA(clazz, id, args);
      check();
      return;
    } else if constexpr (std::is_same_v<R, jboolean>) {
      R result = env->CallStaticBooleanMethodA(clazz, id, args);
      check();
      return result;
    } else if constexpr (std::is_same_v<R, bool>) {
      bool result = env->CallStaticBooleanMethodA(clazz, id, args) != 0;
      check();
      return result;
    } else if constexpr (std::is_same_v<R, jbyte>) {
      R result = env->CallStaticByteMethodA(clazz, id, args);
      check();
      return result;
    } else if constexpr (std::is_same_v<R, jchar>) {
      R result = env->CallStaticCharMethodA(clazz, id, args);
      check();
      return result;
    } else if constexpr (std::is_same_v<R, jshort>) {
      R result = env->CallStaticShortMethodA(clazz, id, args);
      check();
      return result;
    } else if constexpr (std::is_same_v<R, jint>) {
      R result = env->CallStaticIntMethodA(clazz, id, args);
      check();
      return result;
    } else if constexpr (std::is_same_v<R, jlong>) {
      R result = env->CallStaticLongMethodA(clazz, id, args);
      check();
      return result;
    } else if constexpr (std::is_same_v<R, jfloat>) {
      R result = env->CallStaticFloatMethodA(clazz, id, args);
      check();
      return result;
    } else if constexpr (std::is_same_v<R, jdouble>) {
      R result = env->CallStaticDoubleMethodA(clazz, id, args);
      check();
      return result;
    } else {
      const jobject raw = env->CallStaticObjectMethodA(clazz, id, args);
      check();
      return detail::wrapObject<R>(env, raw);
    }
  }

  template<typename R>
  R getFieldValue(JNIEnv* env, const jobject receiver, const jfieldID id) {
    if constexpr (std::is_same_v<R, jboolean>) {
      return env->GetBooleanField(receiver, id);
    } else if constexpr (std::is_same_v<R, bool>) {
      return env->GetBooleanField(receiver, id) != 0;
    } else if constexpr (std::is_same_v<R, jbyte>) {
      return env->GetByteField(receiver, id);
    } else if constexpr (std::is_same_v<R, jchar>) {
      return env->GetCharField(receiver, id);
    } else if constexpr (std::is_same_v<R, jshort>) {
      return env->GetShortField(receiver, id);
    } else if constexpr (std::is_same_v<R, jint>) {
      return env->GetIntField(receiver, id);
    } else if constexpr (std::is_same_v<R, jlong>) {
      return env->GetLongField(receiver, id);
    } else if constexpr (std::is_same_v<R, jfloat>) {
      return env->GetFloatField(receiver, id);
    } else if constexpr (std::is_same_v<R, jdouble>) {
      return env->GetDoubleField(receiver, id);
    } else {
      return detail::wrapObject<R>(env, env->GetObjectField(receiver, id));
    }
  }

  template<typename R>
  R getStaticFieldValue(JNIEnv* env, const jclass clazz, const jfieldID id) {
    if constexpr (std::is_same_v<R, jboolean>) {
      return env->GetStaticBooleanField(clazz, id);
    } else if constexpr (std::is_same_v<R, bool>) {
      return env->GetStaticBooleanField(clazz, id) != 0;
    } else if constexpr (std::is_same_v<R, jbyte>) {
      return env->GetStaticByteField(clazz, id);
    } else if constexpr (std::is_same_v<R, jchar>) {
      return env->GetStaticCharField(clazz, id);
    } else if constexpr (std::is_same_v<R, jshort>) {
      return env->GetStaticShortField(clazz, id);
    } else if constexpr (std::is_same_v<R, jint>) {
      return env->GetStaticIntField(clazz, id);
    } else if constexpr (std::is_same_v<R, jlong>) {
      return env->GetStaticLongField(clazz, id);
    } else if constexpr (std::is_same_v<R, jfloat>) {
      return env->GetStaticFloatField(clazz, id);
    } else if constexpr (std::is_same_v<R, jdouble>) {
      return env->GetStaticDoubleField(clazz, id);
    } else {
      return detail::wrapObject<R>(env, env->GetStaticObjectField(clazz, id));
    }
  }

  template<typename FieldType>
  void setFieldValue(JNIEnv* env, const jobject receiver, const jfieldID id, const jvalue& value) {
    if constexpr (std::is_same_v<FieldType, jboolean> || std::is_same_v<FieldType, bool>) {
      env->SetBooleanField(receiver, id, value.z);
    } else if constexpr (std::is_same_v<FieldType, jbyte>) {
      env->SetByteField(receiver, id, value.b);
    } else if constexpr (std::is_same_v<FieldType, jchar>) {
      env->SetCharField(receiver, id, value.c);
    } else if constexpr (std::is_same_v<FieldType, jshort>) {
      env->SetShortField(receiver, id, value.s);
    } else if constexpr (std::is_same_v<FieldType, jint>) {
      env->SetIntField(receiver, id, value.i);
    } else if constexpr (std::is_same_v<FieldType, jlong>) {
      env->SetLongField(receiver, id, value.j);
    } else if constexpr (std::is_same_v<FieldType, jfloat>) {
      env->SetFloatField(receiver, id, value.f);
    } else if constexpr (std::is_same_v<FieldType, jdouble>) {
      env->SetDoubleField(receiver, id, value.d);
    } else {
      env->SetObjectField(receiver, id, value.l);
    }
  }

  template<typename FieldType>
  void setStaticFieldValue(JNIEnv* env, const jclass clazz, const jfieldID id, const jvalue& value) {
    if constexpr (std::is_same_v<FieldType, jboolean> || std::is_same_v<FieldType, bool>) {
      env->SetStaticBooleanField(clazz, id, value.z);
    } else if constexpr (std::is_same_v<FieldType, jbyte>) {
      env->SetStaticByteField(clazz, id, value.b);
    } else if constexpr (std::is_same_v<FieldType, jchar>) {
      env->SetStaticCharField(clazz, id, value.c);
    } else if constexpr (std::is_same_v<FieldType, jshort>) {
      env->SetStaticShortField(clazz, id, value.s);
    } else if constexpr (std::is_same_v<FieldType, jint>) {
      env->SetStaticIntField(clazz, id, value.i);
    } else if constexpr (std::is_same_v<FieldType, jlong>) {
      env->SetStaticLongField(clazz, id, value.j);
    } else if constexpr (std::is_same_v<FieldType, jfloat>) {
      env->SetStaticFloatField(clazz, id, value.f);
    } else if constexpr (std::is_same_v<FieldType, jdouble>) {
      env->SetStaticDoubleField(clazz, id, value.d);
    } else {
      env->SetStaticObjectField(clazz, id, value.l);
    }
  }

  template<concepts::FunctionToken Token, typename... Args>
  jobject newRawObject(JNIEnv* env, Token, Args&&... args) {
    static_assert(
      sizeof...(Args) == std::tuple_size_v<typename Token::ArgsTuple>,
      "JNI constructor token called with the wrong number of arguments"
    );
    static_assert(
      detail::are_compatible_jni_args_v<typename Token::ArgsTuple, Args...>,
      "JNI constructor token called with an argument that does not match its declared slot type"
    );

    std::array<Ref<>, sizeof...(Args)> retained{};
    auto jargs = makeJValues(env, retained, std::forward<Args>(args)...);
    const jobject result =
        env->NewObjectA(Token::ClassType::javaClass(env), Token::id(env), jargs.data());
    checkAndThrowPending(env);
    return result;
  }

  template<concepts::FunctionToken Token, typename... Args>
  auto newObject(JNIEnv* env, Token t, Args&&... args) {
    return Ref<typename Token::ClassType>::adopt(
      env,
      newRawObject(env, t, std::forward<Args>(args)...)
    );
  }

  template<concepts::FunctionToken Token, typename... Args>
  decltype(auto) call(JNIEnv* env, jobject receiver, Token, Args&&... args) {
    static_assert(
      sizeof...(Args) == std::tuple_size_v<typename Token::ArgsTuple>,
      "JNI method token called with the wrong number of arguments"
    );
    static_assert(
      detail::are_compatible_jni_args_v<typename Token::ArgsTuple, Args...>,
      "JNI method token called with an argument that does not match its declared slot type"
    );

    if (receiver == nullptr) {
      throw std::runtime_error(
        "Cannot call " + tokenDebugDescription<Token>() + " on null receiver"
      );
    }

    std::array<Ref<>, sizeof...(Args)> retained{};
    auto jargs = makeJValues(env, retained, std::forward<Args>(args)...);
    return callMethod<typename Token::Return, Token::noThrow>(
      env,
      receiver,
      Token::id(env),
      jargs.data()
    );
  }

  template<concepts::FunctionToken Token, typename... Args>
  decltype(auto) callStatic(JNIEnv* env, Token, Args&&... args) {
    static_assert(
      sizeof...(Args) == std::tuple_size_v<typename Token::ArgsTuple>,
      "JNI static method token called with the wrong number of arguments"
    );
    static_assert(
      detail::are_compatible_jni_args_v<typename Token::ArgsTuple, Args...>,
      "JNI static method token called with an argument that does not match its declared slot type"
    );

    std::array<Ref<>, sizeof...(Args)> retained{};
    auto jargs = makeJValues(env, retained, std::forward<Args>(args)...);
    return callStaticMethod<typename Token::Return, Token::noThrow>(
      env,
      Token::ClassType::javaClass(env),
      Token::id(env),
      jargs.data()
    );
  }

  template<concepts::FieldToken Token>
  decltype(auto) get(JNIEnv* env, jobject receiver, Token) {
    if (receiver == nullptr) {
      throw std::runtime_error(
        "Cannot read " + tokenDebugDescription<Token>() + " on null receiver"
      );
    }

    return getFieldValue<typename Token::Value>(env, receiver, Token::id(env));
  }

  template<concepts::FieldToken Token, typename Value>
  void set(JNIEnv* env, jobject receiver, Token, Value&& value) {
    static_assert(
      detail::is_compatible_jni_arg_v<typename Token::Value, Value>,
      "JNI field token written with a value that does not match its declared type"
    );
    if (receiver == nullptr) {
      throw std::runtime_error(
        "Cannot write " + tokenDebugDescription<Token>() + " on null receiver"
      );
    }

    std::array<Ref<>, 1> retained{};
    jvalue jv = detail::toJValue(env, std::forward<Value>(value), retained, 0);
    setFieldValue<typename Token::Value>(env, receiver, Token::id(env), jv);
  }

  template<concepts::FieldToken Token>
  decltype(auto) getStatic(JNIEnv* env, Token) {
    return getStaticFieldValue<typename Token::Value>(
      env,
      Token::ClassType::javaClass(env),
      Token::id(env)
    );
  }

  template<concepts::FieldToken Token, typename Value>
  void setStatic(JNIEnv* env, Token, Value&& value) {
    static_assert(
      detail::is_compatible_jni_arg_v<typename Token::Value, Value>,
      "JNI static field token written with a value that does not match its declared type"
    );
    std::array<Ref<>, 1> retained{};
    jvalue jv = detail::toJValue(env, std::forward<Value>(value), retained, 0);
    setStaticFieldValue<typename Token::Value>(
      env,
      Token::ClassType::javaClass(env),
      Token::id(env),
      jv
    );
  }
}
