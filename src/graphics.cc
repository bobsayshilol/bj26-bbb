#include "graphics.h"

#include "loopy.h"

namespace engine::graphics {

namespace {

// TODO: move these
#define BG_TILEMAP_SIZE_64x64 0
#define BG_TILEMAP_SIZE_64x32 1
#define BG_TILEMAP_SIZE_32x64 2
#define BG_TILEMAP_SIZE_32x32 3

#define SPRITE_SIZE_8x8 0
#define SPRITE_SIZE_16x16 1
#define SPRITE_SIZE_16x32 2
#define SPRITE_SIZE_32x32 3

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
	const int bg_tilemap_size = 32;
	const int bg_tilemap_size_enum = BG_TILEMAP_SIZE_32x32;
	const int bg_tile_size_enum = BG_TILESIZE_8X8;
	const bool bg_shared_tilemap = false;

	VDP.LAYER_CTRL |=
		  LAYER_ENABLE_BG0  // Enable background0.
		//| LAYER_WRITE_BG0_TO_A // Write BG0 to screenA.
		| LAYER_ENABLE_BG1  // Enable background1.
		//| LAYER_WRITE_BG1_TO_B // Write BG1 to screenB.
		| LAYER_ENABLE_OBJ0 // Enable sprites.
		//| LAYER_WRITE_OBJ0_TO_A // Write sprites to screenA.
	;

	VDP.BG_CTRL =
		  ((bg_shared_tilemap & 0x1) << 0) // BG0 and BG1 are the same tilemap (1 bit)
		| ((bg_tilemap_size_enum & 0x3) << 1) // BG tilemap size (2 bits)
		| ((1 & 0x1) << 3) // BG0 8bit(1)/4bit(0) (1 bit)
		| ((bg_tile_size_enum & 0x3) << 4) // BG1 tile size (2 bits)
		| ((bg_tile_size_enum & 0x3) << 6) // BG0 tile size (2 bits)
	;

	// Setup sprite stuff.
	VDP.OBJ_CTRL =
		  ((0 & 0xFF) <<  0) // global ID offset (8 bits)
		| ((0 & 0x07) <<  8) // screen1 ID offset (3 bits)
		| ((0 & 0x07) << 11) // screen0 ID offset (3 bits)
		| ((1 & 0x01) << 14) // 8bit(1)/4bit(0) (1 bit)
	;

	// No tileset offset.
	VDP.CHARBASE = 0;

	VDP.PALETTE[1] = RGB555(15, 15, 15); // BG0
	VDP.PALETTE[2] = RGB555(0, 15, 0); // BG1

	// First comes BG0 tile sprites, then BG1 tile sprites (if not shared), then tile data.
	const int tile_data_start = bg_tilemap_size * bg_tilemap_size * (bg_shared_tilemap ? 1 : 2);

	// Setup background tile sprites.
	for (int i = 0; i < tile_data_start; i++) {
		uint16_t sprite =
			  ((i   & 0x7FF) << 0) // tilemap index (11 bits)
			| ((0   & 0x1) << 11) // screen index (1 bit)
			| ((0   & 0x3) << 12) // palette something ??? (2 bits)
			| ((0   & 0x001) << 14) // x-flip (1 bit)
			| ((0   & 0x001) << 15) // y-flip (1 bit)
		;
		VDP.TILE_VRAM[i] = sprite;
	}

	// Setup some sprites.
	for (int sprite_id = 0; sprite_id < 4; sprite_id++) {
		int pos = (sprite_id + 1) * 16;

		uint32_t sprite =
			  ((pos & 0x1FF) << 0) // x pos (9 bits)
			| ((0   & 0x001) << 9) // y pos hi (1 bit)
			| ((SPRITE_SIZE_8x8 & 0x003) << 10) // sprite size (2 bits)
			| ((0   & 0x003) << 12) // palette something ??? (2 bits)
			| ((0   & 0x001) << 14) // x-flip (1 bit)
			| ((0   & 0x001) << 15) // y-flip (1 bit)
			| ((pos & 0xFF) << 16) // y pos lo (8 bits)
			| ((sprite_id & 0xFF) << 24) // tilemap index (8 bits)
		;
		VDP.OAM[sprite_id] = sprite;
	}
	for (int sprite_id = 4; sprite_id < 128; sprite_id++) {
		uint32_t sprite = 0;
		VDP.OAM[sprite_id] = sprite;
	}

	// Fill tile data.
#if 0 // should default to 0
	for (int i = tile_data_start; i < 0x8000; i++) {
		VDP.TILE_VRAM[i] = 0;
	}
#endif
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
