#pragma once

#include "images.h"
#include "font.h"

namespace game::font {

struct FontConfig {
    static constexpr uint8_t palette_start = 128;
    static constexpr uint8_t palette_count = 10;
    static constexpr uint8_t max_sprites = 64; // max chars on screen too
    using Tileset = game::images::text_font;
};

template <uint8_t TileStart, uint8_t SpriteStart>
inline void setup_tiles() {
    engine::font::setup_tiles<TileStart, SpriteStart, FontConfig>();
}


// For backwards compat.
constexpr uint8_t font_palette_start = FontConfig::palette_start;
constexpr uint8_t font_palette_count = FontConfig::palette_count;
constexpr uint8_t font_tile_count = engine::font::tile_count;
constexpr uint8_t font_max_sprites = FontConfig::max_sprites;
using engine::font::write_text;
using engine::font::write_left;
using engine::font::write_right;
using engine::font::write_centered;
using engine::font::clear_text;
using engine::font::glitch_it;
using engine::font::check_line;
using engine::font::CharWidth;
using engine::font::CharHeight;

} // namespace game::font
