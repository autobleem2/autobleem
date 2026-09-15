# lib_ableem - portable SDL2 wrapper (Linux, Windows/MinGW, PlayStation Classic, Raspberry Pi).
# The app must never include an SDL header directly; only headers under include/ableem/ are public.

option(ABLEEM_SHARED "Build lib_ableem as a shared library instead of static" OFF)
option(ABLEEM_BUILD_DEMO "Build the ableem_demo example executable" ON)
option(ABLEEM_EMBEDDED_TARGET "Building for an embedded target (PSC/RPi): no cursor grab, no keyboard-as-pad" OFF)

if (ABLEEM_SHARED)
    set(ABLEEM_LIB_KIND SHARED)
else()
    set(ABLEEM_LIB_KIND STATIC)
endif()

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

find_package(Threads REQUIRED)
target_link_libraries(ableem PRIVATE Threads::Threads)

if (ABLEEM_BUILD_DEMO)
    add_executable(ableem_demo examples/demo.cpp)
    target_link_libraries(ableem_demo PRIVATE ableem)
endif()
