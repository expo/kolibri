#pragma once

#include <jni.h>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

#include <kolibri/JavaClass.h>
#include <kolibri/Ref.h>
#include <kolibri/exception.h>
#include <kolibri/signature.h>
#include <kolibri/meta/meta.h>
#include <kolibri/class.h>

namespace expo::kolibri {
  namespace detail {
    template<typename T>
    struct jni_array_traits;

    // clang-format off
    template<> struct jni_array_traits<jboolean> {
      using array_type = jbooleanArray;
      static constexpr auto newArray        = &JNIEnv::NewBooleanArray;
      static constexpr auto getElements     = &JNIEnv::GetBooleanArrayElements;
      static constexpr auto releaseElements = &JNIEnv::ReleaseBooleanArrayElements;
      static constexpr auto getRegion       = &JNIEnv::GetBooleanArrayRegion;
      static constexpr auto setRegion       = &JNIEnv::SetBooleanArrayRegion;
    };

    template<> struct jni_array_traits<jbyte> {
      using array_type = jbyteArray;
      static constexpr auto newArray        = &JNIEnv::NewByteArray;
      static constexpr auto getElements     = &JNIEnv::GetByteArrayElements;
      static constexpr auto releaseElements = &JNIEnv::ReleaseByteArrayElements;
      static constexpr auto getRegion       = &JNIEnv::GetByteArrayRegion;
      static constexpr auto setRegion       = &JNIEnv::SetByteArrayRegion;
    };

    template<> struct jni_array_traits<jchar> {
      using array_type = jcharArray;
      static constexpr auto newArray        = &JNIEnv::NewCharArray;
      static constexpr auto getElements     = &JNIEnv::GetCharArrayElements;
      static constexpr auto releaseElements = &JNIEnv::ReleaseCharArrayElements;
      static constexpr auto getRegion       = &JNIEnv::GetCharArrayRegion;
      static constexpr auto setRegion       = &JNIEnv::SetCharArrayRegion;
    };

    template<> struct jni_array_traits<jshort> {
      using array_type = jshortArray;
      static constexpr auto newArray        = &JNIEnv::NewShortArray;
      static constexpr auto getElements     = &JNIEnv::GetShortArrayElements;
      static constexpr auto releaseElements = &JNIEnv::ReleaseShortArrayElements;
      static constexpr auto getRegion       = &JNIEnv::GetShortArrayRegion;
      static constexpr auto setRegion       = &JNIEnv::SetShortArrayRegion;
    };

    template<> struct jni_array_traits<jint> {
      using array_type = jintArray;
      static constexpr auto newArray        = &JNIEnv::NewIntArray;
      static constexpr auto getElements     = &JNIEnv::GetIntArrayElements;
      static constexpr auto releaseElements = &JNIEnv::ReleaseIntArrayElements;
      static constexpr auto getRegion       = &JNIEnv::GetIntArrayRegion;
      static constexpr auto setRegion       = &JNIEnv::SetIntArrayRegion;
    };

    template<> struct jni_array_traits<jlong> {
      using array_type = jlongArray;
      static constexpr auto newArray        = &JNIEnv::NewLongArray;
      static constexpr auto getElements     = &JNIEnv::GetLongArrayElements;
      static constexpr auto releaseElements = &JNIEnv::ReleaseLongArrayElements;
      static constexpr auto getRegion       = &JNIEnv::GetLongArrayRegion;
      static constexpr auto setRegion       = &JNIEnv::SetLongArrayRegion;
    };

    template<> struct jni_array_traits<jfloat> {
      using array_type = jfloatArray;
      static constexpr auto newArray        = &JNIEnv::NewFloatArray;
      static constexpr auto getElements     = &JNIEnv::GetFloatArrayElements;
      static constexpr auto releaseElements = &JNIEnv::ReleaseFloatArrayElements;
      static constexpr auto getRegion       = &JNIEnv::GetFloatArrayRegion;
      static constexpr auto setRegion       = &JNIEnv::SetFloatArrayRegion;
    };

    template<> struct jni_array_traits<jdouble> {
      using array_type = jdoubleArray;
      static constexpr auto newArray        = &JNIEnv::NewDoubleArray;
      static constexpr auto getElements     = &JNIEnv::GetDoubleArrayElements;
      static constexpr auto releaseElements = &JNIEnv::ReleaseDoubleArrayElements;
      static constexpr auto getRegion       = &JNIEnv::GetDoubleArrayRegion;
      static constexpr auto setRegion       = &JNIEnv::SetDoubleArrayRegion;
    };
    // clang-format on

    template<typename T>
    concept jni_primitive_array_element = requires { typename jni_array_traits<T>::array_type; };

    template<typename T>
    concept jni_object_array_element = has_descriptor<T> || std::is_same_v<T, jstring>;

    template<typename Element>
    struct jni_object_array_traits {
      static jclass javaClass(JNIEnv* env) {
        return Element::javaClass(env);
      }
    };

    template<>
    struct jni_object_array_traits<jstring> {
      static jclass javaClass(JNIEnv* env) {
        static const jclass clazz = JString::javaClass(env);
        return clazz;
      }
    };
  }

  template<typename Element>
  class PinnedArray {
    using Value = std::remove_const_t<Element>;
    using Traits = detail::jni_array_traits<Value>;
    using JniType = Traits::array_type;

  public:
    static constexpr bool ReadOnly = std::is_const_v<Element>;

    PinnedArray(JNIEnv* env, JniType array)
      : env_(env),
        array_(static_cast<JniType>(env->NewLocalRef(array))),
        size_(env->GetArrayLength(array)) {
      if (array_ == nullptr) {
        throwWithPending(env, "Could not reference a Java array for pinning");
      }
      elements_ = (env->*Traits::getElements)(array_, nullptr);
      if (elements_ == nullptr && size_ != 0) {
        env->DeleteLocalRef(array_);
        array_ = nullptr;
        throwWithPending(env, "Could not pin the elements of a Java array");
      }
    }

    ~PinnedArray() {
      unpin(ReadOnly ? JNI_ABORT : 0);
      if (array_ != nullptr) {
        env_->DeleteLocalRef(array_);
      }
    }

    PinnedArray(PinnedArray&& other) noexcept
      : env_(other.env_), array_(other.array_), elements_(other.elements_), size_(other.size_) {
      other.elements_ = nullptr;
      other.array_ = nullptr;
    }

    PinnedArray& operator=(PinnedArray&& other) noexcept {
      if (this != &other) {
        unpin(ReadOnly ? JNI_ABORT : 0);
        if (array_ != nullptr) {
          env_->DeleteLocalRef(array_);
        }
        env_ = other.env_;
        array_ = other.array_;
        elements_ = other.elements_;
        size_ = other.size_;
        other.elements_ = nullptr;
        other.array_ = nullptr;
      }
      return *this;
    }

    PinnedArray(const PinnedArray&) = delete;

    PinnedArray& operator=(const PinnedArray&) = delete;

    [[nodiscard]] Element* data() const { return elements_; }
    [[nodiscard]] jsize size() const { return size_; }
    Element& operator[](jsize index) const { return elements_[index]; }
    [[nodiscard]] Element* begin() const { return elements_; }
    [[nodiscard]] Element* end() const { return elements_ + size_; }

    [[nodiscard]] std::span<Element> span() const {
      return {elements_, static_cast<std::size_t>(size_)};
    }

    /** Flushes writes back to the Java array and keeps the pin alive (`JNI_COMMIT`). */
    void commit() requires (!ReadOnly) {
      if (elements_) {
        (env_->*Traits::releaseElements)(array_, elements_, JNI_COMMIT);
      }
    }

    /** Unpins now, discarding writes that were not committed (`JNI_ABORT`). */
    void abort() {
      unpin(JNI_ABORT);
    }

    /** Unpins now, flushing writes back to the Java array (mode `0`, the destructor's default). */
    void release() requires (!ReadOnly) {
      unpin(0);
    }

  private:
    void unpin(jint mode) {
      if (elements_) {
        (env_->*Traits::releaseElements)(array_, elements_, mode);
        elements_ = nullptr;
      }
    }

    JNIEnv* env_;
    JniType array_;
    Value* elements_;
    jsize size_;
  };

  template<typename Element>
  struct JArray;

  template<detail::jni_primitive_array_element Element>
  struct JArray<Element> : JavaClass<JArray<Element>> {
    using Traits = detail::jni_array_traits<Element>;
    using JniType = Traits::array_type;

  private:
    static constexpr auto storage = meta::wrap<jni_descriptor_v<Element>, "[", "">();

  public:
    static constexpr std::string_view descriptor{storage.data(), storage.size()};

    static Ref<JArray> create(JNIEnv* env, jsize size) {
      JniType array = (env->*Traits::newArray)(size);
      if (array == nullptr) {
        throwWithPending(env, "Could not create a Java array " + std::string(descriptor));
      }
      return Ref<JArray>::adopt(env, array);
    }

    static Ref<JArray> create(JNIEnv* env, std::span<const Element> data) {
      Ref<JArray> array = create(env, static_cast<jsize>(data.size()));
      if (!data.empty()) {
        (env->*Traits::setRegion)(
          reinterpret_cast<JniType>(array.get()),
          0,
          static_cast<jsize>(data.size()),
          data.data()
        );
      }
      return array;
    }

    static JniType createRaw(JNIEnv* env, std::span<const Element> data) {
      JniType array = (env->*Traits::newArray)(data.size());
      if (array == nullptr) {
        throwWithPending(env, "Could not create a Java array " + std::string(descriptor));
      }

      if (!data.empty()) {
        (env->*Traits::setRegion)(
          array,
          0,
          static_cast<jsize>(data.size()),
          data.data()
        );
      }

      return array;
    }

    struct Accessors : JavaClass<JArray>::BaseAccessors {
      [[nodiscard]] jsize size(JNIEnv* env) const {
        return env->GetArrayLength(handle());
      }

      /** Copies `out.size()` elements starting at `start` out of the array. */
      void getRegion(JNIEnv* env, jsize start, std::span<Element> out) const {
        (env->*Traits::getRegion)(handle(), start, static_cast<jsize>(out.size()), out.data());
        checkAndThrowPending(env);
      }

      /** Copies `data` into the array starting at `start`. */
      void setRegion(JNIEnv* env, jsize start, std::span<const Element> data) const {
        (env->*Traits::setRegion)(handle(), start, static_cast<jsize>(data.size()), data.data());
        checkAndThrowPending(env);
      }

      [[nodiscard]] std::vector<Element> toVector(JNIEnv* env) const {
        std::vector<Element> out(static_cast<std::size_t>(size(env)));
        if (!out.empty()) {
          getRegion(env, 0, std::span<Element>(out));
        }
        return out;
      }

      [[nodiscard]] PinnedArray<Element> pin(JNIEnv* env) const {
        return PinnedArray<Element>(env, handle());
      }

      [[nodiscard]] PinnedArray<const Element> pinReadOnly(JNIEnv* env) const {
        return PinnedArray<const Element>(env, handle());
      }

    private:
      JniType handle() const {
        if (this->handle_ == nullptr) {
          throw std::runtime_error(
            "Cannot access a null Java array " + std::string(descriptor)
          );
        }
        return reinterpret_cast<JniType>(this->handle_);
      }
    };
  };

  template<detail::jni_object_array_element Element>
  struct JArray<Element> : JavaClass<JArray<Element>> {
    using JniType = jobjectArray;

  private:
    static constexpr auto storage = meta::wrap<jni_descriptor_v<Element>, "[", "">();

  public:
    static constexpr std::string_view descriptor{storage.data(), storage.size()};

    static JniType createRaw(JNIEnv* env, jsize size, jobject initial = nullptr) {
      const jobjectArray array = env->NewObjectArray(
        size,
        detail::jni_object_array_traits<Element>::javaClass(env),
        initial
      );
      if (array == nullptr) {
        throwWithPending(env, "Could not create a Java array " + std::string(descriptor));
      }
      return array;
    }

    static Ref<JArray> create(JNIEnv* env, jsize size, jobject initial = nullptr) {
      return Ref<JArray>::adopt(env, createRaw(env, size, initial));
    }

    struct Accessors : JavaClass<JArray>::BaseAccessors {
      [[nodiscard]] jsize size(JNIEnv* env) const {
        return env->GetArrayLength(handle());
      }

      [[nodiscard]] Ref<Element> getElement(JNIEnv* env, jsize index) const {
        jobject value = env->GetObjectArrayElement(handle(), index);
        checkAndThrowPending(env);
        return Ref<Element>::adopt(env, value);
      }

      [[nodiscard]] Ref<Element> getUnsafeElement(JNIEnv* env, jsize index) const {
        jobject value = env->GetObjectArrayElement(handle(), index);
        return Ref<Element>::adopt(env, value);
      }

      void setElement(JNIEnv* env, jsize index, jobject value) const {
        env->SetObjectArrayElement(handle(), index, value);
        checkAndThrowPending(env);
      }

    private:
      JniType handle() const {
        if (this->handle_ == nullptr) {
          throw std::runtime_error(
            "Cannot access a null Java array " + std::string(descriptor)
          );
        }
        return reinterpret_cast<JniType>(this->handle_);
      }
    };
  };

  using JBooleanArray = JArray<jboolean>;
  using JByteArray = JArray<jbyte>;
  using JCharArray = JArray<jchar>;
  using JShortArray = JArray<jshort>;
  using JIntArray = JArray<jint>;
  using JLongArray = JArray<jlong>;
  using JFloatArray = JArray<jfloat>;
  using JDoubleArray = JArray<jdouble>;
  using JObjectArray = JArray<JObject>;
  using JStringArray = JArray<JString>;
} // namespace expo::kolibri
