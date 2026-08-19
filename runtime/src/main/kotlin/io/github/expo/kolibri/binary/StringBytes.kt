package io.github.expo.kolibri.binary

/** Copies a string already proven to be 7-bit ASCII without allocating an intermediate array. */
@Suppress("DEPRECATION", "NOTHING_TO_INLINE", "PLATFORM_CLASS_MAPPED_TO_KOTLIN")
internal inline fun String.copyAsciiTo(destination: ByteArray) {
  (this as java.lang.String).getBytes(0, length, destination, 0)
}
