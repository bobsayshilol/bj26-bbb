#include "game.h"
#include "input.h"
#include "graphics.h"
#include "maths.h"
#include "memory.h"
#include "profiler.h"

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

constexpr uint8_t mouse_sprite_idx = 0;

constexpr uint8_t bouncer_sprite_start = mouse_sprite_idx + 1;
constexpr uint8_t bouncer_count = 16;
constexpr uint8_t bouncer_tile_start = 0;
constexpr uint8_t bouncer_tile_count = 8;
constexpr uint8_t bouncer_pos_shift = 2;
constexpr uint8_t bouncer_rot_shift = 4;
struct Bouncer {
    int16_t x;
    int16_t y;
    // TODO: store angles instead?
    int8_t vx;
    int8_t vy;
    uint8_t rot;
    int8_t avel;
};
Bouncer s_bouncers[bouncer_count];

void bouncers_update() {
    using namespace engine::graphics;

    // Physics.
    for (int idx = 0; idx < bouncer_count; idx++) {
        PROFILE_SCOPE(b_phys);

        auto & bouncer = s_bouncers[idx];

        // Bounds check walls.
        const uint16_t nx = bouncer.x + bouncer.vx; // unsigned to wrap
        const uint16_t ny = bouncer.y + bouncer.vy; // unsigned to wrap
        if (nx >= (SCREEN_WIDTH - bg_tile_size) << bouncer_pos_shift) {
            bouncer.vx = -bouncer.vx;
            bouncer.avel = -bouncer.avel;
        }
        if (ny >= (SCREEN_HEIGHT - bg_tile_size) << bouncer_pos_shift) {
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
        const uint16_t rot_frame = (bouncer.rot >> bouncer_rot_shift) % bouncer_tile_count;

        ObjSprite sprite;
        sprite.set_x(bouncer.x >> bouncer_pos_shift);
        sprite.set_y(bouncer.y >> bouncer_pos_shift);
        sprite.set_tile_index(bouncer_tile_start + rot_frame);
        sprite.set_size(SpriteSize::Size8x8);
        set_sprite(bouncer_sprite_start + idx, sprite);
    }
}

void bouncers_setup() {
    using namespace engine::graphics;

    constexpr uint8_t tile_size = bg_tile_size;
    uint8_t tile_data[tile_size * tile_size];

    // Pick some random places.
    auto & rng = engine::utils::g_rng;
    for (int idx = 0; idx < bouncer_count; idx++) {
        auto & bouncer = s_bouncers[idx];

        // Place this one so that it's not colliding.
        while (true) {
            bouncer.x = (rng() % (SCREEN_WIDTH - tile_size)) << bouncer_pos_shift;
            bouncer.y = (rng() % (SCREEN_HEIGHT - tile_size)) << bouncer_pos_shift;

            bool colliding = false; // TODO
            if (!colliding) {
                break;
            }
        }

#if 1
        const uint8_t angle = rng() & 0b11100000; // 8 choices
        bouncer.vx = engine::maths::cos(angle) >> (6 - bouncer_pos_shift);
        bouncer.vy = engine::maths::sin(angle) >> (6 - bouncer_pos_shift);
#elif 0
        bouncer.vx = (1 - (idx & 1) * 2) << bouncer_pos_shift;
        bouncer.vy = (1 - (idx & 2)) << bouncer_pos_shift;
#else
        const uint8_t angle = rng();
        bouncer.vx = engine::maths::cos(angle) >> (5 - bouncer_pos_shift);
        bouncer.vy = engine::maths::sin(angle) >> (5 - bouncer_pos_shift);
#endif
        DEBUG_MSG(idx, " -> ", bouncer.vx, ", ", bouncer.vy);
        bouncer.avel = 1 - (rng() & 2);// << bouncer_rot_shift;
    }

    // TODO: these should be premade sprites
    for (int idx = 0; idx < bouncer_tile_count; idx++) {
        PROFILE_SCOPE(b_tile);

        // Clear it to white.
        engine::utils::fast_memset8(tile_data, pal_white, tile_size * tile_size);
        // Corners.
        tile_data[0 * tile_size + 0] = 0;
        tile_data[0 * tile_size + (tile_size - 1)] = 0;
        tile_data[(tile_size - 1) * tile_size + 0] = 0;
        tile_data[(tile_size - 1) * tile_size + (tile_size - 1)] = 0;

        static const int8_t angle_to_dir[] = {0, 1, 1, 1, 0, -1, -1, -1, 0, 1};
        static_assert(engine::utils::size(angle_to_dir) == bouncer_tile_count + 2);

        // Draw a line for an angle.
        int x = tile_size / 2;
        int y = tile_size / 2;
        const int dx = angle_to_dir[idx + 2];
        const int dy = angle_to_dir[idx];
        for (int i = 0; i < tile_size / 2; i++) {
            tile_data[y * tile_size + x] = pal_grey;
            x += dx;
            y += dy;
        }

        // Copy it to the tile.
        engine::utils::fast_memcpy(engine::graphics::get_tile_data(bouncer_tile_start + idx), tile_data, tile_size * tile_size);
    }

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
    engine::graphics::enable_sprites();
    engine::graphics::background_0.enable();

    engine::profiler::print_timings();
}

void leave() {
    // Reset graphics state.
    engine::graphics::disable_sprites();
    engine::graphics::background_0.disable();
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
