#pragma once
// ui/     - everything SDL/rendering-facing: window, renderer, textures, fonts, audio, input, screens (target: ableem).
// engine/ - portable, SDL-free logic: filesystem, config files, database, disc images, game scanning
//           (target: ableem_engine, which ableem links).
#include "engine.h"
#include "ui/types.h"
#include "ui/platform.h"
#include "ui/renderer.h"
#include "ui/texture.h"
#include "ui/font.h"
#include "ui/audio.h"
#include "ui/input.h"
#include "ui/gui_base.h"
#include "ui/gui_screen.h"
