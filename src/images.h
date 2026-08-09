#pragma once

#include "graphics.h"

namespace game::images {

constexpr size_t TileSize = engine::graphics::bg_tile_size * engine::graphics::bg_tile_size;

constexpr size_t bucko_ball_offset = 3;
extern const uint8_t bucko_ball_data[16 * TileSize];
extern const uint16_t bucko_ball_pal[16];

} // namespace game::images
