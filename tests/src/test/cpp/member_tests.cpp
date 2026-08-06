#include <string>
#include <variant>

#include <kolibri/JavaClass.h>
#include <kolibri/box.h>
#include <kolibri/class.h>
#include <kolibri/exception.h>
#include <kolibri/native_method.h>

#include "expect.h"
#include "test_helpers.h"

namespace expo::kolibri::tests {
  namespace {
    struct JMemberFixture : JavaClass<JMemberFixture> {
      static constexpr std::string_view descriptor = "io/github/expo/kolibri/tests/MemberFixture";
      static constexpr Constructor<void(jint, jstring)> constructor{};
      static constexpr Method<"addOne", jint(jint)> addOne{};
      static constexpr Method<"concat", Ref<jstring>(jstring, jstring)> concat{};
      static constexpr Method<"sum", jlong(jlong, jlong)> sum{};
      static constexpr Method<"scale", jdouble(jdouble) noexcept> scale{};
      static constexpr Method<"throwing", void()> throwing{};
      static constexpr Field<"intField", jint> intField{};
      static constexpr Field<"stringField", Ref<jstring>> stringField{};
      static constexpr StaticMethod<"multiply", jint(jint, jint)> multiply{};
      static constexpr StaticMethod<"greet", Ref<jstring>(jstring)> greet{};
      static constexpr StaticField<"staticIntField", jint> staticIntField{};
    };

    jboolean constructAndAccess(JNIEnv* env) {
      const auto fixture = JMemberFixture::constructor(env, 5, "ctor");
      KOLIBRI_EXPECT(static_cast<bool>(fixture));
      KOLIBRI_EXPECT_EQ(JMemberFixture::intField(env, fixture), 5);
      KOLIBRI_EXPECT_EQ(JMemberFixture::stringField(env, fixture).toStdString(env),
                        std::string("ctor"));
      const auto another = JMemberFixture::constructor(env, 6, "again");
      KOLIBRI_EXPECT_EQ(JMemberFixture::intField(env, another), 6);
      return JNI_TRUE;
    }

    jboolean callInstanceMethods(JNIEnv* env, UnownedRef<JMemberFixture> fixture) {
      KOLIBRI_EXPECT_EQ(JMemberFixture::addOne(env, fixture, 41), 42);
      KOLIBRI_EXPECT_EQ(JMemberFixture::addOne(env, fixture, -1), 0);
      KOLIBRI_EXPECT_EQ(
        JMemberFixture::concat(env, fixture, std::string("foo"), std::string("bar"))
          .toStdString(env),
        std::string("foobar")
      );
      KOLIBRI_EXPECT_EQ(
        JMemberFixture::sum(env, fixture, jlong{40'000'000'000}, jlong{2}),
        jlong{40'000'000'002}
      );
      KOLIBRI_EXPECT_EQ(JMemberFixture::scale(env, fixture, 2.25), 4.5);
      return JNI_TRUE;
    }

    jboolean mutateFields(JNIEnv* env, UnownedRef<JMemberFixture> fixture) {
      JMemberFixture::intField.set(env, fixture, 42);
      KOLIBRI_EXPECT_EQ(JMemberFixture::intField(env, fixture), 42);
      JMemberFixture::stringField.set(env, fixture, std::string("updated"));
      KOLIBRI_EXPECT_EQ(JMemberFixture::stringField(env, fixture).toStdString(env),
                        std::string("updated"));
      return JNI_TRUE;
    }

    jboolean callStatics(JNIEnv* env) {
      KOLIBRI_EXPECT_EQ(JMemberFixture::multiply(env, 6, 7), 42);
      KOLIBRI_EXPECT_EQ(JMemberFixture::multiply(env, 3, 3), 9);
      KOLIBRI_EXPECT_EQ(
        JMemberFixture::greet(env, std::string("kolibri")).toStdString(env),
        std::string("hello kolibri")
      );
      KOLIBRI_EXPECT_EQ(JMemberFixture::staticIntField(env), 7);
      JMemberFixture::staticIntField.set(env, 11);
      KOLIBRI_EXPECT_EQ(JMemberFixture::staticIntField(env), 11);
      JMemberFixture::staticIntField.set(env, 7);
      return JNI_TRUE;
    }

    jboolean tokenCallbackThrows(JNIEnv* env, UnownedRef<JMemberFixture> fixture) {
      try {
        JMemberFixture::throwing(env, fixture);
      } catch (const JavaException& e) {
        const std::string message = e.what();
        KOLIBRI_EXPECT(message.find("IllegalStateException") != std::string::npos);
        KOLIBRI_EXPECT(message.find("fixture failure") != std::string::npos);
        // throwPending consumed the pending Java exception before rethrowing as C++.
        KOLIBRI_EXPECT(!env->ExceptionCheck());
        return JNI_TRUE;
      }
      KOLIBRI_EXPECT(false);
      return JNI_FALSE;
    }

    jboolean boxedPrimitives(JNIEnv* env) {
      const auto boxedInt = JInteger::valueOf(env, 41);
      KOLIBRI_EXPECT_EQ(JInteger::intValue(env, boxedInt), 41);
      const auto boxedLong = JLong::valueOf(env, jlong{1} << 40);
      KOLIBRI_EXPECT_EQ(JLong::longValue(env, boxedLong), jlong{1} << 40);
      const auto boxedBool = JBoolean::valueOf(env, true);
      KOLIBRI_EXPECT(JBoolean::booleanValue(env, boxedBool) == JNI_TRUE);
      const auto boxedDouble = JDouble::valueOf(env, 2.5);
      KOLIBRI_EXPECT_EQ(JDouble::doubleValue(env, boxedDouble), 2.5);
      return JNI_TRUE;
    }

    jboolean boxAndUnbox(JNIEnv* env) {
      const auto boxedBool = Ref<>::adopt(env, box(env, true));
      const Boxed unboxedBool = unbox(env, boxedBool);
      KOLIBRI_EXPECT(std::get<jboolean>(unboxedBool) == JNI_TRUE);
      const auto boxedInt = Ref<>::adopt(env, box(env, 42));
      const Boxed unboxedInt = unbox(env, boxedInt);
      KOLIBRI_EXPECT_EQ(std::get<jint>(unboxedInt), 42);
      const auto boxedLong = Ref<>::adopt(env, box(env, jlong{1} << 40));
      const Boxed unboxedLong = unbox(env, boxedLong);
      KOLIBRI_EXPECT_EQ(std::get<jlong>(unboxedLong), jlong{1} << 40);
      const auto boxedFloat = Ref<>::adopt(env, box(env, 1.5f));
      const Boxed unboxedFloat = unbox(env, boxedFloat);
      KOLIBRI_EXPECT_EQ(std::get<jfloat>(unboxedFloat), 1.5f);
      const auto boxedDouble = Ref<>::adopt(env, box(env, 2.25));
      const Boxed unboxedDouble = unbox(env, boxedDouble);
      KOLIBRI_EXPECT_EQ(std::get<jdouble>(unboxedDouble), 2.25);
      KOLIBRI_EXPECT(std::holds_alternative<std::monostate>(unbox(env, nullptr)));
      // A non-boxed object (a String) is reported as monostate, not misread as a primitive.
      const auto str = JMemberFixture::greet(env, std::string("x"));
      KOLIBRI_EXPECT(std::holds_alternative<std::monostate>(unbox(env, str)));
      return JNI_TRUE;
    }

    jboolean typedUnbox(JNIEnv* env) {
      KOLIBRI_EXPECT(unbox(env, JBoolean::valueOf(env, true)) == JNI_TRUE);
      KOLIBRI_EXPECT_EQ(unbox(env, JInteger::valueOf(env, 41)), 41);
      KOLIBRI_EXPECT_EQ(unbox(env, JLong::valueOf(env, jlong{1} << 40)), jlong{1} << 40);
      KOLIBRI_EXPECT_EQ(unbox(env, JFloat::valueOf(env, 1.5f)), 1.5f);
      KOLIBRI_EXPECT_EQ(unbox(env, JDouble::valueOf(env, 2.25)), 2.25);

      // Works with any ref kind, not just local Refs.
      const auto global = GlobalRef<JInteger>::make(env, JInteger::valueOf(env, 7));
      KOLIBRI_EXPECT_EQ(unbox(env, global), 7);
      const auto owner = JDouble::valueOf(env, 3.5);
      const UnownedRef<JDouble> unowned{owner.get()};
      KOLIBRI_EXPECT_EQ(unbox(env, unowned), 3.5);
      return JNI_TRUE;
    }

    jint sumIntegerList(JNIEnv* env, UnownedRef<JList> values) {
      KOLIBRI_EXPECT_EQ(values->size(env), 3);
      jint sum = 0;
      auto it = values->iterator(env);
      while (it->hasNext(env) == JNI_TRUE) {
        const auto element = it->next(env);
        sum += JInteger::intValue(env, element);
      }
      return sum;
    }
  } // namespace

  void registerMemberTests(JNIEnv* env) {
    registerNative(env, "io/github/expo/kolibri/tests/MemberTests")
      .method<&constructAndAccess>("nativeConstructAndAccess")
      .method<&callInstanceMethods>("nativeCallInstanceMethods")
      .method<&mutateFields>("nativeMutateFields")
      .method<&callStatics>("nativeCallStatics")
      .method<&tokenCallbackThrows>("nativeTokenCallbackThrows")
      .method<&boxedPrimitives>("nativeBoxedPrimitives")
      .method<&boxAndUnbox>("nativeBoxAndUnbox")
      .method<&typedUnbox>("nativeTypedUnbox")
      .method<&sumIntegerList>("nativeSumIntegerList")
      .commit();
  }
} // namespace expo::kolibri::tests
