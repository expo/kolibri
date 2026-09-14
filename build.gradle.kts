import com.vanniktech.maven.publish.MavenPublishBaseExtension
import org.jetbrains.kotlin.gradle.dsl.JvmTarget
import org.jetbrains.kotlin.gradle.dsl.KotlinJvmProjectExtension

plugins {
  alias(libs.plugins.kotlin.jvm) apply false
  alias(libs.plugins.buildconfig) apply false
  // AGP must sit on the same classpath as the publish plugin so it can detect the AGP version.
  alias(libs.plugins.android.library) apply false
  alias(libs.plugins.vanniktech.mavenPublish) apply false
}

val kolibriVersion: String = libs.versions.kolibri.get()

allprojects {
  group = "io.github.expo.kolibri"
  version = kolibriVersion
}

subprojects {
  // Compile with a JDK 17 toolchain, but emit Java 11 bytecode so consumers can run on JVM 11.
  // `release` (not `targetCompatibility`) so JDK-17-only APIs cannot leak into the bytecode.
  plugins.withId("org.jetbrains.kotlin.jvm") {
    extensions.configure<KotlinJvmProjectExtension> {
      jvmToolchain(17)
      compilerOptions {
        jvmTarget = JvmTarget.JVM_11
      }
    }
    tasks.withType<JavaCompile>().configureEach {
      options.release = 11
    }
  }

  plugins.withId("com.vanniktech.maven.publish") {
    extensions.configure<MavenPublishBaseExtension> {
      publishToMavenCentral(automaticRelease = true)

      // Only sign when signing credentials are available (CI environment)
      if (project.findProperty("signingInMemoryKey") != null) {
        signAllPublications()
      }

      pom {
        name = project.name
        description = "Kolibri - a small JNI layer for Kotlin and C++ with a compiler plugin that removes the JNI boilerplate at compile time"
        inceptionYear = "2026"
        url = "https://github.com/expo/kolibri"
        licenses {
          license {
            name = "The MIT License"
            url = "https://opensource.org/license/mit"
            distribution = "https://opensource.org/license/mit"
          }
        }
        developers {
          developer {
            id = "expo"
            name = "Expo"
            url = "https://github.com/expo"
          }
        }
        scm {
          url = "https://github.com/expo/kolibri"
          connection = "scm:git:git://github.com/expo/kolibri.git"
          developerConnection = "scm:git:ssh://github.com/expo/kolibri.git"
        }
      }
    }
  }
}

