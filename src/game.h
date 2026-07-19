#pragma once

#include "debug.h"

#include <stdint.h>

namespace game {

enum class Entry : uint8_t {
    Attract,
    MainMenu,
    Breakout,
    Driving,
    //Cyber,
};

Entry attract_loop();
Entry main_menu_loop();
Entry breakout_loop();
Entry driving_loop();
//Entry cyber_loop();

} // namespace game
