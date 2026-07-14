#include "engine.h"
#include "debug.h"
#include "input.h"
#include "loopy.h"

namespace engine::core {

void init() {
	// Init systems.
	debug::init();

	// The rest of this is copypasta'd.

	// Turn off all video output
	VDP.SCREENPRIO = 0;
	VDP.BACKDROP_A = 0;
	VDP.BACKDROP_B = 0;

	// Turn off controller input
	bios_vdpMode(CONTROL_MODE_NONE, VIDEO_HEIGHT_224P);

	// Setup sound hardware (takes a few frames)
	bios_soundChannels(SOUND_CHANS_4CH);
	bios_soundVolume(SOUND_VOL_CH2_3, SOUND_VOL_100);
	bios_soundVolume(SOUND_VOL_CH4,   SOUND_VOL_100);
	bios_initSoundTransmission();

	// Check mouse presence and set control mode accordingly
	// Requires that controller input was turned off for the last few frames
	input::g_mouse_plugged = (MOUSE_DET != 0);
	input::g_pad_plugged = DET_GAMEPAD1;

	// Enable interrupts and DMA for music
	sys_setInterruptPriority(INT_PRIO_ITU0, 0xF);
	sys_setInterruptMask(0xE);
	sys_setDmaEnabled(true);
}

} // namespace engine::core
