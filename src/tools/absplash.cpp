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
//
//   --theme DIR   also plays the theme DIR's spinner over the picture (ab_gui G5p2, decision 17 of
//                 autobleem-core's docs/ab-gui-plan.md): the `launcher.spinner` frame strip of DIR's theme.json,
//                 centred at (640, 480) of the 1280x720 picture and scaled with it, or - DIR without a strip (or
//                 not a theme) - ab_gui's ring of dots, the busy spinner's own. No progress bar: RetroArch reports
//                 none. Without --theme the picture is shown alone, as before.
#include <ab_gui/busy.h>
#include <ab_gui/spinner.h>
#include <ab_gui/style.h>
#include <ableem/ableem.h>
#include <ableem/engine/log.h>
#include <ableem/engine/theme_spec.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

using namespace ableem;

namespace {

// where the spinner sits in the 1280x720 picture (the designer's empty spot), and the picture's logical width
constexpr int SpinnerX = 640;
constexpr int SpinnerY = 480;
constexpr int PictureWidth = 1280;

bool fileExists(const std::string &path) {
    struct stat st;
    return stat(path.c_str(), &st) == 0;
}

int usage() {
    PLOG_ERROR << "usage: absplash IMAGE (--until-exists FILE | --until-gone FILE | --seconds S) [--timeout S] "
                  "[--theme DIR]";
    return 2;
}

// the spinner strip of the theme in `dir` (its own theme.json only, as the launcher reads it); none = the ring
abgui::SpinnerSpec themeSpinner(const std::string &dir) {
    abgui::SpinnerSpec spec;
    ThemeSpinner s;
    if (loadThemeSpinner(dir, s)) {
        spec.file = s.image;
        spec.file2x = s.image2x;
        spec.frames = s.frames;
        spec.fps = s.fps;
    }
    return spec;
}

// one frame of the spinner centred on (cx, cy): the strip's current frame at `k` times its logical size, else the ring
void drawSpinner(Renderer &r, abgui::SpinnerStrip &strip, unsigned int nowMs, unsigned int elapsedMs, int cx, int cy,
                 double k) {
    const abgui::SpinnerAnim anim = strip.anim(r);
    if (anim.valid()) {
        const int index = abgui::spinnerFrameIndex(elapsedMs, anim.fps, anim.frames);
        const Rect src = abgui::spinnerFrameRect(anim.strip.size(), anim.frames, index);
        Size frame;
        frame.w = static_cast<int>(std::lround(src.w * k));
        frame.h = static_cast<int>(std::lround(src.h * k));
        const Rect dst = abgui::spinnerDestRect(frame, cx, cy);
        r.setBlendMode(BlendMode::Blend);
        r.copy(anim.strip, &src, &dst);
        return;
    }
    const int radius = static_cast<int>(std::lround(abgui::Busy::SpinnerRadius * k));
    const int dot = std::max(2, static_cast<int>(std::lround(abgui::Busy::SpinnerDot * k)));
    abgui::Style().spinner(r, cx, cy, radius, dot, abgui::Busy::spinnerLead(nowMs));
}

} // namespace

int main(int argc, char **argv) {
    std::atexit(Platform::shutdownSDL);
    Log::initConsoleOnly();
    if (argc < 2)
        return usage();
    std::string image = argv[1];
    std::string untilExists, untilGone, themeDir;
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
        else if (opt == "--theme")
            themeDir = val;
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

    // the theme's spinner, when a theme was named: its strip, or (none) the ring of dots
    const bool spin = !themeDir.empty();
    abgui::SpinnerStrip strip;
    if (spin)
        strip.assign(themeSpinner(themeDir));

    const unsigned int startedMs = gui.platform().ticks();
    const double started = static_cast<double>(startedMs) / 1000.0;
    for (;;) {
        Event e;
        while (gui.input().poll(e)) {
        }
        r.setDrawColor(Color(0, 0, 0, 255));
        r.clear();
        // the picture's box on the canvas: scaled to fit, centred - the pictures are 1280x720 like the screen, so
        // this is a plain copy (no picture: the canvas itself stands in for it, the spinner over black)
        Rect dst(0, 0, r.width(), r.height());
        if (tex.valid()) {
            Size s = tex.size();
            double scale = std::min(static_cast<double>(r.width()) / s.w, static_cast<double>(r.height()) / s.h);
            int w = static_cast<int>(s.w * scale), h = static_cast<int>(s.h * scale);
            dst = Rect((r.width() - w) / 2, (r.height() - h) / 2, w, h);
            r.copy(tex, nullptr, &dst);
        }
        if (spin) {
            // the spinner scales with the picture: 64x64 logical at 1x, the same share of it at any size
            const double k = static_cast<double>(dst.w) / PictureWidth;
            const int cx = dst.x + static_cast<int>(std::lround(SpinnerX * k));
            const int cy = dst.y + static_cast<int>(std::lround(SpinnerY * k));
            const unsigned int nowMs = gui.platform().ticks();
            drawSpinner(r, strip, nowMs, nowMs - startedMs, cx, cy, k);
        }
        r.present();

        const double elapsed = static_cast<double>(gui.platform().ticks()) / 1000.0 - started;
        if (elapsed >= timeout)
            break;
        if (!untilExists.empty() && fileExists(untilExists))
            break;
        if (!untilGone.empty() && !fileExists(untilGone))
            break;
        usleep((spin ? 16 : 100) * 1000); // a playing spinner wants frames; a still picture does not
    }
    return 0;
}
