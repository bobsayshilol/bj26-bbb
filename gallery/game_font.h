#pragma once

#include "images.h"
#include "font.h"

namespace gallery::font {

struct FontConfig {
    static constexpr uint8_t palette_start = 209;
    static constexpr uint8_t palette_count = 10;
    static constexpr uint8_t max_sprites = 64; // max chars on screen too
    using Tileset = gallery::images::tiles_font;
};

template <uint8_t TileStart, uint8_t SpriteStart>
inline void setup_tiles() {
    engine::font::setup_tiles<TileStart, SpriteStart, FontConfig>();
}

} // namespace gallery::font
