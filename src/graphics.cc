#include "graphics.h"

#include "loopy.h"

namespace engine::graphics {

namespace {

// TODO: LAYER_WRITE values seem wrong...
#define LAYER_WRITE_BG0_TO_A 0x100
#define LAYER_WRITE_BG0_TO_B 0x200
#define LAYER_WRITE_BG1_TO_A 0x400
#define LAYER_WRITE_BG1_TO_B 0x800
#define LAYER_WRITE_OBJ0_TO_A 0x1000
#define LAYER_WRITE_OBJ0_TO_B 0x2000
#define LAYER_WRITE_OBJ1_TO_A 0x4000
#define LAYER_WRITE_OBJ1_TO_B 0x8000

void test_sprites() {
	VDP.LAYER_CTRL |=
		  LAYER_ENABLE_BG0  // Enable background0.
		//| LAYER_WRITE_BG0_TO_A // Write BG0 to screenA.
		//| LAYER_ENABLE_BG1  // Enable background1.
		//| LAYER_WRITE_BG1_TO_B // Write BG1 to screenB.
		| LAYER_ENABLE_OBJ0 // Enable sprites.
		//| LAYER_WRITE_OBJ0_TO_A // Write sprites to screenA.
	;

	VDP.BG_CTRL =
		  ((bg_shared_tilemap & 0x1) << 0) // BG0 and BG1 are the same tilemap (1 bit)
		| ((bg_tilemap_size_enum & 0x3) << 1) // BG tilemap size (2 bits)
		| ((sprite_is_8bpp & 0x1) << 3) // BG0 8bit(1)/4bit(0) (1 bit)
		| ((bg_tile_size_enum & 0x3) << 4) // BG1 tile size (2 bits)
		| ((bg_tile_size_enum & 0x3) << 6) // BG0 tile size (2 bits)
	;

	// Setup sprite stuff.
	VDP.OBJ_CTRL =
		  ((0 & 0xFF) <<  0) // global ID offset (8 bits)
		| ((0 & 0x07) <<  8) // screen1 ID offset (3 bits)
		| ((0 & 0x07) << 11) // screen0 ID offset (3 bits)
		| ((sprite_is_8bpp & 0x01) << 14) // 8bit(1)/4bit(0) (1 bit)
	;

	// No tileset offset.
	VDP.CHARBASE = 0;

	const int num_sprites = 6;
	for (int i = 0; i < num_sprites; i++) {
		// Add a sprite.
		auto & sprite = get_sprite(i);
		sprite.set_x(8 * i);
		sprite.set_y(8 * i);
		sprite.set_tile_index(i);
		sprite.set_size(SpriteSize::Size8x8);

		// Give it a colour.
		const int g = i * 31 / num_sprites;
		VDP.PALETTE[i] = RGB555(g, g, g);
		uint8_t * data = get_tile_data(i);
		for (int y = 0; y < 8; y++) {
			for (int x = 0; x < 8; x++) {
				*data++ = i;
			}
		}
	}

	// Point BG tiles to data.
	for (uint32_t y = 0; y < bg_tilemap_size; y++) {
		for (uint32_t x = 0; x < bg_tilemap_size; x++) {
			// BG0 is rows.
			auto & tile0 = get_bg_sprite<0>(x, y);
			tile0.set_tile_index(num_sprites + y);
			// BG1 is columns.
			auto & tile1 = get_bg_sprite<1>(x, y);
			tile1.set_tile_index(num_sprites + x);
		}
	}

	// Add colours.
	for (uint32_t i = 0; i < bg_tilemap_size; i++) {
		const int g = i * 31 / bg_tilemap_size;
		VDP.PALETTE[num_sprites + i] = RGB555(g, g, g);
		uint8_t * data = get_tile_data(num_sprites + i);
		for (int y = 0; y < 8; y++) {
			for (int x = 0; x < 8; x++) {
				*data++ = num_sprites + i;
			}
		}
	}
}

}

void init() {
	// 8bpp simplifies things, probably won't need 4bpp.
	VDP.BM_CTRL = BM_MODE_8BPP_SHARED; // bitmap_ctrl

	// TODO
	VDP.BLEND           = BLEND_MATH;
	VDP.BM_SUBPAL       = BM_SUBPAL(0,0,0,0); // bitmap_palsel
	VDP.SCREENPRIO      = BLEND_MATH_ADD | SCREEN_A_ENABLE | PRIORITY_BM_A | PRIORITY_BG0_A | PRIORITY_OBJ0_A;
	VDP.LAYER_CTRL      = LAYER_SCREEN(LAYER_SCREEN_A, LAYER_SCREEN_A, LAYER_SCREEN_A, LAYER_SCREEN_A);

	set_background_a(RGB555(0, 0, 0));
	set_background_b(RGB555(0, 0, 0));
}

void draw_something() {
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
		VDP.PALETTE[j] = RGB555(gradX*2+1, gradY*2+1, 0);
	}

	set_background_a(RGB555(0, 0, 31));
	set_background_b(RGB555(0, 31, 0));

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

	test_sprites();
}

} // namespace engine::graphics
