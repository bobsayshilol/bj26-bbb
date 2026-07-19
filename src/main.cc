#include "engine.h"
#include "graphics.h"
#include "input.h"
#include "debug.h"
#include "sound.h"
#include "game.h"
#include "profiler.h"

namespace {

PROFILE_STORAGE(sprite_drawing);
PROFILE_STORAGE(bg_tilemap);
PROFILE_STORAGE(bg_drawing);
PROFILE_STORAGE(vsync); // bios_vsync takes ~266426 cycles, which matches the expected 266666.

void init() {
	// Initialise components.
	engine::core::init();

	DEBUG_MSG("Booted");

	// Enable gamepad.
	// TODO: move this
	bios_vdpMode(CONTROL_MODE_GAMEPAD, VIDEO_HEIGHT_224P);
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
	// TODO: draw a logo
	draw_something();

	// Play the music.
	engine::sound::play_startup_sound();

	// TODO: check this
	const int fps = 60;
	const int seconds = 4;
	for (int frame = 0; frame < seconds * fps; frame++) {
		bios_vsync();

		const int speed = 4;
		engine::graphics::bitmap_0.scroll_x() -= speed;
		engine::graphics::bitmap_1.scroll_y() -= speed;
		engine::graphics::bitmap_2.scroll_x() += speed;
		engine::graphics::bitmap_3.scroll_y() += speed;
	}
}

void test_sprites() {
	using namespace engine::graphics;

	bitmap_1.disable();
	bitmap_2.disable();
	bitmap_3.disable();

	enable_sprites();

	const int num_sprites = 6;
	for (int i = 0; i < num_sprites; i++) {
		PROFILE_SCOPE(sprite_drawing);

		// Add a sprite.
		auto & sprite = get_sprite(i);
		sprite.set_x(8 * i);
		sprite.set_y(8 * i);
		sprite.set_tile_index(i);
		sprite.set_size(SpriteSize::Size8x8);

		// Give it a colour.
		const int g = i * 31 / num_sprites;
		set_palette_colour(i, RGB555(g, g, g));
		uint16_t * data16 = (uint16_t*)get_tile_data(i);
		for (int y = 0; y < 8; y++) {
			for (int x = 0; x < 8; x += 2) {
				// Write 2 pixels at a time.
				uint16_t v16 = i;
				v16 |= v16 << 8;
				*data16++ = v16;
			}
		}
	}

	background_0.enable();

	// Point BG tiles to data.
	for (uint32_t y = 0; y < bg_tilemap_size; y++) {
		PROFILE_SCOPE(bg_tilemap);

		for (uint32_t x = 0; x < bg_tilemap_size; x++) {
			// BG0 is rows.
			auto & tile0 = get_bg_sprite<0>(x, y);
			tile0.set_tile_index(num_sprites + y);
			// BG1 isn't enabled so skip it.
			// BG1 is columns.
			//auto & tile1 = get_bg_sprite<1>(x, y);
			//tile1.set_tile_index(num_sprites + x);
		}
	}

	// Add colours.
	for (uint32_t i = 0; i < bg_tilemap_size; i++) {
		PROFILE_SCOPE(bg_drawing);

		const int g = i * 31 / bg_tilemap_size;
		set_palette_colour(num_sprites + i, RGB555(g, g, g));
		uint16_t * data16 = (uint16_t *)get_tile_data(num_sprites + i);
		for (int y = 0; y < 8; y++) {
			for (int x = 0; x < 8; x += 2) {
				// Write 2 pixels at a time.
				uint16_t v16 = num_sprites + i;
				v16 |= v16 << 8;
				*data16++ = v16;
			}
		}
	}

	{
		PROFILE_SCOPE(vsync);
		bios_vsync();
	}

	engine::profiler::print_timings();
}

} // namespace

int main() {
	init();
	splash();

	test_sprites();

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
			engine::graphics::bitmap_0.scroll_x() += speed;
		} else if (held & GAMEPAD_BTN_RIGHT) {
			engine::graphics::bitmap_0.scroll_x() -= speed;
		}
		if (held & GAMEPAD_BTN_UP) {
			engine::graphics::bitmap_0.scroll_y() += speed;
		} else if (held & GAMEPAD_BTN_DOWN) {
			engine::graphics::bitmap_0.scroll_y() -= speed;
		}

		engine::profiler::print_timings();
	}

	return 0;
}
