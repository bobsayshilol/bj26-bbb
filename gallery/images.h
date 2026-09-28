#pragma once

#include "graphics.h"

namespace game::images {

struct tiles_mouse {
static constexpr uint8_t pal_offset = 209;
static const uint8_t data[1 * engine::graphics::tile_data_size];
static const uint16_t palette[10];
};

struct tiles_font {
static constexpr uint8_t pal_offset = 219;
static const uint8_t data[40 * engine::graphics::tile_data_size];
static const uint16_t palette[10];
};

struct tiles_arrow_left {
static constexpr uint8_t pal_offset = 229;
static const uint8_t data[1 * engine::graphics::tile_data_size];
static const uint16_t palette[10];
};

struct tiles_bg {
static constexpr uint8_t pal_offset = 239;
static const uint8_t data[1 * engine::graphics::tile_data_size];
static const uint16_t palette[10];
};

} // namespace game::images

// TODO: the above shouldn't be game::images
namespace gallery::images {
using namespace game::images;
} // namespace gallery::images
