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
}

rootProject.name = "kolibri"

include("runtime")
include("kolibri-android")
include("compiler-plugin")
include("gradle-plugin")
include("tests")
