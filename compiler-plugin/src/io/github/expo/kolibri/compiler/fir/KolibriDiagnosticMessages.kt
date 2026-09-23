package io.github.expo.kolibri.compiler.fir

import org.jetbrains.kotlin.diagnostics.KtDiagnosticFactoryToRendererMap

/**
 * The messages for [KolibriDiagnostics]. The factories and the renderer-map plumbing they hang off
 * live in `compat/diagnostics/<version>` (the diagnostics API changed in Kotlin 2.2.20); the texts
 * are shared here so both variants render identically.
 */
internal fun KtDiagnosticFactoryToRendererMap.registerKolibriMessages() {
  put(
    KolibriDiagnostics.NATIVE_METHOD_NOT_EXTERNAL,
    "@NativeMethod requires the function to be declared 'external' — the plugin replaces its " +
      "body with a delegate to the generated native overload",
  )
  put(
    KolibriDiagnostics.NATIVE_METHOD_NOT_A_MEMBER,
    "@NativeMethod only applies to instance methods of a class",
  )
  put(
    KolibriDiagnostics.NATIVE_METHOD_MISSING_POINTER_FIELD,
    "@NativeMethod requires a 'nativePointer' property on this class or a superclass " +
      "(declare one or extend io.github.expo.kolibri.NativeObject)",
  )
  put(
    KolibriDiagnostics.AS_NATIVE_POINTER_NOT_A_NATIVE_OBJECT,
    "@AsNativePointer only applies to parameters of an io.github.expo.kolibri.NativeObject " +
      "subtype — the argument crosses JNI as its raw pointer",
  )
}
