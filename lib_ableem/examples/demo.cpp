// Standalone smoke test for lib_ableem: one screen with a texture, some text, and a sound effect on Cross.
// Exits on Circle, Esc, or window-close. Does not link or include anything from the AutoBleem app.
//
// usage: ableem_demo [image.png] [font.ttf] [sound.wav]
// any argument may be omitted; the demo degrades gracefully (a color fill instead of a texture, no text,
// no sound) so it still proves the library boots and runs an event loop even with zero assets on hand.
#include <ableem/ableem.h>
#include <iostream>
#include <cstdlib>
#include <ableem/engine/log.h>

using namespace ableem;

class DemoScreen : public GuiScreen {
public:
    DemoScreen(GuiBase &g, Texture tex, Font font, Sound sound)
        : GuiScreen(g), texture(tex), font(font), sound(sound) {}

    void render() override {
        Renderer &r = gui.renderer();
        r.setDrawColor(Color(20, 30, 60, 255));
        r.clear();

        if (texture.valid()) {
            Size s = texture.size();
            Rect dst(gui.renderer().width() / 2 - s.w / 2, gui.renderer().height() / 2 - s.h / 2, s.w, s.h);
            r.copy(texture, nullptr, &dst);
        } else {
            r.setDrawColor(Color(200, 60, 60, 255));
            r.fillRect(Rect(gui.renderer().width() / 2 - 100, gui.renderer().height() / 2 - 60, 200, 120));
        }

        if (font.valid()) {
            font.drawColor(r, 40, 40, Color(255, 255, 255, 255), "ableem_demo - Cross: sound, Circle/Esc: quit");
        }

        r.present();
    }

    void doCross_Pressed() override {
        PLOG_INFO << "Cross pressed";
        sound.play();
    }

    void doCircle_Pressed() override {
        PLOG_INFO << "Circle pressed, exiting";
        menuVisible = false;
    }

private:
    Texture texture;
    Font font;
    Sound sound;
};

int main(int argc, char **argv) {
    std::atexit(Platform::shutdownSDL);
    ableem::Log::initConsoleOnly();
    GuiBase gui("ableem_demo", 800, 480);
    gui.platform().setPowerOffHandler([&]() {
        PLOG_INFO << "power-off / escape requested, exiting";
        std::exit(0);
    });
    gui.audio().open();

    Texture tex = argc > 1 ? Texture::loadFile(gui.renderer(), argv[1]) : Texture();
    Font font = argc > 2 ? Font::load(gui.renderer(), argv[2], 20) : Font();
    Sound sound = argc > 3 ? Sound::load(argv[3]) : Sound();

    PLOG_INFO << gui.platform().versionString();

    DemoScreen screen(gui, tex, font, sound);
    screen.show();

    return 0;
}
