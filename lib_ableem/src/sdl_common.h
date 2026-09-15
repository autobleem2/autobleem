#pragma once
// Internal-only header. Never installed, never reachable from include/ableem/*.h.

#define ABLEEM_BUILDING
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_mixer.h>
#include <SDL2/SDL_ttf.h>
#include "SDL_FontCache.h"

// Compile-time target selection. ABLEEM_EMBEDDED_TARGET is defined by CMake for the PlayStation Classic and
// Raspberry Pi cross/native builds; everything else (Linux/Windows/Mac dev machines) is a "dev host": no
// cursor grab, keyboard-as-pad translation on by default, no forking a real emulator process.
#ifndef ABLEEM_EMBEDDED_TARGET
#define ABLEEM_DEV_HOST 1
#endif
