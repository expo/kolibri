package io.github.expo.kolibri.compiler.fir

import org.jetbrains.kotlin.diagnostics.DiagnosticReporter
import org.jetbrains.kotlin.fir.FirSession
import org.jetbrains.kotlin.fir.analysis.checkers.MppCheckerKind
import org.jetbrains.kotlin.fir.analysis.checkers.context.CheckerContext
import org.jetbrains.kotlin.fir.analysis.checkers.declaration.DeclarationCheckers
import org.jetbrains.kotlin.fir.analysis.checkers.declaration.FirDeclarationChecker
import org.jetbrains.kotlin.fir.analysis.extensions.FirAdditionalCheckersExtension
import org.jetbrains.kotlin.fir.declarations.FirClass
import org.jetbrains.kotlin.fir.declarations.FirFunction

/**
 * Plugs [NativeMethodCheck] into the FIR checker API of Kotlin 2.2.0 - 2.2.10: `check` takes the
 * context and reporter as plain parameters, and the context tracks containing declarations as FIR
 * elements.
 *
 * Registered as a function checker (not a simple-function checker) so the declaration type is
 * [FirFunction], which every supported release spells the same way; the core ignores the kinds
 * of function `@NativeMethod` cannot annotate.
 */
class NativePointerCheckers(session: FirSession) : FirAdditionalCheckersExtension(session) {
  override val declarationCheckers: DeclarationCheckers = object : DeclarationCheckers() {
    override val functionCheckers: Set<FirDeclarationChecker<FirFunction>> = setOf(NativeMethodChecker)
  }

  private object NativeMethodChecker : FirDeclarationChecker<FirFunction>(MppCheckerKind.Common) {
    override fun check(declaration: FirFunction, context: CheckerContext, reporter: DiagnosticReporter) {
      val containingClass = (context.containingDeclarations.lastOrNull() as? FirClass)?.symbol
      NativeMethodCheck.check(declaration, containingClass, context, reporter)
    }
  }
}
