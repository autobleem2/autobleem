//
// DebugDriver: a TCP line server that drives the program the way a pad and a keyboard would and hands
// back what is on the screen - for automated looks at the UI (tools/ab_drive.py is the client). Started
// only when the program asks for it (AutoBleem: AB_DEBUG_PORT in the environment on a dev host); nothing
// here runs otherwise.
//
// One command per line, one reply per command ("ok ..." or "err ..."):
//   press <button> [ms]      ButtonDown, a hold of ms (60), ButtonUp - x o s t start select l1 r1 l2 r2,
//                            up down left right (the d-pad)
//   down <button> / up <button>    a held button (down l2, press r2, up l2 = the L2+R2 system menu)
//   key <name>               KeyDown + KeyUp: escape return up down left right pageup pagedown home end tab
//                            backspace delete
//   text <utf8>              typed text (a TextInput event)
//   wait <ms>                sleep
//   shot <file.bmp|.png>     the last presented frame written to the file; waits up to 400 ms for a frame
//                            newer than the last input first, so a screen that redraws on events is caught
//                            after it did
//   frames                   how many frames were presented so far
//   screen                   the class name of the screen showing (GuiScreen::show keeps a stack; the
//                            launcher is "GuiLauncher", a dialog over it "GuiConfirm", ...) - what a client
//                            waits for before pressing anything
//   ping                     ok
//   quit                     a Quit event, as the window's close button
//
#pragma once

#include "gui_base.h"

#include <string>

namespace ableem {

class ABLEEM_API DebugDriver {
public:
    // listens on 127.0.0.1:port from a thread of its own for the rest of the process; false when the
    // port cannot be taken
    static bool start(GuiBase &gui, int port);

    // the screen stack GuiScreen::show maintains (a typeid name; the compiler's decoration is stripped for
    // the `screen` reply). Cheap, and kept whether or not the driver runs.
    static void pushScreen(const char *typeName);
    static void popScreen();
    static std::string currentScreen();
};

} // namespace ableem
