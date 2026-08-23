#include "engine.h"
#include "graphics.h"
#include "debug.h"
#include "music.h"
#include "sound.h"
#include "game.h"
#include "profiler.h"

namespace game {
uint16_t g_unlocked;
uint32_t g_frame_count;
} // namespace game

namespace {

PROFILE_STORAGE(vsync); // bios_vsync takes ~266426 cycles, which matches the expected 266666.

void init() {
	// Initialise components.
	engine::core::init();

	DEBUG_MSG("Booted");

	// Enable gamepad.
	// TODO: move this
	bios_vdpMode(CONTROL_MODE_GAMEPAD, VIDEO_HEIGHT_224P);

	engine::sound::set_lists(game::music::bgm_list, game::music::sfx_list);
}

void draw_something() {
	using namespace engine::graphics;

	bios_vsync();

	// Fill a 256x224 area with a 16x15 grid of colors 1-240
	for(uint16_t y = 0; y < SCREEN_HEIGHT; y++) {
		uint16_t gradY = bios_mathDivU16(y, 15);
		for(uint16_t x = 0; x < SCREEN_WIDTH; x++) {
			uint16_t gradX = x / 16;
			VDP.BITMAP_VRAM_8BIT[(y*SCREEN_WIDTH)+x] = gradY * 16 + gradX + 1;
		}
	}

	// Create a 2D gradient in colors 1-240
	for(int j = 1; j <= 240; j++) {
		int gradX = (j-1) % 16;
		int gradY = (j-1) / 16;
		set_palette_colour(j, RGB555(gradX*2+1, gradY*2+1, 0));
	}

	set_backdrop_a(RGB555(0, 0, 31));
	set_backdrop_b(RGB555(0, 31, 0));

	// Split the screen into 4 quads
	auto draw_quad = [](auto bitmap, bool x, bool y) {
		const auto w = SCREEN_WIDTH / 2;
		const auto h = SCREEN_HEIGHT / 2;
		bitmap.disable();
			bitmap.position_x() = x ? w : 0;
			bitmap.position_y() = y ? h : 0;
			bitmap.scroll_x() = 0;
			bitmap.scroll_y() = 0;
			bitmap.width() = w - 1;
			bitmap.height() = h - 1;
			bitmap.latch() = 0;
		bitmap.enable();
	};
	draw_quad(bitmap_0, 0, 0);
	draw_quad(bitmap_1, 1, 0);
	draw_quad(bitmap_2, 0, 1);
	draw_quad(bitmap_3, 1, 1);
}

void splash() {
	using namespace engine::graphics;

	// TODO: draw a logo
	draw_something();

	// Play the music.
	engine::sound::play_bgm(game::music::Bgm::Bgm_Startup);

	const int fps = 60;
	const int seconds = 2;
	for (int frame = 0; frame < seconds * fps; frame++) {
		const int speed = 4;
		bitmap_0.scroll_x() -= speed;
		bitmap_1.scroll_y() -= speed;
		bitmap_2.scroll_x() += speed;
		bitmap_3.scroll_y() += speed;

		bios_vsync();
	}

	// Reset state.
	bitmap_0.disable();
	bitmap_1.disable();
	bitmap_2.disable();
	bitmap_3.disable();
}

} // namespace

int main() {
	init();
	splash();

	// Reset globals.
	game::g_unlocked = 0;
	game::g_frame_count = 0;

	// Basic state machine.
	game::Entry state = game::Entry::Attract;
	while (true) {
		// Enter new state.
		game::g_unlocked = engine::utils::max(game::g_unlocked, static_cast<uint16_t>(state));
		switch (state) {
			case game::Entry::Attract: game::attract::enter(); break;
			case game::Entry::MainMenu: game::main_menu::enter(); break;
			case game::Entry::Breakout: game::breakout::enter(); break;
			case game::Entry::Driving: game::driving::enter(); break;
			case game::Entry::Intertitle: game::intertile::enter(); break;
			case game::Entry::Cyber: game::cyber::enter(); break;
			case game::Entry::Winner: game::winner::enter(); break;
		}

		// Run main loop.
		game::Entry next = state;
		while (next == state) {
			switch (state) {
				case game::Entry::Attract: next = game::attract::loop(); break;
				case game::Entry::MainMenu: next = game::main_menu::loop(); break;
				case game::Entry::Breakout: next = game::breakout::loop(); break;
				case game::Entry::Driving: next = game::driving::loop(); break;
				case game::Entry::Intertitle: next = game::intertile::loop(); break;
				case game::Entry::Cyber: next = game::cyber::loop(); break;
				case game::Entry::Winner: next = game::winner::loop(); break;
			}
			game::g_frame_count++;
		}

		// Leave the old state.
		switch (state) {
			case game::Entry::Attract: game::attract::leave(); break;
			case game::Entry::MainMenu: game::main_menu::leave(); break;
			case game::Entry::Breakout: game::breakout::leave(); break;
			case game::Entry::Driving: game::driving::leave(); break;
			case game::Entry::Intertitle: game::intertile::leave(); break;
			case game::Entry::Cyber: game::cyber::leave(); break;
			case game::Entry::Winner: game::winner::leave(); break;
		}

		// Update state.
		state = next;
	}

	return 0;
}
