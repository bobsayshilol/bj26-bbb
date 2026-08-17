#include "music.h"
#include "utils.h"

namespace game::music {

// Effects are manual.
namespace {

// Make a sound effect.
// ... is in MIDI data format.
#define MAKE_SFX(name, ...) \
    constexpr uint8_t name [] = { \
        0x00, /* ??? */ \
        engine::utils::size({ __VA_ARGS__ }), \
        __VA_ARGS__ \
    }; \
    static_assert(sizeof(name) == (name)[1] + 2)

#define SFX_ON(note) MIDI_EVT_NOTE_ON(sfx_channel, note)
#define SFX_OFF(note) MIDI_EVT_NOTE_OFF(sfx_channel, note)
#define SFX_PROG(voice) MIDI_EVT_SET_PROG(sfx_channel, voice)

MAKE_SFX(sfx_placeholder,
    SFX_PROG(voices::drums)
    SFX_ON(notes::crash)
);

MAKE_SFX(sfx_breakout_bounce,
    SFX_PROG(0x62)
    SFX_ON(0x3A)
);
MAKE_SFX(sfx_breakout_hit,
    SFX_PROG(0x62)
    SFX_ON(0x3C)
);

MAKE_SFX(sfx_misclick,
    SFX_PROG(voices::beep)
    SFX_ON(notes::G3)
);
MAKE_SFX(sfx_click,
    SFX_PROG(voices::beep)
    SFX_ON(notes::C4)
);

MAKE_SFX(sfx_tense,
    SFX_PROG(0x17)
    SFX_ON(notes::As4)
    SFX_OFF(notes::As4)
    SFX_ON(notes::A4)
);

MAKE_SFX(sfx_car_engine,
    SFX_PROG(0x5F)
    SFX_ON(0x3B)
);
MAKE_SFX(sfx_weewoo_hi,
    SFX_PROG(0x5F)
    SFX_ON(0x54)
);
MAKE_SFX(sfx_weewoo_low,
    SFX_PROG(0x5F)
    SFX_ON(0x3C)
);

uint8_t sfx_test[] = {
    0, 5,
    SFX_PROG(voices::beep)
    SFX_ON(notes::C4)
};

} // namespace



// Hardcoded BGMs that I cba to remake in an external tool.
namespace {

MIDI_MAKE_BGM(tense_bgm, 128,
    MIDI_PLAY_AFTER(0, // t = 0
        MIDI_EVT_SET_PROG(0, 0x07)
        MIDI_EVT_NOTE_ON(0, notes::As4)
    )

    MIDI_PLAY_AFTER(6, // t = 6
        MIDI_EVT_NOTE_OFF(0, notes::As4)
        MIDI_EVT_NOTE_ON(0, notes::B4)
    )
    MIDI_PLAY_AFTER(1, // t = 7
        MIDI_EVT_NOTE_OFF(0, notes::B4)
        MIDI_EVT_NOTE_ON(0, notes::C4)
    )
    MIDI_PLAY_AFTER(1, // t = 8
        MIDI_EVT_NOTE_OFF(0, notes::C4)
        MIDI_EVT_NOTE_ON(0, notes::Cs4)
    )

    MIDI_PLAY_AFTER(6, // t = 14
        MIDI_EVT_NOTE_OFF(0, notes::Cs4)
        MIDI_EVT_NOTE_ON(0, notes::C4)
    )
    MIDI_PLAY_AFTER(1, // t = 15
        MIDI_EVT_NOTE_OFF(0, notes::C4)
        MIDI_EVT_NOTE_ON(0, notes::B4)
    )
    MIDI_PLAY_AFTER(1, // t = 16/0
        MIDI_EVT_NOTE_OFF(0, notes::B4)
    )

    MIDI_EVT_REPEAT()
);

MIDI_MAKE_BGM(weird_bgm, 60,
    MIDI_PLAY_AFTER(0, // t = 0
        MIDI_EVT_SET_PROG(0, 0x8)
        MIDI_EVT_SET_PROG(1, 0x63)
        MIDI_EVT_NOTE_ON(0, 0x18)
        MIDI_EVT_NOTE_ON(1, 0x18)
    )
    MIDI_PLAY_AFTER(7, // t = 1
        MIDI_EVT_NOTE_OFF(0, 0x18)
        MIDI_EVT_NOTE_OFF(1, 0x18)
        MIDI_EVT_NOTE_ON(0, 0x19)
        MIDI_EVT_NOTE_ON(1, 0x19)
    )
    MIDI_PLAY_AFTER(7, // t = 2
        MIDI_EVT_NOTE_OFF(0, 0x19)
        MIDI_EVT_NOTE_OFF(1, 0x19)
        MIDI_EVT_NOTE_ON(0, 0x1A)
        MIDI_EVT_NOTE_ON(1, 0x1A)
    )
    MIDI_PLAY_AFTER(7, // t = 3
        MIDI_EVT_NOTE_OFF(0, 0x1A)
        MIDI_EVT_NOTE_OFF(1, 0x1A)
        MIDI_EVT_NOTE_ON(0, 0x1B)
        MIDI_EVT_NOTE_ON(1, 0x1B)
    )
    MIDI_PLAY_AFTER(7, // t = 4/0
        MIDI_EVT_NOTE_OFF(0, 0x1B)
        MIDI_EVT_NOTE_OFF(1, 0x1B)
    )

    MIDI_EVT_REPEAT()
);

} // namespace



// Exported symbols.

void set_test_voice(uint8_t i) {
    sfx_test[3] = i;
}
void set_test_note(uint8_t i) {
    sfx_test[5] = i;
}

#pragma GCC diagnostic ignored "-Wpedantic" // silence warning about designated initialisers

const uint8_t * const bgm_list[] {
    [Bgm::Bgm_Test] = bgm_test_mid,
    [Bgm::Bgm_MM_good] = main_menu_bgm_mid,
    [Bgm::Bgm_MM_bad] = main_menu_bgm2_mid,
    [Bgm::Bgm_Breakout] = bgm_test_mid,
    [Bgm::Bgm_Driving] = driving_bgm_mid,
    [Bgm::Bgm_Tense] = tense_bgm,
    [Bgm::Bgm_Weird] = weird_bgm,
};
static_assert(Bgm::Bgm_Count == engine::utils::size(bgm_list));

const uint8_t * const sfx_list[] {
    [SoundEffect::SE_Test] = sfx_test,
    [SoundEffect::SE_Tense] = sfx_tense,
    [SoundEffect::SE_MM_Miss] = sfx_misclick,
    [SoundEffect::SE_MM_Click] = sfx_click,
    [SoundEffect::SE_Breakout_Bounce] = sfx_breakout_bounce,
    [SoundEffect::SE_Breakout_Hit] = sfx_breakout_hit,
    [SoundEffect::SE_Driving_Car] = sfx_car_engine,
    [SoundEffect::SE_Driving_WeewooHi] = sfx_weewoo_hi,
    [SoundEffect::SE_Driving_WeewooLo] = sfx_weewoo_low,
};
static_assert(SoundEffect::SE_Count == engine::utils::size(sfx_list));

} // namespace game::music
