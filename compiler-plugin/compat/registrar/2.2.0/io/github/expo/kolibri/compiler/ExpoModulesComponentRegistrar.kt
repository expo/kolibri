package io.github.expo.kolibri.compiler

import io.github.expo.kolibri.compiler.fir.ExpoModulesFirRegistrar
import io.github.expo.kolibri.compiler.ir.ExpoModulesIrGenerationExtension
import org.jetbrains.kotlin.backend.common.extensions.IrGenerationExtension
import org.jetbrains.kotlin.compiler.plugin.CompilerPluginRegistrar
import org.jetbrains.kotlin.config.CompilerConfiguration
import org.jetbrains.kotlin.fir.extensions.FirExtensionRegistrarAdapter

/**
 * Entry point of the Kolibri Kotlin compiler plugin (Kotlin 2.2.0 - 2.2.21: `pluginId` is not part
 * of [CompilerPluginRegistrar] yet).
 *
 * It wires the FIR frontend extensions (declaration generation, checkers) and the IR backend
 * extension (body generation) that implement the @NativeMethod / @AsNativePointer transform.
 */
class ExpoModulesComponentRegistrar : CompilerPluginRegistrar() {
  override val supportsK2: Boolean
    get() = true

  override fun ExtensionStorage.registerExtensions(configuration: CompilerConfiguration) {
    FirExtensionRegistrarAdapter.registerExtension(ExpoModulesFirRegistrar())
    IrGenerationExtension.registerExtension(ExpoModulesIrGenerationExtension())
  }
}
