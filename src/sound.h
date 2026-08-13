#pragma once

#include "loopy/bios.h"
#include "loopy/extrafuncs.h"
#include "debug.h"

namespace engine::sound {

namespace detail {
extern soundstate_t g_sound_state;
extern const uint8_t * const * s_sfx_tracks;
extern const uint8_t * const * s_bgm_tracks;
} // detail

void init();

// Set the BGM and SFX track lists.
inline void set_lists(const uint8_t * const * bgm, const uint8_t * const * sfx) {
	detail::s_bgm_tracks = bgm;
	detail::s_sfx_tracks = sfx;
}

// Play a BGM or SFX track.
inline void play_bgm(uint8_t index) {
	ASSERT(detail::s_bgm_tracks);
	bios_playBgm(&detail::g_sound_state, 0x80, index, detail::s_bgm_tracks);
}
inline void play_effect(uint8_t index) {
	ASSERT(detail::s_sfx_tracks);
	bios_playSfx(&detail::g_sound_state, 0x80, index, detail::s_sfx_tracks);
}
inline void stop_bgm() {
	sys_stopBgm(&detail::g_sound_state);
}

} // namespace engine::sound
