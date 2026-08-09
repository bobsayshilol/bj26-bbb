#pragma once

#include "loopy/bios.h"

namespace engine::sound {

// Magic sound state.
extern soundstate_t g_sound_state;

void init();

// Set the BGM and SFX track lists.
void set_lists(const uint8_t * const * bgm, const uint8_t * const * sfx);

// Play a BGM or SFX track.
void play_bgm(uint8_t index);
void play_effect(uint8_t index);

} // namespace engine::sound
