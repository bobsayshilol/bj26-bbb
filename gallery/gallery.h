#pragma once

#include "debug.h"
#include "utils.h"

#include <stdint.h>

namespace gallery {

enum class Entry : uint8_t {
    Attract,
    MainMenu,
    //BGM,
    //Viewer,
};

namespace attract {
void enter();
Entry loop();
void leave();
} // namespace attract

namespace main_menu {
void enter();
Entry loop();
void leave();
} // namespace main_menu

} // namespace gallery
