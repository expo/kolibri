// DUMP_IR
// FIR_DUMP

package io.github.expo.kolibri

import java.lang.reflect.Modifier

annotation class NativeMethod

// The pointer field is inherited from a base class (mirrors NativeObject); the plugin must find it by
// walking up the superclass chain and read it via the @JvmField from the subclass.
abstract class NativeBase(@JvmField protected val nativePointer: Long)

class Handle(pointer: Long) : NativeBase(pointer) {
    @NativeMethod external fun isNumber(): Boolean
    @NativeMethod external fun add(x: Int): Int
}

// Annotated overloads: each original must bind to its own generated stub — the 1-arg `foo`'s stub is
// `foo(J, I)`, which the still-external user-written 2-arg `foo` must never be mistaken for.
class Overloaded(pointer: Long) : NativeBase(pointer) {
    @NativeMethod external fun foo(a: Int): Int
    @NativeMethod external fun foo(a: Int, b: Int): Int
}

fun box(): String {
    val cls = Handle::class.java

    // The plugin generates a private native overload `isNumber(pointer: Long)` (same name, `(J)` sig).
    val stub = cls.getDeclaredMethod("isNumber", Long::class.javaPrimitiveType)
    if (!Modifier.isNative(stub.modifiers)) return "Fail: isNumber(Long) is not native"
    if (!Modifier.isPrivate(stub.modifiers)) return "Fail: isNumber(Long) is not private"

    // ...and rewrites the original into a plain (non-native) delegate.
    val original = cls.getDeclaredMethod("isNumber")
    if (Modifier.isNative(original.modifiers)) return "Fail: isNumber is still native"

    // Method arguments are carried after the leading pointer: add(x) -> add(pointer, x).
    val addStub = cls.getDeclaredMethod("add", Long::class.javaPrimitiveType, Int::class.javaPrimitiveType)
    if (addStub.returnType != Int::class.javaPrimitiveType) return "Fail: add(Long, Int) has wrong return type"

    val overloaded = Overloaded::class.java
    val long = Long::class.javaPrimitiveType
    val int = Int::class.javaPrimitiveType

    // Each overload keeps its own native stub and each original becomes a non-native delegate.
    if (!Modifier.isNative(overloaded.getDeclaredMethod("foo", long, int).modifiers)) {
        return "Fail: foo(Long, Int) is not native"
    }
    if (!Modifier.isNative(overloaded.getDeclaredMethod("foo", long, int, int).modifiers)) {
        return "Fail: foo(Long, Int, Int) is not native"
    }
    if (Modifier.isNative(overloaded.getDeclaredMethod("foo", int).modifiers)) {
        return "Fail: foo(Int) is still native"
    }
    if (Modifier.isNative(overloaded.getDeclaredMethod("foo", int, int).modifiers)) {
        return "Fail: foo(Int, Int) is still native"
    }

    return "OK"
}
