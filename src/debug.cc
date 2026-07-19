#include "debug.h"

#if ENABLE_DEBUGGING

#include "serial.h"
#include "utils.h"

namespace engine::debug {

void init() { serial_begin(9600); }

namespace internal {

void print_type(const char *msg) {
    serial_print(msg);
}

void print_type(int32_t i) {
    auto msg = utils::to_string(i);
    serial_print(msg.data());
}

void print_end() {
    // TODO: hacked up the emulator to log this out, so newline is always required
    serial_write('\n');
}

} // namespace internal

} // engine::debug

#endif
