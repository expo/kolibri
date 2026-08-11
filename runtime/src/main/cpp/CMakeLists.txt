cmake_minimum_required(VERSION 3.18)
project(kolibri CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- expo::kolibri — the standalone JNI convenience layer ----------------------------------------
# Everything in the expo::kolibri namespace (refs, env, signatures, JavaClass tokens, native
# methods, the binary codec), plus the header-only expo::meta metaprogramming support in
# kolibri/meta/. Depends only on <jni.h> — no JSI, no Hermes — so it builds as its own static
# library and is reusable outside any JS bridge.
file(GLOB_RECURSE KOLIBRI_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/kolibri/*.cpp")
add_library(kolibri STATIC ${KOLIBRI_SOURCES})

# Linked into consumers' shared libraries, so it has to be relocatable.
set_target_properties(kolibri PROPERTIES POSITION_INDEPENDENT_CODE ON)

# JNI headers (jni.h + the platform jni_md.h). PUBLIC because our own headers include <jni.h>,
# so consumers need the same include paths. Gradle passes -DJAVA_HOME; fall back to find_package.
if (ANDROID)
  # The NDK toolchain puts <jni.h> on the default include path; nothing to add. (find_package(JNI)
  # only learned about Android in CMake 3.24, newer than AGP's default 3.22.)
elseif (DEFINED JAVA_HOME)
  # jni_md.h lives in a per-platform subdirectory of the JDK's include/ dir.
  if (APPLE)
    set(KOLIBRI_JNI_MD_SUBDIR darwin)
  elseif (WIN32)
    set(KOLIBRI_JNI_MD_SUBDIR win32)
  else ()
    set(KOLIBRI_JNI_MD_SUBDIR linux)
  endif ()

  target_include_directories(kolibri PUBLIC
          ${JAVA_HOME}/include
          ${JAVA_HOME}/include/${KOLIBRI_JNI_MD_SUBDIR}
  )
else ()
  find_package(JNI REQUIRED)
  target_include_directories(kolibri PUBLIC ${JNI_INCLUDE_DIRS})
endif ()

# Headers are included as #include <kolibri/...> (including <kolibri/meta/...>), so the include
# root is this directory — the same layout the vendored fbjni uses (cxx/ root, <fbjni/...>).
target_include_directories(kolibri PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# env.cpp uses pthread_key_t/pthread_once for the thread-detach hook. macOS and the NDK fold
# pthread into libc, but glibc did not before 2.34 — link it explicitly instead of relying on that.
# PUBLIC: this is a static library, so the consumer's link line is the one that has to carry it.
find_package(Threads REQUIRED)
target_link_libraries(kolibri PUBLIC Threads::Threads)

target_compile_options(kolibri PRIVATE -Wall -Wextra -Wpedantic -Werror)

# Buffer-format validation in the binary codec (see BinaryFormat.h::kCheckedCodec). OFF by default:
# both ends of the buffer are our own encoders, so the decoders trust the payload shape. PUBLIC so
# every target linking the codec compiles the same variant (the flag lives in inline templates).
option(EXPO_BINARY_CHECKED_CODEC "Enable binary codec payload validation" OFF)
if (EXPO_BINARY_CHECKED_CODEC)
  target_compile_definitions(kolibri PUBLIC EXPO_BINARY_CHECKED_CODEC)
endif ()
