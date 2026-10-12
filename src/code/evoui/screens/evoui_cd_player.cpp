//
// GuiCdPlayer: the Audio CD player screen. See the header.
//
#include "evoui_cd_player.h"
#include "core/services/environment.h"
#include "gui/gui.h"

#include <ab_gui/panel.h>

#include <ableem/engine/log.h>
#include <ableem/ui/debug_driver.h>

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <typeinfo>

using namespace std;

namespace {
// the design is the 1280x720 mockup; everything is placed in those numbers and scaled to the window
const int DesignW = 1280;
const int DesignH = 720;
const int KeySize = 56;
const int KeyGap = 14;
const int GridKeyH = 52;
const int GridGap = 8;
const int GridVisibleRows = 2;
const int SegmentCount = 16;
const unsigned FramesPerPoll = 33; // the bars are worked out about 30 times a second

enum Glyph { GlyphPrev, GlyphPlay, GlyphPause, GlyphStop, GlyphNext, GlyphRepeat, GlyphShuffle };

// a right-pointing triangle filling `box`, row by row
void fillTriangle(ableem::Renderer &r, const ableem::Rect &box, bool pointsRight) {
    for (int row = 0; row < box.h; row++) {
        const int half = box.h / 2;
        const int dist = row < half ? row : box.h - 1 - row;
        const int w = max(1, box.w * (dist + 1) / (half + 1));
        r.fillRect(ableem::Rect(pointsRight ? box.x : box.x + box.w - w, box.y + row, w, 1));
    }
}

void drawGlyph(ableem::Renderer &r, Glyph g, const ableem::Rect &key, const ableem::Color &c) {
    r.setBlendMode(ableem::BlendMode::Blend);
    r.setDrawColor(c);
    const int s = key.w * 5 / 14;
    const int cx = key.x + key.w / 2, cy = key.y + key.h / 2;
    const int bar = max(2, s / 4);
    switch (g) {
    case GlyphPlay:
        fillTriangle(r, ableem::Rect(cx - s / 3, cy - s / 2, s * 3 / 4, s), true);
        break;
    case GlyphPause:
        r.fillRect(ableem::Rect(cx - s / 2, cy - s / 2, s / 3, s));
        r.fillRect(ableem::Rect(cx + s / 2 - s / 3, cy - s / 2, s / 3, s));
        break;
    case GlyphStop:
        r.fillRect(ableem::Rect(cx - s / 2, cy - s / 2, s, s));
        break;
    case GlyphPrev:
        r.fillRect(ableem::Rect(cx - s / 2, cy - s / 2, bar, s));
        fillTriangle(r, ableem::Rect(cx - s / 2 + bar + 1, cy - s / 2, s - bar, s), false);
        break;
    case GlyphNext:
        r.fillRect(ableem::Rect(cx + s / 2 - bar, cy - s / 2, bar, s));
        fillTriangle(r, ableem::Rect(cx - s / 2, cy - s / 2, s - bar, s), true);
        break;
    case GlyphRepeat: { // a loop: a ring with an arrow head at its top right
        const int t = max(2, s / 6);
        r.fillRect(ableem::Rect(cx - s / 2, cy - s / 3, s, t));
        r.fillRect(ableem::Rect(cx - s / 2, cy + s / 3 - t, s, t));
        r.fillRect(ableem::Rect(cx - s / 2, cy - s / 3, t, s * 2 / 3));
        r.fillRect(ableem::Rect(cx + s / 2 - t, cy - s / 3, t, s * 2 / 3));
        fillTriangle(r, ableem::Rect(cx + s / 2 - t, cy - s / 2 - t, s / 3, s / 3), true);
        break;
    }
    case GlyphShuffle: { // two lines crossing, an arrow head at each right end
        const int t = max(2, s / 6);
        for (int i = 0; i < s; i++) {
            const int y1 = cy - s / 3 + (s * 2 / 3) * i / s;
            const int y2 = cy + s / 3 - (s * 2 / 3) * i / s;
            r.fillRect(ableem::Rect(cx - s / 2 + i, y1, t, t));
            r.fillRect(ableem::Rect(cx - s / 2 + i, y2, t, t));
        }
        fillTriangle(r, ableem::Rect(cx + s / 2 - t, cy - s / 3 - t, s / 3, s / 3), true);
        fillTriangle(r, ableem::Rect(cx + s / 2 - t, cy + s / 3 - s / 3 + t, s / 3, s / 3), true);
        break;
    }
    }
}
} // namespace

//*******************************
// GuiCdPlayer::init
//*******************************
void GuiCdPlayer::init() {
    style = gui->panelStyle();
    ready = false;
    openDrive();
    if (drive == nullptr)
        return;
    player = make_unique<CddaPlayer>(*drive, toc);
    numbers.clear();
    for (const CdTrack &t : toc.tracks)
        if (t.audio)
            numbers.push_back(t.number);
    if (numbers.empty()) {
        PLOG_ERROR << "CD player: the disc has no audio track";
        return;
    }
    focus = 0;
    firstRow = 0;
    loadCase();
    spectrum.setDecay(0.1f);
    ready = true;
}

void GuiCdPlayer::openDrive() {
    if (drive == nullptr && !cuePath.empty()) {
        image = make_unique<ImageCdDrive>();
        string error;
        if (!image->open(cuePath, error)) {
            PLOG_ERROR << "CD player: " << cuePath << ": " << error;
            image.reset();
            return;
        }
        drive = image.get();
        toc = image->toc();
    }
}

// the theme's jewel case (launcher.audioCdCase, images/audio_cd_case.png next to its other launcher images), else the
// default theme's evoimg/audio_cd_case.png
void GuiCdPlayer::loadCase() {
    const string background = app.theme().launcher().background;
    const size_t slash = background.find_last_of("/\\");
    if (slash != string::npos) {
        caseImage = ThemeAssets::loadImage(renderer, background.substr(0, slash + 1) + "audio_cd_case.png");
    }
    if (!caseImage.valid())
        caseImage =
            ThemeAssets::loadImage(renderer, Env::getWorkingPath() + sep + "evoimg" + sep + "audio_cd_case.png");
}

string GuiCdPlayer::titleOf(int number) const {
    if (number >= 1 && static_cast<size_t>(number) <= trackTitles.size() && !trackTitles[number - 1].empty())
        return trackTitles[number - 1];
    return _("Track") + " " + cdplayer::twoDigits(number);
}

int GuiCdPlayer::playingIndex() const {
    if (!player || player->state() == CddaPlayer::State::Stopped)
        return -1;
    const int n = player->track();
    for (size_t i = 0; i < numbers.size(); i++)
        if (numbers[i] == n)
            return static_cast<int>(i);
    return -1;
}

int GuiCdPlayer::gridRows() const {
    return (static_cast<int>(numbers.size()) + cdplayer::GridColumns - 1) / cdplayer::GridColumns;
}

//*******************************
// the actions
//*******************************
void GuiCdPlayer::playFocused() {
    if (!ready)
        return;
    player->play(numbers[static_cast<size_t>(focus)]);
}

void GuiCdPlayer::togglePause() {
    if (!ready)
        return;
    switch (player->state()) {
    case CddaPlayer::State::Playing:
        player->pause();
        break;
    case CddaPlayer::State::Paused:
        player->resume();
        break;
    case CddaPlayer::State::Stopped:
        playFocused();
        break;
    }
}

void GuiCdPlayer::goTo(bool forward) {
    if (!ready)
        return;
    if (player->state() == CddaPlayer::State::Stopped) {
        focus = cdplayer::moveFocus(focus, forward ? 1 : -1, 0, static_cast<int>(numbers.size()));
        playFocused();
        return;
    }
    if (forward ? player->next() : player->prev()) {
        const int i = playingIndex();
        if (i >= 0)
            focus = i;
    }
}

void GuiCdPlayer::cycleRepeat() {
    if (!ready)
        return;
    cdplayer::RepeatMode now = cdplayer::RepeatMode::Off;
    if (player->repeat() == CddaPlayer::Repeat::Track)
        now = cdplayer::RepeatMode::Track;
    else if (player->repeat() == CddaPlayer::Repeat::Disc)
        now = cdplayer::RepeatMode::Disc;
    switch (cdplayer::nextRepeat(now)) {
    case cdplayer::RepeatMode::Off:
        player->setRepeat(CddaPlayer::Repeat::Off);
        break;
    case cdplayer::RepeatMode::Track:
        player->setRepeat(CddaPlayer::Repeat::Track);
        break;
    case cdplayer::RepeatMode::Disc:
        player->setRepeat(CddaPlayer::Repeat::Disc);
        break;
    }
}

void GuiCdPlayer::toggleShuffle() {
    if (ready)
        player->setShuffle(!player->shuffle());
}

// Cross on a transport key: previous, play/pause, stop, next, repeat, shuffle
void GuiCdPlayer::pressTransportKey(int key) {
    if (!ready)
        return;
    switch (key) {
    case 0:
        goTo(false);
        break;
    case 1:
        togglePause();
        break;
    case 2:
        player->stop();
        break;
    case 3:
        goTo(true);
        break;
    case 4:
        cycleRepeat();
        break;
    case 5:
        toggleShuffle();
        break;
    default:
        break;
    }
}

void GuiCdPlayer::seekBy(int direction) {
    if (ready && player->state() != CddaPlayer::State::Stopped)
        player->seek(cdplayer::seekTarget(player->position(), direction));
}

//*******************************
// GuiCdPlayer::publishItems
//*******************************
// the DebugDriver's `items`/`selected`: the tracks and the focused one
void GuiCdPlayer::publishItems() const {
    if (!menuVisible || !ableem::DebugDriver::active())
        return;
    vector<string> names;
    for (int n : numbers)
        names.push_back(cdplayer::twoDigits(n) + " " + titleOf(n));
    ableem::DebugDriver::publish(typeid(*this).name(), names, numbers.empty() ? -1 : focus);
}

//*******************************
// GuiCdPlayer::drawTransportKey
//*******************************
void GuiCdPlayer::drawTransportKey(int kind, const ableem::Rect &box, bool lit, bool focused,
                                   const ableem::Color &color) {
    style.key(gui->uiContext(), box,
              focused ? abgui::KeyState::Selected : (lit ? abgui::KeyState::Lit : abgui::KeyState::Normal));
    drawGlyph(renderer, static_cast<Glyph>(kind), box, lit && !focused ? color : style.rowSelected);
}

//*******************************
// GuiCdPlayer::draw
//*******************************
static vector<abgui::HintItem> footerHints() {
    return {{{"X"}, _("Play track")}, {{"S"}, _("Stop")},   {{"L1/R1"}, _("Previous/next")}, {{"O"}, _("Back")},
            {{"L2/R2"}, _("Seek")},   {{"T"}, _("Repeat")}, {{"Select"}, _("Shuffle")},      {{"Start"}, _("Pause")}};
}

void GuiCdPlayer::draw() {
    publishItems();
    gui->renderBackground();
    style.dim(gui->uiContext());

    const double scale = renderer.height() / static_cast<double>(DesignH);
    const int ox = static_cast<int>((renderer.width() - DesignW * scale) / 2);
    auto X = [&](int v) { return ox + static_cast<int>(v * scale); };
    auto Y = [&](int v) { return static_cast<int>(v * scale); };
    auto L = [&](int v) { return static_cast<int>(v * scale); };
    auto R = [&](int x, int y, int w, int h) { return ableem::Rect(X(x), Y(y), L(w), L(h)); };

    const ableem::Rect panel = R(30, 24, 1220, 672);
    style.sheet(gui->uiContext(), panel);

    const TextRenderer::Shadow classicShadow = gui->text().shadow();
    TextRenderer::Shadow shadow;
    shadow.enabled = style.textShadow;
    gui->text().setShadow(shadow);
    Fonts &fonts = gui->assets().themeFonts;
    TextRenderer &text = gui->text();

    const LauncherTheme &theme = app.theme().launcher();
    const ableem::Color accent = theme.colors.selection.set ? TextRenderer::toColor(theme.colors.selection, 255)
                                                            : ableem::Color(style.rowSelected);

    text.renderText_WithColor(fonts[FONT_28_BOLD], _("CD PLAYER"), X(60), Y(38), style.rowSelected, XALIGN_LEFT);
    style.rule(gui->uiContext(), panel, Y(86));

    if (!ready) {
        text.renderText_WithColor(fonts[FONT_22_MED], _("This disc cannot be played"), X(60), Y(120), style.description,
                                  XALIGN_LEFT);
        style.footer(*gui, R(30, 642, 1220, 54), footerHints());
        text.setShadow(classicShadow);
        return;
    }

    const CddaPlayer::State state = player->state();
    const int playing = playingIndex();
    const int shownIndex = playing >= 0 ? playing : focus;
    const int shownNumber = numbers[static_cast<size_t>(shownIndex)];

    // the left column: the jewel case, then what the disc is
    const ableem::Rect caseBox = R(60, 110, 200, 200);
    if (caseImage.valid()) {
        const ableem::Size s = caseImage.size();
        const double f =
            min(caseBox.w / static_cast<double>(max(1, s.w)), caseBox.h / static_cast<double>(max(1, s.h)));
        const int w = static_cast<int>(s.w * f), h = static_cast<int>(s.h * f);
        ableem::Rect dst(caseBox.x + (caseBox.w - w) / 2, caseBox.y + (caseBox.h - h) / 2, w, h);
        renderer.setBlendMode(ableem::BlendMode::Blend);
        renderer.copy(caseImage, nullptr, &dst);
    } else {
        style.box(gui->uiContext(), caseBox);
    }
    const int count = static_cast<int>(numbers.size());
    text.renderText_WithColor(fonts[FONT_22_MED], _("Audio CD"), X(60), Y(322), style.rowSelected, XALIGN_LEFT);
    const string facts =
        to_string(count) + " " + (count == 1 ? _("track") : _("tracks")) + " · " + cdplayer::clockText(player->total());
    text.renderText_WithColor(fonts[FONT_15_BOLD], facts, X(60), Y(354), style.description, XALIGN_LEFT);
    if (!album.empty())
        text.renderText_WithColor(fonts[FONT_15_BOLD], text.elide(fonts[FONT_15_BOLD], album, L(200)), X(60), Y(380),
                                  style.description, XALIGN_LEFT);

    // the spectrum panel
    const ableem::Rect specBox = R(290, 110, 930, 200);
    style.box(gui->uiContext(), specBox, abgui::Tone::Black, 90);
    const long now = gui->platform().ticks();
    if (now - lastAnalysis >= static_cast<long>(FramesPerPoll)) {
        lastAnalysis = now;
        vector<int16_t> frames(SpectrumFrames * 2, 0);
        if (state == CddaPlayer::State::Playing)
            player->lastFrames(frames.data(), SpectrumFrames);
        spectrum.analyse(frames.data(), SpectrumFrames);
        for (int i = 0; i < Bars; i++)
            caps[static_cast<size_t>(i)] =
                max(spectrum.bars()[static_cast<size_t>(i)], caps[static_cast<size_t>(i)] - 0.02f);
    }
    {
        const int pad = L(16);
        const int slotW = (specBox.w - 2 * pad) / Bars;
        const int barW = max(2, slotW - L(4));
        const int segGap = max(1, L(2));
        const int segH = max(2, (specBox.h - 2 * pad - segGap * (SegmentCount - 1)) / SegmentCount);
        const int baseY = specBox.y + specBox.h - pad;
        renderer.setBlendMode(ableem::BlendMode::Blend);
        for (int i = 0; i < Bars; i++) {
            const int lit = cdplayer::litSegments(spectrum.bars()[static_cast<size_t>(i)], SegmentCount);
            const int cap = cdplayer::litSegments(caps[static_cast<size_t>(i)], SegmentCount);
            const int x = specBox.x + pad + i * slotW + (slotW - barW) / 2;
            for (int s = 0; s < SegmentCount; s++) {
                const ableem::Rect seg(x, baseY - (s + 1) * segH - s * segGap, barW, segH);
                if (s < lit)
                    renderer.setDrawColor(ableem::Color(accent.r, accent.g, accent.b, 235));
                else if (s == cap - 1)
                    renderer.setDrawColor(ableem::Color(accent.r, accent.g, accent.b, 255));
                else
                    renderer.setDrawColor(ableem::Color(style.edge.r, style.edge.g, style.edge.b, 40));
                renderer.fillRect(seg);
            }
        }
    }

    // the track, its time and the seek bar
    const int rightEdge = X(1220);
    text.renderText_WithColor(fonts[FONT_15_BOLD], _("Track") + " " + cdplayer::twoDigits(shownNumber), X(290), Y(326),
                              accent, XALIGN_LEFT);
    const uint32_t length = player->trackLength(shownNumber);
    const uint32_t position = playing >= 0 ? player->position() : 0;
    const string times = cdplayer::clockText(position) + " / " + cdplayer::clockText(length);
    const int timesW = text.textWidth(fonts[FONT_22_MED], times);
    text.renderText_WithColor(fonts[FONT_22_MED], times, rightEdge - timesW, Y(346), style.rowSelected, XALIGN_LEFT);
    text.renderText_WithColor(
        fonts[FONT_28_BOLD], text.elide(fonts[FONT_28_BOLD], titleOf(shownNumber), rightEdge - timesW - L(30) - X(290)),
        X(290), Y(344), style.rowSelected, XALIGN_LEFT);
    style.progress(gui->uiContext(), R(290, 392, 930, 12), position, max<uint32_t>(length, 1), abgui::Tone::Secondary,
                   abgui::Style::StyleAlpha, abgui::Tone::SelectionBand, 255);

    // the transport keys: previous, play/pause, stop, next, repeat (with its tag), shuffle
    const bool playingNow = state == CddaPlayer::State::Playing;
    const CddaPlayer::Repeat repeat = player->repeat();
    const bool shuffle = player->shuffle();
    struct Key {
        Glyph glyph;
        bool lit;
    } keys[] = {{GlyphPrev, false}, {playingNow ? GlyphPause : GlyphPlay, playingNow}, {GlyphStop, false},
                {GlyphNext, false}, {GlyphRepeat, repeat != CddaPlayer::Repeat::Off},  {GlyphShuffle, shuffle}};
    for (int i = 0; i < 6; i++) {
        const ableem::Rect box = R(290 + i * (KeySize + KeyGap), 418, KeySize, KeySize);
        drawTransportKey(keys[i].glyph, box, keys[i].lit, i == transportFocus, accent);
    }
    if (repeat != CddaPlayer::Repeat::Off) {
        const string tag = repeat == CddaPlayer::Repeat::Track ? _("TRACK") : _("DISC");
        const ableem::Rect box = R(290 + 4 * (KeySize + KeyGap), 418, KeySize, KeySize);
        const int w = text.textWidth(fonts[FONT_15_BOLD], tag);
        text.renderText_WithColor(fonts[FONT_15_BOLD], tag, box.x + (box.w - w) / 2, box.y + box.h + L(1), accent,
                                  XALIGN_LEFT);
    }
    const string repeatName = repeat == CddaPlayer::Repeat::Track  ? _("Track")
                              : repeat == CddaPlayer::Repeat::Disc ? _("Disc")
                                                                   : _("off");
    text.renderText_WithColor(
        fonts[FONT_22_MED],
        _("Repeat:") + " " + repeatName + " · " + _("Shuffle:") + " " + (shuffle ? _("on") : _("off")),
        X(290 + 6 * (KeySize + KeyGap) + 16), Y(418) + (L(KeySize) - fonts[FONT_22_MED].lineHeight()) / 2,
        style.description, XALIGN_LEFT);

    // the track grid: a key per track, the playing one lit, the focused one selected
    const int cols = cdplayer::GridColumns;
    const int gridW = 1160;
    const int keyW = (gridW - (cols - 1) * GridGap) / cols;
    const int visible = GridVisibleRows;
    const int firstVisibleRow = cdplayer::firstRowFor(focus, firstRow, visible, count);
    firstRow = firstVisibleRow;
    for (int row = 0; row < visible; row++) {
        for (int col = 0; col < cols; col++) {
            const int i = (firstVisibleRow + row) * cols + col;
            if (i >= count)
                break;
            const int number = numbers[static_cast<size_t>(i)];
            const ableem::Rect box = R(60 + col * (keyW + GridGap), 486 + row * (GridKeyH + 6), keyW, GridKeyH);
            const bool focused = i == focus && transportFocus < 0;
            const bool isPlaying = i == playing;
            style.key(gui->uiContext(), box,
                      focused ? abgui::KeyState::Selected
                              : (isPlaying ? abgui::KeyState::Lit : abgui::KeyState::Normal));
            const ableem::Color nameColor = isPlaying ? accent : style.rowColor(focused);
            const string name = cdplayer::twoDigits(number);
            const string len = cdplayer::clockText(player->trackLength(number));
            text.renderText_WithColor(fonts[FONT_20_BOLD], name,
                                      box.x + (box.w - text.textWidth(fonts[FONT_20_BOLD], name)) / 2, box.y + L(4),
                                      nameColor, XALIGN_LEFT);
            text.renderText_WithColor(
                fonts[FONT_15_BOLD], len, box.x + (box.w - text.textWidth(fonts[FONT_15_BOLD], len)) / 2,
                box.y + L(4) + fonts[FONT_20_BOLD].lineHeight(), style.valueColor(focused), XALIGN_LEFT);
        }
    }
    const int focusedNumber = numbers[static_cast<size_t>(focus)];
    text.renderText_WithColor(fonts[FONT_15_BOLD],
                              text.elide(fonts[FONT_15_BOLD],
                                         _("Selected:") + " " + cdplayer::twoDigits(focusedNumber) + " " +
                                             titleOf(focusedNumber) + " · " +
                                             cdplayer::clockText(player->trackLength(focusedNumber)),
                                         X(1220) - X(60)),
                              X(60), Y(604), style.description, XALIGN_LEFT);

    style.footer(*gui, R(30, 642, 1220, 54), footerHints());
    text.setShadow(classicShadow);
}

//*******************************
// GuiCdPlayer::loop
//*******************************
void GuiCdPlayer::loop() {
    menuVisible = true;
    gui->input().setFrameNeed(ableem::Input::FrameNeed::Ambient); // the bars move: about 30 frames a second
    if (ready) {
        // the stream takes the music slot: the theme's music stops first, and comes back on leaving
        app.audio().music.halt();
        musicWasStopped = true;
        CddaPlayer *p = player.get();
        if (!stream.start([p](int16_t *out, size_t frames) { return p->read(out, frames); }))
            PLOG_ERROR << "CD player: the sound device cannot play the disc";
        playFocused();
    }
    while (menuVisible) {
        Event e;
        while (gui->input().poll(e)) {
            if (e.type == Event::Type::Quit) {
                menuVisible = false;
                continue;
            }
            if (e.type == Event::Type::DpadDown) {
                int dx = 0, dy = 0;
                if (gui->input().dpadRight())
                    dx = 1;
                else if (gui->input().dpadLeft())
                    dx = -1;
                else if (gui->input().dpadDown())
                    dy = 1;
                else if (gui->input().dpadUp())
                    dy = -1;
                const int before = focus;
                const int keyBefore = transportFocus;
                if (transportFocus >= 0) { // the transport row: left/right between the keys, down into the grid
                    if (dy > 0)
                        transportFocus = -1;
                    else
                        transportFocus = max(0, min(TransportKeys - 1, transportFocus + dx));
                } else if (dy < 0 && focus < cdplayer::GridColumns) {
                    transportFocus = min(focus * TransportKeys / cdplayer::GridColumns, TransportKeys - 1);
                } else {
                    focus = cdplayer::moveFocus(focus, dx, dy, static_cast<int>(numbers.size()));
                }
                if (focus != before || transportFocus != keyBefore)
                    app.audio().cursor.play();
                publishItems();
            } else if (e.type == Event::Type::ButtonDown) {
                switch (e.button) {
                case Button::Cross:
                    app.audio().cursor.play();
                    if (transportFocus >= 0) {
                        pressTransportKey(transportFocus);
                        break;
                    }
                    if (ready && playingIndex() == focus && player->state() == CddaPlayer::State::Paused)
                        player->resume();
                    else
                        playFocused();
                    break;
                case Button::Square:
                    if (ready)
                        player->stop();
                    break;
                case Button::Start:
                    togglePause();
                    break;
                case Button::Triangle:
                    cycleRepeat();
                    break;
                case Button::Select:
                    toggleShuffle();
                    break;
                case Button::L1:
                    goTo(false);
                    break;
                case Button::R1:
                    goTo(true);
                    break;
                case Button::L2:
                    seekBy(-1);
                    break;
                case Button::R2:
                    seekBy(1);
                    break;
                case Button::Circle:
                    app.audio().cancel.play();
                    menuVisible = false;
                    break;
                default:
                    break;
                }
                publishItems();
            } else if (e.type == Event::Type::KeyDown && e.key == Key::Escape) {
                app.audio().cancel.play();
                menuVisible = false;
            }
        }
        if (gui->input().frameDue())
            render();
    }
    stream.stop();
    if (player)
        player->stop();
    if (musicWasStopped) {
        app.audio().playMusic(); // the theme's music, as it was
        musicWasStopped = false;
    }
}

//*******************************
// runCdPlayerDevEntry
//*******************************
void runCdPlayerDevEntry(ableem::GuiBase &gui) {
    static bool done = false;
    const char *env = getenv("AB_CD_PLAYER_IMAGE");
    string cue = env != nullptr ? env : "";
    if (cue.empty()) { // the VM cannot set the environment: an image laid in as the extension "cdplayer-test" does it
        const string laid = Env::getPathToUSBRoot() + sep + "Extensions" + sep + "cdplayer-test" + sep + "testcd.cue";
        if (ifstream(laid).good())
            cue = laid;
    }
    if (done || cue.empty())
        return;
    done = true;
    PLOG_INFO << "CD player: dev entry on " << cue;
    GuiCdPlayer screen(gui);
    screen.cuePath = cue;
    screen.show();
}
