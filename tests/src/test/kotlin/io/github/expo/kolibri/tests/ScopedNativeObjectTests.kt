package io.github.expo.kolibri.tests

import kotlin.test.Test
import kotlin.test.assertEquals

class ScopedNativeObjectTests : NativeTestBase() {
  @Test
  fun `invalidateScope fires each bound object exactly once`() {
    val scopeA = 0xA100L
    val scopeB = 0xB100L
    val first = ScopedPeer(scopeA)
    val second = ScopedPeer(scopeA)
    val other = ScopedPeer(scopeB)
    try {
      ScopedPeer.invalidateScope(scopeA)
      assertEquals(1, first.invalidatedCount())
      assertEquals(1, second.invalidatedCount())
      assertEquals(0, other.invalidatedCount())

      // The scope is gone after invalidation; a second invalidation must not re-fire.
      ScopedPeer.invalidateScope(scopeA)
      assertEquals(1, first.invalidatedCount())
      assertEquals(1, second.invalidatedCount())
    } finally {
      first.destroyNow()
      second.destroyNow()
      other.destroyNow()
    }
  }

  @Test
  fun `releaseScopedResources fires the hook immediately and unbinds`() {
    val scope = 0xC100L
    val peer = ScopedPeer(scope)
    try {
      peer.releaseScopedResources()
      assertEquals(1, peer.invalidatedCount())

      // Unbound: a later scope invalidation must not fire the hook again.
      ScopedPeer.invalidateScope(scope)
      assertEquals(1, peer.invalidatedCount())

      // Releasing again is a no-op once unbound.
      peer.releaseScopedResources()
      assertEquals(1, peer.invalidatedCount())
    } finally {
      peer.destroyNow()
    }
  }

  @Test
  fun `a destroyed object unbinds and invalidation only reaches live peers`() {
    val scope = 0xD100L
    val destroyed = ScopedPeer(scope)
    val survivor = ScopedPeer(scope)
    try {
      destroyed.destroyNow()
      // Must not touch the freed peer; only the survivor gets the callback.
      ScopedPeer.invalidateScope(scope)
      assertEquals(1, survivor.invalidatedCount())
    } finally {
      survivor.destroyNow()
    }
  }
}
