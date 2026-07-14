#pragma once

#include "loopy/bios.h"

namespace engine::sound {

// Magic sound state.
extern soundstate_t g_sound_state;

void play_startup_sound();

} // namespace engine::sound
