// RUN_PIPELINE_TILL: FRONTEND

package io.github.expo.kolibri

annotation class NativeMethod
annotation class AsNativePointer

abstract class NativeObject(@JvmField val nativePointer: Long)

class OtherHandle(pointer: Long) : NativeObject(pointer)

// Correct usage — no diagnostics: external instance method, pointer field inherited, and an
// @AsNativePointer parameter of a (nullable) NativeObject subtype.
class GoodHandle(pointer: Long) : NativeObject(pointer) {
    @NativeMethod external fun consume(@AsNativePointer other: OtherHandle?): Boolean
}

// The annotated function must be external — the plugin would replace its body.
class BodyHandle(pointer: Long) : NativeObject(pointer) {
    @NativeMethod fun <!NATIVE_METHOD_NOT_EXTERNAL!>hasBody<!>(): Boolean {
        return true
    }
}

// The class (or a superclass) must carry the nativePointer property the delegate reads.
class NoField {
    @NativeMethod external fun <!NATIVE_METHOD_MISSING_POINTER_FIELD!>broken<!>(): Boolean
}

// Top-level functions are silently ignored by the stub generator, so they must be rejected.
@NativeMethod external fun <!NATIVE_METHOD_NOT_A_MEMBER!>topLevel<!>(): Boolean

// An @AsNativePointer argument crosses as its raw pointer, so it must be a NativeObject.
class WrongParam(pointer: Long) : NativeObject(pointer) {
    @NativeMethod external fun wrong(<!AS_NATIVE_POINTER_NOT_A_NATIVE_OBJECT!>@AsNativePointer value: String<!>): Boolean
}
