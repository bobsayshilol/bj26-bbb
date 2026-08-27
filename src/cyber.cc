#include "game.h"
#include "graphics.h"
#include "images.h"
#include "input.h"
#include "maths.h"
#include "memory.h"
#include "profiler.h"
#include "sound.h"
#include "music.h"
#include "font.h"
#include "utils.h"

namespace game::cyber {

namespace {

// font has highest prio.
constexpr uint8_t font_sprite_start = 0;
constexpr uint8_t font_sprite_count = font::font_max_sprites;
constexpr uint8_t font_tile_start = 0;
constexpr uint8_t font_tile_count = font::font_tile_count;

constexpr uint8_t bg_pal_start = engine::graphics::pal_transparent + 1;
constexpr uint8_t bg_pal_count = 8 * 8;

//

void wait_until_line0() { while (VDP.VCOUNT != 0) __asm__ volatile ("":::"memory"); }
void wait_until_line(uint16_t line) { while (VDP.VCOUNT < line) __asm__ volatile ("":::"memory"); }

//

constexpr uint8_t entering_pal_size = 16; // +1 really
static_assert(entering_pal_size + 1 <= bg_pal_count);

enum class BGStyle {
    Outside,
    OutsideWavy,
    Entering,
    EnteringGlitchy,
    Done,
} s_bg_style;

uint16_t s_bg_pallete[bg_pal_count];

uint16_t s_bg_timer;

void bg_pallete_update() {
    const uint16_t timer = s_bg_timer;

    switch (s_bg_style) {
        case BGStyle::Outside:
        case BGStyle::OutsideWavy: {
            // TODO: this is just for testing atm

            // Update palette locally.
            static_assert(bg_pal_count == 8 * 8);

            for (uint8_t y = 0; y < 8; y++) {
                engine::utils::rotate_right<1>(s_bg_pallete + y * 8, s_bg_pallete + (y + 1) * 8);
            }
            engine::utils::rotate_right<8>(s_bg_pallete, s_bg_pallete + bg_pal_count);

            // Update in VDP.
            engine::utils::fast_memcpy(VDP.PALETTE + bg_pal_start, s_bg_pallete, sizeof(s_bg_pallete));
        }
        break;

        case BGStyle::Entering:
        case BGStyle::EnteringGlitchy: {
            for (uint8_t i = 0; i <= entering_pal_size; i++) {
                const int8_t t256 = engine::maths::sin(timer - i * (256 / entering_pal_size));
                const uint8_t b = 15 + bios_mathMulS16(t256, 15) / 256;
                engine::graphics::set_palette_colour(bg_pal_start + i, RGB555(0, 0, b));
            }
        } break;

        case BGStyle::Done:
            break;
    }
}

void bg_redraw() {
    using namespace engine::graphics;

    // Reset scroll.
    bitmap_0.scroll_x() = 0;

    switch (s_bg_style) {
        case BGStyle::Outside: {
#if 1
            static_assert(images::wormhole::width == SCREEN_WIDTH);
            static_assert(images::wormhole::height == SCREEN_HEIGHT);
            static_assert(images::wormhole::pal_offset == bg_pal_start);
            static_assert(images::wormhole::pal_size == bg_pal_count);
            images::wormhole::decompress(VDP.BITMAP_VRAM_8BIT);
#else
            // TODO: precalculate this?
            for (int32_t y = 0; y < SCREEN_HEIGHT; y++) {
                // Double up the pixels so that the projected image looks bigger.
                // It also lets us use 16bit writes.
                constexpr uint8_t scale = 2;
                auto to_pal = [](uint8_t u, uint8_t v) {
                    static_assert(bg_pal_count == 8 * 8);
                    const uint8_t pal = bg_pal_start + ((u / scale) & 7) * 8 + ((v / scale) & 7);
                    return pal;
                };

                // F(x,y) = 1 - 1 / (x^2 + y^2)

                const int32_t d = (y - 0) / SCREEN_HEIGHT;
                for (int32_t x = 0; x < SCREEN_WIDTH; x += scale) {
                    // d = (y - horizon) / SCREEN_HEIGHT
                    // z = 1 / d
                    // tx = (x - SCREEN_WIDTH / 2) * z
                    const int16_t tx = x / scale - SCREEN_WIDTH / 2;
                    const int16_t ty = y / scale - SCREEN_HEIGHT / 2;
                    const uint8_t pal = to_pal(tx, ty);
                    const uint16_t pal16 = (uint16_t(pal) << 8) | pal;
                    VDP.BITMAP_VRAM[(y * SCREEN_WIDTH + x) / 2] = pal16;
                }
            }
#endif

            // Setup the image to be projected.
            static_assert(bg_pal_count == 8 * 8);
            for (uint8_t y = 0; y < 8; y++) {
                for (uint8_t x = 0; x < 8; x++) {
                    uint16_t col = RGB555(x * 4, y * 4, 1);
                    s_bg_pallete[y * 8 + x] = col;
                }
            }
        } break;

        case BGStyle::Entering: {
            // TODO: this jumps around all over the place in memory writing 8bit values...
            for (int32_t t = 0; t <= entering_pal_size; t++) {
                for (int16_t ty = -entering_pal_size; ty < entering_pal_size; ty++) {
                    for (int16_t tx = -entering_pal_size; tx < entering_pal_size; tx++) {
                        const auto ax = engine::utils::abs(tx);
                        const auto ay = engine::utils::abs(ty);
                        const bool show = (ax == t && ay <= t) || (ay == t && ax <= t); // show only the square of radius t
                        if (show) {
                            bool which = false;
                            const uint8_t pal0 = bg_pal_start + t;
                            const uint8_t pal1 = bg_pal_start + entering_pal_size - t;
                            for (int32_t y = 0; y < SCREEN_HEIGHT; y += entering_pal_size * 2) {
                                for (int32_t x = 0; x < SCREEN_WIDTH; x += entering_pal_size * 2) {
                                    VDP.BITMAP_VRAM_8BIT[SCREEN_WIDTH * (y + entering_pal_size + ty) + (x + entering_pal_size + tx)] = which ? pal0 : pal1;
                                    which = !which;
                                }
                                which = !which;
                            }
                        }
                    }
                }
            }
        } break;

        case BGStyle::OutsideWavy:
        case BGStyle::EnteringGlitchy:
            // Don't need to redraw these.
            break;

        case BGStyle::Done:
            break;
    }

    bg_pallete_update();
}

void bg_setup() {
    using namespace engine::graphics;

    // Single big image.
    bitmap_0.position_x() = 0;
    bitmap_0.position_y() = 0;
    bitmap_0.width() = SCREEN_WIDTH - 1;
    bitmap_0.height() = SCREEN_HEIGHT - 1;
    bitmap_0.scroll_x() = 0;
    bitmap_0.scroll_y() = 0;

    // Draw it.
    bg_redraw();
}

void bg_update() {
    const uint16_t timer = s_bg_timer++;
    bg_pallete_update();

    switch (s_bg_style) {
        case BGStyle::Done:
        case BGStyle::Outside:
            wait_until_line(engine::graphics::SCREEN_HEIGHT);
            break;

        case BGStyle::OutsideWavy:
        case BGStyle::Entering:
            // TODO: need a smooth state to bring this in
            for (uint8_t line = 4; line < engine::graphics::SCREEN_HEIGHT; line += 4) {
                wait_until_line(line);
                uint16_t shift = engine::maths::sin(timer + line) / 8;
                engine::graphics::bitmap_0.scroll_x() = shift;
            }
            break;

        case BGStyle::EnteringGlitchy: {
            const uint32_t g0 = engine::utils::g_rng();
            const uint32_t g1 = engine::utils::g_rng();
            const uint8_t glitch_lines[8] {
                static_cast<uint8_t>((g0 >>  0) & 0xFF),
                static_cast<uint8_t>((g0 >>  8) & 0xFF),
                static_cast<uint8_t>((g0 >> 16) & 0xFF),
                static_cast<uint8_t>((g0 >> 24) & 0xFF),
                static_cast<uint8_t>((g1 >>  0) & 0xFF),
                static_cast<uint8_t>((g1 >>  8) & 0xFF),
                static_cast<uint8_t>((g1 >> 16) & 0xFF),
                static_cast<uint8_t>((g1 >> 24) & 0xFF),
            };
            for (uint8_t line = 4; line < engine::graphics::SCREEN_HEIGHT; line += 4) {
                wait_until_line(line);
                uint16_t shift = engine::maths::sin(timer + line) / 8;
                for (uint8_t glitch : glitch_lines) {
                    if (line == glitch) {
                        shift = glitch * 3; //engine::graphics::SCREEN_WIDTH / 3;
                        break;
                    }
                }
                engine::graphics::bitmap_0.scroll_x() = shift;
            }
        } break;
    }
}

//

void ui_setup() {
    game::font::setup_tiles<font_tile_start, font_sprite_start>();
}

} // namespace

void enter() {
    // Draw bits.
    bg_setup();
    ui_setup();

    s_bg_style = BGStyle::Outside;

    // Show everything now that it's drawn.
    bios_vsync();
    engine::graphics::bitmap_0.enable();
    engine::graphics::enable_sprites();

    // Kick off the bgm.
    engine::sound::play_bgm(game::music::Bgm::Bgm_Weird);
}

void leave() {
    // Reset graphics state.
    bios_vsync();
    engine::graphics::disable_sprites();
    engine::graphics::bitmap_0.disable();
    engine::graphics::reset_sprites<font_sprite_start + font_sprite_count>();
    font::clear_text();

    // Reset sound.
    engine::sound::stop_bgm();
}

Entry loop() {
    Entry next = Entry::Cyber;

    // Wait for vsync.
    {
        PROFILE_SCOPE(vsync);
        wait_until_line0();
    }

    // Update gamepad/mouse input.
    engine::input::update_inputs();

    // Return to the main menu if requested.
    if (engine::input::g_buttons_pressed & GAMEPAD_BTN_START) {
        return Entry::MainMenu;
    }

    if (engine::input::g_buttons_pressed & GAMEPAD_BTN_A) {
        s_bg_style = BGStyle((int(s_bg_style) + 1) & 3);
        bg_redraw();
    }

    bg_update();

    engine::profiler::print_timings();
    return next;
}

} // namespace game::cyber
