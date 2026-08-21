package io.github.expo.kolibri.binary

const val STRING_SCRATCH_MIN_CAPACITY = 128
const val MAX_RETAINED_STRING_UNITS = 4 * 1024

internal class StringScratch {
  private var bytes = ByteArray(STRING_SCRATCH_MIN_CAPACITY)
  private var chars = CharArray(STRING_SCRATCH_MIN_CAPACITY)

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
    Integer.highestOneBit(size - 1).shl(1).coerceAtMost(MAX_RETAINED_STRING_UNITS)
}

internal val stringScratch = ThreadLocal.withInitial(::StringScratch)
