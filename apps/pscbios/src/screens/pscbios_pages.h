//
// The tool's static texts: the About credits and the two pairing pages.
//
#pragma once

#include <string>
#include <vector>

std::vector<std::string> pscbiosCredits(); // GuiAbout's credits: headings (GuiAbout::HeadingMark) and names
std::vector<std::string> pscbiosFoot();    // GuiAbout's foot: support and the licence line
std::vector<std::string> dualshock3PairingLines();
std::vector<std::string> bluetoothPairingLines();
