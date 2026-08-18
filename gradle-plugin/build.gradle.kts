import com.vanniktech.maven.publish.GradlePlugin
import com.vanniktech.maven.publish.JavadocJar

plugins {
  alias(libs.plugins.kotlin.jvm)
  alias(libs.plugins.buildconfig)
  alias(libs.plugins.gradle.plugin)
  alias(libs.plugins.vanniktech.mavenPublish)
}

mavenPublishing {
  configure(GradlePlugin(JavadocJar.None(), sourcesJar = true))
}

sourceSets {
  main {
    java.setSrcDirs(listOf("src"))
    resources.setSrcDirs(listOf("resources"))
  }
  test {
    java.setSrcDirs(listOf("test"))
    resources.setSrcDirs(listOf("testResources"))
  }
}

dependencies {
  implementation(libs.kotlin.gradle.plugin.api)
  // compileOnly: KotlinCompile (for the incremental-compilation workaround) lives in the full KGP,
  // which every consumer that triggers applyToCompilation already has on its build classpath.
  compileOnly(libs.kotlin.gradle.plugin)
  // compileOnly for the same reason: the AGP types are only touched inside
  // withPlugin("com.android.base"), so they're guaranteed on the classpath whenever that code runs.
  compileOnly(libs.android.gradle.plugin)
  testImplementation(libs.kotlin.test.junit5)
}

buildConfig {
  packageName("io.github.expo.kolibri.gradle")

  buildConfigField("String", "KOTLIN_PLUGIN_ID", "\"io.github.expo.kolibri.compiler\"")

  val pluginProject = project(":compiler-plugin")
  buildConfigField("String", "KOTLIN_PLUGIN_GROUP", "\"${pluginProject.group}\"")
  buildConfigField("String", "KOTLIN_PLUGIN_NAME", "\"compiler-plugin\"")
  buildConfigField("String", "KOTLIN_PLUGIN_VERSION", "\"${pluginProject.version}\"")

  val runtimeProject = project(":runtime")
  buildConfigField(
    type = "String",
    name = "RUNTIME_LIBRARY_COORDINATES",
    expression = "\"${runtimeProject.group}:runtime:${runtimeProject.version}\"",
  )

  val androidProject = project(":kolibri-android")
  buildConfigField(
    type = "String",
    name = "ANDROID_LIBRARY_COORDINATES",
    expression = "\"${androidProject.group}:kolibri-android:${androidProject.version}\"",
  )
}

gradlePlugin {
  plugins {
    create("kolibri") {
      id = "io.github.expo.kolibri"
      displayName = "Kolibri Gradle plugin"
      description = "Applies the Kolibri Kotlin compiler plugin and wires the runtime library."
      implementationClass = "io.github.expo.kolibri.gradle.KolibriGradlePlugin"
    }
  }
}
