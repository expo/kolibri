import com.vanniktech.maven.publish.JavadocJar
import com.vanniktech.maven.publish.KotlinJvm

plugins {
  alias(libs.plugins.kotlin.jvm)
  alias(libs.plugins.vanniktech.mavenPublish)
}

mavenPublishing {
  configure(KotlinJvm(JavadocJar.Empty(), sourcesJar = true))
}

// All tests for this module (Kotlin and native) live in the sibling `tests` subproject.
dependencies {
  api(libs.kotlin.stdlib)
}

// Consumed as an artifact-only dependency, resolved by classifier:
//   "io.github.expo.kolibri:runtime:<version>:cpp@zip"
val cppSourcesZip by tasks.registering(Zip::class) {
  archiveClassifier = "cpp"
  from(layout.projectDirectory.dir("src/main/cpp"))
}

publishing.publications.named<MavenPublication>("maven") {
  artifact(cppSourcesZip)
}

// --- Standalone native build (CMake + Ninja) -----------------------------------------------------
// Kolibri's C++ is a static library that consumers link into their own shared library, so nothing
// here ends up on any runtime path. Building it standalone as part of `check` proves the layer's
// core invariant: it compiles against ONLY <jni.h> — no Hermes, no JSI, no fbjni, no folly.

val cppDir = layout.projectDirectory.dir("src/main/cpp")
val nativeBuildDir = layout.buildDirectory.dir("native")

// The two tasks share nativeBuildDir on disk but must NOT both declare it as an output —
// overlapping outputs make Gradle's up-to-date checks unsound (each build dirties the other's
// snapshot). Each declares only the artifact it is responsible for; incremental compilation
// within the dir is Ninja's job.
val configureNative by tasks.registering(Exec::class) {
  inputs.dir(cppDir)
  outputs.file(nativeBuildDir.map { it.file("build.ninja") })

  commandLine(
    "cmake",
    "-S", cppDir.asFile.path,
    "-B", nativeBuildDir.get().asFile.path,
    "-G", "Ninja",
    "-DCMAKE_BUILD_TYPE=Release",
    "-DJAVA_HOME=${System.getProperty("java.home")}",
  )
}

val buildNative by tasks.registering(Exec::class) {
  dependsOn(configureNative)
  inputs.dir(cppDir)
  outputs.file(nativeBuildDir.map { it.file("libkolibri.a") })

  commandLine("cmake", "--build", nativeBuildDir.get().asFile.path, "--target", "kolibri")
}

tasks.named("check") { dependsOn(buildNative) }
