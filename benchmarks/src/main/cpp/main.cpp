#include <cstdio>

#include <benchmark/benchmark.h>

#include <kolibri/env.h>

#include "jvm.h"

// Replaces benchmark_main: the JNI benchmarks need a live JVM, so this process embeds one (linked
// against the JDK's libjvm) instead of being loaded into a Java process like the tests are.
namespace expo::kolibri::benchmarks {
  namespace {
    JavaVM* gVm = nullptr;
  }

  JavaVM* benchmarkVm() { return gVm; }
} // namespace expo::kolibri::benchmarks

int main(int argc, char** argv) {
  // Parse the --benchmark_* flags first, so --help and bad flags never pay for a JVM start.
  benchmark::Initialize(&argc, argv);
  if (benchmark::ReportUnrecognizedArguments(argc, argv)) {
    return 1;
  }

  // The benchmarks' own Java classes (see CMakeLists.txt), next to the JDK's.
  JavaVMOption options[] = {
    {const_cast<char*>("-Djava.class.path=" KOLIBRI_BENCHMARK_CLASS_PATH), nullptr},
  };
  JavaVMInitArgs vmArgs{};
  vmArgs.version = JNI_VERSION_1_6;
  vmArgs.nOptions = 1;
  vmArgs.options = options;
  vmArgs.ignoreUnrecognized = JNI_FALSE;

  JavaVM* vm = nullptr;
  JNIEnv* env = nullptr;
  if (JNI_CreateJavaVM(&vm, reinterpret_cast<void**>(&env), &vmArgs) != JNI_OK) {
    std::fprintf(stderr, "kolibri-benchmarks: JNI_CreateJavaVM failed\n");
    return 1;
  }
  expo::kolibri::benchmarks::gVm = vm;
  expo::kolibri::initVM(vm);

  benchmark::RunSpecifiedBenchmarks();
  benchmark::Shutdown();

  vm->DestroyJavaVM();
  return 0;
}
