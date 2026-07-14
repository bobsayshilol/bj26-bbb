#pragma once

#define ENABLE_DEBUGGING 1

#if ENABLE_DEBUGGING
#include "serial.h"
#endif

namespace engine::debug {

#if ENABLE_DEBUGGING

static void init() { serial_begin(9600); }

// TODO: hacked up the emulator to log this out, so newline is always required
#define DEBUG_MSG(msg) do { serial_print(msg "\n"); } while (false)

#else

static void init() {}

#define DEBUG_MSG(msg)

#endif

} // engine::debug
