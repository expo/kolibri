cmake_minimum_required(VERSION 3.18)
project(kolibri-tests CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Gradle injects -DKOLIBRI_DIR (kolibri's C++ source root from the runtime module); the fallback
# keeps direct CMake invocations and IDE syncs working.
if (NOT DEFINED KOLIBRI_DIR)
  set(KOLIBRI_DIR "${CMAKE_CURRENT_LIST_DIR}/../../../../runtime/src/main/cpp")
endif ()

# Gradle injects -DJAVA_HOME (kolibri uses it for the JNI include paths). IDE CMake syncs
# configure this file directly without any -D vars, so derive it from the environment or the
# macOS java_home helper.
if (NOT DEFINED JAVA_HOME)
  if (DEFINED ENV{JAVA_HOME})
    set(JAVA_HOME "$ENV{JAVA_HOME}")
  elseif (APPLE)
    execute_process(
      COMMAND /usr/libexec/java_home
      OUTPUT_VARIABLE JAVA_HOME
      OUTPUT_STRIP_TRAILING_WHITESPACE
      RESULT_VARIABLE JAVA_HOME_RESULT
    )
    if (NOT JAVA_HOME_RESULT EQUAL 0)
      message(FATAL_ERROR "JAVA_HOME is not set and /usr/libexec/java_home failed; pass -DJAVA_HOME=<jdk>.")
    endif ()
  endif ()
endif ()

# Build the validating codec variant: the checked decoder has no other coverage, and tests are
# exactly the place to pay for per-byte validation (bounds, tags, trailing bytes).
set(EXPO_BINARY_CHECKED_CODEC ON CACHE BOOL "Enable binary codec payload validation" FORCE)

add_subdirectory(${KOLIBRI_DIR} kolibri-build)

# The test natives + kolibri's own sources in one dylib. The target is named expo-kolibri so the
# output is libexpo-kolibri.dylib — the name Kolibri.load() loads (see build.gradle.kts).
file(GLOB TEST_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")
add_library(expo-kolibri SHARED ${TEST_SOURCES})

target_compile_options(expo-kolibri PRIVATE -Wall -Wextra -Wpedantic -Werror)
target_link_libraries(expo-kolibri PRIVATE kolibri)
