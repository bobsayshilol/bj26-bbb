#include "music.h"
#include "utils.h"

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

MAKE_SFX(sfx_test,
    0x90,0x3c,0x40, // note on
    0x90,0x3d,0x40,
    0x90,0x3f,0x40,
    0x90,0x3c,0x00, // note off
    0x90,0x3d,0x00,
    0x90,0x3f,0x00,
);

MAKE_SFX(sfx_bounce,
    0x90,0x3f,0x40, // note on
    0x90,0x20,0x40,
    0x90,0x3f,0x00, // note off
    0x90,0x20,0x00,
);

} // namespace



// Exported symbols.

namespace game::music {

#pragma GCC diagnostic ignored "-Wpedantic" // silence warning about designated initialisers

const uint8_t * const bgm_list[] {
    [Bgm::Bgm_Test] = bgm_test_mid,
};
static_assert(Bgm::Bgm_Count == engine::utils::size(bgm_list));

const uint8_t * const sfx_list[] {
    [SoundEffect::SE_Test] = sfx_test,
    [SoundEffect::SE_MM_Miss] = sfx_test,
    [SoundEffect::SE_MM_Click] = sfx_test,
    [SoundEffect::SE_Breakout_Bounce] = sfx_bounce,
};
static_assert(SoundEffect::SE_Count == engine::utils::size(sfx_list));

} // namespace game::music
