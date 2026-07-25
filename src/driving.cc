#include "debug.h"
#include "fixed.h"
#include "game.h"
#include "graphics.h"
#include "profiler.h"
#include "input.h"

//
// +-----+
// |     | < bitmap2 for skyline
// +-----+
// |     | < bitmap0 for road bending
// |     | < bitmap1 overlapped for center line?
// +-----+
//

namespace game {

namespace {

PROFILE_STORAGE(rd_vsy);
PROFILE_STORAGE(rd_upd);
PROFILE_STORAGE(rd_log);

//

// Road drawing is split into sections.
constexpr uint16_t road_sections = 80;
constexpr uint16_t scanlines_per_section = 2;
constexpr uint16_t road_length = road_sections * scanlines_per_section;
static_assert(road_length < engine::graphics::SCREEN_HEIGHT);
constexpr uint16_t road_start = engine::graphics::SCREEN_HEIGHT - road_length;

// Currently location.
constexpr auto dx_per_frame = engine::utils::FixedS1616::div(1, 32);
engine::utils::FixedS1616 s_xpos;

// How fast we move through the road.
// TODO: shifts
//constexpr uint8_t road_shift = 3; // 8 speed levels
//constexpr uint8_t road_max_speed = 1 << road_shift;
uint8_t s_road_position;
uint8_t s_road_speed = 1;

// Current road rotation.
engine::utils::FixedS1616 s_road_rotation;

// Incoming road layouts.
// TODO: just use an RNG
static const int8_t s_road_offsets[256] = {
    1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0,
    1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0,
    1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0,
    1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    -1, 0, -1, 0, -1, 0, -1, 0,
    -1, 0, -1, 0, -1, 0, -1, 0,
    -1, 0, -1, 0, -1, 0, -1, 0,
    -1, 0, -1, 0, -1, 0, -1, 0,
};

// Generated with `for i in range(80): print(int(math.pow(1.06, 79-i)-1), end=',')`
static const uint8_t s_turning_offset[road_sections] = {
    98,93,87,82,78,73,69,65,61,58,54,51,48,45,43,40,38,36,33,31,30,28,26,25,23,22,20,19,18,17,16,15,14,13,12,11,11,10,9,9,8,8,7,7,6,6,5,5,5,4,4,4,3,3,3,3,2,2,2,2,2,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0
};

//

// We need explicit barriers here otherwise GCC will assume that nothing has changed.
void wait_until_line0() { while (VDP.VCOUNT != 0) __asm__ volatile ("":::"memory"); }
void wait_until_line(uint16_t line) { while (VDP.VCOUNT < line) __asm__ volatile ("":::"memory"); }

//

void draw_bitmaps() {
    using namespace engine::graphics;

    // Skyline bitmap.
    bitmap_2.position_x() = 0;
    bitmap_2.position_y() = 0;
    bitmap_2.width() = SCREEN_WIDTH - 1;
    bitmap_2.height() = road_start - 1;

    // TODO: draw the skyline

    // Road bitmap (static).
    bitmap_0.position_x() = 0;
    bitmap_0.position_y() = road_start;
    bitmap_0.width() = SCREEN_WIDTH - 1;
    bitmap_0.height() = road_length - 1;

    // Scroll the bitmap up to match where it is.
    bitmap_0.scroll_y() = road_start;

    // Setup colours.
    constexpr uint8_t pal_black = 1;
    constexpr uint8_t pal_white = 2;
    constexpr uint8_t pal_grey = 3;
    set_palette_colour(pal_black, RGB555(0, 0, 0));
    set_palette_colour(pal_white, RGB555(31, 31, 31));
    set_palette_colour(pal_grey, RGB555(15, 15, 15));

    // Draw the road.
    constexpr int32_t min_pavement_width = -SCREEN_WIDTH / 8; // outside the screen
    constexpr int32_t max_pavement_width = SCREEN_WIDTH / 2 - SCREEN_WIDTH / 16;
    for (int32_t y = road_start; y < SCREEN_HEIGHT; y++) {
        const int32_t pavement_width = max_pavement_width - (max_pavement_width - min_pavement_width) * (y - road_start) / (SCREEN_HEIGHT - 16 - road_start);
        const int32_t stripes_width = (SCREEN_WIDTH / 2 - pavement_width) / 8;
        for (int32_t x = 0; x < 256; x++) {
            uint8_t col = pal_black;
            if (x < pavement_width || x > 256 - pavement_width) col = pal_grey;
            if (128 - stripes_width <= x && x <= 128 + stripes_width && (y & 31)) col = pal_white;
            VDP.BITMAP_VRAM_8BIT[y * 256 + x] = col;
        }
    }
}

//

void update_logic() {
    PROFILE_SCOPE(rd_log);

    // Handle input.
    if (engine::input::g_buttons_held & GAMEPAD_BTN_LEFT) { s_xpos -= dx_per_frame; }
    else if (engine::input::g_buttons_held & GAMEPAD_BTN_RIGHT) { s_xpos += dx_per_frame; }

    // TODO: proper speed handling
    //if ((engine::input::g_buttons_pressed & GAMEPAD_BTN_UP) && s_road_speed < road_max_speed) { s_road_speed += 1; }
    //else if ((engine::input::g_buttons_pressed & GAMEPAD_BTN_DOWN) && s_road_speed > 0) { s_road_speed -= 1; }

    // Move along the road.
    s_road_position += s_road_speed;
    s_road_rotation += engine::utils::FixedS1616::div(s_road_offsets[s_road_position], 32); // TODO: move to array

    // Clamp position.
    if (s_xpos.value() >= 1) {
        s_xpos = engine::utils::FixedS1616::from(1);
    } else if (s_xpos.value() < -1) { // TODO: this should be <=, but off-by-one with -ve (see header)
        s_xpos = engine::utils::FixedS1616::from(-1);
    }

    //

    // For now just wait a few lines.
    wait_until_line(road_start - 10);
}

void draw_road() {
    PROFILE_SCOPE(rd_upd);

    // Store to a local since wait_until_line() issues a barrier.
    const auto xpos = s_xpos;
    const auto road_rot = s_road_rotation;

    // Wait for the next section and scroll the scanline.
    uint8_t ridx = s_road_position;
    uint8_t road_section = 0;
    for (uint16_t line = road_start; line < engine::graphics::SCREEN_HEIGHT; line += scanlines_per_section, ridx++, road_section++) {
        wait_until_line(line);

        int8_t dx = 0;

        // Road curvature.
        dx += (road_rot * s_turning_offset[road_section]).value();

        // Translation of car.
        dx += (xpos * road_section).value();

        engine::graphics::bitmap_0.scroll_x() = dx;
    }
}

//

void enter() {
    // Setup colours.
    engine::graphics::set_backdrop_a(RGB555(0, 0, 31));

    // Setup sprites.
    // TODO

    // Draw the parts of the screen.
    draw_bitmaps();

    // Show everything now that it's drawn.
    engine::graphics::bitmap_0.enable();
    engine::graphics::bitmap_2.enable();
    engine::graphics::enable_sprites();

    engine::profiler::print_timings();
}

void leave() {
    // Reset graphics state.
    engine::graphics::disable_sprites();
    engine::graphics::bitmap_0.disable();
    engine::graphics::bitmap_2.disable();
    engine::graphics::reset_sprites(0); // TODO
}

} // namespace

Entry driving_loop() {
    enter();

    Entry next = Entry::MainMenu;
    while (true) {
        // Reset scroll for the skyline.
        // TODO: this should scroll too.
        engine::graphics::bitmap_0.scroll_x() = 0;

        // Wait for vblank to end.
        {
            PROFILE_SCOPE(rd_vsy);
            wait_until_line0();
        }

        // Update gamepad/mouse input.
        engine::input::update_inputs();

        // TODO: just for testing atm
        const auto pressed = engine::input::g_buttons_pressed;
        if (pressed & GAMEPAD_BTN_A) {
            next = Entry::MainMenu;
            break;
        }

        // Print the last frame's timings before updating logic since we don't seem to have enough time after.
#if 0 // This still causes corruption!
        engine::profiler::print_timings();
#endif

        // We have a bit of breathing room before we need to draw the road.
        update_logic();

        // Draw the road.
        draw_road();

        // TODO: there should be breathing room after too, ie during vblank?
    }

    // Reset timings for the next entry.
    engine::profiler::print_timings();

    leave();
    return next;
}

} // namespace game
