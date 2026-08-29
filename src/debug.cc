#include "debug.h"

#if ENABLE_DEBUGGING

#if !WEB_BUILD
#include "serial.h"
#else
#include <cstdio>
#include <string>
static std::string s_msg;
#define serial_print(x) s_msg += x;
#define serial_write(x) puts(s_msg.c_str()); s_msg.clear()
#endif

#include "utils.h"

namespace engine::debug {

void init() {
#if !WEB_BUILD
    serial_begin(38400);
#endif
}

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
