package io.github.expo.kolibri.tests

import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertTrue

class RefTests : NativeTestBase() {
  private external fun nativeAdoptSemantics(value: String): Boolean
  private external fun nativeMakeAndClone(value: String): Boolean
  private external fun nativeMoveSemantics(value: String): Boolean
  private external fun nativeReleaseAndReset(value: String): Boolean
  private external fun nativeNullHandling(): Boolean
  private external fun nativeUnownedPassThrough(value: String): String
  private external fun nativeGlobalRefAcrossThreads(value: String): String
  private external fun nativeStaticCastBorrows(value: String): Boolean
  private external fun nativeStaticCastTransfersOwnership(value: String): Boolean
  private external fun nativeStaticCastReachesAccessors(values: List<String>): String

  @Test
  fun `adopt takes ownership without creating a new reference`() {
    assertTrue(nativeAdoptSemantics("adopted"))
  }

  @Test
  fun `make and clone create references to the same object`() {
    assertTrue(nativeMakeAndClone("cloned"))
  }

  @Test
  fun `move construction and assignment null out the source`() {
    assertTrue(nativeMoveSemantics("moved"))
  }

  @Test
  fun `release hands back the raw handle and reset empties the ref`() {
    assertTrue(nativeReleaseAndReset("released"))
  }

  @Test
  fun `null refs are falsy and safe to reset and clone`() {
    assertTrue(nativeNullHandling())
  }

  @Test
  fun `unowned refs pass the handle through without owning it`() {
    assertEquals("unowned", nativeUnownedPassThrough("unowned"))
  }

  @Test
  fun `casting an owned ref yields a non-owning retyped view`() {
    assertTrue(nativeStaticCastBorrows("borrowed"))
  }

  @Test
  fun `casting an rvalue ref moves the handle into the new type`() {
    assertTrue(nativeStaticCastTransfersOwnership("transferred"))
  }

  @Test
  fun `a cast ref exposes the accessors of the target class`() {
    assertEquals("onetwo", nativeStaticCastReachesAccessors(listOf("one", "two")))
  }

  @Test
  fun `a global ref is usable and releasable from a natively attached thread`() {
    assertEquals("cross-thread", nativeGlobalRefAcrossThreads("cross-thread"))
  }
}
