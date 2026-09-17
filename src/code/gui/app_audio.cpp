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

    const ableem::ThemeMusic &themeMusic = theme_.music();
    if (themeMusic.none) return;   // a silent theme stays silent, the user's own track included

    if (!customMusic) {
        music = ableem::Music::load(themeMusic.file);
        music.play(themeMusic.loop ? -1 : 0);
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
    if (reloadMusic) {
        freeMusic();
    }

    customMusic = false;
    freq = 32000;
    musicPath = theme_.music().file;
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

    const ableem::ThemeSounds &sounds = theme_.sounds();
    cursor = ableem::Sound::load(sounds.cursor);
    cancel = ableem::Sound::load(sounds.cancel);
    home_up = ableem::Sound::load(sounds.homeUp);
    home_down = ableem::Sound::load(sounds.homeDown);
    resume = ableem::Sound::load(sounds.resume);
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
