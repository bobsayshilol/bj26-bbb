#include "gallery.h"
#include "graphics.h"

namespace gallery::main_menu {

void enter() {
}

void leave() {
}

Entry loop() {
    DEBUG_MSG("TODO: ", __func__);
    bios_vsync();
    return Entry::MainMenu;
}

} // namespace main_menu
