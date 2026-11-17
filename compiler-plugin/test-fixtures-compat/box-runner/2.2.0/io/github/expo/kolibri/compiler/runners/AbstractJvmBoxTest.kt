package io.github.expo.kolibri.compiler.runners

import io.github.expo.kolibri.compiler.services.configurePlugin
import org.jetbrains.kotlin.test.FirParser
import org.jetbrains.kotlin.test.builders.TestConfigurationBuilder
import org.jetbrains.kotlin.test.directives.CodegenTestDirectives
import org.jetbrains.kotlin.test.directives.JvmEnvironmentConfigurationDirectives
import org.jetbrains.kotlin.test.runners.codegen.AbstractFirBlackBoxCodegenTestBase
import org.jetbrains.kotlin.test.services.EnvironmentBasedStandardLibrariesPathProvider
import org.jetbrains.kotlin.test.services.KotlinStandardLibrariesPathProvider

open class AbstractJvmBoxTest : AbstractFirBlackBoxCodegenTestBase(FirParser.LightTree) {
  override fun createKotlinStandardLibrariesPathProvider(): KotlinStandardLibrariesPathProvider {
    return EnvironmentBasedStandardLibrariesPathProvider
  }

  override fun configure(builder: TestConfigurationBuilder) = with(builder) {
    super.configure(this)
    defaultDirectives {
      // IR dump only: it already shows the FIR-generated stubs (after fir2ir) and what the IR
      // transformer made of them. Frontend output is dumped by AbstractJvmDiagnosticTest.
      +CodegenTestDirectives.DUMP_IR
      +JvmEnvironmentConfigurationDirectives.FULL_JDK

      +CodegenTestDirectives.IGNORE_DEXING // Avoids loading R8 from the classpath.
    }

    configurePlugin()
  }
}
