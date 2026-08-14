#pragma once

#include <stdint.h>

namespace game::music {

// Sounds + music.
enum Bgm : uint8_t {
    Bgm_Test,
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



// Seems to be ~8000 ticks per second.
constexpr uint16_t midi_bgm_tps = 8000;
constexpr uint8_t midi_bgm_scale = 32; // TODO: would be nice to be per-BGM

// Create a new BGM. BGMs are composed of blocks by of MIDI_PLAY_AFTER().
#define MIDI_MAKE_BGM(name, bpm, ...) \
    constexpr uint8_t name [] = { \
        (midi_bgm_tps * 60) / (midi_bgm_scale * bpm), \
        0xA2, 0x08, 0xA0, /* ??? */ \
        __VA_ARGS__ \
    }

// Play some events (MIDI_EVT_*) after a dt ticks.
#define MIDI_PLAY_AFTER(dt, ...) \
    midi_bgm_scale * (dt), \
    engine::utils::size({ __VA_ARGS__ }), \
    __VA_ARGS__

// MIDI events.
// Cast to int is for the sizeof check.
#define MIDI_EVT_SET_PROG(chan, voice) 0xC0 | (chan), int(voice | 0),
#define MIDI_EVT_NOTE_ON(chan, note) 0x90 | (chan), int(note | 0), 0x7F,
#define MIDI_EVT_NOTE_OFF(chan, note) 0x90 | (chan), int(note | 0), 0x00,
#define MIDI_EVT_END() 0x00, 0xFE, 0xFF, 0xFF,
#define MIDI_EVT_REPEAT() 0x00, 0xFF, MIDI_EVT_END() // playing it safe with the end event



// BGMs are generated from .mid files.
extern const uint8_t bgm_test_mid[];
#define bgm_test_mid_end_evt MIDI_EVT_REPEAT()
extern const uint8_t main_menu_bgm_mid[];
#define main_menu_bgm_mid_end_evt MIDI_EVT_REPEAT()
extern const uint8_t driving_bgm_mid[];
#define driving_bgm_mid_end_evt MIDI_EVT_REPEAT()

// For testing.
void set_test_voice(uint8_t i);
void set_test_note(uint8_t i);

} // namespace game::music
