package io.github.expo.kolibri

import java.lang.ref.PhantomReference
import java.lang.ref.ReferenceQueue

/**
 * Shared cleaner for native-backed handles ([NativeObject] subclasses).
 */
object NativeCleaner {
  val cleaner: Cleaner = Cleaner.create()
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

  private val lock = Any()

  private var head: Node? = null

  /** Frees the native memory a [registerPointer] registration stands for. Implement it as a singleton. */
  fun interface PointerFree {
    fun free(pointer: Long)
  }

  /**
   * Registers [obj] so that [action] runs once [obj] becomes phantom-reachable (or once the returned
   * [Cleanable] is cleaned, whichever comes first). [action] MUST NOT reference [obj], directly or by
   * capture - doing so keeps [obj] reachable forever and the action would never run.
   */
  fun register(obj: Any, action: Runnable): Cleanable =
    link(ActionNode(obj, action))

  /**
   * Registers [obj] so that [free] runs with [pointer] once [obj] becomes phantom-reachable (or once
   * the returned [Cleanable] is cleaned). Unlike [register] it allocates nothing beyond the reference
   * itself, provided [free] is a shared instance rather than a capturing lambda.
   */
  fun registerPointer(obj: Any, pointer: Long, free: PointerFree): Cleanable =
    link(PointerNode(obj, pointer, free))

  private fun <N : Node> link(node: N): N {
    synchronized(lock) {
      node.next = head
      head?.prev = node
      head = node
      node.linked = true
    }
    return node
  }

  private abstract inner class Node(referent: Any) : PhantomReference<Any>(referent, queue), Cleanable {
    @JvmField
    var prev: Node? = null

    @Suppress("PROPERTY_HIDES_JAVA_FIELD")
    @JvmField
    var next: Node? = null

    /** True while the node is on the list, which is exactly while its action has not run. */
    @JvmField
    var linked = false

    final override fun clean() {
      synchronized(lock) {
        if (!linked) {
          return
        }
        linked = false
        prev?.next = next
        next?.prev = prev
        if (head === this) {
          head = next
        }
        prev = null
        next = null
      }
      // Drop the reference so the GC won't enqueue it after we've already run the action.
      clear()
      perform()
    }

    protected abstract fun perform()
  }

  private inner class ActionNode(referent: Any, private val action: Runnable) : Node(referent) {
    override fun perform() = action.run()
  }

  private inner class PointerNode(
    referent: Any,
    private var pointer: Long,
    private val free: PointerFree,
  ) : Node(referent) {
    override fun perform() {
      val p = pointer
      pointer = 0
      free.free(p)
    }
  }

  companion object {
    fun create(): Cleaner = Cleaner().also { it.startDrainThread() }
  }

  private fun startDrainThread() {
    val thread = Thread({
      while (true) {
        try {
          // Blocks until a registration is enqueued; the cast is safe because Node is the only thing
          // we ever put on this queue.
          (queue.remove() as Cleanable).clean()
        } catch (_: InterruptedException) {
          // Nothing to hand off to; just keep draining.
        } catch (_: Throwable) {
          // A cleaning action threw. Swallow it - one bad free must not tear down the thread and
          // leak every other handle's native memory.
        }
      }
    }, "kolibri-cleaner")
    thread.isDaemon = true
    thread.start()
  }
}
