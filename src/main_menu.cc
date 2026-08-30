#include "aabb.h"
#include "debug.h"
#include "font.h"
#include "game.h"
#include "images.h"
#include "input.h"
#include "graphics.h"
#include "maths.h"
#include "memory.h"
#include "music.h"
#include "profiler.h"
#include "fixed.h"
#include "sound.h"
#include "utils.h"

namespace game::main_menu {

namespace {

PROFILE_STORAGE(b_phys);
PROFILE_STORAGE(b_sprt);
PROFILE_STORAGE(b_tile);
PROFILE_STORAGE(bg_set);
PROFILE_STORAGE(bg_upd);
PROFILE_STORAGE(men_up);
PROFILE_STORAGE(vsync);

constexpr uint8_t pal_white = 1;
constexpr uint8_t pal_grey = 2;

constexpr uint8_t mouse_pal_start = pal_grey + 1;
constexpr uint8_t mouse_pal_count = 10;
constexpr uint8_t mouse_sprite_start = 0;
constexpr uint8_t mouse_sprite_count = 1;
constexpr uint8_t mouse_tile_start = 0;
constexpr uint8_t mouse_tile_count = 1;

constexpr uint8_t pal_bouncer_start = mouse_pal_start + mouse_pal_count;
constexpr uint8_t pal_bouncer_count = 10;
constexpr uint8_t bouncer_sprite_start = mouse_sprite_start + mouse_sprite_count;
constexpr uint8_t bouncer_count = 24;
constexpr uint8_t bouncer_tile_start = mouse_tile_start + mouse_tile_count;
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

constexpr engine::utils::FixedU88 s_avel_choices[4] {
    engine::utils::FixedU88::div(9, 16),
    engine::utils::FixedU88::div(3, 16),
    engine::utils::FixedU88::div(-3, 16),
    engine::utils::FixedU88::div(-9, 16),
};

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

        const uint8_t angle = rng();
        bouncer.vx = engine::utils::FixedS1616::div(engine::maths::cos(angle), 32);
        bouncer.vy = engine::utils::FixedS1616::div(engine::maths::sin(angle), 32);

        const uint8_t avel = rng() % engine::utils::size(s_avel_choices);
        bouncer.avel = s_avel_choices[avel];
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

constexpr uint8_t bg_tile_start = bouncer_tile_start + bouncer_count;
constexpr uint8_t bg_tile_count = engine::graphics::bg_tilemap_size * 2 - 1;
constexpr uint8_t bg_sprite_start = bouncer_sprite_start + bouncer_count;
constexpr uint8_t bg_sprite_count = 0; // they're all BG sprites

constexpr uint8_t pal_bg_start = pal_bouncer_start + pal_bouncer_count;
constexpr uint8_t pal_bg_count = bg_tile_count + 1; // dummy palette at the end for power-of-two
static_assert((pal_bg_count & (pal_bg_count - 1)) == 0);

uint16_t s_bg_pal_idx;
constexpr uint8_t bg_pal_extra = 2; // so we can scroll it slower
uint16_t s_bg_pal[pal_bg_count * bg_pal_extra];
static_assert((engine::utils::size(s_bg_pal) & (engine::utils::size(s_bg_pal) - 1)) == 0);

void background_update() {
    using namespace engine::graphics;

    PROFILE_SCOPE(bg_upd);

    // Moving wave effect thing.
    uint16_t pal_idx = s_bg_pal_idx++ / bg_pal_extra;
    for (uint32_t i = 0; i < bg_tile_count; i++) {
        pal_idx = (pal_idx + 1) % engine::utils::size(s_bg_pal);
        const uint16_t pal = s_bg_pal[pal_idx];
        set_palette_colour(pal_bg_start + i, pal);
    }
}

void background_setup() {
    using namespace engine::graphics;

    PROFILE_SCOPE(bg_set);

    // Point BG tiles to data.
    for (uint32_t y = 0; y < bg_tilemap_size; y++) {
        for (uint32_t x = 0; x < bg_tilemap_size; x++) {
            BGSprite sprite;
            sprite.set_tile_index(bg_tile_start + x + y);
            set_bg_sprite<0>(x, y, sprite);
        }
    }

    // Add colours.
    for (uint32_t i = 0; i < bg_tile_count; i++) {
        Pixel2 * data16 = get_tile_data(bg_tile_start + i);
        const Pixel2 v16 = pal_bg_start + i;
        engine::utils::fast_memset16(data16, v16, 8 * 8 / 2);
    }

    // Precalculate colour palette.
    constexpr uint32_t pal_full = engine::utils::size(s_bg_pal);
    for (uint32_t i = 0; i < pal_full; i++) {
        // Wave thing.
        const uint16_t x = (2 * i >= pal_full) ? 2 * pal_full - 2 * i : 2 * i;

        // Colour split.
        constexpr uint16_t base = 19;
        constexpr uint16_t fade = 12;
        static_assert(base + fade == 31);

        // Colour to fade.
        constexpr uint16_t r = 244;
        constexpr uint16_t g = 192;
        constexpr uint16_t b = 238;

        // TODO: looks grey-ish due to quantizing
        const uint16_t rx = (base * r / 255) + bios_mathDivU16(x * (fade * r / 255), pal_full);
        const uint16_t gx = (base * g / 255) + bios_mathDivU16(x * (fade * g / 255), pal_full);
        const uint16_t bx = (base * b / 255) + bios_mathDivU16(x * (fade * b / 255), pal_full);

        s_bg_pal[i] = RGB555(rx, gx, bx);
    }

    // Draw the background once.
    background_update();
}

//

enum class MenuState {
    Warning,
    Main,
    LevelSelect,
    Options,
    Credits,
};
MenuState s_menu_state;

struct MousePos {
    int16_t x;
    int16_t y;
} s_mouse_pos;

namespace buttons {

enum class Action {
    None,
    VolUp,
    VolDown,
    UnlockAll,

    // Menus.
    MainMenu,
    LevelSelect,
    Options,
    Credits,

    // Game levels.
    Start,
    Breakout,
    Driving,
    Cyber,
};

struct Button {
    engine::utils::AABB aabb;
    const char * text;
    Action action;

    template <uint8_t N>
    constexpr Button(const char (&str)[N], int16_t y, Action act) : aabb{}, text(str), action(act) {
        const uint16_t x = engine::graphics::SCREEN_WIDTH / 2 - (N - 1) * font::CharWidth / 2;
        aabb.x = x;
        aabb.y = y;
        aabb.w = (N - 1) * font::CharWidth;
        aabb.h = font::CharHeight;
    }

    int16_t x() const { return aabb.x; }
    int16_t y() const { return aabb.y; }
};

constexpr const Button warning[] {
    Button("Mouse detected", engine::graphics::SCREEN_HEIGHT / 2, Action::None),
    Button("This game must be played", engine::graphics::SCREEN_HEIGHT / 2 + font::CharWidth * 3, Action::None),
    Button("with a controller", engine::graphics::SCREEN_HEIGHT / 2 + font::CharWidth * 6, Action::None),
    Button("Continue", engine::graphics::SCREEN_HEIGHT - game::font::CharWidth * 2, Action::MainMenu),
};

constexpr const Button main[] {
    Button("Start Game", engine::graphics::SCREEN_HEIGHT / 2, Action::Start),
    Button("Level Select", engine::graphics::SCREEN_HEIGHT / 2 + font::CharWidth * 3, Action::LevelSelect),
    Button("Cheats", engine::graphics::SCREEN_HEIGHT / 2 + font::CharWidth * 6, Action::Options),
    Button("Credits", engine::graphics::SCREEN_HEIGHT / 2 + font::CharWidth * 9, Action::Credits),
};

constexpr const Button level_select[] {
    Button("Back", engine::graphics::SCREEN_HEIGHT - game::font::CharWidth * 2, Action::MainMenu),
    Button("Breakout", engine::graphics::SCREEN_HEIGHT / 2, Action::Breakout),
    Button("Driving", engine::graphics::SCREEN_HEIGHT / 2 + game::font::CharWidth * 3, Action::Driving),
    Button("Dome", engine::graphics::SCREEN_HEIGHT / 2 + game::font::CharWidth * 6, Action::Cyber),
};

constexpr const Button options[] {
#if 0 // doesn't work and causes lag...
    Button("Volume up", engine::graphics::SCREEN_HEIGHT / 2, Action::VolUp),
    Button("Volume Down", engine::graphics::SCREEN_HEIGHT / 2 + game::font::CharWidth * 3, Action::VolDown),
#endif
    Button("Unlock all levels", engine::graphics::SCREEN_HEIGHT / 2, Action::UnlockAll),
    Button("Back", engine::graphics::SCREEN_HEIGHT - game::font::CharWidth * 2, Action::MainMenu),
};

constexpr const Button credits[] {
    Button("yes", engine::graphics::SCREEN_HEIGHT / 2, Action::None),
    Button("Back", engine::graphics::SCREEN_HEIGHT - font::CharWidth * 2, Action::MainMenu),
};

} // namespace buttons

void menu_redraw() {
    game::font::clear_text();

    const buttons::Button * butts = nullptr;
    uint32_t num_butts = 0;
    switch (s_menu_state) {
        case MenuState::Warning:
            game::font::write_centered("Warning", engine::graphics::SCREEN_HEIGHT / 4);
            butts = buttons::warning;
            num_butts = engine::utils::size(buttons::warning);
            break;
        case MenuState::Main:
            game::font::write_centered("Big Bucko Breakout", engine::graphics::SCREEN_HEIGHT / 4);
            butts = buttons::main;
            num_butts = engine::utils::size(buttons::main);
            break;
        case MenuState::LevelSelect:
            game::font::write_centered("Unlocked levels", engine::graphics::SCREEN_HEIGHT / 4);
            butts = buttons::level_select;
            num_butts = engine::utils::size(buttons::level_select);
            // Only show the appropriate ones.
            if (g_unlocked < static_cast<uint8_t>(Entry::Cyber)) num_butts--;
            if (g_unlocked < static_cast<uint8_t>(Entry::Driving)) num_butts--;
            if (g_unlocked < static_cast<uint8_t>(Entry::Breakout)) num_butts--;
            break;
        case MenuState::Options:
            game::font::write_centered("Cheats", engine::graphics::SCREEN_HEIGHT / 4);
            butts = buttons::options;
            num_butts = engine::utils::size(buttons::options);
            break;
        case MenuState::Credits:
            game::font::write_centered("Credits", engine::graphics::SCREEN_HEIGHT / 4);
            butts = buttons::credits;
            num_butts = engine::utils::size(buttons::credits);
            break;
    }
    for (uint32_t i = 0; i < num_butts; i++) {
        auto & button = butts[i];
        game::font::write_text(button.text, button.x(), button.y());
    }
}

void menu_setup() {
    // Load font data.
    constexpr uint8_t font_tile_start = bg_tile_start + bg_tile_count;
    constexpr uint8_t font_sprite_start = bg_sprite_start + bg_sprite_count;
    game::font::setup_tiles<font_tile_start, font_sprite_start>();

    // Title screen.
    s_menu_state = engine::input::g_mouse_plugged ? MenuState::Warning : MenuState::Main;
    menu_redraw();

    // Mouse bits.
    game::images::copy_tile_data<
        mouse_pal_start, mouse_pal_count,
        mouse_tile_start, mouse_tile_count,
        game::images::mouse
    >();
    s_mouse_pos.x = engine::graphics::SCREEN_WIDTH / 4;
    s_mouse_pos.y = engine::graphics::SCREEN_HEIGHT / 3;
}

Entry menu_update() {
    PROFILE_SCOPE(men_up);

    // Mouse input.
    const auto held = engine::input::g_buttons_held;
    if (false && engine::input::g_mouse_plugged) { // TODO: mouse support in all parts
        s_mouse_pos.x += engine::input::g_mouse_dx;
        s_mouse_pos.y += engine::input::g_mouse_dy;
    } else {
        if (held & GAMEPAD_BTN_LEFT) {
            s_mouse_pos.x -= 2;
        } else if (held & GAMEPAD_BTN_RIGHT) {
            s_mouse_pos.x += 2;
        }
        if (held & GAMEPAD_BTN_UP) {
            s_mouse_pos.y -= 2;
        } else if (held & GAMEPAD_BTN_DOWN) {
            s_mouse_pos.y += 2;
        }
    }
    constexpr uint8_t screen_padding = 3;
    s_mouse_pos.x = engine::utils::clamp<int16_t>(
        s_mouse_pos.x,
        screen_padding,
        engine::graphics::SCREEN_WIDTH - engine::graphics::bg_tile_size - screen_padding
    );
    s_mouse_pos.y = engine::utils::clamp<int16_t>(
        s_mouse_pos.y,
        screen_padding,
        engine::graphics::SCREEN_HEIGHT - engine::graphics::bg_tile_size - screen_padding
    );

    // Update the mouse sprite.
    engine::graphics::ObjSprite mouse;
    mouse.set_x(s_mouse_pos.x);
    mouse.set_y(s_mouse_pos.y);
    mouse.set_tile_index(mouse_tile_start);
    engine::graphics::set_sprite(mouse_sprite_start, mouse);

    // Handle any button presses.
    const auto pressed = engine::input::g_buttons_pressed;
    if (pressed & GAMEPAD_BTN_A)
    {
        const engine::utils::AABB mouse_aabb{
            s_mouse_pos.x, s_mouse_pos.y,
            engine::graphics::bg_tile_size, engine::graphics::bg_tile_size
        };

        // See which buttons are active.
        const buttons::Button * butts = nullptr;
        uint32_t num_butts = 0;
        switch (s_menu_state) {
            case MenuState::Warning:
                butts = buttons::warning;
                num_butts = engine::utils::size(buttons::warning);
                break;
            case MenuState::Main:
                butts = buttons::main;
                num_butts = engine::utils::size(buttons::main);
                break;
            case MenuState::LevelSelect:
                butts = buttons::level_select;
                num_butts = engine::utils::size(buttons::level_select);
                break;
            case MenuState::Options:
                butts = buttons::options;
                num_butts = engine::utils::size(buttons::options);
                break;
            case MenuState::Credits:
                butts = buttons::credits;
                num_butts = engine::utils::size(buttons::credits);
                break;
        }

        // Check for button presses.
        buttons::Action action = buttons::Action::None;
        for (uint32_t i = 0; i < num_butts; i++) {
            auto & button = butts[i];
            if (mouse_aabb.intersects(button.aabb)) {
                action = button.action;
                break;
            }
        }

        // Play a sound.
        if (action == buttons::Action::None) {
            engine::sound::play_effect(game::music::SoundEffect::SE_MM_Miss);
        } else {
            engine::sound::play_effect(game::music::SoundEffect::SE_MM_Click);
        }

        // Handle the action.
        switch (action) {
            using buttons::Action;
                case Action::None:
                    break;

                case Action::VolUp:
                    static int s_volume = SOUND_VOL_100;
                    s_volume += 2;
                    [[fallthrough]];
                case Action::VolDown:
                    s_volume -= 1;
                    s_volume = engine::utils::clamp(s_volume, 0, SOUND_VOL_100);
                    engine::sound::set_volume(s_volume);
                    break;
                case Action::UnlockAll:
                    g_unlocked = unlock_all;
                    break;

                case Action::MainMenu:
                    s_menu_state = MenuState::Main;
                    menu_redraw();
                    break;
                case Action::LevelSelect:
                    s_menu_state = MenuState::LevelSelect;
                    menu_redraw();
                    break;
                case Action::Options:
                    s_menu_state = MenuState::Options;
                    menu_redraw();
                    break;
                case Action::Credits:
                    s_menu_state = MenuState::Credits;
                    menu_redraw();
                    break;

                case Action::Start:
                    g_frame_count = 0;
                    intertile::setup(intertile::Text::Intro, Entry::Breakout);
                    return Entry::Intertitle;

                case Action::Breakout:
                    g_frame_count = invalid_frame_count;
                    return Entry::Breakout;
                case Action::Driving:
                    g_frame_count = invalid_frame_count;
                    return Entry::Driving;
                case Action::Cyber:
                    g_frame_count = invalid_frame_count;
                    return Entry::Cyber;
        }
    }

    // Testing stuff on the credits menu.
    if (s_menu_state == MenuState::Credits &&
        (held & GAMEPAD_BTN_C) &&
        (pressed & (GAMEPAD_BTN_UP | GAMEPAD_BTN_DOWN | GAMEPAD_BTN_RIGHT | GAMEPAD_BTN_LEFT)))
    {
        // Make it easier to hear the notes.
        if (sys_bgmRunning()) {
            engine::sound::stop_bgm();
        }

        static uint8_t voice = music::voices::drums;
        static uint8_t note = music::notes::snare;

        // left/right for voice, up/down for note.
        if (pressed & GAMEPAD_BTN_UP) note++;
        else if (pressed & GAMEPAD_BTN_DOWN) note--;
        else if (pressed & GAMEPAD_BTN_RIGHT) voice++;
        else if (pressed & GAMEPAD_BTN_LEFT) voice--;
        note &= 0x7F;
        voice &= 0x7F;
        music::set_test_voice(voice);
        music::set_test_note(note);

        // Stringify.
        auto voice_str = engine::utils::to_hex(voice);
        auto note_str = engine::utils::to_hex(note);
        DEBUG_MSG("voice=", voice_str.data(), " note=", note_str.data());

        // Redraw with the options.
        menu_redraw();
        font::write_text(voice_str.data(), 0, 0);
        font::write_text(note_str.data(), 0, font::CharHeight);

        // Make the noise.
        engine::sound::play_effect(game::music::SoundEffect::SE_Test);
    }

    return Entry::MainMenu;
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

    // Setup the bouncers.
    bouncers_setup();

    // UI bits.
    menu_setup();

    // This screen uses sprites and has a background.
    bios_vsync();
    engine::graphics::enable_sprites();
    engine::graphics::background_0.enable();

    // Kick off the bgm.
    engine::sound::play_bgm(g_unlocked >= static_cast<uint8_t>(Entry::Winner) ? game::music::Bgm::Bgm_MM_good : game::music::Bgm::Bgm_MM_bad);

    engine::profiler::print_timings();
}

void leave() {
    // Reset graphics state.
    bios_vsync();
    engine::graphics::disable_sprites();
    engine::graphics::background_0.disable();
    engine::graphics::reset_sprites<bg_sprite_start + bg_sprite_count>();
    game::font::clear_text();

    // Reset sound.
    engine::sound::stop_bgm();
}

Entry loop() {
    Entry next = Entry::MainMenu;
    // Wait for vsync.
    {
        PROFILE_SCOPE(vsync);
        bios_vsync();
    }

    // Update gamepad/mouse input.
    engine::input::update_inputs();

    // Update text from menu.
    next = menu_update();
    if (next != Entry::MainMenu) {
        return next;
    }

    // Update bouncers.
    bouncers_update();

    // Update background.
    background_update();

    engine::profiler::print_timings();

    return next;
}

} // namespace game
