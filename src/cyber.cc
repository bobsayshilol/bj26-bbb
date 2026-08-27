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

PROFILE_STORAGE(bg_pud);
PROFILE_STORAGE(bg_drw);
PROFILE_STORAGE(vsync);

// font has highest prio.
constexpr uint8_t font_sprite_start = 0;
constexpr uint8_t font_sprite_count = font::font_max_sprites;
constexpr uint8_t font_tile_start = 0;
constexpr uint8_t font_tile_count = font::font_tile_count;

constexpr uint8_t bg_pal_start = engine::graphics::pal_transparent + 1;
constexpr uint8_t bg_pal_count = 8 * 8;

constexpr uint8_t entering_pal_size = 16; // +1 really
static_assert(entering_pal_size + 1 <= bg_pal_count);

constexpr uint8_t cover_pal_start = bg_pal_start + bg_pal_count;
constexpr uint8_t cover_pal_count = 16;


constexpr uint8_t font_pal_start = font::font_palette_start;
constexpr uint8_t font_pal_count = font::font_palette_count;
static_assert(cover_pal_start + cover_pal_count <= font_pal_start);

//

void wait_until_line0() { while (VDP.VCOUNT != 0) __asm__ volatile ("":::"memory"); }
void wait_until_line(uint16_t line) { while (VDP.VCOUNT < line) __asm__ volatile ("":::"memory"); }

//

enum class LevelState {
    Start,
    Reveal,
    RevealFinished,
    OutsideCode,
    OutsideTalk,
    Entering,
    EnteringGlitchy,
    Finished,
} s_level_state;

enum class CodeState {
    CodeA, // upside down V
    CodeM, // clearly a capital M
    CodeI, // the number 1
    CodeC, // square pacman facing right
    CodeU, // square pacman on its back
    CodeT,
    CodeE,
    Done,
} s_code_state;

void level_advance(LevelState state);

//

enum class BGStyle {
    Outside,
    OutsideWavy,
    Entering,
    EnteringGlitchy,
} s_bg_style;

uint16_t s_bg_pallete[bg_pal_count];

uint16_t s_bg_timer;

enum WormSpeed : uint16_t {
    None = 0,
    //EnterSlow = 0x0001,
    //EnterFast = 0x00FF,
    SpinSlow = 0x0100,
    SpinFast = 0xFF00,
    //SpinFastEnterSlow = 0xFF01,
    SpinFastEnterFast = 0xFFFF,
} s_wormhole_speed;

void bg_pallete_update() {
    PROFILE_SCOPE(bg_pud);

    const uint16_t timer = s_bg_timer;

    switch (s_bg_style) {
        case BGStyle::Outside:
        case BGStyle::OutsideWavy: {
            // TODO: this is just for testing atm

            // Update palette locally.
            static_assert(bg_pal_count == 8 * 8);

            const uint8_t worm_spin = s_wormhole_speed;
            const uint8_t worm_enter = s_wormhole_speed >> 8;

            if (timer & worm_spin) {
                for (uint8_t y = 0; y < 8; y++) {
                    engine::utils::rotate_left<1>(s_bg_pallete + y * 8, s_bg_pallete + (y + 1) * 8);
                }
            }
            if (timer & worm_enter) {
                engine::utils::rotate_right<8>(s_bg_pallete, s_bg_pallete + bg_pal_count);
            }

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
    }
}

void bg_redraw() {
    PROFILE_SCOPE(bg_drw);

    using namespace engine::graphics;

    // Reset scroll.
    bitmap_1.scroll_x() = 0;

    switch (s_bg_style) {
        case BGStyle::Outside: {
            // Copy the wormhole to the first half of VRAM.
            static_assert(images::wormhole::width == SCREEN_WIDTH);
            static_assert(images::wormhole::height == SCREEN_HEIGHT);
            static_assert(images::wormhole::pal_offset == bg_pal_start);
            static_assert(images::wormhole::pal_size == bg_pal_count);
            images::wormhole::decompress(VDP.BITMAP_VRAM_8BIT);

            // Setup the image to be projected.
            static_assert(bg_pal_count == 8 * 8);
            for (uint8_t y = 0; y < 8; y++) {
                for (uint8_t x = 0; x < 8; x++) {
                    uint16_t col = (x == 0 || y == 0) ? RGB555(0,0,31) : RGB555(0,0,0);
                    s_bg_pallete[y * 8 + x] = col;
                }
            }
            //s_bg_pallete[2 * 8 + 6] = RGB555(15, 15, 15);
            //s_bg_pallete[6 * 8 + 6] = RGB555(15, 15, 15);
            //s_bg_pallete[3 * 8 + 2] = RGB555(15, 15, 15);
            //s_bg_pallete[4 * 8 + 2] = RGB555(15, 15, 15);
            //s_bg_pallete[5 * 8 + 2] = RGB555(15, 15, 15);

            // Copy a cover image.
            // TODO: proper image
            engine::utils::fast_memset8(VDP.BITMAP_VRAM_8BIT + SCREEN_WIDTH * SCREEN_HEIGHT, cover_pal_start, SCREEN_WIDTH * SCREEN_HEIGHT);
            engine::graphics::set_palette_colour(cover_pal_start, RGB555(15,0,0));
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
    }

    bg_pallete_update();
}

void bg_setup() {
    using namespace engine::graphics;

    // Single big image.
    bitmap_1.position_x() = 0;
    bitmap_1.position_y() = 0;
    bitmap_1.width() = SCREEN_WIDTH - 1;
    bitmap_1.height() = SCREEN_HEIGHT - 1;
    bitmap_1.scroll_x() = 0;
    bitmap_1.scroll_y() = 0;

    // Cover over it.
    bitmap_0.position_x() = 0;
    bitmap_0.position_y() = 0;
    bitmap_0.width() = SCREEN_WIDTH - 1;
    bitmap_0.height() = SCREEN_HEIGHT - 1;
    bitmap_0.scroll_x() = 0;
    bitmap_0.scroll_y() = SCREEN_HEIGHT;
}

void bg_update() {
    const uint16_t timer = s_bg_timer++;
    bg_pallete_update();

    switch (s_bg_style) {
        case BGStyle::Outside:
            wait_until_line(engine::graphics::SCREEN_HEIGHT);
            break;

        case BGStyle::OutsideWavy:
            // TODO: need a smooth state to bring this in
            for (uint8_t line = 4; line < engine::graphics::SCREEN_HEIGHT; line += 4) {
                wait_until_line(line);
                uint16_t shift = engine::maths::sin(timer + line) / 4;
                engine::graphics::bitmap_1.scroll_x() = shift;
            }
            break;

        case BGStyle::Entering:
            for (uint8_t line = 4; line < engine::graphics::SCREEN_HEIGHT; line += 4) {
                wait_until_line(line);
                uint16_t shift = engine::maths::sin(timer + line) / 8;
                engine::graphics::bitmap_1.scroll_x() = shift;
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
                engine::graphics::bitmap_1.scroll_x() = shift;
            }
        } break;
    }
}

//

void ui_setup() {
    game::font::setup_tiles<font_tile_start, font_sprite_start>();
}

void ui_update() {
    PROFILE_SCOPE(ui_upd);

    // Do nothing if we're not in a UI state.
    bool is_ui = false;
    switch (s_level_state) {
        case LevelState::Start:
        case LevelState::RevealFinished:
        case LevelState::OutsideTalk:
        case LevelState::Entering:
        case LevelState::EnteringGlitchy:
            is_ui = true;
            break;
        case LevelState::Reveal:
        case LevelState::OutsideCode:
        case LevelState::Finished:
            break;
    }
    if (!is_ui) return;

    if (engine::input::g_buttons_pressed & GAMEPAD_BTN_A) {
#if 1 // TODO: speech
        level_advance((LevelState)((int)s_level_state + 1));
#else
        // Advance to next line.
        auto *speech = ++s_current_speech;
        if (speech->text == nullptr) {
            // We're finished, change state.
            level_advance(s_next_state);
        } else {
            ui_redraw();
        }
#endif
    }
}

//

bool update_logic() {
    bool finished = false;

    switch (s_level_state) {
        case LevelState::Start:
        case LevelState::RevealFinished:
        case LevelState::OutsideTalk:
        case LevelState::Entering:
        case LevelState::EnteringGlitchy:
            // Event driven logic.
            break;

        case LevelState::OutsideCode:
            // TODO: input handling
            break;

        case LevelState::Reveal: {
            // Remove the cover.
            constexpr uint16_t dh = 1;
            engine::graphics::bitmap_0.scroll_y() += dh;
            engine::graphics::bitmap_0.height() -= dh;
            uint16_t h = engine::graphics::bitmap_0.height();
            if (h < dh) {
                level_advance(LevelState::RevealFinished);
            }
        } break;

        case LevelState::Finished:
            finished = true;
            break;
    }

    return finished;
}

//

void level_advance(LevelState state) {
    DEBUG_MSG("state:", AS_INT(state));
    s_level_state = state;
    switch (state) {
        case LevelState::Start:
            s_bg_style = BGStyle::Outside;
            s_bg_timer = 0;
            s_code_state = CodeState::CodeA;
            bg_redraw();
            break;

        case LevelState::Reveal:
        case LevelState::RevealFinished:
        case LevelState::OutsideCode:
        case LevelState::OutsideTalk:
            // TODO: speech
            break;

        case LevelState::Entering:
            s_bg_style = BGStyle::Entering;
            break;
        case LevelState::EnteringGlitchy:
            s_bg_style = BGStyle::EnteringGlitchy;
            break;

        case LevelState::Finished:
            // Logic will finish this off.
            break;
    }
}

} // namespace

void enter() {
    // Draw bits.
    bg_setup();
    ui_setup();

    // Initial state.
    level_advance(LevelState::Start);

    // Show everything now that it's drawn.
    bios_vsync();
    engine::graphics::bitmap_0.enable();
    engine::graphics::bitmap_1.enable();
    engine::graphics::enable_sprites();

    // Kick off the bgm.
    engine::sound::play_bgm(game::music::Bgm::Bgm_Weird);
}

void leave() {
    // Reset graphics state.
    bios_vsync();
    engine::graphics::disable_sprites();
    engine::graphics::bitmap_0.disable();
    engine::graphics::bitmap_1.disable();
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

    if (update_logic()) {
        return Entry::MainMenu; // TODO
    }

    bg_update();
    ui_update();

    engine::profiler::print_timings();
    return next;
}

} // namespace game::cyber
