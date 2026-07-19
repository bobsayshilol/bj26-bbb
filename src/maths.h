#pragma once

#include <stdint.h>

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
