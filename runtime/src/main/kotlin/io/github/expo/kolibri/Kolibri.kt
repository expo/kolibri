package io.github.expo.kolibri

private const val DEFAULT_LIBRARY_NAME = "expo-kolibri"

object Kolibri {
  @Volatile
  private var loaded = false

  // Guarded by `this`. JNI_OnLoad runs inside System.loadLibrary, and its FindClass initializes
  // Kolibri classes, which call [load] again on this thread; that call must not load anything.
  private var loading = false

  /**
   * Whether [load] has run.
   */
  val isLoaded: Boolean get() = loaded

  /**
   * Loads Kolibri's native library under its default name, `expo-kolibri`. Kolibri's own classes
   * call this before they use native code; after any [load] has succeeded it does nothing.
   */
  fun load() = load(DEFAULT_LIBRARY_NAME)

  /**
   * Loads the native library that holds Kolibri's natives, named as `System.loadLibrary` expects
   * it (`foo` for `libfoo.so`).
   *
   * The first successful call wins, and every later one does nothing, whatever name it gives. A
   * library that links Kolibri into its own `.so` must call this with that name before any Kolibri
   * class initializes: those classes call [load] without a name, which asks for `expo-kolibri`.
   */
  fun load(libraryName: String) = synchronized(this) {
    if (loaded || loading) {
      return
    }

    loading = true
    try {
      System.loadLibrary(libraryName)
    } catch (e: UnsatisfiedLinkError) {
      throw IllegalStateException(
        "Kolibri's native library ($libraryName) could not be loaded.",
        e,
      )
    } finally {
      loading = false
    }
    loaded = true
  }
}
