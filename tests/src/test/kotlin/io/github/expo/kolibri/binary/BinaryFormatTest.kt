package io.github.expo.kolibri.binary

import kotlin.random.Random
import kotlin.test.Test
import kotlin.test.assertContentEquals
import kotlin.test.assertEquals
import kotlin.test.assertFailsWith
import kotlin.test.assertNull
import kotlin.test.assertTrue

/**
 * Pure-JVM tests for [BinaryBuffer]'s operations and for the reference dynamic codec built on
 * them ([encodeValue] / [decodeValue]). No native code involved: both are buffer-agnostic, so a
 * privately allocated buffer stands in for the C++-owned pool.
 */
class BinaryFormatTest {
  private val buffer = BinaryBuffer.allocate(64 * 1024)

  private fun roundTrip(value: Any?): Any? {
    val length = buffer.encodeValue(value)
    assertTrue(length >= 0, "value unexpectedly did not fit: $value")
    return buffer.decodeValue(length)
  }

  private fun assertRoundTrips(value: Any?) {
    assertEquals(value, roundTrip(value))
  }

  @Test
  fun `round-trips scalars`() {
    assertRoundTrips(null)
    assertRoundTrips(true)
    assertRoundTrips(false)
    assertRoundTrips(42)
    assertRoundTrips(Int.MIN_VALUE)
    assertRoundTrips(1L shl 60)
    assertRoundTrips(1.5f)
    assertRoundTrips(3.14159)
    assertTrue((roundTrip(Double.NaN) as Double).isNaN())
  }

  @Test
  fun `round-trips strings including non-ASCII and astral planes`() {
    assertRoundTrips("")
    assertRoundTrips("hello")
    assertRoundTrips("zażółć gęślą jaźń")
    assertRoundTrips("日本語テキスト")
    assertRoundTrips("emoji 😀 and beyond 🤖") // 4-byte UTF-8 sequences
  }

  @Test
  fun `round-trips homogeneous double list via bulk run`() {
    val list = List(1000) { it * 0.5 }
    assertEquals(list, roundTrip(list))
  }

  @Test
  fun `round-trips homogeneous int, long, float and boolean lists`() {
    assertRoundTrips(List(17) { it })
    assertRoundTrips(List(17) { it.toLong() * 1_000_000_007L })
    assertRoundTrips(List(17) { it.toFloat() })
    assertRoundTrips(List(17) { it % 3 == 0 })
  }

  @Test
  fun `falls back to tagged elements when a list turns heterogeneous`() {
    assertRoundTrips(listOf(1.0, 2.0, "x"))
    assertRoundTrips(listOf(1.0, 2.0, null, 3.0))
    assertRoundTrips(listOf(1, 2L)) // Int then Long: class mismatch, not coerced
    assertRoundTrips(listOf("only", "strings"))
  }

  @Test
  fun `round-trips empty containers`() {
    assertRoundTrips(emptyList<Any?>())
    assertRoundTrips(emptyMap<String, Any?>())
  }

  @Test
  fun `round-trips maps with mixed values and nulls`() {
    assertRoundTrips(
      mapOf(
        "int" to 1,
        "double" to 2.5,
        "string" to "three",
        "null" to null,
        "list" to listOf(1.0, 2.0),
        "nested" to mapOf("deep" to listOf("a", null, true)),
      )
    )
  }

  @Test
  fun `rejects maps with non-String keys`() {
    assertFailsWith<IllegalArgumentException> {
      buffer.encodeValue(mapOf(1 to "one"))
    }
  }

  @Test
  fun `rejects unsupported types`() {
    assertFailsWith<IllegalArgumentException> {
      buffer.encodeValue(Any())
    }
  }

  @Test
  fun `round-trips primitive arrays as lists`() {
    // Numeric/boolean primitive arrays have no buffer tags of their own — they cross as LIST bulk
    // runs, so a dynamic decode materializes them boxed.
    assertEquals(
      listOf(1.0, -2.5, Double.MAX_VALUE),
      roundTrip(doubleArrayOf(1.0, -2.5, Double.MAX_VALUE)),
    )
    assertEquals(listOf(1, 2, 3), roundTrip(intArrayOf(1, 2, 3)))
    assertEquals(listOf(1L, -2L), roundTrip(longArrayOf(1L, -2L)))
    assertEquals(listOf(0.5f), roundTrip(floatArrayOf(0.5f)))
    assertEquals(listOf(true, false, true), roundTrip(booleanArrayOf(true, false, true)))
    assertEquals(emptyList<Any?>(), roundTrip(DoubleArray(0)))
    // ByteArray keeps its own tag: it is a binary blob (a JS ArrayBuffer), not a number list.
    assertContentEquals(
      byteArrayOf(0, 127, -128),
      roundTrip(byteArrayOf(0, 127, -128)) as ByteArray,
    )
    assertContentEquals(ByteArray(0), roundTrip(ByteArray(0)) as ByteArray)
  }

  @Test
  fun `a primitive array encodes byte-identically to the equivalent list`() {
    val fromArray = BinaryBuffer.allocate(64)
    val fromList = BinaryBuffer.allocate(64)
    val arrayLength = fromArray.encodeValue(doubleArrayOf(1.0, -2.5))
    val listLength = fromList.encodeValue(listOf(1.0, -2.5))
    assertEquals(listLength, arrayLength)
    assertContentEquals(fromList.getBytes(listLength), fromArray.getBytes(arrayLength))
  }

  @Test
  fun `overflow returns DID_NOT_FIT without touching buffer bookkeeping`() {
    val tiny = BinaryBuffer.allocate(16)
    assertEquals(DID_NOT_FIT, tiny.encodeValue(DoubleArray(100)))
    assertEquals(DID_NOT_FIT, tiny.encodeValue("a longer string than 16 bytes"))
    assertEquals(0, tiny.position) // writes go through a duplicate; original untouched
    // A small value still fits after an overflow attempt.
    assertTrue(tiny.encodeValue(1.0) > 0)
  }

  @Test
  fun `overflow mid-list leaves the buffer reusable`() {
    val tiny = BinaryBuffer.allocate(64)
    assertEquals(DID_NOT_FIT, tiny.encodeValue(List(100) { it * 1.0 }))
    assertEquals(listOf(1.0, 2.0), tiny.encodeValue(listOf(1.0, 2.0)).let { tiny.decodeValue(it) })
  }

  @Test
  fun `decode does not disturb the source buffer position`() {
    val length = buffer.encodeValue(listOf("a", "b"))
    buffer.position = 0
    buffer.decodeValue(length)
    assertEquals(0, buffer.position)
  }

  @Test
  fun `payload is written in native byte order`() {
    // Endianness canary: encode a double whose byte pattern is asymmetric and read the raw
    // bytes back in native order.
    val canary = 1.5e300
    val length = buffer.encodeValue(canary)
    assertEquals(1 + 8, length)
    val raw = buffer.duplicateView()
    assertEquals(BinaryTag.DOUBLE, raw.getTag())
    assertEquals(canary, raw.getDouble())
  }

  @Test
  fun `lone surrogates round-trip faithfully`() {
    // Non-ASCII strings cross as raw UTF-16 code units, so even invalid UTF-16 (an unpaired
    // surrogate) survives the trip byte-for-byte — matching the element-wise JNI path.
    val lone = "a\uD83Db" // high surrogate with no low surrogate
    assertEquals(lone, roundTrip(lone))
  }

  @Test
  fun `string buffer format is adaptive - ASCII bytes or UTF-16 units by prefix sign`() {
    // Pure ASCII: `i32 length` + one byte per char, byte-identical to the pre-adaptive format.
    val ascii = buffer.duplicateView()
    ascii.putString("abc")
    assertEquals(4 + 3, ascii.position)
    ascii.position = 0
    assertEquals(3, ascii.getInt())

    // Non-ASCII: negative unit count + two bytes per UTF-16 code unit. The scan boundary is
    // strictly 7-bit — Latin-1 0x80..0xFF must take the UTF-16 arm (Hermes ASCII cannot hold it).
    for (value in listOf("\u0080", "ÿ", "café", "żółć")) {
      val raw = buffer.duplicateView()
      raw.putString(value)
      assertEquals(4 + 2 * value.length, raw.position, "buffer size of $value")
      raw.position = 0
      assertEquals(-value.length, raw.getInt(), "prefix of $value")
      raw.position = 0
      assertEquals(value, raw.getString())
    }

    // Empty string: prefix 0, no payload, decodes through the ASCII arm.
    val empty = buffer.duplicateView()
    empty.putString("")
    assertEquals(4, empty.position)
    empty.position = 0
    assertEquals("", empty.getString())
  }

  @Test
  fun `unknown buffer tag is rejected by getValue`() {
    val raw = BinaryBuffer.allocate(4)
    raw.putTag(16)
    val error = assertFailsWith<IllegalStateException> { raw.decodeValue(1) }
    assertTrue("16" in error.message!!, error.message)
  }

  @Test
  fun `EXTERNAL_SCHEMA is rejected without a consumer`() {
    assertEquals(10, BinaryTag.EXTERNAL_SCHEMA)
    val raw = BinaryBuffer.allocate(8)
    raw.putTag(BinaryTag.EXTERNAL_SCHEMA)
    raw.putInt(42)
    val error = assertFailsWith<UnsupportedOperationException> {
      raw.decodeValue(raw.position)
    }
    assertTrue("42" in error.message!!, error.message)
    assertTrue("consumer" in error.message!!, error.message)
  }

  @Test
  fun `consumer reads EXTERNAL_SCHEMA payload with primitive operations`() {
    data class ExternalValue(val value: Int)

    val payload = buffer.duplicateView()
    payload.putExternalSchemaHeader(42)
    payload.putInt(7)
    payload.position = 0

    assertEquals(BinaryTag.EXTERNAL_SCHEMA, payload.getTag())
    assertEquals(42, payload.getExternalSchemaId())
    assertEquals(ExternalValue(7), ExternalValue(payload.getInt()))
  }

  @Test
  fun `tagged values round-trip through the buffer`() {
    // Nothing checks the tag on the way out, so this pins that each writer emits the tag naming
    // its own type — a wrong tag would make a dispatching reader decode garbage rather than throw.
    val buf = buffer.duplicateView()
    buf.putTaggedBoolean(true)
    buf.putTaggedInt(Int.MIN_VALUE)
    buf.putTaggedLong(1L shl 60)
    buf.putTaggedFloat(1.5f)
    buf.putTaggedDouble(3.14159)
    buf.putTaggedString("zażółć 😀")
    buf.putTaggedByteArray(byteArrayOf(0, 127, -128))
    buf.putTaggedNull()
    val end = buf.position

    buf.position = 0
    assertEquals(BinaryTag.BOOLEAN, buf.getTag())
    assertEquals(true, buf.getBoolean())
    assertEquals(BinaryTag.INT, buf.getTag())
    assertEquals(Int.MIN_VALUE, buf.getInt())
    assertEquals(BinaryTag.LONG, buf.getTag())
    assertEquals(1L shl 60, buf.getLong())
    assertEquals(BinaryTag.FLOAT, buf.getTag())
    assertEquals(1.5f, buf.getFloat())
    assertEquals(BinaryTag.DOUBLE, buf.getTag())
    assertEquals(3.14159, buf.getDouble())
    assertEquals(BinaryTag.STRING, buf.getTag())
    assertEquals("zażółć 😀", buf.getString())
    assertEquals(BinaryTag.BYTE_ARRAY, buf.getTag())
    assertContentEquals(byteArrayOf(0, 127, -128), buf.getByteArray())
    assertEquals(BinaryTag.NULL, buf.getTag())
    assertEquals(end, buf.position, "readers consumed exactly what the writers wrote")
  }

  @Test
  fun `tagged writers agree with the dynamic reader`() {
    // The single-type putTagged* writers and putValue emit the same bytes: whatever putTagged*
    // writes, getValue must read back as that value.
    val buf = buffer.duplicateView()
    buf.putTaggedString("hi")
    assertEquals("hi", buf.decodeValue(buf.position))

    val bytes = buffer.duplicateView()
    bytes.putTaggedByteArray(byteArrayOf(7, 8))
    assertContentEquals(byteArrayOf(7, 8), bytes.decodeValue(bytes.position) as ByteArray)
  }

  @Test
  fun `null round-trips at root`() {
    val length = buffer.encodeValue(null)
    assertEquals(1, length)
    assertNull(buffer.decodeValue(length))
  }

  @Test
  fun `truncated payload throws underflow instead of decoding garbage`() {
    val length = buffer.encodeValue("hello")
    assertFailsWith<java.nio.BufferUnderflowException> { buffer.decodeValue(length - 4) }

    val listLength = buffer.encodeValue(List(10) { it * 1.0 })
    assertFailsWith<java.nio.BufferUnderflowException> { buffer.decodeValue(listLength - 8) }
  }

  @Test
  fun `corrupt container counts are rejected before they size a collection`() {
    // [getValue] sizes an ArrayList/HashMap from a count it read, so it bounds that count against
    // the readable remainder first. [BinaryBuffer]'s own reads are unchecked — a count that came
    // from a writer always fit, so only the collection capacities need the guard here.
    fun corrupt(fill: BinaryBuffer.() -> Unit): BinaryBuffer =
      buffer.duplicateView().apply(fill)

    val giantList = corrupt { putTag(BinaryTag.LIST); putInt(Int.MAX_VALUE); putTag(BinaryTag.TAGGED) }
    assertFailsWith<java.nio.BufferUnderflowException> { giantList.decodeValue(giantList.position) }

    val giantRun = corrupt { putTag(BinaryTag.LIST); putInt(Int.MAX_VALUE / 4); putTag(BinaryTag.DOUBLE) }
    assertFailsWith<java.nio.BufferUnderflowException> { giantRun.decodeValue(giantRun.position) }

    val giantMap = corrupt { putTag(BinaryTag.MAP); putInt(Int.MAX_VALUE) }
    assertFailsWith<java.nio.BufferUnderflowException> { giantMap.decodeValue(giantMap.position) }
  }

  @Test
  fun `unknown list element tag is rejected by getValue`() {
    val raw = buffer.duplicateView()
    raw.putTag(BinaryTag.LIST)
    raw.putInt(1)
    raw.putTag(16)
    raw.putBoolean(true) // one payload byte, so the count guard passes and the tag check fires
    val error = assertFailsWith<IllegalStateException> { raw.decodeValue(raw.position) }
    assertTrue("element tag" in error.message!!, error.message)
  }

  @Test
  fun `payload of exactly the capacity fits and one byte more does not`() {
    // "hello": STRING tag (1) + i32 prefix (4) + 5 ASCII bytes = 10.
    val exact = BinaryBuffer.allocate(10)
    assertEquals(10, exact.encodeValue("hello"))
    val oneShort = BinaryBuffer.allocate(9)
    assertEquals(DID_NOT_FIT, oneShort.encodeValue("hello"))
  }

  @Test
  fun `every bulk-run kind encodes a primitive array byte-identically to the equivalent list`() {
    fun assertByteIdentical(array: Any, list: List<Any?>) {
      val fromArray = BinaryBuffer.allocate(512)
      val fromList = BinaryBuffer.allocate(512)
      val arrayLength = fromArray.encodeValue(array)
      val listLength = fromList.encodeValue(list)
      assertEquals(listLength, arrayLength, "buffer size of ${array::class.simpleName}")
      assertContentEquals(fromList.getBytes(listLength), fromArray.getBytes(arrayLength))
    }
    assertByteIdentical(intArrayOf(1, -2, 3), listOf(1, -2, 3))
    assertByteIdentical(longArrayOf(1L, 1L shl 60), listOf(1L, 1L shl 60))
    assertByteIdentical(floatArrayOf(0.5f, -1.5f), listOf(0.5f, -1.5f))
    assertByteIdentical(booleanArrayOf(true, false), listOf(true, false))
  }

  @Test
  fun `deep nesting round-trips`() {
    // A String leaf: a lone Double would ride a bulk run, which flattens the innermost level
    // without recursing at all.
    var value: Any? = "x"
    repeat(200) { value = listOf(value) }
    assertEquals(value, roundTrip(value))
  }

  @Test
  fun `long and astral strings round-trip, including as map keys`() {
    // Past the C++ side's 512-unit stack-buffer boundary, and full of surrogate pairs.
    val longAstral = buildString { repeat(600) { append("ż😀") } }
    assertEquals(1200 + 600, longAstral.length) // each 😀 is a surrogate pair
    assertRoundTrips(longAstral)
    assertRoundTrips(mapOf("😀🤖" to listOf(longAstral), "日本語" to mapOf("🎉" to null)))
  }

  @Test
  fun `fuzz - random nested structures round-trip`() {
    val random = Random(20260709)
    repeat(1000) {
      val value = randomValue(random, depth = 0)
      val length = buffer.encodeValue(value)
      assertTrue(length >= 0, "fuzz value did not fit")
      assertEquals(value, buffer.decodeValue(length))
    }
  }

  private fun randomValue(random: Random, depth: Int): Any? {
    // Deeper levels increasingly favor leaves so structures stay bounded.
    val leafBias = depth >= 8 || random.nextInt(depth + 2) != 0
    return if (leafBias) {
      when (random.nextInt(8)) {
        0 -> null
        1 -> random.nextBoolean()
        2 -> random.nextInt()
        3 -> random.nextLong()
        4 -> random.nextFloat()
        5 -> random.nextDouble()
        6 -> randomString(random)
        else -> List(random.nextInt(20)) { random.nextDouble() } // exercises bulk runs
      }
    } else {
      when (random.nextInt(2)) {
        0 -> List(random.nextInt(10)) { randomValue(random, depth + 1) }
        else -> buildMap {
          repeat(random.nextInt(10)) {
            put(randomString(random), randomValue(random, depth + 1))
          }
        }
      }
    }
  }

  private fun randomString(random: Random): String {
    // BMP-only alphabet: indexing by char must not split a surrogate pair (see the dedicated
    // lone-surrogate test for what happens to invalid UTF-16).
    val alphabet = "abcdefghijklmnopqrstuvwxyzżółćę日本"
    return buildString {
      repeat(random.nextInt(12)) { append(alphabet[random.nextInt(alphabet.length)]) }
    }
  }
}
