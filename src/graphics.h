#pragma once

#include "loopy.h"
#include "debug.h"

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



// TODO: move these to constants.h?
constexpr uint32_t BG_TILEMAP_SIZE_64x64 = 0;
constexpr uint32_t BG_TILEMAP_SIZE_64x32 = 1;
constexpr uint32_t BG_TILEMAP_SIZE_32x64 = 2;
constexpr uint32_t BG_TILEMAP_SIZE_32x32 = 3;

// All sprite data is 8bpp (for now).
constexpr bool sprite_is_8bpp = true;

// BG tiles are 8x8.
constexpr uint32_t bg_tile_size = 8;
constexpr uint32_t bg_tile_size_enum = BG_TILESIZE_8X8;

// BG is 32x32 tiles.
constexpr uint32_t bg_tilemap_size = 32;
constexpr uint32_t bg_tilemap_size_enum = BG_TILEMAP_SIZE_32x32;

// BG0 and BG1 are different tilemaps (for now).
constexpr bool bg_shared_tilemap = false;

// First comes BG0 tile sprites, then BG1 tile sprites (if not shared), then tile data.
constexpr uint32_t sprite_tile_data_start = sizeof(uint16_t) * bg_tilemap_size * bg_tilemap_size * (bg_shared_tilemap ? 1 : 2);

using TileIndex = uint16_t;

enum class SpriteSize : uint32_t {
    Size8x8 = 0,
    Size16x16 = 1,
    Size16x32 = 2,
    Size32x32 = 3,
};

enum class Screen : uint8_t {
    A = 0,
    B = 1,
};

// Movable object sprite.
class ObjSprite {
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    uint32_t x : 9;
    uint32_t y_hi : 1;
    uint32_t size : 2; // SpriteSize
    uint32_t palsel : 2;
    uint32_t x_flip : 1;
    uint32_t y_flip : 1;
    uint32_t y_lo : 8;
    uint32_t tile_index : 8;
#else
    uint32_t tile_index : 8;
    uint32_t y_lo : 8;
    uint32_t y_flip : 1;
    uint32_t x_flip : 1;
    uint32_t palsel : 2;
    uint32_t size : 2; // SpriteSize
    uint32_t y_hi : 1;
    uint32_t x : 9;
#endif

public:
    void set_x(uint16_t x_) {
        x = x_;
    }
    void set_y(uint16_t y_) {
        y_lo = y_;
        y_hi = !!(y_ & 0x100);
    }
    void set_size(SpriteSize size_) {
        ASSERT(size_ == SpriteSize::Size8x8); static_assert(bg_tile_size == 8); // TODO: will need changes to get_tile_data
        size = static_cast<uint32_t>(size_);
    }
    void set_x_flip(bool flip_) {
        x_flip = flip_;
    }
    void set_y_flip(bool flip_) {
        y_flip = flip_;
    }
    void set_tile_index(TileIndex idx_) {
        tile_index = idx_;
    }
};
static_assert(sizeof(ObjSprite) == sizeof(uint32_t));

// BG tile sprite.
class BGSprite {
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    uint16_t tile_index : 11;
    uint16_t screen_idx : 1;
    uint16_t palsel : 2;
    uint16_t x_flip : 1;
    uint16_t y_flip : 1;
#else
    uint16_t y_flip : 1;
    uint16_t x_flip : 1;
    uint16_t palsel : 2;
    uint16_t screen_idx : 1;
    uint16_t tile_index : 11;
#endif

public:
    // Note: we have an extra 3 bits here, so BG tiles should be larger if possible!
    void set_tile_index(TileIndex idx_) {
        tile_index = idx_;
    }
    void set_screen(Screen screen_) {
        screen_idx = static_cast<uint8_t>(screen_);
    }
    void set_x_flip(bool flip_) {
        x_flip = flip_;
    }
    void set_y_flip(bool flip_) {
        y_flip = flip_;
    }
};
static_assert(sizeof(BGSprite) == sizeof(uint16_t));



// Returns data for the given tile.
inline uint8_t * get_tile_data(TileIndex idx) {
    ASSERT(idx < (0x10000 - sprite_tile_data_start) / (bg_tile_size * bg_tile_size));
    const uint32_t offset = idx * bg_tile_size * bg_tile_size;
    return VDP.TILE_VRAM_8BIT + sprite_tile_data_start + offset;
}

// Get a sprite.
inline ObjSprite & get_sprite(uint8_t idx) {
    ASSERT(idx < 128);
    auto * sprites = (ObjSprite *)VDP.OAM;
    return sprites[idx];
}

// Get a BG tile.
template <uint8_t BGx>
inline BGSprite & get_bg_sprite(uint8_t x, uint8_t y) {
    ASSERT(x < bg_tilemap_size);
    ASSERT(y < bg_tilemap_size);
    static_assert(BGx == 0 || (BGx == 1 && !bg_shared_tilemap));
    auto * bg = (BGSprite *)VDP.TILE_VRAM;
    if constexpr (BGx == 1) {
        bg += bg_tilemap_size * bg_tilemap_size;
    }
    bg += y * bg_tilemap_size + x;
    return *bg;
}

} // namespace engine::graphics
