#include <cstdint>
#include <memory>
#include <vector>

#include <benchmark/benchmark.h>

#include <kolibri/ScopedNativeObject.h>

// ScopedNativeObject bookkeeping, which is plain C++: binding a peer to a scope and unbinding it
// when the peer dies (once per peer), and invalidating a scope with its peers still bound.
namespace expo::kolibri::benchmarks {
  namespace {
    struct ScopedPeer : ScopedNativeObject<ScopedPeer> {
      explicit ScopedPeer(const void* scope) { bindToScope(scope); }

      void rebind(const void* scope) { bindToScope(scope); }

      void onScopeInvalidated() { invalidated_++; }

      int invalidated_ = 0;
    };

    // A peer made and destroyed in the same scope as the one before it: the common case.
    void BM_ScopedPeerLifetime(benchmark::State& state) {
      int scope = 0;
      for (auto _: state) {
        ScopedPeer peer(&scope);
        benchmark::DoNotOptimize(&peer);
      }
      invalidateScope(&scope);
    }

    // Peers alternating between two scopes, so every bind misses the last-scope cache.
    void BM_ScopedPeerLifetimeAlternatingScopes(benchmark::State& state) {
      int scopes[2] = {};
      size_t next = 0;
      for (auto _: state) {
        ScopedPeer peer(&scopes[next]);
        benchmark::DoNotOptimize(&peer);
        next ^= 1;
      }
      invalidateScope(&scopes[0]);
      invalidateScope(&scopes[1]);
    }

    // Many live peers in one scope; each iteration binds one more and drops the oldest, so
    // unbinding has neighbours to unlink from.
    void BM_ScopedPeerLifetimeCrowdedScope(benchmark::State& state) {
      int scope = 0;
      std::vector<std::unique_ptr<ScopedPeer>> live;
      for (int64_t i = 0; i < state.range(0); i++) {
        live.push_back(std::make_unique<ScopedPeer>(&scope));
      }
      size_t oldest = 0;
      for (auto _: state) {
        live[oldest] = std::make_unique<ScopedPeer>(&scope);
        oldest = (oldest + 1) % live.size();
      }
      live.clear();
      invalidateScope(&scope);
    }

    // Binding `count` peers to a scope, then invalidating it. The same peers are re-bound every
    // iteration (invalidation leaves them alive but unbound), so nothing is allocated or paused.
    void BM_BindAndInvalidateScope(benchmark::State& state) {
      const auto count = static_cast<size_t>(state.range(0));
      int scope = 0;
      std::vector<std::unique_ptr<ScopedPeer>> peers;
      for (size_t i = 0; i < count; i++) {
        peers.push_back(std::make_unique<ScopedPeer>(&scope));
      }
      invalidateScope(&scope);
      for (auto _: state) {
        for (const auto& peer: peers) {
          peer->rebind(&scope);
        }
        invalidateScope(&scope);
      }
      state.SetItemsProcessed(static_cast<int64_t>(state.iterations() * count));
    }
  } // namespace

  BENCHMARK(BM_ScopedPeerLifetime);
  BENCHMARK(BM_ScopedPeerLifetimeAlternatingScopes);
  BENCHMARK(BM_ScopedPeerLifetimeCrowdedScope)->Arg(1000);
  BENCHMARK(BM_BindAndInvalidateScope)->Arg(16)->Arg(1024);
} // namespace expo::kolibri::benchmarks
