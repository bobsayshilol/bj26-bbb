#include "aabb.h"
#include "fixed.h"
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

namespace game {

namespace {

PROFILE_STORAGE(bl_phy);
PROFILE_STORAGE(bl_drw);
PROFILE_STORAGE(bl_set);
PROFILE_STORAGE(bk_set);
PROFILE_STORAGE(bk_upd);
PROFILE_STORAGE(pd_set);
PROFILE_STORAGE(pd_upd);
PROFILE_STORAGE(vsync);

constexpr uint8_t pal_white = 1;
constexpr uint8_t pal_grey = 2;
constexpr uint8_t pal_ball_start = 3;
constexpr uint8_t pal_ball_count = 16; // TODO: less colours

constexpr uint8_t ball_sprite_start = 0;
constexpr uint8_t ball_sprite_count = 4; // big quad - TODO: look into 32x32 tile instead
constexpr uint8_t ball_tile_start = 0;
constexpr uint8_t ball_tile_count = ball_sprite_count * 8; // animation frames

constexpr uint8_t block_pal_start = pal_ball_start + pal_ball_count;
constexpr uint8_t block_pal_count = 6; // num colours
constexpr uint8_t block_sprite_start = ball_sprite_start + ball_sprite_count;
constexpr uint8_t block_sprite_count = block_pal_count * 12; // # of each colour
constexpr uint8_t block_tile_start = ball_tile_start + ball_tile_count;
constexpr uint8_t block_tile_count = block_pal_count;

constexpr uint8_t paddle_pal_start = block_pal_start + block_pal_count;
constexpr uint8_t paddle_pal_count = 1;

//

constexpr uint32_t wall_padding = 2 * engine::graphics::bg_tile_size;
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
constexpr uint8_t paddle_y = engine::graphics::SCREEN_HEIGHT - wall_padding - paddle_height;
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

//

void ball_update() {
    PROFILE_SCOPE(bl_phy);

    auto & ball = s_ball;

    // Bounds check walls.
    const uint16_t nx = (ball.x + ball.vx).value() - wall_padding; // unsigned to wrap
    const uint16_t ny = (ball.y + ball.vy).value() - wall_padding; // unsigned to wrap
    if (nx >= (engine::graphics::SCREEN_WIDTH - 2 * wall_padding - ball_width)) {
        const bool left_side = nx > engine::graphics::SCREEN_WIDTH * 2;
        ball.bounce_x(left_side);
    }
    if (ny >= (engine::graphics::SCREEN_HEIGHT - 2 * wall_padding - ball_height)) {
        const bool top_side = ny > engine::graphics::SCREEN_WIDTH * 2;
        if (top_side) {
            ball.bounce_y(top_side);
        } else {
            // We hit the floor, game over.
            // TODO: lose a life
            s_level_state = LevelState::Holding;
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

    uint8_t rot_frame = ball_tile_start + s_ball.frame.value();
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
        sprite.set_tile_index((rot_frame + i) % ball_tile_count);
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

    s_blocks.clear();

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

    // Setup block locations.
    // TODO: different layouts
    uint8_t sprite_id = 0;
    for (uint8_t y = 0; y < block_pal_count; y++) { // one row per colour
        constexpr uint8_t row_len = block_sprite_count / block_pal_count;
        constexpr uint8_t mid_x = engine::graphics::SCREEN_WIDTH / 2 - row_len * engine::graphics::bg_tile_size / 2;
        constexpr uint8_t mid_y = engine::graphics::SCREEN_HEIGHT / 2 - block_pal_count * engine::graphics::bg_tile_size / 2;
        for (uint8_t x = 0; x < row_len; x++) {
            Block & block = s_blocks.push_back({});
            block.x = mid_x + x * engine::graphics::bg_tile_size;
            block.y = mid_y + y * engine::graphics::bg_tile_size;
            block.sprite_id = block_sprite_start + sprite_id++;

            // Draw it now. We'll erase it when it's destroyed.
            static_assert(block_tile_count == block_pal_count);
            const uint8_t tile_id = block_tile_start + y;
            engine::graphics::ObjSprite sprite;
            sprite.set_size(engine::graphics::SpriteSize::Size8x8);
            sprite.set_tile_index(tile_id);
            sprite.set_x(block.x);
            sprite.set_y(block.y);
            engine::graphics::set_sprite(block.sprite_id, sprite);
        }
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
            wall_padding,
            engine::graphics::SCREEN_WIDTH - wall_padding - paddle_width
        );
    } else if (held & GAMEPAD_BTN_RIGHT) {
        s_paddle.x = engine::utils::clamp<uint8_t>(
            s_paddle.x + paddle_speed,
            wall_padding,
            engine::graphics::SCREEN_WIDTH - wall_padding - paddle_width
        );
    }

    // Move the paddle.
    engine::graphics::bitmap_0.position_x() = s_paddle.x;

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

void enter() {
    // Setup colours.
    engine::graphics::set_backdrop_a(RGB555(0, 0, 0));
    engine::graphics::set_palette_colour(pal_white, RGB555(31, 31, 31));
    engine::graphics::set_palette_colour(pal_grey, RGB555(15, 15, 15));

    // Draw a background.
    //background_setup(); // TODO

    // Setup the parts.
    paddle_setup();
    blocks_setup();
    ball_setup();
    s_level_state = LevelState::Holding;

    // This screen uses sprites and has a background.
    bios_vsync();
    engine::graphics::enable_sprites();
    //engine::graphics::background_0.enable();
    engine::graphics::bitmap_0.enable();

    // Kick off the bgm.
    engine::sound::play_bgm(game::music::Bgm::Bgm_Breakout);

    engine::profiler::print_timings();
}

void leave() {
    // Reset graphics state.
    bios_vsync();
    engine::graphics::disable_sprites();
    //engine::graphics::background_0.disable();
    engine::graphics::bitmap_0.disable();
    engine::graphics::reset_sprites(block_sprite_start + block_sprite_count);

    // Reset sound.
    engine::sound::stop_bgm();
}

} // namespace

Entry breakout_loop() {
    enter();

    Entry next = Entry::MainMenu;
    while (true) {
        // Wait for vsync.
        {
            PROFILE_SCOPE(vsync);
            bios_vsync();
        }

        // Update gamepad/mouse input.
        engine::input::update_inputs();

        // Return to the main menu if requested.
        if (engine::input::g_buttons_pressed & GAMEPAD_BTN_START) {
            next = Entry::MainMenu;
            break;
        }

        // Paddle is controllable.
        paddle_update();

        // Update ball.
        ball_update();
        blocks_update();
        ball_draw();

        engine::profiler::print_timings();
    }

    leave();
    return next;
}

} // namespace game
