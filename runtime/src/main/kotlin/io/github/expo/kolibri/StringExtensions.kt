package io.github.expo.kolibri

fun String.isAscii(): Boolean {
  for (i in indices) {
    if (this[i].code >= 0x80) {
      return false
    }
  }
  return true
}
