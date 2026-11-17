package io.github.expo.kolibri.compiler

import io.github.expo.kolibri.compiler.runners.AbstractJvmBoxTest
import io.github.expo.kolibri.compiler.runners.AbstractJvmDiagnosticTest
import org.jetbrains.kotlin.generators.generateTestGroupSuiteWithJUnit5

fun main(args: Array<String>) {
  generateTestGroupSuiteWithJUnit5 {
    testGroup(testsRoot = args[0], testDataRoot = args[1]) {
      testClass<AbstractJvmDiagnosticTest> {
        model("diagnostics")
      }

      testClass<AbstractJvmBoxTest> {
        model("box")
      }
    }
  }
}
