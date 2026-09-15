#include "gallery.h"
#include "graphics.h"

namespace gallery::main_menu {

void enter() {
}

void leave() {
}

Screen loop() {
    DEBUG_MSG("TODO: ", __func__);
    bios_vsync();
    return Screen::MainMenu;
}

} // namespace main_menu
