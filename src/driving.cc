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
#include "utils.h"
#include "vector.h"
#include "aabb.h"
#include "maths.h"

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
PROFILE_STORAGE(rd_phy);
PROFILE_STORAGE(rd_cns);
PROFILE_STORAGE(ui_upd);
PROFILE_STORAGE(bg_upd);

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
constexpr uint8_t car_pal_count = 10;
constexpr uint8_t car_sprite_start = font_sprite_start + font_sprite_count;
constexpr uint8_t car_sprite_count = 4;
constexpr uint8_t car_tile_start = font_tile_start + font_tile_count;
constexpr uint8_t car_tile_count = car_sprite_count * 2; // one for forwards, one for left/right (flip)

constexpr uint8_t voice_char_width = 3;
constexpr uint8_t voice_char_height = 4;
constexpr uint8_t bucko_troll_right_pal_start = car_pal_start + car_pal_count;
constexpr uint8_t bucko_troll_right_pal_count = 10;
constexpr uint8_t bucko_troll_right_sprite_start = car_sprite_start + car_sprite_count;
constexpr uint8_t bucko_troll_right_sprite_count = voice_char_width * voice_char_height;
constexpr uint8_t bucko_troll_right_tile_start = car_tile_start + car_tile_count;
constexpr uint8_t bucko_troll_right_tile_count = bucko_troll_right_sprite_count;

// Bit awkward, but bucko_left must be at a fixed offset (from breakout).
static_assert(bucko_troll_right_pal_start + bucko_troll_right_pal_count <= images::bucko_left::pal_offset);
constexpr uint8_t bucko_left_pal_start = images::bucko_left::pal_offset;
constexpr uint8_t bucko_left_pal_count = 10;
constexpr uint8_t bucko_left_sprite_start = bucko_troll_right_sprite_start;
constexpr uint8_t bucko_left_sprite_count = bucko_troll_right_sprite_count; // reuse the bucko sprites
constexpr uint8_t bucko_left_tile_start = bucko_troll_right_tile_start + bucko_troll_right_tile_count;
constexpr uint8_t bucko_left_tile_count = bucko_left_sprite_count;

constexpr uint8_t ami_left_pal_start = bucko_left_pal_start + bucko_left_pal_count;
constexpr uint8_t ami_left_pal_count = 10;
constexpr uint8_t ami_left_sprite_start = bucko_left_sprite_start; // reuse the bucko sprites
constexpr uint8_t ami_left_sprite_count = bucko_left_sprite_count;
constexpr uint8_t ami_left_tile_start = bucko_left_tile_start + bucko_left_tile_count;
constexpr uint8_t ami_left_tile_count = ami_left_sprite_count;

constexpr uint8_t tree_pal_start = ami_left_pal_start + ami_left_pal_count;
constexpr uint8_t tree_pal_count = 10;
constexpr uint8_t tree_sprite_start = ami_left_sprite_start + ami_left_sprite_count;
constexpr uint8_t tree_sprite_count = 2; // one on each side
constexpr uint8_t tree_tile_start = ami_left_tile_start + ami_left_tile_count;
constexpr uint8_t tree_tile_count = 3; // far/med/close -> tiny/small/big

constexpr uint8_t bomb_pal_start = tree_pal_start + tree_pal_count;
constexpr uint8_t bomb_pal_count = 10;
constexpr uint8_t bomb_sprite_start = tree_sprite_start + tree_sprite_count;
constexpr uint8_t bomb_sprite_count = 32;
constexpr uint8_t bomb_tile_start = tree_tile_start + tree_tile_count;
constexpr uint8_t bomb_tile_count = 1;

constexpr uint8_t ufo_pal_start = bomb_pal_start + bomb_pal_count;
constexpr uint8_t ufo_pal_count = 10;
constexpr uint8_t ufo_sprite_start = bomb_sprite_start + bomb_sprite_count;
constexpr uint8_t ufo_sprite_count = 4;
constexpr uint8_t ufo_tile_start = bomb_tile_start + bomb_tile_count;
constexpr uint8_t ufo_tile_count = 4;

constexpr uint8_t gauge_pal_start = ufo_pal_start + ufo_pal_count;
constexpr uint8_t gauge_pal_count = 10;
constexpr uint8_t gauge_sprite_start = ufo_sprite_start + ufo_sprite_count;
constexpr uint8_t gauge_sprite_count = 1;
constexpr uint8_t gauge_tile_start = ufo_tile_start + ufo_tile_count;
constexpr uint8_t gauge_tile_count = 5;

// Reuses the basic palette above.
constexpr uint8_t transparent_sprite_start = gauge_sprite_start + gauge_sprite_count;
constexpr uint8_t transparent_sprite_count = SPRITES_FOR_ROAD ? 3 : 0; // max 3 road splits
constexpr uint8_t transparent_tile_start = gauge_tile_start + gauge_tile_count;
constexpr uint8_t transparent_tile_count = SPRITES_FOR_ROAD ? 1 : 0;

constexpr uint8_t skyline_pal_start = gauge_pal_start + gauge_pal_count;
constexpr uint8_t skyline_pal_count = 16;
constexpr uint8_t dome_pal_start = skyline_pal_start + skyline_pal_count;
constexpr uint8_t dome_pal_count = 16;

static_assert(dome_pal_start + dome_pal_count <= font::font_palette_start);

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
constexpr int16_t get_car_x() {
    constexpr uint16_t car_x_scale = 32;
    return (s_xpos * car_x_scale).value() + engine::graphics::SCREEN_WIDTH / 2 - engine::graphics::bg_tile_size;
}
constexpr int16_t car_y = engine::graphics::SCREEN_HEIGHT - engine::graphics::bg_tile_size - 40;

// How fast we move through the road.
constexpr uint8_t road_speed_scale = 4;
constexpr uint8_t min_road_speed = 2;
engine::utils::FixedS1616 s_road_position; // clamped to [0, 256]
engine::utils::FixedS1616 s_road_speed;

uint8_t get_current_max_speed() {
    static constexpr uint8_t max_road_speeds[8+1] = {
        2, 4, 8, 16, 16, 8, 4, 2,
        2, // extra in case of xpos=1
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

// Generated with `for i in range(80): print(int(math.pow(1.06, 79-i)-1), end=',')`
static const uint8_t s_turning_offset[road_sections] = {
    98,93,87,82,78,73,69,65,61,58,54,51,48,45,43,40,38,36,33,31,30,28,26,25,23,22,20,19,18,17,16,15,14,13,12,11,11,10,9,9,8,8,7,7,6,6,5,5,5,4,4,4,3,3,3,3,2,2,2,2,2,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0
};

__attribute__((always_inline))
inline int8_t road_curvature_at_section(engine::utils::FixedS1616 const & road_rot, engine::utils::FixedS1616 const & xpos, uint8_t road_section) {
    auto dx = engine::utils::FixedS1616::from(0);

    // Road curvature.
    dx += road_rot * s_turning_offset[road_section];

    // Translation of car.
    dx += xpos * road_section;

    return dx.value();
}

//

constexpr auto dome_y_per_frame = engine::utils::FixedS1616::div(8, 60);
engine::utils::FixedS1616 s_dome_y;

//

struct alignas(2) Bomb {
    uint8_t x;
    uint8_t y;

    static constexpr uint8_t fall_speed = 2;

    constexpr auto aabb() const {
        return engine::utils::AABB {
            x, y,
            engine::graphics::bg_tile_size, engine::graphics::bg_tile_size,
        };
    }
};
engine::utils::Vector<Bomb, bomb_sprite_count> s_bombs;

struct UFO {
    static constexpr int16_t off_screen = -2 * static_cast<int16_t>(engine::graphics::bg_tile_size);
    static constexpr int16_t middle_screen = engine::graphics::SCREEN_WIDTH / 2 - engine::graphics::bg_tile_size;
    static constexpr auto dx_per_frame = engine::utils::FixedS1616::from(1);
    engine::utils::FixedS1616 x;

    static constexpr int16_t y = road_start / 2 - engine::graphics::bg_tile_size;

    static constexpr uint16_t stage_length = 19 * 60;
    uint16_t timer;

    enum class Pattern : uint8_t { Animate, Arrive, Drop, Escape, Finished, } pattern;

    // Each element is ~ 1/8s apart.
    // Bombs take ~1 to fall.
    static_assert(Bomb::fall_speed == 2);
    static constexpr uint8_t bomb_drop_pattern[] {
        1, 0, 0, 0, 1, 0, 0, 0,
        1, 0, 1, 0, 1, 0, 1, 0,
        1, 0, 1, 0, 1, 0, 1, 0,

        0, 0, 0, 0, 0, 0, 0, 0,

        1, 0, 0, 0, 1, 0, 0, 0,
        1, 0, 1, 0, 1, 0, 1, 0,
        1, 0, 1, 0, 1, 0, 1, 0,

        0, 0, 0, 0, 0, 0, 0, 0,

        1, 0, 0, 0, 1, 0, 0, 0,

        1, 1, 1, 1, 1, 1, 1, 0,
        1, 1, 1, 1, 1, 1, 1, 0,
        1, 1, 1, 1, 1, 1, 1, 0,
        1, 1, 1, 1, 1, 1, 1, 0,

        1, 0, 0, 1, 0, 0, 1, 0, 0,
        1, 1, 1, 1, 1, 0,
        1, 0, 0, 1, 0, 0, 1, 0, 0,
        1, 1, 1, 1, 1, 0,

        // Give time for the remaining bombs to fall.
        0, 0, 0, 0, 0, 0, 0, 0,
    };

    void reset() {
        x = engine::utils::FixedS1616::from(off_screen);
        pattern = Pattern::Finished;
    }

    void animate_in() {
        pattern = Pattern::Animate;
    }

    void start() {
        pattern = Pattern::Arrive;
        timer = 0;
    }

    bool update() {
        bool finished = false;

        switch (pattern) {
            case Pattern::Animate:
            case Pattern::Arrive:
                if (x.value() >= middle_screen) {
                    pattern = pattern == Pattern::Animate ? Pattern::Finished : Pattern::Drop;
                } else {
                    x += dx_per_frame;
                }
                break;

            case Pattern::Drop: {
                const uint16_t t = ++timer;
                if (t == stage_length) {
                    pattern = Pattern::Escape;
                } else {
                    const auto dx = engine::utils::FixedS1616::div(engine::maths::cos(timer), 64);
                    x += dx;

                    // 7 = ~8 per second max.
                    if ((t & 7) == 7) {
                        static_assert((stage_length >> 3) < 0xFF);
                        const uint8_t i = t >> 3;
                        static_assert((stage_length >> 3) == engine::utils::size(bomb_drop_pattern));
                        if (bomb_drop_pattern[i]) {
                            s_bombs.push_back({
                                static_cast<uint8_t>(x.value()),
                                y + engine::graphics::bg_tile_size * 2
                            });
                        }
                    }
                }
            } break;

            case Pattern::Escape:
                if (x.value() <= off_screen) {
                    pattern = Pattern::Finished;
                } else {
                    x -= dx_per_frame;
                }
                break;

            case Pattern::Finished:
                finished = true;
                break;
        }

        return finished;
    }
} s_ufo;

//

enum class LevelState {
    Intro1,
    UFOFlyIn,
    Intro2,
    Bombs,
    Win1,
    Curves,
    BombsCurves,
    Win2,
    BombsCurves2,
    Win3,
    Dome,
    Dome2,
    GameOver,
    GameOver2,
} s_level_state;

void level_advance(LevelState state);

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
    images::copy_tile_data<
        bomb_pal_start, bomb_pal_count,
        bomb_tile_start, bomb_tile_count,
        images::bomb
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
    images::copy_tile_data<
        bucko_troll_right_pal_start, bucko_troll_right_pal_count,
        bucko_troll_right_tile_start, bucko_troll_right_tile_count,
        images::bucko_troll_right
    >();

    // UI bits.
    images::copy_tile_data<
        gauge_pal_start, gauge_pal_count,
        gauge_tile_start, gauge_tile_count,
        images::gauge
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
        static_assert(images::skyline_raw::width == skyline_width);
        static_assert(images::skyline_raw::height == skyline_height);
        uint8_t * data = VDP.BITMAP_VRAM_8BIT + skyline_start_y * SCREEN_WIDTH;
        images::skyline_raw::decompress(data);
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
        static_assert(images::dome_raw::width == dome_width);
        static_assert(images::dome_raw::height == dome_height);
        uint8_t * data = VDP.BITMAP_VRAM_8BIT + dome_start_y * SCREEN_WIDTH;
        images::dome_raw::decompress(data);
    }
}

//

// Incoming road layouts.
// Note: increments are +-10 per layout.
constexpr int8_t road_layout_forward[128] = {};
constexpr int8_t road_layout_left_slow[] = {
    1, 0, 0, 0,
    1, 0, 0, 0,
    1, 0, 0, 0,
    1, 0, 0, 0,
    1, 0, 0, 0,
    1, 0, 0, 0,
    1, 0, 0, 0,
    1, 0, 0, 0,
    1, 0, 0, 0,
    1, 0, 0, 0,
};
constexpr int8_t road_layout_left_fast[] = {
    1, 0,
    1, 0,
    1, 0,
    1, 0,
    1, 0,
    1, 0,
    1, 0,
    1, 0,
    1, 0,
    1, 0,
};
constexpr int8_t road_layout_right_slow[] = {
    -1, 0, 0, 0,
    -1, 0, 0, 0,
    -1, 0, 0, 0,
    -1, 0, 0, 0,
    -1, 0, 0, 0,
    -1, 0, 0, 0,
    -1, 0, 0, 0,
    -1, 0, 0, 0,
    -1, 0, 0, 0,
    -1, 0, 0, 0,
};
constexpr int8_t road_layout_right_fast[] = {
    -1, 0,
    -1, 0,
    -1, 0,
    -1, 0,
    -1, 0,
    -1, 0,
    -1, 0,
    -1, 0,
    -1, 0,
    -1, 0,
};

const int8_t * s_current_road_layout;
int16_t s_current_road_remaining;
int8_t s_current_road_bias;

void road_reset() {
    s_current_road_remaining = 0;
    s_current_road_bias = 0;
    s_road_rotation = engine::utils::FixedS1616::from(0);
}

void road_pick_next(const int8_t * & layout, int16_t & size) {
    const uint8_t rng = engine::utils::g_rng() & 0xFF;
    const bool b0 = (rng >> 0) & 1;
    const bool b1 = (rng >> 1) & 1;
    const bool b2 = (rng >> 2) & 1;

    enum class Dir { LeftFast, LeftSlow, ForwardShort, ForwardLong, RightSlow, RightFast };
    Dir dir = Dir::ForwardLong;

    // left fwd right
    //   2 1 0 -1 -2
    int8_t bias = s_current_road_bias;
    switch (bias) {
        case 2:
            if (b0) dir = Dir::ForwardShort; // 50%
            else dir = b1 ? Dir::RightSlow : Dir::RightFast; // 50% right
            break;

        case 1:
            if (b0) dir = Dir::ForwardShort; // 50%
            else if (b1) dir = Dir::RightSlow; // 25% right
            else if (b2) dir = Dir::RightFast; // 12% right
            else dir = Dir::LeftSlow; // 12% left
            break;

        case 0:
            if (b0) dir = b2 ? Dir::ForwardLong : Dir::ForwardShort; // 50%
            else if (b1) dir = b2 ? Dir::RightSlow : Dir::RightFast; // 25% right
            else dir = b2 ? Dir::LeftSlow : Dir::LeftFast; // 25% left
            break;

        case -1:
            if (b0) dir = Dir::ForwardShort; // 50%
            else if (b1) dir = Dir::LeftSlow; // 25% left
            else if (b2) dir = Dir::LeftFast; // 12% left
            else dir = Dir::RightSlow; // 12% right
            break;

        case -2:
            if (b0) dir = Dir::ForwardShort; // 50%
            else dir = b1 ? Dir::LeftSlow : Dir::LeftFast; // 50% left
            break;

        default:
            ASSERT(false);
            bias = 0; // reset, shouldn't happen!
            break;
    }

    // Lock the direction for some of the levels.
    switch (s_level_state) {
        case LevelState::Intro1:
        case LevelState::UFOFlyIn:
        case LevelState::Intro2:
        case LevelState::Bombs:
            dir = Dir::ForwardLong;
            break;
        case LevelState::Win1:
            dir = Dir::ForwardShort;
            break;
        case LevelState::Curves:
        case LevelState::BombsCurves:
        case LevelState::Win2:
        case LevelState::BombsCurves2:
        case LevelState::Win3:
        case LevelState::GameOver:
        case LevelState::GameOver2:
            break;
        case LevelState::Dome:
        case LevelState::Dome2:
            // Reset to forward.
            if (bias < 0) dir = Dir::LeftFast;
            else if (bias > 0) dir = Dir::RightFast;
            else dir = Dir::ForwardLong;
            break;
    }

    auto set = [&](auto && val) {
        layout = val;
        size = engine::utils::size(val);
    };

    switch (dir) {
        case Dir::LeftFast:
            set(road_layout_left_fast);
            bias += 1;
            break;
        case Dir::LeftSlow:
            set(road_layout_left_slow);
            bias += 1;
            break;
        case Dir::ForwardLong:
            set(road_layout_forward);
            break;
        case Dir::ForwardShort:
            set(road_layout_forward);
            size /= 2;
            break;
        case Dir::RightSlow:
            set(road_layout_right_slow);
            bias -= 1;
            break;
        case Dir::RightFast:
            set(road_layout_right_fast);
            bias -= 1;
            break;
    }
    s_current_road_bias = bias;
}

int16_t road_consume_offsets(int16_t count) {
    PROFILE_SCOPE(rd_cns);

    int16_t total = 0;
    const int8_t * road_layout = s_current_road_layout;
    int16_t road_remaining = s_current_road_remaining;

    // Read chunks of the road at a time.
    while (count > 0) {
        // Pick a new stretch of road if we're out.
        if (road_remaining == 0) {
            road_pick_next(road_layout, road_remaining);
        }

        const int16_t consumed = engine::utils::min(count, road_remaining);
        count -= consumed;

        const int8_t * ptr = road_layout;
        for (uint8_t i = 0; i < consumed; i++) {
            total += *ptr++;
        }
        road_layout += consumed;
        road_remaining -= consumed;
    }

    s_current_road_layout = road_layout;
    s_current_road_remaining = road_remaining;
    return total;
}

//

Entry update_logic() {
    PROFILE_SCOPE(rd_log);

    Entry next = Entry::Driving;
    switch (s_level_state) {
        case LevelState::Intro1:
        case LevelState::Intro2:
        case LevelState::Win1:
        case LevelState::Curves:
        case LevelState::Win2:
        case LevelState::Win3:
        case LevelState::GameOver:
            // UI states don't need an update loop.
            break;

        case LevelState::UFOFlyIn:
            // Move the UFO into view.
            if (s_ufo.update()) {
                level_advance(LevelState::Intro2);
            }
            break;
        case LevelState::Dome:
            // Animate the dome rising.
            s_dome_y += dome_y_per_frame;
            if (s_dome_y.value() >= road_start) {
                level_advance(LevelState::Dome2);
            }
            break;

        case LevelState::Bombs:
            if (s_ufo.update()) level_advance(LevelState::Win1);
            break;
        case LevelState::BombsCurves:
            if (s_ufo.update()) level_advance(LevelState::Win2);
            break;
        case LevelState::BombsCurves2:
            if (s_ufo.update()) level_advance(LevelState::Win3);
            break;

        case LevelState::Dome2:
            intertile::setup(intertile::Text::Dome, Entry::Cyber);
            next = Entry::Intertitle;
            break;
        case LevelState::GameOver2:
            next = Entry::MainMenu;
            break;
    }

    return next;
}

void update_physics() {
    PROFILE_SCOPE(rd_phy);

    const uint16_t held = engine::input::g_buttons_held;

    //

    // Speed.
    const uint16_t max_road_speed = get_current_max_speed();
    if ((held & GAMEPAD_BTN_UP) && (s_road_speed * road_speed_scale).value() <= max_road_speed) { s_road_speed += accel_per_frame; }
    else if ((held & GAMEPAD_BTN_DOWN)) { s_road_speed -= accel_per_frame; } // clamp happens after drag

    // Apply drag.
    s_road_speed -= drag_per_frame;
    if ((s_road_speed * road_speed_scale).value() < min_road_speed) {
        s_road_speed = engine::utils::FixedS1616::div(min_road_speed, road_speed_scale);
    }

    //

    // Move along the road.
    const int16_t position_start = s_road_position.value();
    s_road_position += s_road_speed;
    const int16_t position_end = s_road_position.value();
    if (s_road_position.value() >= 256) {
        s_road_position -= engine::utils::FixedS1616::from(256); // emulate wrapping like uint8_t
    }
    ASSERT(s_road_position.value() >= 0);
    ASSERT(s_road_position.value() <= 255);

    const int16_t total_curve = road_consume_offsets(position_end - position_start);
    s_road_rotation += engine::utils::FixedS1616::div(total_curve, 32);

    //

    // Handle input.
    if (held & GAMEPAD_BTN_LEFT) { s_xpos -= dx_per_frame; }
    else if (held & GAMEPAD_BTN_RIGHT) { s_xpos += dx_per_frame; }

    // The curvature moves us too.
    const auto curvature = (s_road_rotation * 64).value() * (s_road_speed * road_speed_scale).value() / 64;
    s_xpos += engine::utils::FixedS1616::div(curvature, 128);

    // Clamp position.
    if (s_xpos.value() >= 1) {
        s_xpos = engine::utils::FixedS1616::from(1);
    } else if (s_xpos.value() < -1) { // TODO: this should be <=, but off-by-one with -ve (see header)
        s_xpos = engine::utils::FixedS1616::from(-1);
    }

    //

    // Bomb collisions.
    static_assert(car_sprite_count == 4);
    const engine::utils::AABB car_aabb {
        get_car_x(), car_y,
        2 * engine::graphics::bg_tile_size, 2 * engine::graphics::bg_tile_size,
    };

    uint8_t num_bombs = s_bombs.size();
    for (uint8_t i = 0; i < num_bombs; ) {
        auto & bomb = s_bombs[i];

        // Move the bomb down.
        bomb.y += bomb.fall_speed;

        // See if it collided.
        bool remove = false;
        if (bomb.y > car_aabb.y + car_aabb.h) {
            remove = true;
        } else if (car_aabb.intersects(bomb.aabb())) {
            remove = true;

            engine::sound::play_effect(game::music::SoundEffect::SE_Driving_Hit);
            // TODO
            DEBUG_MSG("car hit");
        }

        // Remove this one, otherwise jump to next.
        if (remove) {
            s_bombs.remove_fast(i);
            num_bombs--;
            engine::graphics::set_sprite(bomb_sprite_start + num_bombs, {}); // reset the sprite that was removed
        } else {
            i++;
        }
    }
}

void draw_sprites() {
    using namespace engine::graphics;

    PROFILE_SCOPE(rd_til);

    {
        static_assert(car_sprite_count == 4);

        const uint16_t held = engine::input::g_buttons_held;
        const uint8_t is_turning = (held & (GAMEPAD_BTN_LEFT | GAMEPAD_BTN_RIGHT)) ? 4 : 0;
        const bool x_flipped = held & GAMEPAD_BTN_LEFT;

        // Car sprite.
        const auto car_x = get_car_x();
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
        if (road_start <= road_pos && road_pos < road_start + road_length) {
            // Work out how far down the road we are.
            const uint8_t road_y = road_pos - road_start;
            const int16_t left_x = s_pavement_line_start[road_y] - bg_tile_size * 2;
            const int16_t curvature = road_curvature_at_section(s_road_rotation, s_xpos, road_y / scanlines_per_section);

            if (left_x >= 0) {
                // Map that to the size to use.
                static_assert(tree_tile_count == 3);
                uint8_t size_idx = 0;
                if (road_y > road_length / 3) {
                    size_idx = 2;
                } else if (road_y > road_length / 8) {
                    size_idx = 1;
                }

                // Draw one on each side.
                static_assert(tree_sprite_count == 2);
                ObjSprite sprite;
                sprite.set_x(SCREEN_WIDTH - bg_tile_size - left_x - curvature);
                sprite.set_y(road_pos);
                sprite.set_tile_index(tree_tile_start + size_idx);
                sprite.set_x_flip(false);
                set_sprite(tree_sprite_start + 0, sprite);

                sprite.set_x_flip(true);
                sprite.set_x(left_x - curvature);
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

    // UFO.
    {
        static_assert(ufo_sprite_count == 4);

        constexpr int16_t ufo_y = s_ufo.y;
        const int16_t ufo_x = s_ufo.x.value();

        if (ufo_x >= 0) {
            for (uint8_t idx = 0; idx < ufo_sprite_count; idx++) {
                ObjSprite sprite;
                const uint16_t dx = (idx & 1) ? engine::graphics::bg_tile_size : 0;
                const uint16_t dy = (idx & 2) ? engine::graphics::bg_tile_size : 0;
                sprite.set_x(ufo_x + dx);
                sprite.set_y(ufo_y + dy);
                sprite.set_tile_index(ufo_tile_start + idx);
                set_sprite(ufo_sprite_start + idx, sprite);
            }
        } else {
            for (uint8_t idx = 0; idx < ufo_sprite_count; idx++) {
                ObjSprite sprite;
                set_sprite(ufo_sprite_start + idx, sprite);
            }
        }
    }

    // Bombs.
    {
        ObjSprite sprite;
        sprite.set_tile_index(bomb_tile_start);
        int sprite_idx = 0;
        for (const auto & bomb : s_bombs) {
            sprite.set_x(bomb.x);
            sprite.set_y(bomb.y);
            set_sprite(bomb_sprite_start + sprite_idx, sprite);
            sprite_idx++;
        }
    }

    // Speed gauge.
    {
        constexpr uint8_t x_pos = 3;
        constexpr uint8_t y_pos = SCREEN_HEIGHT / 3;

        // Should really be checking that max speed is number of tiles.
        static_assert(road_speed_scale + 1 == gauge_tile_count);
        const uint8_t speed_idx = s_road_speed.value();

        ObjSprite sprite;
        sprite.set_x(x_pos);
        sprite.set_y(y_pos);
        sprite.set_tile_index(gauge_tile_start + speed_idx);
        set_sprite(gauge_sprite_start, sprite);
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

void draw_bg() {
    using namespace engine::graphics;
    PROFILE_SCOPE(bg_upd);

    // Scroll the skyline.
    const auto scroll = (s_road_rotation * 64).value() * (s_road_speed * road_speed_scale).value() / 64;
    engine::graphics::bitmap_2.scroll_x() -= scroll;

    // Make the dome rise.
    const int16_t dome_y = s_dome_y.value();
    engine::graphics::bitmap_1.height() = dome_y;
    engine::graphics::bitmap_1.position_y() = road_start - dome_y;
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

        const int8_t dx = road_curvature_at_section(road_rot, xpos, road_section);
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

enum class UIC : uint8_t { None, NoneRight, Ami, Bucko, Robucko, BuckoTroll, };
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
        case UIC::NoneRight:
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
        case UIC::BuckoTroll:
            for (int y = 0; y < voice_char_height; y++) {
                for (int x = 0; x < voice_char_width; x++) {
                    sprite.set_x(right_start_x + x * bg_tile_size);
                    sprite.set_y(right_start_y + y * bg_tile_size);
                    sprite.set_tile_index(bucko_troll_right_tile_start + sprite_idx);
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
    constexpr Speech(UIC c, const char (&str)[N]) : uic(c), len(N), text(str) { static_assert(N <= 27); }
    constexpr Speech(decltype(nullptr)) : uic(UIC::None), len(0), text(nullptr) {}
};

constexpr Speech intro1_text[] {
    { UIC::NoneRight, "bzzt" },
    { UIC::Bucko, "Ami!" },
    { UIC::Bucko, "It worked!" },
    { UIC::Bucko, "That unsuspecting bucko" },
    { UIC::Bucko, "did the important thing!" },

    { UIC::Ami, "Good job bucko!" },
    { UIC::Ami, "That's one less outpost" },
    { UIC::Ami, "under robucko control." },
    { UIC::Ami, "But it won't be long" },
    { UIC::Ami, "before the other" },
    { UIC::Ami, "outposts notice." },

    { UIC::Bucko, "You need to get to them" },
    { UIC::Bucko, "quickly then!" },
    { UIC::Bucko, "Use the up and down" },
    { UIC::Bucko, "buttons to control your" },
    { UIC::Bucko, "speed." },
    { UIC::Bucko, "The closer to the centre" },
    { UIC::Bucko, "you are the faster" },
    { UIC::Bucko, "you can go." },

    nullptr,
};

constexpr Speech intro2_text[] {
    { UIC::Robucko, "Halt!" },
    { UIC::Robucko, "We have you surrounded" },
    { UIC::Robucko, "at least from this sky!" },

    { UIC::Bucko, "Look out!" },
    { UIC::Bucko, "That's a highly armoured" },
    { UIC::Bucko, "flying robucko!"},
    { UIC::Bucko, "And it's about to start" },
    { UIC::Bucko, "dropping bombs!" },
    { UIC::Bucko, "Evasive maneuvers!" },

    nullptr,
};

constexpr Speech win1_text[] {
    { UIC::Bucko, "Phew!" },
    { UIC::Bucko, "It looks like it" },
    { UIC::Bucko, "ran out of bombs" },
    { UIC::Bucko, "and flew away." },
    { UIC::Bucko, "You saved us Ami!" },
    { UIC::Ami, "..." },
    { UIC::Ami, "Don't you think that" },
    { UIC::Ami, "it'll come back after" },
    { UIC::Ami, "it's restocked?" },
    { UIC::Bucko, "Oh." },
    { UIC::Bucko, "I didn't think about that." },
    { UIC::Ami, "..." },

    { UIC::Bucko, "Then you should skip the" },
    { UIC::Bucko, "other outposts and head" },
    { UIC::Bucko, "straight to the dome." },
    //{ UIC::Bucko, "And not because the" },
    //{ UIC::Bucko, "dev ran out of time" },

    { UIC::Bucko, "Look out!" },
    { UIC::Bucko, "The road ahead gets" },
    { UIC::Bucko, "a bit twisty." },

    nullptr,
};

constexpr Speech curves_text[] {
    { UIC::Bucko, "If you're not careful" },
    { UIC::Bucko, "you'll get pushed away" },
    { UIC::Bucko, "from the centre." },

    { UIC::Bucko, "It's a good thing that" },
    { UIC::Bucko, "the roads are so wide." },

    { UIC::Ami, "I thought that my car" },
    { UIC::Ami, "was just small." },

#if 0
    { UIC::Ami, "Hey bucko?" },
    { UIC::Bucko, "Yes?" },
    { UIC::Ami, "Why is it that I can" },
    { UIC::Ami, "only say a few words" },
    { UIC::Ami, "at a time?" },
    { UIC::Bucko, "It's a miracle that we" },
    { UIC::Bucko, "can say anything at" },
    { UIC::Bucko, "all in this part of" },
    { UIC::Bucko, "the game lol" },
#endif

    nullptr,
};

constexpr Speech win2_text[] {
    { UIC::Bucko, "Ami!" },
    { UIC::Ami, "Yes?" },
#if 1
    { UIC::Bucko, "Since you're part Irish" },
    { UIC::Bucko, "would you say that buckos" },
    { UIC::Bucko, "are wee creatures?" },
    { UIC::Ami, "Where are you going" },
    { UIC::Ami, "with this?" },
    { UIC::Bucko, "Well you could name" },
    { UIC::Bucko, "this operation" },
    { UIC::Bucko, "help all t' wee creatures!" },
#else
    { UIC::Bucko, "I think you should name" },
    { UIC::Bucko, "this operation" },
    { UIC::Bucko, "help all t' wee creatures!" },
    { UIC::Ami, "..." },
    { UIC::Bucko, "because buckos are..." },
#endif
    { UIC::Ami, "No!" },
    { UIC::Ami, "We're not calling it" },
    { UIC::Ami, "operation H.A.W.C." },
    { UIC::Ami, "That was last year's joke." },
    { UIC::BuckoTroll, "" },

    nullptr,
};

constexpr Speech win3_text[] {
    { UIC::Bucko, "I think that was the" },
    { UIC::Bucko, "last stage." },
    { UIC::Ami, "How can you tell?" },
    { UIC::Bucko, "Rule of 3." },
    { UIC::Ami, "..." },
    { UIC::Bucko, "And we're approaching" },
    { UIC::Bucko, "the dome." },

    nullptr,
};

constexpr Speech gameover_text[] {
    { UIC::Bucko, "Ami?" },
    { UIC::Bucko, "Ami!" },
    { UIC::Bucko, "Ami!!!!!!!" },

    nullptr,
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
            case UIC::Ami:
                font::write_left(speech.text, speech.len, 0, speech_y);
                break;
            case UIC::NoneRight:
            case UIC::Bucko:
            case UIC::BuckoTroll:
                font::write_right(speech.text, speech.len, 0, speech_y);
                break;
            case UIC::None:
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
        case LevelState::Intro1:
        case LevelState::Intro2:
        case LevelState::Win1:
        case LevelState::Curves:
        case LevelState::Win2:
        case LevelState::Win3:
        case LevelState::GameOver:
            is_ui = true;
            break;
        case LevelState::UFOFlyIn:
        case LevelState::Bombs:
        case LevelState::BombsCurves:
        case LevelState::BombsCurves2:
        case LevelState::Dome:
        case LevelState::Dome2:
        case LevelState::GameOver2:
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
        case LevelState::Intro1:
            road_reset();
            engine::sound::play_effect(game::music::SoundEffect::SE_Driving_Car);
            s_current_speech = intro1_text;
            s_next_state = LevelState::UFOFlyIn;
            break;
        case LevelState::Intro2:
            s_current_speech = intro2_text;
            s_next_state = LevelState::Bombs;
            break;
        case LevelState::Win1:
            engine::sound::play_effect(game::music::SoundEffect::SE_Driving_Car);
            s_current_speech = win1_text;
            s_next_state = LevelState::Curves;
            break;
        case LevelState::Curves:
            s_current_speech = curves_text;
            s_next_state = LevelState::BombsCurves;
            break;
        case LevelState::Win2:
            engine::sound::play_effect(game::music::SoundEffect::SE_Driving_Car);
            s_current_speech = win2_text;
            s_next_state = LevelState::BombsCurves2;
            break;
        case LevelState::Win3:
            engine::sound::play_effect(game::music::SoundEffect::SE_Driving_Car);
            s_current_speech = win3_text;
            s_next_state = LevelState::Dome;
            break;

        case LevelState::UFOFlyIn:
            engine::sound::play_effect(game::music::SoundEffect::SE_Driving_WeewooHi);
            s_ufo.animate_in();
            break;
        case LevelState::Dome:
            // Animation state.
            break;

        case LevelState::Bombs:
        case LevelState::BombsCurves:
        case LevelState::BombsCurves2:
            engine::sound::play_effect(game::music::SoundEffect::SE_Driving_WeewooLo);
            s_ufo.start();
            break;

        case LevelState::GameOver:
            s_current_speech = gameover_text;
            s_next_state = LevelState::GameOver2;
            break;

        case LevelState::GameOver2:
        case LevelState::Dome2:
            // These jump straight to the next scene.
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
    s_road_speed = engine::utils::FixedS1616::div(min_road_speed, road_speed_scale);
    s_dome_y = engine::utils::FixedS1616::from(0);
    s_ufo.reset();
    level_advance(LevelState::Intro1);

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
    engine::sound::play_effect(game::music::SoundEffect::SE_Stop);
}

Entry loop() {
    Entry next = Entry::Driving;

    // Wait for vblank to end.
    {
        PROFILE_SCOPE(rd_vsy);
        engine::graphics::wait_until_line0();
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
    next = update_logic();
    if (next != Entry::Driving) {
        return next;
    }

    update_physics();
    draw_sprites();
    draw_bg();
    ui_update();

    // Draw the road.
    draw_road();

    // TODO: there should be breathing room after too, ie during vblank?

    return next;
}

} // namespace game::driving
