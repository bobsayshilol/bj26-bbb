#pragma once

#include <stdint.h>

namespace engine::midi {

// Randomly chosen programs.
namespace voices {
constexpr uint8_t piano = 0x00;
constexpr uint8_t guitar = 0x03;
constexpr uint8_t strings = 0x09;
constexpr uint8_t beep = 0x0A; // anything lower than G3 is a pow noise
constexpr uint8_t xylophone = 0x1E;
constexpr uint8_t drums = 0x27;
constexpr uint8_t beep2 = 0x62;
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
constexpr uint8_t A6 = 81;
constexpr uint8_t As6 = 82;
constexpr uint8_t B6 = 83;
constexpr uint8_t C6 = 84;
constexpr uint8_t Cs6 = 85;
constexpr uint8_t D6 = 86;
constexpr uint8_t Ds6 = 87;
constexpr uint8_t E6 = 88;
constexpr uint8_t F6 = 89;
constexpr uint8_t Fs6 = 90;
constexpr uint8_t G6 = 91;
constexpr uint8_t Gs6 = 92;

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



// Startup sound, extended.
extern const uint8_t bgm_startup[];

} // namespace engine::midi
