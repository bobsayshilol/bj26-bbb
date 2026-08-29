#include "loopy.h"
#include "../src/graphics.h"

#include <SDL3/SDL_error.h>
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_pixels.h>
#include <SDL3/SDL_surface.h>
#include <SDL3/SDL_timer.h>
#include <SDL3/SDL_video.h>

#include <cassert>
#include <array>
#include <chrono>
#include <memory>

#ifdef __EMSCRIPTEN__
#include "emscripten.h"
#endif



namespace web {

vdp VDP;

void init() {
    // TODO
}

uint32_t update_input() {
    // TODO
    return 0;
}

void draw() {
    // TODO
}

} // namespace web

namespace engine::graphics {

void set_backdrop_a(uint16_t rgb) {
    // TODO
    (void)rgb;
}

void set_backdrop_b(uint16_t rgb) {
    // TODO
    (void)rgb;
}

void set_palette_colour(uint8_t idx, uint16_t rgb) {
    // TODO
    (void)idx;
    (void)rgb;
}

//

uint16_t tmp;
template <uint8_t Index>
uint16_t & Bitmap<Index>::position_x() {
    // TODO
    return tmp;
}
template <uint8_t Index>
uint16_t & Bitmap<Index>::position_y() {
    // TODO
    return tmp;
}
template <uint8_t Index>
uint16_t & Bitmap<Index>::scroll_x() {
    // TODO
    return tmp;
}
template <uint8_t Index>
uint16_t & Bitmap<Index>::scroll_y() {
    // TODO
    return tmp;
}
template <uint8_t Index>
uint16_t & Bitmap<Index>::width() {
    // TODO
    return tmp;
}
template <uint8_t Index>
uint16_t & Bitmap<Index>::height() {
    // TODO
    return tmp;
}
template <uint8_t Index>
uint16_t & Bitmap<Index>::latch() {
    // TODO
    return tmp;
}
template <uint8_t Index>
void Bitmap<Index>::enable() {
    // TODO
}
template <uint8_t Index>
void Bitmap<Index>::disable() {
    // TODO
}
template struct Bitmap<0>;
template struct Bitmap<1>;
template struct Bitmap<2>;
template struct Bitmap<3>;

//

Pixel2 * get_tile_data(TileIndex idx) {
    // TODO
    (void)idx;
    return nullptr;
}
void set_sprite(uint8_t idx, ObjSprite const & sprite) {
    // TODO
    (void)idx;
    (void)sprite;
}
void set_bg_sprite_(uint8_t BGx, uint8_t x, uint8_t y, BGSprite const & sprite) {
    // TODO
    (void)BGx;
    (void)x;
    (void)y;
    (void)sprite;
}

//

template <uint8_t Index>
void Background<Index>::enable() {
    // TODO
}
template <uint8_t Index>
void Background<Index>::disable() {
    // TODO
}
template struct Background<0>;
template struct Background<1>;

//

void enable_sprites() {
    // TODO
}
void disable_sprites() {
    // TODO
}

//

void wait_until_line0() {
    // TODO
}
void wait_until_line(uint16_t line) {
    // TODO
    (void)line;
}

} // namespace engine::graphics
