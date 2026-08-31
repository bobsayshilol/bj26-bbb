#include "aabb.h"
#include "draw_line.h"
#include "game.h"
#include "graphics.h"
#include "images.h"
#include "input.h"
#include "maths.h"
#include "memory.h"
#include "profiler.h"
#include "sound.h"
#include "music.h"
#include "font.h"
#include "utils.h"

// Skip stuff for testing/debugging.
#define SKIP_STUFF 0

namespace game::cyber {

namespace {

PROFILE_STORAGE(bg_pud);
PROFILE_STORAGE(bg_drw);
PROFILE_STORAGE(ui_upd);
PROFILE_STORAGE(cr_upd);
PROFILE_STORAGE(dr_lin);
PROFILE_STORAGE(vsync);

constexpr uint8_t pal_black = 1;
constexpr uint8_t pal_white = 2;
constexpr uint8_t pal_red = 3;
constexpr uint8_t pal_green = 4;
constexpr uint8_t pal_blue = 5;
constexpr uint8_t pal_grey = 6;
constexpr uint8_t pal_good_bad = 7;
constexpr uint8_t pal_basic_end = 8;

// font has highest prio.
constexpr uint8_t font_sprite_start = 0;
constexpr uint8_t font_sprite_count = font::font_max_sprites;
constexpr uint8_t font_tile_start = 0;
constexpr uint8_t font_tile_count = font::font_tile_count;

constexpr uint8_t voice_char_width = 3;
constexpr uint8_t voice_char_height = 4;

// Cursor has higher prio than keypad.
constexpr uint8_t cursor_sprite_start = font_sprite_start + font_sprite_count;
constexpr uint8_t cursor_sprite_count = 1;
constexpr uint8_t cursor_tile_start = font_tile_start + font_tile_count;
constexpr uint8_t cursor_tile_count = 1;

constexpr uint8_t keypad_pal_start = pal_basic_end + 1;
constexpr uint8_t keypad_pal_count = 0;
constexpr uint8_t keypad_sprite_start = cursor_sprite_start + cursor_sprite_count;
constexpr uint8_t keypad_sprite_count = 9*4; // 3x3 grid of 2x2 tiles
constexpr uint8_t keypad_tile_start = cursor_tile_start + cursor_tile_count;
constexpr uint8_t keypad_tile_count = 4;
constexpr uint8_t keypad_tile_off = keypad_tile_start + 0;
constexpr uint8_t keypad_tile_on = keypad_tile_start + 1;
constexpr uint8_t keypad_tile_good = keypad_tile_start + 2;
constexpr uint8_t keypad_tile_bad = keypad_tile_start + 3;

// Empty space for palette up to 24.

static_assert(keypad_pal_start + keypad_pal_count <= images::bucko_left::pal_offset);

// Bit awkward, but bucko_left must be at a fixed offset (from breakout).
constexpr uint8_t bucko_left_pal_start = images::bucko_left::pal_offset;
constexpr uint8_t bucko_left_pal_count = 10;
constexpr uint8_t bucko_left_sprite_start = keypad_sprite_start + keypad_sprite_count;
constexpr uint8_t bucko_left_sprite_count = voice_char_width * voice_char_height;
constexpr uint8_t bucko_left_tile_start = keypad_tile_start + keypad_tile_count;
constexpr uint8_t bucko_left_tile_count = bucko_left_sprite_count;

constexpr uint8_t ami_left_pal_start = bucko_left_pal_start + bucko_left_pal_count;
constexpr uint8_t ami_left_pal_count = 10;
constexpr uint8_t ami_left_sprite_start = bucko_left_sprite_start; // reuse the bucko sprites
constexpr uint8_t ami_left_sprite_count = bucko_left_sprite_count;
constexpr uint8_t ami_left_tile_start = bucko_left_tile_start + bucko_left_tile_count;
constexpr uint8_t ami_left_tile_count = ami_left_sprite_count;

constexpr uint8_t bg_pal_start = ami_left_pal_start + ami_left_pal_count;
constexpr uint8_t bg_pal_count = 8 * 8;

constexpr uint8_t entering_pal_size = 16; // +1 really
static_assert(entering_pal_size + 1 <= bg_pal_count);

constexpr uint8_t cover_pal_start = bg_pal_start + bg_pal_count;
constexpr uint8_t cover_pal_count = 16;


constexpr uint8_t font_pal_start = font::font_palette_start;
constexpr uint8_t font_pal_count = font::font_palette_count;
static_assert(cover_pal_start + cover_pal_count <= font_pal_start);

//

enum class LevelState {
    Start,
    Reveal,
    RevealFinished,
    KeypadShown,
    OutsideTalk,
    OutsideCode,
    OutsideGood,
    OutsideBad,
    OutsideFinished,
    Entering,
    Inside,
    InsideGlitchy,
    Finished,
} s_level_state;

enum class CodeState {
    CodeA,
    CodeM,
    CodeI,
    CodeMid,
    CodeC,
    CodeU,
    CodeT,
    CodeE,
} s_code_state;

void level_advance(LevelState state);

//

enum class BGStyle {
    Outside,
    OutsideWavy,
    Inside,
    InsideGlitchy,
} s_bg_style;

uint16_t s_bg_pallete[bg_pal_count];

uint16_t s_bg_timer;

enum WormSpeed : uint16_t {
    None = 0,
    EnterSlow = 0x0001,
    EnterFast = 0x00FF,
    SpinSlow = 0x0100,
    SpinFast = 0xFF00,
    SpinFastEnterSlow = 0xFF01,
    SpinFastEnterFast = 0xFFFF,
} s_wormhole_speed;

void bg_pallete_update() {
    PROFILE_SCOPE(bg_pud);

    const uint16_t timer = s_bg_timer;

    switch (s_bg_style) {
        case BGStyle::Outside:
        case BGStyle::OutsideWavy: {
            static_assert(bg_pal_count == 8 * 8);

            const uint8_t worm_enter = s_wormhole_speed;
            const uint8_t worm_spin = s_wormhole_speed >> 8;

            // If we're not spinning then make the space fade in/out.
            if (!worm_spin && !worm_enter) {
                const uint8_t g = (3 * static_cast<uint16_t>(128 + engine::maths::cos((timer * 3) / 2))) / 64;
                for (uint8_t y = 1; y < 8; y++) {
                    for (uint8_t x = 1; x < 8; x++) {
                        s_bg_pallete[y * 8 + x] = RGB555(0,0,g);
                    }
                }
            }
            if (timer & worm_enter) {
                for (uint8_t y = 0; y < 8; y++) {
                    engine::utils::rotate_right<1>(s_bg_pallete + y * 8, s_bg_pallete + (y + 1) * 8);
                }
            }
            if (timer & worm_spin) {
                engine::utils::rotate_right<8>(s_bg_pallete, s_bg_pallete + bg_pal_count);
            }

            // Update in VDP.
            engine::utils::fast_memcpy(VDP.PALETTE + bg_pal_start, s_bg_pallete, sizeof(s_bg_pallete));
        }
        break;

        case BGStyle::Inside:
        case BGStyle::InsideGlitchy: {
            for (uint8_t i = 0; i <= entering_pal_size; i++) {
                const int8_t t256 = engine::maths::sin(timer - i * (256 / entering_pal_size));
                const uint8_t b = 15 + bios_mathMulS16(t256, 15) / 256;
                engine::graphics::set_palette_colour(bg_pal_start + i, RGB555(0, 0, b));
            }
        } break;
    }
}

void bg_redraw() {
    PROFILE_SCOPE(bg_drw);

    using namespace engine::graphics;

    // Reset scroll.
    bitmap_1.scroll_x() = 0;

    switch (s_bg_style) {
        case BGStyle::Outside: {
            // Copy the wormhole to the first half of VRAM.
            static_assert(images::wormhole::width == SCREEN_WIDTH);
            static_assert(images::wormhole::height == SCREEN_HEIGHT);
            static_assert(images::wormhole::pal_offset == bg_pal_start);
            static_assert(images::wormhole::pal_size == bg_pal_count);
            images::wormhole::decompress(VDP.BITMAP_VRAM_8BIT);

            // Setup the image to be projected.
            static_assert(bg_pal_count == 8 * 8);
            for (uint8_t y = 0; y < 8; y++) {
                for (uint8_t x = 0; x < 8; x++) {
                    uint16_t col = (x == 0 || y == 0) ? RGB555(0,0,31) : RGB555(0,0,0);
                    s_bg_pallete[y * 8 + x] = col;
                }
            }
            //s_bg_pallete[2 * 8 + 6] = RGB555(15, 15, 15);
            //s_bg_pallete[6 * 8 + 6] = RGB555(15, 15, 15);
            //s_bg_pallete[3 * 8 + 2] = RGB555(15, 15, 15);
            //s_bg_pallete[4 * 8 + 2] = RGB555(15, 15, 15);
            //s_bg_pallete[5 * 8 + 2] = RGB555(15, 15, 15);


            // Same for the cover.

            // Setup palette.
            static_assert(engine::utils::size(images::cover_raw::palette) == cover_pal_count);
            static_assert(images::cover_raw::pal_offset == cover_pal_start);
            for (uint8_t i = 0; i < cover_pal_count; i++) {
                set_palette_colour(cover_pal_start + i, images::cover_raw::palette[i]);
            }

            // Copy data.
            static_assert(images::cover_raw::width == SCREEN_WIDTH);
            static_assert(images::cover_raw::height == SCREEN_HEIGHT);
            images::cover_raw::decompress(VDP.BITMAP_VRAM_8BIT + SCREEN_WIDTH * SCREEN_HEIGHT);
        } break;

        case BGStyle::Inside: {
            // TODO: this jumps around all over the place in memory writing 8bit values...
            for (int32_t t = 0; t <= entering_pal_size; t++) {
                for (int16_t ty = -entering_pal_size; ty < entering_pal_size; ty++) {
                    for (int16_t tx = -entering_pal_size; tx < entering_pal_size; tx++) {
                        const auto ax = engine::utils::abs(tx);
                        const auto ay = engine::utils::abs(ty);
                        const bool show = (ax == t && ay <= t) || (ay == t && ax <= t); // show only the square of radius t
                        if (show) {
                            bool which = false;
                            const uint8_t pal0 = bg_pal_start + t;
                            const uint8_t pal1 = bg_pal_start + entering_pal_size - t;
                            for (int32_t y = 0; y < SCREEN_HEIGHT; y += entering_pal_size * 2) {
                                for (int32_t x = 0; x < SCREEN_WIDTH; x += entering_pal_size * 2) {
                                    VDP.BITMAP_VRAM_8BIT[SCREEN_WIDTH * (y + entering_pal_size + ty) + (x + entering_pal_size + tx)] = which ? pal0 : pal1;
                                    which = !which;
                                }
                                which = !which;
                            }
                        }
                    }
                }
            }
        } break;

        case BGStyle::OutsideWavy:
        case BGStyle::InsideGlitchy:
            // Don't need to redraw these.
            break;
    }

    bg_pallete_update();
}

void bg_setup() {
    using namespace engine::graphics;

    // Single big image.
    bitmap_1.position_x() = 0;
    bitmap_1.position_y() = 0;
    bitmap_1.width() = SCREEN_WIDTH - 1;
    bitmap_1.height() = SCREEN_HEIGHT - 1;
    bitmap_1.scroll_x() = 0;
    bitmap_1.scroll_y() = 0;

    // Cover over it.
    bitmap_0.position_x() = 0;
    bitmap_0.position_y() = 0;
    bitmap_0.width() = SCREEN_WIDTH - 1;
    bitmap_0.height() = SCREEN_HEIGHT - 1;
    bitmap_0.scroll_x() = 0;
    bitmap_0.scroll_y() = SCREEN_HEIGHT;
}

void bg_update() {
    const uint16_t timer = s_bg_timer++;
    bg_pallete_update();

    switch (s_bg_style) {
        case BGStyle::Outside:
            engine::graphics::wait_until_line(engine::graphics::SCREEN_HEIGHT);
            break;

        case BGStyle::OutsideWavy:
            // TODO: need a smooth state to bring this in
            for (uint8_t line = 4; line < engine::graphics::SCREEN_HEIGHT; line += 4) {
                engine::graphics::wait_until_line(line);
                uint16_t shift = engine::maths::sin(timer + line) / 4;
                engine::graphics::bitmap_1.scroll_x() = shift;
            }
            break;

        case BGStyle::Inside:
            for (uint8_t line = 4; line < engine::graphics::SCREEN_HEIGHT; line += 4) {
                engine::graphics::wait_until_line(line);
                uint16_t shift = engine::maths::sin(timer + line) / 8;
                engine::graphics::bitmap_1.scroll_x() = shift;
            }
            break;

        case BGStyle::InsideGlitchy: {
            const uint32_t g0 = engine::utils::g_rng();
            const uint32_t g1 = engine::utils::g_rng();
            const uint8_t glitch_lines[8] {
                static_cast<uint8_t>((g0 >>  0) & 0xFF),
                static_cast<uint8_t>((g0 >>  8) & 0xFF),
                static_cast<uint8_t>((g0 >> 16) & 0xFF),
                static_cast<uint8_t>((g0 >> 24) & 0xFF),
                static_cast<uint8_t>((g1 >>  0) & 0xFF),
                static_cast<uint8_t>((g1 >>  8) & 0xFF),
                static_cast<uint8_t>((g1 >> 16) & 0xFF),
                static_cast<uint8_t>((g1 >> 24) & 0xFF),
            };
            for (uint8_t line = 4; line < engine::graphics::SCREEN_HEIGHT; line += 4) {
                engine::graphics::wait_until_line(line);
                uint16_t shift = engine::maths::sin(timer + line) / 8;
                for (uint8_t glitch : glitch_lines) {
                    if (line == glitch) {
                        shift = glitch * 3; //engine::graphics::SCREEN_WIDTH / 3;
                        break;
                    }
                }
                engine::graphics::bitmap_1.scroll_x() = shift;
            }
        } break;
    }
}

//

// 0 1 2
// 3 4 5
// 6 7 8
template <uint8_t A, uint8_t B> constexpr uint16_t make_mask() { return (1 << A) | (1 << B); }
constexpr uint16_t keypad_lines_A[] { make_mask<6,1>(), make_mask<1,8>(), make_mask<3,5>(), };
constexpr uint16_t keypad_lines_M[] { make_mask<6,0>(), make_mask<0,4>(), make_mask<4,2>(), make_mask<2,8>(), };
constexpr uint16_t keypad_lines_I[] { make_mask<1,7>(), };
constexpr uint16_t keypad_lines_C[] { make_mask<8,6>(), make_mask<6,0>(), make_mask<0,2>(), };
constexpr uint16_t keypad_lines_U[] { make_mask<2,8>(), make_mask<8,6>(), make_mask<6,0>(), };
constexpr uint16_t keypad_lines_T[] { make_mask<0,2>(), make_mask<1,7>(), };
constexpr uint16_t keypad_lines_E[] { make_mask<0,2>(), make_mask<3,5>(), make_mask<6,8>(), make_mask<0,6>(), };

const uint16_t * s_keypad_current_masks;
uint8_t s_keypad_current_masks_size;

uint16_t s_keypad_line_start_bit; // current line start bit
uint8_t s_keypad_lines_used; // number of lines used so far
uint8_t s_keypad_lines_matched; // how many lines matched
uint8_t s_keypad_total_line_mask; // bit N means that s_keypad_current_masks[N] has been matched

struct alignas(uint16_t) ButtonPos {
    uint8_t x;
    uint8_t y;

    constexpr auto aabb() const {
        return engine::utils::AABB{
            x, y,
            engine::graphics::bg_tile_size * 2, engine::graphics::bg_tile_size * 2,
        };
    }
};
constexpr auto button_positions = []{
    using namespace engine::graphics;
    engine::utils::Array<ButtonPos, 9> butts{};
    const int x_offset = -5; // to make it harder to drag down the middle.
    int idx = 0;
    for (int j = -1; j <= 1; j++) {
        for (int i = -1; i <= 1; i++) {
            const uint8_t x = (SCREEN_WIDTH / 2) + i * (SCREEN_WIDTH / 3) - bg_tile_size;
            const uint8_t y = (SCREEN_HEIGHT / 2) + j * (SCREEN_HEIGHT / 3) - bg_tile_size;
            butts[idx].x = x + x_offset;
            butts[idx].y = y;
            idx++;
        }
    }
    return butts;
}();

struct alignas(uint16_t) CursorPos {
    uint8_t x = 0;
    uint8_t y = 0;

    constexpr bool valid() const { return x > 0; }

    constexpr auto aabb() const {
        return engine::utils::AABB{
            x, y,
            engine::graphics::bg_tile_size, engine::graphics::bg_tile_size,
        };
    }
};
CursorPos s_cursor_cur;
CursorPos s_cursor_start;

void keypad_setup() {
    using namespace engine::graphics;

    // Setup the tiles.
    engine::utils::fast_memset8(get_tile_data(keypad_tile_off), pal_white, bg_tile_size * bg_tile_size);
    engine::utils::fast_memset8(get_tile_data(keypad_tile_on), pal_blue, bg_tile_size * bg_tile_size);
    engine::utils::fast_memset8(get_tile_data(keypad_tile_good), pal_green, bg_tile_size * bg_tile_size);
    engine::utils::fast_memset8(get_tile_data(keypad_tile_bad), pal_red, bg_tile_size * bg_tile_size);

    Pixel2 * cursor_data = get_tile_data(cursor_tile_start);
    engine::utils::fast_memset8(cursor_data, pal_transparent, bg_tile_size * bg_tile_size);
    constexpr int cursor_size = 2;
    for (int i = -cursor_size; i <= cursor_size; i++) {
        constexpr uint16_t pal16 = pal_grey | uint16_t{pal_grey} << 8;
        constexpr int center = 4;
        cursor_data[((center + i) + (center + 0) * 8) / 2] = pal16; // x
        cursor_data[((center + 0) + (center + i) * 8) / 2] = pal16; // y
    }

    // Slight offset so that it doesn't overlap.
    s_cursor_cur.x = SCREEN_WIDTH / 2 - 2 * bg_tile_size;
    s_cursor_cur.y = SCREEN_HEIGHT / 2;

    s_cursor_start = {};
}

void keypad_show() {
    using namespace engine::graphics;

    // Reuse the cover for the cursor and lines.
    engine::utils::fast_memset8(VDP.BITMAP_VRAM_8BIT + SCREEN_WIDTH * SCREEN_HEIGHT, pal_transparent, SCREEN_WIDTH * SCREEN_HEIGHT);
    bitmap_0.enable();
    bitmap_0.position_x() = 0;
    bitmap_0.position_y() = 0;
    bitmap_0.width() = SCREEN_WIDTH - 1;
    bitmap_0.height() = SCREEN_HEIGHT - 1;
    bitmap_0.scroll_x() = 0;
    bitmap_0.scroll_y() = SCREEN_HEIGHT;

    // Draw the buttons.
    ObjSprite sprite;
    sprite.set_tile_index(keypad_tile_off);
    static_assert(button_positions.size() * 4 == keypad_sprite_count);
    for (uint8_t i = 0; i < button_positions.size(); i++) {
        const auto & butt = button_positions[i];
        for (uint8_t y = 0; y < 2; y++) {
            for (uint8_t x = 0; x < 2; x++) {
                sprite.set_x(butt.x + x * bg_tile_size);
                sprite.set_y(butt.y + y * bg_tile_size);
                set_sprite(keypad_sprite_start + 4 * i + 2 * y + x, sprite);
            }
        }
    }
}

void keypad_hide() {
    using namespace engine::graphics;

    // Hide the lines.
    bitmap_0.disable();

    // Hide the buttons.
    ObjSprite sprite;
    for (uint8_t i = 0; i < keypad_sprite_count; i++) {
        set_sprite(keypad_sprite_start + i, sprite);
    }
}

void draw_web_line(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint8_t pal) {
    PROFILE_SCOPE(dr_lin);
    using namespace engine::graphics;

    // Reusing cover data.
    auto * data = VDP.BITMAP_VRAM_8BIT + SCREEN_WIDTH * SCREEN_HEIGHT;

    // Move the cursor to the center of the tile.
    constexpr uint8_t offset = bg_tile_size / 2;
    data += offset + offset * SCREEN_WIDTH;

    engine::utils::draw_line(
        data, SCREEN_WIDTH,
        x0, y0,
        x1, y1,
        pal
    );
}

void cursor_update() {
    using namespace engine::graphics;
    PROFILE_SCOPE(cr_upd);

    const auto old_pos = s_cursor_cur;
    bool changed = false;

    // Handle cursor movement.
    const auto held = engine::input::g_buttons_held;
    if (held & GAMEPAD_BTN_LEFT) {
        s_cursor_cur.x -= 2;
        changed = true;
    } else if (held & GAMEPAD_BTN_RIGHT) {
        s_cursor_cur.x += 2;
        changed = true;
    }
    if (held & GAMEPAD_BTN_UP) {
        s_cursor_cur.y -= 2;
        changed = true;
    } else if (held & GAMEPAD_BTN_DOWN) {
        s_cursor_cur.y += 2;
        changed = true;
    }

    // Pull the cursor towards the center.
    {
        constexpr uint8_t center_x = (SCREEN_WIDTH - bg_tile_size) / 2;
        constexpr uint8_t center_y = 160 - bg_tile_size / 2;
        const int16_t dx = s_cursor_cur.x - center_x;
        const int16_t dy = s_cursor_cur.y - center_y;
        const auto adx = engine::utils::abs(dx);
        const auto ady = engine::utils::abs(dy);
        constexpr int16_t r = 32;

        uint16_t tx = 1; // how often to change
        uint16_t &ty = tx;
        if (adx + ady > r) tx = 3;

        int16_t ddx = 0; // which direction
        if (dx > 0) ddx = -1; else if (dx < 0) ddx = 1;
        if (tx && (s_bg_timer & tx) == tx) {
            s_cursor_cur.x -= ddx;
            changed = true;
        }

        int16_t ddy = 0;
        if (dy > 0) ddy = -1; else if (dy < 0) ddy = 1;
        if (ty && (s_bg_timer & ty) == ty) {
            s_cursor_cur.y -= ddy;
            changed = true;
        }
    }

    // Clamp it.
    constexpr uint8_t screen_padding = 3;
    s_cursor_cur.x = engine::utils::clamp<uint8_t>(
        s_cursor_cur.x,
        screen_padding,
        SCREEN_WIDTH - bg_tile_size - screen_padding
    );
    s_cursor_cur.y = engine::utils::clamp<uint8_t>(
        s_cursor_cur.y,
        screen_padding,
        SCREEN_HEIGHT - bg_tile_size - screen_padding
    );

    if (s_cursor_start.valid() && changed) {
        // Undraw old line.
        draw_web_line(
            s_cursor_start.x, s_cursor_start.y,
            old_pos.x, old_pos.y,
            pal_transparent
        );

        // Draw the new line.
        draw_web_line(
            s_cursor_start.x, s_cursor_start.y,
            s_cursor_cur.x, s_cursor_cur.y,
            pal_grey
        );
    }

    // Move the cursor.
    ObjSprite sprite;
    sprite.set_tile_index(cursor_tile_start);
    sprite.set_x(s_cursor_cur.x);
    sprite.set_y(s_cursor_cur.y);
    set_sprite(cursor_sprite_start, sprite);
}

uint8_t s_keypad_fade;
void keypad_fade_draw(uint8_t pal) {
    auto undraw = [pal](const auto& lines) {
        for (auto & line : lines) {
            draw_web_line(line.x0, line.y0, line.x1, line.y1, pal);
        }
    };
    switch (s_code_state) {
        case CodeState::CodeA: undraw(g_lines_A); break;
        case CodeState::CodeM: undraw(g_lines_M); break;
        case CodeState::CodeI: undraw(g_lines_I); break;
        case CodeState::CodeC: undraw(g_lines_C); break;
        case CodeState::CodeU: undraw(g_lines_U); break;
        case CodeState::CodeT: undraw(g_lines_T); break;
        case CodeState::CodeE: undraw(g_lines_E); break;

        case CodeState::CodeMid: ASSERT(false); break;
    }
}
void keypad_fade_enter() {
    s_keypad_fade = 0;

    // Redraw the lines as the fade palette.
    keypad_fade_draw(pal_good_bad);
}
void keypad_fade_result(bool success) {
    const uint8_t t = s_keypad_fade++;
    if (t < 60) {
        // Fade the colour.
        const uint8_t c = (60 - t) / 2;
        const uint16_t rgb = success ? RGB555(0, c, 0) : RGB555(c, 0, 0);
        engine::graphics::set_palette_colour(pal_good_bad, rgb);
        return;
    }

    // Undraw the lines.
    keypad_fade_draw(engine::graphics::pal_transparent);

    if (success) {
        // Advance to next stage.
        switch (s_code_state) {
            case CodeState::CodeA:
            case CodeState::CodeM:
            case CodeState::CodeI:
            case CodeState::CodeC:
            case CodeState::CodeU:
            case CodeState::CodeT:
                if (s_code_state == CodeState::CodeI) {
                    s_wormhole_speed = WormSpeed::SpinSlow;
                }
                s_code_state = static_cast<CodeState>((uint8_t)s_code_state + 1);
                level_advance(LevelState::OutsideTalk);
                break;
            case CodeState::CodeE:
                s_wormhole_speed = WormSpeed::SpinFast;
                level_advance(LevelState::OutsideFinished);
                break;

            case CodeState::CodeMid: ASSERT(false); break;
        }
    } else {
        // Repeat the help.
        level_advance(LevelState::OutsideTalk);
    }
}

void keypad_update() {
    cursor_update();

    const auto pressed_a = engine::input::g_buttons_pressed & GAMEPAD_BTN_A;
    const bool used_all_lines = s_keypad_lines_used == s_keypad_current_masks_size;

    // Don't trigger a solve while A is pressed or it'll skip to the next line of speech.
    if (used_all_lines && !pressed_a) {
        DEBUG_MSG("Total matched: ", s_keypad_lines_matched);
        if (s_keypad_current_masks_size == s_keypad_lines_matched) {
            level_advance(LevelState::OutsideGood);
        } else {
            level_advance(LevelState::OutsideBad);
        }
        return;
    }

    if (pressed_a && !used_all_lines) {
        // See if we can start/finish a line.
        const auto cursor_aabb = s_cursor_cur.aabb();
        constexpr uint8_t num_butts = button_positions.size();
        for (uint8_t i = 0; i < num_butts; i++) {
            const uint16_t butt_bit = 1 << i;
            const auto & butt = button_positions[i];
            if (cursor_aabb.intersects(butt.aabb())) {
                if (s_cursor_start.valid()) {
                    // Read and clear mask;
                    const uint16_t line_mask = s_keypad_line_start_bit | butt_bit;
                    s_keypad_line_start_bit = 0;

                    bool keep = false;
                    // Ignore lines that go back to themselves.
                    if (line_mask != butt_bit) {
                        keep = true;

                        // Check for a match.
                        for (uint8_t mask_idx = 0; mask_idx < s_keypad_current_masks_size; mask_idx++) {
                            const uint16_t mask_bit = 1 << mask_idx;
                            const uint16_t mask = s_keypad_current_masks[mask_idx];
                            if (mask == line_mask) {
                                // See if we already have this one.
                                if (s_keypad_total_line_mask & mask_bit) {
                                    // We already have this one.
                                    DEBUG_MSG("Already added ", mask_bit);
                                } else {
                                    // Add it.
                                    DEBUG_MSG("Matched ", mask_bit);
                                    s_keypad_total_line_mask |= mask_bit;
                                    s_keypad_lines_matched++;
                                }
                                break;
                            }
                        }
                    }

                    if (!keep) {
                        DEBUG_MSG("Didn't add");
                        // Didn't count, remove it.
                        draw_web_line(
                            s_cursor_start.x, s_cursor_start.y,
                            s_cursor_cur.x, s_cursor_cur.y,
                            engine::graphics::pal_transparent
                        );

                    } else {
                        DEBUG_MSG("Count increased");
                        const auto idx = s_keypad_lines_used++;

                        // Add the line that was drawn.
                        const Line line{
                            s_cursor_start.x, s_cursor_start.y,
                            s_cursor_cur.x, s_cursor_cur.y,
                        };
                        switch (s_code_state) {
                            case CodeState::CodeA: g_lines_A[idx] = line; break;
                            case CodeState::CodeM: g_lines_M[idx] = line; break;
                            case CodeState::CodeI: g_lines_I[idx] = line; break;
                            case CodeState::CodeC: g_lines_C[idx] = line; break;
                            case CodeState::CodeU: g_lines_U[idx] = line; break;
                            case CodeState::CodeT: g_lines_T[idx] = line; break;
                            case CodeState::CodeE: g_lines_E[idx] = line; break;

                            case CodeState::CodeMid: ASSERT(false); break;
                        }
                    }

                    s_cursor_start = {};

                } else {
                    // Start a new line.
                    s_cursor_start = s_cursor_cur;
                    s_keypad_line_start_bit = butt_bit;
                }

                // No other buttons will collide.
                break;
            }
        }
    }
}

//

// TODO: this is a dupe of the text from driving

enum class UIC : uint8_t { None, Ami, Bucko, };
void ui_character(UIC voice) {
    using namespace engine::graphics;

    // All characters have the same sprites.
    static_assert(ami_left_sprite_start == bucko_left_sprite_start);
    constexpr uint8_t sprite_start = ami_left_sprite_start;

    constexpr uint8_t left_start_x = 0;
    constexpr uint8_t left_start_y = SCREEN_HEIGHT - bg_tile_size * voice_char_height;
    constexpr uint8_t right_start_x = SCREEN_WIDTH - bg_tile_size * voice_char_width;
    constexpr uint8_t right_start_y = left_start_y;

    ObjSprite sprite;
    uint8_t sprite_idx = 0;
    switch (voice) {
        case UIC::None:
            // No voice.
            for (int y = 0; y < voice_char_height; y++) {
                for (int x = 0; x < voice_char_width; x++) {
                    set_sprite(sprite_start + sprite_idx, sprite);
                    sprite_idx++;
                }
            }
            break;
        case UIC::Ami:
            for (int y = 0; y < voice_char_height; y++) {
                for (int x = 0; x < voice_char_width; x++) {
                    sprite.set_x(left_start_x + x * bg_tile_size);
                    sprite.set_y(left_start_y + y * bg_tile_size);
                    sprite.set_tile_index(ami_left_tile_start + sprite_idx);
                    set_sprite(sprite_start + sprite_idx, sprite);
                    sprite_idx++;
                }
            }
            break;
        case UIC::Bucko:
            // Tiles need a flip, and the x co-ord too.
            for (int y = 0; y < voice_char_height; y++) {
                for (int x = voice_char_width - 1; x >= 0; x--) {
                    sprite.set_x(right_start_x + x * bg_tile_size);
                    sprite.set_y(right_start_y + y * bg_tile_size);
                    sprite.set_tile_index(bucko_left_tile_start + sprite_idx);
                    sprite.set_x_flip(true);
                    set_sprite(sprite_start + sprite_idx, sprite);
                    sprite_idx++;
                }
            }
            break;
    }
}

struct Speech {
    UIC uic;
    uint8_t len;
    const char * text;

    template <uint8_t N>
    constexpr Speech(UIC c, const char (&str)[N]) : uic(c), len(N), text(str) { static_assert(N <= 27); }
    constexpr Speech(decltype(nullptr)) : uic(UIC::None), len(0), text(nullptr) {}
};

constexpr Speech start_text[] {
    { UIC::Bucko, "Ami!" },
#if !SKIP_STUFF
    { UIC::Bucko, "Can you still hear me?" },
    { UIC::Bucko, "The reception near the" },
    { UIC::Bucko, "dome has gotten worse" },
    { UIC::Bucko, "since the takeover." },
    { UIC::Ami, "All clear here." },
    { UIC::Bucko, "Great!" },
    { UIC::Bucko, "Near your location there" },
    { UIC::Bucko, "should be an unassuming" },
    { UIC::Bucko, "and well hidden vent." },
    { UIC::Bucko, "It might be hidden behind" },
    { UIC::Bucko, "a poorly drawn fence." },
    { UIC::Bucko, "Do you see it?" },
    { UIC::Ami, "..." },
    { UIC::Ami, "I see it." },
    { UIC::Bucko, "Great!" },
    { UIC::Bucko, "I'll open it from here." },
#endif

    nullptr,
};

constexpr Speech revealed_text[] {
    { UIC::Ami, "Whoa!" },
    { UIC::Bucko, "We call this" },
#if !SKIP_STUFF
    { UIC::Bucko, "The W.E.B." },
    { UIC::Bucko, "I don't know what it" },
    { UIC::Bucko, "stands for." },
    { UIC::Bucko, "But apparently it's" },
    { UIC::Bucko, "really important to" },
    { UIC::Bucko, "everything in buckopia." },
    { UIC::Bucko, "Every time I ask the" },
    { UIC::Bucko, "elder buckos about it" },
    { UIC::Bucko, "they start talking about" },
    { UIC::Bucko, "lawn mowers and" },
    { UIC::Bucko, "mainframes." },
    { UIC::Ami, "..." },
    { UIC::Bucko, "Anyway." },
    { UIC::Bucko, "You'll need to get in" },
    { UIC::Bucko, "there and reboot! it." },
    { UIC::Bucko, "That should reset" },
    { UIC::Bucko, "the robuckos!" },
    { UIC::Ami, "OK." },
    { UIC::Ami, "How do I do that?" },
#endif

    nullptr,
};

constexpr Speech keypad_text[] {
    { UIC::Bucko, "Do you see a keypad" },
#if !SKIP_STUFF
    { UIC::Bucko, "with 9 buttons?" },
    { UIC::Ami, "I see 9... somethings." },
    { UIC::Bucko, "They're probably it." },
    { UIC::Bucko, "It looks like you need" },
    { UIC::Bucko, "to join them together" },
    { UIC::Bucko, "in certain patterns." },
    { UIC::Bucko, "I have the codes here" },
    { UIC::Bucko, "but they're written in" },
    { UIC::Bucko, "an older script which" },
    { UIC::Bucko, "is difficult to read." },
#endif

    nullptr,
};

constexpr Speech code_A_text[] {
    { UIC::Bucko, "It looks like the first" },
#if !SKIP_STUFF
    { UIC::Bucko, "one is formed of 3 lines." },
    { UIC::Bucko, "They're in the shape of a" },
    { UIC::Bucko, "'V' that's been" },
    { UIC::Bucko, "crossed out." },
    { UIC::Bucko, "Oh!" },
    { UIC::Bucko, "It could be upside down." },
#endif

    nullptr,
};

constexpr Speech code_M_text[] {
    { UIC::Bucko, "Attagirl!" },
#if !SKIP_STUFF
    { UIC::Bucko, "That was it!" },
    { UIC::Bucko, "The next one is clearly" },
    { UIC::Bucko, "4 lines in the shape of" },
    { UIC::Bucko, "a 'W'." },
    { UIC::Bucko, "..." },
    { UIC::Bucko, "Wait!" },
    { UIC::Bucko, "This one's upside" },
    { UIC::Bucko, "down too!" },
#endif

    nullptr,
};

constexpr Speech code_I_text[] {
    { UIC::Bucko, "I think the next one" },
    { UIC::Bucko, "is a number?" },
    { UIC::Bucko, "But it only has" },
    { UIC::Bucko, "one line." },

    nullptr,
};

constexpr Speech code_mid_text[] {
    { UIC::None, "Stage 1 emitters online" },
    { UIC::Ami, "What's happening?!" },
    { UIC::Bucko, "You're entering access" },
    { UIC::Bucko, "codes into a machine." },
    { UIC::Ami, "..." },

    nullptr,
};

constexpr Speech code_C_text[] {
    { UIC::Bucko, "This next one looks like" },
    { UIC::Bucko, "a big square" },
    { UIC::Bucko, "but the front" },
    { UIC::Bucko, "has fallen off." },

    nullptr,
};

constexpr Speech code_U_text[] {
    { UIC::Bucko, "Another big square" },
    { UIC::Bucko, "but this one" },
    { UIC::Bucko, "has no roof." },

    nullptr,
};

constexpr Speech code_T_text[] {
    { UIC::Bucko, "Some bucko took a bite" },
#if !SKIP_STUFF
    { UIC::Bucko, "out of the remaining" },
    { UIC::Bucko, "pages!" },
    { UIC::Bucko, "I can't tell what" },
    { UIC::Bucko, "this one is meant" },
    { UIC::Bucko, "to be." },
    { UIC::Bucko, "Maybe a plus sign" },
    { UIC::Bucko, "but without the top?" },
#endif

    nullptr,
};

constexpr Speech code_E_text[] {
    { UIC::Bucko, "It's hard to tell" },
    { UIC::Bucko, "but this might be a" },
    { UIC::Bucko, "wide number three" },
    { UIC::Bucko, "facing the wrong way." },

    nullptr,
};

constexpr Speech code_finished_text[] {
    { UIC::None, "Stage 2 emitters activated" },
#if !SKIP_STUFF
    { UIC::Bucko, "Attagirl!" },
    { UIC::Bucko, "Time to jump in!" },
    { UIC::Ami, "That looks scary." },
    { UIC::Bucko, "Don't worry!" },
    { UIC::Bucko, "It's a powerful vortex" },
    { UIC::Bucko, "that will pull you in" },
    { UIC::Bucko, "even if you..." },
#endif

    nullptr,
};

constexpr Speech inside_text[] {
    { UIC::Bucko, "We're in!" },
#if !SKIP_STUFF
    { UIC::Bucko, "Well you are." },
    { UIC::Bucko, "What's it like in there?" },
    { UIC::Bucko, "Do they have cake?" },

    { UIC::Ami, "I... no." },
#endif

    nullptr,
};

constexpr Speech glitchy_text[] {
    { UIC::Bucko, "Somethings off." },
    { UIC::Bucko, "Somethings off" },

    nullptr,
};

const Speech * s_current_speech;
LevelState s_next_state;

void ui_redraw() {
    // Empty it out.
    font::clear_text();

    constexpr uint8_t speech_y = engine::graphics::SCREEN_HEIGHT * 2 / 3;
    constexpr uint8_t speech_sky_y = engine::graphics::SCREEN_HEIGHT / 5;

    // Show any speech if it's active.
    const auto & speech = *s_current_speech;
    ui_character(speech.uic);
    if (speech.text) {
        switch (speech.uic) {
            case UIC::Ami:
                font::write_left(speech.text, speech.len, 0, speech_y);
                break;
            case UIC::Bucko:
                font::write_right(speech.text, speech.len, 0, speech_y);
                break;
            case UIC::None:
                font::write_centered(speech.text, speech.len, speech_sky_y);
                break;
        }
    }
}

void ui_update() {
    PROFILE_SCOPE(ui_upd);

    // Do nothing if we're not in a UI state.
    bool is_ui = false;
    switch (s_level_state) {
        case LevelState::Start:
        case LevelState::RevealFinished:
        case LevelState::KeypadShown:
        case LevelState::OutsideTalk:
        case LevelState::OutsideFinished:
        case LevelState::Inside:
        case LevelState::InsideGlitchy:
            is_ui = true;
            break;
        case LevelState::Reveal:
        case LevelState::OutsideCode:
        case LevelState::OutsideGood:
        case LevelState::OutsideBad:
        case LevelState::Entering:
        case LevelState::Finished:
            break;
    }
    if (!is_ui) return;

    if (engine::input::g_buttons_pressed & GAMEPAD_BTN_A) {
        // Advance to next line.
        auto *speech = ++s_current_speech;
        if (speech->text == nullptr) {
            // We're finished, change state.
            level_advance(s_next_state);
        } else {
            ui_redraw();
        }
    }
}

void ui_setup() {
    game::font::setup_tiles<font_tile_start, font_sprite_start>();

    // Character sprites.
    images::copy_tile_data<
        bucko_left_pal_start, bucko_left_pal_count,
        bucko_left_tile_start, bucko_left_tile_count,
        images::bucko_left
    >();
    images::copy_tile_data<
        ami_left_pal_start, ami_left_pal_count,
        ami_left_tile_start, ami_left_tile_count,
        images::ami_left
    >();
}

//

bool update_logic() {
    bool finished = false;

    switch (s_level_state) {
        case LevelState::Start:
        case LevelState::RevealFinished:
        case LevelState::KeypadShown:
        case LevelState::OutsideTalk:
        case LevelState::OutsideFinished:
        case LevelState::Inside:
        case LevelState::InsideGlitchy:
            // Event driven logic.
            break;

        case LevelState::Reveal: {
            // Remove the cover.
            constexpr uint16_t dh = SKIP_STUFF ? 10 : 1;
            engine::graphics::bitmap_0.scroll_y() += dh;
            engine::graphics::bitmap_0.height() -= dh;
            uint16_t h = engine::graphics::bitmap_0.height();
            if (h < dh) {
                level_advance(LevelState::RevealFinished);
            }
        } break;

        case LevelState::OutsideCode:
            keypad_update();
            break;

        case LevelState::OutsideGood:
            keypad_fade_result(true);
            break;
        case LevelState::OutsideBad:
            keypad_fade_result(false);
            break;

        case LevelState::Entering:
            if (s_bg_timer > 4 * 60) {
                level_advance(LevelState::Inside);
            }
            break;

        case LevelState::Finished:
            finished = true;
            break;
    }

    return finished;
}

//

void level_advance(LevelState state) {
    DEBUG_MSG("state:", AS_INT(state));
    s_level_state = state;
    switch (state) {
        case LevelState::Start:
            s_current_speech = start_text;
            s_next_state = LevelState::Reveal;
            s_bg_style = BGStyle::Outside;
            s_wormhole_speed = WormSpeed::None;
            s_bg_timer = 0;
            bg_redraw();
            break;

        case LevelState::Reveal:
            // Nothing to do.
            break;

        case LevelState::RevealFinished:
            s_current_speech = revealed_text;
            s_next_state = LevelState::KeypadShown;
            // Hide the cover completely.
            engine::graphics::bitmap_0.disable();
            break;

        case LevelState::KeypadShown:
            s_current_speech = keypad_text;
            s_next_state = LevelState::OutsideTalk;
            // Start on the codes.
            s_code_state = CodeState::CodeA;
            keypad_show();
            break;

        case LevelState::OutsideTalk: {
            s_next_state = LevelState::OutsideCode;
            auto set_keys = [](const auto & masks) {
                s_keypad_current_masks = masks;
                s_keypad_current_masks_size = engine::utils::size(masks);
            };
            switch (s_code_state) {
                case CodeState::CodeA: s_current_speech = code_A_text; set_keys(keypad_lines_A); break;
                case CodeState::CodeM: s_current_speech = code_M_text; set_keys(keypad_lines_M); break;
                case CodeState::CodeI: s_current_speech = code_I_text; set_keys(keypad_lines_I); break;
                case CodeState::CodeC: s_current_speech = code_C_text; set_keys(keypad_lines_C); break;
                case CodeState::CodeU: s_current_speech = code_U_text; set_keys(keypad_lines_U); break;
                case CodeState::CodeT: s_current_speech = code_T_text; set_keys(keypad_lines_T); break;
                case CodeState::CodeE: s_current_speech = code_E_text; set_keys(keypad_lines_E); break;

                case CodeState::CodeMid:
                    // HACK: jump back to this state with different text.
                    s_current_speech = code_mid_text;
                    s_next_state = LevelState::OutsideTalk;
                    s_code_state = CodeState::CodeC;
                    break;
            }
        } break;

        case LevelState::OutsideCode:
            // Reset state.
            s_keypad_line_start_bit = 0;
            s_keypad_lines_used = 0;
            s_keypad_lines_matched = 0;
            s_keypad_total_line_mask = 0;
            break;

        case LevelState::OutsideGood:
        case LevelState::OutsideBad:
            keypad_fade_enter();
            break;

        case LevelState::OutsideFinished:
            s_current_speech = code_finished_text;
            s_next_state = LevelState::Entering;
            keypad_hide();
            break;

        case LevelState::Entering:
            s_wormhole_speed = WormSpeed::SpinFastEnterFast; // or slow?
            // Reset timer for countdown.
            s_bg_timer = 0;
            break;

        case LevelState::Inside:
            s_current_speech = inside_text;
            s_next_state = LevelState::InsideGlitchy;
            s_bg_style = BGStyle::Inside;
            bg_redraw();
            break;
        case LevelState::InsideGlitchy:
            s_current_speech = glitchy_text;
            s_next_state = LevelState::Finished;
            s_bg_style = BGStyle::InsideGlitchy;
            break;

        case LevelState::Finished:
            // Logic will finish this off.
            break;
    }
    ui_redraw();
}

} // namespace

engine::utils::Array<Line, 3> g_lines_A;
engine::utils::Array<Line, 4> g_lines_M;
engine::utils::Array<Line, 1> g_lines_I;
engine::utils::Array<Line, 3> g_lines_C;
engine::utils::Array<Line, 3> g_lines_U;
engine::utils::Array<Line, 2> g_lines_T;
engine::utils::Array<Line, 4> g_lines_E;

void enter() {
    // Setup palette.
    engine::graphics::set_palette_colour(pal_black, RGB555(0,0,0));
    engine::graphics::set_palette_colour(pal_white, RGB555(31,31,31));
    engine::graphics::set_palette_colour(pal_red, RGB555(31,0,0));
    engine::graphics::set_palette_colour(pal_green, RGB555(0,31,0));
    engine::graphics::set_palette_colour(pal_blue, RGB555(0,0,31));
    engine::graphics::set_palette_colour(pal_grey, RGB555(20,20,20));

    // Draw bits.
    bg_setup();
    ui_setup();
    keypad_setup();

    // Initial state.
    level_advance(LevelState::Start);

    // Show everything now that it's drawn.
    bios_vsync();
    engine::graphics::bitmap_0.enable();
    engine::graphics::bitmap_1.enable();
    engine::graphics::enable_sprites();

    // Kick off the bgm.
    engine::sound::play_bgm(game::music::Bgm::Bgm_Weird);
}

void leave() {
    // Reset graphics state.
    bios_vsync();
    engine::graphics::disable_sprites();
    engine::graphics::bitmap_0.disable();
    engine::graphics::bitmap_1.disable();
    engine::graphics::reset_sprites<ami_left_sprite_start + ami_left_sprite_count>();
    font::clear_text();

    // Reset sound.
    engine::sound::stop_bgm();
}

Entry loop() {
    Entry next = Entry::Cyber;

    // Wait for vsync.
    {
        PROFILE_SCOPE(vsync);
        engine::graphics::wait_until_line0();
    }

    // Update gamepad/mouse input.
    engine::input::update_inputs();

    // Return to the main menu if requested.
    if (engine::input::g_buttons_pressed & GAMEPAD_BTN_START) {
        return Entry::MainMenu;
    }

    if (update_logic()) {
        return Entry::Winner;
    }

    bg_update();
    ui_update();

    engine::profiler::print_timings();
    return next;
}

} // namespace game::cyber
