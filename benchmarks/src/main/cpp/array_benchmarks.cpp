#include <cstdint>
#include <numeric>
#include <span>
#include <vector>

#include <benchmark/benchmark.h>

#include <kolibri/array.h>
#include <kolibri/box.h>
#include <kolibri/env.h>

// The ways kolibri moves a primitive array across JNI: bulk copies (create / toVector), pinning,
// and the chunked visitor. Parameterized by element count; reports elements/s.
namespace expo::kolibri::benchmarks {
  namespace {
    std::vector<jint> makeInts(size_t count) {
      std::vector<jint> values(count);
      std::iota(values.begin(), values.end(), 0);
      return values;
    }

    Ref<JIntArray> makeIntArray(JNIEnv* env, size_t count) {
      const std::vector<jint> values = makeInts(count);
      return JIntArray::create(env, std::span<const jint>(values));
    }

    void setItems(benchmark::State& state) {
      state.SetItemsProcessed(state.iterations() * state.range(0));
    }

    void BM_IntArrayCreate(benchmark::State& state) {
      JNIEnv* env = getEnv();
      const std::vector<jint> values = makeInts(static_cast<size_t>(state.range(0)));
      for (auto _: state) {
        const auto array = JIntArray::create(env, std::span<const jint>(values));
        benchmark::DoNotOptimize(array.get());
      }
      setItems(state);
    }

    void BM_IntArrayCreateFromProducer(benchmark::State& state) {
      JNIEnv* env = getEnv();
      const auto count = static_cast<jsize>(state.range(0));
      for (auto _: state) {
        const auto array = JIntArray::create(env, count, [](jsize i) { return static_cast<jint>(i); });
        benchmark::DoNotOptimize(array.get());
      }
      setItems(state);
    }

    void BM_IntArrayToVector(benchmark::State& state) {
      JNIEnv* env = getEnv();
      const auto array = makeIntArray(env, static_cast<size_t>(state.range(0)));
      for (auto _: state) {
        std::vector<jint> values = array->toVector(env);
        benchmark::DoNotOptimize(values.data());
      }
      setItems(state);
    }

    void BM_IntArraySumPinned(benchmark::State& state) {
      JNIEnv* env = getEnv();
      const auto array = makeIntArray(env, static_cast<size_t>(state.range(0)));
      for (auto _: state) {
        const auto pinned = array->pinReadOnly(env);
        int64_t sum = 0;
        for (const jint value: pinned.span()) {
          sum += value;
        }
        benchmark::DoNotOptimize(sum);
      }
      setItems(state);
    }

    void BM_IntArraySumChunked(benchmark::State& state) {
      JNIEnv* env = getEnv();
      const auto array = makeIntArray(env, static_cast<size_t>(state.range(0)));
      for (auto _: state) {
        int64_t sum = 0;
        array->forEachChunk(env, [&sum](jsize, std::span<const jint> chunk) {
          for (const jint value: chunk) {
            sum += value;
          }
        });
        benchmark::DoNotOptimize(sum);
      }
      setItems(state);
    }

    // Baseline for the read benchmarks: one GetIntArrayRegion into a buffer allocated up front.
    void BM_RawGetIntArrayRegion(benchmark::State& state) {
      JNIEnv* env = getEnv();
      const auto count = static_cast<jsize>(state.range(0));
      const auto array = makeIntArray(env, static_cast<size_t>(count));
      std::vector<jint> out(static_cast<size_t>(count));
      for (auto _: state) {
        env->GetIntArrayRegion(static_cast<jintArray>(array.get()), 0, count, out.data());
        benchmark::DoNotOptimize(out.data());
        benchmark::ClobberMemory();
      }
      setItems(state);
    }

    // Object arrays go element by element: a call per element, and a local ref per read.
    void BM_ObjectArrayGetElement(benchmark::State& state) {
      JNIEnv* env = getEnv();
      const auto array = JObjectArray::create(env, 16, JInteger::valueOf(env, 42).get());
      jsize index = 0;
      for (auto _: state) {
        const auto element = array->getElement(env, index);
        benchmark::DoNotOptimize(element.get());
        index = (index + 1) & 15;
      }
    }

    void BM_RawGetObjectArrayElement(benchmark::State& state) {
      JNIEnv* env = getEnv();
      const auto array = JObjectArray::create(env, 16, JInteger::valueOf(env, 42).get());
      const auto raw = static_cast<jobjectArray>(array.get());
      jsize index = 0;
      for (auto _: state) {
        jobject element = env->GetObjectArrayElement(raw, index);
        if (env->ExceptionCheck()) {
          state.SkipWithError("GetObjectArrayElement threw");
          return;
        }
        benchmark::DoNotOptimize(element);
        env->DeleteLocalRef(element);
        index = (index + 1) & 15;
      }
    }

    void BM_ObjectArraySetElement(benchmark::State& state) {
      JNIEnv* env = getEnv();
      const auto array = JObjectArray::create(env, 16);
      const auto value = JInteger::valueOf(env, 42);
      jsize index = 0;
      for (auto _: state) {
        array->setElement(env, index, value);
        index = (index + 1) & 15;
      }
    }

    void BM_RawSetObjectArrayElement(benchmark::State& state) {
      JNIEnv* env = getEnv();
      const auto array = JObjectArray::create(env, 16);
      const auto raw = static_cast<jobjectArray>(array.get());
      const auto value = JInteger::valueOf(env, 42);
      jsize index = 0;
      for (auto _: state) {
        env->SetObjectArrayElement(raw, index, value);
        if (env->ExceptionCheck()) {
          state.SkipWithError("SetObjectArrayElement threw");
          return;
        }
        index = (index + 1) & 15;
      }
    }
  } // namespace

  BENCHMARK(BM_IntArrayCreate)->RangeMultiplier(16)->Range(16, 64 << 10);
  BENCHMARK(BM_IntArrayCreateFromProducer)->RangeMultiplier(16)->Range(16, 64 << 10);
  BENCHMARK(BM_IntArrayToVector)->RangeMultiplier(16)->Range(16, 64 << 10);
  BENCHMARK(BM_IntArraySumPinned)->RangeMultiplier(16)->Range(16, 64 << 10);
  BENCHMARK(BM_IntArraySumChunked)->RangeMultiplier(16)->Range(16, 64 << 10);
  BENCHMARK(BM_RawGetIntArrayRegion)->RangeMultiplier(16)->Range(16, 64 << 10);
  BENCHMARK(BM_ObjectArrayGetElement);
  BENCHMARK(BM_RawGetObjectArrayElement);
  BENCHMARK(BM_ObjectArraySetElement);
  BENCHMARK(BM_RawSetObjectArrayElement);
} // namespace expo::kolibri::benchmarks
