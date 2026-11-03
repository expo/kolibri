package io.github.expo.kolibri.tests

import java.util.concurrent.ConcurrentLinkedQueue
import kotlin.concurrent.thread
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertTrue

class MemberTests : NativeTestBase() {
  private external fun nativeConstructAndAccess(): Boolean
  private external fun nativeCallInstanceMethods(fixture: MemberFixture): Boolean
  private external fun nativeMutateFields(fixture: MemberFixture): Boolean
  private external fun nativeCallStatics(): Boolean
  private external fun nativeTokenCallbackThrows(fixture: MemberFixture): Boolean
  private external fun nativeBoxedPrimitives(): Boolean
  private external fun nativeBoxAndUnbox(): Boolean
  private external fun nativeTypedUnbox(): Boolean
  private external fun nativeBoxCacheMatchesValueOf(): Boolean
  private external fun nativeUnboxReadsEveryValue(): Boolean
  private external fun nativeTypedUnboxOfNullThrows(): Boolean
  private external fun nativeSumIntegerList(values: List<Int>): Int

  @Test
  fun `constructor token creates an object whose fields are readable`() {
    assertTrue(nativeConstructAndAccess())
  }

  @Test
  fun `instance method tokens call back into Kotlin`() {
    assertTrue(nativeCallInstanceMethods(MemberFixture(0, "unused")))
  }

  @Test
  fun `field tokens read and write instance fields`() {
    val fixture = MemberFixture(1, "initial")
    assertTrue(nativeMutateFields(fixture))
    assertEquals(42, fixture.intField)
    assertEquals("updated", fixture.stringField)
  }

  @Test
  fun `static method and field tokens work without a receiver`() {
    assertTrue(nativeCallStatics())
  }

  @Test
  fun `a throwing Kotlin callback surfaces as a JavaException on the C++ side`() {
    assertTrue(nativeTokenCallbackThrows(MemberFixture(0, "unused")))
  }

  @Test
  fun `boxed primitive tokens round-trip values`() {
    assertTrue(nativeBoxedPrimitives())
  }

  @Test
  fun `box and unbox helpers round-trip every primitive wrapper`() {
    assertTrue(nativeBoxAndUnbox())
  }

  @Test
  fun `typed unbox reads primitives straight from typed refs`() {
    assertTrue(nativeTypedUnbox())
  }

  @Test
  fun `box hands out Java's own cached boxes and the right value for every other one`() {
    assertTrue(nativeBoxCacheMatchesValueOf())
  }

  @Test
  fun `box caches fill correctly when many threads box at once`() {
    val failures = ConcurrentLinkedQueue<Throwable>()
    val threads = List(8) {
      thread {
        try {
          repeat(20) { assertTrue(nativeBoxCacheMatchesValueOf()) }
        } catch (failure: Throwable) {
          failures.add(failure)
        }
      }
    }
    threads.forEach { it.join() }
    failures.firstOrNull()?.let { throw it }
  }

  @Test
  fun `unbox reads extremes and special values exactly`() {
    assertTrue(nativeUnboxReadsEveryValue())
  }

  @Test
  fun `typed unbox of a null ref throws instead of crashing`() {
    assertTrue(nativeTypedUnboxOfNullThrows())
  }

  @Test
  fun `collection tokens iterate a Kotlin list from C++`() {
    assertEquals(60, nativeSumIntegerList(listOf(10, 20, 30)))
  }
}
