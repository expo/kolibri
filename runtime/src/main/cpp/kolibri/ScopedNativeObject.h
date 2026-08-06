#pragma once

#include <kolibri/NativeObject.h>

namespace expo::kolibri {
  namespace detail {
    // clang-format off
    struct ScopeHook {
      void* owner = nullptr;
      void (*invalidate)(void* owner) = nullptr;
      const void* scope = nullptr;
    };

    void bindToScope(ScopeHook& hook, const void* scope);
    void releaseScopedResources(ScopeHook& hook);
    void unbindFromScope(ScopeHook& hook);
    // clang-format on
  } // namespace detail

  void invalidateScope(const void* scope);

  template<typename Derived>
  class ScopedNativeObject : public NativeObject<Derived> {
  protected:
    void bindToScope(const void* scope) {
      static_assert(
        requires(Derived& derived) { derived.onScopeInvalidated(); },
        "ScopedNativeObject<Derived> requires Derived::onScopeInvalidated() "
        "(private works with `friend kolibri::ScopedNativeObject<Derived>;`)"
      );
      hook_.owner = this;
      hook_.invalidate = [](void* owner) {
        static_cast<Derived*>(static_cast<ScopedNativeObject*>(owner))->onScopeInvalidated();
      };
      detail::bindToScope(hook_, scope);
    }

    void releaseScopedResources() { detail::releaseScopedResources(hook_); }

    ~ScopedNativeObject() override { detail::unbindFromScope(hook_); }

  private:
    detail::ScopeHook hook_;
  };
} // namespace expo::kolibri
