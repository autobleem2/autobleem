cmake_minimum_required(VERSION 3.10)
project(ABFlashKit)

set(CMAKE_CXX_STANDARD 11)







find_package(Threads REQUIRED
        SDL2 REQUIRED)

add_executable(ABFlashKit main.cpp zip/zip.c src/flasher.cpp src/flasher.h src/gui/gui_sdl_wrapper.cpp src/gui/gui.cpp src/gui/gui.h src/rawc.h src/led.cpp src/led.h
        src/gui/gui_confirm.cpp src/gui/gui_screen.cpp src/engine/padmapper.cpp src/engine/padmapper.h src/lang.cpp src/environment.cpp src/util.cpp src/DirEntry.cpp
        src/gui/abl.c src/engine/config.cpp src/engine/inifile.cpp)

target_link_libraries(ABFlashKit
        Threads::Threads
        SDL2
        SDL2_image
        SDL2_mixer
        SDL2_ttf
        ${CMAKE_DL_LIBS}
        )

add_custom_command(TARGET ABFlashKit PRE_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/res $<TARGET_FILE_DIR:ABFlashKit>)