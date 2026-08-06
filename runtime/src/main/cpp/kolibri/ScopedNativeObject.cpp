#include <kolibri/ScopedNativeObject.h>

#include <mutex>
#include <unordered_map>
#include <unordered_set>

namespace expo::kolibri {
  namespace {
    std::mutex& scopeMutex() {
      static std::mutex mutex;
      return mutex;
    }

    std::unordered_map<const void*, std::unordered_set<detail::ScopeHook*>>& scopeMap() {
      static std::unordered_map<const void*, std::unordered_set<detail::ScopeHook*>> map;
      return map;
    }
  } // namespace

  namespace detail {
    void bindToScope(ScopeHook& hook, const void* scope) {
      const std::lock_guard lock(scopeMutex());
      hook.scope = scope;
      scopeMap()[scope].insert(&hook);
    }

    void releaseScopedResources(ScopeHook& hook) {
      const std::lock_guard lock(scopeMutex());
      if (hook.scope == nullptr) {
        return;
      }

      const auto it = scopeMap().find(hook.scope);
      if (it != scopeMap().end() && it->second.erase(&hook) != 0) {
        hook.invalidate(hook.owner);
      }
      hook.scope = nullptr;
    }

    void unbindFromScope(ScopeHook& hook) {
      const std::lock_guard lock(scopeMutex());
      if (hook.scope == nullptr) {
        return;
      }

      const auto it = scopeMap().find(hook.scope);
      if (it != scopeMap().end()) {
        it->second.erase(&hook);
      }
      hook.scope = nullptr;
    }
  } // namespace detail

  void invalidateScope(const void* scope) {
    const std::lock_guard lock(scopeMutex());
    const auto it = scopeMap().find(scope);
    if (it == scopeMap().end()) {
      return;
    }
    for (detail::ScopeHook* hook: it->second) {
      hook->scope = nullptr;
      hook->invalidate(hook->owner);
    }
    scopeMap().erase(it);
  }
} // namespace expo::kolibri
