#pragma once

#include "utils.h"

#define ENABLE_DEBUGGING 1

#if ENABLE_DEBUGGING
#include "serial.h"
#endif

namespace engine::debug {

#if ENABLE_DEBUGGING

static inline void init() { serial_begin(9600); }

#define DEBUG_MSG(...) engine::debug::internal::print(__VA_ARGS__)

#define ASSERT(x) if (!(x)) { DEBUG_MSG(__FILE__, ":", __LINE__, ": assert failed: ", #x); while(true); }

namespace internal {

inline void print_type(const char *msg) {
    serial_print(msg);
}

inline void print_type(int32_t i) {
    auto msg = utils::to_string(i);
    serial_print(msg.data());
}

template <typename ...Strs>
inline void print(Strs&&... strs) {
    (..., print_type(strs));
    // TODO: hacked up the emulator to log this out, so newline is always required
    serial_write('\n');
}

} // namespace internal

#else

static inline void init() {}

#define DEBUG_MSG(msg)

#define ASSERT(x)

#endif

} // engine::debug
