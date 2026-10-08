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
 * The rules in this library's `META-INF/proguard/kolibri.pro`, which AGP applies to an app, keep
 * everything this annotation marks from R8: a marked class keeps its name and every `external`
 * function it declares, since `registerNative` registers a class's whole table at once; a marked
 * method, field or constructor keeps its name and signature. Annotate the class whose natives you
 * register rather than the `external` functions themselves - those include the ones the kolibri
 * compiler plugin generates for `@NativeMethod`. On a `@JvmField` property, target the field:
 * `@field:CalledFromNative`.
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
