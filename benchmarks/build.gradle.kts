// --- Native benchmarks (Google Benchmark, CMake + Ninja) -----------------------------------------
// A standalone executable over kolibri's C++ sources; CMake fetches Google Benchmark itself (see
// src/main/cpp/CMakeLists.txt). The JNI benchmarks run against a JVM the executable embeds via the
// JDK's libjvm, using only JDK classes and a few plain Java ones CMake compiles itself (see
// src/main/java), so nothing from the Kotlin build has to be on a classpath.
// Deliberately NOT wired into `check`: the first configure needs the
// network, and timing runs on shared CI runners are noise. Run them explicitly:
//   ./gradlew :benchmarks:runBenchmarks
//   ./gradlew :benchmarks:runBenchmarks -PbenchmarkArgs="--benchmark_filter=Doubles"

val cppDir = layout.projectDirectory.dir("src/main/cpp")
val javaDir = layout.projectDirectory.dir("src/main/java")
val kolibriCppDir = rootProject.layout.projectDirectory.dir("runtime/src/main/cpp")
val nativeBuildDir = layout.buildDirectory.dir("native")
val benchmarkExecutable = nativeBuildDir.map { it.file("kolibri-benchmarks") }

val configureNative by tasks.registering(Exec::class) {
  inputs.dir(cppDir)
  inputs.dir(javaDir)
  inputs.dir(kolibriCppDir)
  outputs.file(nativeBuildDir.map { it.file("build.ninja") })

  commandLine(
    "cmake",
    "-S", cppDir.asFile.path,
    "-B", nativeBuildDir.get().asFile.path,
    "-G", "Ninja",
    "-DCMAKE_BUILD_TYPE=Release",
    "-DKOLIBRI_DIR=${kolibriCppDir.asFile.path}",
    "-DJAVA_HOME=${System.getProperty("java.home")}",
  )
}

val buildNative by tasks.registering(Exec::class) {
  dependsOn(configureNative)
  inputs.dir(cppDir)
  inputs.dir(javaDir)
  inputs.dir(kolibriCppDir)
  outputs.file(benchmarkExecutable)

  commandLine("cmake", "--build", nativeBuildDir.get().asFile.path, "--target", "kolibri-benchmarks")
}

val runBenchmarks by tasks.registering(Exec::class) {
  dependsOn(buildNative)
  // A measurement, not a build step: never up to date.
  outputs.upToDateWhen { false }

  val extraArgs = providers.gradleProperty("benchmarkArgs").map { it.split(" ").filter(String::isNotBlank) }
  executable(benchmarkExecutable.get().asFile.path)
  args(extraArgs.getOrElse(emptyList()))
}
