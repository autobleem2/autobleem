//
// AppAudio: AutoBleem's music track and UI sound effects.
//
#pragma once

#include <string>
#include <ableem/ui/audio.h>

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
    explicit AppAudio(ableem::Audio &device) : device(device) {}

    // (re)loads the current theme's sound effects. When reloadMusic is true the mixer is also reopened at the
    // sample rate the track needs and the track is started. Called from Gui::loadAssets.
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

    bool customMusic = false;    // true when config.ini names a track of the user's own
    int freq = 44100;
    std::string musicPath;       // the theme's music value, or the custom file name
};
