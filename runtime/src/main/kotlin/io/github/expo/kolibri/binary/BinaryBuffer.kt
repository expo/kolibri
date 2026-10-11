package io.github.expo.kolibri.binary

import io.github.expo.kolibri.CalledFromNative
import io.github.expo.kolibri.Kolibri
import io.github.expo.kolibri.isAscii
import java.nio.BufferOverflowException
import java.nio.ByteBuffer
import java.nio.ByteOrder

@JvmInline
@CalledFromNative(by = "kolibri/binary/BinaryBuffer.cpp")
value class BinaryBuffer private constructor(private val buffer: ByteBuffer) {
  var position: Int
    get() = buffer.position()
    set(value) {
      buffer.position(value)
    }

  var limit: Int
    get() = buffer.limit()
    set(value) {
      buffer.limit(value)
    }

  val remaining: Int
    get() = buffer.remaining()

  fun duplicateView(): BinaryBuffer {
    val copy = buffer.duplicate().order(ByteOrder.nativeOrder())
    copy.clear()
    return BinaryBuffer(copy)
  }

  fun putTag(tag: Int) = apply {
    buffer.put(tag.toByte())
  }

  fun getTag(): Int = buffer.get().toInt() and 0xFF

  fun putBoolean(value: Boolean) = apply {
    buffer.put(if (value) 1 else 0)
  }

  fun putInt(value: Int) = apply {
    buffer.putInt(value)
  }

  fun putLong(value: Long) = apply {
    buffer.putLong(value)
  }

  fun putFloat(value: Float) = apply {
    buffer.putFloat(value)
  }

  fun putDouble(value: Double) = apply {
    buffer.putDouble(value)
  }

  fun putString(value: String) = apply {
    val size = value.length
    val useScratch = size <= MAX_RETAINED_STRING_UNITS

    val isAscii = if (useScratch) {
      when (tryPutAscii(value, size)) {
        WROTE_ASCII -> return@apply
        FOUND_NON_ASCII -> false
        else -> value.isAscii()
      }
    } else {
      value.isAscii()
    }

    if (isAscii) {
      buffer.putInt(size)
      if (useScratch) {
        val bytes = stringScratch.get().bytes(size)
        value.copyAsciiTo(bytes)
        buffer.put(bytes, 0, size)
      } else {
        buffer.put(value.toByteArray(Charsets.ISO_8859_1))
      }
    } else {
      putUtf16(value, size)
    }
  }

  /**
   * Writes [value], which must be ASCII, in the ASCII form of [putString], without the check that
   * picks the form. The position does not move if it throws.
   *
   * @throws IllegalArgumentException if [value] is not ASCII
   * @throws BufferOverflowException if [value] does not fit
   */
  fun putAsciiString(value: String) = apply {
    when (tryPutAscii(value, value.length)) {
      WROTE_ASCII -> Unit
      FOUND_NON_ASCII -> throw IllegalArgumentException("Not an ASCII string")
      else -> throw BufferOverflowException()
    }
  }

  /**
   * Writes [value] in the UTF-16 form of [putString], which carries any string, without the check
   * that picks the form.
   */
  fun putUtf16String(value: String) = apply {
    putUtf16(value, value.length)
  }

  private fun putUtf16(value: String, size: Int) {
    buffer.putInt(-size)
    if (size <= MAX_RETAINED_STRING_UNITS) {
      val chars = stringScratch.get().chars(size)
      value.toCharArray(chars, 0, 0, size)
      buffer.asCharBuffer().put(chars, 0, size)
    } else {
      buffer.asCharBuffer().put(value)
    }
    position += size * Char.SIZE_BYTES
  }

  /**
   * Writes [value] the way [putString] writes an ASCII string, if it is one and it fits, and
   * reports what it found: [WROTE_ASCII], [FOUND_NON_ASCII] or [NO_ROOM].
   */
  private fun tryPutAscii(value: String, size: Int): Int {
    val start = buffer.position()
    if (size > buffer.limit() - start - Int.SIZE_BYTES) {
      return NO_ROOM
    }

    var index = start + Int.SIZE_BYTES
    var i = 0
    if (LITTLE_ENDIAN) {
      while (i + Long.SIZE_BYTES <= size) {
        val c0 = value[i].code
        val c1 = value[i + 1].code
        val c2 = value[i + 2].code
        val c3 = value[i + 3].code
        val c4 = value[i + 4].code
        val c5 = value[i + 5].code
        val c6 = value[i + 6].code
        val c7 = value[i + 7].code
        if ((c0 or c1 or c2 or c3 or c4 or c5 or c6 or c7) >= 0x80) {
          return FOUND_NON_ASCII
        }
        buffer.putLong(
          index,
          c0.toLong() or
            (c1.toLong() shl 8) or
            (c2.toLong() shl 16) or
            (c3.toLong() shl 24) or
            (c4.toLong() shl 32) or
            (c5.toLong() shl 40) or
            (c6.toLong() shl 48) or
            (c7.toLong() shl 56),
        )
        i += Long.SIZE_BYTES
        index += Long.SIZE_BYTES
      }
    }
    while (i < size) {
      val c = value[i].code
      if (c >= 0x80) {
        return FOUND_NON_ASCII
      }
      buffer.put(index, c.toByte())
      i++
      index++
    }

    buffer.putInt(start, size)
    buffer.position(index)
    return WROTE_ASCII
  }

  fun getBoolean(): Boolean = buffer.get() != 0.toByte()

  fun getInt(): Int = buffer.getInt()

  fun getLong(): Long = buffer.getLong()

  fun getFloat(): Float = buffer.getFloat()

  fun getDouble(): Double = buffer.getDouble()

  fun getString(): String {
    val prefix = buffer.getInt()
    val isAscii = prefix >= 0

    return if (isAscii) {
      if (prefix <= MAX_RETAINED_STRING_UNITS) {
        val bytes = stringScratch.get().bytes(prefix)
        buffer.get(bytes, 0, prefix)
        String(bytes, 0, prefix, Charsets.ISO_8859_1)
      } else {
        val bytes = ByteArray(prefix)
        buffer.get(bytes)
        String(bytes, Charsets.ISO_8859_1)
      }
    } else {
      val size = -prefix
      val value = if (size <= MAX_RETAINED_STRING_UNITS) {
        val chars = stringScratch.get().chars(size)
        buffer.asCharBuffer().get(chars, 0, size)
        String(chars, 0, size)
      } else {
        val chars = CharArray(size)
        buffer.asCharBuffer().get(chars)
        String(chars)
      }
      position += size * Char.SIZE_BYTES
      value
    }
  }

  /**
   * Reads a string written in the ASCII form, as [putAsciiString] writes it. The position does not
   * move if it throws.
   *
   * @throws IllegalStateException if the string is in the UTF-16 form
   */
  fun getAsciiString(): String {
    check(buffer.getInt(buffer.position()) >= 0) { "Expected an ASCII string, found a UTF-16 one" }
    return getString()
  }

  /**
   * Reads a string written in the UTF-16 form, as [putUtf16String] writes it. The position does not
   * move if it throws.
   *
   * @throws IllegalStateException if the string is in the ASCII form
   */
  fun getUtf16String(): String {
    // Reads through getString: a reader of its own measured slower on HotSpot for short strings.
    check(buffer.getInt(buffer.position()) <= 0) { "Expected a UTF-16 string, found an ASCII one" }
    return getString()
  }

  fun putDoubleArray(values: DoubleArray) = apply {
    buffer.putInt(values.size)
    putDoubles(values)
  }

  fun putIntArray(values: IntArray) = apply {
    buffer.putInt(values.size)
    putInts(values)
  }

  fun putLongArray(values: LongArray) = apply {
    buffer.putInt(values.size)
    putLongs(values)
  }

  fun putFloatArray(values: FloatArray) = apply {
    buffer.putInt(values.size)
    putFloats(values)
  }

  fun putBooleanArray(values: BooleanArray) = apply {
    buffer.putInt(values.size)
    for (value in values) {
      putBoolean(value)
    }
  }

  fun putByteArray(values: ByteArray) = apply {
    buffer.putInt(values.size)
    buffer.put(values)
  }

  fun putStringCollection(values: Collection<String>) = apply {
    buffer.putInt(values.size)
    for (value in values) {
      putString(value)
    }
  }

  fun getDoubleArray(): DoubleArray = getDoubles(getInt())

  fun getIntArray(): IntArray = getInts(getInt())

  fun getLongArray(): LongArray = getLongs(getInt())

  fun getFloatArray(): FloatArray = getFloats(getInt())

  fun getBooleanArray(): BooleanArray =
    BooleanArray(getInt()) {
      getBoolean()
    }

  fun getByteArray(): ByteArray = getBytes(getInt())

  fun putDoubles(values: DoubleArray) = apply {
    buffer.asDoubleBuffer().put(values)
    position += values.size * Double.SIZE_BYTES
  }

  fun putInts(values: IntArray) = apply {
    buffer.asIntBuffer().put(values)
    position += values.size * Int.SIZE_BYTES
  }

  fun putLongs(values: LongArray) = apply {
    buffer.asLongBuffer().put(values)
    position += values.size * Long.SIZE_BYTES
  }

  fun putFloats(values: FloatArray) = apply {
    buffer.asFloatBuffer().put(values)
    position += values.size * Float.SIZE_BYTES
  }

  fun putBytes(values: ByteArray) = apply {
    buffer.put(values)
  }

  fun getDoubles(count: Int): DoubleArray =
    DoubleArray(count).also {
      buffer.asDoubleBuffer().get(it)
      position += count * Double.SIZE_BYTES
    }

  fun getInts(count: Int): IntArray =
    IntArray(count).also {
      buffer.asIntBuffer().get(it)
      position += count * Int.SIZE_BYTES
    }

  fun getLongs(count: Int): LongArray =
    LongArray(count).also {
      buffer.asLongBuffer().get(it)
      position += count * Long.SIZE_BYTES
    }

  fun getFloats(count: Int): FloatArray =
    FloatArray(count).also {
      buffer.asFloatBuffer().get(it)
      position += count * Float.SIZE_BYTES
    }

  fun getBytes(count: Int): ByteArray =
    ByteArray(count).also {
      buffer.get(it)
    }

  fun putTaggedNull() = apply {
    putTag(BinaryTag.NULL)
  }

  fun putTaggedBoolean(value: Boolean) = apply {
    putTag(BinaryTag.BOOLEAN)
    putBoolean(value)
  }

  fun putTaggedInt(value: Int) = apply {
    putTag(BinaryTag.INT)
    putInt(value)
  }

  fun putTaggedLong(value: Long) = apply {
    putTag(BinaryTag.LONG)
    putLong(value)
  }

  fun putTaggedFloat(value: Float) = apply {
    putTag(BinaryTag.FLOAT)
    putFloat(value)
  }

  fun putTaggedDouble(value: Double) = apply {
    putTag(BinaryTag.DOUBLE)
    putDouble(value)
  }

  fun putTaggedString(value: String) = apply {
    putTag(BinaryTag.STRING)
    putString(value)
  }

  fun putTaggedByteArray(values: ByteArray) = apply {
    putTag(BinaryTag.BYTE_ARRAY)
    putByteArray(values)
  }

  fun putListHeader(count: Int, elementTag: Int) = apply {
    putTag(BinaryTag.LIST)
    putInt(count)
    putTag(elementTag)
  }

  fun getListHeader(): ListHeader = ListHeader(getInt(), getTag())

  fun putMapHeader(count: Int) = apply {
    putTag(BinaryTag.MAP)
    putInt(count)
  }

  fun getMapSize(): Int = getInt()

  fun putExternalSchemaHeader(schemaId: Int) = apply {
    putTag(BinaryTag.EXTERNAL_SCHEMA)
    putInt(schemaId)
  }

  fun getExternalSchemaId(): Int = getInt()

  companion object {
    /**
     * Whether a `long` stored in this buffer's (native) byte order puts its lowest byte first, so
     * that eight packed characters land in string order.
     */
    private val LITTLE_ENDIAN = ByteOrder.nativeOrder() == ByteOrder.LITTLE_ENDIAN

    /** What [tryPutAscii] found. */
    private const val WROTE_ASCII = 0
    private const val FOUND_NON_ASCII = 1
    private const val NO_ROOM = 2

    fun shared(): BinaryBuffer = threadBuffer.get()

    /**
     * A private buffer of [capacity] bytes, for payloads that do not go through the bridge.
     */
    fun allocate(capacity: Int): BinaryBuffer =
      BinaryBuffer(ByteBuffer.allocateDirect(capacity).order(ByteOrder.nativeOrder()))

    private val threadBuffer = ThreadLocal.withInitial {
      Kolibri.load()
      BinaryBuffer(nativeGetBuffer().order(ByteOrder.nativeOrder()))
    }

    @JvmStatic
    private external fun nativeGetBuffer(): ByteBuffer
  }
}

data class ListHeader(val count: Int, val elementTag: Int)
