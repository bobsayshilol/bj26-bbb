#include "gallery.h"
#include "graphics.h"
#include "input.h"
#include "data_gallery.h"
#include "memory.h"

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

// TODO: cursor
//constexpr uint8_t cursor_pal_start = fullscreen_pal_offset + stickers::max_palette_size;
//constexpr uint8_t cursor_pal_size = 

// TODO: font

// TODO: bg for menus

//

// Page is idx / 4.
int16_t s_selected_idx;
constexpr int16_t previews_per_page = 4;

enum class UIState {
    Previews,
    Fullscreen,
} s_ui_state;

//

void change_ui() {
    using namespace engine::graphics;

    switch (s_ui_state) {
        case UIState::Previews: {
            auto quad = [](auto && bitmap, uint16_t sx, uint16_t x, uint16_t y) {
                bitmap.position_x() = x;
                bitmap.position_y() = y;
                bitmap.width() = stickers::preview_width - 1;
                bitmap.height() = stickers::preview_height - 1;
                bitmap.scroll_x() = sx;
                bitmap.scroll_y() = preview_start / SCREEN_WIDTH;
                bitmap.enable();
            };
            quad(bitmap_0, stickers::preview_width * 0, SCREEN_WIDTH * 3 / 16, SCREEN_HEIGHT * 3 / 16);
            quad(bitmap_1, stickers::preview_width * 1, SCREEN_WIDTH * 9 / 16, SCREEN_HEIGHT * 3 / 16);
            quad(bitmap_2, stickers::preview_width * 2, SCREEN_WIDTH * 3 / 16, SCREEN_HEIGHT * 9 / 16);
            quad(bitmap_3, stickers::preview_width * 3, SCREEN_WIDTH * 9 / 16, SCREEN_HEIGHT * 9 / 16);
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

    change_ui();
}

bool ui_update() {
    const auto pressed = engine::input::g_buttons_pressed;
    const auto state = s_ui_state;

    bool redraw = false;

    switch (state) {
        case UIState::Previews:
            if (pressed & GAMEPAD_BTN_LTRIG) {
                s_selected_idx -= previews_per_page;
                if (s_selected_idx < 0) s_selected_idx = (stickers::num_stickers - 1) / 4 * 4;
                redraw = true;
            } else if (pressed & GAMEPAD_BTN_RTRIG) {
                s_selected_idx += previews_per_page;
                if (s_selected_idx >= stickers::num_stickers) s_selected_idx = 0;
                redraw = true;
            } else if (pressed & GAMEPAD_BTN_A) {
                s_ui_state = UIState::Fullscreen;
                redraw = true;
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
                s_ui_state = UIState::Previews;
                s_selected_idx = static_cast<uint16_t>(s_selected_idx) / 4 * 4;
                redraw = true;
            }
            break;
    }

    if (redraw) {
        ui_redraw();
    }

    return false;
}

} // namespace

void enter() {
    // Reset state.
    s_selected_idx = 0;
    s_ui_state = UIState::Previews;

    ui_redraw();
}

void leave() {
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
