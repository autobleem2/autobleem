//
// GuiSetPicker: the launcher's "which games" screen. See the header.
//
#include "evoui_set_picker.h"
#include "../../core/services/environment.h"
#include "../../gui/gui.h"

#include <algorithm>

using namespace std;

namespace {
const int PanelWidth = 800;
const int PanelMargin = PanelStyle::Margin;
const int TabsHeight = 112; // the tab strip at the top, in the header's place: the icon, the label, the rule
const int FooterHeight = PanelStyle::FooterHeight;
const int RowHeight = 44;
const int RowInset = PanelStyle::RowInset;
const int TabWidth = 150;
const int IconSize = 56; // the tab icons (evoimg/tab_*.png, tools/make_evoimg_icons.py)
} // namespace

//*******************************
// GuiSetPicker::init
//*******************************
void GuiSetPicker::init() {
    style = gui->panelStyle();
    const string img = Env::getWorkingPath() + sep + "evoimg" + sep;
    tabs.clear();
    tabs.push_back({_("PlayStation"), ableem::Texture::loadFile(renderer, img + "tab_playstation.png"), {}, 0, 0});
    tabs.push_back({_("RetroArch"), ableem::Texture::loadFile(renderer, img + "tab_retroarch.png"), {}, 0, 0});
    tabs.push_back({_("Apps"), ableem::Texture::loadFile(renderer, img + "tab_apps.png"), {}, 0, 0});
    buildTabs();
    cancelled = true;
}

//*******************************
// GuiSetPicker::buildTabs
//*******************************
void GuiSetPicker::buildTabs() {
    auto games = [](size_t n) { return to_string(n) + " " + _("games"); };
    GameQueryService &query = app.gameQuery();

    // PlayStation: all / internal / the folders / favorites / history / light-gun games
    Tab &ps = tabs[0];
    ps.entries.clear();
    SubDirRowInfos rows;
    app.library().usbGames().loadSubDirRows(&rows);
    const bool internal = query.showInternalGames();
    if (internal) {
        const size_t usb = query.ps1GamesInSubDirRow(0).size();
        const size_t all = usb + query.internalGames().size();
        ps.entries.push_back({_("All Games"), games(all), 0, GameSet::PS1, Ps1SelectState::AllGames, 0, ""});
        ps.entries.push_back(
            {_("Internal Games"), games(all - usb), 0, GameSet::PS1, Ps1SelectState::InternalOnly, 0, ""});
    }
    bool top = true;
    for (const SubDirRowInfo &row : rows) {
        const string title = top ? _("USB Games") : row.rowName;
        ps.entries.push_back({title, games(static_cast<size_t>(row.numGames)), top ? 0 : row.indentLevel, GameSet::PS1,
                              Ps1SelectState::GamesSubdir, row.subDirRowIndex, row.rowName});
        top = false;
    }
    ps.entries.push_back(
        {_("Favorite Games"), games(query.favorites().size()), 0, GameSet::PS1, Ps1SelectState::Favorites, 0, ""});
    ps.entries.push_back(
        {_("Game History"), games(query.history().size()), 0, GameSet::PS1, Ps1SelectState::History, 0, ""});
    const size_t lightgun = query.lightgunGames().size();
    if (lightgun > 0)
        ps.entries.push_back(
            {_("Lightgun Games"), games(lightgun), 0, GameSet::Lightgun, Ps1SelectState::AllGames, 0, ""});

    // RetroArch: a playlist each
    Tab &ra = tabs[1];
    ra.entries.clear();
    for (size_t i = 0; i < raPlaylists.size(); i++) {
        GameSetSelection sel;
        sel.set = GameSet::RetroArch;
        sel.raPlaylistIndex = static_cast<int>(i);
        sel.raPlaylistName = raPlaylists[i];
        ra.entries.push_back({raPlaylists[i], games(query.gamesFor(sel).size()), 0, GameSet::RetroArch,
                              Ps1SelectState::AllGames, static_cast<int>(i), raPlaylists[i]});
    }

    // Apps: the one group
    Tab &apps = tabs[2];
    apps.entries.clear();
    apps.entries.push_back({_("Apps"), games(query.apps().size()), 0, GameSet::Apps, Ps1SelectState::AllGames, 0, ""});

    // the tab and row of what shows now
    tab = selection.set == GameSet::RetroArch ? 1 : selection.set == GameSet::Apps ? 2 : 0;
    Tab &current = tabs[tab];
    current.selected = 0;
    for (size_t i = 0; i < current.entries.size(); i++) {
        const Entry &e = current.entries[i];
        bool match = false;
        if (selection.set == GameSet::RetroArch)
            match = e.index == selection.raPlaylistIndex;
        else if (selection.set == GameSet::Lightgun)
            match = e.set == GameSet::Lightgun;
        else if (selection.set == GameSet::PS1)
            match = e.set == GameSet::PS1 && e.ps1State == selection.ps1SelectState &&
                    (e.ps1State != Ps1SelectState::GamesSubdir || e.index == selection.usbGameDirIndex);
        else
            match = true;
        if (match) {
            current.selected = static_cast<int>(i);
            break;
        }
    }
    keepSelectedVisible();
}

//*******************************
// GuiSetPicker::visibleRows / moveSelection / keepSelectedVisible
//*******************************
int GuiSetPicker::visibleRows() const {
    const int room = SCREEN_HEIGHT - 2 * PanelMargin - TabsHeight - FooterHeight;
    return max(1, room / RowHeight);
}

void GuiSetPicker::moveSelection(int step) {
    Tab &t = tabs[tab];
    if (t.entries.empty())
        return;
    const int count = static_cast<int>(t.entries.size());
    t.selected = max(0, min(count - 1, t.selected + step));
    keepSelectedVisible();
}

void GuiSetPicker::keepSelectedVisible() {
    Tab &t = tabs[tab];
    const int rows = visibleRows();
    if (t.selected < t.firstVisible)
        t.firstVisible = t.selected;
    if (t.selected >= t.firstVisible + rows)
        t.firstVisible = t.selected - rows + 1;
    t.firstVisible = max(0, min(t.firstVisible, max(0, static_cast<int>(t.entries.size()) - rows)));
}

//*******************************
// GuiSetPicker::pick
//*******************************
void GuiSetPicker::pick() {
    const Tab &t = tabs[tab];
    if (t.entries.empty())
        return;
    const Entry &e = t.entries[t.selected];
    selection.set = e.set;
    if (e.set == GameSet::PS1) {
        selection.ps1SelectState = e.ps1State;
        if (e.ps1State == Ps1SelectState::GamesSubdir) {
            selection.usbGameDirIndex = e.index;
            selection.usbGameDirName = e.name;
        }
    } else if (e.set == GameSet::RetroArch) {
        selection.raPlaylistIndex = e.index;
        selection.raPlaylistName = e.name;
    }
    cancelled = false;
}

//*******************************
// GuiSetPicker::render
//*******************************
void GuiSetPicker::render() {
    if (background.valid())
        renderer.copy(background, nullptr, nullptr);
    else
        gui->renderBackground();
    style.dim(renderer);

    const int rows = visibleRows();
    const Tab &t = tabs[tab];
    const int shown = max(1, min(rows, static_cast<int>(t.entries.size())));
    const int panelHeight = TabsHeight + shown * RowHeight + FooterHeight;
    ableem::Rect panel{(SCREEN_WIDTH - PanelWidth) / 2, (SCREEN_HEIGHT - panelHeight) / 2, PanelWidth, panelHeight};
    style.sheet(renderer, panel);

    const TextRenderer::Shadow classicShadow = gui->text().shadow();
    TextRenderer::Shadow shadow;
    shadow.enabled = style.textShadow;
    gui->text().setShadow(shadow);
    Fonts &fonts = gui->assets().themeFonts;

    // the tab strip: three icons across the top, the current one on a band with a bar under it
    const int stripX = panel.x + (panel.w - TabWidth * static_cast<int>(tabs.size())) / 2;
    for (size_t i = 0; i < tabs.size(); i++) {
        const ableem::Rect cell(stripX + TabWidth * static_cast<int>(i), panel.y + 1, TabWidth, TabsHeight - 10);
        const bool current = static_cast<int>(i) == tab;
        if (current) {
            renderer.setBlendMode(ableem::BlendMode::Blend);
            renderer.setDrawColor(ableem::Color(style.text.r, style.text.g, style.text.b, 38));
            renderer.fillRect(cell);
            renderer.setDrawColor(style.text);
            renderer.fillRect(
                ableem::Rect(cell.x, cell.y + cell.h - PanelStyle::SelectionBar, cell.w, PanelStyle::SelectionBar));
        }
        if (tabs[i].icon.valid()) {
            ableem::Rect dst(cell.x + (cell.w - IconSize) / 2, cell.y + 10, IconSize, IconSize);
            tabs[i].icon.setAlphaMod(current ? 255 : 120);
            renderer.copy(tabs[i].icon, nullptr, &dst);
        }
        gui->text().renderText_WithColor(
            fonts[FONT_15_BOLD], tabs[i].title,
            cell.x + cell.w / 2 - gui->text().textWidth(fonts[FONT_15_BOLD], tabs[i].title) / 2,
            cell.y + 10 + IconSize + 4, current ? style.text : style.secondary, XALIGN_LEFT);
    }
    style.rule(renderer, panel, panel.y + TabsHeight - 8);

    // the rows
    int rowY = panel.y + TabsHeight;
    for (int i = t.firstVisible; i < t.firstVisible + rows && i < static_cast<int>(t.entries.size()); i++) {
        const Entry &e = t.entries[i];
        if (i == t.selected)
            style.selection(renderer, ableem::Rect(panel.x + 1, rowY, panel.w - 2, RowHeight));
        const int x = panel.x + RowInset + 8 + e.indent * 24;
        gui->text().renderText_WithColor(fonts[FONT_22_MED], e.title, x, rowY + 8,
                                         i == t.selected ? style.text : style.secondary, XALIGN_LEFT);
        const int w = gui->text().textWidth(fonts[FONT_15_BOLD], e.detail);
        gui->text().renderText_WithColor(fonts[FONT_15_BOLD], e.detail, panel.x + panel.w - RowInset - w, rowY + 14,
                                         style.secondary, XALIGN_LEFT);
        rowY += RowHeight;
    }
    if (t.entries.empty())
        gui->text().renderText_WithColor(fonts[FONT_22_MED], _("Not installed"), panel.x + RowInset + 8,
                                         panel.y + TabsHeight + 8, style.secondary, XALIGN_LEFT);
    const int markerX = panel.x + panel.w - RowInset;
    if (t.firstVisible > 0)
        style.scrollMarker(renderer, markerX, panel.y + TabsHeight - 4, -1);
    if (t.firstVisible + rows < static_cast<int>(t.entries.size()))
        style.scrollMarker(renderer, markerX, panel.y + TabsHeight + shown * RowHeight + 2, 1);

    style.footer(*gui, ableem::Rect(panel.x, panel.y + panel.h - FooterHeight, panel.w, FooterHeight),
                 {{{"X"}, _("Select")}, {{"O"}, _("Cancel")}, {{"L1", "R1"}, _("Tab")}, {{"L2", "R2"}, _("Page")}}, "",
                 false);

    gui->text().setShadow(classicShadow);
    renderer.present();
}

//*******************************
// GuiSetPicker::loop
//*******************************
void GuiSetPicker::loop() {
    menuVisible = true;
    while (menuVisible) {
        Event e;
        while (gui->input().poll(e)) {
            if (e.type == Event::Type::Quit) {
                menuVisible = false;
                continue;
            }
            if (e.type == Event::Type::DpadDown) {
                if (gui->input().dpadDown()) {
                    app.audio().cursor.play();
                    moveSelection(1);
                } else if (gui->input().dpadUp()) {
                    app.audio().cursor.play();
                    moveSelection(-1);
                } else if (gui->input().dpadRight()) {
                    app.audio().cursor.play();
                    tab = (tab + 1) % static_cast<int>(tabs.size());
                } else if (gui->input().dpadLeft()) {
                    app.audio().cursor.play();
                    tab = (tab + static_cast<int>(tabs.size()) - 1) % static_cast<int>(tabs.size());
                }
            } else if (e.type == Event::Type::ButtonDown) {
                switch (e.button) {
                case Button::R1:
                    app.audio().cursor.play();
                    tab = (tab + 1) % static_cast<int>(tabs.size());
                    break;
                case Button::L1:
                    app.audio().cursor.play();
                    tab = (tab + static_cast<int>(tabs.size()) - 1) % static_cast<int>(tabs.size());
                    break;
                case Button::R2:
                    app.audio().cursor.play();
                    moveSelection(visibleRows());
                    break;
                case Button::L2:
                    app.audio().cursor.play();
                    moveSelection(-visibleRows());
                    break;
                case Button::Cross:
                    app.audio().cursor.play();
                    pick();
                    menuVisible = false;
                    break;
                case Button::Circle:
                    app.audio().cancel.play();
                    menuVisible = false;
                    break;
                default:
                    break;
                }
            } else if (e.type == Event::Type::KeyDown && e.key == Key::Escape) {
                app.audio().cancel.play();
                menuVisible = false;
            }
        }
        render();
    }
}
