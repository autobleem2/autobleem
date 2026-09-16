# lib_ableem - portable game-launcher library (Linux, Windows/MinGW, PlayStation Classic, Raspberry Pi).
# Two targets:
#   ableem_engine - SDL-free: filesystem, strings, ini/cfg files, SQLite game database, disc images, scanner.
#   ableem        - the SDL2 wrapper (window, renderer, textures, fonts, audio, input); links ableem_engine.
# The app must never include an SDL header directly; only headers under include/ableem/ are public.

option(ABLEEM_SHARED "Build lib_ableem as a shared library instead of static" OFF)
option(ABLEEM_BUILD_DEMO "Build the ableem_demo example executable" ON)
option(ABLEEM_EMBEDDED_TARGET "Building for an embedded target (PSC/RPi): no cursor grab, no keyboard-as-pad" OFF)
option(ABLEEM_ENABLE_CHD "CHD disc image support (needs libmamecd from the PSC toolchain sysroot)" ON)

if (ABLEEM_SHARED)
    set(ABLEEM_LIB_KIND SHARED)
else()
    set(ABLEEM_LIB_KIND STATIC)
endif()

#*******************************
# ableem_engine
#*******************************
add_library(ableem_engine ${ABLEEM_LIB_KIND}
        src/engine/strings.cpp
        src/engine/filesystem.cpp
        src/engine/environment.cpp
        src/engine/ini_file.cpp
        src/engine/lang.cpp
        src/engine/config_file_editor.cpp
        src/engine/memcard_manager.cpp
        src/engine/iso_directory.cpp
        src/engine/serial_scanner.cpp
        src/engine/ecm_decoder.cpp
        src/engine/game_database.cpp
        src/engine/cover_database.cpp
        src/engine/usb_game.cpp
        src/engine/games_hierarchy.cpp
        src/engine/game_scanner.cpp
        src/engine/retroarch_playlist.cpp
        src/engine/game_library.cpp
        src/engine/md5.cpp
        src/engine/unecm.c
        src/engine/cd_image_reader.h
        src/engine/binary_reader.h
        src/engine/md5.h
        third_party/sqlite/sqlite3ab.c
        )

target_include_directories(ableem_engine
        PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/third_party/sqlite
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/third_party/nlohmann
        )

# the vendored sqlite amalgamation is not ours to keep warning-free
set_source_files_properties(third_party/sqlite/sqlite3ab.c PROPERTIES COMPILE_FLAGS "-w")

find_package(Threads REQUIRED)
target_link_libraries(ableem_engine PUBLIC Threads::Threads ${CMAKE_DL_LIBS})

if (ABLEEM_ENABLE_CHD)
    target_link_libraries(ableem_engine PUBLIC mamecd)
else()
    target_compile_definitions(ableem_engine PRIVATE ABLEEM_NO_CHD)
endif()

if (ABLEEM_SHARED)
    target_compile_definitions(ableem_engine PUBLIC ABLEEM_SHARED)
endif()

#*******************************
# ableem (ui)
#*******************************
add_library(ableem ${ABLEEM_LIB_KIND}
        src/ui/platform.cpp
        src/ui/renderer.cpp
        src/ui/texture.cpp
        src/ui/font.cpp
        src/ui/audio.cpp
        src/ui/input.cpp
        src/ui/gui_base.cpp
        src/ui/gui_screen.cpp
        src/ui/SDL_FontCache.c
        src/ui/SDL_FontCache.h
        src/ui/psc_event_filter.c
        src/ui/psc_event_filter.h
        src/ui/sdl_common.h
        )

target_include_directories(ableem
        PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
        )

target_link_libraries(ableem PUBLIC ableem_engine)

if (ABLEEM_EMBEDDED_TARGET)
    target_compile_definitions(ableem PRIVATE ABLEEM_EMBEDDED_TARGET)
endif()

if (ABLEEM_SHARED)
    target_compile_definitions(ableem PUBLIC ABLEEM_SHARED)
endif()

# SDL2 discovery: MinGW uses pkg-config (see the root CMakeLists.txt comment for why), everything else uses
# the CMake SDL2 config package. Either way, SDL is a PRIVATE dependency of the lib - nothing SDL-shaped
# leaks into a consumer's include path.
if (MINGW)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(ABLEEM_SDL REQUIRED IMPORTED_TARGET sdl2 SDL2_image SDL2_mixer SDL2_ttf)
    target_link_libraries(ableem PRIVATE PkgConfig::ABLEEM_SDL)
else()
    find_package(SDL2 REQUIRED)
    target_link_libraries(ableem PRIVATE SDL2 SDL2_image SDL2_mixer SDL2_ttf)
endif()

if (ABLEEM_BUILD_DEMO)
    add_executable(ableem_demo examples/demo.cpp)
    target_link_libraries(ableem_demo PRIVATE ableem)
endif()
