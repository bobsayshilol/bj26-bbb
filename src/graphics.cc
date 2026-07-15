#include "graphics.h"

#include "loopy.h"

namespace engine::graphics {

namespace {

void test_sprites() {
	// Enable sprites.
	VDP.LAYER_CTRL |= LAYER_ENABLE_OBJ0;

	// Sprites are 16x16?
	VDP.BG_CTRL = BG_TILESIZE_16X16;

	// Setup sprite stuff.
	VDP.OBJ_CTRL =
		  ((0 & 0xFF) <<  0) // global ID offset (8 bits)
		| ((0 & 0x07) <<  8) // screen1 ID offset (3 bits)
		| ((0 & 0x07) << 11) // screen0 ID offset (3 bits)
		| ((1 & 0x01) << 14) // 8bit (1 bit)
	;

	// Fill tile data.
	for (int i = 0; i < 0x10000; i++) {
		VDP.TILE_VRAM_8BIT[i] = i;
	}

	// Setup some sprites.
	for (int sprite_id = 0; sprite_id < 4; sprite_id++) {
		// TODO: move these
#define SPRITE_SIZE_8x8 0
#define SPRITE_SIZE_16x16 1
#define SPRITE_SIZE_16x32 2
#define SPRITE_SIZE_32x32 3

		int pos = sprite_id * 16;

		uint32_t sprite =
			  ((pos & 0x1FF) << 0) // x pos (9 bits)
			| ((0   & 0x001) << 9) // y pos hi (1 bit)
			| ((SPRITE_SIZE_16x16 & 0x003) << 10) // sprite size (2 bits)
			| ((0   & 0x003) << 12) // palette something ??? (2 bits)
			| ((0   & 0x001) << 14) // x-flip (1 bit)
			| ((0   & 0x001) << 15) // y-flip (1 bit)
			| ((pos & 0xFF) << 16) // y pos lo (8 bits)
			| ((0   & 0xFF) << 24) // tile index ??? (8 bits)
		;
		VDP.OAM[sprite_id] = sprite;
	}
	for (int sprite_id = 4; sprite_id < 128; sprite_id++) {
		uint32_t sprite = 0;
		VDP.OAM[sprite_id] = sprite;
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

	test_sprites();
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

	set_background_a(RGB555(0, 0, 0));
	set_background_b(RGB555(0, 0, 0));

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

} // namespace engine::graphics
