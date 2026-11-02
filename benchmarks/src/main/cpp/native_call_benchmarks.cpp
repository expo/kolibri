#include <stdexcept>

#include <benchmark/benchmark.h>

#include <kolibri/NativeObject.h>
#include <kolibri/env.h>
#include <kolibri/native_method.h>
#include <kolibri/utils.h>

#include "jvm.h"

// Java -> C++: JIT-compiled loops in NativeCalls.java call natives bound by kolibri's
// makeNativeMethod, each paired with a hand-written JNI function doing the same work. The gap is
// the trampoline: argument lifting, the exception guard (unless noexcept), and return lowering.
// Every benchmark reports `per_call`, the time of one Java -> native call.
namespace expo::kolibri::benchmarks {
  namespace {
    constexpr std::string_view kNativeCalls = "io/github/expo/kolibri/benchmarks/NativeCalls";

    struct Peer : NativeObject<Peer> {
      static constexpr std::string_view descriptor = kNativeCalls;

      jint value() { return value_; }

      jint valueNoexcept() noexcept { return value_; }

      jint value_ = 42;
    };

    // --- kolibri-bound natives ------------------------------------------------------------------

    jint add(jint a, jint b) { return a + b; }

    jint addNoexcept(jint a, jint b) noexcept { return a + b; }

    NOINLINE void throwFailure() { throw std::runtime_error("native failure"); }

    void fail() { throwFailure(); }

    // --- Raw JNI baselines --------------------------------------------------------------------

    jclass gRuntimeException = nullptr;
    jfieldID gPointerField = nullptr;

    jint JNICALL rawAdd(JNIEnv*, jclass, jint a, jint b) { return a + b; }

    jint JNICALL rawPeerValue(JNIEnv*, jclass, jlong peer) {
      return reinterpret_cast<Peer*>(peer)->value_;
    }

    jint JNICALL rawPeerValueFromField(JNIEnv* env, jobject self) {
      return reinterpret_cast<Peer*>(env->GetLongField(self, gPointerField))->value_;
    }

    // What a hand-written binding does when its C++ code throws.
    void JNICALL rawFail(JNIEnv* env, jclass) {
      try {
        throwFailure();
      } catch (const std::exception& e) {
        env->ThrowNew(gRuntimeException, e.what());
      }
    }

    struct Loops {
      jclass clazz;
      jmethodID rawAdd, add, addNoexcept;
      jmethodID rawPeerValue, rawPeerValueFromField, peerValue, peerValueNoexcept;
      jmethodID rawFail, fail;
    };

    Loops setUp(JNIEnv* env) {
      const auto clazz = reinterpret_cast<jclass>(findGlobalClass(env, kNativeCalls).release());
      gRuntimeException =
        reinterpret_cast<jclass>(findGlobalClass(env, "java/lang/RuntimeException").release());
      gPointerField = env->GetFieldID(clazz, "nativePointer", "J");

      const JNINativeMethod raw[] = {
        {const_cast<char*>("rawAdd"), const_cast<char*>("(II)I"), reinterpret_cast<void*>(&rawAdd)},
        {const_cast<char*>("rawPeerValue"), const_cast<char*>("(J)I"), reinterpret_cast<void*>(&rawPeerValue)},
        {const_cast<char*>("rawPeerValueFromField"), const_cast<char*>("()I"), reinterpret_cast<void*>(&rawPeerValueFromField)},
        {const_cast<char*>("rawFail"), const_cast<char*>("()V"), reinterpret_cast<void*>(&rawFail)},
      };
      registerNative(env, clazz, raw);

      registerNative<Peer>(env)
        .method<&add>("add")
        .method<&addNoexcept>("addNoexcept")
        .method<&Peer::value>("peerValue")
        .method<&Peer::valueNoexcept>("peerValueNoexcept")
        .method<&fail>("fail")
        .commit();

      const auto loop = [&](const char* name, const char* signature) {
        return env->GetStaticMethodID(clazz, name, signature);
      };
      return {
        .clazz = clazz,
        .rawAdd = loop("loopRawAdd", "(I)J"),
        .add = loop("loopAdd", "(I)J"),
        .addNoexcept = loop("loopAddNoexcept", "(I)J"),
        .rawPeerValue = loop("loopRawPeerValue", "(JI)J"),
        .rawPeerValueFromField = loop("loopRawPeerValueFromField", "(JI)J"),
        .peerValue = loop("loopPeerValue", "(JI)J"),
        .peerValueNoexcept = loop("loopPeerValueNoexcept", "(JI)J"),
        .rawFail = loop("loopRawFail", "(I)J"),
        .fail = loop("loopFail", "(I)J"),
      };
    }

    const Loops& loops(JNIEnv* env) {
      static const Loops instance = setUp(env);
      return instance;
    }

    // Runs `loop` (calls native methods `calls` times) per iteration. A peer loop also takes the
    // peer's address.
    template<jmethodID Loops::* Loop, bool WithPeer = false>
    void runLoop(benchmark::State& state) {
      JNIEnv* env = getEnv();
      const Loops& l = loops(env);
      const auto calls = static_cast<jint>(state.range(0));
      Peer peer;
      for (auto _: state) {
        jlong result = WithPeer
          ? env->CallStaticLongMethod(l.clazz, l.*Loop, reinterpret_cast<jlong>(&peer), calls)
          : env->CallStaticLongMethod(l.clazz, l.*Loop, calls);
        benchmark::DoNotOptimize(result);
      }
      if (env->ExceptionCheck()) {
        env->ExceptionClear();
        state.SkipWithError("the Java loop threw");
        return;
      }
      state.counters["per_call"] = benchmark::Counter(
        static_cast<double>(calls),
        benchmark::Counter::kIsIterationInvariantRate | benchmark::Counter::kInvert
      );
    }
  } // namespace

  BENCHMARK(runLoop<&Loops::rawAdd>)->Name("BM_NativeCallRawAdd")->Arg(1000)->MinWarmUpTime(kJitWarmUpSeconds);
  BENCHMARK(runLoop<&Loops::add>)->Name("BM_NativeCallAdd")->Arg(1000)->MinWarmUpTime(kJitWarmUpSeconds);
  BENCHMARK(runLoop<&Loops::addNoexcept>)->Name("BM_NativeCallAddNoexcept")->Arg(1000)->MinWarmUpTime(kJitWarmUpSeconds);
  BENCHMARK(runLoop<&Loops::rawPeerValue, true>)->Name("BM_NativeCallRawPeer")->Arg(1000)->MinWarmUpTime(kJitWarmUpSeconds);
  BENCHMARK(runLoop<&Loops::rawPeerValueFromField, true>)->Name("BM_NativeCallRawPeerFromField")->Arg(1000)->MinWarmUpTime(kJitWarmUpSeconds);
  BENCHMARK(runLoop<&Loops::peerValue, true>)->Name("BM_NativeCallPeer")->Arg(1000)->MinWarmUpTime(kJitWarmUpSeconds);
  BENCHMARK(runLoop<&Loops::peerValueNoexcept, true>)->Name("BM_NativeCallPeerNoexcept")->Arg(1000)->MinWarmUpTime(kJitWarmUpSeconds);
  BENCHMARK(runLoop<&Loops::rawFail>)->Name("BM_NativeCallRawThrows")->Arg(100)->MinWarmUpTime(kJitWarmUpSeconds);
  BENCHMARK(runLoop<&Loops::fail>)->Name("BM_NativeCallThrows")->Arg(100)->MinWarmUpTime(kJitWarmUpSeconds);
} // namespace expo::kolibri::benchmarks
