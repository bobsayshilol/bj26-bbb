#include "draw_line.h"
#include "font.h"
#include "game.h"
#include "graphics.h"
#include "input.h"
#include "memory.h"
#include "music.h"
#include "profiler.h"
#include "sound.h"

namespace game::winner {

namespace {

PROFILE_STORAGE(vsync);

constexpr uint8_t pal_black = 1;
constexpr uint8_t pal_white = 2;
constexpr uint8_t pal_basic_end = 3;

// font has highest prio.
constexpr uint8_t font_sprite_start = 0;
constexpr uint8_t font_sprite_count = font::font_max_sprites;
constexpr uint8_t font_tile_start = 0;
constexpr uint8_t font_tile_count = font::font_tile_count;

// Empty space for palette up to 24.

static_assert(pal_basic_end <= images::bucko_left::pal_offset);

constexpr uint8_t voice_char_width = 3;
constexpr uint8_t voice_char_height = 4;

// Bit awkward, but bucko_left must be at a fixed offset (from breakout).
constexpr uint8_t bucko_left_pal_start = images::bucko_left::pal_offset;
constexpr uint8_t bucko_left_pal_count = 10;
constexpr uint8_t bucko_left_sprite_start = font_sprite_start + font_sprite_count;
constexpr uint8_t bucko_left_sprite_count = voice_char_width * voice_char_height;
constexpr uint8_t bucko_left_tile_start = font_tile_start + font_tile_count;
constexpr uint8_t bucko_left_tile_count = bucko_left_sprite_count;

constexpr uint8_t ami_left_pal_start = bucko_left_pal_start + bucko_left_pal_count;
constexpr uint8_t ami_left_pal_count = 10;
constexpr uint8_t ami_left_sprite_start = bucko_left_sprite_start; // reuse the bucko sprites
constexpr uint8_t ami_left_sprite_count = bucko_left_sprite_count;
constexpr uint8_t ami_left_tile_start = bucko_left_tile_start + bucko_left_tile_count;
constexpr uint8_t ami_left_tile_count = ami_left_sprite_count;

//

constexpr uint16_t printout_start = engine::graphics::SCREEN_WIDTH * engine::graphics::SCREEN_HEIGHT;
constexpr uint16_t printout_width = 256;
constexpr uint16_t printout_height = 224;

//

enum class LevelState {
    Text1,
    Text1b,
    Print,
    Printing,
    Printed,
    Text2,
    Finished,
} s_level_state;

void level_advance(LevelState state);

//

// Stolen from LoopyMSE.
constexpr static int PRINT_STATUS_SUCCESS = 0;
constexpr static int PRINT_STATUS_GENERAL_FAILURE = 1;
constexpr static int PRINT_STATUS_NO_SEAL_CART = 2;
constexpr static int PRINT_STATUS_CANCELLED = 3;
constexpr static int PRINT_STATUS_PAPER_JAM = 4;
constexpr static int PRINT_STATUS_OVERHEAT = 5;
int s_print_error = -1;

//

void bg_show_codes() {
    engine::graphics::bitmap_0.enable();
}

void bg_setup() {
    using namespace engine::graphics;

    // Point bitmap0 at the text.
    bitmap_0.disable();
    bitmap_0.position_x() = 0;
    bitmap_0.position_y() = 0;
    bitmap_0.width() = printout_width - 1;
    bitmap_0.height() = printout_height - 1;
    bitmap_0.scroll_x() = 0;
    bitmap_0.scroll_y() = SCREEN_HEIGHT + printout_height / 8;

#if 0
    cyber::g_lines_A[0] = { 0x24, 0xAE, 0x7C, 0x28, };
    cyber::g_lines_A[1] = { 0x7C, 0x28, 0xD6, 0xBA, };
    cyber::g_lines_A[2] = { 0xD0, 0x6E, 0x22, 0x6E, };

    cyber::g_lines_M[0] = { 0x28, 0xB8, 0x28, 0x26, };
    cyber::g_lines_M[1] = { 0x28, 0x26, 0x80, 0x6E, };
    cyber::g_lines_M[2] = { 0x80, 0x6E, 0xCA, 0x2A, };
    cyber::g_lines_M[3] = { 0xCA, 0x2A, 0xD0, 0xB6, };

    cyber::g_lines_I[0] = { 0x7C, 0xB6, 0x7C, 0x2A, };

    cyber::g_lines_C[0] = { 0xD0, 0x1E, 0x2A, 0x20, };
    cyber::g_lines_C[1] = { 0x2A, 0x22, 0x2A, 0xBE, };
    cyber::g_lines_C[2] = { 0x2A, 0xBE, 0xD6, 0xB8, };

    cyber::g_lines_U[0] = { 0xD6, 0xB8, 0xD4, 0x1C, };
    cyber::g_lines_U[1] = { 0x20, 0x26, 0x20, 0xBC, };
    cyber::g_lines_U[2] = { 0x20, 0xBC, 0xD4, 0xB4, };

    cyber::g_lines_T[0] = { 0x7E, 0xB4, 0x7E, 0x22, };
    cyber::g_lines_T[1] = { 0xCE, 0x22, 0x24, 0x22, };

    cyber::g_lines_E[0] = { 0x24, 0x22, 0x24, 0xB0, };
    cyber::g_lines_E[1] = { 0x24, 0xB0, 0xD0, 0xB0, };
    cyber::g_lines_E[2] = { 0xD0, 0x6E, 0x26, 0x6A, };
    cyber::g_lines_E[3] = { 0x28, 0x1A, 0xDC, 0x20, };
#endif

    // Clear out the printout state.
    uint8_t * const printout_data = VDP.BITMAP_VRAM_8BIT + printout_start;
    engine::utils::fast_memset8(printout_data, pal_transparent, printout_width * printout_height);

    // Draw the lines
    constexpr uint8_t letters_scale = 4;
    const auto draw_lines = [&](const auto& lines, uint8_t x, uint8_t y) {
        for (const auto & line : lines) {
            engine::utils::draw_line(
                printout_data, printout_width,
                x + line.x0 / letters_scale, y + line.y0 / letters_scale,
                x + line.x1 / letters_scale, y + line.y1 / letters_scale,
                pal_white
            );
        }
    };
    draw_lines(cyber::g_lines_A, printout_width * 1 / (2 * letters_scale), printout_height * 1 / letters_scale);
    draw_lines(cyber::g_lines_M, printout_width * 3 / (2 * letters_scale), printout_height * 1 / letters_scale);
    draw_lines(cyber::g_lines_I, printout_width * 5 / (2 * letters_scale), printout_height * 1 / letters_scale);
    draw_lines(cyber::g_lines_C, printout_width * 0 / (2 * letters_scale), printout_height * 2 / letters_scale);
    draw_lines(cyber::g_lines_U, printout_width * 2 / (2 * letters_scale), printout_height * 2 / letters_scale);
    draw_lines(cyber::g_lines_T, printout_width * 4 / (2 * letters_scale), printout_height * 2 / letters_scale);
    draw_lines(cyber::g_lines_E, printout_width * 6 / (2 * letters_scale), printout_height * 2 / letters_scale);
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

//

constexpr Speech text1_text[] {
    { UIC::Bucko, "Ami!" },
    { UIC::Bucko, "Welcome back!" },
    { UIC::Bucko, "How was your trip" },
    { UIC::Bucko, "into the W.E.B.?" },

    { UIC::Ami, "I... don't remember." },

    { UIC::Bucko, "That's normal for your" },
    { UIC::Bucko, "first time." },
    { UIC::Bucko, "And probably for the best." },
    { UIC::Bucko, "I'd be on the chopping" },
    { UIC::Bucko, "block if the other buckos" },
    { UIC::Bucko, "knew that you'd seen" },
    { UIC::Bucko, "inside the dome." },

    { UIC::Ami, "I think I saw Gex." },
    { UIC::Bucko, "..." },
    { UIC::Ami, "And what was that whole" },
    { UIC::Ami, "thing thing with the" },
    { UIC::Bucko, "Well it's a shame that" },
    { UIC::Bucko, "you don't remember" },
    { UIC::Bucko, "anything from your trip!" },
    { UIC::Ami, "But" },
    { UIC::Bucko, "Not a single thing." },
    { UIC::Bucko, "Such a shame." },
    { UIC::Ami, "..." },

    { UIC::Bucko, "Anyway." },
    { UIC::Bucko, "Thanks to you the robuckos" },
    { UIC::Bucko, "are no longer a problem!" },
    { UIC::Bucko, "And Buckopia can go back" },
    { UIC::Bucko, "to how it was before." },

    // TODO: confetti
    { UIC::None, "pretend that there's" },
    { UIC::None, "confetti falling here." },

    { UIC::Bucko, "The old scriptures for" },
    { UIC::Bucko, "the W.E.B. are basically" },
    { UIC::Bucko, "unreadable at this point." },
    { UIC::Bucko, "So I saved the codes" },
    { UIC::Bucko, "that you entered." },

    nullptr,
};

constexpr Speech text1b_text[] {
    { UIC::Bucko, "I'll have to ask" },
    { UIC::Bucko, "the scribes what" },
    { UIC::Bucko, "these moon runes" },
    { UIC::Bucko, "could mean." },
    { UIC::Ami, "..." },
    { UIC::Bucko, "But in the meantime" },
    { UIC::Bucko, "would you like a copy?" },

    nullptr,
};

constexpr Speech text2_text[] {
    { UIC::Ami, "Hey Bucko?" },
    { UIC::Bucko, "Yes Ami?" },
    { UIC::Ami, "I still don't understand" },
    { UIC::Ami, "how this all happened." },
    { UIC::Ami, "I only left the buckos" },
    { UIC::Ami, "unattended for 2 days and" },
    { UIC::Ami, "they managed to get taken" },
    { UIC::Ami, "over by robots." },
    { UIC::Ami, "What happened?" },

    // TODO: punchline - "aliens" ?
    { UIC::Bucko, "aliens" },

    { UIC::Ami, "..." },

    nullptr,
};

const Speech * s_current_speech;

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
    LevelState next = s_level_state;
    switch (s_level_state) {
        case LevelState::Text1: next = LevelState::Text1b; break;
        case LevelState::Text1b: next = LevelState::Print; break;
        case LevelState::Text2: next = LevelState::Finished; break;

        case LevelState::Print:
        case LevelState::Printing:
        case LevelState::Printed:
        case LevelState::Finished:
            // Not UI.
            break;
    }
    if (next == s_level_state) return;

    if (engine::input::g_buttons_pressed & GAMEPAD_BTN_A) {
        // Advance to next line.
        auto *speech = ++s_current_speech;
        if (speech->text == nullptr) {
            // We're finished, change state.
            level_advance(next);
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

constexpr uint16_t printing_text_y = engine::graphics::SCREEN_HEIGHT * 2 / 3;

bool logic_update() {
    bool finished = false;

    const auto pressed = engine::input::g_buttons_pressed;

    switch (s_level_state) {
        case LevelState::Text1:
        case LevelState::Text1b:
        case LevelState::Text2:
            // Event driven.
            break;

        case LevelState::Printing:
            font::clear_text();
            font::write_centered("Attempting to print", printing_text_y + font::CharHeight * 0);
            font::write_centered("This may take a while...", printing_text_y + font::CharHeight * 2);

            // Try to print it.
            s_print_error = bios_print8bpp(VDP.BITMAP_VRAM_8BIT + printout_start, VDP.PALETTE, 1);
            level_advance(LevelState::Printed);
            break;

        case LevelState::Print:
        case LevelState::Printed:
            if (pressed & GAMEPAD_BTN_B) {
                level_advance(LevelState::Printing);
            } else if (pressed & GAMEPAD_BTN_C) {
                level_advance(LevelState::Text2);
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
    bool redraw = true;
    switch (state) {
        case LevelState::Text1:
            s_current_speech = text1_text;
            break;

        case LevelState::Text1b:
            s_current_speech = text1b_text;
            bg_show_codes();
            break;

        case LevelState::Print:
            // HACK: clean up UI code since we're doing it here
            redraw = false;
            font::clear_text();
            ui_character(UIC::None);

            font::write_centered("Press B to print a copy", printing_text_y + font::CharHeight * 0);
            font::write_centered("Press C to skip", printing_text_y + font::CharHeight * 2);
            font::write_centered("Hasn't been tested", printing_text_y + font::CharHeight * 4);
            font::write_centered("on real hardware!", printing_text_y + font::CharHeight * 6);
            break;

        case LevelState::Printing:
            // All logic.
            break;

        case LevelState::Printed:
            // HACK: clean up UI code since we're doing it here
            redraw = false;
            font::clear_text();
            ui_character(UIC::None);

            // WARNING: there's a limit on text sprites so messages must be small
            switch (s_print_error) {
                case PRINT_STATUS_SUCCESS:
                    font::write_centered("Printing successful", printing_text_y + font::CharHeight * 0);
                    break;
                default:
                case PRINT_STATUS_GENERAL_FAILURE:
                    font::write_centered("Printing failed for", printing_text_y + font::CharHeight * 0);
                    font::write_centered("an unknown reason", printing_text_y + font::CharHeight * 2);
                    break;
                case PRINT_STATUS_NO_SEAL_CART:
                    font::write_centered("Cannot print since no", printing_text_y + font::CharHeight * 0);
                    font::write_centered("seal cart was found", printing_text_y + font::CharHeight * 2);
                    break;
                case PRINT_STATUS_CANCELLED:
                    font::write_centered("Printing was cancelled", printing_text_y + font::CharHeight * 0);
                    break;
                case PRINT_STATUS_PAPER_JAM:
                    font::write_centered("Printer jammed", printing_text_y + font::CharHeight * 0);
                    font::write_centered("Unjam and try again", printing_text_y + font::CharHeight * 2);
                    break;
                case PRINT_STATUS_OVERHEAT:
                    font::write_centered("Printer overheated", printing_text_y + font::CharHeight * 0);
                    font::write_centered("Wait and try again", printing_text_y + font::CharHeight * 2);
                    break;
            }
            font::write_centered("B to print again", printing_text_y + font::CharHeight * 4);
            font::write_centered("C to continue", printing_text_y + font::CharHeight * 6);
            break;

        case LevelState::Text2:
            s_current_speech = text2_text;
            break;

        case LevelState::Finished:
            // Logic will finish this off.
            break;
    }
    if (redraw) {
        ui_redraw();
    }
}

} // namespace

void enter() {
    // Setup palette.
    engine::graphics::set_palette_colour(pal_black, RGB555(0,0,0));
    engine::graphics::set_palette_colour(pal_white, RGB555(31,31,31));

    // Setup the parts.
    bg_setup();
    ui_setup();

    // Kick off state machine.
    level_advance(LevelState::Text1);

    // This screen uses sprites and has a background.
    bios_vsync();
    engine::graphics::enable_sprites();
    //engine::graphics::bitmap_0.enable(); // we'll enable this later

    // Kick off the bgm.
    //engine::sound::play_bgm(game::music::Bgm::???);

    engine::profiler::print_timings();
}

void leave() {
    // Reset graphics state.
    bios_vsync();
    engine::graphics::disable_sprites();
    engine::graphics::bitmap_0.disable();
    engine::graphics::reset_sprites<ami_left_sprite_start + ami_left_sprite_count>();
    font::clear_text();

    // Reset sound.
    engine::sound::stop_bgm();
}

Entry loop() {
    Entry next = Entry::Winner;

    // Wait for vsync.
    {
        PROFILE_SCOPE(vsync);
        bios_vsync();
    }

    // Update gamepad/mouse input.
    engine::input::update_inputs();

    // Return to the main menu if requested.
    if (engine::input::g_buttons_pressed & GAMEPAD_BTN_START) {
        return Entry::MainMenu;
    }

    if (logic_update()) {
        intertile::setup(intertile::Text::Won, Entry::MainMenu);
        return Entry::Intertitle;
    }
    ui_update();

    engine::profiler::print_timings();
    return next;
}

} // namespace game::winner
