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
    "Or are they robuckos?",
    "But enough exposition",
    "Here's a breakout clone",
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
constexpr Lines s_dome[] = {
    "Outside the dome",
    nullptr,
};
constexpr Lines s_finished[] = {
    "You win!",
    "Thanks for playing!",
    "Ami will return",
    nullptr,
};

const Lines * s_text;
uint16_t s_line;
Entry s_next;
Entry s_current;

//

constexpr uint16_t fade_in_frames = 80;
constexpr uint16_t fade_display_frames = 120;
uint16_t s_fade_tick;

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

void next_line() {
    font::clear_text();

    const Lines & lines = s_text[s_line];
    if (lines.count > 0) {
        write_centered_runtime(lines);
        s_line++;
        s_fade_tick = 0;
    } else {
        s_current = s_next;
    }
}

//

void fade_palette(uint16_t tick) {
    for (uint8_t pal_idx = 0; pal_idx < font_pal_count; pal_idx++) {
        // Decompose palette.
        uint16_t rgb = game::images::text_font::palette[pal_idx];
        uint8_t r = (rgb >> 10) & 31;
        uint8_t g = (rgb >>  5) & 31;
        uint8_t b = (rgb >>  0) & 31;

        // Fade them.
        r = bios_mathDivU16(bios_mathMulU16(tick, r), fade_in_frames);
        g = bios_mathDivU16(bios_mathMulU16(tick, g), fade_in_frames);
        b = bios_mathDivU16(bios_mathMulU16(tick, b), fade_in_frames);

        // Store it back.
        rgb = RGB555(r, g, b);
        engine::graphics::set_palette_colour(font_pal_start + pal_idx, rgb);
    }
}

void fade_tick() {
    constexpr uint16_t display_until = fade_in_frames + fade_display_frames;
    constexpr uint16_t fade_out_until = display_until + fade_in_frames;

    const uint16_t tick = s_fade_tick++;
    if (tick <= fade_in_frames) {
        fade_palette(tick);
    } else if (tick <= display_until) {
        // Nothing to do.
    } else if (tick <= fade_out_until) {
        fade_palette(fade_out_until - tick);
    } else {
        // Advance to the next line anyway.
        next_line();
    }
}

} // namespace

void setup(Text text, Entry next) {
    switch (text) {
        case Text::Intro: s_text = s_intro; break;
        case Text::Meanwhile: s_text = s_meanwhile; break;
        case Text::GameOver: s_text = s_game_over; break;
        case Text::Dome: s_text = s_dome; break;
        case Text::Won: s_text = s_finished; break;
    }
    s_line = 0;
    s_next = next;
}

void enter() {
    // Load the sprites.
    font::setup_tiles<font_tile_start, font_sprite_start>();

    // Reset stuff.
    s_current = Entry::Intertitle;
    fade_palette(0);

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
        next_line();
    }

    fade_tick();
    return s_current;
}

} // namespace game::intertile
