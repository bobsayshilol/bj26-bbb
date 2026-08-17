#include "game.h"
#include "graphics.h"
#include "font.h"
#include "input.h"

namespace game::intertile {

namespace {

constexpr uint8_t font_sprite_start = 0;
constexpr uint8_t font_sprite_count = font::font_max_sprites;
constexpr uint8_t font_tile_start = 0;
constexpr uint8_t font_tile_count = font::font_tile_count;

const char * const s_intro[] = {
    "These",
    "Are",
    "Words",
    nullptr,
};
const char * const s_meanwhile[] = {
    "Meanwhile...",
    nullptr,
};
const char * const s_game_over[] = {
    "Game over",
    nullptr,
};

const char *const * s_text;
uint16_t s_line;
Entry s_next;

//

bool next_line() {
    const char * str = s_text[s_line];
    if (str == nullptr) {
        return true;
    }

    // TODO: draw it
    // TODO: fade in/out?
    DEBUG_MSG(str);

    s_line++;
    return false;
}

} // namespace

void setup(Text text, Entry next) {
    switch (text) {
        case Text::Intro: s_text = s_intro; break;
        case Text::Meanwhile: s_text = s_meanwhile; break;
        case Text::GameOver: s_text = s_game_over; break;
    }
    s_line = 0;
    s_next = next;
}

void enter() {
    // Load the sprites.
    font::setup_tiles<font_tile_start, font_sprite_start>();

    // Draw the first line.
    next_line();

    // Only text, so only need sprites.
    bios_vsync();
    engine::graphics::enable_sprites();
}

void leave() {
    // Reset graphics state.
    bios_vsync();
    engine::graphics::disable_sprites();
    engine::graphics::reset_sprites<font_tile_start + font_tile_count>();
    game::font::clear_text();
}

Entry loop() {
    bios_vsync();

    // Advance on button press.
    engine::input::update_inputs();
    const uint16_t pressed = engine::input::g_buttons_pressed;
    if (pressed & GAMEPAD_BTN_A) {
        if (next_line()) {
            return s_next;
        }
    }

    return Entry::Intertitle;
}

} // namespace game::intertile
