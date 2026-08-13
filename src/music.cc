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
#define SFX_PROG(prog) MIDI_EVT_SET_PROG(sfx_channel, prog)

MAKE_SFX(sfx_placeholder,
    SFX_PROG(drums)
    SFX_ON(crash)
);

MAKE_SFX(sfx_bounce,
    SFX_PROG(beep)
    SFX_ON(Ds4)
);

} // namespace



// Exported symbols.

#pragma GCC diagnostic ignored "-Wpedantic" // silence warning about designated initialisers

const uint8_t * const bgm_list[] {
    [Bgm::Bgm_Test] = bgm_test_mid,
};
static_assert(Bgm::Bgm_Count == engine::utils::size(bgm_list));

const uint8_t * const sfx_list[] {
    [SoundEffect::SE_MM_Miss] = sfx_placeholder,
    [SoundEffect::SE_MM_Click] = sfx_placeholder,
    [SoundEffect::SE_Breakout_Bounce] = sfx_bounce,
};
static_assert(SoundEffect::SE_Count == engine::utils::size(sfx_list));

} // namespace game::music
