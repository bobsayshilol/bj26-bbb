#include "music.h"
#include "utils.h"

// BGMs are generated from .mid files.
extern uint8_t bgm_test_mid[];
extern uint8_t canyon_mid[];



// Effects are manual.
namespace {

#define CHECK_SIZE(sfx) static_assert(sizeof(sfx) == (sfx)[1] + 2);

constexpr uint8_t sfx_test[] = {
    0x0, // ???
    3 * 6, // size, bytes
    // MIDI data
    0x90,0x3c,0x40, // note on
    0x90,0x3d,0x40,
    0x90,0x3f,0x40,
    0x90,0x3c,0x00, // note off
    0x90,0x3d,0x00,
    0x90,0x3f,0x00,
};
CHECK_SIZE(sfx_test);

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
    [SoundEffect::SE_Breakout_Bounce] = sfx_test,
};
static_assert(SoundEffect::SE_Count == engine::utils::size(sfx_list));

} // namespace game::music
