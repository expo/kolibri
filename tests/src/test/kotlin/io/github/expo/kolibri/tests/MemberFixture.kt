package io.github.expo.kolibri.tests

class MemberFixture(
  @JvmField var intField: Int,
  @JvmField var stringField: String,
) {
  fun addOne(value: Int): Int = value + 1

  fun concat(prefix: String, suffix: String): String = prefix + suffix

  fun sum(a: Long, b: Long): Long = a + b

  fun scale(value: Double): Double = value * 2.0

  fun throwing() {
    throw IllegalStateException("fixture failure")
  }

  companion object {
    @JvmField
    var staticIntField: Int = 7

    @JvmStatic
    fun multiply(a: Int, b: Int): Int = a * b

    @JvmStatic
    fun greet(name: String): String = "hello $name"
  }
}
