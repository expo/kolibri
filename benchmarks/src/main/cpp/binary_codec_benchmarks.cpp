#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include <benchmark/benchmark.h>

#include <kolibri/binary/BinaryBuffer.h>
#include <kolibri/binary/BinaryCodec.h>

// The binary codec is plain C++ over the thread-local buffer, so it runs without a JVM. Each
// benchmark is parameterized by element count and reports bytes/s over the encoded payload.
namespace expo::kolibri::benchmarks {
  namespace {
    using namespace expo::kolibri::binary;

    template<typename T>
    std::vector<T> makeScalars(size_t count) {
      std::vector<T> values(count);
      for (size_t i = 0; i < count; i++) {
        values[i] = static_cast<T>(i);
      }
      return values;
    }

    std::vector<std::string> makeStrings(size_t count, const std::string& base) {
      std::vector<std::string> values;
      values.reserve(count);
      for (size_t i = 0; i < count; i++) {
        values.push_back(base + std::to_string(i));
      }
      return values;
    }

    std::vector<bool> makeBools(size_t count) {
      std::vector<bool> values(count);
      for (size_t i = 0; i < count; i++) {
        values[i] = i % 3 == 0;
      }
      return values;
    }

    // Every other element is absent, so both presence branches are taken.
    std::vector<std::optional<std::string>> makeOptionalStrings(size_t count) {
      std::vector<std::optional<std::string>> values;
      values.reserve(count);
      for (size_t i = 0; i < count; i++) {
        if (i % 2 == 0) {
          values.emplace_back("value-" + std::to_string(i));
        } else {
          values.emplace_back(std::nullopt);
        }
      }
      return values;
    }

    // `count` rows of 16 ints: the outer list is element-wise, each row a raw scalar run.
    std::vector<std::vector<int32_t>> makeRows(size_t count) {
      std::vector<std::vector<int32_t>> rows;
      rows.reserve(count);
      for (size_t i = 0; i < count; i++) {
        rows.push_back(makeScalars<int32_t>(16));
      }
      return rows;
    }

    std::unordered_map<std::string, double> makeMap(size_t count) {
      std::unordered_map<std::string, double> values;
      values.reserve(count);
      for (size_t i = 0; i < count; i++) {
        values.emplace("key" + std::to_string(i), static_cast<double>(i));
      }
      return values;
    }

    template<typename T>
    void encode(benchmark::State& state, const T& value) {
      BinaryBuffer& buffer = BinaryBuffer::shared();
      for (auto _: state) {
        if (!toBinary(value, buffer)) {
          state.SkipWithError("payload does not fit in the binary buffer");
          return;
        }
        benchmark::DoNotOptimize(buffer.data());
        benchmark::ClobberMemory();
      }
      state.SetBytesProcessed(static_cast<int64_t>(state.iterations() * buffer.size()));
    }

    template<typename T>
    void decode(benchmark::State& state, const T& value) {
      BinaryBuffer& buffer = BinaryBuffer::shared();
      if (!toBinary(value, buffer)) {
        state.SkipWithError("payload does not fit in the binary buffer");
        return;
      }
      for (auto _: state) {
        T decoded = fromBinary<T>(buffer.data(), buffer.size());
        benchmark::DoNotOptimize(decoded);
      }
      state.SetBytesProcessed(static_cast<int64_t>(state.iterations() * buffer.size()));
    }

    void BM_EncodeDoubles(benchmark::State& state) {
      encode(state, makeScalars<double>(static_cast<size_t>(state.range(0))));
    }

    void BM_DecodeDoubles(benchmark::State& state) {
      decode(state, makeScalars<double>(static_cast<size_t>(state.range(0))));
    }

    void BM_EncodeInt64s(benchmark::State& state) {
      encode(state, makeScalars<int64_t>(static_cast<size_t>(state.range(0))));
    }

    void BM_DecodeInt64s(benchmark::State& state) {
      decode(state, makeScalars<int64_t>(static_cast<size_t>(state.range(0))));
    }

    // bool is not a raw scalar: one byte per element, converted both ways.
    void BM_EncodeBools(benchmark::State& state) {
      encode(state, makeBools(static_cast<size_t>(state.range(0))));
    }

    void BM_DecodeBools(benchmark::State& state) {
      decode(state, makeBools(static_cast<size_t>(state.range(0))));
    }

    void BM_EncodeBytes(benchmark::State& state) {
      encode(state, makeScalars<uint8_t>(static_cast<size_t>(state.range(0))));
    }

    void BM_DecodeBytes(benchmark::State& state) {
      decode(state, makeScalars<uint8_t>(static_cast<size_t>(state.range(0))));
    }

    void BM_EncodeOptionalStrings(benchmark::State& state) {
      encode(state, makeOptionalStrings(static_cast<size_t>(state.range(0))));
    }

    void BM_DecodeOptionalStrings(benchmark::State& state) {
      decode(state, makeOptionalStrings(static_cast<size_t>(state.range(0))));
    }

    void BM_EncodeNestedIntRows(benchmark::State& state) {
      encode(state, makeRows(static_cast<size_t>(state.range(0))));
    }

    void BM_DecodeNestedIntRows(benchmark::State& state) {
      decode(state, makeRows(static_cast<size_t>(state.range(0))));
    }

    void BM_EncodeAsciiStrings(benchmark::State& state) {
      encode(state, makeStrings(static_cast<size_t>(state.range(0)), "value-"));
    }

    void BM_DecodeAsciiStrings(benchmark::State& state) {
      decode(state, makeStrings(static_cast<size_t>(state.range(0)), "value-"));
    }

    // "café-" forces the UTF-16 path on both sides.
    void BM_EncodeUtf8Strings(benchmark::State& state) {
      encode(state, makeStrings(static_cast<size_t>(state.range(0)), "caf\xC3\xA9-"));
    }

    void BM_DecodeUtf8Strings(benchmark::State& state) {
      decode(state, makeStrings(static_cast<size_t>(state.range(0)), "caf\xC3\xA9-"));
    }

    void BM_EncodeStringDoubleMap(benchmark::State& state) {
      encode(state, makeMap(static_cast<size_t>(state.range(0))));
    }

    void BM_DecodeStringDoubleMap(benchmark::State& state) {
      decode(state, makeMap(static_cast<size_t>(state.range(0))));
    }
  } // namespace

  // Capped at 16k: 32k doubles plus the count header no longer fit in the 256 KiB buffer.
  BENCHMARK(BM_EncodeDoubles)->RangeMultiplier(8)->Range(8, 16 << 10);
  BENCHMARK(BM_DecodeDoubles)->RangeMultiplier(8)->Range(8, 16 << 10);
  BENCHMARK(BM_EncodeInt64s)->RangeMultiplier(8)->Range(8, 16 << 10);
  BENCHMARK(BM_DecodeInt64s)->RangeMultiplier(8)->Range(8, 16 << 10);
  BENCHMARK(BM_EncodeBools)->RangeMultiplier(8)->Range(8, 32 << 10);
  BENCHMARK(BM_DecodeBools)->RangeMultiplier(8)->Range(8, 32 << 10);
  BENCHMARK(BM_EncodeBytes)->RangeMultiplier(8)->Range(8, 128 << 10);
  BENCHMARK(BM_DecodeBytes)->RangeMultiplier(8)->Range(8, 128 << 10);
  BENCHMARK(BM_EncodeOptionalStrings)->RangeMultiplier(8)->Range(8, 4 << 10);
  BENCHMARK(BM_DecodeOptionalStrings)->RangeMultiplier(8)->Range(8, 4 << 10);
  // 16 ints per row: 1024 rows is ~68 KiB.
  BENCHMARK(BM_EncodeNestedIntRows)->RangeMultiplier(8)->Range(8, 1 << 10);
  BENCHMARK(BM_DecodeNestedIntRows)->RangeMultiplier(8)->Range(8, 1 << 10);
  BENCHMARK(BM_EncodeAsciiStrings)->RangeMultiplier(8)->Range(8, 4 << 10);
  BENCHMARK(BM_DecodeAsciiStrings)->RangeMultiplier(8)->Range(8, 4 << 10);
  BENCHMARK(BM_EncodeUtf8Strings)->RangeMultiplier(8)->Range(8, 4 << 10);
  BENCHMARK(BM_DecodeUtf8Strings)->RangeMultiplier(8)->Range(8, 4 << 10);
  BENCHMARK(BM_EncodeStringDoubleMap)->RangeMultiplier(8)->Range(8, 4 << 10);
  BENCHMARK(BM_DecodeStringDoubleMap)->RangeMultiplier(8)->Range(8, 4 << 10);
} // namespace expo::kolibri::benchmarks
