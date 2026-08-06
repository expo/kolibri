#pragma once

#define ALWAYS_INLINE inline __attribute__((always_inline))

#define ALWAYS_INLINE_LAMBDA __attribute__((always_inline))

#if defined(__GNUC__)
#define HIDDEN __attribute__((visibility("hidden")))
#else
#define HIDDEN
#endif

namespace expo::kolibri {
  inline constexpr int kMaxLocalFrameSize = 128;
}
