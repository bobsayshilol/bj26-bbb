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
	gallery::Screen state = gallery::Screen::Attract;
	while (true) {
		// Enter new state.
		switch (state) {
			case gallery::Screen::Attract: gallery::attract::enter(); break;
			case gallery::Screen::MainMenu: gallery::main_menu::enter(); break;
			case gallery::Screen::Viewer: gallery::viewer::enter(); break;
		}

		// Run main loop.
		gallery::Screen next = state;
		while (next == state) {
			switch (state) {
				case gallery::Screen::Attract: next = gallery::attract::loop(); break;
				case gallery::Screen::MainMenu: next = gallery::main_menu::loop(); break;
				case gallery::Screen::Viewer: next = gallery::viewer::loop(); break;
			}
		}

		// Leave the old state.
		switch (state) {
			case gallery::Screen::Attract: gallery::attract::leave(); break;
			case gallery::Screen::MainMenu: gallery::main_menu::leave(); break;
			case gallery::Screen::Viewer: gallery::viewer::leave(); break;
		}

		// Update state.
		state = next;
	}

	return 0;
}
