plugins {
  alias(libs.plugins.kotlin.jvm) apply false
  alias(libs.plugins.buildconfig) apply false
}

allprojects {
  group = "io.github.expo.kolibri"
  version = "0.1.0-SNAPSHOT"
}
