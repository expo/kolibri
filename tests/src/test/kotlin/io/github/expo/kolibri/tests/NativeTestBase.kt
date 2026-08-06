package io.github.expo.kolibri.tests

import io.github.expo.kolibri.Kolibri

abstract class NativeTestBase {
  companion object {
    init {
      Kolibri.load()
    }
  }
}
