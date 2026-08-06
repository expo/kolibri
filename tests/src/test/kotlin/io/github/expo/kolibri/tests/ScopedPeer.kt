package io.github.expo.kolibri.tests

import io.github.expo.kolibri.Kolibri
import io.github.expo.kolibri.NativeObject
import io.github.expo.kolibri.NativePointer

class ScopedPeer(scopeId: Long) : NativeObject(create(scopeId)) {
  fun invalidatedCount(): Int = nativeInvalidatedCount(nativePointer)

  fun releaseScopedResources() = nativeReleaseScopedResources(nativePointer)

  fun destroyNow() = destroy()

  companion object {
    init {
      Kolibri.load()
    }

    private fun create(scopeId: Long) = NativePointer(nativeCreate(scopeId))

    @JvmStatic
    external fun invalidateScope(scopeId: Long)

    @JvmStatic
    private external fun nativeCreate(scopeId: Long): Long

    @JvmStatic
    private external fun nativeInvalidatedCount(pointer: Long): Int

    @JvmStatic
    private external fun nativeReleaseScopedResources(pointer: Long)
  }
}
