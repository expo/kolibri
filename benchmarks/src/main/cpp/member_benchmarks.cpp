#include <cstdint>

#include <benchmark/benchmark.h>

#include <kolibri/JavaClass.h>
#include <kolibri/box.h>
#include <kolibri/class.h>
#include <kolibri/env.h>
#include <kolibri/exception.h>
#include <kolibri/string_utils.h>

#include "jvm.h"

// Calls through kolibri's member tokens against JDK classes, each paired with raw JNI using IDs
// cached up front — the fastest a hand-written binding can be. The gap is the token overhead:
// the cached-ID load, argument conversion, ref wrapping and (unless noexcept) the exception check.
namespace expo::kolibri::benchmarks {
  namespace {
    struct JStringMembers : JavaClass<JStringMembers> {
      static constexpr std::string_view descriptor = "java/lang/String";
      // The same method twice: noexcept skips the ExceptionCheck after the call.
      static constexpr Method<"length", jint() noexcept> lengthNoexcept{};
      static constexpr Method<"length", jint()> length{};
      static constexpr Method<"concat", Ref<jstring>(jstring)> concat{};
    };

    struct JIntegerMembers : JavaClass<JIntegerMembers> {
      static constexpr std::string_view descriptor = "java/lang/Integer";
      static constexpr StaticField<"MAX_VALUE", jint> maxValue{};
    };

    void BM_Method(benchmark::State& state) {
      JNIEnv* env = getEnv();
      const auto str = Ref<jstring>::adopt(env, toJString(env, "kolibri"));
      for (auto _: state) {
        benchmark::DoNotOptimize(JStringMembers::length(env, str));
      }
    }

    void BM_MethodNoexcept(benchmark::State& state) {
      JNIEnv* env = getEnv();
      const auto str = Ref<jstring>::adopt(env, toJString(env, "kolibri"));
      for (auto _: state) {
        benchmark::DoNotOptimize(JStringMembers::lengthNoexcept(env, str));
      }
    }

    void BM_RawCallIntMethod(benchmark::State& state) {
      JNIEnv* env = getEnv();
      const auto str = Ref<jstring>::adopt(env, toJString(env, "kolibri"));
      const jmethodID length = env->GetMethodID(JString::javaClass(env), "length", "()I");
      for (auto _: state) {
        benchmark::DoNotOptimize(env->CallIntMethod(str, length));
        if (env->ExceptionCheck()) {
          state.SkipWithError("String.length() threw");
          return;
        }
      }
    }

    // An object argument and an owned object result.
    void BM_MethodObjectResult(benchmark::State& state) {
      JNIEnv* env = getEnv();
      const auto str = Ref<jstring>::adopt(env, toJString(env, "kolibri"));
      const auto suffix = Ref<jstring>::adopt(env, toJString(env, "-bench"));
      for (auto _: state) {
        const auto result = JStringMembers::concat(env, str, static_cast<jstring>(suffix.get()));
        benchmark::DoNotOptimize(result.get());
      }
    }

    void BM_RawCallObjectMethod(benchmark::State& state) {
      JNIEnv* env = getEnv();
      const auto str = Ref<jstring>::adopt(env, toJString(env, "kolibri"));
      const auto suffix = Ref<jstring>::adopt(env, toJString(env, "-bench"));
      const jmethodID concat = env->GetMethodID(
        JString::javaClass(env), "concat", "(Ljava/lang/String;)Ljava/lang/String;"
      );
      for (auto _: state) {
        jobject result = env->CallObjectMethod(str, concat, suffix.get());
        if (env->ExceptionCheck()) {
          state.SkipWithError("String.concat() threw");
          return;
        }
        benchmark::DoNotOptimize(result);
        env->DeleteLocalRef(result);
      }
    }

    // Integer.valueOf(42) comes from the Integer cache, so this is call overhead, not allocation.
    void BM_StaticMethod(benchmark::State& state) {
      JNIEnv* env = getEnv();
      for (auto _: state) {
        const auto boxed = JInteger::valueOf(env, 42);
        benchmark::DoNotOptimize(boxed.get());
      }
    }

    void BM_RawCallStaticObjectMethod(benchmark::State& state) {
      JNIEnv* env = getEnv();
      const jclass integer = JInteger::javaClass(env);
      const jmethodID valueOf = env->GetStaticMethodID(integer, "valueOf", "(I)Ljava/lang/Integer;");
      for (auto _: state) {
        jobject boxed = env->CallStaticObjectMethod(integer, valueOf, jint{42});
        if (env->ExceptionCheck()) {
          state.SkipWithError("Integer.valueOf() threw");
          return;
        }
        benchmark::DoNotOptimize(boxed);
        env->DeleteLocalRef(boxed);
      }
    }

    void BM_StaticField(benchmark::State& state) {
      JNIEnv* env = getEnv();
      for (auto _: state) {
        benchmark::DoNotOptimize(JIntegerMembers::maxValue(env));
      }
    }

    void BM_RawGetStaticIntField(benchmark::State& state) {
      JNIEnv* env = getEnv();
      const jclass integer = JInteger::javaClass(env);
      const jfieldID maxValue = env->GetStaticFieldID(integer, "MAX_VALUE", "I");
      for (auto _: state) {
        benchmark::DoNotOptimize(env->GetStaticIntField(integer, maxValue));
      }
    }

    void BM_Constructor(benchmark::State& state) {
      JNIEnv* env = getEnv();
      for (auto _: state) {
        const auto list = JArrayList::constructor(env, 16);
        benchmark::DoNotOptimize(list.get());
      }
    }

    void BM_RawNewObject(benchmark::State& state) {
      JNIEnv* env = getEnv();
      const jclass arrayList = JArrayList::javaClass(env);
      const jmethodID init = env->GetMethodID(arrayList, "<init>", "(I)V");
      for (auto _: state) {
        jobject list = env->NewObject(arrayList, init, jint{16});
        if (list == nullptr) {
          state.SkipWithError("new ArrayList(16) failed");
          return;
        }
        benchmark::DoNotOptimize(list);
        env->DeleteLocalRef(list);
      }
    }

    // A composite: walking a List<Integer> through Iterator.hasNext()/next() and unboxing each
    // element, three calls per element.
    void BM_IterateIntegerList(benchmark::State& state) {
      JNIEnv* env = getEnv();
      const auto count = static_cast<jint>(state.range(0));
      const auto list = JArrayList::constructor(env, count);
      for (jint i = 0; i < count; i++) {
        list->add(env, JInteger::valueOf(env, i).get());
      }
      for (auto _: state) {
        int64_t sum = 0;
        const auto it = list->iterator(env);
        while (it->hasNext(env) == JNI_TRUE) {
          sum += JInteger::intValue(env, it->next(env));
        }
        benchmark::DoNotOptimize(sum);
      }
      state.SetItemsProcessed(state.iterations() * count);
    }

    struct JIntegerParsing : JavaClass<JIntegerParsing> {
      static constexpr std::string_view descriptor = "java/lang/Integer";
      static constexpr StaticMethod<"parseInt", jint(jstring)> parseInt{};
    };

    void BM_CallNotThrowing(benchmark::State& state) {
      JNIEnv* env = getEnv();
      const auto input = Ref<jstring>::adopt(env, toJString(env, "42"));
      for (auto _: state) {
        benchmark::DoNotOptimize(JIntegerParsing::parseInt(env, static_cast<jstring>(input.get())));
      }
    }

    // The cold path: Java throws, kolibri converts the pending exception into a JavaException
    // (building its message from the Java side) and C++ catches it.
    void BM_CallThrowingJavaException(benchmark::State& state) {
      JNIEnv* env = getEnv();
      const auto input = Ref<jstring>::adopt(env, toJString(env, "not a number"));
      for (auto _: state) {
        try {
          benchmark::DoNotOptimize(JIntegerParsing::parseInt(env, static_cast<jstring>(input.get())));
          state.SkipWithError("parseInt did not throw");
          return;
        } catch (const JavaException& e) {
          benchmark::DoNotOptimize(e.what());
        }
      }
    }
  } // namespace

  BENCHMARK(BM_Method)->MinWarmUpTime(kJitWarmUpSeconds);
  BENCHMARK(BM_MethodNoexcept)->MinWarmUpTime(kJitWarmUpSeconds);
  BENCHMARK(BM_RawCallIntMethod)->MinWarmUpTime(kJitWarmUpSeconds);
  BENCHMARK(BM_MethodObjectResult)->MinWarmUpTime(kJitWarmUpSeconds);
  BENCHMARK(BM_RawCallObjectMethod)->MinWarmUpTime(kJitWarmUpSeconds);
  BENCHMARK(BM_StaticMethod)->MinWarmUpTime(kJitWarmUpSeconds);
  BENCHMARK(BM_RawCallStaticObjectMethod)->MinWarmUpTime(kJitWarmUpSeconds);
  BENCHMARK(BM_StaticField);
  BENCHMARK(BM_RawGetStaticIntField);
  BENCHMARK(BM_Constructor)->MinWarmUpTime(kJitWarmUpSeconds);
  BENCHMARK(BM_RawNewObject)->MinWarmUpTime(kJitWarmUpSeconds);
  BENCHMARK(BM_IterateIntegerList)->MinWarmUpTime(kJitWarmUpSeconds)->Arg(16)->Arg(1024);
  BENCHMARK(BM_CallNotThrowing)->MinWarmUpTime(kJitWarmUpSeconds);
  BENCHMARK(BM_CallThrowingJavaException)->MinWarmUpTime(kJitWarmUpSeconds);
} // namespace expo::kolibri::benchmarks
