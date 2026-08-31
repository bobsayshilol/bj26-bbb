cmake_minimum_required(VERSION 4.3)
project(loopy)

include(web/compiler_flags.cmake)

add_compile_definitions(WEB_BUILD)
include_directories(web/include)

find_package(SDL3 REQUIRED COMPONENTS SDL3-static)

# BBB game
add_executable(bbb
    src/attract.cc
    src/breakout.cc
    src/cyber.cc
    src/data_ami_left.png.cc
    src/data_bomb.png.cc
    src/data_bucko_ball.png.cc
    src/data_bucko_left.png.cc
    src/data_bucko_troll_right.png.cc
    src/data_car.png.cc
    src/data_cover_raw.png.cc
    src/data_dome_raw.png.cc
    src/data_driving_bgm.mid.cc
    src/data_gauge.png.cc
    src/data_heart.png.cc
    src/data_main_menu_bgm2.mid.cc
    src/data_main_menu_bgm.mid.cc
    src/data_mm_bouncer.png.cc
    src/data_mouse.png.cc
    src/data_robucko_right.png.cc
    src/data_skyline_raw.png.cc
    src/data_text_font.png.cc
    src/data_tree.png.cc
    src/data_ufo.png.cc
    src/data_wormhole.cc
    src/debug.cc
    src/driving.cc
    src/engine.cc
    src/font.cc
    src/graphics.cc
    src/input.cc
    src/intertitle.cc
    src/main_menu.cc
    src/main.cc
    src/music.cc
    src/profiler.cc
    src/sound.cc
    src/winner.cc
    web/web.cc
)
target_link_libraries(bbb SDL3::SDL3-static)
if (EMSCRIPTEN)
    target_link_options(bbb PRIVATE
        #--embed-file "${CMAKE_SOURCE_DIR}/data@/aaa"
        --shell-file "${CMAKE_SOURCE_DIR}/web/shell_minimal.html"
    )
endif()
