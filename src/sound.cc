#include "sound.h"
#include "debug.h"
#include "loopy.h"

namespace engine::sound {

namespace {

const uint8_t * const * s_sfx_tracks;
const uint8_t * const * s_bgm_tracks;

} // namespace

soundstate_t g_sound_state;

void init() {
	// Mostly copypasta'd.

	// Setup sound hardware (takes a few frames)
	bios_soundChannels(SOUND_CHANS_4CH);
	bios_soundVolume(SOUND_VOL_CH2_3, SOUND_VOL_100);
	bios_soundVolume(SOUND_VOL_CH4,   SOUND_VOL_100);
	bios_initSoundTransmission();

	// Enable interrupts and DMA for music
	sys_setInterruptPriority(INT_PRIO_ITU0, 0xF);
	sys_setInterruptMask(0xE);
	sys_setDmaEnabled(true);

	biosvar_autoSoundState = &g_sound_state;
}

void set_lists(const uint8_t * const * bgm, const uint8_t * const * sfx) {
	s_bgm_tracks = bgm;
	s_sfx_tracks = sfx;
}

void play_bgm(uint8_t index) {
	ASSERT(s_bgm_tracks);
	bios_playBgm(&g_sound_state, 0x80, index, s_bgm_tracks);
}

void play_effect(uint8_t index) {
	ASSERT(s_sfx_tracks);
	bios_playSfx(&g_sound_state, 0x80, index, s_sfx_tracks);
}

} // namespace engine::sound
