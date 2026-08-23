#pragma once

#include "images.h"

namespace game::font {

constexpr uint8_t font_palette_start = 128;
constexpr uint8_t font_palette_count = 10;
constexpr uint8_t font_tile_count = 40;
constexpr uint8_t font_max_sprites = 64; // max chars on screen too

constexpr uint16_t CharWidth = engine::graphics::bg_tile_size;
constexpr uint16_t CharHeight = engine::graphics::bg_tile_size;

namespace detail {
extern uint16_t s_tile_start;
extern uint16_t s_sprite_start;
} // namespace detail

// Setup tile data for the font.
template <uint8_t TileStart, uint8_t SpriteStart>
inline void setup_tiles() {
    game::images::copy_tile_data<
        font_palette_start, font_palette_count,
        TileStart, font_tile_count,
        game::images::text_font
    >();
    detail::s_tile_start = TileStart;
    detail::s_sprite_start = SpriteStart;
}

// Write a line of text somewhere on screen.
// x,y are screen co-ords.
// These will stay around until cleared!
// Limit of font_max_sprites chars, including those off-screen!
void write_text(const char *text, uint16_t x, uint16_t y);

// Note: doesn't work with newlines.
inline void write_centered(const char *text, uint16_t N, uint16_t y) {
    const uint16_t x = engine::graphics::SCREEN_WIDTH / 2 - (N - 1) * CharWidth / 2;
    write_text(text, x, y);
}
inline void write_left(const char *text, uint16_t N, uint8_t padding, uint16_t y) {
    (void)N;
    write_text(text, padding, y);
}
inline void write_right(const char *text, uint8_t N, uint8_t padding, uint16_t y) {
    const uint16_t x = engine::graphics::SCREEN_WIDTH - (N - 1) * CharWidth - padding;
    write_text(text, x, y);
}
template <uint16_t N>
inline void write_centered(const char (&text)[N], uint16_t y) {
    write_centered(text, N, y);
}
template <uint16_t N>
inline void write_left(const char (&text)[N], uint8_t padding, uint16_t y) {
    write_left(text, N, padding, y);
}
template <uint16_t N>
inline void write_right(const char (&text)[N], uint8_t padding, uint16_t y) {
    write_right(text, N, padding, y);
}

// Clear any existing text.
void clear_text();

} // namespace game::font
