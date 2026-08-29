#include "loopy.h"
#include "../src/graphics.h"
#include "../src/utils.h"

#include <SDL3/SDL_error.h>
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_pixels.h>
#include <SDL3/SDL_surface.h>
#include <SDL3/SDL_timer.h>
#include <SDL3/SDL_video.h>

#include <array>
#include <cassert>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <memory>

#ifdef __EMSCRIPTEN__
#include "emscripten.h"
#endif

#define UNIMPLEMENTED() printf("Unimplemented: %s\n", __func__)


namespace {

struct SDLDeleter {
  void operator()(SDL_Palette *p) {
    SDL_DestroyPalette(p);
  }
  void operator()(SDL_Surface *s) {
    SDL_DestroySurface(s);
  }
  void operator()(SDL_Window *w) {
    SDL_DestroyWindow(w);
  }
};

//

constexpr std::size_t MAX_PALETTE_SIZE = 256;
constexpr int web_scale = 2;

std::unique_ptr<SDL_Window, SDLDeleter> s_window;
std::unique_ptr<SDL_Palette, SDLDeleter> s_palette;
std::unique_ptr<SDL_Surface, SDLDeleter> s_vram_buffer;
std::unique_ptr<SDL_Surface, SDLDeleter> s_sprites_buffer;

//

void error() {
    printf("\nExiting\n");
    exit(EXIT_FAILURE);
}

} // namespace


namespace web {

alignas(uint16_t) uint8_t vdp::BITMAP_VRAM_8BIT[0x20000];
uint16_t vdp::PALETTE[0x100];
uint32_t vdp::OAM[128];
uint16_t vdp::tile_data[(0x10000 - 0x1000) / 2];
uint16_t vdp::bg_sprite_data[32 * 32];
bool vdp::sprites_enabled;

void init() {
    using namespace engine::graphics;

    // Mostly copied from spidork98.

    if (!SDL_InitSubSystem(SDL_INIT_VIDEO)) {
        printf("Failed to init video: %s\n", SDL_GetError());
        error();
    }

    s_window.reset(SDL_CreateWindow("LoopyAmi", SCREEN_WIDTH * web_scale, SCREEN_HEIGHT * web_scale, 0));
    if (!s_window) {
        printf("Failed to create window: %s", SDL_GetError());
        error();
    }

    s_palette.reset(SDL_CreatePalette(MAX_PALETTE_SIZE));
    if (!s_palette) {
        printf("Failed to create palette: %s", SDL_GetError());
        error();
    }

    static_assert(engine::utils::size(VDP.BITMAP_VRAM_8BIT) == 256 * 512);
    s_vram_buffer.reset(SDL_CreateSurfaceFrom(256, 512, SDL_PIXELFORMAT_INDEX8, VDP.BITMAP_VRAM_8BIT, 256));
    if (!s_vram_buffer) {
        printf("Failed to create VRAM surface: %s", SDL_GetError());
        error();
    }
    static_assert(sizeof(VDP.tile_data) == bg_tile_size * bg_tile_size * 960);
    s_sprites_buffer.reset(SDL_CreateSurfaceFrom(8, bg_tile_size * bg_tile_size * 960, SDL_PIXELFORMAT_INDEX8, VDP.tile_data, 8));
    if (!s_sprites_buffer) {
        printf("Failed to create sprites surface: %s", SDL_GetError());
        error();
    }

    for (auto *surface : {s_vram_buffer.get(), s_sprites_buffer.get()}) {
        if (!SDL_SetSurfacePalette(surface, s_palette.get())) {
            printf("Failed to set palette on a surface: %s", SDL_GetError());
            error();
        }
    }

    // Setup the default palette.
    {
        std::array<SDL_Color, MAX_PALETTE_SIZE> pal;
        pal.fill({ .r = 0, .g = 0, .b = 0, .a = 0xFF });
        if (!SDL_SetPaletteColors(s_palette.get(), pal.data(), 0, pal.size())) {
            printf("Failed to set palette colours: %s", SDL_GetError());
            error();
        }
    }
}

uint32_t update_input() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
#ifndef __EMSCRIPTEN__
        if (event.type == SDL_EventType::SDL_EVENT_QUIT) {
            error();
        }
#endif
    }

    auto *keys = SDL_GetKeyboardState(nullptr);

#ifndef __EMSCRIPTEN__
    if (keys[SDL_Scancode::SDL_SCANCODE_ESCAPE]) error();
#endif

    uint32_t bits = 0;
    if (keys[SDL_Scancode::SDL_SCANCODE_Z]) bits |= GAMEPAD_BTN_A;
    if (keys[SDL_Scancode::SDL_SCANCODE_X]) bits |= GAMEPAD_BTN_B;
    if (keys[SDL_Scancode::SDL_SCANCODE_C]) bits |= GAMEPAD_BTN_C;
    if (keys[SDL_Scancode::SDL_SCANCODE_V]) bits |= GAMEPAD_BTN_D;
    if (keys[SDL_Scancode::SDL_SCANCODE_UP]) bits |= GAMEPAD_BTN_UP;
    if (keys[SDL_Scancode::SDL_SCANCODE_DOWN]) bits |= GAMEPAD_BTN_DOWN;
    if (keys[SDL_Scancode::SDL_SCANCODE_LEFT]) bits |= GAMEPAD_BTN_LEFT;
    if (keys[SDL_Scancode::SDL_SCANCODE_RIGHT]) bits |= GAMEPAD_BTN_RIGHT;
    if (keys[SDL_Scancode::SDL_SCANCODE_KP_ENTER]) bits |= GAMEPAD_BTN_START;
    return bits;
}

void draw() {
    using namespace engine::graphics;

    // Update the palette.
    {
        static_assert(engine::utils::size(VDP.PALETTE) == MAX_PALETTE_SIZE);
        std::array<SDL_Color, MAX_PALETTE_SIZE> pal;
        pal[0] = {0, 0, 0, 0xFF}; // first is always transparent
        for (std::size_t i = 1; i < MAX_PALETTE_SIZE; i++) {
            const auto rgb = VDP.PALETTE[i];
            auto & p = pal[i];
            p.r = ((rgb >> 10) & 31) * 255 / 31;
            p.g = ((rgb >>  5) & 31) * 255 / 31;
            p.b = ((rgb >>  0) & 31) * 255 / 31;
            p.a = 0;
        }
        SDL_SetPaletteColors(s_palette.get(), pal.data(), 0, pal.size());
    }

    // Clear the screen.
    SDL_Surface * window_surface = SDL_GetWindowSurface(s_window.get());
    SDL_FillSurfaceRect(window_surface, nullptr, 0);

    // Blit background.
    if (VDP.sprites_enabled) {
        if (Background<0>::show) {
            for (int j = 0; j < static_cast<int>(bg_tilemap_size); j++) {
                for (int i = 0; i < static_cast<int>(bg_tilemap_size); i++) {
                    BGSprite sprite;
                    sprite.raw = VDP.bg_sprite_data[j * bg_tilemap_size + i];
                    SDL_Rect src {
                        0, sprite.parts.tile_index * static_cast<int>(bg_tile_size),
                        bg_tile_size, bg_tile_size
                    };
                    constexpr int ts = bg_tile_size * web_scale;
                    SDL_Rect dst {
                        i * ts, j * ts,
                        ts, ts
                    };
                    SDL_BlitSurfaceScaled(s_sprites_buffer.get(), &src, window_surface, &dst, SDL_ScaleMode::SDL_SCALEMODE_NEAREST);
                }
            }
        }
    }

    // Blit bitmaps.
    auto blit_bitmap = [&](auto && bitmap) {
        if (bitmap.show) {
            SDL_Rect src {bitmap.sx, bitmap.sy, bitmap.w, bitmap.h};
            SDL_Rect dst {bitmap.px * web_scale, bitmap.py * web_scale, bitmap.w * web_scale, bitmap.h * web_scale};
            SDL_BlitSurfaceScaled(s_vram_buffer.get(), &src, window_surface, &dst, SDL_ScaleMode::SDL_SCALEMODE_NEAREST);
        }
    };
    blit_bitmap(Bitmap<0>{});
    blit_bitmap(Bitmap<1>{});
    blit_bitmap(Bitmap<2>{});
    blit_bitmap(Bitmap<3>{});

    // Blit sprites.
    if (VDP.sprites_enabled) {
        for (uint32_t raw : VDP.OAM) {
            ObjSprite sprite;
            sprite.raw = raw;
            if (!sprite.parts.y_hi) {
                SDL_Rect src {
                    0, sprite.parts.tile_index * static_cast<int>(bg_tile_size),
                    bg_tile_size, bg_tile_size
                };
                SDL_Rect dst {
                    sprite.parts.x * web_scale, sprite.parts.y_lo * web_scale,
                    bg_tile_size * web_scale, bg_tile_size * web_scale
                };
                SDL_BlitSurfaceScaled(s_sprites_buffer.get(), &src, window_surface, &dst, SDL_ScaleMode::SDL_SCALEMODE_NEAREST);
            }
        }
    }

    // Update the screen.
    SDL_UpdateWindowSurface(s_window.get());

    // Fake 60fps.
    SDL_Delay(16);
}

} // namespace web

namespace engine::graphics {

void set_backdrop_a(uint16_t rgb) {
    // TODO
    (void)rgb;
    UNIMPLEMENTED();
}

void set_backdrop_b(uint16_t rgb) {
    // TODO
    (void)rgb;
    UNIMPLEMENTED();
}

//

void enable_sprites() {
    VDP.sprites_enabled = true;
}
void disable_sprites() {
    VDP.sprites_enabled = false;
}

//

void wait_until_line0() {
    // TODO
    UNIMPLEMENTED();
}
void wait_until_line(uint16_t line) {
    // TODO
    (void)line;
    UNIMPLEMENTED();
}

} // namespace engine::graphics
