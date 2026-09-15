#pragma once
// Convenience header pulling in the whole public API. Individual headers can be included on their own too.
//
// ui/    - everything SDL/rendering-facing: window, renderer, textures, fonts, audio, input, screens.
// engine/ - reserved for future non-rendering portable library code; empty for now.

#include "ui/types.h"
#include "ui/platform.h"
#include "ui/renderer.h"
#include "ui/texture.h"
#include "ui/font.h"
#include "ui/audio.h"
#include "ui/input.h"
#include "ui/gui_base.h"
#include "ui/gui_screen.h"
