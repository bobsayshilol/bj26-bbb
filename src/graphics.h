#pragma once

#include "loopy.h"

namespace engine::graphics {

// The graphics mode we'll use everywhere to keep things simpler.
constexpr uint16_t SCREEN_WIDTH = 256;
constexpr uint16_t SCREEN_HEIGHT = 224;
constexpr uint16_t VIDEO_MODE = VIDEO_HEIGHT_224P;

void init();

void draw_something();

inline void set_background_a(uint16_t rgb) { VDP.BACKDROP_A = rgb; }
inline void set_background_b(uint16_t rgb) { VDP.BACKDROP_B = rgb; }

// Each Bitmap views a region of vram.
template <int Index>
struct Bitmap {
    static_assert(0 <= Index && Index < 4);
    static auto & position_x() { return VDP.BM_SCREENX[Index]; }
    static auto & position_y() { return VDP.BM_SCREENY[Index]; }
    static auto & scroll_x() { return VDP.BM_SCROLLX[Index]; }
    static auto & scroll_y() { return VDP.BM_SCROLLY[Index]; }
    static auto & width() { return VDP.BM_WIDTH[Index]; }
    static auto & height() { return VDP.BM_HEIGHT[Index]; }
    static auto & latch() { return VDP.BM_COL_LATCH[Index]; } // maps to buffer_ctrl, not a latch?
    static void enable() { VDP.LAYER_CTRL |= uint16_t(LAYER_ENABLE_BM0) << Index; }
    static void disable() { VDP.LAYER_CTRL &= ~(uint16_t(LAYER_ENABLE_BM0) << Index); }
};
constexpr inline Bitmap<0> bitmap_0;
constexpr inline Bitmap<1> bitmap_1;
constexpr inline Bitmap<2> bitmap_2;
constexpr inline Bitmap<3> bitmap_3;

} // namespace engine::graphics
