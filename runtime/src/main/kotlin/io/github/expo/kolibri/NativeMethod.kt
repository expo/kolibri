package io.github.expo.kolibri

/**
 * Marks an `external` instance method whose native counterpart should receive its object's
 * [NativeObject.nativePointer] as the first argument, instead of recovering it on the native side
 * with a JNI `env->GetLongField(thiz, ...)` call.
 *
 * The kolibri compiler plugin rewrites each annotated method `foo(args): R` into a thin
 * delegate to a generated `private external fun fooNative(pointer: Long, args): R`, passing the raw
 * pointer address. The public method keeps its original signature, so callers (and other modules)
 * are unaffected; only the native binding target changes - the JNI method registered on the native
 * side is `fooNative`, which reads the pointer straight from its first argument.
 *
 * The enclosing class must expose the pointer via a `nativePointer` field - normally by extending
 * [NativeObject].
 */
@Target(AnnotationTarget.FUNCTION)
@Retention(AnnotationRetention.BINARY)
annotation class NativeMethod
