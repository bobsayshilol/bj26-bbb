#include "engine.h"
#include "debug.h"
#include "input.h"
#include "loopy.h"
#include "graphics.h"
#include "sound.h"

namespace engine::core {

void init() {
#if WEB_BUILD
	web::init();
#endif
	// Init systems.
	debug::init();
	graphics::init();
	sound::init();

#if !WEB_BUILD
	// The rest of this is copypasta'd.

	// Turn off controller input
	bios_vdpMode(CONTROL_MODE_NONE, VIDEO_HEIGHT_224P);

	// Check mouse presence and set control mode accordingly
	// Requires that controller input was turned off for the last few frames
	input::g_mouse_plugged = (MOUSE_DET != 0);
	input::g_pad_plugged = DET_GAMEPAD1;
#else
	input::g_mouse_plugged = false;
	input::g_pad_plugged = true;
#endif
}

} // namespace engine::core
