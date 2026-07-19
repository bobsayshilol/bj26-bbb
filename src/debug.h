#pragma once

//
// Debugging helpers.
//
// Usage:
//
//   DEBUG_MSG("x = ", x);
//
//   ASSERT(x > 10);
//

#include <stdint.h>

#define ENABLE_DEBUGGING 1

namespace engine::debug {

#if ENABLE_DEBUGGING

void init();

#define DEBUG_MSG(...) engine::debug::internal::print(__VA_ARGS__)

#define ASSERT(x) if (!(x)) { DEBUG_MSG(__FILE__, ":", __LINE__, ": assert failed: ", #x); while(true); }

namespace internal {

void print_type(const char *msg);
void print_type(int32_t i);
void print_end();

template <typename ...Strs>
inline void print(Strs&&... strs) {
    (..., print_type(strs));
    print_end();
}

} // namespace internal

#else

static inline void init() {}

#define DEBUG_MSG(...)

#define ASSERT(x)

#endif

} // engine::debug
