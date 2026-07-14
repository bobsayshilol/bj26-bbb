#include "engine.h"
#include "graphics.h"
#include "input.h"
#include "debug.h"
#include "sound.h"
#include "game.h"

namespace {

void init() {
	// Initialise components.
	engine::core::init();

	DEBUG_MSG("Booted");

	// Enable gamepad.
	// TODO: move this
	bios_vdpMode(CONTROL_MODE_GAMEPAD, VIDEO_HEIGHT_224P);
}

void splash() {
	// TODO: draw a logo
	engine::graphics::draw_something();

	// Play the music.
	engine::sound::play_startup_sound();

	// TODO: check this
	const int fps = 60;
	const int seconds = 4;
	for (int frame = 0; frame < seconds * fps; frame++) {
		bios_vsync();
		VDP.BM_SCROLLX[0] += 4;
		VDP.BM_SCROLLY[0] += 4;
	}
}

} // namespace

int main() {
	init();
	splash();

	engine::graphics::draw_something();

	// Main game loop.
	while(1) {
		// Wait for vsync.
		bios_vsync();

		// Update gamepad/mouse input.
		engine::input::update_inputs();

		// TODO: gameplay
		const auto held = engine::input::g_buttons_held;
		const auto speed = (held & GAMEPAD_BTN_A) ? 4 : 1;
		if (held & GAMEPAD_BTN_LEFT) {
			VDP.BM_SCROLLX[0] += speed;
		} else if (held & GAMEPAD_BTN_RIGHT) {
			VDP.BM_SCROLLX[0] -= speed;
		}
		if (held & GAMEPAD_BTN_UP) {
			VDP.BM_SCROLLY[0] += speed;
		} else if (held & GAMEPAD_BTN_DOWN) {
			VDP.BM_SCROLLY[0] -= speed;
		}
	}

	return 0;
}
