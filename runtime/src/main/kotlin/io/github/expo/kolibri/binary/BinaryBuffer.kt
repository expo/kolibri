package io.github.expo.kolibri.binary

import io.github.expo.kolibri.Kolibri
import io.github.expo.kolibri.isAscii
import java.nio.ByteBuffer
import java.nio.ByteOrder

@JvmInline
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
    val useScratch = size in STRING_SCRATCH_THRESHOLD..MAX_RETAINED_STRING_UNITS

    if (value.isAscii()) {
      buffer.putInt(size)
      if (useScratch) {
        val bytes = stringScratch.get().bytes(size)
        value.copyAsciiTo(bytes)
        buffer.put(bytes, 0, size)
      } else {
        buffer.put(value.toByteArray(Charsets.ISO_8859_1))
      }
    } else {
      buffer.putInt(-size)
      if (useScratch) {
        val chars = stringScratch.get().chars(size)
        value.toCharArray(chars, 0, 0, size)
        buffer.asCharBuffer().put(chars, 0, size)
      } else {
        buffer.asCharBuffer().put(value)
      }
      position += size * Char.SIZE_BYTES
    }
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
      if (prefix in STRING_SCRATCH_THRESHOLD..MAX_RETAINED_STRING_UNITS) {
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
      val value = if (size in STRING_SCRATCH_THRESHOLD..MAX_RETAINED_STRING_UNITS) {
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
