cmake_minimum_required(VERSION 4.3)
project(loopy)

include(web/compiler_flags.cmake)

add_compile_definitions(WEB_BUILD)
include_directories(web/include)

find_package(SDL3 REQUIRED COMPONENTS SDL3-static)

# Library
add_library(loopylib
    lib/debug.cc
    lib/engine.cc
    lib/graphics.cc
    lib/input.cc
    lib/profiler.cc
    lib/sound.cc
    web/web.cc
)
target_link_libraries(loopylib PRIVATE SDL3::SDL3-static)
target_include_directories(loopylib PUBLIC lib)

# BBB game
add_executable(bbb
    bbb/attract.cc
    bbb/breakout.cc
    bbb/cyber.cc
    bbb/data_ami_left.png.cc
    bbb/data_bomb.png.cc
    bbb/data_bucko_ball.png.cc
    bbb/data_bucko_left.png.cc
    bbb/data_bucko_troll_right.png.cc
    bbb/data_car.png.cc
    bbb/data_cover_raw.png.cc
    bbb/data_dome_raw.png.cc
    bbb/data_driving_bgm.mid.cc
    bbb/data_gauge.png.cc
    bbb/data_heart.png.cc
    bbb/data_main_menu_bgm2.mid.cc
    bbb/data_main_menu_bgm.mid.cc
    bbb/data_mm_bouncer.png.cc
    bbb/data_mouse.png.cc
    bbb/data_robucko_right.png.cc
    bbb/data_skyline_raw.png.cc
    bbb/data_text_font.png.cc
    bbb/data_tree.png.cc
    bbb/data_ufo.png.cc
    bbb/data_wormhole.cc
    bbb/driving.cc
    bbb/font.cc
    bbb/intertitle.cc
    bbb/main_menu.cc
    bbb/main.cc
    bbb/music.cc
    bbb/winner.cc
)
target_link_libraries(bbb loopylib)
if (EMSCRIPTEN)
    target_link_options(bbb PRIVATE
        --shell-file "${CMAKE_SOURCE_DIR}/web/shell_minimal.html"
    )
endif()

# Gallery
add_executable(gallery
    gallery/main.cc
    gallery/main_menu.cc
    gallery/attract.cc
)
target_link_libraries(gallery loopylib)
if (EMSCRIPTEN)
    target_link_options(gallery PRIVATE
        --shell-file "${CMAKE_SOURCE_DIR}/web/shell_minimal.html"
    )
endif()
