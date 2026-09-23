package io.github.expo.kolibri.compiler.fir

import org.jetbrains.kotlin.diagnostics.KtDiagnosticFactoryToRendererMap
import org.jetbrains.kotlin.diagnostics.SourceElementPositioningStrategies
import org.jetbrains.kotlin.diagnostics.error0
import org.jetbrains.kotlin.diagnostics.rendering.BaseDiagnosticRendererFactory
import org.jetbrains.kotlin.diagnostics.rendering.RootDiagnosticRendererFactory
import org.jetbrains.kotlin.psi.KtNamedFunction
import org.jetbrains.kotlin.psi.KtParameter

/**
 * The frontend diagnostics [NativeMethodCheck] reports, so every misuse of the annotations is a
 * readable compile error at its source location — never an internal-compiler-error from the IR
 * transformer (whose `require` guards remain only as a backstop).
 *
 * Kotlin 2.2.0 - 2.2.10 variant: factories are free-standing and their renderer is registered with
 * the global [RootDiagnosticRendererFactory].
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
  override val MAP = KtDiagnosticFactoryToRendererMap("Kolibri").apply { registerKolibriMessages() }
}
