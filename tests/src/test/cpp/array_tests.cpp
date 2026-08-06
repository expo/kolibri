#include <numeric>
#include <span>
#include <string>
#include <vector>

#include <kolibri/Ref.h>
#include <kolibri/array.h>
#include <kolibri/class.h>
#include <kolibri/native_method.h>
#include <kolibri/string_utils.h>

#include "expect.h"
#include "test_helpers.h"

namespace expo::kolibri::tests {
  namespace {
    template<typename T, typename Fill>
    Ref<JArray<T>> makeFilled(JNIEnv* env, jsize size, Fill fill) {
      auto array = JArray<T>::create(env, size);
      std::vector<T> data(static_cast<size_t>(size));
      for (jsize i = 0; i < size; i++) {
        data[static_cast<size_t>(i)] = fill(i);
      }
      if (size > 0) {
        array->setRegion(env, 0, std::span<const T>(data));
      }
      return array;
    }

    // clang-format off
    Ref<JBooleanArray> createBooleans(JNIEnv* env, jint size) {
      return makeFilled<jboolean>(env, size, [](jsize i) -> jboolean { return i % 2 != 0 ? JNI_TRUE : JNI_FALSE; });
    }
    Ref<JByteArray> createBytes(JNIEnv* env, jint size) {
      return makeFilled<jbyte>(env, size, [](jsize i) { return static_cast<jbyte>(i * 2); });
    }
    Ref<JCharArray> createChars(JNIEnv* env, jint size) {
      return makeFilled<jchar>(env, size, [](jsize i) { return static_cast<jchar>(u'a' + i); });
    }
    Ref<JShortArray> createShorts(JNIEnv* env, jint size) {
      return makeFilled<jshort>(env, size, [](jsize i) { return static_cast<jshort>(i * 3); });
    }
    Ref<JIntArray> createInts(JNIEnv* env, jint size) {
      return makeFilled<jint>(env, size, [](jsize i) { return static_cast<jint>(i * 4); });
    }
    Ref<JLongArray> createLongs(JNIEnv* env, jint size) {
      return makeFilled<jlong>(env, size, [](jsize i) { return static_cast<jlong>(i * 5); });
    }
    Ref<JFloatArray> createFloats(JNIEnv* env, jint size) {
      return makeFilled<jfloat>(env, size, [](jsize i) { return static_cast<jfloat>(i) * 0.5f; });
    }
    Ref<JDoubleArray> createDoubles(JNIEnv* env, jint size) {
      return makeFilled<jdouble>(env, size, [](jsize i) { return static_cast<jdouble>(i) * 0.25; });
    }
    // clang-format on

    jlong sumInts(JNIEnv* env, jintArray values) {
      const UnownedRef<JIntArray> array{values};
      const std::vector<jint> data = array->toVector(env);
      return std::accumulate(data.begin(), data.end(), jlong{0});
    }

    jdouble sumDoubles(JNIEnv* env, jdoubleArray values) {
      const UnownedRef<JDoubleArray> array{values};
      const auto pinned = array->pinReadOnly(env);
      jdouble sum = 0;
      for (const jdouble value: pinned.span()) {
        sum += value;
      }
      return sum;
    }

    jint countTrue(JNIEnv* env, jbooleanArray values) {
      const UnownedRef<JBooleanArray> array{values};
      const auto pinned = array->pinReadOnly(env);
      jint count = 0;
      for (jsize i = 0; i < pinned.size(); i++) {
        if (pinned[i] == JNI_TRUE) {
          count++;
        }
      }
      return count;
    }

    jboolean regionRoundTrip(JNIEnv* env, jintArray values) {
      const UnownedRef<JIntArray> array{values};
      KOLIBRI_EXPECT_EQ(array->size(env), 8);

      std::vector<jint> middle(4);
      array->getRegion(env, 2, std::span<jint>(middle));
      KOLIBRI_EXPECT_EQ(middle[0], 2);
      KOLIBRI_EXPECT_EQ(middle[3], 5);

      std::vector<jint> reversed(middle.rbegin(), middle.rend());
      array->setRegion(env, 4, std::span<const jint>(reversed));
      return JNI_TRUE;
    }

    jboolean pinMutate(JNIEnv* env, jintArray values) {
      const UnownedRef<JIntArray> array{values};
      auto pinned = array->pin(env);
      KOLIBRI_EXPECT_EQ(pinned.size(), 5);
      for (jint& value: pinned.span()) {
        value += 100;
      }
      pinned.commit();
      pinned.release();
      return JNI_TRUE;
    }

    jboolean zeroLengthArrays(JNIEnv* env) {
      const auto empty = JIntArray::create(env, 0);
      KOLIBRI_EXPECT_EQ(empty->size(env), 0);
      KOLIBRI_EXPECT(empty->toVector(env).empty());
      {
        const auto pinned = empty->pinReadOnly(env);
        KOLIBRI_EXPECT_EQ(pinned.size(), 0);
        KOLIBRI_EXPECT(pinned.span().empty());
      }

      const auto fromEmptySpan = JDoubleArray::create(env, std::span<const jdouble>{});
      KOLIBRI_EXPECT_EQ(fromEmptySpan->size(env), 0);

      const auto emptyObjects = JArray<jstring>::create(env, 0);
      KOLIBRI_EXPECT_EQ(emptyObjects->size(env), 0);
      return JNI_TRUE;
    }

    Ref<JArray<jstring>> makeStringArray(JNIEnv* env) {
      auto array = JArray<jstring>::create(env, 3);
      const auto first = Ref<jstring>::adopt(env, toJString(env, std::string("first")));
      const auto third = Ref<jstring>::adopt(env, toJString(env, std::string("third")));
      array->setElement(env, 0, first.get());
      array->setElement(env, 2, third.get());

      const auto readBack = array->getElement(env, 0);
      KOLIBRI_EXPECT_EQ(toStdString(env, reinterpret_cast<jstring>(readBack.get())),
                        std::string("first"));
      KOLIBRI_EXPECT(!array->getElement(env, 1));
      return array;
    }

    UnownedRef<JString> joinStringArray(JNIEnv* env, UnownedRef<JArray<jstring>> array) {
      std::string joined;
      const jsize size = array->size(env);
      for (jsize i = 0; i < size; i++) {
        const auto element = array->getElement(env, i);
        if (!element) {
          continue;
        }
        if (!joined.empty()) {
          joined += ',';
        }
        joined += toStdString(env, reinterpret_cast<jstring>(element.get()));
      }
      return toJString(env, joined);
    }
  } // namespace

  void registerArrayTests(JNIEnv* env) {
    registerNative(env, "io/github/expo/kolibri/tests/ArrayTests")
      .method<&createBooleans>("nativeCreateBooleanArray")
      .method<&createBytes>("nativeCreateByteArray")
      .method<&createChars>("nativeCreateCharArray")
      .method<&createShorts>("nativeCreateShortArray")
      .method<&createInts>("nativeCreateIntArray")
      .method<&createLongs>("nativeCreateLongArray")
      .method<&createFloats>("nativeCreateFloatArray")
      .method<&createDoubles>("nativeCreateDoubleArray")
      .method<&sumInts>("nativeSumIntArray")
      .method<&sumDoubles>("nativeSumDoubleArray")
      .method<&countTrue>("nativeCountTrue")
      .method<&regionRoundTrip>("nativeRegionRoundTrip")
      .method<&pinMutate>("nativePinMutate")
      .method<&zeroLengthArrays>("nativeZeroLengthArrays")
      .method<&makeStringArray>("nativeMakeStringArray")
      .method<
        &joinStringArray
      >("nativeJoinStringArray")
      .commit();
  }
}
