package io.github.expo.kolibri.tests

import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertNull
import kotlin.test.assertTrue

/**
 * Every registration shape native_method.h supports: free functions (auto and template-signature),
 * captureless lambdas (auto and explicit signature), receiver plumbing (leading `JNIEnv*`,
 * `JavaReceiver`, both, neither), and `Ref`/`GlobalRef` returns lowered to Java objects.
 * Member-function bindings are covered by NativeObjectTests. The natives live in
 * registration_tests.cpp and are registered through NativeMethodsBuilder like every other suite.
 */
class RegistrationTests : NativeTestBase() {
  private external fun freeFunctionEcho(value: Int): Int
  private external fun freeFunctionWithSignatureTemplate(value: Long): Long
  private external fun lambdaConcat(a: String, b: String): String
  private external fun lambdaWithExplicitSignature(value: Int): Int
  private external fun envOnly(): Boolean
  private external fun receiverOnly(): Boolean
  private external fun envAndReceiver(): Boolean
  private external fun noEnvNoReceiver(a: Int, b: Int): Int
  private external fun refReturn(): String
  private external fun nullableRefReturn(returnNull: Boolean): String?
  private external fun globalRefReturn(): String

  @Test
  fun `a free function binds with an auto-generated signature`() {
    assertEquals(7, freeFunctionEcho(7))
  }

  @Test
  fun `a free function binds with a signature supplied as a template argument`() {
    assertEquals(1L shl 40, freeFunctionWithSignatureTemplate(1L shl 40))
  }

  @Test
  fun `a captureless lambda binds with an auto-generated signature`() {
    assertEquals("foobar", lambdaConcat("foo", "bar"))
  }

  @Test
  fun `a captureless lambda binds with an explicit signature string`() {
    assertEquals(42, lambdaWithExplicitSignature(41))
  }

  @Test
  fun `a leading JNIEnv parameter is injected, not read from Java arguments`() {
    assertTrue(envOnly())
  }

  @Test
  fun `a JavaReceiver parameter receives the Java receiver`() {
    assertTrue(receiverOnly())
  }

  @Test
  fun `JNIEnv and JavaReceiver can both be requested`() {
    assertTrue(envAndReceiver())
  }

  @Test
  fun `a binding with neither env nor receiver sees only the Java arguments`() {
    assertEquals(5, noEnvNoReceiver(2, 3))
  }

  @Test
  fun `a returned local Ref is released to Java as the result object`() {
    assertEquals("owned local", refReturn())
  }

  @Test
  fun `an empty returned Ref becomes null`() {
    assertNull(nullableRefReturn(true))
    assertEquals("present", nullableRefReturn(false))
  }

  @Test
  fun `a returned GlobalRef is lowered to a fresh local reference`() {
    assertEquals("from global", globalRefReturn())
  }
}
