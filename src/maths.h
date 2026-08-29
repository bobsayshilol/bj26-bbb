#pragma once

#include <stdint.h>

#if !WEB_BUILD

namespace engine::maths {

extern "C" int8_t sin_table[256];
extern "C" int8_t cos_table[256];

// sin(2 pi angle / 256) * 127
inline int8_t sin(uint8_t angle) {
    return sin_table[angle];
}

// cos(2 pi angle / 256) * 127
inline int8_t cos(uint8_t angle) {
    return cos_table[angle];
}

} // namespace engine::maths

#else
#include <cmath>

namespace engine::maths {

inline int8_t sin(uint8_t angle) {
    return std::sin(2 * 3.14159 * angle / 256) * 127;
}
inline int8_t cos(uint8_t angle) {
    return std::cos(2 * 3.14159 * angle / 256) * 127;
}

} // namespace engine::maths

#endif
