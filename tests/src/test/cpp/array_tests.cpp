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
#include "../../../../runtime/src/main/cpp/kolibri/array.h"

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

    // `values` is 0..9: a 4-element chunk size gives two full chunks and a 2-element tail.
    jboolean chunkedRead(JNIEnv* env, jintArray values) {
      const UnownedRef<JIntArray> array{values};
      KOLIBRI_EXPECT_EQ(array->size(env), 10);

      std::vector<jsize> starts;
      std::vector<std::size_t> sizes;
      jlong sum = 0;
      array->forEachChunk<4>(env, [&](jsize start, std::span<const jint> chunk) {
        starts.push_back(start);
        sizes.push_back(chunk.size());
        for (const jint value: chunk) {
          sum += value;
        }
      });
      KOLIBRI_EXPECT_EQ(starts.size(), std::size_t{3});
      KOLIBRI_EXPECT_EQ(starts[0], 0);
      KOLIBRI_EXPECT_EQ(starts[1], 4);
      KOLIBRI_EXPECT_EQ(starts[2], 8);
      KOLIBRI_EXPECT_EQ(sizes[0], std::size_t{4});
      KOLIBRI_EXPECT_EQ(sizes[1], std::size_t{4});
      KOLIBRI_EXPECT_EQ(sizes[2], std::size_t{2});
      KOLIBRI_EXPECT_EQ(sum, jlong{45});

      jsize expectedIndex = 0;
      array->forEach<4>(env, [&](jsize index, jint value) {
        KOLIBRI_EXPECT_EQ(index, expectedIndex);
        KOLIBRI_EXPECT_EQ(value, index);
        expectedIndex++;
      });
      KOLIBRI_EXPECT_EQ(expectedIndex, 10);
      return JNI_TRUE;
    }

    // Default chunk size: the Kotlin side passes more than kArrayChunkSize elements.
    jdouble chunkedSumDoubles(JNIEnv* env, jdoubleArray values) {
      const UnownedRef<JDoubleArray> array{values};
      jdouble sum = 0;
      jsize chunks = 0;
      array->forEachChunk(env, [&](jsize, std::span<const jdouble> chunk) {
        KOLIBRI_EXPECT(chunk.size() <= detail::kArrayChunkSize);
        chunks++;
        for (const jdouble value: chunk) {
          sum += value;
        }
      });
      const jsize size = array->size(env);
      KOLIBRI_EXPECT_EQ(chunks, static_cast<jsize>((size + detail::kArrayChunkSize - 1) / detail::kArrayChunkSize));
      return sum;
    }

    jboolean fillByChunk(JNIEnv* env, jintArray values) {
      const UnownedRef<JIntArray> array{values};
      array->fill<4>(env, [](jsize start, std::span<jint> chunk) {
        for (std::size_t i = 0; i < chunk.size(); i++) {
          chunk[i] = (start + static_cast<jsize>(i)) * 10;
        }
      });
      return JNI_TRUE;
    }

    jboolean fillByIndex(JNIEnv* env, jlongArray values) {
      const UnownedRef<JLongArray> array{values};
      // Six elements over a 3-element chunk: two full chunks, no tail.
      array->fill<3>(env, [](jsize index) { return jlong{index} * index; });
      return JNI_TRUE;
    }

    Ref<JDoubleArray> createByChunk(JNIEnv* env, jint size) {
      return JDoubleArray::create<4>(env, size, [](jsize start, std::span<jdouble> chunk) {
        for (std::size_t i = 0; i < chunk.size(); i++) {
          chunk[i] = static_cast<jdouble>(start) + static_cast<jdouble>(i) + 0.5;
        }
      });
    }

    jshortArray createRawByIndex(JNIEnv* env, jint size) {
      return JShortArray::createRaw(env, size, [](jsize index) {
        return static_cast<jshort>(-index);
      });
    }

    Ref<JBooleanArray> createBooleansByIndex(JNIEnv* env, jint size) {
      // A `bool` result converts to jboolean.
      return JBooleanArray::create(env, size, [](jsize index) { return index % 3 == 0; });
    }

    jboolean zeroLengthArrays(JNIEnv* env) {
      const auto empty = JIntArray::create(env, 0);
      KOLIBRI_EXPECT_EQ(empty->size(env), 0);
      KOLIBRI_EXPECT(empty->toVector(env).empty());
      empty->forEachChunk(env, [](jsize, std::span<const jint>) {
        KOLIBRI_EXPECT(false);
      });
      empty->fill(env, [](jsize) -> jint {
        KOLIBRI_EXPECT(false);
        return 0;
      });
      const auto emptyProduced = JIntArray::create(env, 0, [](jsize, std::span<jint>) {
        KOLIBRI_EXPECT(false);
      });
      KOLIBRI_EXPECT_EQ(emptyProduced->size(env), 0);
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
      .method<&chunkedRead>("nativeChunkedRead")
      .method<&chunkedSumDoubles>("nativeChunkedSumDoubles")
      .method<&fillByChunk>("nativeFillByChunk")
      .method<&fillByIndex>("nativeFillByIndex")
      .method<&createByChunk>("nativeCreateByChunk")
      .method<&createRawByIndex>("nativeCreateRawByIndex")
      .method<&createBooleansByIndex>("nativeCreateBooleansByIndex")
      .method<&zeroLengthArrays>("nativeZeroLengthArrays")
      .method<&makeStringArray>("nativeMakeStringArray")
      .method<
        &joinStringArray
      >("nativeJoinStringArray")
      .commit();
  }
}
