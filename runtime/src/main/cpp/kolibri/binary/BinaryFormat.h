#pragma once

#include <cstddef>
#include <cstdint>

namespace expo::kolibri::binary {
  /** Standard dynamic buffer tags plus the consumer-owned extension marker. */
  enum class Tag : uint8_t {
    BOOLEAN = 0,
    STRING = 1,
    INT = 2,
    LONG = 3,
    FLOAT = 4,
    DOUBLE = 5,
    LIST = 6,
    MAP = 7,

    NULLTAG = 8,
    BYTE_ARRAY = 9,

    /** Consumer-owned schema: `i32 schemaId` followed by a consumer-defined payload. */
    EXTERNAL_SCHEMA = 10,

    /**
     * `elemTag` sentinel: the list is not a bulk run — each element is written as an ordinary
     * tagged value (variable-size, nullable, or nested elements need self-describing entries).
     */
    TAGGED = 0xFF,
  };

  /**
   * Fixed capacity of each thread-local buffer. Deliberately not growable: most payloads are
   * small, and anything larger falls back to the element-wise conversion path instead of paying
   * for re-registration machinery. 256 KiB keeps arrays up to ~32k doubles on the fast path at
   * the cost of 256 KiB of lazily-allocated memory per bridge thread (in practice: one).
   */
  inline constexpr size_t kBufferCapacity = 256 * 1024;

  /**
   * Compile-time switch for buffer-format validation in the decoders.
   *
   * The default is the check-free build: both ends of the buffer are our own encoders, so decode
   * TRUSTS the payload shape — it consumes tag bytes without looking at them and reads exactly
   * the payload the caller's type expects. Validation of untrusted data happens at the system's
   * boundaries (JS values at encode time, Kotlin codecs), not per byte on the hot path.
   *
   * Define EXPO_BINARY_CHECKED_CODEC (CMake: -DEXPO_BINARY_CHECKED_CODEC=ON) to build the
   * validating variant: bounds checks on every read, tag verification against the expected
   * shape, count/depth/trailing-bytes checks. Use it when debugging codec changes or payload
   * corruption. The flag must be consistent across every target linking the codec (it is a
   * PUBLIC compile definition on kolibri).
   */
#ifdef EXPO_BINARY_CHECKED_CODEC
  inline constexpr bool kCheckedCodec = true;
#else
  inline constexpr bool kCheckedCodec = false;
#endif
} // namespace expo::kolibri::binary
