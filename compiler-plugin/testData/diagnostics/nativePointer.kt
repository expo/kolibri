// RUN_PIPELINE_TILL: FRONTEND

package io.github.expo.kolibri

annotation class NativeMethod

abstract class NativeBase(@JvmField protected val nativePointer: Long)

class Handle(pointer: Long) : NativeBase(pointer) {
    @NativeMethod external fun isNumber(): Boolean
}

fun test(handle: Handle) {
    val b = handle.isNumber()
    b.<!UNRESOLVED_REFERENCE!>inc<!>() // isNumber() returns a Boolean, so inc() must not resolve
}

/* GENERATED_FIR_TAGS: annotationDeclaration, classDeclaration, external, functionDeclaration, localProperty,
primaryConstructor, propertyDeclaration */
