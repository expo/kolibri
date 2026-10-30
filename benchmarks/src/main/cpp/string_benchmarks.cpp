#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include <benchmark/benchmark.h>

#include <kolibri/env.h>
#include <kolibri/string_utils.h>

// UTF-8 <-> UTF-16 conversion, alone (pure C++) and through JNI. The raw JNI baselines use the
// modified-UTF-8 entry points (NewStringUTF / GetStringUTFChars) a hand-written binding would
// reach for; for the texts below, which have no NUL and no non-BMP characters, they produce the
// same strings, so the difference is kolibri's conversion cost.
//
// Every benchmark takes {minimum UTF-8 bytes, text kind}.
namespace expo::kolibri::benchmarks {
  namespace {
    struct TextKind {
      const char* label;
      std::string_view pattern;
    };

    constexpr std::array<TextKind, 3> kTextKinds{{
      {"ascii", "The quick brown fox jumps over the lazy dog. "},
      {"latin", "Zażółć gęślą jaźń, déjà vu. "},
      {"cjk", "日本語のテキスト、中文文本。"},
    }};

    const TextKind& textKind(const benchmark::State& state) {
      return kTextKinds[static_cast<size_t>(state.range(1))];
    }

    // Whole repetitions of the pattern, so the text never ends in a split code point.
    std::string makeText(const benchmark::State& state) {
      const auto minBytes = static_cast<size_t>(state.range(0));
      const std::string_view pattern = textKind(state).pattern;
      std::string text;
      while (text.size() < minBytes) {
        text.append(pattern);
      }
      return text;
    }

    std::u16string toUtf16(const std::string& text) {
      std::u16string out(text.size(), u'\0');
      out.resize(utf8ToUtf16Into(text, out.data()));
      return out;
    }

    void setUp(benchmark::State& state, const std::string& text) {
      state.SetLabel(textKind(state).label);
      state.SetBytesProcessed(static_cast<int64_t>(state.iterations() * text.size()));
    }

    void BM_Utf8ToUtf16(benchmark::State& state) {
      const std::string text = makeText(state);
      std::vector<char16_t> out(text.size());
      for (auto _: state) {
        benchmark::DoNotOptimize(utf8ToUtf16Into(text, out.data()));
        benchmark::ClobberMemory();
      }
      setUp(state, text);
    }

    void BM_Utf16ToUtf8(benchmark::State& state) {
      const std::string text = makeText(state);
      const std::u16string units = toUtf16(text);
      for (auto _: state) {
        std::string utf8 = utf16ToUtf8(units.data(), units.size());
        benchmark::DoNotOptimize(utf8);
      }
      setUp(state, text);
    }

    void BM_ToJString(benchmark::State& state) {
      JNIEnv* env = getEnv();
      const std::string text = makeText(state);
      for (auto _: state) {
        jstring str = toJString(env, text);
        benchmark::DoNotOptimize(str);
        env->DeleteLocalRef(str);
      }
      setUp(state, text);
    }

    void BM_ToJStringUtf16(benchmark::State& state) {
      JNIEnv* env = getEnv();
      const std::string text = makeText(state);
      const std::u16string units = toUtf16(text);
      for (auto _: state) {
        jstring str = toJString(env, std::u16string_view{units});
        benchmark::DoNotOptimize(str);
        env->DeleteLocalRef(str);
      }
      setUp(state, text);
    }

    void BM_RawNewStringUTF(benchmark::State& state) {
      JNIEnv* env = getEnv();
      const std::string text = makeText(state);
      for (auto _: state) {
        jstring str = env->NewStringUTF(text.c_str());
        benchmark::DoNotOptimize(str);
        env->DeleteLocalRef(str);
      }
      setUp(state, text);
    }

    void BM_ToStdString(benchmark::State& state) {
      JNIEnv* env = getEnv();
      const std::string text = makeText(state);
      jstring str = toJString(env, text);
      for (auto _: state) {
        std::string utf8 = toStdString(env, str);
        benchmark::DoNotOptimize(utf8);
      }
      env->DeleteLocalRef(str);
      setUp(state, text);
    }

    void BM_ToU16StdString(benchmark::State& state) {
      JNIEnv* env = getEnv();
      const std::string text = makeText(state);
      jstring str = toJString(env, text);
      for (auto _: state) {
        std::u16string utf16 = toU16StdString(env, str);
        benchmark::DoNotOptimize(utf16);
      }
      env->DeleteLocalRef(str);
      setUp(state, text);
    }

    void BM_RawGetStringUTFChars(benchmark::State& state) {
      JNIEnv* env = getEnv();
      const std::string text = makeText(state);
      jstring str = toJString(env, text);
      for (auto _: state) {
        const char* chars = env->GetStringUTFChars(str, nullptr);
        std::string utf8{chars, static_cast<size_t>(env->GetStringUTFLength(str))};
        env->ReleaseStringUTFChars(str, chars);
        benchmark::DoNotOptimize(utf8);
      }
      env->DeleteLocalRef(str);
      setUp(state, text);
    }

    void textArgs(benchmark::Benchmark* benchmark) {
      benchmark->ArgNames({"bytes", "kind"})->ArgsProduct({{16, 256, 4096}, {0, 1, 2}});
    }
  } // namespace

  BENCHMARK(BM_Utf8ToUtf16)->Apply(textArgs);
  BENCHMARK(BM_Utf16ToUtf8)->Apply(textArgs);
  BENCHMARK(BM_ToJString)->Apply(textArgs);
  BENCHMARK(BM_ToJStringUtf16)->Apply(textArgs);
  BENCHMARK(BM_RawNewStringUTF)->Apply(textArgs);
  BENCHMARK(BM_ToStdString)->Apply(textArgs);
  BENCHMARK(BM_ToU16StdString)->Apply(textArgs);
  BENCHMARK(BM_RawGetStringUTFChars)->Apply(textArgs);
} // namespace expo::kolibri::benchmarks
