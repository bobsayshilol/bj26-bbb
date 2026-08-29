#pragma once

#include <cstdint>
#include <cstddef>
#include "../../include/loopy/constants.h"

namespace web {
void init();
uint32_t update_input();
void draw();

struct vdp {
    // These are intentionally static to catch reads/writes out of bounds.

    alignas(uint16_t) static uint8_t BITMAP_VRAM_8BIT[0x20000];
    static uint16_t PALETTE[0x100];
    static uint32_t OAM[128];

    static uint16_t tile_data[(0x10000 - 0x1000) / 2];
    static uint16_t bg_sprite_data[32 * 32];
    static bool sprites_enabled;
};
inline constexpr vdp VDP;
} // namespace web

namespace loopy {

//
// Graphics.
//

constexpr inline uint16_t RGB555(uint8_t r, uint8_t g, uint8_t b) {
    return (((r) << 10) | ((g) << 5) | (b));
}

inline void bios_vsync() { web::draw(); }

using web::VDP;

//
// Sound.
//

struct soundstate_t {};

inline void bios_playBgm(soundstate_t *, uint8_t, uint16_t, const uint8_t *const *) {}
inline void bios_playSfx(soundstate_t *, uint8_t, uint16_t, const uint8_t *const *) {}
inline void sys_stopBgm(soundstate_t *) {}
inline void bios_soundVolume(int, int) {}
inline bool sys_bgmRunning() { return false; }

//
// Maffs.
//

#define bios_mathMulS16(a, b) ((a) * (b))
#define bios_mathMulU16(a, b) ((a) * (b))
#define bios_mathDivU16(a, b) ((a) / (b))

} // namespace loopy

using namespace loopy;
