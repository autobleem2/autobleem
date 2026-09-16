//
// AppAudio: AutoBleem's music track and UI sound effects.
//

#include "app_audio.h"
#include "../core/services/environment.h"
#include "../core/main.h"
#include "../core/model/timing.h"   // TicksPerSecond

#include <unistd.h>

using namespace std;

//*******************************
// AppAudio::playMusic
//*******************************
void AppAudio::playMusic() {
    if (config_.inifile.values["nomusic"] == "true") return;

    Theme &theme = theme_;
    if (theme.data.values["loop"] == "-1") return;

    if (!customMusic) {
        music = ableem::Music::load(theme.loadedPath() + theme.data.values["music"]);
        music.play(theme.data.values["loop"] == "1" ? -1 : 0);
    } else {
        music = ableem::Music::load(Env::getWorkingPath() + sep + "music/" + musicPath);
        music.play(-1);
    }
}

//*******************************
// AppAudio::freeMusic
//*******************************
void AppAudio::freeMusic() {
    music = ableem::Music();
}

//*******************************
// AppAudio::loadTheme
//*******************************
void AppAudio::loadTheme(bool reloadMusic) {
    Theme &theme = theme_;

    if (reloadMusic) {
        freeMusic();
    }

    customMusic = false;
    freq = 32000;
    musicPath = theme.data.values["music"];
    if (config_.inifile.values["music"] != "--") {
        customMusic = true;
        musicPath = config_.inifile.values["music"];
    }

    if (DirEntry::getFileExtension(musicPath) == "ogg") {
        freq = 44100;
    }

    if (reloadMusic) {
        restart();
        playMusic();
    }

    string sounds = theme.soundPath() + sep;
    cursor = ableem::Sound::load(sounds + "cursor.wav");
    cancel = ableem::Sound::load(sounds + "cancel.wav");
    home_up = ableem::Sound::load(sounds + "home_up.wav");
    home_down = ableem::Sound::load(sounds + "home_down.wav");
    resume = ableem::Sound::load(sounds + "resume_new.wav");
}

//*******************************
// AppAudio::shutdown
//*******************************
void AppAudio::shutdown() {
    if (music.isPlaying()) {
        music.fadeOut(300);
        while (music.isPlaying()) {
        }
    } else {
        usleep(300 * TicksPerSecond);
    }

    music.halt();
    music = ableem::Music();
    cursor = ableem::Sound();
    cancel = ableem::Sound();
    home_down = ableem::Sound();
    home_up = ableem::Sound();
    resume = ableem::Sound();
    device.close();
}
