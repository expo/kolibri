#include <benchmark/benchmark.h>

#include <kolibri/LocalFrame.h>
#include <kolibri/Ref.h>
#include <kolibri/box.h>
#include <kolibri/env.h>

#include "jvm.h"

// The per-call plumbing every binding pays: finding the JNIEnv, owning refs, cached class lookups.
// Each kolibri benchmark is paired with the raw JNI it wraps.
namespace expo::kolibri::benchmarks {
  namespace {
    void BM_GetEnv(benchmark::State& state) {
      for (auto _: state) {
        benchmark::DoNotOptimize(getEnv());
      }
    }

    void BM_RawGetEnv(benchmark::State& state) {
      JavaVM* vm = benchmarkVm();
      for (auto _: state) {
        JNIEnv* env = nullptr;
        vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6);
        benchmark::DoNotOptimize(env);
      }
    }

    void BM_LocalRef(benchmark::State& state) {
      JNIEnv* env = getEnv();
      const auto object = JInteger::valueOf(env, 42);
      for (auto _: state) {
        const auto ref = Ref<>::make(env, object);
        benchmark::DoNotOptimize(ref.get());
      }
    }

    void BM_RawNewLocalRef(benchmark::State& state) {
      JNIEnv* env = getEnv();
      const auto object = JInteger::valueOf(env, 42);
      for (auto _: state) {
        jobject ref = env->NewLocalRef(object);
        benchmark::DoNotOptimize(ref);
        env->DeleteLocalRef(ref);
      }
    }

    void BM_GlobalRef(benchmark::State& state) {
      JNIEnv* env = getEnv();
      const auto object = JInteger::valueOf(env, 42);
      for (auto _: state) {
        const auto ref = GlobalRef<>::make(env, object);
        benchmark::DoNotOptimize(ref.get());
      }
    }

    void BM_RawNewGlobalRef(benchmark::State& state) {
      JNIEnv* env = getEnv();
      const auto object = JInteger::valueOf(env, 42);
      for (auto _: state) {
        jobject ref = env->NewGlobalRef(object);
        benchmark::DoNotOptimize(ref);
        env->DeleteGlobalRef(ref);
      }
    }

    void BM_LocalFrame(benchmark::State& state) {
      JNIEnv* env = getEnv();
      const auto capacity = static_cast<size_t>(state.range(0));
      for (auto _: state) {
        const LocalFrame frame(env, capacity);
        benchmark::ClobberMemory();
      }
    }

    // After the first call javaClass() is an atomic load; FindClass is what an uncached binding pays.
    void BM_JavaClass(benchmark::State& state) {
      JNIEnv* env = getEnv();
      for (auto _: state) {
        benchmark::DoNotOptimize(JInteger::javaClass(env));
      }
    }

    void BM_RawFindClass(benchmark::State& state) {
      JNIEnv* env = getEnv();
      for (auto _: state) {
        jclass clazz = env->FindClass("java/lang/Integer");
        benchmark::DoNotOptimize(clazz);
        env->DeleteLocalRef(clazz);
      }
    }
  } // namespace

  BENCHMARK(BM_GetEnv);
  BENCHMARK(BM_RawGetEnv);
  BENCHMARK(BM_LocalRef);
  BENCHMARK(BM_RawNewLocalRef);
  BENCHMARK(BM_GlobalRef);
  BENCHMARK(BM_RawNewGlobalRef);
  BENCHMARK(BM_LocalFrame)->Arg(16)->Arg(256);
  BENCHMARK(BM_JavaClass);
  BENCHMARK(BM_RawFindClass);
} // namespace expo::kolibri::benchmarks
