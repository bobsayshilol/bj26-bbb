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
// VDP.LAYER_CTRL |= LAYER_WRITE_OBJ0_TO_A

}

void init() {
#if !WEB_BUILD
	// 8bpp simplifies things, probably won't need 4bpp.
	VDP.BM_CTRL = BM_MODE_8BPP_SHARED; // bitmap_ctrl

	// TODO
	VDP.BLEND           = BLEND_MATH;
	VDP.BM_SUBPAL       = BM_SUBPAL(0,0,0,0); // bitmap_palsel
	VDP.SCREENPRIO      = BLEND_MATH_ADD | SCREEN_A_ENABLE | PRIORITY_BM_A | PRIORITY_BG0_A | PRIORITY_OBJ0_A;
	VDP.LAYER_CTRL      = LAYER_SCREEN(LAYER_SCREEN_A, LAYER_SCREEN_A, LAYER_SCREEN_A, LAYER_SCREEN_A);

	// Setup BG/sprite split.
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
#endif

	set_backdrop_a(RGB555(0, 0, 0));
	set_backdrop_b(RGB555(0, 0, 0));
}

} // namespace engine::graphics
