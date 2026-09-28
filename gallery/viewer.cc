#include "gallery.h"
#include "graphics.h"
#include "images.h"
#include "input.h"
#include "data_gallery.h"
#include "memory.h"
#include "game_font.h"
#include "aabb.h"

namespace gallery::viewer {

namespace {

//
// +-----+
// |     | <- big view
// +-----+
// |     | <- temp area
// +-----+
// | | | | <- previews
// +-+-+-+
//
constexpr uint32_t fullscreen_start = 0;
constexpr uint32_t fullscreen_size = stickers::fullscreen_width * stickers::fullscreen_height;
constexpr uint32_t temp_start = fullscreen_start + fullscreen_size;
constexpr uint32_t temp_size = fullscreen_size;
constexpr uint32_t preview_start = temp_start + temp_size;
constexpr uint32_t preview_size = stickers::preview_width * stickers::preview_height;
static_assert(preview_start + 4 * preview_size <= engine::utils::size(VDP.BITMAP_VRAM_8BIT));

//

// Start it as early as possible so we don't have any issues with printing.
static_assert(engine::graphics::pal_transparent == 0);
constexpr uint8_t fullscreen_pal_offset = 1;

//

constexpr uint8_t cursor_pal_start = fullscreen_pal_offset + stickers::max_palette_size;
constexpr uint8_t cursor_pal_count = 10;
constexpr uint8_t cursor_tile_start = 0;
constexpr uint8_t cursor_tile_count = 1;
constexpr uint8_t cursor_sprite_start = 0; // highest prio
constexpr uint8_t cursor_sprite_count = 1;

constexpr uint8_t font_pal_start = cursor_pal_start + cursor_pal_count;
constexpr uint8_t font_pal_count = font::FontConfig::palette_count;
constexpr uint8_t font_tile_start = cursor_tile_start + cursor_tile_count;
constexpr uint8_t font_tile_count = engine::font::tile_count;
constexpr uint8_t font_sprite_start = cursor_sprite_start + cursor_sprite_count;
constexpr uint8_t font_sprite_count = font::FontConfig::max_sprites;
static_assert(font_pal_start == font::FontConfig::palette_start);

// TODO: arrows
constexpr uint8_t arrows_pal_start = font_pal_start + font_pal_count;
constexpr uint8_t arrows_pal_count = 10;
constexpr uint8_t arrows_tile_start = font_tile_start + font_tile_count;
constexpr uint8_t arrows_tile_count = 1;
constexpr uint8_t arrows_sprite_start = font_sprite_start + font_sprite_count;
constexpr uint8_t arrows_sprite_count = 2; // one on either side, flipped

// TODO: bg for menus
constexpr uint8_t bg0_pal_start = arrows_pal_start + arrows_pal_count;
constexpr uint8_t bg0_pal_count = 10;
constexpr uint8_t bg0_tile_start = arrows_tile_start + arrows_tile_count;
constexpr uint8_t bg0_tile_count = 1; // repeating pattern

// TODO: highlight for preview

//

// Page is idx / 4.
int16_t s_selected_idx;
constexpr int16_t previews_per_page = 4;

enum class UIState {
    Previews,
    Fullscreen,
} s_ui_state;

void ui_advance(UIState state);
void ui_redraw();

//

struct alignas(2) Mouse {
    uint8_t x, y;
} s_mouse;

//

enum class ButtonType : uint8_t {
    // PreviewX is used as an offset, so needs to be fixed numbers.
    Preview0 = 0, Preview1 = 1, Preview2 = 2, Preview3 = 3,
};
struct Button {
    engine::utils::AABB aabb;
    ButtonType type;
    constexpr Button(uint8_t x, uint8_t y, uint8_t w, uint8_t h, ButtonType t)
        : aabb{x, y, w, h}
        , type(t)
    {}
};

constexpr Button buttons[] {
    Button(
        engine::graphics::SCREEN_WIDTH * 3 / 16, engine::graphics::SCREEN_HEIGHT * 3 / 16,
        stickers::preview_width, stickers::preview_height,
        ButtonType::Preview0
    ),
    Button(
        engine::graphics::SCREEN_WIDTH * 9 / 16, engine::graphics::SCREEN_HEIGHT * 3 / 16,
        stickers::preview_width, stickers::preview_height,
        ButtonType::Preview1
    ),
    Button(
        engine::graphics::SCREEN_WIDTH * 3 / 16, engine::graphics::SCREEN_HEIGHT * 9 / 16,
        stickers::preview_width, stickers::preview_height,
        ButtonType::Preview2
    ),
    Button(
        engine::graphics::SCREEN_WIDTH * 9 / 16, engine::graphics::SCREEN_HEIGHT * 9 / 16,
        stickers::preview_width, stickers::preview_height,
        ButtonType::Preview3
    ),
};

const Button * s_current_button;

//

void mouse_redraw() {
    // Update sprite.
    engine::graphics::ObjSprite sprite;
    sprite.set_tile_index(cursor_tile_start);
    sprite.set_x(s_mouse.x);
    sprite.set_y(s_mouse.y);
    engine::graphics::set_sprite(cursor_sprite_start, sprite);
}

void mouse_show(bool show) {
    if (show) {
        s_mouse.x = engine::graphics::SCREEN_WIDTH / 2;
        s_mouse.y = engine::graphics::SCREEN_HEIGHT / 2;
        mouse_redraw();
    } else {
        engine::graphics::set_sprite(cursor_sprite_start, {});
    }
}

void mouse_setup() {
    engine::graphics::copy_tile_data<
        cursor_pal_start, cursor_pal_count,
        cursor_tile_start, cursor_tile_count,
        images::tiles_mouse
    >();
}

void mouse_enter(const Button & button);
void mouse_leave(const Button & button);

void mouse_reset() {
    if (s_current_button) {
        mouse_leave(*s_current_button);
        s_current_button = nullptr;
    }
}

void mouse_move() {
    const engine::utils::AABB cursor_aabb{
        s_mouse.x, s_mouse.y,
        engine::graphics::bg_tile_size, engine::graphics::bg_tile_size,
    };
    if (s_current_button) {
        // Check for moving off of the button.
        if (!s_current_button->aabb.intersects(cursor_aabb)) {
            mouse_reset();
        }
    } else {
        // Look for a new button.
        for (const Button & butt : buttons) {
            // No overlapping so should be single hit.
            if (cursor_aabb.intersects(butt.aabb)) {
                s_current_button = &butt;
                mouse_enter(butt);
                break;
            }
        }
    }
}

void mouse_update() {
    const auto held = engine::input::g_buttons_held;

    constexpr uint8_t speed = 2;
    constexpr uint8_t tile_size = engine::graphics::bg_tile_size;
    constexpr uint8_t padding = 3;

    if (held & GAMEPAD_BTN_LEFT) {
        s_mouse.x = s_mouse.x - speed;
    } else if (held & GAMEPAD_BTN_RIGHT) {
        s_mouse.x = s_mouse.x + speed;
    }
    s_mouse.x = engine::utils::clamp<int16_t>(
        s_mouse.x,
        padding,
        engine::graphics::SCREEN_WIDTH - tile_size - padding
    );

    if (held & GAMEPAD_BTN_DOWN) {
        s_mouse.y = s_mouse.y + speed;
    } else if (held & GAMEPAD_BTN_UP) {
        s_mouse.y = s_mouse.y - speed;
    }
    s_mouse.y = engine::utils::clamp<int16_t>(
        s_mouse.y,
        padding,
        engine::graphics::SCREEN_HEIGHT - tile_size - padding
    );

    mouse_move();
    mouse_redraw();
}

//

enum class PreviewAnimation {
    None,
    Spiral,
} s_preview_anim;

uint16_t s_anim_time;

enum class PreviewAction {
    Decrement, Increment,
} s_anim_action;

void animation_start(PreviewAction action) {
    // TODO: random animations
    s_preview_anim = PreviewAnimation::Spiral;
    s_anim_time = 0;
    s_anim_action = action;
}

bool animation_update() {
    const uint16_t t = s_anim_time++;

    enum class AnimState { Inactive, Playing, Trigger, };
    AnimState state = AnimState::Inactive;

    // Display the animation.
    switch (s_preview_anim) {
        case PreviewAnimation::None:
            break;

        case PreviewAnimation::Spiral:
            if (t == 30) {
                state = AnimState::Trigger;
            } else if (t > 60) {
                state = AnimState::Inactive;
                s_preview_anim = PreviewAnimation::None;
            } else {
                state = AnimState::Playing;
                using namespace engine::graphics;

                constexpr int16_t speed = 6;
                const uint16_t dt = t > 30 ? 60 - t : t;

                auto update_bitmap = [&](auto && bitmap, int16_t dx, int16_t dy, const Button & button) {
                    bitmap.position_x() = button.aabb.x + bios_mathMulS16(dx, dt);
                    bitmap.position_y() = button.aabb.y + bios_mathMulS16(dy, dt);
                };
                update_bitmap(bitmap_0, -speed, -speed, buttons[0]); static_assert(buttons[0].type == ButtonType::Preview0);
                update_bitmap(bitmap_1, speed, -speed, buttons[1]); static_assert(buttons[1].type == ButtonType::Preview1);
                update_bitmap(bitmap_2, -speed, speed, buttons[2]); static_assert(buttons[2].type == ButtonType::Preview2);
                update_bitmap(bitmap_3, speed, speed, buttons[3]); static_assert(buttons[3].type == ButtonType::Preview3);
            }
            break;
    }

    // Trigger the action.
    if (state == AnimState::Trigger) {
        switch (s_anim_action) {
            case PreviewAction::Increment:
                s_selected_idx += previews_per_page;
                if (s_selected_idx >= stickers::num_stickers) s_selected_idx = 0;
                break;
            case PreviewAction::Decrement:
                s_selected_idx -= previews_per_page;
                if (s_selected_idx < 0) s_selected_idx = (stickers::num_stickers - 1) / 4 * 4;
                break;
        }
        ui_redraw();
    }

    return state != AnimState::Inactive;
}

//

void change_ui() {
    using namespace engine::graphics;

    switch (s_ui_state) {
        case UIState::Previews: {
            auto quad = [](auto && bitmap, uint16_t sx, const Button & button) {
                bitmap.position_x() = button.aabb.x;
                bitmap.position_y() = button.aabb.y;
                bitmap.width() = stickers::preview_width - 1;
                bitmap.height() = stickers::preview_height - 1;
                bitmap.scroll_x() = sx;
                bitmap.scroll_y() = preview_start / SCREEN_WIDTH;
                bitmap.enable();
            };
            quad(bitmap_0, stickers::preview_width * 0, buttons[0]); static_assert(buttons[0].type == ButtonType::Preview0);
            quad(bitmap_1, stickers::preview_width * 1, buttons[1]); static_assert(buttons[1].type == ButtonType::Preview1);
            quad(bitmap_2, stickers::preview_width * 2, buttons[2]); static_assert(buttons[2].type == ButtonType::Preview2);
            quad(bitmap_3, stickers::preview_width * 3, buttons[3]); static_assert(buttons[3].type == ButtonType::Preview3);
        } break;

        case UIState::Fullscreen:
            bitmap_0.position_x() = 0;
            bitmap_0.position_y() = 0;
            bitmap_0.width() = stickers::fullscreen_width - 1;
            bitmap_0.height() = stickers::fullscreen_height - 1;
            bitmap_0.scroll_x() = 0;
            bitmap_0.scroll_y() = fullscreen_start / SCREEN_WIDTH;
            bitmap_0.enable();
            bitmap_1.disable();
            bitmap_2.disable();
            bitmap_3.disable();
            break;
    }
}

//

void do_print() {
    DEBUG_MSG("printing");

    // TODO: display a message

    // TODO: pause/unpause music

    bios_print8bpp(VDP.BITMAP_VRAM_8BIT + fullscreen_start, VDP.PALETTE, 1);
}

//

void ui_redraw() {
    // Reset the mouse to clear stuff since we reuse the same buttons.
    mouse_reset();

    uint8_t * temp_data = VDP.BITMAP_VRAM_8BIT + temp_start;
    const uint8_t idx = s_selected_idx;

    switch (s_ui_state) {
        case UIState::Fullscreen: {
            const auto img = (stickers::Image)idx;
            stickers::decompress_fullscreen(img, fullscreen_pal_offset, temp_data);
            stickers::load_palette(img, VDP.PALETTE + fullscreen_pal_offset);

            // TODO: could just decompress straight to the visible region
            uint8_t * fullscreen_data = VDP.BITMAP_VRAM_8BIT + fullscreen_start;
            engine::utils::fast_memcpy(fullscreen_data, temp_data, fullscreen_size);
        } break;

        case UIState::Previews: {
            const auto end = engine::utils::min<uint8_t>(idx + previews_per_page, stickers::num_stickers);
            for (uint16_t i = idx; i < end; i++) {
                stickers::decompress_preview((stickers::Image)i, fullscreen_pal_offset, temp_data);

                uint8_t * preview_data = VDP.BITMAP_VRAM_8BIT + preview_start + stickers::preview_width * (i - idx);
                for (uint16_t y = 0; y < stickers::preview_height; y++) {
                    engine::utils::fast_memcpy(preview_data, temp_data, stickers::preview_width);
                    preview_data += engine::graphics::SCREEN_WIDTH;
                    temp_data += stickers::preview_width;
                }
            }

            // Clear empty ones.
            for (uint16_t i = end; i < idx + previews_per_page; i++) {
                uint8_t * preview_data = VDP.BITMAP_VRAM_8BIT + preview_start + stickers::preview_width * (i - idx);
                for (uint16_t y = 0; y < stickers::preview_height; y++) {
                    engine::utils::fast_memset8(preview_data, engine::graphics::pal_transparent, stickers::preview_width);
                    preview_data += engine::graphics::SCREEN_WIDTH;
                    temp_data += stickers::preview_width;
                }
            }

            stickers::preview_palette(VDP.PALETTE + fullscreen_pal_offset);
        } break;
    }
}

bool ui_update() {
    const auto pressed = engine::input::g_buttons_pressed;
    const auto state = s_ui_state;

    bool redraw = false;

    // If there's an animation playing, do that.
    if (animation_update()) {
        return false;
    }

    switch (state) {
        case UIState::Previews:
            mouse_update();
            if (pressed & GAMEPAD_BTN_LTRIG) {
                animation_start(PreviewAction::Decrement);
            } else if (pressed & GAMEPAD_BTN_RTRIG) {
                animation_start(PreviewAction::Increment);
            } else if (pressed & GAMEPAD_BTN_A) {
                if (s_current_button) {
                    // Select the one that's selected.
                    const uint8_t preview_idx = static_cast<uint8_t>(s_current_button->type);
                    // TODO: should really avoid making the button if it'd be out of bounds
                    if (s_selected_idx + preview_idx < stickers::num_stickers) {
                        s_selected_idx += preview_idx;
                        ui_advance(UIState::Fullscreen);
                        redraw = true;
                    }
                }
            } else if (pressed & GAMEPAD_BTN_B) {
                // Return from menu.
                return true;
            }
            break;

        case UIState::Fullscreen:
            if (pressed & GAMEPAD_BTN_LTRIG) {
                s_selected_idx -= 1;
                if (s_selected_idx < 0) s_selected_idx = stickers::num_stickers - 1;
                redraw = true;
            } else if (pressed & GAMEPAD_BTN_RTRIG) {
                s_selected_idx += 1;
                if (s_selected_idx >= stickers::num_stickers) s_selected_idx = 0;
                redraw = true;
            } else if (pressed & GAMEPAD_BTN_A) {
                do_print();
            } else if (pressed & GAMEPAD_BTN_B) {
                ui_advance(UIState::Previews);
                s_selected_idx = static_cast<uint16_t>(s_selected_idx) / 4 * 4;
                redraw = true;
            }
            break;
    }

    if (redraw) {
        ui_redraw();
        change_ui();
    }

    return false;
}

void ui_setup() {
    // Load font.
    font::setup_tiles<font_tile_start, font_sprite_start>();
}

void background_setup() {
    using namespace engine::graphics;

    // Copy tiles.
    copy_tile_data<
        bg0_pal_start, bg0_pal_count,
        bg0_tile_start, bg0_tile_count,
        images::tiles_bg
    >();

    // Setup bg.
    BGSprite sprite;
    for (uint8_t y = 0; y < bg_tilemap_size; y++) {
        sprite.set_y_flip(y & 1);
        for (uint8_t x = 0; x < bg_tilemap_size; x++) {
            sprite.set_tile_index(bg0_tile_start);
            sprite.set_x_flip(x & 1);
            background_0.set_sprite(x, y, sprite);
        }
    }
}

//

void ui_advance(UIState state) {
    s_ui_state = state;
    switch (state) {
        case UIState::Previews:
            mouse_show(true);
            break;

        case UIState::Fullscreen:
            mouse_show(false);
            break;
    }
}

void mouse_enter(const Button & button) {
    switch (button.type) {
        case ButtonType::Preview0:
        case ButtonType::Preview1:
        case ButtonType::Preview2:
        case ButtonType::Preview3: {
            // Work out which image we're looking at.
            const uint8_t preview_index = s_selected_idx + static_cast<uint8_t>(button.type);
            if (preview_index < stickers::num_stickers) {
                uint8_t length = 0;
                const char * name = stickers::image_name((stickers::Image)preview_index, length);

                // Display it.
                constexpr uint8_t y = engine::graphics::SCREEN_HEIGHT - engine::graphics::bg_tile_size * 2;
                engine::font::write_centered(name, length + 1, y);
            }
        } break;
    }
}

void mouse_leave(const Button &) {
    // Remove the name.
    engine::font::clear_text();
}

} // namespace

void enter() {
    // Load stuff.
    ui_setup();
    background_setup();
    mouse_setup();

    // Reset state.
    s_selected_idx = 0;
    s_current_button = nullptr;
    s_preview_anim = PreviewAnimation::None;
    ui_advance(UIState::Previews);
    ui_redraw();
    change_ui();

    // This screen uses sprites and has a background.
    bios_vsync();
    engine::graphics::enable_sprites();
    engine::graphics::background_0.enable();
}

void leave() {
    // Reset graphics state.
    bios_vsync();
    engine::graphics::disable_sprites();
    engine::graphics::background_0.disable();
    engine::graphics::bitmap_0.disable();
    engine::graphics::bitmap_1.disable();
    engine::graphics::bitmap_2.disable();
    engine::graphics::bitmap_3.disable();
    engine::graphics::reset_sprites<arrows_sprite_start + arrows_sprite_count>();
    engine::font::clear_text();
}

Screen loop() {
    bios_vsync();

    engine::input::update_inputs();

    if (ui_update()) {
        return Screen::MainMenu;
    }

    return Screen::Viewer;
}

} // namespace gallery::viewer
