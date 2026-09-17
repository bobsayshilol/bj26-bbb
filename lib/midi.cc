#include "midi.h"
#include "utils.h"

namespace engine::midi {

MIDI_MAKE_BGM(bgm_startup, 400,
    MIDI_PLAY_AFTER(0, // t = 0
        MIDI_EVT_SET_PROG(0, 0x60)
        MIDI_EVT_SET_PROG(1, 0x5C)
        MIDI_EVT_NOTE_ON(0, notes::Gs3)
        MIDI_EVT_NOTE_ON(1, notes::Gs3)
    )
    MIDI_PLAY_AFTER(1, // t = 1
        MIDI_EVT_NOTE_OFF(0, notes::Gs3)
        MIDI_EVT_NOTE_OFF(1, notes::Gs3)
        MIDI_EVT_NOTE_ON(0, notes::C4)
        MIDI_EVT_NOTE_ON(1, notes::C4)
    )
    MIDI_PLAY_AFTER(1, // t = 2
        MIDI_EVT_NOTE_OFF(0, notes::C4)
        MIDI_EVT_NOTE_OFF(1, notes::C4)
        MIDI_EVT_NOTE_ON(0, notes::Ds4)
        MIDI_EVT_NOTE_ON(1, notes::Ds4)
    )
    MIDI_PLAY_AFTER(1, // t = 3
        MIDI_EVT_NOTE_OFF(0, notes::Ds4)
        MIDI_EVT_NOTE_OFF(1, notes::Ds4)
        MIDI_EVT_NOTE_ON(0, notes::As5)
        MIDI_EVT_NOTE_ON(1, notes::As5)
    )
    MIDI_PLAY_AFTER(1, // t = 4
        MIDI_EVT_NOTE_OFF(0, notes::As5)
        MIDI_EVT_NOTE_OFF(1, notes::As5)
        MIDI_EVT_NOTE_ON(0, notes::Ds5)
        MIDI_EVT_NOTE_ON(1, notes::Ds5)
    )
    MIDI_PLAY_AFTER(1, // t = 5
        MIDI_EVT_NOTE_OFF(0, notes::Ds5)
        MIDI_EVT_NOTE_OFF(1, notes::Ds5)
        MIDI_EVT_NOTE_ON(0, notes::G5)
        MIDI_EVT_NOTE_ON(1, notes::G5)
    )
//#if 1 // can't #if inside a macro
    MIDI_PLAY_AFTER(1, // t = 6
        MIDI_EVT_NOTE_OFF(0, notes::G5)
        MIDI_EVT_NOTE_OFF(1, notes::G5)
        MIDI_EVT_NOTE_ON(0, notes::As6)
        MIDI_EVT_NOTE_ON(1, notes::As6)
    )
    MIDI_PLAY_AFTER(1, // t = 7
        MIDI_EVT_NOTE_OFF(0, notes::As6)
        MIDI_EVT_NOTE_OFF(1, notes::As6)
        MIDI_EVT_NOTE_ON(0, notes::G5)
        MIDI_EVT_NOTE_ON(1, notes::G5)
    )
    MIDI_PLAY_AFTER(1, // t = 8
        MIDI_EVT_NOTE_OFF(0, notes::G5)
        MIDI_EVT_NOTE_OFF(1, notes::G5)
        MIDI_EVT_NOTE_ON(0, notes::Ds5)
        MIDI_EVT_NOTE_ON(1, notes::Ds5)
    )
    MIDI_PLAY_AFTER(1, // t = 9
        MIDI_EVT_NOTE_OFF(0, notes::Ds5)
        MIDI_EVT_NOTE_OFF(1, notes::Ds5)
        MIDI_EVT_NOTE_ON(0, notes::As5)
        MIDI_EVT_NOTE_ON(1, notes::As5)
    )
    MIDI_PLAY_AFTER(1, // t = 10
        MIDI_EVT_NOTE_OFF(0, notes::As5)
        MIDI_EVT_NOTE_OFF(1, notes::As5)
        MIDI_EVT_NOTE_ON(0, notes::As6)
        MIDI_EVT_NOTE_ON(1, notes::As6)
    )
    MIDI_PLAY_AFTER(1, // t = 11
        MIDI_EVT_NOTE_OFF(0, notes::As6)
        MIDI_EVT_NOTE_OFF(1, notes::As6)
        MIDI_EVT_NOTE_ON(0, notes::G5)
        MIDI_EVT_NOTE_ON(1, notes::G5)
    )
    MIDI_PLAY_AFTER(1, // t = 12
        MIDI_EVT_NOTE_OFF(0, notes::G5)
        MIDI_EVT_NOTE_OFF(1, notes::G5)
        MIDI_EVT_NOTE_ON(0, notes::Ds5)
        MIDI_EVT_NOTE_ON(1, notes::Ds5)
    )
    MIDI_PLAY_AFTER(1, // t = 13
        MIDI_EVT_NOTE_OFF(0, notes::Ds5)
        MIDI_EVT_NOTE_OFF(1, notes::Ds5)
        MIDI_EVT_NOTE_ON(0, notes::As5)
        MIDI_EVT_NOTE_ON(1, notes::As5)
    )
    MIDI_PLAY_AFTER(1, // t = 14
        MIDI_EVT_NOTE_OFF(0, notes::As5)
        MIDI_EVT_NOTE_OFF(1, notes::As5)
        MIDI_EVT_NOTE_ON(0, notes::Ds4)
        MIDI_EVT_NOTE_ON(1, notes::Ds4)
    )
    MIDI_PLAY_AFTER(5,
        MIDI_EVT_NOTE_OFF(0, notes::Ds4)
        MIDI_EVT_NOTE_OFF(1, notes::Ds4)
    )
//#else
//    MIDI_PLAY_AFTER(5, // t = 10
//        MIDI_EVT_NOTE_OFF(0, notes::G5)
//        MIDI_EVT_NOTE_OFF(1, notes::G5)
//    )
//#endif
    MIDI_EVT_END()
);

} // namespace engine::midi
