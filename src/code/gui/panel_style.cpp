//
// PanelStyle: the shared look of the menus and dialogs. See the header.
//
#include "panel_style.h"
#include "gui.h"
#include "theme_assets.h"

#include <ableem/engine/theme_spec.h>

#include <algorithm>

using namespace std;
using ableem::Color;
using ableem::Rect;

//*******************************
// PanelStyle::fromTheme
//*******************************
PanelStyle PanelStyle::fromTheme(const ableem::LauncherTheme &theme) {
    PanelStyle s;
    if (theme.colors.text.set)
        s.text = TextRenderer::toColor(theme.colors.text, 255);
    if (theme.colors.secondary.set)
        s.secondary = TextRenderer::toColor(theme.colors.secondary, 255);
    s.hint = theme.colors.hint.set ? TextRenderer::toColor(theme.colors.hint, 255) : s.secondary;
    s.textShadow = !theme.textShadow.set || theme.textShadow;
    return s;
}

//*******************************
// PanelStyle::dim
//*******************************
void PanelStyle::dim(ableem::Renderer &renderer) const {
    renderer.setBlendMode(ableem::BlendMode::Blend);
    renderer.setDrawColor(Color(0, 0, 0, 110));
    renderer.fillRect();
}

//*******************************
// PanelStyle::sheet
//*******************************
void PanelStyle::sheet(ableem::Renderer &renderer, const Rect &panel) const {
    renderer.setBlendMode(ableem::BlendMode::Blend);
    renderer.setDrawColor(Color(0, 0, 0, 200));
    renderer.fillRect(panel);
    renderer.setDrawColor(Color(secondary.r, secondary.g, secondary.b, 160));
    renderer.drawRect(panel);
}

//*******************************
// PanelStyle::rule
//*******************************
void PanelStyle::rule(ableem::Renderer &renderer, const Rect &panel, int y) const {
    renderer.setBlendMode(ableem::BlendMode::Blend);
    renderer.setDrawColor(Color(secondary.r, secondary.g, secondary.b, 160));
    renderer.fillRect(Rect(panel.x + RowInset, y, panel.w - 2 * RowInset, 1));
}

//*******************************
// PanelStyle::header
//*******************************
int PanelStyle::header(Gui &gui, const Rect &panel, const string &title) const {
    gui.text().renderText_WithColor(gui.assets().themeFonts[FONT_28_BOLD], title, panel.x + RowInset, panel.y + 18,
                                    text, XALIGN_LEFT);
    rule(gui.renderer(), panel, panel.y + HeaderHeight - 8);
    return panel.y + HeaderHeight;
}

//*******************************
// PanelStyle::selection
//*******************************
void PanelStyle::selection(ableem::Renderer &renderer, const Rect &rect) const {
    renderer.setBlendMode(ableem::BlendMode::Blend);
    renderer.setDrawColor(Color(text.r, text.g, text.b, 38));
    renderer.fillRect(rect);
    renderer.setDrawColor(text);
    renderer.fillRect(Rect(rect.x, rect.y, SelectionBar, rect.h));
}

//*******************************
// PanelStyle::label
//*******************************
void PanelStyle::label(ableem::Renderer &renderer, const Rect &rect) const {
    renderer.setBlendMode(ableem::BlendMode::Blend);
    renderer.setDrawColor(Color(secondary.r, secondary.g, secondary.b, 70));
    renderer.fillRect(rect);
}

//*******************************
// PanelStyle::scrollMarker
//*******************************
void PanelStyle::scrollMarker(ableem::Renderer &renderer, int cx, int cy, int direction) const {
    renderer.setDrawColor(text);
    for (int i = 0; i < 5; i++)
        renderer.fillRect(Rect(cx - i, cy + direction * i, 2 * i + 1, 1));
}

//*******************************
// PanelStyle::parseHints
//*******************************
vector<PanelStyle::HintItem> PanelStyle::parseHints(const string &line, string &status) {
    auto trim = [](string s) {
        // spaces and the "|" separators the old lines carried
        const string junk = " |\t";
        size_t a = s.find_first_not_of(junk);
        size_t b = s.find_last_not_of(junk);
        return a == string::npos ? string() : s.substr(a, b - a + 1);
    };
    vector<HintItem> items;
    vector<string> pendingIcons; // icons whose text was only a separator: they belong to the next hint
    size_t pos = line.find("|@");
    status = trim(line.substr(0, pos == string::npos ? line.size() : pos));
    while (pos != string::npos) {
        size_t end = line.find('|', pos + 2);
        if (end == string::npos)
            break;
        string icon = line.substr(pos + 2, end - pos - 2);
        size_t next = line.find("|@", end + 1);
        string text = line.substr(end + 1, next == string::npos ? string::npos : next - end - 1);
        string label = trim(text);
        pendingIcons.push_back(icon);
        if (label.empty() || label == "/") {
            pos = next;
            continue;
        }
        items.push_back({pendingIcons, label});
        pendingIcons.clear();
        pos = next;
    }
    if (!pendingIcons.empty())
        items.push_back({pendingIcons, ""});
    return items;
}

//*******************************
// PanelStyle::footer
//*******************************
namespace {
// the shared order of the footer's hints, by the hint's first button
int buttonRank(const string &icon) {
    static const char *order[] = {"X", "O", "T", "S", "Start", "Select", "L1", "R1", "L2", "R2", "Enter", "Esc", "Tab"};
    for (size_t i = 0; i < sizeof(order) / sizeof(order[0]); i++)
        if (icon == order[i])
            return static_cast<int>(i);
    return 100;
}
} // namespace

void PanelStyle::footer(Gui &gui, const Rect &footer, const vector<HintItem> &given, const string &status,
                        bool withRule) const {
    if (withRule)
        rule(gui.renderer(), footer, footer.y);
    vector<HintItem> hints = given;
    stable_sort(hints.begin(), hints.end(), [](const HintItem &a, const HintItem &b) {
        return buttonRank(a.icons.empty() ? "" : a.icons[0]) < buttonRank(b.icons.empty() ? "" : b.icons[0]);
    });
    ThemeAssets &assets = gui.assets();
    TextRenderer &text = gui.text();
    const int iconH = 30;
    const int y = footer.y + 14;
    auto iconFor = [&](const string &key) -> ableem::Texture {
        if (key == "X")
            return assets.hintCross.valid() ? assets.hintCross : assets.buttonTextureMap["X"];
        if (key == "O")
            return assets.hintCircle.valid() ? assets.hintCircle : assets.buttonTextureMap["O"];
        if (key == "T")
            return assets.hintTriangle.valid() ? assets.hintTriangle : assets.buttonTextureMap["T"];
        return assets.buttonTextureMap[key];
    };
    // the status at the right edge, in the secondary colour; the hints get what is left
    int right = footer.x + footer.w - RowInset;
    const ableem::Font &statusFont = assets.themeFonts[FONT_22_MED];
    if (!status.empty()) {
        const int w = text.textWidth(statusFont, status);
        text.renderText_WithColor(statusFont, status, right - w, y, secondary, XALIGN_LEFT);
        right -= w + 36;
    }
    // the largest font the hints fit in, then the gap between them
    const int room = right - (footer.x + RowInset);
    auto widthAt = [&](const ableem::Font &font, int gap) {
        int w = 0;
        for (const HintItem &h : hints) {
            for (const string &icon : h.icons)
                w += iconFor(icon).valid() ? iconH + 6 : 0;
            w += 2 + text.textWidth(font, h.label) + gap;
        }
        return w - gap;
    };
    ableem::Font font = assets.themeFonts[FONT_22_MED];
    int gap = 36;
    if (widthAt(font, gap) > room) {
        gap = 22;
        if (widthAt(font, gap) > room) {
            font = assets.themeFonts[FONT_20_BOLD];
            if (widthAt(font, gap) > room)
                font = assets.themeFonts[FONT_15_BOLD];
        }
    }
    const int fontH = font.lineHeight();
    int x = footer.x + RowInset;
    for (const HintItem &h : hints) {
        for (const string &key : h.icons) {
            ableem::Texture icon = iconFor(key);
            if (!icon.valid())
                continue;
            ableem::Size s = icon.size();
            Rect dst(x, y + (iconH - s.h) / 2, s.w, s.h);
            gui.renderer().copy(icon, nullptr, &dst);
            x += iconH + 6;
        }
        x += 2;
        text.renderText_WithColor(font, h.label, x, y + (iconH - fontH) / 2, hint, XALIGN_LEFT);
        x += text.textWidth(font, h.label) + gap;
    }
}

void PanelStyle::footer(Gui &gui, const Rect &footerRect, const string &line, bool withRule) const {
    string status;
    vector<HintItem> items = parseHints(line, status);
    footer(gui, footerRect, items, status, withRule);
}
