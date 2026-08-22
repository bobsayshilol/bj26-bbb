#include "debug.h"
#include "fixed.h"
#include "game.h"
#include "graphics.h"
#include "memory.h"
#include "music.h"
#include "profiler.h"
#include "input.h"
#include "sound.h"
#include "images.h"
#include "font.h"

//
// +--+--+
// |     | < bitmap_2 for skyline
// +--+--+
// |     | < bitmap_1 slowly comes up for the dome
// +--+--+
// |     | < bitmap_0 for road bending
// +-----+
//

#define PRINT_PROFILING 0 // This causes UI corruption when enabled
#define SPRITES_FOR_ROAD 0 // This seems like it'd be more hassle to shift and requires lots of sprites if not multiplexing

namespace game::driving {

namespace {

PROFILE_STORAGE(rd_vsy);
PROFILE_STORAGE(rd_upd);
PROFILE_STORAGE(rd_til);
PROFILE_STORAGE(rd_log);
PROFILE_STORAGE(ui_upd);

//

constexpr uint8_t pal_black = 1;
constexpr uint8_t pal_white = 2;
constexpr uint8_t pal_grey = 3;

// font has highest prio.
constexpr uint8_t font_sprite_start = 0;
constexpr uint8_t font_sprite_count = font::font_max_sprites;
constexpr uint8_t font_tile_start = 0;
constexpr uint8_t font_tile_count = font::font_tile_count;

constexpr uint8_t car_pal_start = 4;
constexpr uint8_t car_pal_count = 16;
constexpr uint8_t car_sprite_start = font_sprite_start + font_sprite_count;
constexpr uint8_t car_sprite_count = 4;
constexpr uint8_t car_tile_start = font_tile_start + font_tile_count;
constexpr uint8_t car_tile_count = car_sprite_count * 2; // one for forwards, one for left/right (flip)

// There's free palette space here (20-29).

// Bit awkward, but bucko_left must be at a fixed offset (from breakout).
static_assert(car_pal_start + car_pal_count <= images::bucko_left::pal_offset);
constexpr uint8_t voice_char_width = 3;
constexpr uint8_t voice_char_height = 4;
constexpr uint8_t bucko_left_pal_start = images::bucko_left::pal_offset;
constexpr uint8_t bucko_left_pal_count = 16;
constexpr uint8_t bucko_left_sprite_start = car_sprite_start + car_sprite_count;
constexpr uint8_t bucko_left_sprite_count = voice_char_width * voice_char_height;
constexpr uint8_t bucko_left_tile_start = car_tile_start + car_tile_count;
constexpr uint8_t bucko_left_tile_count = bucko_left_sprite_count;

constexpr uint8_t ami_left_pal_start = bucko_left_pal_start + bucko_left_pal_count;
constexpr uint8_t ami_left_pal_count = 16;
constexpr uint8_t ami_left_sprite_start = bucko_left_sprite_start; // reuse the bucko sprites
constexpr uint8_t ami_left_sprite_count = bucko_left_sprite_count;
constexpr uint8_t ami_left_tile_start = bucko_left_tile_start + bucko_left_tile_count;
constexpr uint8_t ami_left_tile_count = ami_left_sprite_count;

constexpr uint8_t tree_pal_start = ami_left_pal_start + ami_left_pal_count;
constexpr uint8_t tree_pal_count = 16;
constexpr uint8_t tree_sprite_start = ami_left_sprite_start + ami_left_sprite_count;
constexpr uint8_t tree_sprite_count = 2; // one on each side
constexpr uint8_t tree_tile_start = ami_left_tile_start + ami_left_tile_count;
constexpr uint8_t tree_tile_count = 3; // far/med/close -> tiny/small/big

constexpr uint8_t ufo_pal_start = tree_pal_start + tree_pal_count;
constexpr uint8_t ufo_pal_count = 16;
constexpr uint8_t ufo_sprite_start = tree_sprite_start + tree_sprite_count;
constexpr uint8_t ufo_sprite_count = 4;
constexpr uint8_t ufo_tile_start = tree_tile_start + tree_tile_count;
constexpr uint8_t ufo_tile_count = 4;

// Reuses the basic palette above.
constexpr uint8_t transparent_sprite_start = ufo_sprite_start + ufo_sprite_count;
constexpr uint8_t transparent_sprite_count = SPRITES_FOR_ROAD ? 3 : 0; // max 3 road splits
constexpr uint8_t transparent_tile_start = ufo_tile_start + ufo_tile_count;
constexpr uint8_t transparent_tile_count = SPRITES_FOR_ROAD ? 1 : 0;

constexpr uint8_t skyline_pal_start = ufo_pal_start + ufo_pal_count;
constexpr uint8_t skyline_pal_count = 16;
constexpr uint8_t dome_pal_start = skyline_pal_start + skyline_pal_count;
constexpr uint8_t dome_pal_count = 16;

//

// Road drawing is split into sections.
constexpr uint16_t road_sections = 80;
constexpr uint16_t scanlines_per_section = 2;
constexpr uint16_t road_length = road_sections * scanlines_per_section;
static_assert(road_length < engine::graphics::SCREEN_HEIGHT);
constexpr uint16_t road_start = engine::graphics::SCREEN_HEIGHT - road_length;

uint8_t s_center_line_widths[road_length]; // built during setup
int16_t s_pavement_line_start[road_length];

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

enum class LevelState {
    Intro,
    Bombs,
    GameOver,
} s_level_state;

void level_advance(LevelState state);

//

// We need explicit barriers here otherwise GCC will assume that nothing has changed.
void wait_until_line0() { while (VDP.VCOUNT != 0) __asm__ volatile ("":::"memory"); }
void wait_until_line(uint16_t line) { while (VDP.VCOUNT < line) __asm__ volatile ("":::"memory"); }

//

void setup_tiles() {
    using namespace engine::graphics;

    // Car tiles.
    images::copy_tile_data<
        car_pal_start, car_pal_count,
        car_tile_start, car_tile_count,
        images::car
    >();

    // Tree tiles.
    images::copy_tile_data<
        tree_pal_start, tree_pal_count,
        tree_tile_start, tree_tile_count,
        images::tree
    >();

    // UFO tiles.
    images::copy_tile_data<
        ufo_pal_start, ufo_pal_count,
        ufo_tile_start, ufo_tile_count,
        images::ufo
    >();

    // Characters.
    images::copy_tile_data<
        bucko_left_pal_start, bucko_left_pal_count,
        bucko_left_tile_start, bucko_left_tile_count,
        images::bucko_left
    >();
    images::copy_tile_data<
        ami_left_pal_start, ami_left_pal_count,
        ami_left_tile_start, ami_left_tile_count,
        images::ami_left
    >();

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
        s_pavement_line_start[y - road_start] = pavement_width;
        for (int32_t x = 0; x < 256; x++) {
            uint8_t col = pal_black;
            if (x < pavement_width || x > 256 - pavement_width) col = pal_grey;
            if (128 - stripes_width <= x && x < 128 + stripes_width) col = pal_white;
            VDP.BITMAP_VRAM_8BIT[y * 256 + x] = col;
        }
    }

    // Skyline bitmap.
    // TODO: this could be done programatically to save 16k space
    {
        // First road_start lines are for the skyline.
        constexpr uint16_t skyline_start_y = 0;
        constexpr uint16_t skyline_width = SCREEN_WIDTH;
        constexpr uint16_t skyline_height = road_start;

        bitmap_2.position_x() = 0;
        bitmap_2.position_y() = 0;
        bitmap_2.width() = skyline_width - 1;
        bitmap_2.height() = skyline_height - 1;
        bitmap_2.scroll_x() = 0;
        bitmap_2.scroll_y() = skyline_start_y;

        // Setup palette.
        static_assert(engine::utils::size(images::skyline_raw::palette) == skyline_pal_count);
        static_assert(images::skyline_raw::pal_offset == skyline_pal_start);
        for (uint8_t i = 0; i < skyline_pal_count; i++) {
            set_palette_colour(skyline_pal_start + i, images::skyline_raw::palette[i]);
        }

        // Copy data.
        static_assert(engine::utils::size(images::skyline_raw::data) == skyline_width * skyline_height);
        uint8_t * data = VDP.BITMAP_VRAM_8BIT + skyline_start_y * SCREEN_WIDTH;
        engine::utils::fast_memcpy(data, images::skyline_raw::data, skyline_width * skyline_height);
    }

    // Dome bitmap.
    // TODO: compress (even RLE) would save huge space here
    {
        // Dome comes after the road.
        constexpr uint16_t dome_start_y = road_start + road_length;
        constexpr uint16_t dome_width = SCREEN_WIDTH;
        constexpr uint16_t dome_height = road_start;

        bitmap_1.position_x() = 0;
        bitmap_1.position_y() = road_start;
        bitmap_1.width() = dome_width - 1;
        bitmap_1.height() = 0; // we'll grow this when it appears
        bitmap_1.scroll_x() = 0;
        bitmap_1.scroll_y() = dome_start_y;

        // Setup palette.
        static_assert(engine::utils::size(images::dome_raw::palette) == dome_pal_count);
        static_assert(images::dome_raw::pal_offset == dome_pal_start);
        for (uint8_t i = 0; i < dome_pal_count; i++) {
            set_palette_colour(dome_pal_start + i, images::dome_raw::palette[i]);
        }

        // Copy data.
        static_assert(engine::utils::size(images::dome_raw::data) == dome_width * dome_height);
        uint8_t * data = VDP.BITMAP_VRAM_8BIT + dome_start_y * SCREEN_WIDTH;
        engine::utils::fast_memcpy(data, images::dome_raw::data, dome_width * dome_height);
    }
}

//

void update_logic() {
    PROFILE_SCOPE(rd_log);

    const uint16_t held = engine::input::g_buttons_held;

    // Handle input.
    if (held & GAMEPAD_BTN_LEFT) { s_xpos -= dx_per_frame; }
    else if (held & GAMEPAD_BTN_RIGHT) { s_xpos += dx_per_frame; }

    // Clamp position.
    if (s_xpos.value() >= 1) {
        s_xpos = engine::utils::FixedS1616::from(1);
    } else if (s_xpos.value() < -1) { // TODO: this should be <=, but off-by-one with -ve (see header)
        s_xpos = engine::utils::FixedS1616::from(-1);
    }

    //

    // Speed.
    const uint16_t max_road_speed = get_current_max_speed();
    if ((held & GAMEPAD_BTN_UP) && s_road_speed.value() < max_road_speed) { s_road_speed += accel_per_frame; }
    else if ((held & GAMEPAD_BTN_DOWN)) { s_road_speed -= accel_per_frame; } // clamp happens after drag

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

    {
        static_assert(car_sprite_count == 4);

        constexpr uint16_t car_x_scale = 32;
        constexpr uint16_t car_y = SCREEN_HEIGHT - engine::graphics::bg_tile_size - 40;

        const uint16_t held = engine::input::g_buttons_held;
        const uint8_t is_turning = (held & (GAMEPAD_BTN_LEFT | GAMEPAD_BTN_RIGHT)) ? 4 : 0;
        const bool x_flipped = held & GAMEPAD_BTN_LEFT;

        // Car sprite.
        const auto car_x = (s_xpos * car_x_scale).value() + SCREEN_WIDTH / 2 - engine::graphics::bg_tile_size;
        for (uint8_t idx = 0; idx < car_sprite_count; idx++) {
            ObjSprite sprite;
            const uint16_t dx = ((idx & 1) ^ x_flipped) ? engine::graphics::bg_tile_size : 0;
            const uint16_t dy = (idx & 2) ? engine::graphics::bg_tile_size : 0;
            sprite.set_x(car_x + dx);
            sprite.set_y(car_y + dy);
            sprite.set_tile_index(car_tile_start + idx + is_turning);
            sprite.set_x_flip(x_flipped);
            set_sprite(car_sprite_start + idx, sprite);
        }
    }

    // Tree sprites.
    {
        bool hide_trees = true;

        const uint8_t road_pos = s_road_position.value();
        if (road_pos > road_start) {
            // Work out how far down the road we are.
            const uint8_t road_y = road_pos - road_start;
            // TODO: need to account for road curvature
            const int16_t left_x = s_pavement_line_start[road_y] - 4; // magic number

            if (left_x >= 0) {
                // Map that to the size to use.
                static_assert(tree_tile_count == 3);
                uint8_t size_idx = 0;
                if (road_y > road_length / 2) {
                    size_idx = 2;
                } else if (road_y > road_length / 6) {
                    size_idx = 1;
                }

                // Draw one on each side.
                static_assert(tree_sprite_count == 2);
                ObjSprite sprite;
                sprite.set_x(SCREEN_WIDTH - left_x);
                sprite.set_y(road_pos);
                sprite.set_tile_index(tree_tile_start + size_idx);
                sprite.set_x_flip(false);
                set_sprite(tree_sprite_start + 0, sprite);

                sprite.set_x_flip(true);
                sprite.set_x(left_x);
                set_sprite(tree_sprite_start + 1, sprite);

                hide_trees = false;
            }
        }

        if (hide_trees) {
            static_assert(tree_sprite_count == 2);
            ObjSprite sprite;
            for (uint8_t i = 0; i < tree_sprite_count; i++) {
                set_sprite(tree_sprite_start + i, sprite);
            }
        }
    }

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

enum class UIC : uint8_t { None, Ami, Bucko, Robucko, };
void ui_character(UIC voice) {
    using namespace engine::graphics;

    // All characters have the same sprites.
    static_assert(ami_left_sprite_start == bucko_left_sprite_start);
    constexpr uint8_t sprite_start = ami_left_sprite_start;

    constexpr uint8_t left_start_x = 0;
    constexpr uint8_t left_start_y = SCREEN_HEIGHT - bg_tile_size * voice_char_height;
    constexpr uint8_t right_start_x = SCREEN_WIDTH - bg_tile_size * voice_char_width;
    constexpr uint8_t right_start_y = left_start_y;

    ObjSprite sprite;
    uint8_t sprite_idx = 0;
    switch (voice) {
        case UIC::None:
        case UIC::Robucko:
            // No voice.
            for (int y = 0; y < voice_char_height; y++) {
                for (int x = 0; x < voice_char_width; x++) {
                    set_sprite(sprite_start + sprite_idx, sprite);
                    sprite_idx++;
                }
            }
            break;
        case UIC::Ami:
            for (int y = 0; y < voice_char_height; y++) {
                for (int x = 0; x < voice_char_width; x++) {
                    sprite.set_x(left_start_x + x * bg_tile_size);
                    sprite.set_y(left_start_y + y * bg_tile_size);
                    sprite.set_tile_index(ami_left_tile_start + sprite_idx);
                    set_sprite(sprite_start + sprite_idx, sprite);
                    sprite_idx++;
                }
            }
            break;
        case UIC::Bucko:
            // Tiles need a flip, and the x co-ord too.
            for (int y = 0; y < voice_char_height; y++) {
                for (int x = voice_char_width - 1; x >= 0; x--) {
                    sprite.set_x(right_start_x + x * bg_tile_size);
                    sprite.set_y(right_start_y + y * bg_tile_size);
                    sprite.set_tile_index(bucko_left_tile_start + sprite_idx);
                    sprite.set_x_flip(true);
                    set_sprite(sprite_start + sprite_idx, sprite);
                    sprite_idx++;
                }
            }
            break;
    }
}

struct Speech {
    UIC uic;
    uint8_t len;
    const char * text;

    template <uint8_t N>
    constexpr Speech(UIC c, const char (&str)[N]) : uic(c), len(N), text(str) {}
    constexpr Speech(decltype(nullptr)) : uic(UIC::None), len(0), text(nullptr) {}
};

constexpr Speech intro_text[] {
    { UIC::Bucko, "test" },
    { UIC::Bucko, "test2" },
    { UIC::Ami, "words" },
    { UIC::Robucko, "sky" },
    nullptr
};

const Speech * s_current_speech;
LevelState s_next_state;

void ui_redraw() {
    // Empty it out.
    font::clear_text();

    constexpr uint8_t speech_y = engine::graphics::SCREEN_HEIGHT * 2 / 3;
    constexpr uint8_t speech_sky_y = engine::graphics::SCREEN_HEIGHT / 3;

    // Show any speech if it's active.
    const auto & speech = *s_current_speech;
    ui_character(speech.uic);
    if (speech.text) {
        switch (speech.uic) {
            case UIC::None:
                break;
            case UIC::Ami:
                font::write_left(speech.text, speech.len, 0, speech_y);
                break;
            case UIC::Bucko:
                font::write_right(speech.text, speech.len, 0, speech_y);
                break;
            case UIC::Robucko:
                font::write_centered(speech.text, speech.len, speech_sky_y);
                break;
        }
    }
}

void ui_update() {
    PROFILE_SCOPE(ui_upd);

    // Do nothing if we're not in a UI state.
    bool is_ui = false;
    switch (s_level_state) {
        case LevelState::Intro:
        case LevelState::GameOver:
            is_ui = true;
            break;
        case LevelState::Bombs:
            break;
    }
    if (!is_ui) return;

    if (engine::input::g_buttons_pressed & GAMEPAD_BTN_A) {
        // Advance to next line.
        auto *speech = ++s_current_speech;
        if (speech->text == nullptr) {
            // We're finished, change state.
            level_advance(s_next_state);
        } else {
            ui_redraw();
        }
    }
}

void ui_setup() {
    game::font::setup_tiles<font_tile_start, font_sprite_start>();

    // Copy the tile data for the characters.
    game::images::copy_tile_data<
        ami_left_pal_start, ami_left_pal_count,
        ami_left_tile_start, ami_left_tile_count,
        game::images::ami_left
    >();
    game::images::copy_tile_data<
        bucko_left_pal_start, bucko_left_pal_count,
        bucko_left_tile_start, bucko_left_tile_count,
        game::images::bucko_left
    >();
}

//

void level_advance(LevelState state) {
    s_level_state = state;
    switch (state) {
        case LevelState::Intro:
            s_current_speech = intro_text;
            s_next_state = LevelState::Bombs;
            break;
        case LevelState::Bombs:
            break;
        case LevelState::GameOver:
            break;
    }
    ui_redraw();
}

} // namespace

//

void enter() {
    // Setup colours.
    engine::graphics::set_backdrop_a(RGB555(0, 0, 31));

    // Draw the parts of the screen.
    setup_tiles();
    setup_bitmaps();
    ui_setup();

    // Initial game state.
    level_advance(LevelState::Intro);

    // Show everything now that it's drawn.
    bios_vsync();
    engine::graphics::bitmap_0.enable();
    engine::graphics::bitmap_1.enable();
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
    engine::graphics::bitmap_1.disable();
    engine::graphics::bitmap_2.disable();
    engine::graphics::reset_sprites<transparent_sprite_start + transparent_sprite_count>();

    // Reset sound.
    engine::sound::stop_bgm();
}

Entry loop() {
    Entry next = Entry::Driving;

    // Wait for vblank to end.
    {
        PROFILE_SCOPE(rd_vsy);
        wait_until_line0();
    }

    // Update gamepad/mouse input.
    engine::input::update_inputs();

    // Return to the main menu if requested.
    if (engine::input::g_buttons_pressed & GAMEPAD_BTN_START) {
        return Entry::MainMenu;
    }

    // Print the last frame's timings before updating logic since we don't seem to have enough time after.
#if PRINT_PROFILING
    engine::profiler::print_timings();
#endif

    // We have a bit of breathing room before we need to draw the road.
    update_logic();
    draw_sprites();
    ui_update();

    // Draw the road.
    draw_road();

    // TODO: there should be breathing room after too, ie during vblank?

    return next;
}

} // namespace game::driving
