#include "input.h"
#include "loopy.h"
#include "utils.h"

namespace engine::utils {

RNG g_rng(123);

} // namespace engine::utils

namespace engine::input {

bool g_mouse_plugged;
bool g_pad_plugged;

uint16_t g_buttons_pressed;
uint16_t g_buttons_held;
int16_t g_mouse_dx;
int16_t g_mouse_dy;

void update_inputs() {
	// Get the mouse XY deltas and buttons
	int16_t mouseXB = VDP.IO_MOUSEX;
	int16_t mouseY  = VDP.IO_MOUSEY;

	// Set the motion from XY deltas
	// A scale of 1/2 seems comfortable but may not match other games
	// Convert from 12bit signed using MOUSE_DELTA
	g_mouse_dx =  MOUSE_DELTA(mouseXB) / 2; // X is +right
	g_mouse_dy = -MOUSE_DELTA(mouseY)  / 2; // Y is +up so invert it

	// Update button state from mouse and gamepad buttons
	uint32_t buttonsNow = MOUSE_BUTTONS(mouseXB) | READ_GAMEPAD1;
	g_buttons_pressed = buttonsNow & ~g_buttons_held;
	g_buttons_held = buttonsNow;

	// Add some randomness to the global RNG.
	utils::g_rng.add_entropy(g_mouse_dx);
	utils::g_rng.add_entropy(g_buttons_held);
}

} // namespace engine::input
