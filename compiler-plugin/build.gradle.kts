import com.vanniktech.maven.publish.JavadocJar
import com.vanniktech.maven.publish.KotlinJvm

plugins {
  alias(libs.plugins.kotlin.jvm)
  alias(libs.plugins.buildconfig)
  alias(libs.plugins.gradle.java.test.fixtures)
  alias(libs.plugins.gradle.idea)
  alias(libs.plugins.vanniktech.mavenPublish)
}

mavenPublishing {
  configure(KotlinJvm(JavadocJar.Empty(), sourcesJar = true))
}

val testDataDir = layout.projectDirectory.dir("testData")
val testGenDirectory = layout.buildDirectory.dir("test-gen")

sourceSets {
  main {
    java.setSrcDirs(listOf("src"))
    resources.setSrcDirs(listOf("resources"))
  }
  testFixtures {
    java.setSrcDirs(listOf("test-fixtures"))
  }
  test {
    java.setSrcDirs(listOf("test", testGenDirectory))
    resources.setSrcDirs(listOf(testDataDir))
  }
}

idea {
  // This is needed until IDEA fixes IDEA-339729.
  module.generatedSourceDirs.add(testGenDirectory.get().asFile)
}

val testArtifacts: Configuration by configurations.creating

dependencies {
  compileOnly(libs.kotlin.compiler)

  testFixturesApi(libs.kotlin.test.junit5)
  testFixturesApi(libs.kotlin.test.framework)
  testFixturesApi(libs.kotlin.compiler)
  testFixturesRuntimeOnly(libs.junit)

  // Dependencies required to run the internal test framework.
  testArtifacts(libs.kotlin.stdlib)
  testArtifacts(libs.kotlin.stdlib.jdk8)
  testArtifacts(libs.kotlin.reflect)
  testArtifacts(libs.kotlin.test)
  testArtifacts(libs.kotlin.script.runtime)
  testArtifacts(libs.kotlin.annotations.jvm)
}

buildConfig {
  useKotlinOutput {
    internalVisibility = true
  }

  packageName("io.github.expo.kolibri.compiler")
  buildConfigField("String", "KOTLIN_PLUGIN_ID", "\"io.github.expo.kolibri.compiler\"")
}

tasks.test {
  dependsOn(testArtifacts)

  inputs.files(testArtifacts)
    .withPropertyName("testArtifacts")

  useJUnitPlatform()
  workingDir = rootDir

  // Properties required to run the internal test framework.
  val libraryProperties = mapOf(
    "org.jetbrains.kotlin.test.kotlin-stdlib" to "kotlin-stdlib",
    "org.jetbrains.kotlin.test.kotlin-stdlib-jdk8" to "kotlin-stdlib-jdk8",
    "org.jetbrains.kotlin.test.kotlin-reflect" to "kotlin-reflect",
    "org.jetbrains.kotlin.test.kotlin-test" to "kotlin-test",
    "org.jetbrains.kotlin.test.kotlin-script-runtime" to "kotlin-script-runtime",
    "org.jetbrains.kotlin.test.kotlin-annotations-jvm" to "kotlin-annotations-jvm",
  )
  doFirst {
    val artifacts = testArtifacts.files
    for ((propertyName, jarName) in libraryProperties) {
      artifacts
        .find { """$jarName-\d.*""".toRegex().matches(it.name) }
        ?.let { systemProperty(propertyName, it.absolutePath) }
    }
  }

  systemProperty("idea.ignore.disabled.plugins", "true")
  systemProperty("idea.home.path", rootDir)

  // Regenerate the golden .txt dumps in testData instead of asserting against them:
  //   ./gradlew :compiler-plugin:test -PupdateTestData
  if (providers.gradleProperty("updateTestData").isPresent) {
    systemProperty("kotlin.test.update.test.data", "true")
    outputs.upToDateWhen { false }
  }
}

kotlin {
  compilerOptions {
    optIn.add("org.jetbrains.kotlin.compiler.plugin.ExperimentalCompilerApi")
    optIn.add("org.jetbrains.kotlin.ir.symbols.UnsafeDuringIrConstructionAPI")
  }
}

val generateTests by tasks.registering(JavaExec::class) {
  inputs.dir(testDataDir)
    .withPropertyName("testData")
    .withPathSensitivity(PathSensitivity.RELATIVE)
  outputs.dir(testGenDirectory)
    .withPropertyName("generatedTests")

  classpath = sourceSets.testFixtures.get().runtimeClasspath
  mainClass.set("io.github.expo.kolibri.compiler.GenerateTestsKt")
  workingDir = rootDir
  args(
    listOf(
      testGenDirectory.get().asFile.absolutePath,
      testDataDir.asFile.absolutePath,
    )
  )
}

tasks.compileTestKotlin {
  dependsOn(generateTests)
}
