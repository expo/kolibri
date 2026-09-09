package io.github.expo.kolibri

/**
 * The address of a C++ object owned by a [NativeObject]. A value class over the raw `long`, so it
 * costs nothing at runtime - it is stored and passed as a plain `long` - while keeping construction type-safe.
 */
@JvmInline
value class NativePointer(val value: Long)

/**
 * The raw C++ address behind [obj], or `0` if it is null. The compiler plugin emits a call to this
 * for each `@AsNativePointer` argument, so a null handle becomes a `0` pointer the native side reads
 * as `nullptr`. Public because the plugin-generated code lives in consumer modules.
 */
fun nativePointerOf(obj: NativeObject?): Long = obj?.nativePointer ?: 0L

private val hasJvmReachabilityFence: Boolean = try {
  @Suppress("CheckResult")
  java.lang.ref.Reference::class.java.getDeclaredMethod("reachabilityFence", Any::class.java)
  true
} catch (_: Throwable) {
  false
}

/**
 * Isolates the JDK 9+/API 28+ call in its own class, so an older ART never loads (or verifies) a
 * class that references the missing method - [reachabilityFenceOf] itself stays fully optimizable.
 */
private object JvmReachabilityFence {
  fun fence(obj: Any?) {
    java.lang.ref.Reference.reachabilityFence(obj)
  }
}

/**
 * Keeps [obj] strongly reachable until this call returns. The compiler plugin emits a call to this
 * after the delegated native call, for each `@AsNativePointer` argument: only the raw `long`
 * crosses the JNI boundary, so without the fence the JIT may treat the handle as dead mid-call and
 * let the GC (via [NativeCleaner]) free the C++ object the native code is still using. Public for
 * the same reason as [nativePointerOf].
 *
 * Below Android API 28 the fence is an empty `synchronized` block: a monitor operation on the handle is a
 * use no runtime eliminates (another thread could be contending the same monitor), so it pins the
 * handle just as hard — at the cost of an uncontended lock/unlock on those devices only.
 */
fun reachabilityFenceOf(obj: NativeObject?) {
  if (hasJvmReachabilityFence) {
    JvmReachabilityFence.fence(obj)
  } else if (obj != null) {
    synchronized(obj) {}
  }
}

abstract class NativeObject(pointer: NativePointer) {
  @Volatile
  @JvmField
  var nativePointer: Long = pointer.value

  private val cleanable: Cleanable = NativeCleaner
    .cleaner
    .registerPointer(
      obj = this,
      pointer = nativePointer,
      free = destroyer
    )

  /**
   * Frees the backing C++ object now. Idempotent - safe to call more than once, and a no-op if the
   * [NativeCleaner] has already run. After this the handle MUST NOT be used.
   */
  fun destroy() = synchronized(this) {
    cleanable.clean()
    nativePointer = 0
  }

  private companion object {
    init {
      Kolibri.load()
    }

    @JvmStatic
    private external fun nativeDestroy(pointer: Long)

    /**
     * Hoisted so every handle shares one instance.
     */
    private val destroyer = Cleaner.PointerFree(::nativeDestroy)
  }
}
