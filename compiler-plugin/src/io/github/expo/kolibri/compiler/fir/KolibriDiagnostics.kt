package io.github.expo.kolibri.compiler.fir

import org.jetbrains.kotlin.diagnostics.KtDiagnosticFactoryToRendererMap
import org.jetbrains.kotlin.diagnostics.SourceElementPositioningStrategies
import org.jetbrains.kotlin.diagnostics.error0
import org.jetbrains.kotlin.diagnostics.rendering.BaseDiagnosticRendererFactory
import org.jetbrains.kotlin.diagnostics.rendering.RootDiagnosticRendererFactory
import org.jetbrains.kotlin.psi.KtNamedFunction
import org.jetbrains.kotlin.psi.KtParameter

/**
 * The frontend diagnostics [NativePointerCheckers] reports, so every misuse of the annotations is
 * a readable compile error at its source location — never an internal-compiler-error from the IR
 * transformer (whose `require` guards remain only as a backstop).
 */
object KolibriDiagnostics {
  val NATIVE_METHOD_NOT_EXTERNAL by error0<KtNamedFunction>(SourceElementPositioningStrategies.DECLARATION_NAME)
  val NATIVE_METHOD_NOT_A_MEMBER by error0<KtNamedFunction>(SourceElementPositioningStrategies.DECLARATION_NAME)
  val NATIVE_METHOD_MISSING_POINTER_FIELD by error0<KtNamedFunction>(SourceElementPositioningStrategies.DECLARATION_NAME)
  val AS_NATIVE_POINTER_NOT_A_NATIVE_OBJECT by error0<KtParameter>()

  init {
    RootDiagnosticRendererFactory.registerFactory(KolibriDiagnosticMessages)
  }
}

object KolibriDiagnosticMessages : BaseDiagnosticRendererFactory() {
  override val MAP = KtDiagnosticFactoryToRendererMap("Kolibri").apply {
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
}
