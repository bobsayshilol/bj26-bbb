#pragma once

#include "graphics.h"

namespace game::images {

constexpr size_t TileSize = engine::graphics::tile_data_size;

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

struct heart {
static constexpr uint8_t pal_offset = 84;
static const uint8_t data[1 * TileSize];
static const uint16_t palette[10];
};

struct skyline_raw {
static constexpr uint8_t pal_offset = 94;
static constexpr uint16_t width = 256;
static constexpr uint16_t height = 64;
static void decompress(uint8_t * output);
static const uint16_t palette[16];
};

struct dome_raw {
static constexpr uint8_t pal_offset = 110;
static constexpr uint16_t width = 256;
static constexpr uint16_t height = 64;
static void decompress(uint8_t * output);
static const uint16_t palette[16];
};

struct cover_raw {
static constexpr uint8_t pal_offset = 108;
static constexpr uint16_t width = 256;
static constexpr uint16_t height = 224;
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

} // namespace game::images
