-keep @io.github.expo.kolibri.CalledFromNative class *
-keepclassmembers @io.github.expo.kolibri.CalledFromNative class * {
  native <methods>;
}

-keepclassmembers class * {
  @io.github.expo.kolibri.CalledFromNative *;
  @io.github.expo.kolibri.CalledFromNative <init>(...);
}

# `kolibri::JUnit` reads `kotlin.Unit.INSTANCE` to return `Unit` from a native call.
-keep class kotlin.Unit {
  public static final kotlin.Unit INSTANCE;
}
