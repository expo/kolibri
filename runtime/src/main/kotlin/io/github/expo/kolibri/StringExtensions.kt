package io.github.expo.kolibri

fun String.isAscii(): Boolean {
  for (c in this) {
    if (c.code >= 0x80) {
      return false
    }
  }
  return true
}
