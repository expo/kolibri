package io.github.expo.kolibri.tests

import kotlin.test.Test
import kotlin.test.assertEquals

class NativeObjectTests : NativeTestBase() {
  private external fun nativeThisValue(peer: TestPeer): Int

  @Test
  fun `a peer's C++ state round-trips through member bindings`() {
    val peer = TestPeer(7)
    try {
      assertEquals(7, peer.value())
      peer.setValue(19)
      assertEquals(19, peer.value())
    } finally {
      peer.destroyNow()
    }
  }

  @Test
  fun `nativeThis recovers the same C++ object the handle owns`() {
    val peer = TestPeer(123)
    try {
      assertEquals(123, nativeThisValue(peer))
      peer.setValue(456)
      assertEquals(456, nativeThisValue(peer))
    } finally {
      peer.destroyNow()
    }
  }

  @Test
  fun `destroy frees the peer exactly once and is idempotent`() {
    val before = TestPeer.liveCount()
    val peer = TestPeer(1)
    assertEquals(before + 1, TestPeer.liveCount())

    peer.destroyNow()
    assertEquals(before, TestPeer.liveCount())
    assertEquals(0, peer.nativePointer)

    peer.destroyNow()
    assertEquals(before, TestPeer.liveCount())
  }

  @Test
  fun `the cleaner frees peers once their handles are unreachable`() {
    allocateGarbagePeers(32)
    val deadline = System.currentTimeMillis() + 20_000
    while (TestPeer.liveCount() > 0 && System.currentTimeMillis() < deadline) {
      System.gc()
      Thread.sleep(50)
    }
    assertEquals(0, TestPeer.liveCount(), "cleaner did not free all unreachable peers in time")
  }

  private fun allocateGarbagePeers(count: Int) {
    repeat(count) { TestPeer(it) }
  }
}
