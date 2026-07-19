#pragma once

#include "debug.h"

namespace engine::utils {

// Fast memcpy that assumes src+dst are aligned to 2 bytes, and size is a multiple of 2.
inline void fast_memcpy(void * dst, const void * src, uint32_t elems) {
    ASSERT(((uintptr_t)dst & 1) == 0);
    ASSERT(((uintptr_t)src & 1) == 0);
    ASSERT((elems & 1) == 0);

    auto *dst16 = (uint16_t*)dst;
    auto *src16 = (uint16_t*)src;
    elems >>= 1;
    while (elems) {
        *dst16++ = *src16++;
        elems--;
    }
}

// Fast memset16 that assumes dst is aligned to 2 bytes. elems is in uint16_t's.
inline void fast_memset16(void * dst, uint16_t val, uint32_t elems) {
    ASSERT(((uintptr_t)dst & 1) == 0);

    auto *dst16 = (uint16_t*)dst;
    uint16_t val16 = val;
    val16 |= val16 << 8;
    while (elems) {
        *dst16++ = val16;
        elems--;
    }
}

// Fast memset that assumes dst is aligned to 2 bytes, and elems is a multiple of 2.
inline void fast_memset8(void * dst, uint8_t val, uint32_t elems) {
    ASSERT(((uintptr_t)dst & 1) == 0);
    ASSERT((elems & 1) == 0);

    uint16_t val16 = val;
    val16 |= val16 << 8;
    fast_memset16(dst, val16, elems >> 1);
}

} // namespace engine::utils
