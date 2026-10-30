package io.github.expo.kolibri.tests

import java.util.concurrent.ConcurrentLinkedQueue
import kotlin.concurrent.thread
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFailsWith
import kotlin.test.assertTrue

class ExceptionTests : NativeTestBase() {
  private external fun nativeThrowJavaException(message: String)
  private external fun nativeThrowRuntimeDirect(message: String)
  private external fun nativeRethrowFromCallback(): Boolean
  private external fun nativePropagateFromCallback()
  private external fun nativeThrowWithPending(context: String)

  @Test
  fun `a JavaException thrown in C++ surfaces as a RuntimeException with its message`() {
    val error = assertFailsWith<RuntimeException> { nativeThrowJavaException("boom from C++") }
    assertEquals("boom from C++", error.message)
  }

  @Test
  fun `throwJavaRuntimeException sets a pending exception that surfaces after the native returns`() {
    val error = assertFailsWith<RuntimeException> { nativeThrowRuntimeDirect("direct throw") }
    assertEquals("direct throw", error.message)
  }

  @Test
  fun `a throwing Kotlin callback is caught in C++ as a JavaException carrying class and message`() {
    assertTrue(nativeRethrowFromCallback())
  }

  @Test
  fun `a throwing Kotlin callback propagates back to Java through the guard`() {
    val error = assertFailsWith<RuntimeException> { nativePropagateFromCallback() }
    val message = error.message!!
    assertTrue("IllegalArgumentException" in message, "expected the original class in: $message")
    assertTrue("callback failed" in message, "expected the original message in: $message")
  }

  @Test
  fun `throwWithPending prefixes its context onto the pending exception`() {
    val error = assertFailsWith<RuntimeException> { nativeThrowWithPending("while testing") }
    val message = error.message!!
    assertTrue("while testing" in message, "expected the C++ context in: $message")
    assertTrue("pending failure" in message, "expected the pending message in: $message")
  }

  @Test
  fun `exception conversions keep their messages when repeated from many threads at once`() {
    // Both directions look their Java members up once and cache them for the process; every
    // later conversion, on any thread, has to produce the same exception and message.
    val failures = ConcurrentLinkedQueue<Throwable>()
    val threads = List(8) { index ->
      thread {
        try {
          repeat(200) { round ->
            val message = "thread $index round $round"
            val error = assertFailsWith<RuntimeException> { nativeThrowRuntimeDirect(message) }
            assertEquals(message, error.message)
            assertTrue(nativeRethrowFromCallback())
          }
        } catch (failure: Throwable) {
          failures.add(failure)
        }
      }
    }
    threads.forEach { it.join() }
    failures.firstOrNull()?.let { throw it }
  }

  companion object {
    @JvmStatic
    fun throwingCallback() {
      throw IllegalArgumentException("callback failed")
    }
  }
}
