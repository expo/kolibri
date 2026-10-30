package io.github.expo.kolibri.benchmarks;

/**
 * The Java side of native_call_benchmarks.cpp: native methods the C++ side registers, and loops
 * that call them. A measured call is a real Java -> native transition made from JIT-compiled
 * code, the way an app calls a kolibri @NativeMethod. Plain Java, compiled by CMake, so the
 * benchmarks still need nothing from the Gradle/Kotlin build on their class path.
 */
final class NativeCalls {
  /** What a @NativeMethod receiver holds: the C++ peer's address. */
  final long nativePointer;

  /** Fields member_benchmarks.cpp reads and writes through kolibri's Field token. */
  int counter;

  Object reference;

  NativeCalls(long nativePointer) {
    this.nativePointer = nativePointer;
  }

  // A free function, (II)I: raw JNI, then kolibri's binding, guarded and noexcept.
  static native int rawAdd(int a, int b);

  static native int add(int a, int b);

  static native int addNoexcept(int a, int b);

  // A peer method, (J)I: kolibri receives the peer's address as an argument (the raw function
  // does the same by hand); the fbjni-style baseline reads it from the receiver's field instead.
  static native int rawPeerValue(long peer);

  native int rawPeerValueFromField();

  static native int peerValue(long peer);

  static native int peerValueNoexcept(long peer);

  // A native that throws: kolibri turns the C++ exception into a RuntimeException; the raw one
  // raises it with ThrowNew.
  static native void rawFail();

  static native void fail();

  static long loopRawAdd(int n) {
    long sum = 0;
    for (int i = 0; i < n; i++) sum += rawAdd(i, 1);
    return sum;
  }

  static long loopAdd(int n) {
    long sum = 0;
    for (int i = 0; i < n; i++) sum += add(i, 1);
    return sum;
  }

  static long loopAddNoexcept(int n) {
    long sum = 0;
    for (int i = 0; i < n; i++) sum += addNoexcept(i, 1);
    return sum;
  }

  static long loopRawPeerValue(long peer, int n) {
    long sum = 0;
    for (int i = 0; i < n; i++) sum += rawPeerValue(peer);
    return sum;
  }

  static long loopRawPeerValueFromField(long peer, int n) {
    final NativeCalls receiver = new NativeCalls(peer);
    long sum = 0;
    for (int i = 0; i < n; i++) sum += receiver.rawPeerValueFromField();
    return sum;
  }

  static long loopPeerValue(long peer, int n) {
    long sum = 0;
    for (int i = 0; i < n; i++) sum += peerValue(peer);
    return sum;
  }

  static long loopPeerValueNoexcept(long peer, int n) {
    long sum = 0;
    for (int i = 0; i < n; i++) sum += peerValueNoexcept(peer);
    return sum;
  }

  static long loopRawFail(int n) {
    long caught = 0;
    for (int i = 0; i < n; i++) {
      try {
        rawFail();
      } catch (RuntimeException e) {
        caught++;
      }
    }
    return caught;
  }

  static long loopFail(int n) {
    long caught = 0;
    for (int i = 0; i < n; i++) {
      try {
        fail();
      } catch (RuntimeException e) {
        caught++;
      }
    }
    return caught;
  }
}
