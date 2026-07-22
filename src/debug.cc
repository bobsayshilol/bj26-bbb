#include "debug.h"

#if ENABLE_DEBUGGING

#include "serial.h"
#include "utils.h"

namespace engine::debug {

void init() { serial_begin(38400); }

namespace internal {

void print_type(const char *msg) {
    serial_print(msg);
}

void print_type(uint32_t i) {
    auto msg = utils::to_hex(i);
    serial_print(msg.data());
}

void print_type(as_int i) {
    auto msg = utils::to_string(i.v);
    serial_print(msg.data());
}

void print_end() {
    // TODO: hacked up the emulator to log this out, so newline is always required
    serial_write('\n');
}

} // namespace internal

} // engine::debug

#endif
