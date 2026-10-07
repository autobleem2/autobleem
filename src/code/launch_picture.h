// Which picture stands on the screen while a program starts (splash/<name>), pure so a test can say it.
#pragma once

// A RetroArch game or another emulator gets RetroArch's loading picture; a PS1 game and an App (a program the
// scanner lists under Apps: it is a "foreign" entry like a RetroArch game, but RetroArch plays no part in it) get
// AutoBleem's own.
inline const char *waitingPictureName(bool foreign, bool app, bool otherEmulator) {
    return (foreign && !app) || otherEmulator ? "retroarch.jpg" : "autobleem.jpg";
}
