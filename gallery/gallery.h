#pragma once

#include "debug.h"
#include "utils.h"

#include <stdint.h>

namespace gallery {

enum class Screen : uint8_t {
    Attract,
    MainMenu,
    //BGM,
    //Viewer,
};

namespace attract {
void enter();
Screen loop();
void leave();
} // namespace attract

namespace main_menu {
void enter();
Screen loop();
void leave();
} // namespace main_menu

} // namespace gallery
