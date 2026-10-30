#pragma once

#include <jni.h>

namespace expo::kolibri::benchmarks {
  /**
   * The JVM that main.cpp creates for this process before any benchmark runs. kolibri::initVM()
   * has already been called with it, and benchmarks run on the main thread, which creating the
   * VM attached — so kolibri::getEnv() is valid everywhere.
   */
  JavaVM* benchmarkVm();

  /**
   * Warm-up for benchmarks that call into Java: long enough for HotSpot to JIT the callee, so the
   * measured phase does not mix interpreted and compiled iterations.
   */
  inline constexpr double kJitWarmUpSeconds = 0.2;
} // namespace expo::kolibri::benchmarks
