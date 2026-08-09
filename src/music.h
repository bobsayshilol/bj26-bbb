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

// BGMs are generated from .mid files.
extern const uint8_t bgm_test_mid[];
extern const uint8_t bgm_test2_mid[];
extern const uint8_t main_menu_bgm_mid[];
extern const uint8_t canyon_mid[];

} // namespace game::music
