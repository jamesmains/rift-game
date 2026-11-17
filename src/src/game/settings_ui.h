// 1.8.3 (#170 #174 #175 #178 #179 #182 #183): the Settings pages, drawn in the active UI theme. One panel serves the
// desktop's Settings window and the in-game Settings screen (pause > Settings no longer leaves the game). Both write
// settings.ini. Pages: General (Display incl. Theme / Sound / Play), Controls (every action x Key / Alt key / Gamepad),
// Access (Reading / Colour / Comfort + the Steam Deck note). Changes apply at once; Apply / OK / Done save them,
// Cancel (desktop-native) puts back what was there when the panel opened.
#pragma once
#include "game.h"
#include <functional>

struct SettingsPanel {
    int page = 0;                 // 0 General, 1 Controls, 2 Access
    int focus = 0;                // own navigation: the focused control (in drawing order)
    int scroll = 0;               // Controls: first row shown
    int row = 0, col = 0;         // Controls: chosen action and column (0 key, 1 alt key, 2 gamepad)
    int waiting = -1;             // Controls: action * 3 + column waiting for a key (-1 none)
    int openCombo = -1;           // desktop-native: an open drop-down (its control index)
    bool ownNav = true;           // true: Up/Down/Left/Right/Enter drive the focus here (the game); false: the desktop's cursor does
    bool inGame = false;          // buttons: Apply + Done (game) or Defaults / Apply / OK (+ Cancel in desktop-native)
    std::function<void(Rect)> hot;        // desktop: register a control for the pad cursor
    std::function<void(Rect)> sliderHot;  // desktop: a slider the pad's left / right adjusts
    bool padCursor = false;       // desktop: the pad / keyboard is steering the cursor (hover = focus)
    float repeat = 0;             // slider key repeat
    GameSettings orig; Keybinds origKeys; bool haveOrig = false;   // for Cancel
    void begin(const GameSettings& s);   // when the window / screen opens
    enum Result { R_NONE = 0, R_APPLY, R_OK, R_CANCEL };
    // draws the pages inside r and handles input; returns what the buttons asked for (the caller saves / closes)
    int draw(const Theme::Pen& p, InputState& in, Rect r, GameSettings& s, PlatformRequests* req, bool active);
    // what a frame's controls looked like (for pad navigation and tests)
    struct Ctl { Rect r; int kind; };
    std::vector<Ctl> ctls;
    int lastCount = 0;
    std::string helpLine() const;
};
