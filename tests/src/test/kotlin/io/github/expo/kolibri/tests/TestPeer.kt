package io.github.expo.kolibri.tests

import io.github.expo.kolibri.Kolibri
import io.github.expo.kolibri.NativeObject
import io.github.expo.kolibri.NativePointer

class TestPeer(initialValue: Int) : NativeObject(create(initialValue)) {
  fun value(): Int = nativeGetValue(nativePointer)

  fun setValue(value: Int) = nativeSetValue(nativePointer, value)

  fun destroyNow() = destroy()

  companion object {
    init {
      Kolibri.load()
    }

    private fun create(initialValue: Int) = NativePointer(nativeCreate(initialValue))

    @JvmStatic
    external fun liveCount(): Int

    @JvmStatic
    private external fun nativeCreate(initialValue: Int): Long

    @JvmStatic
    private external fun nativeGetValue(pointer: Long): Int

    @JvmStatic
    private external fun nativeSetValue(pointer: Long, value: Int)
  }
}
