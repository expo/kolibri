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

Kolibri is published to Maven Central. Apply the Gradle plugin next to the Kotlin plugin:

```kotlin
// build.gradle.kts
plugins {
  kotlin("jvm") version "2.3.20" // or kotlin("android") — any release listed below
  id("io.github.expo.kolibri") version "0.1.9"
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

## Supported Kotlin versions

A Kotlin compiler plugin is bound to the exact compiler it was built against, so Kolibri builds and
publishes its compiler plugin once per Kotlin release, as
`io.github.expo.kolibri:compiler-plugin:<kolibri>-<kotlin>` (for example `0.1.9-2.3.20`). The Gradle
plugin picks the build that matches your project's Kotlin, so nothing in your build refers to it
directly. The `runtime` jar, the `kolibri-android` AAR and the Gradle plugin itself are the same for
every release.

Every release in [`gradle/kotlin-versions.txt`](gradle/kotlin-versions.txt) is built and tested on
CI: 2.2.0, 2.2.10, 2.2.20, 2.2.21, 2.3.0, 2.3.10, 2.3.20, 2.3.21, 2.4.0, 2.4.10 and 2.4.20. A Kotlin
release outside that list has no compiler-plugin artifact, and the build fails resolving it.

### Building locally

`-PkotlinVersion=<version>` makes the whole build use that Kotlin; without it the catalog's default,
the oldest supported release, applies. To try an unreleased Kolibri in a project, publish it into
your local Maven repository with the project's Kotlin:

```bash
./gradlew publishToMavenLocal -PkotlinVersion=2.3.20
```

and add `mavenLocal()` to both `pluginManagement.repositories` in `settings.gradle.kts` and the
project's `repositories`.

`scripts/test-all-versions.sh` runs the Kotlin-dependent checks against every listed release, and
`./gradlew :compiler-plugin:test -PkotlinVersion=<version>` against one. To add a release, append it
to `gradle/kotlin-versions.txt` and run the script; if the release changed a compiler API the plugin
uses, or the golden dumps, see [`compiler-plugin/compat/README.md`](compiler-plugin/compat/README.md).
