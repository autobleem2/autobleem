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
//   --anim sweep  a light streak runs along the cyan/magenta rule under the logo of the AutoBleem 2 picture
//                 (the ab2.0.0 palette: cyan 00E5FF at the left, magenta at the right), one frame every 50 ms -
//                 the boot and wake pictures use it so the screen is seen to be alive while the launcher loads.
//   --anim-at X0,Y,X1,H   where the rule is, in thousandths of the picture (the defaults fit autobleem.jpg:
//                 207,513,785,13 = left end, vertical centre, right end, thickness)
#include <ab_gui/busy.h>
#include <ab_gui/spinner.h>
#include <ab_gui/style.h>
#include <ableem/ableem.h>
#include <ableem/engine/log.h>
#include <ableem/engine/theme_spec.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
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
constexpr int PictureHeight = 720;

bool fileExists(const std::string &path) {
    struct stat st;
    return stat(path.c_str(), &st) == 0;
}

// "<dir>/name.jpg" -> "<dir>/name-4x3.jpg"
std::string crtTwin(const std::string &image) {
    const size_t dot = image.find_last_of('.');
    const size_t slash = image.find_last_of('/');
    if (dot == std::string::npos || (slash != std::string::npos && dot < slash))
        return image + "-4x3";
    return image.substr(0, dot) + "-4x3" + image.substr(dot);
}

int usage() {
    PLOG_ERROR << "usage: absplash IMAGE (--until-exists FILE | --until-gone FILE | --seconds S) [--timeout S] "
                  "[--theme DIR] [--anim sweep] [--anim-at X0,Y,X1,H]";
    return 2;
}

// the rule under the logo in the picture, in thousandths of its width/height (autobleem.jpg, 1920x1080:
// x 398..1507, y 554, 14 thick)
struct AnimRule {
    int x0 = 207, y = 513, x1 = 785, h = 13;
    // the 4:3 picture (autobleem-4x3.jpg, the CRT mode's): the same rule, in a picture cut to the 4:3 centre
    static AnimRule forPicture(int w, int h) {
        AnimRule rule;
        if (w * 3 <= h * 4) { // 4:3 or narrower
            rule.x0 = 110;
            rule.x1 = 880;
        }
        return rule;
    }
};

// one frame of the sweep: a streak whose bright head runs left to right along the rule and fades behind it, the
// colour going from cyan to magenta with the position (the rule's own ends); about 1.8 s a pass, eased, a short
// rest before the next. Drawn as 28 alpha steps of one rect each - no texture.
void drawSweep(Renderer &r, const Rect &pic, const AnimRule &rule, unsigned int elapsedMs) {
    constexpr unsigned int Period = 2100, Pass = 1700;
    constexpr int Slices = 28;
    constexpr double TailShare = 0.26; // the tail's length as a share of the rule
    const double x0 = pic.x + pic.w * rule.x0 / 1000.0;
    const double x1 = pic.x + pic.w * rule.x1 / 1000.0;
    const double len = x1 - x0;
    const int h = std::max(2, static_cast<int>(std::lround(pic.h * rule.h / 1000.0)));
    const int cy = pic.y + static_cast<int>(std::lround(pic.h * rule.y / 1000.0));
    const unsigned int t = elapsedMs % Period;
    if (t >= Pass)
        return;
    double u = static_cast<double>(t) / Pass;
    u = u * u * (3.0 - 2.0 * u); // smoothstep: leaves and arrives gently
    const double tail = len * TailShare;
    const double head = x0 + (len + tail) * u; // the head runs on past the end so the tail leaves the rule
    r.setBlendMode(BlendMode::Blend);
    const double step = tail / Slices;
    for (int i = 0; i < Slices; ++i) {
        const double sx = head - (i + 1) * step;
        const double ex = sx + step;
        if (ex <= x0 || sx >= x1)
            continue;
        const double a = 1.0 - static_cast<double>(i) / Slices; // 1 at the head, 0 at the tail's end
        const double pos = std::min(1.0, std::max(0.0, (sx + step / 2 - x0) / len));
        const uint8_t red = static_cast<uint8_t>(std::lround(pos * 255));
        const uint8_t green = static_cast<uint8_t>(std::lround(229 - pos * (229 - 64)));
        const uint8_t blue = static_cast<uint8_t>(std::lround(255 - pos * (255 - 160)));
        r.setDrawColor(Color(red, green, blue, static_cast<uint8_t>(std::lround(a * a * 230))));
        const int rx = static_cast<int>(std::floor(std::max(sx, x0)));
        const int rw = static_cast<int>(std::ceil(std::min(ex, x1))) - rx;
        if (rw > 0)
            r.fillRect(Rect(rx, cy - h / 2, rw, h));
    }
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
    std::string untilExists, untilGone, themeDir, anim;
    AnimRule rule;
    bool ruleGiven = false;
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
        else if (opt == "--anim")
            anim = val;
        else if (opt == "--anim-at") {
            if (sscanf(val.c_str(), "%d,%d,%d,%d", &rule.x0, &rule.y, &rule.x1, &rule.h) != 4)
                return usage();
            ruleGiven = true;
        } else
            return usage();
    }
    if (!anim.empty() && anim != "sweep")
        return usage();
    const bool sweep = anim == "sweep";
    if (untilExists.empty() && untilGone.empty() && seconds <= 0)
        return usage();
    if (seconds > 0)
        timeout = seconds;

    GuiBase gui("absplash");
    gui.platform().setPowerOffHandler([]() {}); // the console's front buttons are not ours to act on
    Renderer &r = gui.renderer();
    // the CRT 4:3 mode (a 720x480 window): the picture's 4:3 twin, <name>-4x3.<ext> next to it, when there is one -
    // one place for every splash of the launcher, the emulator and App hand-overs, the update and the power-off
    const Size shown = gui.platform().windowDisplaySize();
    const std::string twin = crtTwin(image);
    if (((shown.w == 720 && shown.h == 480) || (r.width() == 720 && r.height() == 480)) && fileExists(twin))
        image = twin;
    Texture tex = Texture::loadFile(r, image);
    if (!tex.valid()) {
        PLOG_WARNING << "absplash: could not load " << image << " - black it is";
    } else if (!ruleGiven) {
        rule = AnimRule::forPicture(tex.size().w, tex.size().h);
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
            // a 4:3 picture (the CRT mode's retroarch-4x3.jpg): the free spot under the lockup is the centre, 2/3 down;
            // the size follows the picture's height (720 in the 16:9 one)
            const bool fourThree = dst.w * 3 <= dst.h * 4;
            const double k = fourThree ? static_cast<double>(dst.h) / PictureHeight
                                       : static_cast<double>(dst.w) / PictureWidth;
            const int cx = dst.x + (fourThree ? dst.w / 2 : static_cast<int>(std::lround(SpinnerX * k)));
            const int cy = dst.y + (fourThree ? dst.h * 2 / 3 : static_cast<int>(std::lround(SpinnerY * k)));
            const unsigned int nowMs = gui.platform().ticks();
            drawSpinner(r, strip, nowMs, nowMs - startedMs, cx, cy, k);
        }
        if (sweep)
            drawSweep(r, dst, rule, gui.platform().ticks() - startedMs);
        r.present();

        const double elapsed = static_cast<double>(gui.platform().ticks()) / 1000.0 - started;
        if (elapsed >= timeout)
            break;
        if (!untilExists.empty() && fileExists(untilExists))
            break;
        if (!untilGone.empty() && !fileExists(untilGone))
            break;
        // a playing spinner wants frames, a sweep a frame every 50 ms (the CPU stays low); a still picture does not
        usleep((spin ? 16 : sweep ? 50 : 100) * 1000);
    }
    return 0;
}
