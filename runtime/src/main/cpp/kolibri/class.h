#pragma once

#include <jni.h>
#include <string_view>
#include <utility>

#include <kolibri/members/Constructor.h>
#include <kolibri/JavaClass.h>
#include <kolibri/members/Method.h>
#include <kolibri/meta/CTString.h>
#include <kolibri/Ref.h>

namespace expo::kolibri {
  struct JUnit : JavaClass<JUnit> {
    static constexpr std::string_view descriptor = "kotlin/Unit";
    static constexpr StaticField<"INSTANCE", Ref<JUnit>> instance{};
  };

  struct JClass : JavaClass<JClass> {
    static constexpr std::string_view descriptor = "java/lang/Class";
  };

  struct JObject : JavaClass<JObject> {
    static constexpr std::string_view descriptor = "java/lang/Object";
  };

  struct JString : JavaClass<JString> {
    static constexpr std::string_view descriptor = "java/lang/String";

    struct Accessors : BaseAccessors {
      std::string toStdString(JNIEnv* env) const {
        return kolibri::toStdString(env, static_cast<jstring>(handle_));
      }
    };
  };

  struct JByteBuffer : JavaClass<JByteBuffer> {
    static constexpr std::string_view descriptor = "java/nio/ByteBuffer";
  };

  struct JIterator : JavaClass<JIterator> {
    static constexpr std::string_view descriptor = "java/util/Iterator";
    static constexpr Method<"hasNext", jboolean() noexcept> hasNext{};
    static constexpr Method<"next", Ref<>()> next{};

    struct Accessors : BaseAccessors {
      JAVA_FORWARD_ACCESSOR(hasNext)
      JAVA_FORWARD_ACCESSOR(next)
    };
  };

  struct JCollection : JavaClass<JCollection> {
    static constexpr std::string_view descriptor = "java/util/Collection";
    static constexpr Method<"size", jint() noexcept> size{};
    static constexpr Method<"add", jboolean(jobject)> add{};
    static constexpr Method<"iterator", Ref<JIterator>()> iterator{};

    struct Accessors : BaseAccessors {
      JAVA_FORWARD_ACCESSOR(size)
      JAVA_FORWARD_ACCESSOR(add)
      JAVA_FORWARD_ACCESSOR(iterator)
    };
  };

  struct JList : JavaClass<JList, JCollection> {
    static constexpr std::string_view descriptor = "java/util/List";
    static constexpr Method<"get", Ref<jobject>(jint)> get{};

    struct Accessors : BaseAccessors {
      JAVA_FORWARD_ACCESSOR(get)
    };
  };

  struct JArrayList : JavaClass<JArrayList, JList> {
    static constexpr std::string_view descriptor = "java/util/ArrayList";
    static constexpr Constructor<void(jint)> constructor{};
    static constexpr Method<"add", jboolean(jobject)> add{};
    static constexpr Method<"size", jint() noexcept> size{};
    static constexpr Method<"get", Ref<>(jint)> get{};

    struct Accessors : BaseAccessors {
      JAVA_FORWARD_ACCESSOR(add)
      JAVA_FORWARD_ACCESSOR(size)
      JAVA_FORWARD_ACCESSOR(get)
    };
  };

  // `java.util.Set` extends `Collection`. All instance methods are inherited from `JCollection`.
  struct JSet : JavaClass<JSet, JCollection> {
    static constexpr std::string_view descriptor = "java/util/Set";

    struct Accessors : BaseAccessors {
    };
  };

  struct JMap : JavaClass<JMap> {
    static constexpr std::string_view descriptor = "java/util/Map";
    static constexpr Method<"entrySet", Ref<JSet>()> entrySet{};
    static constexpr Method<"get", Ref<>(jobject)> get{};
    static constexpr Method<"size", jint() noexcept> size{};

    struct Accessors : BaseAccessors {
      JAVA_FORWARD_ACCESSOR(entrySet)
      JAVA_FORWARD_ACCESSOR(get)
      JAVA_FORWARD_ACCESSOR(size)
    };
  };

  struct JHashMap : JavaClass<JHashMap> {
    static constexpr std::string_view descriptor = "java/util/HashMap";
    static constexpr Constructor<void()> constructor{};
    static constexpr Method<"put", Ref<>(jobject, jobject)> put{};
    static constexpr Method<"get", Ref<>(jobject)> get{};
    static constexpr Method<"size", jint() noexcept> size{};
    static constexpr Method<"entrySet", Ref<JSet>()> entrySet{};

    struct Accessors : BaseAccessors {
      JAVA_FORWARD_ACCESSOR(put)
      JAVA_FORWARD_ACCESSOR(get)
      JAVA_FORWARD_ACCESSOR(size)
      JAVA_FORWARD_ACCESSOR(entrySet)
    };
  };

  struct JMapEntry : JavaClass<JMapEntry> {
    static constexpr std::string_view descriptor = "java/util/Map$Entry";
    static constexpr Method<"getKey", Ref<>()> getKey{};
    static constexpr Method<"getValue", Ref<>()> getValue{};

    struct Accessors : BaseAccessors {
      JAVA_FORWARD_ACCESSOR(getKey)
      JAVA_FORWARD_ACCESSOR(getValue)
    };
  };
} // namespace expo::kolibri
