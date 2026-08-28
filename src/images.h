#pragma once

#include "graphics.h"
#include "memory.h"
#include "utils.h"

namespace game::images {

constexpr size_t TileSize = engine::graphics::bg_tile_size * engine::graphics::bg_tile_size;

struct bucko_ball {
static constexpr uint8_t pal_offset = 3;
static const uint8_t data[32 * TileSize];
static const uint16_t palette[10];
};

struct bucko_troll_right {
static constexpr uint8_t pal_offset = 14;
static const uint8_t data[12 * TileSize];
static const uint16_t palette[10];
};

struct bucko_left {
static constexpr uint8_t pal_offset = 24;
static const uint8_t data[12 * TileSize];
static const uint16_t palette[10];
};

struct robucko_right {
static constexpr uint8_t pal_offset = 34;
static const uint8_t data[12 * TileSize];
static const uint16_t palette[10];
};

struct ami_left {
static constexpr uint8_t pal_offset = 34;
static const uint8_t data[12 * TileSize];
static const uint16_t palette[10];
};

struct mm_bouncer {
static constexpr uint8_t pal_offset = 13;
static const uint8_t data[8 * TileSize];
static const uint16_t palette[10];
};

struct mouse {
static constexpr uint8_t pal_offset = 3;
static const uint8_t data[1 * TileSize];
static const uint16_t palette[10];
};

struct car {
static constexpr uint8_t pal_offset = 4;
static const uint8_t data[8 * TileSize];
static const uint16_t palette[10];
};

struct tree {
static constexpr uint8_t pal_offset = 44;
static const uint8_t data[3 * TileSize];
static const uint16_t palette[10];
};

struct bomb {
static constexpr uint8_t pal_offset = 54;
static const uint8_t data[1 * TileSize];
static const uint16_t palette[10];
};

struct ufo {
static constexpr uint8_t pal_offset = 64;
static const uint8_t data[4 * TileSize];
static const uint16_t palette[10];
};

struct gauge {
static constexpr uint8_t pal_offset = 74;
static const uint8_t data[5 * TileSize];
static const uint16_t palette[10];
};

struct skyline_raw {
static constexpr uint8_t pal_offset = 84;
static constexpr uint16_t width = 256;
static constexpr uint16_t height = 64;
static void decompress(uint8_t * output);
static const uint16_t palette[16];
};

struct dome_raw {
static constexpr uint8_t pal_offset = 100;
static constexpr uint16_t width = 256;
static constexpr uint16_t height = 64;
static void decompress(uint8_t * output);
static const uint16_t palette[16];
};

struct wormhole {
static constexpr uint8_t pal_offset = 44;
static constexpr uint8_t pal_size = 64;
static constexpr uint16_t width = 256;
static constexpr uint16_t height = 224;
static void decompress(uint8_t * output);
};

struct text_font {
static constexpr uint8_t pal_offset = 128;
// TODO: should compress this to bits, but there's tons of space in the ROM
static const uint8_t data[40 * TileSize];
static const uint16_t palette[10];
};

template <uint8_t PalStart, uint8_t PalCount, uint8_t TileStart, uint8_t TileCount, typename Tileset>
inline void copy_tile_data() {
    // Set the palette.
    static_assert(PalStart == Tileset::pal_offset);
    static_assert(PalCount == engine::utils::size(Tileset::palette));
    for (int idx = 0; idx < PalCount; idx++) {
        engine::graphics::set_palette_colour(PalStart + idx, Tileset::palette[idx]);
    }

    // Copy each frame to a tile.
    static_assert(TileCount * TileSize == engine::utils::size(Tileset::data));
    for (int idx = 0; idx < TileCount; idx++) {
        auto * dst = engine::graphics::get_tile_data(TileStart + idx);
        auto * tile = Tileset::data + TileSize * idx;
        engine::utils::fast_memcpy(dst, tile, TileSize);
    }
}

} // namespace game::images
