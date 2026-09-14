#include "engine.h"
#include "debug.h"
#include "gallery.h"
#include "sound.h"

namespace {

void init() {
	// Initialise components.
	engine::core::init();

	DEBUG_MSG("Booted");

#if !WEB_BUILD
	// Enable gamepad.
	// TODO: move this
	bios_vdpMode(CONTROL_MODE_GAMEPAD, VIDEO_HEIGHT_224P);
#endif

	// TODO: music
	//engine::sound::set_lists(game::music::bgm_list, game::music::sfx_list);
}

} // namespace

int main() {
	init();

	// Basic state machine.
	gallery::Entry state = gallery::Entry::Attract;
	while (true) {
		// Enter new state.
		switch (state) {
			case gallery::Entry::Attract: gallery::attract::enter(); break;
			case gallery::Entry::MainMenu: gallery::main_menu::enter(); break;
		}

		// Run main loop.
		gallery::Entry next = state;
		while (next == state) {
			switch (state) {
				case gallery::Entry::Attract: next = gallery::attract::loop(); break;
				case gallery::Entry::MainMenu: next = gallery::main_menu::loop(); break;
			}
		}

		// Leave the old state.
		switch (state) {
			case gallery::Entry::Attract: gallery::attract::leave(); break;
			case gallery::Entry::MainMenu: gallery::main_menu::leave(); break;
		}

		// Update state.
		state = next;
	}

	return 0;
}
