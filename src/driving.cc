#include "debug.h"
#include "fixed.h"
#include "game.h"
#include "graphics.h"
#include "memory.h"
#include "music.h"
#include "profiler.h"
#include "input.h"
#include "sound.h"

//
// +--+--+
// |  |  | < bitmap1+bitmap2 for skyline
// +--+--+
// |     | < bitmap0 for road bending
// +-----+
//

#define PRINT_PROFILING 0 // This causes UI corruption when enabled
#define SPRITES_FOR_ROAD 0 // This seems like it'd be more hassle to shift and requires lots of sprites if not multiplexing

namespace game {

namespace {

PROFILE_STORAGE(rd_vsy);
PROFILE_STORAGE(rd_upd);
PROFILE_STORAGE(rd_til);
PROFILE_STORAGE(rd_log);

//

constexpr uint8_t pal_black = 1;
constexpr uint8_t pal_white = 2;
constexpr uint8_t pal_grey = 3;
constexpr uint8_t pal_car_start = 4; // shared with trees
constexpr uint8_t pal_car_count = 16;

constexpr uint8_t car_sprite_start = 0;
constexpr uint8_t car_sprite_count = 1;
constexpr uint8_t car_tile_start = 0;
constexpr uint8_t car_tile_count = 1; // TODO: more for turning

constexpr uint8_t tree_sprite_start = car_sprite_start + car_sprite_count;
constexpr uint8_t tree_sprite_count = 2; // one on each side
constexpr uint8_t tree_tile_start = car_tile_start + car_tile_count;
constexpr uint8_t tree_tile_count = 3; // far/med/close -> tiny/small/big

constexpr uint8_t transparent_sprite_start = tree_sprite_start + tree_sprite_count;
constexpr uint8_t transparent_sprite_count = 3; // max 3 road splits
constexpr uint8_t transparent_tile_start = tree_tile_start + tree_tile_count;
constexpr uint8_t transparent_tile_count = 1;

//

// Road drawing is split into sections.
constexpr uint16_t road_sections = 80;
constexpr uint16_t scanlines_per_section = 2;
constexpr uint16_t road_length = road_sections * scanlines_per_section;
static_assert(road_length < engine::graphics::SCREEN_HEIGHT);
constexpr uint16_t road_start = engine::graphics::SCREEN_HEIGHT - road_length;

uint8_t s_center_line_widths[road_length]; // built during setup

// Currently location.
constexpr auto dx_per_frame = engine::utils::FixedS1616::div(1, 32);
engine::utils::FixedS1616 s_xpos; // clamped to [-1, 1]

// How fast we move through the road.
constexpr uint8_t min_road_speed = 1;
engine::utils::FixedS1616 s_road_position; // clamped to [0, 256]
engine::utils::FixedS1616 s_road_speed = engine::utils::FixedS1616::from(min_road_speed);

uint8_t get_current_max_speed() {
    static constexpr uint8_t max_road_speeds[8+1] = {
        1, 2, 3, 4, 4, 3, 2, 1,
        1, // extra in case of xpos=1
    };
    const int16_t idx = 4 + (s_xpos * 4).value(); // 4 + [-4, 4]
    ASSERT(idx >= 0);
    ASSERT(idx <= 8);
    return max_road_speeds[idx];
}

constexpr auto accel_per_frame = engine::utils::FixedS1616::div(2, 60); // 2 units per frame
constexpr auto drag_per_frame = engine::utils::FixedS1616::div(1, 60); // 1 unit per frame

// Current road rotation.
engine::utils::FixedS1616 s_road_rotation;

// Incoming road layouts.
// TODO: just use an RNG
static constexpr engine::utils::FixedS1616 s_road_offsets[256] = {
#define F(x) engine::utils::FixedS1616::div(x, 32)
    F(1), F(0), F(0), F(1), F(0), F(0), F(1), F(0), F(0), F(1), F(0), F(0),
    F(1), F(0), F(0), F(1), F(0), F(0), F(1), F(0), F(0), F(1), F(0), F(0),
    F(1), F(0), F(0), F(1), F(0), F(0), F(1), F(0), F(0), F(1), F(0), F(0),
    F(1), F(0), F(0), F(1), F(0), F(0), F(1), F(0), F(0), F(1), F(0), F(0),
    F(0), F(0), F(0), F(0), F(0), F(0), F(0), F(0),
    F(0), F(0), F(0), F(0), F(0), F(0), F(0), F(0),
    F(0), F(0), F(0), F(0), F(0), F(0), F(0), F(0),
    F(0), F(0), F(0), F(0), F(0), F(0), F(0), F(0),
    F(-1), F(0), F(-1), F(0), F(-1), F(0), F(-1), F(0),
    F(-1), F(0), F(-1), F(0), F(-1), F(0), F(-1), F(0),
    F(-1), F(0), F(-1), F(0), F(-1), F(0), F(-1), F(0),
    F(-1), F(0), F(-1), F(0), F(-1), F(0), F(-1), F(0),

    // Silence compiler warning...
    F(0),F(0),F(0),F(0),F(0),F(0),F(0),F(0),F(0),F(0),F(0),F(0),F(0),F(0),F(0),F(0),
    F(0),F(0),F(0),F(0),F(0),F(0),F(0),F(0),F(0),F(0),F(0),F(0),F(0),F(0),F(0),F(0),
    F(0),F(0),F(0),F(0),F(0),F(0),F(0),F(0),F(0),F(0),F(0),F(0),F(0),F(0),F(0),F(0),
    F(0),F(0),F(0),F(0),F(0),F(0),F(0),F(0),F(0),F(0),F(0),F(0),F(0),F(0),F(0),F(0),
    F(0),F(0),F(0),F(0),F(0),F(0),F(0),F(0),F(0),F(0),F(0),F(0),F(0),F(0),F(0),F(0),
    F(0),F(0),F(0),F(0),F(0),F(0),F(0),F(0),F(0),F(0),F(0),F(0),F(0),F(0),F(0),F(0),
    F(0),F(0),F(0),F(0),F(0),F(0),F(0),F(0),F(0),F(0),F(0),F(0),F(0),F(0),F(0),F(0),
    F(0),F(0),F(0),F(0),F(0),F(0),F(0),F(0),F(0),F(0),F(0),F(0),F(0),F(0),F(0),F(0),
    F(0),F(0),F(0),F(0),F(0),F(0),F(0),F(0),F(0),F(0),F(0),F(0),F(0),F(0),F(0),F(0),
#undef F
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

void setup_tiles() {
    using namespace engine::graphics;

    // Car tiles.
    // TODO: replace with real tiles
    static_assert(pal_car_count >= car_tile_count);
    for (int idx = 0; idx < car_tile_count; idx++) {
        engine::graphics::set_palette_colour(pal_car_start + idx, RGB555(31, idx+2, idx+2));
        engine::utils::fast_memset8(get_tile_data(car_tile_start + idx), pal_car_start + idx, bg_tile_size * bg_tile_size);
    }

    // Tree tiles.
    // TODO: replace with real tiles
    static_assert(pal_car_count >= car_tile_count + tree_tile_count);
    for (int idx = 0; idx < tree_tile_count; idx++) {
        const auto pal_idx = car_tile_count + idx + 1;
        engine::graphics::set_palette_colour(pal_car_start + pal_idx, RGB555(pal_idx+2, 31, pal_idx+2));
        engine::utils::fast_memset8(get_tile_data(tree_tile_start + idx), pal_car_start + pal_idx, bg_tile_size * bg_tile_size);
    }

#if SPRITES_FOR_ROAD
    // Transparent line split tiles.
    for (int idx = 0; idx < transparent_sprite_count; idx++) {
        Pixel2 * tile_data = get_tile_data(transparent_sprite_start + idx);
        // First line is road colour.
        engine::utils::fast_memset8(tile_data, pal_black, bg_tile_size);
        // The rest is transparent.
        engine::utils::fast_memset8((char*)tile_data + bg_tile_size, pal_transparent, bg_tile_size * (bg_tile_size - 1));
    }
#endif
}

void setup_bitmaps() {
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
    set_palette_colour(pal_black, RGB555(0, 0, 0));
    set_palette_colour(pal_white, RGB555(31, 31, 31));
    set_palette_colour(pal_grey, RGB555(15, 15, 15));

    // Draw the road.
    constexpr int32_t min_pavement_width = -SCREEN_WIDTH / 8; // outside the screen
    constexpr int32_t max_pavement_width = SCREEN_WIDTH / 2 - SCREEN_WIDTH / 16;
    for (int32_t y = road_start; y < SCREEN_HEIGHT; y++) {
        const int32_t pavement_width = max_pavement_width - (max_pavement_width - min_pavement_width) * (y - road_start) / (SCREEN_HEIGHT - 16 - road_start);
        const int32_t stripes_width = (SCREEN_WIDTH / 2 - pavement_width) / 8;
        s_center_line_widths[y - road_start] = stripes_width & ~1; // must be even for fast memset later
        for (int32_t x = 0; x < 256; x++) {
            uint8_t col = pal_black;
            if (x < pavement_width || x > 256 - pavement_width) col = pal_grey;
            if (128 - stripes_width <= x && x < 128 + stripes_width) col = pal_white;
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

    // Clamp position.
    if (s_xpos.value() >= 1) {
        s_xpos = engine::utils::FixedS1616::from(1);
    } else if (s_xpos.value() < -1) { // TODO: this should be <=, but off-by-one with -ve (see header)
        s_xpos = engine::utils::FixedS1616::from(-1);
    }

    //

    // Speed.
    const uint16_t max_road_speed = get_current_max_speed();
    if ((engine::input::g_buttons_held & GAMEPAD_BTN_UP) && s_road_speed.value() < max_road_speed) { s_road_speed += accel_per_frame; }
    else if ((engine::input::g_buttons_held & GAMEPAD_BTN_DOWN)) { s_road_speed -= accel_per_frame; } // clamp happens after drag

    // Apply drag.
    s_road_speed -= drag_per_frame;
    if (s_road_speed.value() < min_road_speed) {
        s_road_speed = engine::utils::FixedS1616::from(min_road_speed);
    }

    //

    // Move along the road.
    s_road_position += s_road_speed;
    if (s_road_position.value() >= 256) {
        s_road_position -= engine::utils::FixedS1616::from(256); // emulate wrapping like uint8_t
    }
    ASSERT(s_road_position.value() >= 0);
    ASSERT(s_road_position.value() <= 255);
    s_road_rotation += s_road_offsets[s_road_position.value()];
}

void draw_sprites() {
    using namespace engine::graphics;

    PROFILE_SCOPE(rd_til);

    constexpr uint16_t car_x_scale = 32;
    constexpr uint16_t car_y_offset = 40;

    // Car sprite.
    ObjSprite sprite;
    const auto car_x = s_xpos * car_x_scale;
    sprite.set_y(SCREEN_HEIGHT - car_y_offset);
    for (int idx = 0; idx < car_sprite_count; idx++) {
        sprite.set_x(SCREEN_WIDTH / 2 + car_x.value());
        sprite.set_tile_index(car_tile_start + idx);
        set_sprite(car_sprite_start + idx, sprite);
    }

    // Tree sprites.
    // TODO

#if SPRITES_FOR_ROAD
    // Transparent line split tiles.
    sprite.set_tile_index(transparent_tile_start); // single tile
    constexpr int split_every = 64;
    static_assert(SCREEN_HEIGHT - road_start <= transparent_sprite_count * split_every);
    uint8_t split_idx = 0;
    const uint8_t offset = s_road_position & (split_every - 1);
    for (int line = road_start + offset; line <= SCREEN_HEIGHT; line += split_every, split_idx++) {
        sprite.set_x(SCREEN_WIDTH / 2);
        sprite.set_y(line);
        set_sprite(transparent_sprite_start + split_idx, sprite);
    }
#endif
}

void draw_road() {
    using namespace engine::graphics;

    PROFILE_SCOPE(rd_upd);

#if !SPRITES_FOR_ROAD
    // Store the last splits so we can remove them next frame.
    constexpr uint16_t max_num_splits = 6;
    static uint16_t s_num_last_splits = 0;
    static uint16_t s_last_splits[max_num_splits];

    // Clear out old splits.
    const int num_last_splits = s_num_last_splits;
    for (uint16_t i = 0; i < num_last_splits; i++) {
        const uint16_t line = s_last_splits[i];
        const uint8_t split_width = s_center_line_widths[line - road_start];
        uint8_t * line_data = VDP.BITMAP_VRAM_8BIT + line * SCREEN_WIDTH + SCREEN_WIDTH / 2 - split_width;
        engine::utils::fast_memset8(line_data, pal_white, split_width * 2);
    }

    // Build this frame's splits.
    s_num_last_splits = 0;
    uint16_t * last_splits = s_last_splits;
#endif

    // Store to a local since wait_until_line() issues a barrier.
    const auto xpos = s_xpos;
    const auto road_rot = s_road_rotation;

    // Wait for the next section and scroll the scanline.
    uint8_t road_pos = s_road_position.value();
    uint8_t road_section = 0;
    for (uint16_t line = road_start; line < SCREEN_HEIGHT; line += scanlines_per_section, road_pos++, road_section++) {
        wait_until_line(line);

        int8_t dx = 0;

        // Road curvature.
        dx += (road_rot * s_turning_offset[road_section]).value();

        // Translation of car.
        dx += (xpos * road_section).value();

        bitmap_0.scroll_x() = dx;

#if !SPRITES_FOR_ROAD
        constexpr uint8_t split_every = 32;
        static_assert(max_num_splits > road_length / split_every);

        // See if this is a line with a stripe.
        constexpr uint8_t split_mask = split_every - 1;
        static_assert((split_every & split_mask) == 0);
        if ((line & split_mask) == (road_pos & split_mask)) {
            // Add a split to the current line
            const uint8_t split_width = s_center_line_widths[line - road_start];
            uint8_t * line_data = VDP.BITMAP_VRAM_8BIT + line * SCREEN_WIDTH + SCREEN_WIDTH / 2 - split_width;
            engine::utils::fast_memset8(line_data, pal_black, split_width * 2);

            // Add it to the list.
            *last_splits++ = line;
            s_num_last_splits++;
        }
#endif
    }

    // Wait for the final line to pass then reset scroll for the skyline.
    wait_until_line(SCREEN_HEIGHT);
    bitmap_0.scroll_x() = 0;
}

//

void enter() {
    // Setup colours.
    engine::graphics::set_backdrop_a(RGB555(0, 0, 31));

    // Draw the parts of the screen.
    setup_tiles();
    setup_bitmaps();

    // Show everything now that it's drawn.
    bios_vsync();
    engine::graphics::bitmap_0.enable();
    engine::graphics::bitmap_2.enable();
    engine::graphics::enable_sprites();

    // Kick off the bgm.
    engine::sound::play_bgm(game::music::Bgm::Bgm_Driving);

    engine::profiler::print_timings();
}

void leave() {
    // Reset graphics state.
    bios_vsync();
    engine::graphics::disable_sprites();
    engine::graphics::bitmap_0.disable();
    engine::graphics::bitmap_2.disable();
    engine::graphics::reset_sprites(transparent_sprite_start + transparent_sprite_count);

    // Reset sound.
    engine::sound::stop_bgm();
}

} // namespace

Entry driving_loop() {
    enter();

    Entry next = Entry::MainMenu;
    while (true) {
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
#if PRINT_PROFILING
        engine::profiler::print_timings();
#endif

        // We have a bit of breathing room before we need to draw the road.
        update_logic();
        draw_sprites();

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
