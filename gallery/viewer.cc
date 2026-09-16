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

enum class UIType { Preview, Fullscreen, };
void change_ui(UIType type) {
    using namespace engine::graphics;

    switch (type) {
        case UIType::Preview: {
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

        case UIType::Fullscreen:
            bitmap_0.position_x() = 0;
            bitmap_0.position_y() = 0;
            bitmap_0.width() = stickers::fullscreen_width - 1;
            bitmap_0.height() = stickers::fullscreen_height - 1;
            bitmap_0.scroll_x() = 0;
            bitmap_0.scroll_y() = 0;
            bitmap_0.enable();
            bitmap_1.disable();
            bitmap_2.disable();
            bitmap_3.disable();
            break;
    }
}

//

int s_idx = 0;

void load_thing() {
    const int N = 3;
    if (s_idx < 0) s_idx += N + 1;
    else if (s_idx >= N + 1) s_idx -= N + 1;
    DEBUG_MSG("change:", s_idx);

    const uint8_t pal_offset = 2;
    uint8_t * temp_data = VDP.BITMAP_VRAM_8BIT + temp_start;
    if (s_idx < N) {
        const auto img = (stickers::Image)s_idx;
        stickers::decompress_fullscreen(img, pal_offset, temp_data);
        stickers::load_palette(img, VDP.PALETTE + pal_offset);

        // TODO: could just decompress straight to the visible region
        uint8_t * fullscreen_data = VDP.BITMAP_VRAM_8BIT + fullscreen_start;
        engine::utils::fast_memcpy(fullscreen_data, temp_data, fullscreen_size);

        change_ui(UIType::Fullscreen);
    } else {
        for (int i = 0; i < N; i++) {
            stickers::decompress_preview((stickers::Image)i, pal_offset, temp_data);

            uint8_t * preview_data = VDP.BITMAP_VRAM_8BIT + preview_start + stickers::preview_width * i;
            for (uint16_t y = 0; y < stickers::preview_height; y++) {
                engine::utils::fast_memcpy(preview_data, temp_data, stickers::preview_width);
                preview_data += engine::graphics::SCREEN_WIDTH;
                temp_data += stickers::preview_width;
            }
        }

        stickers::preview_palette(VDP.PALETTE + pal_offset);
        change_ui(UIType::Preview);
    }
}

} // namespace

void enter() {
    load_thing();

    change_ui(UIType::Fullscreen);
}

void leave() {
}

Screen loop() {
    bios_vsync();

    engine::input::update_inputs();
    const auto pressed = engine::input::g_buttons_pressed;
    if (pressed & GAMEPAD_BTN_A) {
        s_idx++;
        load_thing();
    } else if (pressed & GAMEPAD_BTN_B) {
        s_idx--;
        load_thing();
    }

    return Screen::Viewer;
}

} // namespace gallery::viewer
