package io.github.expo.kolibri

private const val LIBRARY_NAME = "expo-kolibri"

object Kolibri {
  @Volatile
  private var loaded = false

  fun load() = synchronized(this) {
    if (loaded) {
      return
    }

    try {
      System.loadLibrary(LIBRARY_NAME)
    } catch (e: UnsatisfiedLinkError) {
      throw IllegalStateException(
        "Kolibri's native library ($LIBRARY_NAME) could not be loaded.",
        e,
      )
    }
    loaded = true
  }
}
