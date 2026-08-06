package io.github.expo.kolibri.compiler.ir

import org.jetbrains.kotlin.backend.common.extensions.IrGenerationExtension
import org.jetbrains.kotlin.backend.common.extensions.IrPluginContext
import org.jetbrains.kotlin.ir.declarations.IrModuleFragment
import org.jetbrains.kotlin.ir.visitors.IrVisitorVoid
import org.jetbrains.kotlin.ir.visitors.acceptChildrenVoid

class ExpoModulesIrGenerationExtension : IrGenerationExtension {
  override fun generate(moduleFragment: IrModuleFragment, pluginContext: IrPluginContext) {
    val transformers = listOf<IrVisitorVoid>(
      NativePointerIrTransformer(pluginContext),
    )
    for (transformer in transformers) {
      moduleFragment.acceptChildrenVoid(transformer)
    }
  }
}
