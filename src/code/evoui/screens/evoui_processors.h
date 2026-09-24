//
// GuiProcessors: the System menu's Scanner processors - the two sequences (PS1, ROMs) the scan runs the
// processors in System/Processors/ in, for the user to sort and switch on and off
// (docs/scanner-processors-plan.md, "Sequences").
//
#pragma once

#include "core/services/processor_catalog.h"
#include "core/services/processor_sequences.h"
#include "core/services/processor_state.h"
#include "gui/gui_screen.h"
#include "gui/panel_style.h"

#include <set>
#include <string>
#include <vector>

//******************
// GuiProcessors
//******************
// A compact panel over the launcher's dimmed background, one tab per sequence (L1/R1), one row per processor
// in the order it runs. Cross switches the selected one on or off, Square picks it up so Up/Down move it
// (Square again, or Cross, puts it down), Triangle has it run again on everything at the next scan. A
// processor with no program for this machine keeps its place, greyed. Leaving saves sequence.ini when
// anything changed; changed() tells the caller to request a scan.
class GuiProcessors : public GuiScreen {
public:
    using GuiScreen::GuiScreen;

    void init() override;
    void render() override;
    void loop() override;

    ableem::Texture background; // the launcher's frame, drawn dimmed under the panel

    // the order, an on/off switch or a "run again" changed: the caller requests a scan
    bool changed() const { return changed_; }

private:
    ProcessorSequence sequence = ProcessorSequence::Ps1;
    std::vector<ProcessorInfo> installed;
    ProcessorSequences sequences{""};
    ProcessorState state{""};
    bool changed_ = false;
    bool stateChanged = false;
    bool moving = false;         // Square picked the selected row up
    std::set<std::string> rerun; // Triangle: forgotten, runs on everything again
    int selected = 0;
    int firstVisible = 0;

    const ProcessorInfo *info(const std::string &name) const;
    int count() const { return static_cast<int>(sequences.entries(sequence).size()); }
    int visibleRows() const;
    void moveSelection(int step);
    void switchTab(ProcessorSequence to);
    void save();

    PanelStyle style;
};
