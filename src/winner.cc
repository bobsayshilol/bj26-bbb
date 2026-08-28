#include "draw_line.h"
#include "font.h"
#include "game.h"
#include "graphics.h"
#include "input.h"
#include "memory.h"
#include "music.h"
#include "profiler.h"
#include "sound.h"

namespace game::winner {

namespace {

PROFILE_STORAGE(vsync);

constexpr uint8_t pal_black = 1;
constexpr uint8_t pal_white = 2;

// font has highest prio.
constexpr uint8_t font_sprite_start = 0;
constexpr uint8_t font_sprite_count = font::font_max_sprites;
constexpr uint8_t font_tile_start = 0;
constexpr uint8_t font_tile_count = font::font_tile_count;

//

constexpr uint16_t printout_start = engine::graphics::SCREEN_WIDTH * engine::graphics::SCREEN_HEIGHT;
constexpr uint16_t printout_width = 256;
constexpr uint16_t printout_height = 224;

//

void bg_setup() {
    using namespace engine::graphics;

    // TODO: just visualisation atm
    bitmap_0.position_x() = 0;
    bitmap_0.position_y() = 0;
    bitmap_0.width() = SCREEN_WIDTH - 1;
    bitmap_0.height() = SCREEN_HEIGHT - 1;
    bitmap_0.scroll_x() = 0;
    bitmap_0.scroll_y() = SCREEN_HEIGHT;

#if 0
    cyber::g_lines_A[0] = { 0x24, 0xAE, 0x7C, 0x28, };
    cyber::g_lines_A[1] = { 0x7C, 0x28, 0xD6, 0xBA, };
    cyber::g_lines_A[2] = { 0xD0, 0x6E, 0x22, 0x6E, };

    cyber::g_lines_M[0] = { 0x28, 0xB8, 0x28, 0x26, };
    cyber::g_lines_M[1] = { 0x28, 0x26, 0x80, 0x6E, };
    cyber::g_lines_M[2] = { 0x80, 0x6E, 0xCA, 0x2A, };
    cyber::g_lines_M[3] = { 0xCA, 0x2A, 0xD0, 0xB6, };

    cyber::g_lines_I[0] = { 0x7C, 0xB6, 0x7C, 0x2A, };

    cyber::g_lines_C[0] = { 0xD0, 0x1E, 0x2A, 0x20, };
    cyber::g_lines_C[1] = { 0x2A, 0x22, 0x2A, 0xBE, };
    cyber::g_lines_C[2] = { 0x2A, 0xBE, 0xD6, 0xB8, };

    cyber::g_lines_U[0] = { 0xD6, 0xB8, 0xD4, 0x1C, };
    cyber::g_lines_U[1] = { 0x20, 0x26, 0x20, 0xBC, };
    cyber::g_lines_U[2] = { 0x20, 0xBC, 0xD4, 0xB4, };

    cyber::g_lines_T[0] = { 0x7E, 0xB4, 0x7E, 0x22, };
    cyber::g_lines_T[1] = { 0xCE, 0x22, 0x24, 0x22, };

    cyber::g_lines_E[0] = { 0x24, 0x22, 0x24, 0xB0, };
    cyber::g_lines_E[1] = { 0x24, 0xB0, 0xD0, 0xB0, };
    cyber::g_lines_E[2] = { 0xD0, 0x6E, 0x26, 0x6A, };
    cyber::g_lines_E[3] = { 0x28, 0x1A, 0xDC, 0x20, };
#endif

    // Clear out the printout state.
    uint8_t * const printout_data = VDP.BITMAP_VRAM_8BIT + printout_start;
    engine::utils::fast_memset8(printout_data, pal_transparent, printout_width * printout_height);

    // Draw the lines
    constexpr uint8_t letters_scale = 4;
    const auto draw_lines = [&](const auto& lines, uint8_t x, uint8_t y) {
        for (const auto & line : lines) {
            engine::utils::draw_line(
                printout_data, printout_width,
                x + line.x0 / letters_scale, y + line.y0 / letters_scale,
                x + line.x1 / letters_scale, y + line.y1 / letters_scale,
                pal_white
            );
        }
    };
    draw_lines(cyber::g_lines_A, printout_width * 1 / (2 * letters_scale), printout_height * 1 / letters_scale);
    draw_lines(cyber::g_lines_M, printout_width * 3 / (2 * letters_scale), printout_height * 1 / letters_scale);
    draw_lines(cyber::g_lines_I, printout_width * 5 / (2 * letters_scale), printout_height * 1 / letters_scale);
    draw_lines(cyber::g_lines_C, printout_width * 0 / (2 * letters_scale), printout_height * 2 / letters_scale);
    draw_lines(cyber::g_lines_U, printout_width * 2 / (2 * letters_scale), printout_height * 2 / letters_scale);
    draw_lines(cyber::g_lines_T, printout_width * 4 / (2 * letters_scale), printout_height * 2 / letters_scale);
    draw_lines(cyber::g_lines_E, printout_width * 6 / (2 * letters_scale), printout_height * 2 / letters_scale);
}

//

void ui_setup() {
    // TODO
}

//

bool logic_update() {
    bool finished = false;

    const auto pressed = engine::input::g_buttons_pressed;
    if (pressed & GAMEPAD_BTN_A) {
        // TODO: needs a warning that it's printing
        //bios_print8bpp(VDP.BITMAP_VRAM_8BIT + printout_start, VDP.PALETTE, 1);
    }

    return finished;
}

} // namespace

void enter() {
    // Setup palette.
    engine::graphics::set_palette_colour(pal_black, RGB555(0,0,0));
    engine::graphics::set_palette_colour(pal_white, RGB555(31,31,31));

    // Setup the parts.
    game::font::setup_tiles<font_tile_start, font_sprite_start>();
    bg_setup();
    ui_setup();

    // This screen uses sprites and has a background.
    bios_vsync();
    engine::graphics::enable_sprites();
    engine::graphics::bitmap_0.enable();

    // Kick off the bgm.
    //engine::sound::play_bgm(game::music::Bgm::???);

    engine::profiler::print_timings();
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
    Entry next = Entry::Winner;

    // Wait for vsync.
    {
        PROFILE_SCOPE(vsync);
        bios_vsync();
    }

    // Update gamepad/mouse input.
    engine::input::update_inputs();

    // Return to the main menu if requested.
    if (engine::input::g_buttons_pressed & GAMEPAD_BTN_START) {
        return Entry::MainMenu;
    }

    if (logic_update()) {
        return Entry::MainMenu;
    }

    engine::profiler::print_timings();
    return next;
}

} // namespace game::winner
