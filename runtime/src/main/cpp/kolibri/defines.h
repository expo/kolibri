#pragma once

#define ALWAYS_INLINE inline __attribute__((always_inline))

#define ALWAYS_INLINE_LAMBDA __attribute__((always_inline))

#if defined(__GNUC__)
#define HIDDEN __attribute__((visibility("hidden")))
#else
#define HIDDEN
#endif

#define NOINLINE [[gnu::noinline]]

#define COLD [[gnu::cold]]

#define NORETURN [[noreturn]]

// A static that is never destroyed: it lives until the process ends, and a destructor run at exit
// only adds code and races with threads that still use it. Without Clang the destructor stays.
#if defined(__clang__)
#define NO_DESTROY [[clang::no_destroy]]
#else
#define NO_DESTROY
#endif

namespace expo::kolibri {
  inline constexpr int kMaxLocalFrameSize = 128;
}
