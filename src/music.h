#pragma once

#include <stdint.h>

namespace game::music {

// Sounds + music.
enum Bgm : uint8_t {
    Bgm_Test,
    Bgm_Driving,
    Bgm_Count,
};

enum SoundEffect : uint8_t {
    SE_Test,
    SE_Tense,
    SE_MM_Miss,
    SE_MM_Click,
    SE_Breakout_Bounce,
    SE_Count,
};

// BGM and SFX track lists.
extern const uint8_t * const bgm_list[];
extern const uint8_t * const sfx_list[];



// LoopyMSE has 32 channels but only the first 3 are usable.
// BGM gets first 2 channels, SFX gets 3rd voice.
constexpr uint8_t sfx_channel = 2;

// Randomly chosen programs.
namespace voices {
constexpr uint8_t piano = 0x00;
constexpr uint8_t guitar = 0x03;
constexpr uint8_t beep = 0x0A; // anything lower than G3 is a pow noise
constexpr uint8_t xylophone = 0x1E;
constexpr uint8_t drums = 0x27;
} // namespace voices

// Notes.
namespace notes {
constexpr uint8_t A3 = 45;
constexpr uint8_t As3 = 46;
constexpr uint8_t B3 = 47;
constexpr uint8_t C3 = 48;
constexpr uint8_t Cs3 = 49;
constexpr uint8_t D3 = 50;
constexpr uint8_t Ds3 = 51;
constexpr uint8_t E3 = 52;
constexpr uint8_t F3 = 53;
constexpr uint8_t Fs3 = 54;
constexpr uint8_t G3 = 55;
constexpr uint8_t Gs3 = 56;
constexpr uint8_t A4 = 57;
constexpr uint8_t As4 = 58;
constexpr uint8_t B4 = 59;
constexpr uint8_t C4 = 60;
constexpr uint8_t Cs4 = 61;
constexpr uint8_t D4 = 62;
constexpr uint8_t Ds4 = 63;
constexpr uint8_t E4 = 64;
constexpr uint8_t F4 = 65;
constexpr uint8_t Fs4 = 66;
constexpr uint8_t G4 = 67;
constexpr uint8_t Gs4 = 68;
constexpr uint8_t A5 = 69;
constexpr uint8_t As5 = 70;
constexpr uint8_t B5 = 71;
constexpr uint8_t C5 = 72;
constexpr uint8_t Cs5 = 73;
constexpr uint8_t D5 = 74;
constexpr uint8_t Ds5 = 75;
constexpr uint8_t E5 = 76;
constexpr uint8_t F5 = 77;
constexpr uint8_t Fs5 = 78;
constexpr uint8_t G5 = 79;
constexpr uint8_t Gs5 = 80;

// Notes for drums that shared with standard MIDI:
constexpr uint8_t bass  = 36; // 36 - C2 - Bass Drum 1
constexpr uint8_t snare = 40; // 40 - E2 - Electric Snare
constexpr uint8_t hihat = 46; // 46 - As3 - Open Hi-Hat (but closed)
constexpr uint8_t crash = 49; // 49 - Cs3 - Crash 1
} // namespace note

// Cast to int is for the sizeof check.
#define MIDI_EVT_SET_PROG(chan, voice) 0xC0 | (chan), int(voice | 0),
#define MIDI_EVT_NOTE_ON(chan, note) 0x90 | (chan), int(note | 0), 0x40,
#define MIDI_EVT_NOTE_OFF(chan, note) 0x90 | (chan), int(note | 0), 0x00,

// BGMs are generated from .mid files.
extern const uint8_t bgm_test_mid[];
extern const uint8_t bgm_test2_mid[];
extern const uint8_t main_menu_bgm_mid[];
extern const uint8_t driving_bgm_mid[];
extern const uint8_t canyon_mid[];

// For testing.
void set_test_voice(uint8_t i);
void set_test_note(uint8_t i);

} // namespace game::music
