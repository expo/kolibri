#include <string>
#include <thread>

#include <kolibri/Ref.h>
#include <kolibri/class.h>
#include <kolibri/env.h>
#include <kolibri/native_method.h>
#include <kolibri/string_utils.h>

#include "expect.h"
#include "test_helpers.h"

namespace expo::kolibri::tests {
  namespace {
    jboolean adoptSemantics(JNIEnv* env, jstring value) {
      jobject raw = env->NewLocalRef(value);
      const auto ref = Ref<jstring>::adopt(env, raw);
      KOLIBRI_EXPECT(ref.get() == raw);
      KOLIBRI_EXPECT(static_cast<bool>(ref));
      KOLIBRI_EXPECT(env->IsSameObject(ref.get(), value));
      KOLIBRI_EXPECT_EQ(ref.toStdString(env), toStdString(env, value));
      return JNI_TRUE;
    }

    jboolean makeAndClone(JNIEnv* env, jstring value) {
      const auto ref = Ref<jstring>::make(env, value);
      KOLIBRI_EXPECT(static_cast<bool>(ref));
      KOLIBRI_EXPECT(env->IsSameObject(ref.get(), value));

      const auto clone = ref.clone();
      KOLIBRI_EXPECT(static_cast<bool>(clone));
      KOLIBRI_EXPECT(env->IsSameObject(clone.get(), ref.get()));
      KOLIBRI_EXPECT_EQ(clone.toStdString(env), toStdString(env, value));
      return JNI_TRUE;
    }

    jboolean moveSemantics(JNIEnv* env, jstring value) {
      auto source = Ref<jstring>::make(env, value);
      const jobject handle = source.get();

      Ref<jstring> moved(std::move(source));
      KOLIBRI_EXPECT(!source);
      KOLIBRI_EXPECT(moved.get() == handle);

      auto target = Ref<jstring>::make(env, value);
      target = std::move(moved);
      KOLIBRI_EXPECT(!moved);
      KOLIBRI_EXPECT(target.get() == handle);
      return JNI_TRUE;
    }

    jboolean releaseAndReset(JNIEnv* env, jstring value) {
      auto ref = Ref<jstring>::make(env, value);
      const jobject handle = ref.get();
      const jobject released = ref.release();
      KOLIBRI_EXPECT(released == handle);
      KOLIBRI_EXPECT(!ref);

      auto readopted = Ref<jstring>::adopt(env, released);
      KOLIBRI_EXPECT(static_cast<bool>(readopted));
      readopted.reset();
      KOLIBRI_EXPECT(!readopted);

      readopted.reset();
      KOLIBRI_EXPECT(!readopted);
      return JNI_TRUE;
    }

    jboolean nullHandling(JNIEnv* env) {
      Ref<> defaulted;
      KOLIBRI_EXPECT(!defaulted);
      KOLIBRI_EXPECT(defaulted.get() == nullptr);
      defaulted.reset();

      const auto fromNull = Ref<>::make(env, nullptr);
      KOLIBRI_EXPECT(!fromNull);

      const auto cloneOfNull = fromNull.clone();
      KOLIBRI_EXPECT(!cloneOfNull);

      const auto adoptedNull = Ref<>::adopt(env, nullptr);
      KOLIBRI_EXPECT(!adoptedNull);
      return JNI_TRUE;
    }

    jstring unownedPassThrough(JNIEnv* env, jstring value) {
      const UnownedRef<jstring> unowned{value};
      KOLIBRI_EXPECT(unowned.get() == value);
      KOLIBRI_EXPECT(static_cast<bool>(unowned));
      return toJString(env, unowned.toStdString(env));
    }

    jboolean staticCastBorrows(JNIEnv* env, jstring value) {
      const auto ref = Ref<>::make(env, value);

      // Casting an lvalue yields a view: the same handle, still owned by `ref`.
      const auto view = ref.staticCast<jstring>();
      KOLIBRI_EXPECT(view.get() == ref.get());
      KOLIBRI_EXPECT_EQ(view.toStdString(env), toStdString(env, value));
      KOLIBRI_EXPECT(static_cast<bool>(ref));

      // A view retypes again without touching ownership.
      KOLIBRI_EXPECT(view.staticCast<JString>().get() == ref.get());

      const UnownedRef<jobject> unowned{value};
      KOLIBRI_EXPECT_EQ(unowned.staticCast<jstring>().toStdString(env), toStdString(env, value));

      const auto emptyView = Ref<>().staticCast<jstring>();
      KOLIBRI_EXPECT(!emptyView);
      return JNI_TRUE;
    }

    jboolean staticCastTransfersOwnership(JNIEnv* env, jstring value) {
      auto source = Ref<>::make(env, value);
      const jobject handle = source.get();

      const Ref<jstring> typed = std::move(source).staticCast<jstring>();
      KOLIBRI_EXPECT(!source);
      KOLIBRI_EXPECT(typed.get() == handle);
      KOLIBRI_EXPECT_EQ(typed.toStdString(env), toStdString(env, value));

      // clone() hands over an rvalue, so the original owner survives the cast.
      const auto kept = Ref<>::make(env, value);
      const Ref<jstring> copy = kept.clone().staticCast<jstring>();
      KOLIBRI_EXPECT(static_cast<bool>(kept));
      KOLIBRI_EXPECT(copy.get() != kept.get());
      KOLIBRI_EXPECT(env->IsSameObject(copy.get(), kept.get()));

      auto global = GlobalRef<>::make(env, value);
      const GlobalRef<jstring> typedGlobal = std::move(global).staticCast<jstring>();
      KOLIBRI_EXPECT(!global);
      KOLIBRI_EXPECT_EQ(typedGlobal.toStdString(env), toStdString(env, value));

      const Ref<jstring> emptyOwner = Ref<>().staticCast<jstring>();
      KOLIBRI_EXPECT(!emptyOwner);
      return JNI_TRUE;
    }

    jstring staticCastReachesAccessors(JNIEnv* env, UnownedRef<JList> list) {
      // The motivating case: an erased `Ref<>` coming out of a Java collection, retyped so the
      // class accessors become available.
      const Ref<> first = list->get(env, 0);
      std::string joined = first.staticCast<jstring>().toStdString(env);

      const Ref<JString> owned = list->get(env, 1).staticCast<JString>();
      joined += owned.staticCast<jstring>().toStdString(env);
      return toJString(env, joined);
    }

    jstring globalRefAcrossThreads(JNIEnv* env, jstring value) {
      auto global = GlobalRef<jstring>::make(env, value);
      KOLIBRI_EXPECT(static_cast<bool>(global));

      std::string readOnThread;
      std::string failure;
      std::thread worker([&] {
        try {
          JNIEnv* threadEnv = getEnv();
          KOLIBRI_EXPECT(threadEnv != nullptr);
          readOnThread = global.toStdString(threadEnv);
          global.reset();
        } catch (const std::exception& e) {
          failure = e.what();
        }
      });
      worker.join();
      if (!failure.empty()) {
        throw std::runtime_error(failure);
      }

      KOLIBRI_EXPECT(!global);
      return toJString(env, readOnThread);
    }
  } // namespace

  void registerRefTests(JNIEnv* env) {
    registerNative(env, "io/github/expo/kolibri/tests/RefTests")
      .method<&adoptSemantics>("nativeAdoptSemantics")
      .method<&makeAndClone>("nativeMakeAndClone")
      .method<&moveSemantics>("nativeMoveSemantics")
      .method<&releaseAndReset>("nativeReleaseAndReset")
      .method<&nullHandling>("nativeNullHandling")
      .method<&unownedPassThrough>("nativeUnownedPassThrough")
      .method<&staticCastBorrows>("nativeStaticCastBorrows")
      .method<&staticCastTransfersOwnership>("nativeStaticCastTransfersOwnership")
      .method<&staticCastReachesAccessors>("nativeStaticCastReachesAccessors")
      .method<&globalRefAcrossThreads>("nativeGlobalRefAcrossThreads")
      .commit();
  }
} // namespace expo::kolibri::tests
