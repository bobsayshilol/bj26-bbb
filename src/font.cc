#include "font.h"
#include "graphics.h"

namespace game::font {

namespace {

uint16_t s_sprites_used;

uint32_t char_to_idx(uint8_t ch) {
    if ('A' <= ch && ch <= 'Z') {
        return ch - 'A';
    } else if ('a' <= ch && ch <= 'z') {
        return ch - 'a';
    }
    constexpr uint32_t punc = 'z' - 'a' + 1;

    if (ch == ' ') {
        return punc + 0;
    } else if (ch == '!') {
        return punc + 1;
    } else if (ch == '?') {
        return punc + 2;
    } else if (ch == '.') {
        return punc + 3;
    }
    constexpr uint32_t nums = punc + 4;

    if ('0' <= ch && ch <= '9') {
        return nums + ch - '0';
    }

    return punc + 2; // ?
}

} // namespace

namespace detail {
uint16_t s_tile_start;
uint16_t s_sprite_start;
} // namespace detail

void write_text(const char *text, uint16_t x, uint16_t y) {
    const uint16_t sprite_start = detail::s_sprite_start;
    const uint16_t tile_start = detail::s_tile_start;
    const uint16_t x0 = x;
    uint16_t used = s_sprites_used;

    // Add each character.
    engine::graphics::ObjSprite sprite;
    sprite.set_y(y);
    while (*text) {
        const char ch = *text++;

        // Break on newlines.
        if (ch == '\n') {
            x = x0;
            y += CharHeight;
            sprite.set_y(y);
            continue;
        } else if (ch == ' ') {
            // No need to use a sprite for a space.
            x += CharWidth;
            continue;
        }

        // Lookup the tile.
        const uint32_t tile_idx = tile_start + char_to_idx(ch);
        sprite.set_tile_index(tile_idx);

        // Check it's in bounds.
        if (x > engine::graphics::SCREEN_WIDTH) break;
        sprite.set_x(x);

        // Assign the next sprite to it.
        const uint32_t sprite_idx = sprite_start + used;
        ASSERT(sprite_idx < font_max_sprites);
        engine::graphics::set_sprite(sprite_idx, sprite);

        // Advance to the next character.
        x += CharWidth;
        used++;
    }

    s_sprites_used = used;
}

void clear_text() {
    const uint16_t sprite_start = detail::s_sprite_start;

    // Read and reset how many are used.
    const uint16_t used = s_sprites_used;
    s_sprites_used = 0;

    // Reset any used sprites.
    engine::graphics::ObjSprite sprite;
    for (uint16_t i = 0; i < used; i++) {
        engine::graphics::set_sprite(sprite_start + i, sprite);
    }
}

} // namespace game::font
