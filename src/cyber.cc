#include "game.h"
#include "graphics.h"
#include "sound.h"
#include "music.h"

namespace game::cyber {

namespace {

} // namespace

void enter() {
    // TODO: draw bits.

    // Show everything now that it's drawn.
    bios_vsync();
    engine::graphics::bitmap_0.enable();
    engine::graphics::enable_sprites();

    // Kick off the bgm.
    engine::sound::play_bgm(game::music::Bgm::Bgm_Weird);
}

void leave() {
    // Reset graphics state.
    bios_vsync();
    engine::graphics::disable_sprites();
    engine::graphics::bitmap_0.disable();
    //engine::graphics::reset_sprites<transparent_sprite_start + transparent_sprite_count>();

    // Reset sound.
    engine::sound::stop_bgm();
}

Entry loop() {
    // TODO
    return Entry::Winner;
}

} // namespace game::cyber
