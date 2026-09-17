#pragma once

#include "midi.h"

#include <stdint.h>

namespace game::music {

// Sounds + music.
enum Bgm : uint8_t {
    Bgm_Startup,
    Bgm_MM_good,
    Bgm_MM_bad,
    Bgm_Breakout,
    Bgm_Driving,
    Bgm_Tense,
    Bgm_Weird,
    Bgm_Count,
};

enum SoundEffect : uint8_t {
    SE_Test,
    SE_Tense,
    SE_MM_Miss,
    SE_MM_Click,
    SE_Breakout_Bounce,
    SE_Breakout_Hit,
    SE_Driving_Car,
    SE_Driving_WeewooHi,
    SE_Driving_WeewooLo,
    SE_Driving_Hit,
    SE_Stop,
    SE_Count,
};

// BGM and SFX track lists.
extern const uint8_t * const bgm_list[];
extern const uint8_t * const sfx_list[];

// BGMs are generated from .mid files.
extern const uint8_t main_menu_bgm_mid[];
#define main_menu_bgm_mid_end_evt MIDI_EVT_REPEAT()
extern const uint8_t main_menu_bgm2_mid[];
#define main_menu_bgm2_mid_end_evt MIDI_EVT_REPEAT()
extern const uint8_t driving_bgm_mid[];
#define driving_bgm_mid_end_evt MIDI_EVT_REPEAT()

// For testing.
void set_test_voice(uint8_t i);
void set_test_note(uint8_t i);

} // namespace game::music
