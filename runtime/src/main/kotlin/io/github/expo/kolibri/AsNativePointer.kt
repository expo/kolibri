package io.github.expo.kolibri

/**
 * Marks a `@NativeMethod` parameter that is itself a [NativeObject]: the compiler plugin passes its
 * [NativeObject.nativePointer] (a `Long`) to the generated native stub instead of the whole handle,
 * so the native side receives the C++ peer's address directly - no JNI field read, mirroring how the
 * receiver is passed. A null argument is passed as `0`.
 */
@Target(AnnotationTarget.VALUE_PARAMETER)
@Retention(AnnotationRetention.BINARY)
annotation class AsNativePointer
