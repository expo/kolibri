package io.github.expo.kolibri.binary

internal const val STRING_SCRATCH_THRESHOLD = 128
internal const val MAX_RETAINED_STRING_UNITS = 4 * 1024

internal class StringScratch {
  private var bytes = ByteArray(0)
  private var chars = CharArray(0)

  fun bytes(size: Int): ByteArray {
    if (bytes.size < size) {
      bytes = ByteArray(nextCapacity(size))
    }
    return bytes
  }

  fun chars(size: Int): CharArray {
    if (chars.size < size) {
      chars = CharArray(nextCapacity(size))
    }
    return chars
  }

  private fun nextCapacity(size: Int): Int =
    Integer.highestOneBit(size - 1).shl(1).coerceAtLeast(STRING_SCRATCH_THRESHOLD)
}

internal val stringScratch = ThreadLocal.withInitial(::StringScratch)
