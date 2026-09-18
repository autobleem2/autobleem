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
// AppAudio::wantedMusicState
//*******************************
AppAudio::MusicState AppAudio::wantedMusicState() const {
    MusicState next;
    const ableem::ThemeMusic &themeMusic = theme_.music();
    next.custom = config_.inifile.values["music"] != "--";
    next.path = next.custom ? config_.inifile.values["music"] : themeMusic.file;
    next.freq = DirEntry::getFileExtension(next.path) == "ogg" ? 44100 : 32000;
    next.enabled = config_.inifile.values["nomusic"] != "true" && !themeMusic.none;
    next.loops = (next.custom || themeMusic.loop) ? -1 : 0;
    return next;
}

//*******************************
// AppAudio::loadTheme
//*******************************
void AppAudio::loadTheme(bool reloadMusic) {
    MusicState next = wantedMusicState();
    customMusic = next.custom;
    freq = next.freq;
    musicPath = next.path;

    if (reloadMusic && !(next == playing_)) {
        freeMusic();
        restart();
        playMusic();
        playing_ = next;
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
    playing_ = MusicState();
    cursor = ableem::Sound();
    cancel = ableem::Sound();
    home_down = ableem::Sound();
    home_up = ableem::Sound();
    resume = ableem::Sound();
    device.close();
}
