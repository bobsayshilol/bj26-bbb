#pragma once

#include "loopy.h"
#include "debug.h"

namespace engine::graphics {

// The graphics mode we'll use everywhere to keep things simpler.
constexpr uint16_t SCREEN_WIDTH = 256;
constexpr uint16_t SCREEN_HEIGHT = 224;
constexpr uint16_t VIDEO_MODE = VIDEO_HEIGHT_224P;

void init();

// Backdrop is a solid colour.
inline void set_backdrop_a(uint16_t rgb) { VDP.BACKDROP_A = rgb; }
inline void set_backdrop_b(uint16_t rgb) { VDP.BACKDROP_B = rgb; }

// Use RGB555() for palette colours. 256 colours.
inline void set_palette_colour(uint8_t idx, uint16_t rgb) { VDP.PALETTE[idx] = rgb; }

// Each Bitmap views a region of vram.
template <uint8_t Index>
struct Bitmap {
    static_assert(Index < 4);
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
union ObjSprite {
private:
    struct {
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
    } parts;

public:
    // Value must be read/written as u16x2 or u32x1 (according to MAME).
    uint32_t raw = {};

    void set_x(uint16_t x_) {
        parts.x = x_;
    }
    void set_y(uint16_t y_) {
        parts.y_lo = y_;
        parts.y_hi = !!(y_ & 0x100);
    }
    void set_size(SpriteSize size_) {
        ASSERT(size_ == SpriteSize::Size8x8); static_assert(bg_tile_size == 8); // TODO: will need changes to get_tile_data
        parts.size = static_cast<uint32_t>(size_);
    }
    void set_x_flip(bool flip_) {
        parts.x_flip = flip_;
    }
    void set_y_flip(bool flip_) {
        parts.y_flip = flip_;
    }
    void set_tile_index(TileIndex idx_) {
        parts.tile_index = idx_;
    }
};
static_assert(sizeof(ObjSprite) == sizeof(uint32_t));

// BG tile sprite.
union BGSprite {
private:
    struct {
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
    } parts;

public:
    // Value must be read/written as u16x2 or u32x1 (according to MAME).
    uint16_t raw = {};

    // Note: we have an extra 3 bits here, so BG tiles should be larger if possible!
    void set_tile_index(TileIndex idx_) {
        parts.tile_index = idx_;
    }
    void set_screen(Screen screen_) {
        parts.screen_idx = static_cast<uint8_t>(screen_);
    }
    void set_x_flip(bool flip_) {
        parts.x_flip = flip_;
    }
    void set_y_flip(bool flip_) {
        parts.y_flip = flip_;
    }
};
static_assert(sizeof(BGSprite) == sizeof(uint16_t));



// Returns data for the given tile.
// It's much more efficient to store to this 2 pixels at a time (uint16_t) rather than 1.
// MAME says it's only legal to store uint16_t's too...
using Pixel2 = uint16_t;
inline Pixel2 * get_tile_data(TileIndex idx) {
    ASSERT(idx < (0x10000 - sprite_tile_data_start) / (bg_tile_size * bg_tile_size));
    const uint32_t offset = idx * bg_tile_size * bg_tile_size;
    // |sprite_tile_data_start| and |offset| are both even so this is safe.
    return VDP.TILE_VRAM + ((sprite_tile_data_start + offset) >> 1);
}

// Get a sprite.
inline void set_sprite(uint8_t idx, ObjSprite const & sprite) {
    ASSERT(idx < 128);
    VDP.OAM[idx] = sprite.raw;
}

// Get a BG tile.
template <uint8_t BGx>
inline void set_bg_sprite(uint8_t x, uint8_t y, BGSprite const & sprite) {
    ASSERT(x < bg_tilemap_size);
    ASSERT(y < bg_tilemap_size);
    static_assert(BGx == 0 || (BGx == 1 && !bg_shared_tilemap));
    auto * bg = VDP.TILE_VRAM;
    if constexpr (BGx == 1) {
        bg += bg_tilemap_size * bg_tilemap_size;
    }
    bg += y * bg_tilemap_size + x;
    *bg = sprite.raw;
}



// Backgrounds are made of tiles (see above).
template <uint8_t Index>
struct Background {
    static_assert(Index <= 1);
    static void enable() { VDP.LAYER_CTRL |= uint16_t(LAYER_ENABLE_BG0) << Index; }
    static void disable() { VDP.LAYER_CTRL &= ~(uint16_t(LAYER_ENABLE_BG0) << Index); };
    static void set_sprite(uint8_t x, uint8_t y, const BGSprite & sprite) { set_bg_sprite<Index>(x, y, sprite); }
};
constexpr inline Background<0> background_0;
constexpr inline Background<1> background_1;

// For now only OBJ0 is displayed (OBJ1 requires tile index offsets).
inline void enable_sprites() { VDP.LAYER_CTRL |= uint16_t(LAYER_ENABLE_OBJ0); }
inline void disable_sprites() { VDP.LAYER_CTRL &= ~uint16_t(LAYER_ENABLE_OBJ0); }

inline void reset_sprites(uint8_t count) {
    ASSERT(count < 128);
    ObjSprite sprite;
    for (int i = 0; i < count; i++) {
        set_sprite(i, sprite);
    }
}

} // namespace engine::graphics
