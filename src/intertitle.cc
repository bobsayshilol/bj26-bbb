#include "debug.h"
#include "game.h"
#include "graphics.h"
#include "font.h"
#include "input.h"
#include "utils.h"
#include "vector.h"

namespace game::intertile {

namespace {

constexpr uint8_t font_sprite_start = 0;
constexpr uint8_t font_sprite_count = font::font_max_sprites;
constexpr uint8_t font_tile_start = 0;
constexpr uint8_t font_tile_count = font::font_tile_count;
constexpr uint8_t font_pal_start = font::font_palette_start;
constexpr uint8_t font_pal_count = font::font_palette_count;


constexpr uint8_t max_splits = 3;
struct Lines {
    uint8_t count;
    const char *text;
    template <uint8_t N>
    constexpr Lines(const char (&str)[N]) : count(0), text(str) {
        for (char ch : str) {
            if (ch == '\0') count++;
        }
        ASSERT(count <= max_splits);
    }
    constexpr Lines(decltype(nullptr)) : count(0), text() {}
};

constexpr Lines s_intro[] = {
    "Years in the future",
    "But not that many",
    "Things have happened",
    "Lots of things",
    "Like so many things",
    "It would take hours to\0explain them all",
    "Maybe even days",
    "A week if you took\0breaks",
    "Not more than a\0month though",
    "A year is right out",
    "...",
    "Anyway",
    "The year is 20XX",
    "The buckos have been\0left unattended\0for too long",
    //"In that time they somehow\0invented robobuckos",
    "Now Buckopia has been\0overtaken by\0evil robobuckos!",
    "But enough backstory",
    "Here is a breakout clone",
    nullptr,
};
constexpr Lines s_meanwhile[] = {
    "Meanwhile...",
    nullptr,
};
constexpr Lines s_game_over[] = {
    "Game over",
    nullptr,
};

const Lines * s_text;
uint16_t s_line;
Entry s_next;

//

// TODO: move to font.cc
void write_centered_runtime(const Lines & lines) {
    // Split into lines.
    struct Span { const char *data; uint8_t size; };
    engine::utils::Vector<Span, max_splits> spans;
    const char *text = lines.text;
    for (int i = 0; i < lines.count; i++) {
        auto &span = spans.push_back({});
        span.data = text;
        span.size = engine::utils::strlen(text);
        text += span.size + 1;
    }

    // Work out where it'll go.
    const uint16_t y_spacing = font::CharHeight * 2;
    uint16_t y = (engine::graphics::SCREEN_HEIGHT - y_spacing * spans.size()) / 2;

    // Draw it.
    for (const Span & span : spans) {
        const uint16_t x0 = (engine::graphics::SCREEN_WIDTH - font::CharWidth * span.size) / 2;
        font::write_text(span.data, x0, y);
        y += y_spacing;
    }
}

bool next_line() {
    const Lines & lines = s_text[s_line];
    if (lines.count == 0) {
        return true;
    }

    // TODO: fade in/out?
    font::clear_text();
    write_centered_runtime(lines);

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
