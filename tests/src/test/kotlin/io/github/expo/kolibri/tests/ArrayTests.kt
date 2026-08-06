package io.github.expo.kolibri.tests

import kotlin.test.Test
import kotlin.test.assertContentEquals
import kotlin.test.assertEquals
import kotlin.test.assertNull
import kotlin.test.assertTrue

class ArrayTests : NativeTestBase() {
  private external fun nativeCreateBooleanArray(size: Int): BooleanArray
  private external fun nativeCreateByteArray(size: Int): ByteArray
  private external fun nativeCreateCharArray(size: Int): CharArray
  private external fun nativeCreateShortArray(size: Int): ShortArray
  private external fun nativeCreateIntArray(size: Int): IntArray
  private external fun nativeCreateLongArray(size: Int): LongArray
  private external fun nativeCreateFloatArray(size: Int): FloatArray
  private external fun nativeCreateDoubleArray(size: Int): DoubleArray

  private external fun nativeSumIntArray(values: IntArray): Long
  private external fun nativeSumDoubleArray(values: DoubleArray): Double
  private external fun nativeCountTrue(values: BooleanArray): Int

  private external fun nativeRegionRoundTrip(values: IntArray): Boolean
  private external fun nativePinMutate(values: IntArray): Boolean
  private external fun nativeZeroLengthArrays(): Boolean
  private external fun nativeMakeStringArray(): Array<String?>
  private external fun nativeJoinStringArray(values: Array<String?>): String

  @Test
  fun `arrays created and filled in C++ are visible in Kotlin`() {
    // Each array is filled with the pattern index * k (see array_tests.cpp).
    assertContentEquals(booleanArrayOf(false, true, false, true), nativeCreateBooleanArray(4))
    assertContentEquals(byteArrayOf(0, 2, 4, 6), nativeCreateByteArray(4))
    assertContentEquals(charArrayOf('a', 'b', 'c', 'd'), nativeCreateCharArray(4))
    assertContentEquals(shortArrayOf(0, 3, 6, 9), nativeCreateShortArray(4))
    assertContentEquals(intArrayOf(0, 4, 8, 12), nativeCreateIntArray(4))
    assertContentEquals(longArrayOf(0, 5, 10, 15), nativeCreateLongArray(4))
    assertContentEquals(floatArrayOf(0f, 0.5f, 1f, 1.5f), nativeCreateFloatArray(4))
    assertContentEquals(doubleArrayOf(0.0, 0.25, 0.5, 0.75), nativeCreateDoubleArray(4))
  }

  @Test
  fun `arrays created in Kotlin are readable in C++`() {
    assertEquals(6_000_000_000L, nativeSumIntArray(intArrayOf(2_000_000_000, 2_000_000_000, 2_000_000_000)))
    assertEquals(3.75, nativeSumDoubleArray(doubleArrayOf(1.25, 2.5)))
    assertEquals(2, nativeCountTrue(booleanArrayOf(true, false, true)))
  }

  @Test
  fun `getRegion and setRegion copy sub-ranges with offsets`() {
    val values = intArrayOf(0, 1, 2,  3, 4, 5, 6, 7)
    assertTrue(nativeRegionRoundTrip(values))
    assertContentEquals(intArrayOf(0, 1, 2, 3, 5, 4, 3, 2), values)
  }

  @Test
  fun `pinned spans mutate the Java array through commit and release`() {
    val values = IntArray(5) { it }
    assertTrue(nativePinMutate(values))
    assertContentEquals(intArrayOf(100, 101, 102, 103, 104), values)
  }

  @Test
  fun `zero-length arrays are creatable and safely accessible`() {
    assertTrue(nativeZeroLengthArrays())
    assertContentEquals(intArrayOf(), nativeCreateIntArray(0))
  }

  @Test
  fun `object arrays support setElement and getElement`() {
    val values = nativeMakeStringArray()
    assertContentEquals(arrayOf("first", null, "third"), values)
    assertEquals("first,third", nativeJoinStringArray(arrayOf("first", null, "third")))
    assertNull(values[1])
  }
}
