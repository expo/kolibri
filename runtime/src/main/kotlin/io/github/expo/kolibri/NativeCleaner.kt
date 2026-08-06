package io.github.expo.kolibri

import java.lang.ref.PhantomReference
import java.lang.ref.Reference
import java.lang.ref.ReferenceQueue
import java.util.Collections
import java.util.concurrent.ConcurrentHashMap
import java.util.concurrent.atomic.AtomicBoolean

/**
 * Shared cleaner for native-backed handles ([NativeObject] subclasses).
 */
object NativeCleaner {
  val cleaner: Cleaner = Cleaner.create()
}

/**
 * A [Runnable] that frees the native object at [pointer].
 */
class NativeDeallocator(
  private var pointer: Long,
  private val free: (Long) -> Unit,
) : Runnable {
  override fun run() = synchronized(this) {
    free(pointer)
    pointer = 0
  }
}

/**
 * A registration handle returned by [Cleaner.register]. Calling [clean] runs the associated action
 * immediately (deterministic teardown) instead of waiting for the GC. It should be idempotent and race
 * safely against the cleaner thread.
 */
interface Cleanable {
  fun clean()
}

/**
 * A minimal backport of [java.lang.ref.Cleaner], which only exists on API 33+ (Android 13). We need
 * the exact same GC-driven cleanup on every version we support, so this reimplements it on the
 * reference primitives available since API 1: a [PhantomReference] per registration, a shared
 * [ReferenceQueue], and one daemon thread that drains the queue.
 *
 * Semantics match the platform [java.lang.ref.Cleaner] we replace:
 *  - the cleaning action runs at most once, whether triggered by GC or by an explicit [Cleanable.clean]
 *  - the action never sees the referent (a [PhantomReference] can't be dereferenced), so it cannot
 *    resurrect it
 *  - a throwing action is contained and never kills the cleaner thread
 */
class Cleaner private constructor() {
  private val queue = ReferenceQueue<Any>()

  private val registrations: MutableSet<CleanableImpl> =
    Collections.newSetFromMap(ConcurrentHashMap())

  /**
   * Registers [obj] so that [action] runs once [obj] becomes phantom-reachable (or once the returned
   * [Cleanable] is cleaned, whichever comes first). [action] MUST NOT reference [obj], directly or by
   * capture - doing so keeps [obj] reachable forever and the action would never run.
   */
  fun register(obj: Any, action: Runnable): Cleanable {
    val cleanable = CleanableImpl(obj, action)
    registrations.add(cleanable)
    return cleanable
  }

  private inner class CleanableImpl(
    referent: Any,
    private val action: Runnable,
  ) : PhantomReference<Any>(referent, queue), Cleanable {
    // Guards the action so it runs exactly once even when an explicit clean() races the GC draining
    // this same reference off the queue.
    private val cleaned = AtomicBoolean(false)

    override fun clean() {
      if (cleaned.compareAndSet(false, true)) {
        registrations.remove(this)
        // Drop the reference so the GC won't enqueue it after we've already run the action.
        clear()
        action.run()
      }
    }
  }

  companion object {
    fun create(): Cleaner = Cleaner().also { it.startDrainThread() }
  }

  private fun startDrainThread() {
    val thread = Thread({
      while (true) {
        try {
          // Blocks until a registration is enqueued; the cast is safe because CleanableImpl is the
          // only thing we ever put on this queue.
          val ref = queue.remove() as Reference<*>
          (ref as Cleanable).clean()
        } catch (_: InterruptedException) {
          // Nothing to hand off to; just keep draining.
        } catch (_: Throwable) {
          // A cleaning action threw. Swallow it — one bad free must not tear down the thread and
          // leak every other handle's native memory.
        }
      }
    }, "kolibri-cleaner")
    thread.isDaemon = true
    thread.start()
  }
}
