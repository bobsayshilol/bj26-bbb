#include "graphics.h"

#include "loopy.h"

namespace engine::graphics {

void draw_something() {
	bios_vsync();

	// Fill a 256x224 area with a 16x15 grid of colors 1-240
	for(uint16_t y = 0; y < 224; y++) {
		uint16_t gradY = bios_mathDivU16(y, 15);
		for(uint16_t x = 0; x < 256; x++) {
			uint16_t gradX = x / 16;
			VDP.BITMAP_VRAM_8BIT[(y*256)+x] = gradY * 16 + gradX + 1;
		}
	}

	// Create a 2D gradient in colors 1-240
	for(int j = 1; j <= 240; j++) {
		int gradX = (j-1) % 16;
		int gradY = (j-1) / 16;
		VDP.PALETTE[j] = RGB555(gradX*2+1, gradY*2+1, 0);
	}

	// Set up registers to display BM0 full screen
	VDP.BACKDROP_A      = RGB555(0, 0, 0);
	VDP.BACKDROP_B      = RGB555(0, 0, 0);
	VDP.BLEND           = BLEND_MATH;
	VDP.BM_SCROLLX[0]   = 0;
	VDP.BM_SCROLLY[0]   = 0;
	VDP.BM_SCREENX[0]   = 0;
	VDP.BM_SCREENY[0]   = 0;
	VDP.BM_WIDTH[0]     = 256-1;
	VDP.BM_HEIGHT[0]    = 224-1;
	VDP.BM_CTRL         = BM_MODE_8BPP_SHARED;
	VDP.BM_SUBPAL       = BM_SUBPAL(0,0,0,0);
	VDP.BM_COL_LATCH[0] = 0;
	VDP.SCREENPRIO      = BLEND_MATH_ADD | SCREEN_A_ENABLE | PRIORITY_BM_A | PRIORITY_BG0_A | PRIORITY_OBJ0_A;
	VDP.LAYER_CTRL      = LAYER_SCREEN(LAYER_SCREEN_A, LAYER_SCREEN_A, LAYER_SCREEN_A, LAYER_SCREEN_A) | LAYER_ENABLE_BM0;
}

} // namespace engine::graphics
