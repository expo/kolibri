package io.github.expo.kolibri.compiler.runners

import io.github.expo.kolibri.compiler.services.configurePlugin
import org.jetbrains.kotlin.test.FirParser
import org.jetbrains.kotlin.test.builders.TestConfigurationBuilder
import org.jetbrains.kotlin.test.directives.CodegenTestDirectives
import org.jetbrains.kotlin.test.directives.FirDiagnosticsDirectives
import org.jetbrains.kotlin.test.directives.JvmEnvironmentConfigurationDirectives
import org.jetbrains.kotlin.test.runners.AbstractFirPhasedDiagnosticTest
import org.jetbrains.kotlin.test.services.EnvironmentBasedStandardLibrariesPathProvider
import org.jetbrains.kotlin.test.services.KotlinStandardLibrariesPathProvider

open class AbstractJvmDiagnosticTest : AbstractFirPhasedDiagnosticTest(FirParser.LightTree) {
  override fun createKotlinStandardLibrariesPathProvider(): KotlinStandardLibrariesPathProvider {
    return EnvironmentBasedStandardLibrariesPathProvider
  }

  override fun configure(builder: TestConfigurationBuilder) = with(builder) {
    super.configure(builder)
    defaultDirectives {
      // These tests stop after the frontend (RUN_PIPELINE_TILL: FRONTEND), so there is no IR to dump.
      // The FIR dump shows which stubs the generator declared, also for the invalid code.
      +FirDiagnosticsDirectives.FIR_DUMP
      +JvmEnvironmentConfigurationDirectives.FULL_JDK

      +CodegenTestDirectives.IGNORE_DEXING // Avoids loading R8 from the classpath.
    }

    configurePlugin()
  }
}
