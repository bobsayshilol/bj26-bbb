#pragma once

#include "debug.h"
#include "utils.h"

#include <stdint.h>

namespace game {

enum class Entry : uint8_t {
    // Helpers.
    Attract,
    Intertitle,
    MainMenu,

    // Levels.
    // Used by g_unlocked below.
    Breakout,
    Driving,
    Cyber,
    Winner,
};

constexpr uint16_t unlock_all = 100;
extern uint16_t g_unlocked;
constexpr uint32_t invalid_frame_count = 1 << 20;
extern uint32_t g_frame_count;

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

namespace intertile {
enum class Text { Intro, Meanwhile, Dome, GameOver, Won, };
void setup(Text text, Entry next);
void enter();
Entry loop();
void leave();
} // namespace driving

namespace cyber {
struct Line { uint8_t x0, y0, x1, y1; };
extern engine::utils::Array<Line, 3> g_lines_A;
extern engine::utils::Array<Line, 4> g_lines_M;
extern engine::utils::Array<Line, 1> g_lines_I;
extern engine::utils::Array<Line, 3> g_lines_C;
extern engine::utils::Array<Line, 3> g_lines_U;
extern engine::utils::Array<Line, 2> g_lines_T;
extern engine::utils::Array<Line, 4> g_lines_E;
void enter();
Entry loop();
void leave();
} // namespace cyber

namespace winner {
void enter();
Entry loop();
void leave();
} // namespace winner

} // namespace game
