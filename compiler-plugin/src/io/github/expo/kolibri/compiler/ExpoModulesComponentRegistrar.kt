package io.github.expo.kolibri.compiler

import io.github.expo.kolibri.compiler.fir.ExpoModulesFirRegistrar
import io.github.expo.kolibri.compiler.ir.ExpoModulesIrGenerationExtension
import org.jetbrains.kotlin.backend.common.extensions.IrGenerationExtension
import org.jetbrains.kotlin.compiler.plugin.CompilerPluginRegistrar
import org.jetbrains.kotlin.config.CompilerConfiguration
import org.jetbrains.kotlin.fir.extensions.FirExtensionRegistrarAdapter

/**
 * Entry point of the Expo Modules API v2 Kotlin compiler plugin.
 *
 * It wires the FIR frontend extensions (declaration generation, checkers) and the IR backend
 * extensions (body generation) that implement the Expo Modules API v2 code generation.
 */
class ExpoModulesComponentRegistrar : CompilerPluginRegistrar() {
  override val supportsK2: Boolean
    get() = true

  override fun ExtensionStorage.registerExtensions(configuration: CompilerConfiguration) {
    FirExtensionRegistrarAdapter.registerExtension(ExpoModulesFirRegistrar())
    IrGenerationExtension.registerExtension(ExpoModulesIrGenerationExtension())
  }
}
