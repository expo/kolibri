package io.github.expo.kolibri.tests

import io.github.expo.kolibri.Cleaner
import java.util.concurrent.atomic.AtomicInteger
import kotlin.test.Test
import kotlin.test.assertEquals

class CleanerTests {
  private val cleaner = Cleaner.create()

  @Test
  fun `an explicit clean runs the action once and later cleans are no-ops`() {
    val runs = AtomicInteger()
    val cleanable = cleaner.register(Any()) { runs.incrementAndGet() }

    cleanable.clean()
    cleanable.clean()

    assertEquals(1, runs.get())
  }

  @Test
  fun `a pointer registration hands the pointer to the shared free exactly once`() {
    val freed = mutableListOf<Long>()
    val free = Cleaner.PointerFree { freed.add(it) }
    val cleanable = cleaner.registerPointer(Any(), 42L, free)

    cleanable.clean()
    cleanable.clean()

    assertEquals(listOf(42L), freed)
  }

  @Test
  fun `the cleaner runs actions for unreachable referents and leaves cleaned ones alone`() {
    val runs = AtomicInteger()
    val alreadyCleaned = AtomicInteger()
    registerGarbage(16, runs)
    cleaner.register(Any()) { alreadyCleaned.incrementAndGet() }.clean()

    val deadline = System.currentTimeMillis() + 20_000
    while (runs.get() < 16 && System.currentTimeMillis() < deadline) {
      System.gc()
      Thread.sleep(50)
    }

    assertEquals(16, runs.get(), "cleaner did not run every action in time")
    assertEquals(1, alreadyCleaned.get())
  }

  @Test
  fun `a throwing action does not stop later ones`() {
    val runs = AtomicInteger()
    registerThrowingGarbage()
    registerGarbage(8, runs)

    val deadline = System.currentTimeMillis() + 20_000
    while (runs.get() < 8 && System.currentTimeMillis() < deadline) {
      System.gc()
      Thread.sleep(50)
    }

    assertEquals(8, runs.get())
  }

  private fun registerGarbage(count: Int, runs: AtomicInteger) {
    repeat(count) { cleaner.register(Any()) { runs.incrementAndGet() } }
  }

  private fun registerThrowingGarbage() {
    cleaner.register(Any()) { throw IllegalStateException("boom") }
  }
}
