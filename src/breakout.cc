#include "fixed.h"
#include "game.h"
#include "input.h"
#include "graphics.h"
#include "memory.h"
#include "profiler.h"
#include "utils.h"

namespace game {

namespace {

PROFILE_STORAGE(bl_phy);
PROFILE_STORAGE(bl_drw);
PROFILE_STORAGE(bl_set);
PROFILE_STORAGE(vsync);

constexpr uint8_t pal_white = 1;
constexpr uint8_t pal_grey = 2;
constexpr uint8_t pal_ball_start = 3;
constexpr uint8_t pal_ball_count = 16; // TODO: less colours

constexpr uint8_t ball_sprite_start = 0;
constexpr uint8_t ball_sprite_count = 4; // big quad - TODO: look into 32x32 tile instead
constexpr uint8_t ball_tile_start = 0;
constexpr uint8_t ball_tile_count = 16; // animation frames

constexpr uint8_t block_sprite_start = ball_sprite_start + ball_sprite_count;
constexpr uint8_t block_sprite_count = 8 * 4;
constexpr uint8_t block_tile_start = 0;
constexpr uint8_t block_tile_count = 4; // num colours

constexpr uint32_t wall_padding = engine::graphics::bg_tile_size;

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
};
Ball s_ball;

constexpr engine::utils::FixedU88 s_ball_speeds[] = {
    engine::utils::FixedU88::div(-7, 8),
    engine::utils::FixedU88::div(-5, 8),
    engine::utils::FixedU88::div(-3, 8),
    engine::utils::FixedU88::div(-1, 8),
    engine::utils::FixedU88::div(1, 8),
    engine::utils::FixedU88::div(3, 8),
    engine::utils::FixedU88::div(5, 8),
    engine::utils::FixedU88::div(7, 8),
};

#if 0
constexpr uint8_t block_destroyed_y = 255; // move it off screen to destroy it
union Block {
    uint16_t raw; // enforce word read
    struct {
        uint8_t x;
        uint8_t y;
    } parts;
};
Block s_blocks[block_sprite_count];
#endif

//

void ball_update() {
    PROFILE_SCOPE(bl_phy);

    auto & ball = s_ball;

    // Bounds check walls.
    const uint16_t nx = (ball.x + ball.vx).value() - wall_padding; // unsigned to wrap
    const uint16_t ny = (ball.y + ball.vy).value() - wall_padding; // unsigned to wrap
    if (nx >= (engine::graphics::SCREEN_WIDTH - 2 * wall_padding - ball_width)) {
        // Increase/decrease if going same/opposite direction
        const bool left_side = nx > engine::graphics::SCREEN_WIDTH * 2;
        const bool moving_up = ball.vy.value() < 0;
        if (left_side == moving_up) ball.rot_speed++; else ball.rot_speed--;

        ball.vx = -ball.vx;
    }
    if (ny >= (engine::graphics::SCREEN_HEIGHT - 2 * wall_padding - ball_height)) {
        // Increase/decrease if going same/opposite direction
        const bool top_side = ny > engine::graphics::SCREEN_WIDTH * 2;
        const bool moving_left = ball.vx.value() < 0;
        if (top_side != moving_left) ball.rot_speed++; else ball.rot_speed--;

        ball.vy = -ball.vy;
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

    const uint8_t rot_frame = ball_tile_start + s_ball.frame.value();
    const uint16_t x = s_ball.x.value();
    const uint16_t y = s_ball.y.value();

    ObjSprite sprite;
    sprite.set_size(SpriteSize::Size8x8);

    // Draw the parts.
    //  0 1
    //  3 2
    constexpr uint8_t idxs[4] = { 0, 1, 3, 2 };
    static_assert(ball_sprite_count <= 4);
    for (uint8_t i = 0; i < ball_sprite_count; i++) {
        const uint16_t dx = (i & 1) ? engine::graphics::bg_tile_size : 0;
        const uint16_t dy = (i & 2) ? engine::graphics::bg_tile_size : 0;
        const uint8_t idx = idxs[i];
        const uint8_t frame_idx = idx * (ball_tile_count / ball_sprite_count);
        sprite.set_x(x + dx);
        sprite.set_y(y + dy);
        sprite.set_tile_index((rot_frame + frame_idx) % ball_tile_count);
        set_sprite(ball_sprite_start + idx, sprite);
    }
}

void ball_reset() {
    s_ball.x = engine::utils::FixedS1616::from(engine::graphics::SCREEN_WIDTH / 2);
    s_ball.y = engine::utils::FixedS1616::from(engine::graphics::SCREEN_HEIGHT / 2);
    s_ball.vx = engine::utils::FixedS1616::div(3, 2);
    s_ball.vy = engine::utils::FixedS1616::div(3, 2);
    s_ball.frame = engine::utils::FixedU88::from(0);
    s_ball.rot_speed = engine::utils::size(s_ball_speeds) / 2;
}

// TODO: generate these from an image
/*const*/ uint8_t s_ball_sprites[ball_tile_count][engine::graphics::bg_tile_size * engine::graphics::bg_tile_size] {

};

void ball_setup() {
    using namespace engine::graphics;

    PROFILE_SCOPE(bl_set);

    // Reset the ball.
    ball_reset();

    // Copy each frame to a tile.
    for (int idx = 0; idx < ball_tile_count; idx++) {
        auto & tile = s_ball_sprites[idx];
        static_assert(sizeof(tile) > 16); // sanity check it's not decayed

        // TODO: temp to show it's working
        static_assert(ball_tile_count == pal_ball_count);
        engine::graphics::set_palette_colour(pal_ball_start + idx, RGB555(idx+1, idx+1, idx+1));
        engine::utils::fast_memset8(tile, pal_ball_start + idx, sizeof(tile));

        engine::utils::fast_memcpy(engine::graphics::get_tile_data(ball_tile_start + idx), tile, sizeof(tile));
    }

    // Draw it.
    ball_draw();
}

//

void enter() {
    // Setup colours.
    engine::graphics::set_backdrop_a(RGB555(0, 0, 0));
    engine::graphics::set_palette_colour(pal_white, RGB555(31, 31, 31));
    engine::graphics::set_palette_colour(pal_grey, RGB555(15, 15, 15));

    // Draw a background.
    //background_setup(); // TODO

    // Setup the ball.
    ball_setup();

    // This screen uses sprites and has a background.
    engine::graphics::enable_sprites();
    //engine::graphics::background_0.enable();

    engine::profiler::print_timings();
}

void leave() {
    // Reset graphics state.
    engine::graphics::disable_sprites();
    //engine::graphics::background_0.disable();
    engine::graphics::reset_sprites(block_sprite_start + block_sprite_count);
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

        // Handle the input.
        const auto pressed = engine::input::g_buttons_pressed;
        if (pressed & GAMEPAD_BTN_A) {
            // TODO
            next = Entry::Driving;
            break;
        }

        // Update ball.
        ball_update();
        ball_draw();

        engine::profiler::print_timings();
    }

    leave();
    return next;
}

} // namespace game
