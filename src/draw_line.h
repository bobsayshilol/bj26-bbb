#pragma once

#include "utils.h"
#include "profiler.h"

namespace engine::utils {

// TODO: optimise this
inline void draw_line(uint8_t * data, uint16_t width, uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint8_t pal) {
    PROFILE_SCOPE(dr_lin);

    if (x1 < x0) {
        engine::utils::swap(x0, x1);
        engine::utils::swap(y0, y1);
    } else if (x0 == x1 && y0 == y1) {
        // Draw a single dot.
        x1++;
    }

    constexpr uint8_t shift = 16;
    const uint32_t sx0 = uint32_t{x0} << shift;
    const uint32_t sy0 = uint32_t{y0} << shift;
    const uint32_t sx1 = uint32_t{x1} << shift;
    const uint32_t sy1 = uint32_t{y1} << shift;
    const int32_t dsx = sx1 - sx0;
    const int32_t dsy = sy1 - sy0;
    ASSERT(dsx >= 0); // we know x1 > x0
    const int32_t adsx = dsx;
    const int32_t adsy = engine::utils::abs(dsy);

    uint32_t sx = sx0;
    uint32_t sy = sy0;
    int32_t ddsx = 0;
    int32_t ddsy = 0;

    auto step = [&]{
        uint32_t x = sx >> shift;
        uint32_t y = sy >> shift;
        data[y * width + x] = pal;
        sx += ddsx;
        sy += ddsy;
    };

    if (adsy <= adsx) { // moving to the right, small slope
        ddsx = 1 << shift;
        ddsy = dsy / (dsx >> shift);
        while (sx <= sx1) { step(); }
    } else if (dsy > 0) { // tall up slope
        ddsx = dsx / (dsy >> shift);
        ddsy = 1 << shift;
        while (sy <= sy1) { step(); }
    } else { // tall down slope
        ASSERT(dsy < 0);
        ddsx = dsx / (adsy >> shift);
        ddsy = -(1 << shift);
        while (sy >= sy1) { step(); }
    }
}

} // namespace engine::utils
