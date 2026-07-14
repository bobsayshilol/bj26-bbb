#pragma once

#include <stdint.h>

#include "loopy/constants.h"

namespace engine::input {

// Whether or not devices are plugged in.
extern bool g_mouse_plugged;
extern bool g_pad_plugged; // assumed to always be set?

// Active keypad buttons. GAMEPAD_* and MOUSE_* macros.
extern uint16_t g_buttons_pressed; // pressed this frame
extern uint16_t g_buttons_held; // held down
extern int16_t g_mouse_dx;
extern int16_t g_mouse_dy;

void update_inputs();

} // namespace engine::input
