//
// GuiCdPlayer: the Audio CD player screen (autobleem-main docs/disc-drive-plan.md piece 8) on core's engine - a
// CddaPlayer reading a CdDrive, an ableem::AudioStream playing it and an ableem::Spectrum for the bars. The jewel
// case and the disc's facts at the left, the spectrum in a panel beside them, the track and its time with a seek bar
// under it, a row of transport keys, then one key per track.
//
// The theme's music stops on entry (Music::halt before the stream starts - AudioStream replaces the music slot) and
// comes back on leaving. Circle leaves. Until the disc view exists the screen is opened by a dev entry: the
// environment variable AB_CD_PLAYER_IMAGE=<cue> (runCdPlayerDevEntry, called once by the launcher at start) plays
// that image through an ImageCdDrive.
//
#pragma once

#include "../../app.h"
#include "gui/gui_screen.h"
#include "gui/panel_style.h"
#include "cd_player_logic.h"

#include <ableem/engine/spectrum.h>
#include <ableem/ui/audio_stream.h>
#include <ableem/ui/texture.h>

#include "core/services/cdda_player.h"
#include "core/services/image_cd_drive.h"

#include <memory>
#include <string>
#include <vector>

class GuiCdPlayer : public GuiScreen {
public:
    App &app = App::get();
    void init() override;
    void draw() override; // the frame's picture: the stack clears before and presents after
    void loop() override;

    // in: one of the two. A drive that is open and its TOC (borrowed: they outlive the screen), or a cue to open.
    CdDrive *drive = nullptr;
    CdToc toc;
    std::string cuePath;
    // CD-Text, when the drive gives it (none does yet): the album line and a title per track number (index 0 = track 1)
    std::string album;
    std::vector<std::string> trackTitles;

    explicit GuiCdPlayer(ableem::GuiBase &g) : GuiScreen(g) {
        declareTransitions(abgui::ScreenTransitions(abgui::Transition::pop()));
    }

private:
    static const int Bars = 40;
    static const int SpectrumFrames = 1024;

    std::unique_ptr<ImageCdDrive> image;
    // declared before the stream: the stream (its callback reads the player) is destroyed first
    std::unique_ptr<CddaPlayer> player;
    ableem::AudioStream stream;
    ableem::Spectrum spectrum{Bars, SpectrumFrames};
    std::vector<float> caps = std::vector<float>(Bars, 0.0f); // the peak caps, falling slower than the bars
    long lastAnalysis = 0;
    bool ready = false;
    bool musicWasStopped = false;

    PanelStyle style;
    ableem::Texture caseImage;
    std::vector<int> numbers; // the audio tracks' numbers, in disc order
    int focus = 0;            // index into numbers
    int firstRow = 0;

    void openDrive();
    void loadCase();
    std::string titleOf(int number) const;
    int playingIndex() const; // index into numbers of the track being heard, -1 when stopped
    void playFocused();
    void togglePause();
    void goTo(bool forward);
    void cycleRepeat();
    void toggleShuffle();
    void seekBy(int direction);
    void publishItems() const;
    void drawTransportKey(int kind, const ableem::Rect &box, bool lit, const ableem::Color &color);
    int gridRows() const;
};

// the dev entry: with AB_CD_PLAYER_IMAGE set, opens the screen on that cue once (no-op the second time and when unset)
void runCdPlayerDevEntry(ableem::GuiBase &gui);
