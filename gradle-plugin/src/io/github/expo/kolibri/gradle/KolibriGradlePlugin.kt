package io.github.expo.kolibri.gradle

import com.android.build.api.dsl.CommonExtension
import io.github.expo.kolibri.gradle.BuildConfig.ANDROID_LIBRARY_COORDINATES
import io.github.expo.kolibri.gradle.BuildConfig.RUNTIME_LIBRARY_COORDINATES
import org.gradle.api.Project
import org.gradle.api.provider.Provider
import org.jetbrains.kotlin.gradle.plugin.KotlinCompilation
import org.jetbrains.kotlin.gradle.plugin.KotlinCompilerPluginSupportPlugin
import org.jetbrains.kotlin.gradle.plugin.SubpluginArtifact
import org.jetbrains.kotlin.gradle.plugin.SubpluginOption
import org.jetbrains.kotlin.gradle.plugin.getKotlinPluginVersion
import org.jetbrains.kotlin.gradle.tasks.KotlinCompile

/**
 * Gradle plugin that integrates Kolibri into a consumer project. Applying it:
 *
 *  - registers the Kolibri Kotlin compiler plugin (the @NativeMethod/@AsNativePointer transform)
 *    with every Kotlin compilation, picking the build of it that matches the project's Kotlin,
 *  - adds the Kolibri runtime ([RUNTIME_LIBRARY_COORDINATES]) to the implementation configuration
 *    so generated code has its supporting types available,
 *  - on Android projects, adds the prebuilt native AAR ([ANDROID_LIBRARY_COORDINATES]) and enables
 *    prefab consumption so the project's CMake can `find_package(kolibri-android)`, and
 *  - disables Kotlin incremental compilation for the affected tasks (IC's caches choke on the
 *    compiler plugin's source-less synthetic declarations).
 */
@Suppress("unused") // Used via reflection.
class KolibriGradlePlugin : KotlinCompilerPluginSupportPlugin {
  private lateinit var project: Project

  override fun apply(target: Project) {
    project = target
    target.extensions.create("kolibri", KolibriGradleExtension::class.java)

    // On the desktop JVM the C++ layer arrives statically linked inside the host's own shared
    // library; on Android it ships prebuilt as the kolibri-android Prefab AAR instead. The hook
    // reacts to the concrete AGP plugin ids, NOT to `com.android.base`: base is applied at the
    // start of every AGP plugin's own apply(), so its withPlugin callback fires before the
    // `android` extension exists and the lookup below would throw when kolibri is applied first.
    for (androidPluginId in listOf("com.android.application", "com.android.library")) {
      target.pluginManager.withPlugin(androidPluginId) {
        target.dependencies.add("implementation", ANDROID_LIBRARY_COORDINATES)
        val android = target.extensions.getByName("android") as CommonExtension
        android.buildFeatures.prefab = true
      }
    }
  }

  override fun isApplicable(kotlinCompilation: KotlinCompilation<*>): Boolean = true

  override fun getCompilerPluginId(): String = BuildConfig.KOTLIN_PLUGIN_ID

  // A compiler plugin is bound to the exact compiler it was built against, so Kolibri publishes one
  // compiler-plugin artifact per supported Kotlin release, versioned `<kolibri>-<kotlin>`. Resolving
  // by the project's own Kotlin version keeps the consumer's `plugins {}` block Kotlin-agnostic; a
  // Kotlin release Kolibri has no build for fails resolution with the missing coordinates.
  override fun getPluginArtifact(): SubpluginArtifact = SubpluginArtifact(
    groupId = BuildConfig.KOTLIN_PLUGIN_GROUP,
    artifactId = BuildConfig.KOTLIN_PLUGIN_NAME,
    version = "${BuildConfig.KOLIBRI_VERSION}-${project.getKotlinPluginVersion()}",
  )

  override fun applyToCompilation(
    kotlinCompilation: KotlinCompilation<*>,
  ): Provider<List<SubpluginOption>> {
    val project = kotlinCompilation.target.project

    kotlinCompilation.dependencies { implementation(RUNTIME_LIBRARY_COORDINATES) }

    // Kotlin incremental compilation's caches choke on the compiler plugin's source-less synthetic
    // declarations; disable IC for every compilation the plugin participates in.
    kotlinCompilation.compileTaskProvider.configure { task ->
      (task as? KotlinCompile)?.incremental = false
    }

    // No options yet. The lambda must not capture the Project (KGP serializes this provider into
    // compile-task inputs, and a Project reference breaks the configuration cache).
    return project.provider { emptyList() }
  }
}
