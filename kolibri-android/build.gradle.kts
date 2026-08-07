import com.vanniktech.maven.publish.AndroidSingleVariantLibrary

plugins {
  alias(libs.plugins.android.library)
  alias(libs.plugins.vanniktech.mavenPublish)
}

mavenPublishing {
  configure(AndroidSingleVariantLibrary("release", sourcesJar = true, publishJavadocJar = true))
}

// Prefab exports a whole directory as an include root. Kolibri's include root is the cpp/ dir
// itself (#include <kolibri/...>), but exporting cpp/ verbatim would also ship the .cpp files — so
// stage only the public headers and point prefab at the staged root.
val prefabHeadersDir = layout.buildDirectory.dir("prefab-headers/kolibri")

val stageKolibriPrefabHeaders by tasks.registering(Sync::class) {
  from("../runtime/src/main/cpp/kolibri") {
    include("**/*.h")
    into("kolibri")
  }
  into(prefabHeadersDir)
}

android {
  namespace = "io.github.expo.kolibri.android"
  compileSdk = 36
  ndkVersion = "27.1.12297006"

  defaultConfig {
    minSdk = 24

    ndk {
      abiFilters += listOf("arm64-v8a", "armeabi-v7a", "x86", "x86_64")
    }

    externalNativeBuild {
      cmake {
        // c++_shared is stamped into the prefab abi.json and enforced against consumers; it's the
        // fbjni/React-Native ecosystem norm and the only STL that composes across multiple .so's.
        arguments += listOf("-DANDROID_STL=c++_shared")
        targets += "kolibri"
      }
    }
  }

  externalNativeBuild {
    cmake {
      // Reuses the desktop CMake project; on Android the NDK toolchain provides <jni.h>.
      path = file("../runtime/src/main/cpp/CMakeLists.txt")
      version = "3.22.1"
    }
  }

  buildFeatures {
    prefabPublishing = true
  }

  packaging {
    jniLibs {
      // ANDROID_STL=c++_shared makes AGP copy libc++_shared.so into the AAR's jni/ dir, but this
      // AAR ships only a static archive — consumers' own NDK builds provide the STL, and a bundled
      // copy would collide with theirs at merge time.
      excludes += "**/libc++_shared.so"
    }
  }

  prefab {
    create("kolibri") {
      headers = prefabHeadersDir.get().asFile.path
    }
  }
}

// AGP's prefab packaging tasks (prefab<Variant>ConfigurePackage / prefab<Variant>Package) copy the
// headers directory; make sure staging ran first.
tasks.matching { it.name.startsWith("prefab") }.configureEach {
  dependsOn(stageKolibriPrefabHeaders)
}
