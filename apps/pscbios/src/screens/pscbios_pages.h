//
// The tool's static texts: the About credits and the DualShock 3 USB-pairing page. (Bluetooth pairing of
// DualShock 4 and other gamepads is an interactive screen now - GuiBtPairing - not a static page.)
//
#pragma once

#include <string>
#include <vector>

std::vector<std::string> pscbiosCredits(); // GuiAbout's credits: headings (GuiAbout::HeadingMark) and names
std::vector<std::string> pscbiosFoot();    // GuiAbout's foot: support and the licence line
std::vector<std::string> dualshock3PairingLines();
