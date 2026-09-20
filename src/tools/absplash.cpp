// absplash - a full-screen picture while something else starts or stops.
//
//   absplash IMAGE --until-exists FILE   shows IMAGE until FILE appears (or --timeout S, default 30)
//   absplash IMAGE --until-gone FILE     shows IMAGE until FILE disappears
//   absplash IMAGE --seconds S           shows IMAGE for S seconds
//
// The console's launch scripts show it between the launcher and RetroArch: the launcher's window is gone
// during a game (it gives the display up), and RetroArch takes a few seconds to come up and a moment to go,
// which used to be black. RetroBoot's own rbimage/abimage did this with a plain toplevel window, which the
// console's Weston no longer shows in our setup; this one uses the same full-screen window the launcher
// and RetroArch use, over lib_ableem, and is killed (or times out) rather than asked to close - a SIGTERM
// ends it the same as the condition. Draws the picture scaled to the screen, letterboxed on black, and
// polls events so the compositor stays happy.
#include <ableem/ableem.h>
#include <ableem/engine/log.h>

#include <algorithm>
#include <cstdlib>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

using namespace ableem;

namespace {

bool fileExists(const std::string &path) {
    struct stat st;
    return stat(path.c_str(), &st) == 0;
}

int usage() {
    PLOG_ERROR << "usage: absplash IMAGE (--until-exists FILE | --until-gone FILE | --seconds S) [--timeout S]";
    return 2;
}

} // namespace

int main(int argc, char **argv) {
    std::atexit(Platform::shutdownSDL);
    Log::initConsoleOnly();
    if (argc < 2)
        return usage();
    std::string image = argv[1];
    std::string untilExists, untilGone;
    double seconds = 0, timeout = 30;
    for (int i = 2; i + 1 < argc; i += 2) {
        std::string opt = argv[i], val = argv[i + 1];
        if (opt == "--until-exists")
            untilExists = val;
        else if (opt == "--until-gone")
            untilGone = val;
        else if (opt == "--seconds")
            seconds = atof(val.c_str());
        else if (opt == "--timeout")
            timeout = atof(val.c_str());
        else
            return usage();
    }
    if (untilExists.empty() && untilGone.empty() && seconds <= 0)
        return usage();
    if (seconds > 0)
        timeout = seconds;

    GuiBase gui("absplash");
    gui.platform().setPowerOffHandler([]() {}); // the console's front buttons are not ours to act on
    Renderer &r = gui.renderer();
    Texture tex = Texture::loadFile(r, image);
    if (!tex.valid()) {
        PLOG_WARNING << "absplash: could not load " << image << " - black it is";
    }

    const double started = static_cast<double>(gui.platform().ticks()) / 1000.0;
    for (;;) {
        Event e;
        while (gui.input().poll(e)) {
        }
        r.setDrawColor(Color(0, 0, 0, 255));
        r.clear();
        if (tex.valid()) {
            // scaled to fit, centred - the pictures are 1280x720 like the screen, so this is a plain copy
            Size s = tex.size();
            double scale = std::min(static_cast<double>(r.width()) / s.w, static_cast<double>(r.height()) / s.h);
            int w = static_cast<int>(s.w * scale), h = static_cast<int>(s.h * scale);
            Rect dst((r.width() - w) / 2, (r.height() - h) / 2, w, h);
            r.copy(tex, nullptr, &dst);
        }
        r.present();

        const double elapsed = static_cast<double>(gui.platform().ticks()) / 1000.0 - started;
        if (elapsed >= timeout)
            break;
        if (!untilExists.empty() && fileExists(untilExists))
            break;
        if (!untilGone.empty() && !fileExists(untilGone))
            break;
        usleep(100 * 1000);
    }
    return 0;
}
