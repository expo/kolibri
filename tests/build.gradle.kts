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

val configureNative by tasks.registering(Exec::class) {
  onlyIf { OperatingSystem.current().isMacOsX }
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
  onlyIf { OperatingSystem.current().isMacOsX }
  dependsOn(configureNative)
  inputs.dir(cppDir)
  inputs.dir(kolibriCppDir)
  outputs.file(nativeBuildDir.map { it.file("libexpo-kolibri.dylib") })

  commandLine("cmake", "--build", nativeBuildDir.get().asFile.path, "--target", "expo-kolibri")
}

val copyNativeLibs by tasks.registering(Copy::class) {
  onlyIf { OperatingSystem.current().isMacOsX }
  dependsOn(buildNative)
  from(nativeBuildDir.map { it.file("libexpo-kolibri.dylib") })
  into(nativeLibsDir)
}

tasks.named<Test>("test") {
  useJUnitPlatform()
  onlyIf { OperatingSystem.current().isMacOsX }
  dependsOn(copyNativeLibs)
  systemProperty("java.library.path", nativeLibsDir.get().asFile.path)
}
