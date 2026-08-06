package io.github.expo.kolibri.compiler.fir

import org.jetbrains.kotlin.GeneratedDeclarationKey
import org.jetbrains.kotlin.descriptors.Visibilities
import org.jetbrains.kotlin.fir.FirSession
import org.jetbrains.kotlin.fir.declarations.hasAnnotation
import org.jetbrains.kotlin.fir.extensions.FirDeclarationGenerationExtension
import org.jetbrains.kotlin.fir.extensions.FirDeclarationPredicateRegistrar
import org.jetbrains.kotlin.fir.extensions.MemberGenerationContext
import org.jetbrains.kotlin.fir.extensions.predicate.LookupPredicate
import org.jetbrains.kotlin.fir.plugin.createMemberFunction
import org.jetbrains.kotlin.fir.symbols.impl.FirClassSymbol
import org.jetbrains.kotlin.fir.symbols.impl.FirNamedFunctionSymbol
import org.jetbrains.kotlin.name.CallableId
import org.jetbrains.kotlin.name.ClassId
import org.jetbrains.kotlin.name.FqName
import org.jetbrains.kotlin.name.Name

/**
 * For each `@NativePointer`-annotated `external` method `foo(args): R` in a class, generates a
 * `private external fun foo(pointer: Long, args): R` — a native *overload* of the original, not a
 * renamed sibling. The two share a name but differ in descriptor, so JNI's name+signature-based
 * `RegisterNatives` binds the `(J…)` overload unambiguously and the C++ side needs no name mangling.
 * The [NativePointerIrTransformer] then rewrites the original `foo` to delegate to this overload,
 * passing `nativePointer`.
 *
 * Only the overload is declared here (as a native method); its body stays empty (native). The public
 * method's signature is untouched, so consumers in other modules link unchanged.
 */
class NativePointerStubGenerator(session: FirSession) : FirDeclarationGenerationExtension(session) {
  companion object {
    val NATIVE_METHOD_FQN = FqName("io.github.expo.kolibri.NativeMethod")
    val NATIVE_METHOD_CLASS_ID = ClassId.topLevel(NATIVE_METHOD_FQN)
    val AS_NATIVE_POINTER_CLASS_ID = ClassId.topLevel(FqName("io.github.expo.kolibri.AsNativePointer"))

    // `annotated` supports the global-lookup path (unlike `hasAnnotated`); registering it makes the
    // frontend resolve @NativeMethod early so we can read it off member symbols below.
    private val PREDICATE = LookupPredicate.create { annotated(NATIVE_METHOD_FQN) }
  }

  object Key : GeneratedDeclarationKey()

  private fun annotatedFunctions(classSymbol: FirClassSymbol<*>): List<FirNamedFunctionSymbol> =
    classSymbol.declarationSymbols
      .filterIsInstance<FirNamedFunctionSymbol>()
      .filter { it.hasAnnotation(NATIVE_METHOD_CLASS_ID, session) }

  override fun getCallableNamesForClass(classSymbol: FirClassSymbol<*>, context: MemberGenerationContext): Set<Name> =
    annotatedFunctions(classSymbol).mapTo(mutableSetOf()) { it.name }

  override fun generateFunctions(callableId: CallableId, context: MemberGenerationContext?): List<FirNamedFunctionSymbol> {
    val owner = context?.owner ?: return emptyList()
    // Generate a native overload for every annotated function of this name (same name, `(J…)` signature).
    return annotatedFunctions(owner)
      .filter { it.name == callableId.callableName }
      .map { original ->
        createMemberFunction(owner, Key, callableId.callableName, original.resolvedReturnType) {
          visibility = Visibilities.Private
          status { isExternal = true }
          valueParameter(Name.identifier("pointer"), session.builtinTypes.longType.coneType)
          for (param in original.valueParameterSymbols) {
            // An @AsNativePointer handle argument is passed as its Long pointer (see the IR transformer).
            val type = if (param.hasAnnotation(AS_NATIVE_POINTER_CLASS_ID, session)) {
              session.builtinTypes.longType.coneType
            } else {
              param.resolvedReturnType
            }
            valueParameter(param.name, type)
          }
        }.symbol
      }
  }

  override fun FirDeclarationPredicateRegistrar.registerPredicates() {
    register(PREDICATE)
  }
}
