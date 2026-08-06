#include <kolibri/ScopedNativeObject.h>
#include <kolibri/native_method.h>

#include "test_helpers.h"

namespace expo::kolibri::tests {
  namespace {
    struct ScopedPeer : ScopedNativeObject<ScopedPeer> {
      static constexpr std::string_view descriptor = "io/github/expo/kolibri/tests/ScopedPeer";

      explicit ScopedPeer(jlong scopeId) {
        bindToScope(reinterpret_cast<const void*>(scopeId));
      }

      void onScopeInvalidated() { invalidatedCount_++; }

      jint invalidatedCount() noexcept { return invalidatedCount_; }

      void releaseScoped() noexcept { releaseScopedResources(); }

      jint invalidatedCount_ = 0;
    };
  } // namespace

  void registerScopedNativeObjectTests(JNIEnv* env) {
    // registerNative<T>, not ScopedPeer::registerNatives: the latter is ambiguous between the
    // JNativeObject and JavaClass bases.
    registerNative<ScopedPeer>(env)
        .method("nativeCreate", [](jlong scopeId) -> jlong {
          return reinterpret_cast<jlong>(new ScopedPeer(scopeId));
        })
        .method("invalidateScope", [](jlong scopeId) noexcept -> void {
          invalidateScope(reinterpret_cast<const void*>(scopeId));
        })
        .method<&ScopedPeer::invalidatedCount>("nativeInvalidatedCount")
        .method<&ScopedPeer::releaseScoped>("nativeReleaseScopedResources")
        .commit();
  }
} // namespace expo::kolibri::tests
