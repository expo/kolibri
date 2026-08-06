package io.github.expo.kolibri.tests

import io.github.expo.kolibri.binary.BinaryBuffer
import io.github.expo.kolibri.binary.DID_NOT_FIT
import io.github.expo.kolibri.binary.decodeValue
import io.github.expo.kolibri.binary.encodeValue
import kotlin.concurrent.thread
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertNotEquals
import kotlin.test.assertTrue

class BinaryCodecTests : NativeTestBase() {
  private external fun nativeVerifyKotlinPayload(length: Int): Boolean
  private external fun nativeVerifyDeepList(length: Int, expectedDepth: Int, innermost: Int): Boolean
  private external fun nativeEncodePayload(): Int
  private external fun nativeBufferAddress(): Long
  private external fun nativeClaimSemantics(): Boolean
  private external fun nativeCppRoundTrips(): Boolean
  private external fun nativeOverflowReturnsFalse(): Boolean

  @Test
  fun `a Kotlin-encoded payload decodes correctly in C++`() {
    val payload = listOf(
      null,
      true,
      42,
      1L shl 40,
      2.25,
      "ascii",
      "odd",
      "café",
      1.5f,
      byteArrayOf(1, 2, 3),
      listOf(1, 2, 3),
      mapOf("key" to "value"),
    )
    val length = BinaryBuffer.shared().encodeValue(payload)
    assertTrue(length > 0, "payload unexpectedly did not fit")
    assertTrue(nativeVerifyKotlinPayload(length))
  }

  @Test
  fun `a deeply nested Kotlin payload walks correctly in C++`() {
    val depth = 99
    var value: Any? = 7
    repeat(depth) { value = listOf(value) }
    val length = BinaryBuffer.shared().encodeValue(value)
    assertTrue(length > 0)
    assertTrue(nativeVerifyDeepList(length, depth, 7))
  }

  @Test
  fun `a C++-encoded payload decodes correctly in Kotlin`() {
    val length = nativeEncodePayload()
    assertTrue(length > 0)
    val decoded = BinaryBuffer.shared().decodeValue(length)
    assertEquals(listOf(null, true, -7, 3.5, "hello", "zażółć", "gęś 🦆"), decoded)
  }

  @Test
  fun `the shared buffer is stable per thread and distinct across threads`() {
    val here = nativeBufferAddress()
    assertEquals(here, nativeBufferAddress())

    var elsewhere = 0L
    thread { elsewhere = nativeBufferAddress() }.join()
    assertNotEquals(here, elsewhere)
  }

  @Test
  fun `a claim blocks nested claims until released`() {
    assertTrue(nativeClaimSemantics())
  }

  @Test
  fun `the C++ BinaryConverter round-trips every supported shape`() {
    assertTrue(nativeCppRoundTrips())
  }

  @Test
  fun `an oversized payload reports overflow instead of growing the buffer`() {
    // 40k doubles = 320 KiB > the fixed 256 KiB capacity, on both sides of the bridge.
    assertTrue(nativeOverflowReturnsFalse())
    val tooBig = DoubleArray(40_000)
    assertEquals(DID_NOT_FIT, BinaryBuffer.shared().encodeValue(tooBig))
  }
}
