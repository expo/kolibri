package io.github.expo.kolibri

/**
 * Marks a declaration that native (C++) code looks up by name through JNI, so its signature is part
 * of a contract the Kotlin compiler cannot check.
 *
 * Renaming an annotated member, changing its parameter or return types, changing its visibility or
 * `static`-ness, or moving/renaming an annotated class breaks the native lookup. The failure is not
 * a compile error - it surfaces at runtime as a `NoSuchMethodError`, a `NoSuchFieldError` or a
 * `ClassNotFoundException` when the native side resolves the member. Every such change must be made
 * together with the matching C++ change.
 *
 * Use [by] to point at the native counterpart, normally the header that declares the descriptor or
 * the member handle:
 *
 * ```kotlin
 * @CalledFromNative(by = "expo-modules-v2/jni/JRecordRegistry.h")
 * internal object RecordRegistry {
 *   @JvmStatic
 *   @CalledFromNative(by = "expo-modules-v2/jni/JRecordRegistry.h")
 *   fun fetchSchema(schemaId: Int): RecordSchemaData = ...
 * }
 * ```
 *
 * This annotation is documentation only - it changes no behaviour and is not read by the kolibri
 * compiler plugin. It does not apply to `external` functions: those are Kotlin declarations that
 * *call into* native code, and the compiler already ties them to a registered native binding.
 */
@Target(
  AnnotationTarget.CLASS,
  AnnotationTarget.CONSTRUCTOR,
  AnnotationTarget.FUNCTION,
  AnnotationTarget.PROPERTY,
  AnnotationTarget.PROPERTY_GETTER,
  AnnotationTarget.PROPERTY_SETTER,
  AnnotationTarget.FIELD,
)
@Retention(AnnotationRetention.BINARY)
annotation class CalledFromNative(
  /** Where the native side performs the lookup, e.g. a header or source path under `src/main/cpp`. */
  val by: String = "",
)
