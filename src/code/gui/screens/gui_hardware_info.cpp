//
// GuiHardwareInfo: the built-in Hardware Information screen.
//
#include "gui_hardware_info.h"
#include "../gui.h"

#include <algorithm>
#include <cstdio>

using namespace std;

//*******************************
// GuiHardwareInfo::collect
//*******************************
vector<InfoSection> GuiHardwareInfo::collect() {
    vector<InfoSection> sections = systemInfo.collect();
    sections.push_back(displayAndInput());
    return sections;
}

//*******************************
// GuiHardwareInfo::displayAndInput
//*******************************
// what SystemInfoService cannot know: the window it is drawn in and the pads it is driven with
InfoSection GuiHardwareInfo::displayAndInput() {
    InfoSection section{_("Display and input"), {}};
    auto add = [&](const string &label, const string &value) {
        if (!value.empty())
            section.rows.push_back({label, value});
    };
    ableem::Platform &platform = gui->platform();
    string rendererName = renderer.driverName();
    if (platform.multisampleSamples() > 0)
        rendererName += ", " + to_string(platform.multisampleSamples()) + "x MSAA";
    add(_("Renderer"), rendererName);
    add(_("Video driver"), platform.videoDriverName());
    add(_("Display mode"), platform.displayModeString());
    // the canvas every screen draws on, and how many output pixels one of its pixels is (see Gui::outputScale)
    string canvas = to_string(renderer.width()) + "x" + to_string(renderer.height());
    if (renderer.outputScale() != 1.0f) {
        char scale[16];
        snprintf(scale, sizeof(scale), " x%.2g", renderer.outputScale());
        canvas += scale;
    }
    add(_("Canvas"), canvas);
    add(_("Audio driver"), gui->audio().driverName());
    add("SDL", platform.linkedVersion());
    vector<ableem::PadInfo> pads = gui->input().pads();
    if (pads.empty()) {
        add(_("Controllers"), platform.isDevHost() ? _("Keyboard") : _("None"));
    } else {
        for (size_t i = 0; i < pads.size(); i++)
            add(_("Controller") + " " + to_string(i + 1), pads[i].name);
    }
    return section;
}
