//
// The arithmetic of the launcher's state-change animations (ab_gui step G5o3), pure and header-only: where the
// menu row, its icons, the meta panel and the settings band are at a point of a transition, given the transition's
// eased progress (0..1 - the value of a non-ambient abgui::Tween with easeOutCubic, which is what the old hand-written
// timers computed). These are the old PsMenu/PsMeta/PsSettingsBack formulas, unchanged; the controls call them from
// their render(), and tests/screens/test_launcher_state_tweens.cpp holds them to a frozen copy of the old code.
//
#pragma once

namespace evomotion {

// the distance between two icons of the menu row, and its animations' lengths (milliseconds)
constexpr float IconGap = 130.0f;
constexpr unsigned int MenuSlideMs = 200;    // the row up or down with the state
constexpr unsigned int OptionMoveMs = 100;   // one icon to the next (move and zoom)
constexpr unsigned int MetaSlideMs = 200;    // the meta panel
constexpr unsigned int SettingsBandMs = 100; // the settings band

// the offset that keeps an icon drawn at `scale` centred on its unzoomed place
inline float zoomOffset(float scale) {
    return -(118.0f * scale - 118.0f) / 2.0f;
}

// the row's y on its way from `restY` to `targetY`
inline float rowY(float restY, int targetY, float progress) {
    return restY + (progress * (targetY - restY));
}

// the selected icon's scale while the row opens (zooms in) or closes (zooms out)
inline float openingScale(float progress, float maxZoom) {
    return 1 + progress * (maxZoom - 1);
}
inline float closingScale(float progress, float maxZoom) {
    return 1 + (1 - progress) * (maxZoom - 1);
}

// the row's x while the selection moves to the icon on the left (direction 0) or on the right (1)
inline float optionX(int direction, float restX, float progress) {
    return direction == 0 ? restX + progress * IconGap : restX - progress * IconGap;
}

// the integer position (the meta panel's y, the settings band's length) between `from` and `to`
inline int slidInt(int from, int to, float progress) {
    return static_cast<int>(from + ((to - from) * progress));
}

} // namespace evomotion
