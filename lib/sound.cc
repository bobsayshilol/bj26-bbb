#include "sound.h"
#include "loopy.h"

namespace engine::sound {

namespace detail {

soundstate_t g_sound_state;
const uint8_t * const * s_sfx_tracks;
const uint8_t * const * s_bgm_tracks;

} // namespace detail

void init() {
#if !WEB_BUILD
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

	biosvar_autoSoundState = &detail::g_sound_state;
#endif
}

} // namespace engine::sound
