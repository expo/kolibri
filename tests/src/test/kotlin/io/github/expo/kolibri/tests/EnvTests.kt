package io.github.expo.kolibri.tests

import kotlin.test.Test
import kotlin.test.assertTrue

class EnvTests : NativeTestBase() {
  private external fun nativeEnvOnJniThread(): Boolean
  private external fun nativeGetEnvAttachesFreshThread(): Boolean

  @Test
  fun `the JNI thread's env is returned as-is`() {
    assertTrue(nativeEnvOnJniThread())
  }

  @Test
  fun `getEnv attaches a fresh native thread while getEnvIfAttached never does`() {
    assertTrue(nativeGetEnvAttachesFreshThread())
  }
}
