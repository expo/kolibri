#include <kolibri/ScopedNativeObject.h>

#include <mutex>
#include <unordered_map>

#include <kolibri/defines.h>

namespace expo::kolibri {
  namespace {
    std::mutex& scopeMutex() {
      NO_DESTROY static std::mutex mutex;
      return mutex;
    }

    std::unordered_map<const void*, detail::ScopeHook>& scopeAnchors() {
      NO_DESTROY static std::unordered_map<const void*, detail::ScopeHook> anchors;
      return anchors;
    }

    // The anchor found last: objects bind to the scope they were made in, mostly the same one.
    const void* lastScope = nullptr;
    detail::ScopeHook* lastAnchor = nullptr;

    detail::ScopeHook& anchorOf(const void* scope) {
      if (scope == lastScope && lastAnchor != nullptr) [[likely]] {
        return *lastAnchor;
      }

      auto [it, inserted] = scopeAnchors().try_emplace(scope);
      detail::ScopeHook& anchor = it->second;
      if (inserted) {
        anchor.prev = &anchor;
        anchor.next = &anchor;
      }
      lastScope = scope;
      lastAnchor = &anchor;
      return anchor;
    }

    void unlink(detail::ScopeHook& hook) noexcept {
      hook.prev->next = hook.next;
      hook.next->prev = hook.prev;
      hook.prev = nullptr;
      hook.next = nullptr;
      hook.scope = nullptr;
    }
  } // namespace

  namespace detail {
    void bindToScope(ScopeHook& hook, const void* scope) {
      const std::lock_guard lock(scopeMutex());
      if (hook.scope != nullptr) {
        unlink(hook);
      }

      ScopeHook& anchor = anchorOf(scope);
      hook.scope = scope;
      hook.prev = &anchor;
      hook.next = anchor.next;
      anchor.next->prev = &hook;
      anchor.next = &hook;
    }

    void releaseScopedResources(ScopeHook& hook) {
      const std::lock_guard lock(scopeMutex());
      if (hook.scope == nullptr) {
        return;
      }

      unlink(hook);
      hook.invalidate(hook.owner);
    }

    void unbindFromScope(ScopeHook& hook) {
      const std::lock_guard lock(scopeMutex());
      if (hook.scope == nullptr) {
        return;
      }

      unlink(hook);
    }
  } // namespace detail

  void invalidateScope(const void* scope) {
    const std::lock_guard lock(scopeMutex());
    const auto it = scopeAnchors().find(scope);
    if (it == scopeAnchors().end()) {
      return;
    }

    const detail::ScopeHook& anchor = it->second;
    while (anchor.next != &anchor) {
      detail::ScopeHook& hook = *anchor.next;
      unlink(hook);
      hook.invalidate(hook.owner);
    }

    if (lastScope == scope) {
      lastScope = nullptr;
      lastAnchor = nullptr;
    }
    scopeAnchors().erase(it);
  }
} // namespace expo::kolibri
