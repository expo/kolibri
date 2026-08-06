package io.github.expo.kolibri.binary


/**
 * Tags understood by Kolibri's dynamic binary format:
 *
 * ```
 * dyn := NULL | BOOLEAN u8 | INT i32 | LONG i64 | FLOAT f32 | DOUBLE f64
 *      | STRING string
 *      | LIST i32 count, u8 elemTag, elements             // bulk runs for scalar elemTags
 *      | MAP i32 count, count x (string key, dyn)
 *      | BYTE_ARRAY i32 count + raw                       // binary blob (JS ArrayBuffer)
 *      | EXTERNAL_SCHEMA i32 schemaId, then consumer-defined payload
 *
 * string := i32 prefix, then `prefix` ASCII bytes when prefix >= 0,
 *           or `-prefix` UTF-16 code units (native order) when prefix < 0
 *
 * elements := elemTag == TAGGED            -> count x dyn
 *           | elemTag is fixed-size scalar -> count x raw payload
 * ```
 *
 * [EXTERNAL_SCHEMA] reserves tag `10` for consumer-owned values. Kolibri exposes primitive
 * header and payload operations for it, but does not register, dispatch, or interpret schemas.
 *
 * Everything is native byte order and packed. The values MUST stay in sync with the C++
 * `binary::Tag` enum in
 * `kolibri/runtime/src/main/cpp/kolibri/binary/BinaryFormat.h`.
 */
object BinaryTag {
  const val BOOLEAN = 0
  const val STRING = 1
  const val INT = 2
  const val LONG = 3
  const val FLOAT = 4
  const val DOUBLE = 5
  const val LIST = 6
  const val MAP = 7

  const val NULL = 8
  const val BYTE_ARRAY = 9

  /** Consumer-owned schema: `i32 schemaId` followed by a consumer-defined payload. */
  const val EXTERNAL_SCHEMA = 10

  /**
   * `elemTag` sentinel: the list is not a bulk run - each element is written as an ordinary
   * tagged value (variable-size, nullable, or nested elements need self-describing entries).
   */
  const val TAGGED = 0xFF
}
