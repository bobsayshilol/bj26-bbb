#include "aabb.h"
#include "fixed.h"
#include "font.h"
#include "game.h"
#include "images.h"
#include "input.h"
#include "graphics.h"
#include "maths.h"
#include "memory.h"
#include "music.h"
#include "profiler.h"
#include "sound.h"
#include "utils.h"
#include "vector.h"

namespace game::breakout {

namespace {

PROFILE_STORAGE(bl_phy);
PROFILE_STORAGE(bl_drw);
PROFILE_STORAGE(bl_set);
PROFILE_STORAGE(bk_set);
PROFILE_STORAGE(bk_upd);
PROFILE_STORAGE(pd_set);
PROFILE_STORAGE(pd_upd);
PROFILE_STORAGE(bg_set);
PROFILE_STORAGE(bk_rec);
PROFILE_STORAGE(vsync);

constexpr uint8_t pal_white = 1;
constexpr uint8_t pal_grey = 2;
constexpr uint8_t pal_ball_start = 3;
constexpr uint8_t pal_ball_count = 16; // TODO: less colours

// font has highest prio.
constexpr uint8_t font_sprite_start = 0;
constexpr uint8_t font_sprite_count = font::font_max_sprites;
constexpr uint8_t font_tile_start = 0;
constexpr uint8_t font_tile_count = font::font_tile_count;

constexpr uint8_t ball_sprite_start = font_sprite_start + font_sprite_count;
constexpr uint8_t ball_sprite_count = 4; // big quad - TODO: look into 32x32 tile instead
constexpr uint8_t ball_tile_start = font_tile_start + font_tile_count;
constexpr uint8_t ball_tile_count = ball_sprite_count * 8; // animation frames

constexpr uint8_t trapped_sprite_start = ball_sprite_start + ball_sprite_count;
constexpr uint8_t trapped_sprite_count = ball_sprite_count;

constexpr uint8_t block_pal_start = pal_ball_start + pal_ball_count;
constexpr uint8_t block_pal_count = 6; // num colours
constexpr uint8_t block_sprite_start = trapped_sprite_start + trapped_sprite_count;
constexpr uint8_t block_sprite_count = block_pal_count * 8; // # of each colour
constexpr uint8_t block_tile_start = ball_tile_start + ball_tile_count;
constexpr uint8_t block_tile_count = block_pal_count;

constexpr uint8_t paddle_pal_start = block_pal_start + block_pal_count;
constexpr uint8_t paddle_pal_count = 1;

constexpr uint8_t bg_pal_start = paddle_pal_start + paddle_pal_count;
constexpr uint8_t bg_pal_count = 3;
constexpr uint8_t bg_tile_start = block_tile_start + block_tile_count;
constexpr uint8_t bg_tile_inner = bg_tile_start + 0;
constexpr uint8_t bg_tile_border = bg_tile_start + 1;
constexpr uint8_t bg_tile_outer = bg_tile_start + 2;
constexpr uint8_t bg_pal_outer = bg_pal_start + (bg_tile_outer - bg_tile_start);
constexpr uint8_t bg_tile_count = 3;
static_assert(bg_pal_count == bg_tile_count);

//

constexpr uint32_t wall_padding_x = 6 * engine::graphics::bg_tile_size;
constexpr uint32_t wall_padding_y = 4 * engine::graphics::bg_tile_size;
constexpr uint32_t block_width = 8;
constexpr uint32_t block_height = 2;

static_assert(ball_sprite_count == 4);
constexpr uint32_t ball_width = engine::graphics::bg_tile_size * 2;
constexpr uint32_t ball_height = ball_width;
struct Ball {
    // Position is top left
    engine::utils::FixedS1616 x;
    engine::utils::FixedS1616 y;
    engine::utils::FixedS1616 vx; // TODO: S88?
    engine::utils::FixedS1616 vy;
    engine::utils::FixedU88 frame;
    int8_t rot_speed;

    constexpr engine::utils::AABB aabb() const {
        return {
            x.value(), y.value(),
            ball_width, ball_height,
        };
    }

    // with_left is whether or not the wall/block is on the left of the ball.
    void bounce_x(bool with_left) {
        // Increase/decrease if going same/opposite direction
        const bool moving_up = vy.value() < 0;
        if (with_left == moving_up) rot_speed++; else rot_speed--;
        vx = -vx;
        x += vx; // move back since we might still be overlapping
        engine::sound::play_effect(game::music::SoundEffect::SE_Breakout_Bounce);
    }
    // with_top is whether or not the wall/block is on the top of the ball.
    void bounce_y(bool with_top) {
        // Increase/decrease if going same/opposite direction
        const bool moving_left = vx.value() < 0;
        if (with_top != moving_left) rot_speed++; else rot_speed--;
        vy = -vy;
        y += vy; // move back since we might still be overlapping
        engine::sound::play_effect(game::music::SoundEffect::SE_Breakout_Bounce);
    }
};
Ball s_ball;

constexpr engine::utils::FixedU88 s_ball_speeds[] = {
    engine::utils::FixedU88::div(-9, 8),
    engine::utils::FixedU88::div(-2, 8),
    engine::utils::FixedU88::div(2, 8),
    engine::utils::FixedU88::div(9, 8),
};

//

constexpr uint32_t paddle_speed = 1;
constexpr uint32_t paddle_width = 32;
constexpr uint32_t paddle_height = 8;
constexpr uint8_t paddle_y = engine::graphics::SCREEN_HEIGHT - wall_padding_y - 4; // - paddle_height;
constexpr auto paddle_hit_boost = engine::utils::FixedS1616::div(1, 3);

struct Paddle {
    uint8_t x;

    constexpr auto & bitmap() {
        return engine::graphics::bitmap_0;
    }

    void reset() {
        x = (engine::graphics::SCREEN_WIDTH - paddle_width) / 2;
        bitmap().position_x() = x;
    }

    constexpr engine::utils::AABB aabb() const {
        return {
            x, paddle_y,
            paddle_width, paddle_height,
        };
    }
};
Paddle s_paddle;

//

struct alignas(4) Block {
    uint8_t x;
    uint8_t y;
    uint8_t sprite_id;

    auto aabb() const {
        return engine::utils::AABB{
            x, y,
            block_width, block_height,
        };
    }
};
engine::utils::Vector<Block, block_sprite_count> s_blocks;

//

enum class LevelState {
    Holding,
    Playing,
    Text,
} s_level_state;

// Got the names of these wrong - UI now controls what's going on...
enum class UIState {
    Intro,
    Playing1,

    Win1_0,
    Win1_1,
    Win1_2,
    Win1_3,
    Win1_4,
    Win1_5,
    Win1_6,
    Win1_7,
    Win1_8,
    Win1_9,
    Win1_10,
    Playing2,

    Win2_0,
    Win2_1,
    Win2_2,
    Win2_3,
    Win2_4,
    Playing3,

    Win3_0,
    Win3_1,
    Win3_2,
    Win3_3,
    Win3_4,
    Win3_5,
    Win3_6,

    GameOver,
} s_ui_state;

constexpr int16_t max_lives = 3;
int16_t s_lives = 0;
char s_level_char;

//

struct Lightning {
    uint16_t counter;
    uint16_t end;
    UIState next;
    static constexpr uint16_t rate = 3;

    void reset(UIState ui, uint16_t frames) {
        counter = 0;
        end = frames * rate;
        next = ui;
    }

    bool tick() {
        counter += rate;
        const uint8_t g = engine::utils::max(31 - counter, 0);
        engine::graphics::set_palette_colour(bg_pal_outer, RGB555(g, g, g));
        return counter >= end;
    }
} s_lightning;

void ui_redraw();
void ui_advance(UIState ui);

//

void ball_update() {
    PROFILE_SCOPE(bl_phy);

    auto & ball = s_ball;

    // Bounds check walls.
    const uint16_t nx = (ball.x + ball.vx).value() - wall_padding_x; // unsigned to wrap
    const uint16_t ny = (ball.y + ball.vy).value() - wall_padding_y; // unsigned to wrap
    if (nx >= (engine::graphics::SCREEN_WIDTH - 2 * wall_padding_x - ball_width)) {
        const bool left_side = nx > engine::graphics::SCREEN_WIDTH * 2;
        ball.bounce_x(left_side);
    }
    if (ny >= (engine::graphics::SCREEN_HEIGHT - 2 * wall_padding_y - ball_height)) {
        const bool top_side = ny > engine::graphics::SCREEN_WIDTH * 2;
        if (top_side) {
            ball.bounce_y(top_side);
        } else {
            // We hit the floor, lose a life.
            if (s_lives-- == 0) {
                ui_advance(UIState::GameOver);
            } else {
                s_level_state = LevelState::Holding;
                ui_redraw();
            }
        }
    }

    // Apply movement.
    ball.x += ball.vx;
    ball.y += ball.vy;

    // Apply rotation.
    ball.rot_speed = engine::utils::clamp<int8_t>(ball.rot_speed, 0, engine::utils::size(s_ball_speeds) - 1);
    ball.frame += s_ball_speeds[ball.rot_speed];
}

void ball_draw() {
    using namespace engine::graphics;

    PROFILE_SCOPE(bl_drw);

    static_assert(ball_tile_count % ball_sprite_count == 0, "Each part needs a tile");
    static_assert((ball_tile_count & (ball_tile_count - 1)) == 0, "Power of 2");

    uint8_t rot_frame = s_ball.frame.value();
    // Round down to multiple of sprite count since data is stored a full quad at a time.
    rot_frame &= ~(ball_sprite_count - 1);
    const uint16_t x = s_ball.x.value();
    const uint16_t y = s_ball.y.value();

    ObjSprite sprite;
    sprite.set_size(SpriteSize::Size8x8);

    // Draw the parts.
    for (uint8_t i = 0; i < ball_sprite_count; i++) {
        const uint16_t dx = (i & 1) ? engine::graphics::bg_tile_size : 0;
        const uint16_t dy = (i & 2) ? engine::graphics::bg_tile_size : 0;
        sprite.set_x(x + dx);
        sprite.set_y(y + dy);
        sprite.set_tile_index(ball_tile_start + ((rot_frame + i) % ball_tile_count));
        set_sprite(ball_sprite_start + i, sprite);
    }
}

void ball_reset_to_paddle() {
    s_ball.x = engine::utils::FixedS1616::from(s_paddle.x + paddle_width / 2 - ball_width / 2);
    s_ball.y = engine::utils::FixedS1616::from(paddle_y - ball_height);
    s_ball.vx = engine::utils::FixedS1616::from(0);
    s_ball.vy = engine::utils::FixedS1616::from(0);
    s_ball.frame = engine::utils::FixedU88::from(0);
    s_ball.rot_speed = engine::utils::size(s_ball_speeds) / 2;
}

void ball_launch() {
    int16_t angle = engine::utils::g_rng() & 0x3F; // 1 quadrant
    angle -= 0x20; // rotate to point up
    s_ball.vx = engine::utils::FixedS1616::div(engine::maths::sin(angle), 128);
    s_ball.vy = engine::utils::FixedS1616::div(engine::maths::cos(angle), 128);
}

void ball_setup() {
    PROFILE_SCOPE(bl_set);

    // Reset the ball.
    ball_reset_to_paddle();

    // Copy the tile data for the ball.
    game::images::copy_tile_data<
        pal_ball_start, pal_ball_count,
        ball_tile_start, ball_tile_count,
        game::images::bucko_ball
    >();

    // Draw it.
    ball_draw();
}

//

constexpr uint16_t block_palette[] {
    RGB555(31, 0, 0),
    RGB555(0, 31, 0),
    RGB555(0, 0, 31),
    RGB555(31, 31, 0),
    RGB555(31, 0, 31),
    RGB555(0, 31, 31),
};
static_assert(engine::utils::size(block_palette) == block_pal_count);

void blocks_setup() {
    PROFILE_SCOPE(bk_set);

    // Clear out the tile.
    uint8_t tile_data[images::TileSize];
    engine::utils::fast_memset8(tile_data, engine::graphics::pal_transparent, sizeof(tile_data));

    // One tile per colour.
    static_assert(block_tile_count == block_pal_count);
    for (int idx = 0; idx < block_pal_count; idx++) {
        const uint8_t pal_idx = block_pal_start + idx;

        // Set the colour.
        engine::graphics::set_palette_colour(pal_idx, block_palette[idx]);

        // Draw the colour for this tile.
        for (uint32_t y = 0; y < block_height; y++) {
            auto *line_data = tile_data + y * engine::graphics::bg_tile_size;
            engine::utils::fast_memset8(line_data, pal_idx, block_width);
        }

        // Copy it to VRAM.
        auto * dst = engine::graphics::get_tile_data(block_tile_start + idx);
        engine::utils::fast_memcpy(dst, tile_data, sizeof(tile_data));
    }
}

constexpr uint8_t grid_start_y = wall_padding_y * 2;

struct BlockDef { uint8_t x, y, tile; };
constexpr auto make_grid_1() {
    engine::utils::Array<BlockDef, block_sprite_count> grid{};
    uint8_t sprite_id = 0;
    for (uint8_t j = 0; j < block_pal_count; j++) { // one row per colour
        constexpr uint8_t block_x_spacing = 2;
        constexpr uint8_t block_y_spacing = 2;
        constexpr uint8_t row_len = block_sprite_count / block_pal_count;
        constexpr uint8_t start_x = engine::graphics::SCREEN_WIDTH / 2 - row_len * (block_width + block_x_spacing) / 2;
        for (uint8_t i = 0; i < row_len; i++) {
            const uint8_t x = start_x + i * (block_width + block_x_spacing);
            const uint8_t y = grid_start_y + j * (block_height + block_y_spacing);
            static_assert(block_tile_count == block_pal_count);
            const uint8_t tile_id = block_tile_start + j;
            grid[sprite_id++] = {x, y, tile_id};
        }
    }
    ASSERT(sprite_id == grid.size());
    return grid;
}
constexpr auto grid_layout_1 = make_grid_1();

constexpr auto make_grid_2() {
    const uint8_t widths[] = { 3, 5, 7, 7, 7, 5, 3 };
    constexpr uint8_t total = 37;
    static_assert(block_sprite_count >= total, "Need to redesign widths");

    engine::utils::Array<BlockDef, total> grid{};

    uint8_t sprite_id = 0;
    for (uint8_t j = 0; j < engine::utils::size(widths); j++) {
        constexpr uint8_t block_x_spacing = 4;
        constexpr uint8_t block_y_spacing = 4;
        const uint8_t row_len = widths[j];
        const uint8_t start_x = engine::graphics::SCREEN_WIDTH / 2 - row_len * (block_width + block_x_spacing) / 2;
        for (uint8_t i = 0; i < row_len; i++) {
            const uint8_t x = start_x + i * (block_width + block_x_spacing);
            const uint8_t y = grid_start_y + j * (block_height + block_y_spacing);
            const uint8_t ii = (i >= row_len / 2) ? row_len - i - 1: i;
            const uint8_t jj = (j >= 3) ? 6 - j : j;
            uint8_t tile_id = engine::utils::min(ii, jj);
            tile_id += block_tile_start + 2;
            grid[sprite_id++] = {x, y, tile_id};
        }
    }
    ASSERT(sprite_id == grid.size());
    return grid;
}
constexpr auto grid_layout_2 = make_grid_2();

constexpr uint8_t grid_3_y_spacing = 4;
constexpr uint16_t grid_3_trapped_x_start = engine::graphics::SCREEN_WIDTH / 2 - engine::graphics::bg_tile_size;
constexpr uint16_t grid_3_trapped_y_start = grid_start_y + (block_height + grid_3_y_spacing) * 2;
constexpr auto make_grid_3() {
    const uint8_t widths[] = { 6, 6, 2, 2, 2, 2 };
    constexpr uint8_t total = 40;
    static_assert(block_sprite_count >= total, "Need to redesign shape");

    engine::utils::Array<BlockDef, 40> grid{};

    uint8_t sprite_id = 0;
    for (uint8_t j = 0; j < engine::utils::size(widths); j++) {
        constexpr uint8_t block_x_spacing = 4;
        constexpr uint8_t block_y_spacing = grid_3_y_spacing;
        const uint8_t row_len = widths[j];
        const uint8_t start_x = engine::graphics::SCREEN_WIDTH / 2 - widths[0] * (block_width + block_x_spacing) / 2;
        const uint8_t end_x = start_x + (widths[0] - 1) * (block_width + block_x_spacing);
        constexpr uint8_t end_y = grid_start_y + 7 * (block_height + block_y_spacing);
        for (uint8_t i = 0; i < row_len; i++) {
            const uint8_t x = start_x + i * (block_width + block_x_spacing);
            const uint8_t y = grid_start_y + j * (block_height + block_y_spacing);
            const uint8_t rx = end_x - i * (block_width + block_x_spacing);
            const uint8_t ry = end_y - j * (block_height + block_y_spacing);
            const uint8_t tile_id = block_tile_start;
            grid[sprite_id++] = {x, y, tile_id};
            grid[sprite_id++] = {rx, ry, tile_id}; // rotated version
        }
    }
    ASSERT(sprite_id == grid.size());
    return grid;
}
constexpr auto grid_layout_3 = make_grid_3();

void blocks_create() {
    PROFILE_SCOPE(bk_rec);

    s_blocks.clear();

    // Setup block locations.
    uint8_t sprite_id = 0;
    auto add_block = [&](const BlockDef & def) {
        // Add the block to the active set.
        Block & block = s_blocks.push_back({});
        block.x = def.x;
        block.y = def.y;
        block.sprite_id = block_sprite_start + sprite_id++;

        // Draw it now. We'll erase it when it's destroyed.
        engine::graphics::ObjSprite sprite;
        sprite.set_size(engine::graphics::SpriteSize::Size8x8);
        sprite.set_tile_index(def.tile);
        sprite.set_x(block.x);
        sprite.set_y(block.y);
        engine::graphics::set_sprite(block.sprite_id, sprite);
    };

    switch (s_ui_state) {
        case UIState::Intro:
            for (const auto & pos : grid_layout_1) {
                add_block(pos);
            }
            break;
        case UIState::Playing2:
            for (const auto & pos : grid_layout_2) {
                add_block(pos);
            }
            break;
        case UIState::Playing3:
            for (const auto & pos : grid_layout_3) {
                add_block(pos);
            }
            // Add the trapped bucko too.
            for (uint8_t i = 0; i < trapped_sprite_count; i++) {
                engine::graphics::ObjSprite sprite;
                const uint16_t dx = (i & 1) ? engine::graphics::bg_tile_size : 0;
                const uint16_t dy = (i & 2) ? engine::graphics::bg_tile_size : 0;
                sprite.set_x(grid_3_trapped_x_start + dx);
                sprite.set_y(grid_3_trapped_y_start + dy);
                sprite.set_tile_index(ball_tile_start + i);
                set_sprite(trapped_sprite_start + i, sprite);
            }
            break;

        default:
            ASSERT(false);
            break;
    }
}

void blocks_update() {
    PROFILE_SCOPE(bk_upd);

    const auto ball_aabb = s_ball.aabb();

    // Check for collisions.
    // TODO: quadtree or something, but this performs well enough already
    int16_t hit_idx = -1;
    uint32_t min_dist2 = (ball_width + block_width) * (ball_width + block_width) / 4; // for a circle
    const uint32_t num_blocks = s_blocks.size();
    for (uint16_t idx = 0; idx < num_blocks; idx++) {
        const auto & block = s_blocks[idx];
        const auto block_aabb = block.aabb();
        if (ball_aabb.intersects(block_aabb)) {
            const int16_t dx = ball_aabb.center_x() - block_aabb.center_x();
            const int16_t dy = ball_aabb.center_y() - block_aabb.center_y();
            const uint32_t dist2 = bios_mathMulS16(dx, dx) + bios_mathMulS16(dy, dy);
            if (dist2 <= min_dist2) {
                hit_idx = idx;
                min_dist2 = dist2;
            }
        }
    }

    // Handle the collision with the closest.
    if (hit_idx != -1) {
        // Work out which side we bounce from.
        const auto block = s_blocks[hit_idx];
        const auto block_aabb = block.aabb();
        const auto dx = ball_aabb.center_x() - block_aabb.center_x();
        const auto dy = ball_aabb.center_y() - block_aabb.center_y();
        if (engine::utils::abs(dx) * (block_height + ball_height) > engine::utils::abs(dy) * (block_width + ball_width)) {
            s_ball.bounce_x(dx > 0);
        } else {
            s_ball.bounce_y(dy > 0);
        }

        // Kill it.
        engine::graphics::ObjSprite sprite;
        engine::graphics::set_sprite(block.sprite_id, sprite);

        // Knock this one off.
        s_blocks.remove_fast(hit_idx);

        // Play a sound.
        engine::sound::play_effect(music::SoundEffect::SE_Breakout_Hit);

        if (s_blocks.empty()) {
            // Cleared the level.
            switch (s_ui_state) {
                case UIState::Playing1:
                    ui_advance(UIState::Win1_0);
                    break;
                case UIState::Playing2:
                    ui_advance(UIState::Win2_0);
                    break;
                case UIState::Playing3:
                    // Have to hit the trapped bucko instead, see below.
                    //ui_advance(UIState::Win3_0);
                    break;

                default:
                    ASSERT(false);
                    break;
            }
        }
    }

    if (s_ui_state == UIState::Playing3) {
        constexpr engine::utils::AABB trapped_aabb{
            grid_3_trapped_x_start, grid_3_trapped_y_start,
            ball_width, ball_height,
        };
        if (ball_aabb.intersects(trapped_aabb)) {
            ui_advance(UIState::Win3_0);
        }
    }
}

//

void paddle_setup() {
    using namespace engine::graphics;

    PROFILE_SCOPE(pd_set);

    constexpr uint16_t paddle_vram_x = 0;
    constexpr uint16_t paddle_vram_y = 0;

    // Setup data in VRAM.
    // TODO: proper image here
    set_palette_colour(paddle_pal_start, RGB555(31, 31, 31));
    for (uint16_t y = 0; y < paddle_height; y++) {
        uint8_t *row_data = VDP.BITMAP_VRAM_8BIT + (paddle_vram_y + y) * SCREEN_WIDTH;
        engine::utils::fast_memset8(row_data + paddle_vram_x, paddle_pal_start, paddle_width);
    }

    // Setup the bitmap.
    auto & bitmap = s_paddle.bitmap();
    bitmap.position_y() = paddle_y;
    bitmap.scroll_x() = 0;
    bitmap.scroll_y() = 0;
    bitmap.width() = paddle_width;
    bitmap.height() = paddle_height;
    bitmap.latch() = 0;

    // Reset paddle position.
    s_paddle.reset();
}

void paddle_update() {
    PROFILE_SCOPE(pd_upd);

    // React to inputs.
    const uint16_t held = engine::input::g_buttons_held;
    const uint16_t pressed = engine::input::g_buttons_pressed;
    if (held & GAMEPAD_BTN_LEFT) {
        s_paddle.x = engine::utils::clamp<uint8_t>(
            s_paddle.x - paddle_speed,
            wall_padding_x,
            engine::graphics::SCREEN_WIDTH - wall_padding_x - paddle_width
        );
    } else if (held & GAMEPAD_BTN_RIGHT) {
        s_paddle.x = engine::utils::clamp<uint8_t>(
            s_paddle.x + paddle_speed,
            wall_padding_x,
            engine::graphics::SCREEN_WIDTH - wall_padding_x - paddle_width
        );
    }

    // Move the paddle.
    s_paddle.bitmap().position_x() = s_paddle.x;

    // per-state functionality.
    switch (s_level_state) {
        case LevelState::Holding: {
            // Keep the ball up with the paddle.
            ball_reset_to_paddle();

            // Trigger the launch.
            if (pressed & GAMEPAD_BTN_A) {
                ball_launch();
                s_level_state = LevelState::Playing;
            }
        } break;

        case LevelState::Playing: {
            // Bounce the ball.
            const auto ball_aabb = s_ball.aabb();
            const auto paddle_aabb = s_paddle.aabb();
            if (ball_aabb.intersects(paddle_aabb)) {
                s_ball.bounce_y(false);

                // Add some speed to the ball.
                if (held & GAMEPAD_BTN_LEFT) {
                    s_ball.vx -= paddle_hit_boost;
                } else if (held & GAMEPAD_BTN_RIGHT) {
                    s_ball.vx += paddle_hit_boost;
                }
            }
        } break;

        case LevelState::Text:
            break;
    }
}

//

static char s_lives_text[] = "Buckos 0";
static char s_level_text[] = "Level 0";
void ui_redraw() {
    using namespace engine::graphics;

    game::font::clear_text();

    // Always draw the lives counter.
    s_lives_text[7] = '0' + engine::utils::max<int16_t>(s_lives, 0);
    s_level_text[6] = s_level_char;
    game::font::write_left(s_lives_text, 1, 1);
    game::font::write_right(s_level_text, 1, 1);

    constexpr uint8_t text_padding = 10;

    switch (s_ui_state) {
        case UIState::Intro:
            game::font::write_centered("Press A to launch a bucko", SCREEN_HEIGHT / 2);
            break;

        case UIState::Win1_0:
        case UIState::Win1_1:
        case UIState::Win2_0:
        case UIState::Win2_1:
        case UIState::Win3_0:
        case UIState::Win3_1:
            // Timed
            break;

        case UIState::Win1_2:
            game::font::write_right("What was that?", text_padding, SCREEN_HEIGHT / 2);
            break;
        case UIState::Win1_3:
            game::font::write_left("What was what?", text_padding, SCREEN_HEIGHT / 2);
            break;
        case UIState::Win1_4:
            game::font::write_right("That flash", text_padding, SCREEN_HEIGHT / 2);
            break;
        case UIState::Win1_5:
            game::font::write_left("Oh", text_padding, SCREEN_HEIGHT / 2);
            break;
        case UIState::Win1_6:
            game::font::write_left("...", text_padding, SCREEN_HEIGHT / 2);
            break;
        case UIState::Win1_7:
            game::font::write_left("I dunno", text_padding, SCREEN_HEIGHT / 2);
            break;
        case UIState::Win1_8:
            game::font::write_left("But the timing", text_padding, SCREEN_HEIGHT / 2);
            game::font::write_left("matched my game", text_padding, SCREEN_HEIGHT / 2 + text_padding);
            break;
        case UIState::Win1_9:
            game::font::write_left("It was pretty", text_padding, SCREEN_HEIGHT / 2);
            game::font::write_left("sick", text_padding, SCREEN_HEIGHT / 2 + text_padding);
            break;
        case UIState::Win1_10:
            game::font::write_right("Sure", text_padding, SCREEN_HEIGHT / 2);
            break;

        case UIState::Win2_2:
            game::font::write_right("Again?", text_padding, SCREEN_HEIGHT / 2);
            break;
        case UIState::Win2_3:
            game::font::write_left("...", text_padding, SCREEN_HEIGHT / 2);
            break;
        case UIState::Win2_4:
            game::font::write_right("Hmm", text_padding, SCREEN_HEIGHT / 2);
            break;

        case UIState::Win3_2:
            game::font::write_right("Where did you", text_padding, SCREEN_HEIGHT / 2);
            game::font::write_right("get that game", text_padding, SCREEN_HEIGHT / 2 + text_padding);
            game::font::write_right("anyway?", text_padding, SCREEN_HEIGHT / 2 + text_padding * 2);
            break;
        case UIState::Win3_3:
            game::font::write_left("There was a cute 8", text_padding, SCREEN_HEIGHT / 2);
            game::font::write_left("legged bucko outside", text_padding, SCREEN_HEIGHT / 2 + text_padding);
            game::font::write_left("who gave it to me", text_padding, SCREEN_HEIGHT / 2 + text_padding * 2);
            break;
        case UIState::Win3_4:
            game::font::write_right("!", text_padding, SCREEN_HEIGHT / 2);
            break;
        case UIState::Win3_5:
            game::font::write_right("an 8 legged", text_padding, SCREEN_HEIGHT / 2);
            game::font::write_right("bucko?!", text_padding, SCREEN_HEIGHT / 2 + text_padding);
            break;
        case UIState::Win3_6:
            game::font::write_right("give me that!", text_padding, SCREEN_HEIGHT / 2);
            break;

        case UIState::Playing1:
        case UIState::Playing2:
        case UIState::Playing3:
            break;
        case UIState::GameOver:
            game::font::write_centered("No buckos left!", SCREEN_HEIGHT * 2 / 3);
            break;
    }
}

void ui_advance(UIState ui) {
    s_ui_state = ui;
    switch (ui) {
        case UIState::Intro:
            s_level_state = LevelState::Text;
            s_lives = max_lives;
            s_level_char = '0';
            blocks_create();
            break;

        case UIState::Playing1:
            s_level_state = LevelState::Playing; // straight into play
            s_lives = max_lives;
            s_level_char = '1';
            ball_launch();
            break;

        case UIState::Win1_0:
            s_level_state = LevelState::Text;
            s_lightning.reset(UIState::Win1_1, 10);
            break;
        case UIState::Win1_1:
            s_level_state = LevelState::Text;
            s_lightning.reset(UIState::Win1_2, 60);
            break;

        case UIState::Win2_0:
            s_level_state = LevelState::Text;
            s_lightning.reset(UIState::Win2_1, 10);
            break;
        case UIState::Win2_1:
            s_level_state = LevelState::Text;
            s_lightning.reset(UIState::Win2_2, 60);
            break;

        case UIState::Win3_0:
            s_level_state = LevelState::Text;
            s_lightning.reset(UIState::Win3_1, 10);
            break;
        case UIState::Win3_1:
            s_level_state = LevelState::Text;
            s_lightning.reset(UIState::Win3_2, 60);
            break;

        case UIState::Win1_2:
        case UIState::Win1_3:
        case UIState::Win1_4:
        case UIState::Win1_5:
        case UIState::Win1_6:
        case UIState::Win1_7:
        case UIState::Win1_8:
        case UIState::Win1_9:
        case UIState::Win1_10:
        case UIState::Win2_2:
        case UIState::Win2_3:
        case UIState::Win2_4:
        case UIState::Win3_2:
        case UIState::Win3_3:
        case UIState::Win3_4:
        case UIState::Win3_5:
        case UIState::Win3_6:
            s_level_state = LevelState::Text;
            break;

        case UIState::Playing2:
            s_level_state = LevelState::Holding;
            s_lives = max_lives;
            s_level_char = '2';
            blocks_create();
            break;

        case UIState::Playing3:
            s_level_state = LevelState::Holding;
            s_lives = max_lives;
            s_level_char = 'X';
            blocks_create();
            break;

        case UIState::GameOver:
            s_level_state = LevelState::Text;
            break;
    }
    ui_redraw();
}

Entry ui_update() {
    const uint16_t pressed = engine::input::g_buttons_pressed;
    if (pressed & GAMEPAD_BTN_A) {
        switch (s_ui_state) {
            case UIState::Intro: ui_advance(UIState::Playing1); break;

            case UIState::Win1_0: break; // timed below
            case UIState::Win1_1: break; // timed below
            case UIState::Win1_2: ui_advance(UIState::Win1_3); break;
            case UIState::Win1_3: ui_advance(UIState::Win1_4); break;
            case UIState::Win1_4: ui_advance(UIState::Win1_5); break;
            case UIState::Win1_5: ui_advance(UIState::Win1_6); break;
            case UIState::Win1_6: ui_advance(UIState::Win1_7); break;
            case UIState::Win1_7: ui_advance(UIState::Win1_8); break;
            case UIState::Win1_8: ui_advance(UIState::Win1_9); break;
            case UIState::Win1_9: ui_advance(UIState::Win1_10); break;
            case UIState::Win1_10: ui_advance(UIState::Playing2); break;

            case UIState::Win2_0: break; // timed below
            case UIState::Win2_1: break; // timed below
            case UIState::Win2_2: ui_advance(UIState::Win2_3); break;
            case UIState::Win2_3: ui_advance(UIState::Win2_4); break;
            case UIState::Win2_4: ui_advance(UIState::Playing3); break;

            case UIState::Win3_0: break; // timed below
            case UIState::Win3_1: break; // timed below
            case UIState::Win3_2: ui_advance(UIState::Win3_3); break;
            case UIState::Win3_3: ui_advance(UIState::Win3_4); break;
            case UIState::Win3_4: ui_advance(UIState::Win3_5); break;
            case UIState::Win3_5: ui_advance(UIState::Win3_6); break;
            case UIState::Win3_6: return Entry::Driving;

            case UIState::Playing1:
            case UIState::Playing2:
            case UIState::Playing3:
                ASSERT(false);
                break;

            case UIState::GameOver: return Entry::MainMenu;
        }
    }

    // Lightning effect.
    switch (s_ui_state) {
        case UIState::Win1_0:
        case UIState::Win1_1:
        case UIState::Win2_0:
        case UIState::Win2_1:
        case UIState::Win3_0:
        case UIState::Win3_1:
            if (s_lightning.tick()) {
                ui_advance(s_lightning.next);
            }
            break;
        default:
            break;
    }

    return Entry::Breakout;
}

//

void background_setup() {
    using namespace engine::graphics;
    PROFILE_SCOPE(bg_set);

    auto & bg0 = background_0;

    // TODO: proper tile data
    auto set_tile = [](int idx, uint16_t rgb) {
        const uint8_t pal = idx - bg_tile_start + bg_pal_start;
        set_palette_colour(pal, rgb);
        auto * dst = get_tile_data(idx);
        engine::utils::fast_memset8(dst, pal, bg_tile_size * bg_tile_size);
    };
    set_tile(bg_tile_inner, RGB555(5, 5, 5));
    set_tile(bg_tile_border, RGB555(17, 6, 22));
    set_tile(bg_tile_outer, RGB555(0, 0, 0));

    constexpr uint8_t border_x = wall_padding_x / bg_tile_size - 1;
    constexpr uint8_t border_y = wall_padding_y / bg_tile_size - 1;

    // Above and below play area.
    BGSprite sprite;
    sprite.set_tile_index(bg_tile_outer);
    for (uint32_t y = 0; y < border_y; y++) {
        for (uint32_t x = 0; x < bg_tilemap_size; x++) {
            bg0.set_sprite(x, y, sprite);
            bg0.set_sprite(x, bg_tilemap_size - y - 1, sprite);
        }
    }
    sprite.set_tile_index(bg_tile_border);
    for (uint32_t x = border_x; x < bg_tilemap_size - border_x - 1; x++) {
        constexpr uint32_t y = border_y;
        bg0.set_sprite(x, y, sprite);
        bg0.set_sprite(x, bg_tilemap_size - y - 1, sprite);
    }

    // Left and right.
    sprite.set_tile_index(bg_tile_outer);
    for (uint32_t y = border_y; y < bg_tilemap_size - border_y - 1; y++) {
        for (uint32_t x = 0; x < border_x; x++) {
            bg0.set_sprite(x, y, sprite);
            bg0.set_sprite(bg_tilemap_size - x - 1, y, sprite);
        }
    }
    sprite.set_tile_index(bg_tile_border);
    for (uint32_t y = border_y; y < bg_tilemap_size - border_y - 1; y++) {
        constexpr uint32_t x = border_x;
        bg0.set_sprite(x, y, sprite);
        bg0.set_sprite(bg_tilemap_size - x - 1, y, sprite);
    }

    // Center.
    sprite.set_tile_index(bg_tile_inner);
    for (uint32_t y = border_y + 1; y < bg_tilemap_size - border_y - 1; y++) {
        for (uint32_t x = border_x + 1; x < bg_tilemap_size - border_x - 1; x++) {
            bg0.set_sprite(x, y, sprite);
        }
    }
}

} // namespace

//

void enter() {
    // Setup colours.
    engine::graphics::set_backdrop_a(RGB555(0, 0, 0));
    engine::graphics::set_palette_colour(pal_white, RGB555(31, 31, 31));
    engine::graphics::set_palette_colour(pal_grey, RGB555(15, 15, 15));

    // Draw a background.
    background_setup();

    // Setup the parts.
    paddle_setup();
    blocks_setup();
    ball_setup();
    game::font::setup_tiles<font_tile_start, font_sprite_start>();

    // Kick off the UI state machine.
    ui_advance(UIState::Intro);

    // This screen uses sprites and has a background.
    bios_vsync();
    engine::graphics::enable_sprites();
    engine::graphics::bitmap_0.enable();
    engine::graphics::background_0.enable();

    // Kick off the bgm.
    engine::sound::play_bgm(game::music::Bgm::Bgm_Breakout);

    engine::profiler::print_timings();
}

void leave() {
    // Reset graphics state.
    bios_vsync();
    engine::graphics::disable_sprites();
    engine::graphics::bitmap_0.disable();
    engine::graphics::background_0.disable();
    engine::graphics::reset_sprites<block_sprite_start + block_sprite_count>();
    font::clear_text();

    // Reset sound.
    engine::sound::stop_bgm();
}

Entry loop() {
    Entry next = Entry::Breakout;

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

    if (s_level_state == LevelState::Text) {
        next = ui_update();
    } else {
        // Paddle is controllable.
        paddle_update();

        // Update ball.
        ball_update();
        blocks_update();
    }
    ball_draw();

    engine::profiler::print_timings();
    return next;
}

} // namespace game::breakout
