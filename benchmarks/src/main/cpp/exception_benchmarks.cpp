#include <benchmark/benchmark.h>

#include <kolibri/Ref.h>
#include <kolibri/env.h>
#include <kolibri/exception.h>

#include "jvm.h"

// C++ -> Java: what a guarded native method does when its body throws, turning the C++ exception
// into a pending RuntimeException for the JVM to raise on return. Paired with raw JNI using a
// class looked up up front. (The Java -> C++ direction is BM_CallThrowingJavaException.)
namespace expo::kolibri::benchmarks {
  namespace {
    void BM_ThrowJavaRuntimeException(benchmark::State& state) {
      JNIEnv* env = getEnv();
      for (auto _: state) {
        throwJavaRuntimeException(env, "native failure");
        if (!env->ExceptionCheck()) {
          state.SkipWithError("no exception pending");
          return;
        }
        env->ExceptionClear();
      }
    }

    void BM_RawThrowNew(benchmark::State& state) {
      JNIEnv* env = getEnv();
      const auto clazz = Ref<>::adopt(env, env->FindClass("java/lang/RuntimeException"));
      for (auto _: state) {
        env->ThrowNew(static_cast<jclass>(clazz.get()), "native failure");
        if (!env->ExceptionCheck()) {
          state.SkipWithError("no exception pending");
          return;
        }
        env->ExceptionClear();
      }
    }
  } // namespace

  BENCHMARK(BM_ThrowJavaRuntimeException)->MinWarmUpTime(kJitWarmUpSeconds);
  BENCHMARK(BM_RawThrowNew)->MinWarmUpTime(kJitWarmUpSeconds);
} // namespace expo::kolibri::benchmarks
