package io.github.expo.kolibri.compiler.fir

import org.jetbrains.kotlin.diagnostics.DiagnosticReporter
import org.jetbrains.kotlin.diagnostics.reportOn
import org.jetbrains.kotlin.fir.FirSession
import org.jetbrains.kotlin.fir.analysis.checkers.MppCheckerKind
import org.jetbrains.kotlin.fir.analysis.checkers.context.CheckerContext
import org.jetbrains.kotlin.fir.analysis.checkers.declaration.DeclarationCheckers
import org.jetbrains.kotlin.fir.analysis.checkers.declaration.FirSimpleFunctionChecker
import org.jetbrains.kotlin.fir.analysis.checkers.unsubstitutedScope
import org.jetbrains.kotlin.fir.analysis.extensions.FirAdditionalCheckersExtension
import org.jetbrains.kotlin.fir.declarations.FirClass
import org.jetbrains.kotlin.fir.declarations.FirSimpleFunction
import org.jetbrains.kotlin.fir.declarations.hasAnnotation
import org.jetbrains.kotlin.fir.declarations.utils.isExternal
import org.jetbrains.kotlin.fir.types.ConeKotlinType
import org.jetbrains.kotlin.fir.types.coneType
import org.jetbrains.kotlin.fir.types.constructClassLikeType
import org.jetbrains.kotlin.fir.types.isSubtypeOf
import org.jetbrains.kotlin.name.ClassId
import org.jetbrains.kotlin.name.FqName
import org.jetbrains.kotlin.name.Name

/**
 * Frontend validation for `@NativeMethod`/`@AsNativePointer` (see [KolibriDiagnostics]). Runs
 * before the stub generator's output matters, so misuse is reported where it is written instead
 * of surfacing as an IR-transformer crash or a runtime `UnsatisfiedLinkError`.
 */
class NativePointerCheckers(session: FirSession) : FirAdditionalCheckersExtension(session) {
  override val declarationCheckers: DeclarationCheckers = object : DeclarationCheckers() {
    override val simpleFunctionCheckers: Set<FirSimpleFunctionChecker> = setOf(NativeMethodChecker)
  }

  private object NativeMethodChecker : FirSimpleFunctionChecker(MppCheckerKind.Common) {
    private val NATIVE_OBJECT_CLASS_ID = ClassId.topLevel(FqName("io.github.expo.kolibri.NativeObject"))
    private val nativePointerName = Name.identifier("nativePointer")

    override fun check(declaration: FirSimpleFunction, context: CheckerContext, reporter: DiagnosticReporter) {
      val session = context.session
      checkAsNativePointerParameters(declaration, context, reporter)

      if (!declaration.hasAnnotation(NativePointerStubGenerator.NATIVE_METHOD_CLASS_ID, session)) {
        return
      }
      val containingClass = context.containingDeclarations.lastOrNull() as? FirClass
      if (containingClass == null) {
        reporter.reportOn(
          declaration.source,
          KolibriDiagnostics.NATIVE_METHOD_NOT_A_MEMBER,
          context,
        )
        return
      }
      if (!declaration.isExternal) {
        reporter.reportOn(
          declaration.source,
          KolibriDiagnostics.NATIVE_METHOD_NOT_EXTERNAL,
          context,
        )
      }
      if (!containingClass.hasNativePointerProperty(context)) {
        reporter.reportOn(
          declaration.source,
          KolibriDiagnostics.NATIVE_METHOD_MISSING_POINTER_FIELD,
          context,
        )
      }
    }

    private fun checkAsNativePointerParameters(
      declaration: FirSimpleFunction,
      context: CheckerContext,
      reporter: DiagnosticReporter,
    ) {
      val session = context.session
      // Nullable on purpose: a null handle crosses as a 0 pointer (see `nativePointerOf`).
      val nativeObjectType: ConeKotlinType =
        NATIVE_OBJECT_CLASS_ID.constructClassLikeType(emptyArray(), isMarkedNullable = true)
      for (parameter in declaration.valueParameters) {
        if (!parameter.hasAnnotation(NativePointerStubGenerator.AS_NATIVE_POINTER_CLASS_ID, session)) {
          continue
        }
        if (!parameter.returnTypeRef.coneType.isSubtypeOf(nativeObjectType, session)) {
          reporter.reportOn(
            parameter.source,
            KolibriDiagnostics.AS_NATIVE_POINTER_NOT_A_NATIVE_OBJECT,
            context,
          )
        }
      }
    }

    /** Whether the class declares or inherits a `nativePointer` property. */
    private fun FirClass.hasNativePointerProperty(context: CheckerContext): Boolean {
      var found = false
      symbol.unsubstitutedScope(context).processPropertiesByName(nativePointerName) { found = true }
      return found
    }
  }
}
