pluginManagement {
  repositories {
    google()
    mavenCentral()
    gradlePluginPortal()
  }
}

dependencyResolutionManagement {
  repositories {
    google()
    mavenCentral()
  }
  // `libs` comes from gradle/libs.versions.toml by convention. Kolibri owns that catalog so it
  // builds standalone; consumers that include this build (e.g. expo-modules-android-v2) keep their
  // own catalog, so the shared versions — kotlin, agp — must be bumped on both sides together.
  versionCatalogs {
    create("libs") {
      // The compiler plugin is compiled once per supported Kotlin release (see
      // gradle/kotlin-versions.txt): `-PkotlinVersion=<version>` swaps the Kotlin the whole build
      // uses. Without it the catalog's own `kotlin` — the oldest supported release — applies.
      providers.gradleProperty("kotlinVersion").orNull?.let { version("kotlin", it) }
    }
  }
}

rootProject.name = "kolibri"

include("runtime")
include("kolibri-android")
include("compiler-plugin")
include("gradle-plugin")
include("tests")
