package io.github.expo.kolibri.compiler.fir

import org.jetbrains.kotlin.fir.extensions.FirExtensionRegistrar

class ExpoModulesFirRegistrar : FirExtensionRegistrar() {
  override fun ExtensionRegistrarContext.configurePlugin() {
    +::NativePointerStubGenerator
    +::NativePointerCheckers
  }
}
