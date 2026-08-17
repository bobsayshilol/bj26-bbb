#pragma once

#include "debug.h"

#include <stdint.h>

namespace game {

extern bool g_won;

enum class Entry : uint8_t {
    Attract,
    MainMenu,
    Breakout,
    Driving,
    //Cyber,
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

namespace breakout {
void enter();
Entry loop();
void leave();
} // namespace breakout

namespace driving {
void enter();
Entry loop();
void leave();
} // namespace driving

namespace cyber {
void enter();
Entry loop();
void leave();
} // namespace cyber

} // namespace game
