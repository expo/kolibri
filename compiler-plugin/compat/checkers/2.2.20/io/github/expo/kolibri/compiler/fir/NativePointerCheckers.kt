package io.github.expo.kolibri.compiler.fir

import org.jetbrains.kotlin.diagnostics.DiagnosticReporter
import org.jetbrains.kotlin.fir.FirSession
import org.jetbrains.kotlin.fir.analysis.checkers.MppCheckerKind
import org.jetbrains.kotlin.fir.analysis.checkers.context.CheckerContext
import org.jetbrains.kotlin.fir.analysis.checkers.declaration.DeclarationCheckers
import org.jetbrains.kotlin.fir.analysis.checkers.declaration.FirDeclarationChecker
import org.jetbrains.kotlin.fir.analysis.extensions.FirAdditionalCheckersExtension
import org.jetbrains.kotlin.fir.declarations.FirFunction
import org.jetbrains.kotlin.fir.symbols.impl.FirClassSymbol

/**
 * Plugs [NativeMethodCheck] into the FIR checker API of Kotlin 2.2.20+: `check` receives the
 * context and reporter as context parameters (hence `-Xcontext-parameters` in the build for these
 * releases), and the context tracks containing declarations as symbols.
 *
 * Registered as a function checker (not a simple-function checker) so the declaration type is
 * [FirFunction], which every supported release spells the same way — 2.3.20 renamed
 * `FirSimpleFunction` to `FirNamedFunction` and 2.4.20 renamed the checker set after it; the core
 * ignores the kinds of function `@NativeMethod` cannot annotate.
 */
class NativePointerCheckers(session: FirSession) : FirAdditionalCheckersExtension(session) {
  override val declarationCheckers: DeclarationCheckers = object : DeclarationCheckers() {
    override val functionCheckers: Set<FirDeclarationChecker<FirFunction>> = setOf(NativeMethodChecker)
  }

  private object NativeMethodChecker : FirDeclarationChecker<FirFunction>(MppCheckerKind.Common) {
    context(context: CheckerContext, reporter: DiagnosticReporter)
    override fun check(declaration: FirFunction) {
      val containingClass = context.containingDeclarations.lastOrNull() as? FirClassSymbol<*>
      NativeMethodCheck.check(declaration, containingClass, context, reporter)
    }
  }
}
