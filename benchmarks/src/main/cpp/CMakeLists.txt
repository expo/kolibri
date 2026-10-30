cmake_minimum_required(VERSION 3.18)
project(kolibri-benchmarks CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Numbers from an unoptimized build are meaningless; default to Release unless the caller asked
# for something else (e.g. RelWithDebInfo for profiling).
if (NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif ()

# Gradle injects -DKOLIBRI_DIR (kolibri's C++ source root from the runtime module); the fallback
# keeps direct CMake invocations and IDE syncs working.
if (NOT DEFINED KOLIBRI_DIR)
  set(KOLIBRI_DIR "${CMAKE_CURRENT_LIST_DIR}/../../../../runtime/src/main/cpp")
endif ()

# Gradle injects -DJAVA_HOME (kolibri uses it for the JNI include paths); fall back to the
# environment for direct CMake invocations. Without either, kolibri falls back to find_package(JNI).
if (NOT DEFINED JAVA_HOME AND DEFINED ENV{JAVA_HOME})
  set(JAVA_HOME "$ENV{JAVA_HOME}")
endif ()

add_subdirectory(${KOLIBRI_DIR} kolibri-build)

# --- Google Benchmark ----------------------------------------------------------------------------
# https://github.com/google/benchmark — pinned by archive hash, so a moved tag cannot change what
# gets built. Only the library itself: its own tests, install rules and docs are of no use here.
include(FetchContent)

set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
set(BENCHMARK_ENABLE_GTEST_TESTS OFF CACHE BOOL "" FORCE)
set(BENCHMARK_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
set(BENCHMARK_INSTALL_DOCS OFF CACHE BOOL "" FORCE)
# -Werror on a third-party library only turns a newer compiler's warnings into our build failures.
set(BENCHMARK_ENABLE_WERROR OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
  googlebenchmark
  URL https://github.com/google/benchmark/archive/refs/tags/v1.10.0.tar.gz
  URL_HASH SHA256=acc5c7c0fbed855c4aaa2d7c3c36fb82b0c15c28f4e151e3f679c2fc0aa404af
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(googlebenchmark)

# --- libjvm --------------------------------------------------------------------------------------
# The JNI benchmarks run against a JVM this executable creates itself (see main.cpp), so it links
# the JDK's libjvm directly. The JDK ships it outside any default search path, hence the rpath.
if (DEFINED JAVA_HOME)
  find_library(KOLIBRI_JVM_LIBRARY jvm
          PATHS ${JAVA_HOME}/lib/server ${JAVA_HOME}/jre/lib/server
          NO_DEFAULT_PATH
          REQUIRED
  )
else ()
  find_package(JNI REQUIRED)
  set(KOLIBRI_JVM_LIBRARY ${JAVA_JVM_LIBRARY})
endif ()
get_filename_component(KOLIBRI_JVM_LIBRARY_DIR ${KOLIBRI_JVM_LIBRARY} DIRECTORY)

# --- Benchmarks ----------------------------------------------------------------------------------
# One executable for every *.cpp here. main.cpp starts the JVM, then hands over to the library's
# runner, so all the --benchmark_* flags work as usual.
file(GLOB BENCHMARK_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")
add_executable(kolibri-benchmarks ${BENCHMARK_SOURCES})

target_compile_options(kolibri-benchmarks PRIVATE -Wall -Wextra -Wpedantic -Werror)
target_link_libraries(kolibri-benchmarks PRIVATE kolibri benchmark::benchmark ${KOLIBRI_JVM_LIBRARY})
set_target_properties(kolibri-benchmarks PROPERTIES BUILD_RPATH ${KOLIBRI_JVM_LIBRARY_DIR})
