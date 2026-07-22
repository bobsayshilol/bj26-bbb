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

// Converting to int is much slower than hex, so default to hex.
#define AS_INT(x) engine::debug::internal::as_int{.v = (int32_t)x}

#define ASSERT(x) if (!(x)) { DEBUG_MSG(__FILE__, ":", __LINE__, ": assert failed: ", #x); while(true); }

namespace internal {

struct as_int { int32_t v; };

void print_type(const char *msg);
void print_type(uint32_t i);
void print_type(as_int i);
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

#define AS_INT(x)

#define ASSERT(x)

#endif

} // engine::debug
