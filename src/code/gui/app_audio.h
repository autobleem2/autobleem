//
// AppAudio: AutoBleem's music track and UI sound effects.
//
#pragma once

#include "../core/services/config.h"
#include "../core/services/theme.h"

#include <ableem/ui/audio.h>

#include <string>

//******************
// AppAudio
//******************
// The theme's five UI sound effects and the one background music track, plus the rules about which track to
// play (the theme's, or the user's own file from resources/music) and at which sample rate. Owned by App and
// reached as `app.audio()`.
//
// Not to be confused with `gui->audio()`, the ableem::Audio this sits on: that one is just the mixer device
// (open/close). Everything that knows what a theme or config.ini is lives here.
class AppAudio {
public:
    AppAudio(ableem::Audio &device, Config &config, Theme &theme) : device(device), config_(config), theme_(theme) {}

    // (re)loads the current theme's sound effects. When reloadMusic is true the music is brought in line with
    // the theme and config.ini too - but only if something about it changed (the track, its sample rate,
    // looping, or whether music is on at all): stepping through the Options theme list used to restart the
    // same track on every step. Called from Gui::loadAssets.
    void loadTheme(bool reloadMusic);

    void playMusic();
    void freeMusic();
    void restart() { device.open(freq, 2, 1024); }
    void close() { device.close(); }

    // fades the music out (or waits the same 300ms), then drops every sound and closes the mixer. Used when
    // the GUI is about to go away - before a game starts and on the way out of the app.
    void shutdown();

    ableem::Music music;

    ableem::Sound cursor;
    ableem::Sound cancel;
    ableem::Sound home_down;
    ableem::Sound home_up;
    ableem::Sound resume;

private:
    ableem::Audio &device;
    Config &config_;
    Theme &theme_;

    bool customMusic = false;    // true when config.ini names a track of the user's own
    int freq = 44100;
    std::string musicPath;       // the theme's music file (resolved), or the custom file name

    // everything that decides what plays; loadTheme() compares the next one against what is playing
    struct MusicState {
        bool custom = false;
        int freq = 0;
        std::string path;
        bool enabled = false;
        int loops = 0;
        bool operator==(const MusicState &o) const {
            return custom == o.custom && freq == o.freq && path == o.path && enabled == o.enabled && loops == o.loops;
        }
    };
    MusicState wantedMusicState() const;
    MusicState playing_;         // what loadTheme() last started (nothing yet: freq 0)
};
