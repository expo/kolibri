![Kolibri](assets/banner.png)

A small JNI layer for Kotlin ⇄ C++: a C++ static library that includes nothing but `<jni.h>`, a
Kotlin runtime of native-handle types, and a Kotlin compiler plugin that removes the JNI boilerplate
at compile time — **no fbjni, no folly, no JSI**.

`external` methods annotated with `@NativeMethod` are rewritten to take their object's C++ address
as the first argument, so the native side reads the peer straight off the call instead of doing an
`env->GetLongField(thiz, ...)`; `@AsNativePointer` parameters cross the same way, each fenced so the
GC cannot free a peer the native code is still using. `NativeObject` and `NativeCleaner` own that
peer's lifetime. On the C++ side, `expo::kolibri` wraps the raw `JNIEnv` in typed classes, members
and refs, with a header-only `expo::meta` layer doing the compile-time work.

## Installation

Kolibri is not published to a remote repository yet, so build it into your local Maven repository
first:

```bash
./gradlew publishToMavenLocal
```

```kotlin
// settings.gradle.kts
pluginManagement {
  repositories {
    mavenLocal()
    mavenCentral()
    gradlePluginPortal()
  }
}
```

```kotlin
// build.gradle.kts
plugins {
  id("io.github.expo.kolibri") version "0.1.0-SNAPSHOT"
}

repositories {
  mavenLocal() // the plugin adds the runtime jar, so this side needs the snapshot too
  mavenCentral()
}
```

The plugin registers the compiler plugin and adds the `runtime` jar. On an Android project it also
adds the `kolibri-android` AAR — the same C++ tree prebuilt for all four ABIs — and turns on Prefab,
so CMake can link the static library into your own shared library:

```cmake
find_package(kolibri-android REQUIRED CONFIG)
target_link_libraries(mylib PRIVATE kolibri-android::kolibri)
```

Build with `-DANDROID_STL=c++_shared`; Prefab fails the configure on an STL mismatch.
