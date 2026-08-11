#include "debug.h"
#include "game.h"
#include "images.h"
#include "input.h"
#include "graphics.h"
#include "maths.h"
#include "memory.h"
#include "profiler.h"
#include "fixed.h"

namespace game {

namespace {

PROFILE_STORAGE(b_phys);
PROFILE_STORAGE(b_sprt);
PROFILE_STORAGE(b_tile);
PROFILE_STORAGE(bg_set);
PROFILE_STORAGE(bg_upd);
PROFILE_STORAGE(vsync);

constexpr uint8_t pal_white = 1;
constexpr uint8_t pal_grey = 2;
constexpr uint8_t pal_bouncer_start = 3;
constexpr uint8_t pal_bouncer_count = 16;

constexpr uint8_t mouse_sprite_idx = 0;

constexpr uint8_t bouncer_sprite_start = mouse_sprite_idx + 1;
constexpr uint8_t bouncer_count = 24;
constexpr uint8_t bouncer_tile_start = 0;
constexpr uint8_t bouncer_tile_count = 8;
struct Bouncer {
    engine::utils::FixedS1616 x;
    engine::utils::FixedS1616 y;
    // TODO: store angles instead?
    engine::utils::FixedS1616 vx;
    engine::utils::FixedS1616 vy;
    engine::utils::FixedU88 rot;
    engine::utils::FixedU88 avel;
};
Bouncer s_bouncers[bouncer_count];

void bouncers_update() {
    using namespace engine::graphics;

    // Physics.
    for (int idx = 0; idx < bouncer_count; idx++) {
        PROFILE_SCOPE(b_phys);

        auto & bouncer = s_bouncers[idx];

        // Bounds check walls.
        const uint16_t nx = (bouncer.x + bouncer.vx).value(); // unsigned to wrap
        const uint16_t ny = (bouncer.y + bouncer.vy).value(); // unsigned to wrap
        if (nx >= (SCREEN_WIDTH - bg_tile_size)) {
            bouncer.vx = -bouncer.vx;
            bouncer.avel = -bouncer.avel;
        }
        if (ny >= (SCREEN_HEIGHT - bg_tile_size)) {
            bouncer.vy = -bouncer.vy;
            bouncer.avel = -bouncer.avel;
        }

        // Apply movement.
        bouncer.x += bouncer.vx;
        bouncer.y += bouncer.vy;

        // Apply rotation.
        bouncer.rot += bouncer.avel;
    }

    // Update the sprites.
    for (int idx = 0; idx < bouncer_count; idx++) {
        PROFILE_SCOPE(b_sprt);

        const auto & bouncer = s_bouncers[idx];
        const uint16_t rot_frame = bouncer.rot.value() % bouncer_tile_count;

        ObjSprite sprite;
        sprite.set_x(bouncer.x.value());
        sprite.set_y(bouncer.y.value());
        sprite.set_tile_index(bouncer_tile_start + rot_frame);
        sprite.set_size(SpriteSize::Size8x8);
        set_sprite(bouncer_sprite_start + idx, sprite);
    }
}

void bouncers_setup() {
    using namespace engine::graphics;

    constexpr uint8_t tile_size = game::images::TileSize;

    // Pick some random places.
    auto & rng = engine::utils::g_rng;
    for (int idx = 0; idx < bouncer_count; idx++) {
        auto & bouncer = s_bouncers[idx];

        // Place this one so that it's not colliding.
        while (true) {
            bouncer.x = engine::utils::FixedS1616::from(rng() % (SCREEN_WIDTH - tile_size));
            bouncer.y = engine::utils::FixedS1616::from(rng() % (SCREEN_HEIGHT - tile_size));

            bool colliding = false; // TODO
            if (!colliding) {
                break;
            }
        }

        const uint8_t angle = rng() & 0b11110000; // 16 choices
        bouncer.vx = engine::utils::FixedS1616::div(engine::maths::cos(angle), 32);
        bouncer.vy = engine::utils::FixedS1616::div(engine::maths::sin(angle), 32);
        bouncer.avel = engine::utils::FixedU88::div(1, 16);
        if (rng() & 1) bouncer.avel -= engine::utils::FixedU88::div(2, 16);
    }

    // Copy the tile data for the bouncers.
    game::images::copy_tile_data<
        pal_bouncer_start, pal_bouncer_count,
        bouncer_tile_start, bouncer_tile_count,
        game::images::mm_bouncer
    >();

    bouncers_update();
}

//

constexpr uint8_t bg_sprite_start = bouncer_sprite_start + bouncer_count;
constexpr uint8_t bg_sprite_count = engine::graphics::bg_tilemap_size * 2 - 1;

void background_setup() {
    using namespace engine::graphics;

    PROFILE_SCOPE(bg_set);

    // Point BG tiles to data.
    for (uint32_t y = 0; y < bg_tilemap_size; y++) {
        for (uint32_t x = 0; x < bg_tilemap_size; x++) {
            BGSprite sprite;
            sprite.set_tile_index(bg_sprite_start + x + y);
            set_bg_sprite<0>(x, y, sprite);
        }
    }

    // Add colours.
    for (uint32_t i = 0; i < bg_sprite_count; i++) {
        const int g = i * 31 / bg_sprite_count;
        set_palette_colour(bg_sprite_start + i, RGB555(g, g, g));
        Pixel2 * data16 = get_tile_data(bg_sprite_start + i);
        for (int y = 0; y < 8; y++) {
            for (int x = 0; x < 8; x += 2) {
                // Write 2 pixels at a time.
                Pixel2 v16 = bg_sprite_start + i;
                v16 |= v16 << 8;
                *data16++ = v16;
            }
        }
    }
}

void background_update() {
    using namespace engine::graphics;

    PROFILE_SCOPE(bg_upd);

    // TODO: make things move
}

//

void enter() {
    // Setup colours.
    engine::graphics::set_backdrop_a(RGB555(0, 0, 0));
    engine::graphics::set_palette_colour(pal_white, RGB555(31, 31, 31));
    engine::graphics::set_palette_colour(pal_grey, RGB555(15, 15, 15));

    // Draw a background.
    background_setup();

    // Setup the bouncers.
    bouncers_setup();

    // This screen uses sprites and has a background.
    bios_vsync();
    engine::graphics::enable_sprites();
    engine::graphics::background_0.enable();

    engine::profiler::print_timings();
}

void leave() {
    // Reset graphics state.
    bios_vsync();
    engine::graphics::disable_sprites();
    engine::graphics::background_0.disable();
    engine::graphics::reset_sprites(bouncer_sprite_start + bouncer_count);
}

} // namespace

Entry main_menu_loop() {
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
            next = Entry::Breakout;
            break;
        }

        // Update bouncers.
        bouncers_update();

        // Update background.
        background_update();

        engine::profiler::print_timings();
    }

    leave();
    return next;
}

} // namespace game
