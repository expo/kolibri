package io.github.expo.kolibri.binary

import java.nio.BufferOverflowException
import java.nio.BufferUnderflowException

/**
 * A consumer-side codec for `Any?`, written against [BinaryBuffer]'s primitive operations.
 *
 * Kolibri deliberately does not ship this: the buffer knows tags, fixed-type values, bulk runs and
 * container headers, and every generic `Any` walk belongs to whoever owns the value types (the API
 * module has its own, schema-directed). This one is the reference walk for the standard Kotlin
 * shapes — `null`, primitives, strings, lists, maps with string keys, and primitive arrays — and it
 * exists so the format tests and the C++ interop tests have both sides of the wire in Kotlin.
 */

/** Returned by [encodeValue] when the value does not fit in the buffer. */
const val DID_NOT_FIT = -1

/**
 * Writes one dynamic value into `[0, ...)` and returns its length, or [DID_NOT_FIT] when it does
 * not fit. Writes go through a `duplicateView`, so an overflow leaves the caller's cursor alone.
 */
fun BinaryBuffer.encodeValue(value: Any?): Int {
  val buf = duplicateView()
  return try {
    buf.putValue(value)
    buf.position
  } catch (_: BufferOverflowException) {
    DID_NOT_FIT
  }
}

/** Reads back what [encodeValue] wrote: one dynamic value from `[0, length)`. */
fun BinaryBuffer.decodeValue(length: Int): Any? =
  duplicateView().also { it.limit = length }.getValue()

/** Writes one tagged dynamic value at the cursor. */
fun BinaryBuffer.putValue(value: Any?) {
  when (value) {
    null -> putTaggedNull()
    is Boolean -> putTaggedBoolean(value)
    is Int -> putTaggedInt(value)
    is Long -> putTaggedLong(value)
    is Float -> putTaggedFloat(value)
    is Double -> putTaggedDouble(value)
    is String -> putTaggedString(value)
    is List<*> -> putListValue(value)
    is Map<*, *> -> putMapValue(value)
    // Primitive arrays have no tags of their own: they cross as LIST bulk runs, with the payload
    // moved in one memory copy.
    is DoubleArray -> putListHeader(value.size, BinaryTag.DOUBLE).putDoubles(value)
    is IntArray -> putListHeader(value.size, BinaryTag.INT).putInts(value)
    is LongArray -> putListHeader(value.size, BinaryTag.LONG).putLongs(value)
    is FloatArray -> putListHeader(value.size, BinaryTag.FLOAT).putFloats(value)
    is BooleanArray -> {
      putListHeader(value.size, BinaryTag.BOOLEAN)
      for (element in value) putBoolean(element)
    }
    // ByteArray keeps its own tag: it is a binary blob (a JS ArrayBuffer), not a number list.
    is ByteArray -> putTaggedByteArray(value)
    else -> throw IllegalArgumentException(
      "Cannot binary-encode a value of type ${value.javaClass.name}",
    )
  }
}

/** Reads one tagged dynamic value at the cursor. Consumer-owned tags are rejected. */
fun BinaryBuffer.getValue(): Any? {
  // The tag is consumed here, so each branch reads its payload untagged.
  return when (val tag = getTag()) {
    BinaryTag.NULL -> null
    BinaryTag.BOOLEAN -> getBoolean()
    BinaryTag.INT -> getInt()
    BinaryTag.LONG -> getLong()
    BinaryTag.FLOAT -> getFloat()
    BinaryTag.DOUBLE -> getDouble()
    BinaryTag.STRING -> getString()
    BinaryTag.LIST -> getListValue()
    BinaryTag.MAP -> getMapValue()
    BinaryTag.BYTE_ARRAY -> getByteArray()
    BinaryTag.EXTERNAL_SCHEMA -> throw UnsupportedOperationException(
      "EXTERNAL_SCHEMA ${getExternalSchemaId()} must be decoded by the binary format consumer",
    )

    else -> throw IllegalStateException("Unknown binary tag: $tag")
  }
}

/**
 * Writes [list] as an untagged scalar bulk run and returns true when all of its elements are one
 * non-null scalar type; otherwise writes nothing and returns false.
 */
fun BinaryBuffer.tryPutScalarList(list: List<*>): Boolean {
  val scalarTag = scalarTagOf(list) ?: return false
  putListHeader(list.size, scalarTag)
  for (element in list) putScalar(element!!)
  return true
}

private fun BinaryBuffer.putListValue(list: List<*>) {
  if (tryPutScalarList(list)) return
  putListHeader(list.size, BinaryTag.TAGGED)
  for (element in list) putValue(element)
}

private fun BinaryBuffer.getListValue(): ArrayList<Any?> {
  val (count, elementTag) = getListHeader()
  // The count sizes the list below, so bound it first: every element occupies at least one buffer
  // byte, so a count beyond the readable remainder is a corrupt payload.
  if (count < 0 || count > remaining) throw BufferUnderflowException()
  val list = ArrayList<Any?>(count)
  when (elementTag) {
    BinaryTag.TAGGED -> repeat(count) { list.add(getValue()) }
    BinaryTag.DOUBLE -> getDoubles(count).forEach { list.add(it) }
    BinaryTag.INT -> getInts(count).forEach { list.add(it) }
    BinaryTag.LONG -> getLongs(count).forEach { list.add(it) }
    BinaryTag.FLOAT -> getFloats(count).forEach { list.add(it) }
    BinaryTag.BOOLEAN -> repeat(count) { list.add(getBoolean()) }
    else -> throw IllegalStateException("Unknown list element tag: $elementTag")
  }
  return list
}

private fun BinaryBuffer.putMapValue(map: Map<*, *>) {
  putMapHeader(map.size)
  for ((key, value) in map) {
    require(key is String) {
      "Cannot binary-encode a Map with non-String key: ${key?.javaClass?.name}"
    }
    putString(key)
    putValue(value)
  }
}

private fun BinaryBuffer.getMapValue(): HashMap<String, Any?> {
  val count = getMapSize()
  // As in [getListValue]: each entry needs at least a key prefix (4 bytes) and a tagged value.
  if (count < 0 || count.toLong() * 5 > remaining.toLong()) throw BufferUnderflowException()
  val map = HashMap<String, Any?>(count * 4 / 3 + 1)
  repeat(count) {
    map[getString()] = getValue()
  }
  return map
}

private fun scalarTagOf(list: List<*>): Int? {
  val first = list.firstOrNull() ?: return null
  val tag = scalarTagOf(first) ?: return null
  return tag.takeIf { candidate -> list.all { it != null && scalarTagOf(it) == candidate } }
}

private fun scalarTagOf(value: Any): Int? = when (value) {
  is Double -> BinaryTag.DOUBLE
  is Int -> BinaryTag.INT
  is Long -> BinaryTag.LONG
  is Float -> BinaryTag.FLOAT
  is Boolean -> BinaryTag.BOOLEAN
  else -> null
}

/** Writes a bulk-run element: the list header already named the type, so no tag of its own. */
private fun BinaryBuffer.putScalar(value: Any) {
  when (value) {
    is Double -> putDouble(value)
    is Int -> putInt(value)
    is Long -> putLong(value)
    is Float -> putFloat(value)
    is Boolean -> putBoolean(value)
    else -> throw IllegalStateException("Not a scalar list element: $value")
  }
}
