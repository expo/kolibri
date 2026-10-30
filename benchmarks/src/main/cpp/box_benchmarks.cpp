#include <variant>

#include <benchmark/benchmark.h>

#include <kolibri/Ref.h>
#include <kolibri/box.h>
#include <kolibri/env.h>

#include "jvm.h"

// Boxing primitives into java.lang wrappers and back. The untyped unbox() walks an instanceof
// chain (Integer, Double, Boolean, Long, Float), so Integer is its best case and Long one of its
// worst; the typed overload knows the class and makes a single call.
namespace expo::kolibri::benchmarks {
  namespace {
    void BM_BoxInt(benchmark::State& state) {
      JNIEnv* env = getEnv();
      for (auto _: state) {
        const auto boxed = Ref<>::adopt(env, box(env, jint{42}));
        benchmark::DoNotOptimize(boxed.get());
      }
    }

    // 4096 is outside the Integer cache, so every call allocates.
    void BM_BoxIntUncached(benchmark::State& state) {
      JNIEnv* env = getEnv();
      for (auto _: state) {
        const auto boxed = Ref<>::adopt(env, box(env, jint{4096}));
        benchmark::DoNotOptimize(boxed.get());
      }
    }

    void BM_BoxDouble(benchmark::State& state) {
      JNIEnv* env = getEnv();
      for (auto _: state) {
        const auto boxed = Ref<>::adopt(env, box(env, 2.5));
        benchmark::DoNotOptimize(boxed.get());
      }
    }

    void BM_UnboxUntypedInt(benchmark::State& state) {
      JNIEnv* env = getEnv();
      const auto boxed = Ref<>::adopt(env, box(env, jint{42}));
      for (auto _: state) {
        Boxed value = unbox(env, boxed.get());
        benchmark::DoNotOptimize(value);
      }
    }

    void BM_UnboxUntypedLong(benchmark::State& state) {
      JNIEnv* env = getEnv();
      const auto boxed = Ref<>::adopt(env, box(env, jlong{1} << 40));
      for (auto _: state) {
        Boxed value = unbox(env, boxed.get());
        benchmark::DoNotOptimize(value);
      }
    }

    void BM_UnboxTypedInt(benchmark::State& state) {
      JNIEnv* env = getEnv();
      const auto boxed = JInteger::valueOf(env, 42);
      for (auto _: state) {
        benchmark::DoNotOptimize(unbox(env, boxed));
      }
    }

    void BM_UnboxTypedLong(benchmark::State& state) {
      JNIEnv* env = getEnv();
      const auto boxed = JLong::valueOf(env, jlong{1} << 40);
      for (auto _: state) {
        benchmark::DoNotOptimize(unbox(env, boxed));
      }
    }
  } // namespace

  BENCHMARK(BM_BoxInt)->MinWarmUpTime(kJitWarmUpSeconds);
  BENCHMARK(BM_BoxIntUncached)->MinWarmUpTime(kJitWarmUpSeconds);
  BENCHMARK(BM_BoxDouble)->MinWarmUpTime(kJitWarmUpSeconds);
  BENCHMARK(BM_UnboxUntypedInt)->MinWarmUpTime(kJitWarmUpSeconds);
  BENCHMARK(BM_UnboxUntypedLong)->MinWarmUpTime(kJitWarmUpSeconds);
  BENCHMARK(BM_UnboxTypedInt)->MinWarmUpTime(kJitWarmUpSeconds);
  BENCHMARK(BM_UnboxTypedLong)->MinWarmUpTime(kJitWarmUpSeconds);
} // namespace expo::kolibri::benchmarks
