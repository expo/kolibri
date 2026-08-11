import org.gradle.internal.os.OperatingSystem

plugins {
  alias(libs.plugins.kotlin.jvm)
}

dependencies {
  testImplementation(project(":runtime"))
  testImplementation(libs.kotlin.test.junit5)
}

val cppDir = layout.projectDirectory.dir("src/test/cpp")
val kolibriCppDir = rootProject.layout.projectDirectory.dir("runtime/src/main/cpp")
val nativeBuildDir = layout.buildDirectory.dir("native")
val nativeLibsDir = layout.buildDirectory.dir("native-libs")

// CMake names the SHARED target's output per platform. System.loadLibrary() in Kolibri.load()
// resolves the same mapping at runtime, so only the Gradle-side input/output wiring needs it.
val nativeLibFileName =
  if (OperatingSystem.current().isMacOsX) "libexpo-kolibri.dylib" else "libexpo-kolibri.so"

val configureNative by tasks.registering(Exec::class) {
  inputs.dir(cppDir)
  inputs.dir(kolibriCppDir)
  outputs.file(nativeBuildDir.map { it.file("build.ninja") })

  commandLine(
    "cmake",
    "-S", cppDir.asFile.path,
    "-B", nativeBuildDir.get().asFile.path,
    "-G", "Ninja",
    "-DCMAKE_BUILD_TYPE=Debug",
    "-DKOLIBRI_DIR=${kolibriCppDir.asFile.path}",
    "-DJAVA_HOME=${System.getProperty("java.home")}",
  )
}

val buildNative by tasks.registering(Exec::class) {
  dependsOn(configureNative)
  inputs.dir(cppDir)
  inputs.dir(kolibriCppDir)
  outputs.file(nativeBuildDir.map { it.file(nativeLibFileName) })

  commandLine("cmake", "--build", nativeBuildDir.get().asFile.path, "--target", "expo-kolibri")
}

val copyNativeLibs by tasks.registering(Copy::class) {
  dependsOn(buildNative)
  from(nativeBuildDir.map { it.file(nativeLibFileName) })
  into(nativeLibsDir)
}

tasks.named<Test>("test") {
  useJUnitPlatform()
  dependsOn(copyNativeLibs)
  systemProperty("java.library.path", nativeLibsDir.get().asFile.path)
}
