#include "gallery.h"
#include "graphics.h"
#include "input.h"
#include "data_gallery.h"

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

int s_idx = 0;

void load_thing() {
    const int N = 3;
    if (s_idx < 0) s_idx += N;
    else if (s_idx >= N) s_idx -= N;
    DEBUG_MSG("change:", s_idx);

    const uint8_t pal_offset = 2;
    const auto img = (stickers::Image)s_idx;
    stickers::decompress_fullscreen(img, pal_offset, VDP.BITMAP_VRAM_8BIT);
    stickers::load_palette(img, VDP.PALETTE + pal_offset);
}

enum class UIType { Preview, Fullscreen, };
void change_ui(UIType type) {
    using namespace engine::graphics;

    switch (type) {
        case UIType::Preview:
            // TODO
            break;

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

} // namespace

void enter() {
    load_thing();

    change_ui(UIType::Fullscreen);
}

void leave() {
}

Screen loop() {
    DEBUG_MSG("TODO: ", __func__);
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
