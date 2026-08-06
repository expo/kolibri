package io.github.expo.kolibri.tests

import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertTrue

class StringTests : NativeTestBase() {
  /** toStdString (UTF-16 → UTF-8) then toJString (UTF-8 → UTF-16). */
  private external fun nativeRoundTripUtf8(value: String): String

  /** toU16StdString then the u16string_view toJString overload — no transcoding. */
  private external fun nativeRoundTripUtf16(value: String): String

  private external fun nativeUtf8Conversions(): Boolean
  private external fun nativeEmptyStringIsNotNull(): Boolean

  private fun assertRoundTrips(value: String) {
    assertEquals(value, nativeRoundTripUtf8(value), "UTF-8 round trip")
    assertEquals(value, nativeRoundTripUtf16(value), "UTF-16 round trip")
  }

  @Test
  fun `ascii strings round-trip`() {
    assertRoundTrips("plain ascii 123")
  }

  @Test
  fun `latin-1 and multi-byte text round-trips`() {
    assertRoundTrips("café żółć")
    assertRoundTrips("日本語のテキスト")
  }

  @Test
  fun `non-BMP characters round-trip as surrogate pairs`() {
    assertRoundTrips("emoji 😀 and beyond 🤯")
  }

  @Test
  fun `lone surrogates round-trip through both paths`() {
    // The UTF-8 path is WTF-8-style: a lone surrogate becomes a 3-byte sequence and decodes back.
    assertRoundTrips("lone high \uD800 and low \uDFFF")
  }

  @Test
  fun `embedded NUL is preserved (real UTF-8, not modified UTF-8)`() {
    assertRoundTrips("before\u0000after")
  }

  @Test
  fun `lengths around the stack-buffer threshold round-trip`() {
    // Non-ASCII payload so the UTF-8 arm actually transcodes on both sides of the boundary.
    for (length in intArrayOf(STACK_UNITS - 1, STACK_UNITS, STACK_UNITS + 1, 4 * STACK_UNITS)) {
      assertRoundTrips("ż".repeat(length))
    }
  }

  @Test
  fun `C++ level utf8 and utf16 conversion units behave`() {
    assertTrue(nativeUtf8Conversions())
  }

  @Test
  fun `empty strings become empty Java strings, never null`() {
    assertRoundTrips("")
    assertTrue(nativeEmptyStringIsNotNull())
  }

  companion object {
    /** Mirrors kStackUnits in string_utils.h. */
    const val STACK_UNITS = 512
  }
}
