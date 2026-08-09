#pragma once

#include <stdint.h>

namespace game::music {

// Sounds + music.
enum Bgm : uint8_t {
    Bgm_Test,
    Bgm_Count,
};

enum SoundEffect : uint8_t {
    SE_Test,
    SE_Breakout_Bounce,
    SE_Count,
};

extern const uint8_t * const bgm_list[];
extern const uint8_t * const sfx_list[];

} // namespace game::music
