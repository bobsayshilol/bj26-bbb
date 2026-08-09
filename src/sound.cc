#include "sound.h"
#include "music_data.h"

namespace engine::sound {

soundstate_t g_sound_state;

void play_startup_sound() {
	biosvar_autoSoundState = &g_sound_state;
	bios_playBgm(&g_sound_state, 0x80, 0, data_musicTrackList);
}

} // namespace engine::sound
