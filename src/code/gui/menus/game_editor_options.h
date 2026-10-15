//
// The game editor's option ids (the rows' `opt`) and the rule for a disc in the drive. Pure: no screen needed.
//
#pragma once

#define OPT_FIRST 0
#define OPT_FAVORITE 0
#define OPT_LIGHTGUN 1
#define OPT_PLAY_USING_RA 2
#define OPT_LOCK 3
#define OPT_HIGHRES 4 // Resolution: the built-in NEON GPU only
#define OPT_SPEEDHACK 5
#define OPT_SCANLINES 6
#define OPT_SCANLINELV 7
#define OPT_CLOCK_PSX 8
#define OPT_FRAMESKIP 9
#define OPT_PLUGIN 10
#define OPT_INTERPOLATION 11
#define OPT_BOOTLOGO 12
#define OPT_SMOOTHING 13
#define OPT_SONYHACKS 14 // pcsx-abnxt only
#define OPT_FILTER 15
#define OPT_UNLOCK 16  // only while the game has its own config
#define OPT_NOSEAMS 17 // Remove seams: with Resolution
#define OPT_DITHERING 18
#define OPT_RESUME 19  // Resume: ask / last slot / never (EMU-26)
#define OPT_CRTMASK 20 // CRT mask: pcsx-abnxt, with the CRT (raster) filter

// A disc in the drive is no entry in the library: it can be neither a favorite, a light-gun game, played with
// RetroArch nor locked. Resume and the picture settings work (the disc has its own pcsx.cfg).
inline bool greyedForDisc(bool physicalDisc, int opt) {
    return physicalDisc && (opt == OPT_FAVORITE || opt == OPT_LIGHTGUN || opt == OPT_PLAY_USING_RA || opt == OPT_LOCK);
}
