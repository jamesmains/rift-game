// 1.8.3 (#170 #174 #175 #178 #179 #182 #183): the Settings pages in the active UI theme (see settings_ui.h).
#include "settings_ui.h"
#include "../core/audio.h"
#include "../core/spatnav.h"
#include <algorithm>

using namespace Theme;
using UI::VIEW_X; using UI::VIEW_Y; using UI::VIEW_W; using UI::VIEW_H; using UI::SCREEN_W; using UI::SCREEN_H;

void SettingsPanel::begin(const GameSettings& s) {
    orig = s; origKeys = gKeys; haveOrig = true;
    page = 0; focus = 0; scroll = 0; row = 0; col = 0; waiting = -1; openCombo = -1; nav = SpatNav::Memory();
}

std::string SettingsPanel::helpLine() const {
    if (page == 1) return "Pick a row, press Enter, then press the new key or pad button. Esc clears it.";
    if (page == 2) return "Easier reading, colours and fewer flashes.";
    return "";
}

namespace {
const char* THEME_NAMES[] = {"Adventurer's Frame", "Desktop-native", "One-Bit Desktop"};
const char* SPEEDS[] = {"slow", "normal", "fast", "instant"};
const char* CHATTER[] = {"off", "rare", "normal", "often"};
}

int SettingsPanel::draw(const Pen& p, InputState& in, Rect r, GameSettings& s, PlatformRequests* req, bool active) {
    const UiTheme& t = th(); Canvas& cv = *p.cv;
    if (!haveOrig) begin(s);
    std::vector<Ctl> now;
    int result = R_NONE;
    const bool nat = native(), one = onebit();
    const bool keyNav = ownNav && active && waiting < 0 && openCombo < 0;
    // ---- own navigation (the game): which control has the focus, and what the keys do to it ----
    bool upP = keyNav && actP(in, A_UP), downP = keyNav && actP(in, A_DOWN);
    bool leftP = keyNav && actP(in, A_LEFT), rightP = keyNav && actP(in, A_RIGHT);
    bool okP = keyNav && actP(in, A_CONFIRM);
    int leftRight = 0;
    if (leftP) { leftRight = -1; repeat = 0.35f; } else if (rightP) { leftRight = 1; repeat = 0.35f; }
    if (keyNav && !leftRight && (actD(in, A_LEFT) || actD(in, A_RIGHT))) { repeat -= 1.0f / 60; if (repeat <= 0) { leftRight = actD(in, A_LEFT) ? -1 : 1; repeat = 0.05f; } }
    if (keyNav && (in.pressed[K_TAB] || actP(in, A_PAGEDOWN))) { page = (page + 1) % 3; focus = page; in.eatKeys(); }
    else if (keyNav && actP(in, A_PAGEUP)) { page = (page + 2) % 3; focus = page; in.eatKeys(); }

    struct DropList { bool open = false; int x = 0, y = 0, w = 0, lh = 15, sel = 0; std::vector<std::string> opts; } dropList;   // an open combo's list, drawn last
    int idx = 0;   // controls in drawing order
    struct St { bool hover, focused, click, activate; };
    auto ctl = [&](Rect rc, int kind) -> St {
        int i = idx++; now.push_back({rc, kind});
        if (hot && active) hot(rc);
        bool hover = active && openCombo < 0 && rc.contains(in.mouseX, in.mouseY);
        bool focused = ownNav ? (active && i == focus) : (hover && padCursor);
        bool click = hover && in.mousePressed[0];
        bool act = ownNav && focused && okP;
        if (click && ownNav) focus = i;
        return {hover, focused, click, act};
    };
    auto reqFull = [&]() { if (req) { req->setFullscreen = true; req->fullscreenValue = s.fullscreen; } };
    auto reqInt = [&]() { if (req) { req->setIntegerScaling = true; req->integerScalingValue = s.integerScaling; } };
    auto reqVsync = [&]() { if (req) { req->setVsync = true; req->vsyncValue = s.vsync; } };
    auto reqScale = [&]() { if (req) req->setScale = std::max(1, s.scale); };

    // ---- tabs ----
    int tx = r.x + 8, ty = r.y + 6;
    if (inGame && !nat && !one) { title(p, r.x + 10, r.y + 10, "Settings"); tx = r.x + 16 + titleW(p, "Settings") + 30; }
    std::vector<std::string> tabNames = {"General", "Controls", "Access"};
    int tabW = nat ? 70 : 106, tabH = nat ? 20 : 24;
    if (one) tabW = 0;
    std::vector<int> tws;
    if (one) { for (auto& n : tabNames) tws.push_back(titleW(p, n) + 22); }
    if (one) {
        int x = tx;
        for (int i = 0; i < 3; i++) {
            Rect rc{x, ty, tws[i], tabH}; St st = ctl(rc, 5);
            if (i == page) { cv.fillRect(x, ty, tws[i], tabH, t.text); p.font->drawClassic(cv, *p.ts, x + 11, ty + 6, tabNames[i], t.back, false); }
            else { cv.fillRect(x, ty, tws[i], tabH, t.back); cv.rect(x, ty, tws[i], tabH, st.hover ? t.accent : t.dim); p.font->drawClassic(cv, *p.ts, x + 11, ty + 6, tabNames[i], t.text, false); }
            if (st.focused && !(i == page)) glow(cv, x, ty, tws[i], tabH);
            if (st.click || st.activate) { page = i; focus = i; }
            x += tws[i] + 6;
        }
        textRight(p, r.x + r.w - 10, ty + 6, gPadGlyphs ? "lb / rb: page" : "tab: next page", t.dim);
    } else {
        tabs(p, tx, ty, tabW, tabH, tabNames, page, nat ? 2 : 6);
        for (int i = 0; i < 3; i++) {
            Rect rc{tx + i * (tabW + (nat ? 2 : 6)), ty, tabW, tabH}; St st = ctl(rc, 5);
            if (st.focused) { if (nat) glow(cv, rc.x + 3, rc.y + 3, rc.w - 6, rc.h - 4); else frameRect(cv, rc.x - 2, rc.y - 2, rc.w + 4, rc.h + 4, t.accentHi); }
            if (st.click || st.activate) { page = i; focus = i; }
        }
    }
    int top = ty + tabH + 6;
    if (nat) { // the property sheet's body
        Color8 f = t.face; bevel(cv, r.x + 4, ty + tabH, r.w - 8, r.h - (ty + tabH - r.y) - 40, t.hi, t.lo, &f);
        bevel(cv, r.x + 5, ty + tabH + 1, r.w - 10, r.h - (ty + tabH - r.y) - 42, t.hi2, t.lo2);
        // the open tab joins the body
        int ox = tx + page * (tabW + 2); cv.fillRect(ox + 1, ty + tabH - 1, tabW - 3, 3, t.face);
    } else rule(cv, r.x + 8, top - 2, r.w - 16);
    top += 4;
    int bottom = r.y + r.h - (inGame && !nat && !one ? 50 : 36);   // the button row (the frame's in-game screen has a key hint line under it)
    const int gx = r.x + (nat ? 14 : 8), gw = r.w - (nat ? 28 : 16);

    // ---- helpers for the General / Access pages ----
    auto checkRow = [&](int x, int y, const std::string& label, bool& v, bool enabled = true) -> bool {
        int w = textW(p, label) + 22; St st = ctl({x - 2, y - 3, w + 4, 18}, 1);
        checkbox(p, x, y, v, label, st.focused, enabled);
        if (st.hover && !nat && !st.focused && !one) frameRect(cv, x - 3, y - 4, w + 5, 20, t.hi2);
        if ((st.click || st.activate) && enabled) { v = !v; return true; }
        return false;
    };
    // a choice: segmented (frame), inverted boxes (onebit) or a drop-down combo / toggle buttons (native)
    auto choiceRow = [&](int x, int y, const std::vector<std::string>& opts, int& v, bool combo) -> bool {
        int before = v;
        if (nat && combo) {
            int w = 0; for (auto& o : opts) w = std::max(w, textW(p, o)); w += 34;
            int myIdx = idx; St st = ctl({x, y - 2, w, 20}, 3);
            Color8 white{255, 255, 255, 255}; well(cv, x, y - 2, w, 20, &white);
            if (st.focused) cv.fillRect(x + 3, y + 1, w - 22, 14, t.rowSel);
            text(p, x + 5, y + 2, opts[std::clamp(v, 0, (int)opts.size() - 1)], st.focused ? t.selText : t.text);
            Color8 f = t.face; int bx = x + w - 18; bevel(cv, bx, y, 16, 16, t.hi, t.lo, &f); bevel(cv, bx + 1, y + 1, 14, 14, t.hi2, t.lo2);
            for (int i = 0; i < 4; i++) cv.fillRect(bx + 4 + i, y + 6 + i, 8 - 2 * i, 1, t.lo);
            // 1.8.4 (#187): the click that opens the list must not also close it (it used to reach the "click elsewhere" test below)
            if (st.click && openCombo != myIdx) { openCombo = myIdx; comboSel = v; in.mousePressed[0] = false; }
            // 1.8.6 (#203): Confirm (the pad's A) opens the list like a click; Left / Right still step the value in place
            if (st.activate && openCombo != myIdx) { openCombo = myIdx; comboSel = v; in.eatKeys(); gAudio.play("ui_open"); }
            else if (st.focused && leftRight) v = (v + (leftRight < 0 ? (int)opts.size() - 1 : 1)) % (int)opts.size();
            if (openCombo == myIdx) {
                // the list drops below the field; a click picks, a click elsewhere (the field too) closes it. It is drawn
                // after everything else (dropList), so the rows under it can't cover it.
                int lh = 15, ly = y + 18; Rect field{x, y - 2, w, 20};
                bool pressed = in.mousePressed[0];
                for (int i = 0; i < (int)opts.size(); i++) {
                    Rect orc{x + 1, ly + 1 + i * lh, w - 2, lh};
                    if (pressed && orc.contains(in.mouseX, in.mouseY)) { v = i; openCombo = -1; in.mousePressed[0] = false; pressed = false; }
                }
                if (pressed) { openCombo = -1; in.mousePressed[0] = false; (void)field; }
                if (openCombo == myIdx && active) {   // 1.8.6 (#203): keys walk a highlight in the open list; Confirm takes it, Cancel leaves the value
                    int n = (int)opts.size();
                    comboSel = std::clamp(comboSel, 0, n - 1);
                    for (int i = 0; i < n; i++) { Rect orc{x + 1, ly + 1 + i * lh, w - 2, lh}; if ((in.mouseDX || in.mouseDY) && orc.contains(in.mouseX, in.mouseY)) comboSel = i; }
                    if (actP(in, A_UP)) { comboSel = (comboSel + n - 1) % n; gAudio.play("ui_move"); }
                    else if (actP(in, A_DOWN)) { comboSel = (comboSel + 1) % n; gAudio.play("ui_move"); }
                    else if (actP(in, A_CONFIRM)) { v = comboSel; openCombo = -1; in.eatKeys(); gAudio.play("ui_confirm"); }
                    else if (actP(in, A_CANCEL)) { openCombo = -1; in.eatKeys(); gAudio.play("ui_cancel"); }
                }
                if (openCombo == myIdx) { dropList.open = true; dropList.x = x; dropList.y = ly; dropList.w = w; dropList.lh = lh; dropList.opts = opts; dropList.sel = comboSel; }
            }
            return v != before;
        }
        int hoverOpt = choiceHit(p, x, y, opts, in.mouseX, in.mouseY);
        int w = choiceW(p, opts); St st = ctl({x, y, w, 20}, 3);
        choice(p, x, y, opts, v, st.focused, active ? hoverOpt : -1);
        if (st.click && hoverOpt >= 0) v = hoverOpt;
        if (st.focused && leftRight) v = std::clamp(v + leftRight, 0, (int)opts.size() - 1);
        if (st.activate) v = (v + 1) % (int)opts.size();
        return v != before;
    };
    auto sliderRow = [&](int x, int y, int w, const std::string& label, int& v) {
        text(p, x, y, label, t.text);
        int sx = x + 130, sw = std::max(60, w - 130 - 54);
        St st = ctl({sx - 4, y - 3, sw + 8, 18}, 2);
        if (sliderHot && active) sliderHot({sx - 4, y - 3, sw + 8, 18});
        slider(p, sx, y, sw, v, 0, 100, st.focused);
        if (active && st.hover && in.mouseDown[0]) v = std::clamp((in.mouseX - sx - 5) * 100 / std::max(1, sw - 10), 0, 100);
        if (st.focused && leftRight) v = std::clamp(v + leftRight, 0, 100);
        if (!ownNav && st.hover && padCursor) {   // the desktop's cursor sits on it: left / right adjust
            int d = 0;
            if (actP(in, A_LEFT)) { d = -1; repeat = 0.3f; } else if (actP(in, A_RIGHT)) { d = 1; repeat = 0.3f; }
            else if (actD(in, A_LEFT) || actD(in, A_RIGHT)) { repeat -= 1.0f / 60; if (repeat <= 0) { d = actD(in, A_LEFT) ? -1 : 1; repeat = 0.05f; } }
            if (d) v = std::clamp(v + d, 0, 100);
        }
    };
    // a group: a carved panel (frame), a group box (native) or a heading with a dotted rule (onebit)
    auto group = [&](int y, int h, const std::string& name) -> int {
        if (nat) { panelTitled(p, gx, y, gw, h, name); return y + 18; }
        if (one) { title(p, gx + 4, y, name); int tw2 = titleW(p, name) + 12; for (int xx = gx + 4 + tw2; xx < gx + gw - 4; xx += 2) cv.put(xx, y + 7, t.dim); return y + 20; }
        panel(cv, gx, y, gw, h); title(p, gx + 10, y + 7, name); return y + 26;
    };

    if (page == 0) {
        // Display 5 rows, Sound 4, Play 4: the row height fits what there is room for
        int gt = nat ? 22 : one ? 22 : 30, gap = 6;
        // 1.8.4 (#188): the Theme row's buttons (and their focus ring) get room of their own above the check boxes
        const int themeExtra = 8;
        if ((bottom - top - 4 - themeExtra - 3 * (gt + 8 + gap)) / 13 < 18) { gt = nat || one ? 20 : 24; gap = 2; }   // a short window (640x480): tighter groups
        int rh = std::clamp((bottom - top - 4 - themeExtra - 3 * (gt + 8 + gap)) / 13, 15, 24);
        int y = top;
        int h1 = gt + 5 * rh + 8 + themeExtra, h2 = gt + 4 * rh + 8, h3 = gt + 4 * rh + 8;
        int cy = group(y, h1, "Display");
        int c1 = gx + 14, c2 = gx + gw / 2 + 6;
        { text(p, c1, cy + 3, "Theme", nat ? t.text : t.dim); int th0 = s.theme; if (choiceRow(c1 + 64, cy, {THEME_NAMES[0], THEME_NAMES[1], THEME_NAMES[2]}, s.theme, true) && s.theme != th0) {} cy += rh + themeExtra; }
        if (checkRow(c1, cy + 2, "Fullscreen (F11)", s.fullscreen)) reqFull();
        if (checkRow(c2, cy + 2, "Classic 4:3 screen", s.classic43)) reqScale();
        cy += rh;
        if (checkRow(c1, cy + 2, "Pixel-perfect integer scaling", s.integerScaling)) reqInt();
        checkRow(c2, cy + 2, "CRT display effect", s.crt);
        cy += rh;
        if (checkRow(c1, cy + 2, "V-Sync", s.vsync)) reqVsync();
        cy += rh;
        {   // window scale (left) and night colours (right)
            text(p, c1, cy + 3, "Window scale", nat ? t.text : t.dim);
            int sc = std::clamp(s.scale, 1, 3) - 1; int lw = textW(p, "Window scale") + 12;
            if (nat) {   // three toggle buttons
                for (int i = 0; i < 3; i++) { Rect rc{c1 + lw + i * 34, cy, 30, 20}; St st = ctl(rc, 0); button(p, rc.x, rc.y, rc.w, rc.h, std::to_string(i + 1) + "x", i == sc ? BS_SELECTED : BS_NORMAL, st.hover); if (st.focused) glow(cv, rc.x + 3, rc.y + 3, rc.w - 6, rc.h - 6); if (st.click || st.activate) { s.scale = i + 1; reqScale(); } }
            } else if (choiceRow(c1 + lw, cy, {"1x", "2x", "3x"}, sc, false)) { s.scale = sc + 1; reqScale(); }
            int nc = s.nightClassic ? 1 : 0; int lw2 = textW(p, "Night colours") + 12;
            text(p, c2, cy + 3, "Night colours", nat ? t.text : t.dim);
            if (choiceRow(c2 + lw2, cy, {"Stage", "Darken (classic)"}, nc, true)) s.nightClassic = nc != 0;
        }
        y += h1 + gap;
        cy = group(y, h2, "Sound");
        sliderRow(c1, cy + 2, gw - 28, "Master volume", s.masterVolume); cy += rh;
        sliderRow(c1, cy + 2, gw - 28, "Music volume", s.musicVolume); cy += rh;
        sliderRow(c1, cy + 2, gw - 28, "SFX volume", s.sfxVolume); cy += rh;
        sliderRow(c1, cy + 2, gw - 28, "Ambience volume", s.ambientVolume);
        y += h2 + gap;
        cy = group(y, h3, nat ? "Gameplay" : "Play");
        checkRow(c1, cy + 2, "Gamepad enabled", s.gamepad); checkRow(c2, cy + 2, "Screen shake", s.screenShake); cy += rh;
        checkRow(c1, cy + 2, "Fast combat animations", s.fastCombat); checkRow(c2, cy + 2, "Interact while moving", s.interactWhileMoving); cy += rh;
        checkRow(c1, cy + 2, "Edge scrolling in combat", s.edgeScroll); cy += rh;
        { text(p, c1, cy + 3, "Companion chatter", nat ? t.text : t.dim); int b = std::clamp(s.banter, 0, 3); if (choiceRow(c1 + textW(p, "Companion chatter") + 14, cy, {CHATTER[0], CHATTER[1], CHATTER[2], CHATTER[3]}, b, true)) s.banter = b; }
    } else if (page == 2) {
        const int gt = nat ? 22 : one ? 22 : 30, gap = 6, rh = 22;
        int y = top, c1 = gx + 14;
        int cy = group(y, gt + 2 * rh + 8, "Reading");
        checkRow(c1, cy + 2, "Large text (dialogue, messages, hints, story)", s.largeText); cy += rh;
        { text(p, c1, cy + 3, "Text speed", nat ? t.text : t.dim); int v = std::clamp(s.textSpeed, 0, 3); if (choiceRow(c1 + textW(p, "Text speed") + 14, cy, {SPEEDS[0], SPEEDS[1], SPEEDS[2], SPEEDS[3]}, v, true)) s.textSpeed = v; }
        y += gt + 2 * rh + 8 + gap;
        cy = group(y, gt + 2 * rh + 8, "Colour");
        checkRow(c1, cy + 2, "Colour-blind friendly colours (blue / orange)", s.colorblind); cy += rh;
        text(p, c1 + 20, cy + 2, "Your side, hit chances and cover stop using red / green.", t.dim);
        y += gt + 2 * rh + 8 + gap;
        cy = group(y, gt + 3 * rh + 8, "Comfort");
        checkRow(c1, cy + 2, "Reduce flashing (no lightning flashes)", s.reduceFlashing); cy += rh;
        checkRow(c1, cy + 2, "Screen shake", s.screenShake); cy += rh;
        checkRow(c1, cy + 2, "Fast combat animations", s.fastCombat);
        y += gt + 3 * rh + 8 + gap + 4;
        if (y < bottom - 30) p.font->drawWrapped(cv, *p.ts, gx + 6, y, gw - 12, "Small screens (Steam Deck): with integer scaling on, the game uses a sharp 2x when it fits and a smooth fit to the screen when it doesn't.", t.dim);
    } else {
        // ---- controls: every action x Key / Alt key / Gamepad ----
        text(p, gx + 4, top, helpLine(), t.dim);
        int ly = top + 18, lh = bottom - 8 - ly;
        if (nat) { Color8 white{255, 255, 255, 255}; well(cv, gx, ly, gw, lh, &white); }
        else if (one) { /* black, no box */ }
        else well(cv, gx, ly, gw, lh);
        const int rowH = nat ? 17 : 20, cellW = std::min(120, (gw - 200) / 3 - 10);
        int ax = gx + 14, kx = gx + std::max(170, gw - 3 * (cellW + 10) - 24);
        int hy = ly + 6;
        auto head = [&](int x, const std::string& sname) { if (one) p.font->drawClassic(cv, *p.ts, x, hy - 2, sname, t.text, false); else if (nat) text(p, x, hy - 2, sname, t.text); else small(p, x, hy, sname, t.accent); };
        if (nat) { Color8 f = t.face; int hx = gx + 2; int ws[4] = {kx - gx - 2, cellW + 10, cellW + 10, gx + gw - 2 - (kx + 2 * (cellW + 10))}; for (int i = 0; i < 4; i++) { bevel(cv, hx, ly + 2, ws[i], 18, t.hi, t.lo, &f); hx += ws[i]; } hy = ly + 5; }
        head(ax - 4, "Action"); head(kx, "Key"); head(kx + cellW + 10, "Alt key"); head(kx + 2 * (cellW + 10), "Gamepad");
        int ry0 = ly + (nat ? 24 : 22);
        if (one) for (int xx = gx; xx < gx + gw; xx += 2) cv.put(xx, ry0 - 4, t.dim);
        int rows = std::max(1, (ly + lh - 6 - ry0) / rowH);
        int maxScroll = std::max(0, (int)A_COUNT - rows);
        // keys: Up/Down a row, Left/Right a column, Enter waits for the new key; the wheel scrolls
        const int tableIdx = idx++;   // the table is one control (after the tabs)
        if (keyNav && focus == tableIdx) {
            if (upP) { if (row > 0) row--; else focus = tableIdx - 1; upP = false; }
            if (downP) { if (row + 1 < (int)A_COUNT) row++; else focus = tableIdx + 1; downP = false; }
            if (leftP) col = std::max(0, col - 1);
            if (rightP) col = std::min(2, col + 1);
            if (okP) { waiting = row * 3 + col; in.eatKeys(); okP = false; }
        }
        if (active && in.wheel && r.contains(in.mouseX, in.mouseY)) scroll = std::clamp(scroll - in.wheel * 3, 0, maxScroll);
        if (row < scroll) scroll = row; if (row >= scroll + rows) scroll = row - rows + 1;
        scroll = std::clamp(scroll, 0, maxScroll);
        for (int k = 0; k < rows && scroll + k < (int)A_COUNT; k++) {
            int a = scroll + k, ry = ry0 + k * rowH;
            bool chosen = ownNav ? (a == row && focus == tableIdx) : (waiting >= 0 && waiting / 3 == a);
            if (chosen) { listRow(p, gx + 4, ry - 2, gw - 22, rowH - 1, RS_SELECTED); if (!one) text(p, gx + 7, ry + 1, ">", nat ? t.selText : t.accent); }
            text(p, ax, ry + 1, actionLabel(a), chosen ? (nat ? t.selText : t.accent) : t.text);
            for (int c = 0; c < 3; c++) {
                uint8_t slot = c == 0 ? gKeys.key1[a] : c == 1 ? gKeys.key2[a] : gKeys.pad[a];
                int bx = kx + c * (cellW + 10); bool wait = waiting == a * 3 + c;
                Rect rc{bx, ry - 1, cellW, rowH - 4};
                bool hover = active && rc.contains(in.mouseX, in.mouseY);
                if (hot && active) hot(rc);
                std::string name = slot ? keyName(slot) : "None";
                bool none = !slot;
                if (nat) {
                    if (wait) { cv.fillRect(rc.x - 2, rc.y, rc.w + 4, rc.h + 1, t.tooltip); cv.rect(rc.x - 2, rc.y, rc.w + 4, rc.h + 1, t.lo); text(p, rc.x + 2, ry + 1, "press a button...", RGB(192, 0, 0)); }
                    else text(p, rc.x + 2, ry + 1, name, chosen ? t.selText : none ? t.dim : t.text);
                    if (hover && padCursor && !wait) glow(cv, rc.x - 2, rc.y, rc.w + 4, rc.h + 1);
                } else if (one) {
                    if (wait) { cv.fillRect(rc.x, rc.y, rc.w, rc.h, t.accent); text(p, rc.x + 5, ry + 1, "press a button...", t.back); }
                    else { cv.fillRect(rc.x, rc.y, rc.w, rc.h, t.back); cv.rect(rc.x, rc.y, rc.w, rc.h, none ? t.pipOff : (hover ? t.accent : t.text)); text(p, rc.x + 5, ry + 1, name, none ? t.dim : t.text); }
                    if (chosen && c == col && !wait) glow(cv, rc.x, rc.y, rc.w, rc.h);
                } else {
                    Color8 f = wait ? t.selFill : RGB(42, 46, 64);
                    bevel(cv, rc.x, rc.y, rc.w, rc.h, wait ? t.selHi : t.hi, wait ? t.selLo : t.lo, &f);
                    text(p, rc.x + 6, ry + 1, wait ? "press a button..." : name, wait ? t.selText : none ? t.faint : t.text);
                    if ((chosen && c == col && !wait) || (hover && padCursor)) frameRect(cv, rc.x - 1, rc.y - 1, rc.w + 2, rc.h + 2, t.accentHi);
                }
                if (hover && in.mousePressed[0] && waiting < 0) { row = a; col = c; waiting = a * 3 + c; focus = tableIdx; in.mousePressed[0] = false; }
            }
        }
        scrollbar(cv, gx + gw - 14, ry0 - 2, 10, rows * rowH, scroll, rows, A_COUNT);
        // a key for the waiting cell (Esc clears it; a pad cell takes only pad buttons, a key cell only keys)
        if (waiting >= 0 && active && in.lastKey != K_NONE) {
            int a = waiting / 3, c = waiting % 3; int k = in.lastKey;
            if (k == K_ESCAPE) k = K_NONE;
            if (k == K_NONE || isPadKey(k) == (c == 2)) { (c == 0 ? gKeys.key1[a] : c == 1 ? gKeys.key2[a] : gKeys.pad[a]) = (uint8_t)k; waiting = -1; in.eatKeys(); }
        }
        text(p, gx + (nat ? 2 : 152), bottom + (nat ? -2 : 6), nat ? fmt("Showing %d-%d of %d actions.", scroll + 1, std::min((int)A_COUNT, scroll + rows), (int)A_COUNT) : fmt("rows %d-%d of %d", scroll + 1, std::min((int)A_COUNT, scroll + rows), (int)A_COUNT), t.dim);
    }

    // ---- the button row ----
    {
        int by = bottom + (nat ? 8 : 4), bh = 24;
        if (!nat && !one) rule(cv, r.x + 8, bottom - 2, r.w - 16);
        if (one) for (int xx = r.x + 8; xx < r.x + r.w - 8; xx += 2) cv.put(xx, bottom - 2, t.dim);
        struct B { std::string label; int act; int state; };
        std::vector<B> left, right;
        std::string defLabel = page == 1 ? "Reset defaults" : "Defaults";
        if (!(inGame && page != 1)) left.push_back({defLabel, 9, BS_NORMAL});
        if (nat) { right.push_back({"OK", R_OK, BS_DEFAULT}); if (!inGame || true) right.push_back({"Cancel", R_CANCEL, BS_NORMAL}); right.push_back({"Apply", R_APPLY, BS_NORMAL}); }
        else { right.push_back({"Apply", R_APPLY, BS_NORMAL}); right.push_back({inGame ? "Done" : "OK", R_OK, BS_DEFAULT}); }
        int x = r.x + 8;
        auto doButton = [&](const B& b, int bx, int bw) {
            St st = ctl({bx, by, bw, bh}, 0);
            button(p, bx, by, bw, bh, b.label, b.state, st.hover);
            if (st.focused) { if (one) glow(cv, bx, by, bw, bh); else if (nat) glow(cv, bx + 4, by + 4, bw - 8, bh - 8); else frameRect(cv, bx - 2, by - 2, bw + 4, bh + 4, t.accentHi); }
            if (st.click || st.activate) {
                if (b.act == 9) {
                    if (page == 1) gKeys.reset();
                    else {
                        GameSettings d;
                        if (page == 0) {
                            bool fs = s.fullscreen != d.fullscreen, vs = s.vsync != d.vsync, is = s.integerScaling != d.integerScaling, sc = s.scale != d.scale || s.classic43 != d.classic43;
                            s.theme = d.theme; s.fullscreen = d.fullscreen; s.classic43 = d.classic43; s.integerScaling = d.integerScaling; s.crt = d.crt; s.vsync = d.vsync; s.nightClassic = d.nightClassic; s.scale = d.scale;
                            s.masterVolume = d.masterVolume; s.musicVolume = d.musicVolume; s.sfxVolume = d.sfxVolume; s.ambientVolume = d.ambientVolume;
                            s.gamepad = d.gamepad; s.screenShake = d.screenShake; s.fastCombat = d.fastCombat; s.interactWhileMoving = d.interactWhileMoving; s.edgeScroll = d.edgeScroll; s.banter = d.banter;
                            if (fs) reqFull(); if (vs) reqVsync(); if (is) reqInt(); if (sc) reqScale();
                        } else { s.largeText = d.largeText; s.textSpeed = d.textSpeed; s.colorblind = d.colorblind; s.reduceFlashing = d.reduceFlashing; s.screenShake = d.screenShake; s.fastCombat = d.fastCombat; }
                    }
                } else if (b.act == R_CANCEL) {
                    bool fs = s.fullscreen != orig.fullscreen, vs = s.vsync != orig.vsync, is = s.integerScaling != orig.integerScaling, sc = s.scale != orig.scale || s.classic43 != orig.classic43;
                    s = orig; gKeys = origKeys;
                    if (fs) reqFull(); if (vs) reqVsync(); if (is) reqInt(); if (sc) reqScale();
                    result = R_CANCEL;
                } else result = b.act;
            }
        };
        for (auto& b : left) { int bw = std::max(nat ? 100 : 104, textW(p, b.label) + 24); doButton(b, x, bw); x += bw + 8; }
        int rx = r.x + r.w - 8;
        std::vector<int> ws; for (auto& b : right) ws.push_back(nat ? 84 : 104);
        int total = 0; for (int w : ws) total += w + 8;
        int bx = rx - total + 8;
        for (size_t i = 0; i < right.size(); i++) { doButton(right[i], bx, ws[i]); bx += ws[i] + 8; }
        if (inGame && !nat && !one) text(p, r.x + 10, by + bh + 6, gPadGlyphs ? UI::bindLabel(A_PAGEUP) + "/" + UI::bindLabel(A_PAGEDOWN) + ": page   D-pad: move   " + UI::bindLabel(A_CONFIRM) + ": change   " + UI::bindLabel(A_CANCEL) + ": back to the game" : std::string("Tab: next page   Up/Down: row   Esc: back to the game"), t.dim);
    }
    // the open drop-down list, over everything drawn above
    if (dropList.open) {
        NoMap nm2; Color8 white2{255, 255, 255, 255}; int n = (int)dropList.opts.size(), x = dropList.x, ly = dropList.y, w = dropList.w, lh = dropList.lh;
        cv.fillRect(x, ly, w, n * lh + 2, white2); cv.rect(x, ly, w, n * lh + 2, t.lo);
        for (int i = 0; i < n; i++) {
            Rect orc{x + 1, ly + 1 + i * lh, w - 2, lh}; bool ho = orc.contains(in.mouseX, in.mouseY);
            bool on = i == dropList.sel;   // 1.8.6: the keys' / pad's highlight (the mouse moves it too)
            if (on) cv.fillRect(orc.x, orc.y, orc.w, orc.h, t.rowSel); else if (ho) cv.fillRect(orc.x, orc.y, orc.w, orc.h, RGB(216, 216, 232));
            text(p, x + 5, orc.y + 1, dropList.opts[i], on ? t.selText : t.text);
        }
    }
    // own navigation moves the focus through what was drawn (the Controls table is control 0 there)
    lastCount = idx;
    // 1.8.5 (#200) / 1.8.6 (#204): by place, grid-like (core/spatnav.h): Up / Down walk a column, Left / Right go to the
    // control at the same height (tabs, check boxes and buttons; choices and sliders keep Left / Right for their value),
    // the opposite press goes back where you came from; Up / Down past the last control wrap round
    if (keyNav && lastCount > 0 && focus >= 0 && focus < lastCount && (upP || downP || ((leftP || rightP) && (now[focus].kind == 0 || now[focus].kind == 1 || now[focus].kind == 5)))) {
        std::vector<Rect> rs; for (auto& c : now) rs.push_back(c.r);
        const int dx = leftP ? -1 : rightP ? 1 : 0, dy = upP ? -1 : downP ? 1 : 0;
        int best = SpatNav::move(rs, now[focus].r, dy ? 0 : dx, dy, nav, focus);
        if (best < 0 && (upP || downP)) {   // wrap round: the top-most (Down) or bottom-most (Up) control
            long bs = 0;
            for (int i = 0; i < lastCount; i++) { const Rect& c = now[i].r; long sc = (long)(downP ? c.y : -(c.y + c.h)) * 4096 + c.x; if (best < 0 || sc < bs) { best = i; bs = sc; } }
            nav = SpatNav::Memory();
        }
        if (best >= 0) { focus = best; gAudio.play("ui_move"); if (now[best].kind == 5 && (leftP || rightP) && best < 3) { page = best; } }
    }
    if (focus >= lastCount) focus = std::max(0, lastCount - 1);
    ctls = now;
    return result;
}

// ---------------- the in-game Settings screen (pause > Settings) ----------------
static SettingsPanel& gamePanel() { static SettingsPanel p; return p; }

void Game::openSettings() {
    SettingsPanel& sp = gamePanel(); sp.begin(*settings); sp.ownNav = true; sp.inGame = true;
    pushMode(GMode::SETTINGS);
}
// where the screen goes: the viewport (frame), a dialog owned by the game window (native), a window over the dimmed world (onebit)
static Rect settingsRect() {
    const int W = UI::SCREEN_W, H = UI::SCREEN_H;
    if (Theme::native()) { int w = std::min(W - 8, 660), h = std::min(H - 30, 468); return {(W - w) / 2, std::max(2, (H - 28 - h) / 2), w, h}; }
    if (Theme::onebit()) { int w = std::min(W - 16, 740), h = std::min(H - 30, 480); int x = W >= 900 ? W - w - 24 : (W - w) / 2; return {x, std::min(36, H - h - 4), w, h}; }   // beside the Start menu (the HUD dims the world)
    int colW = W >= 853 ? 236 : 200;
    if (W - colW - 2 < 600) return {0, 0, W, H};
    return {2, 2, W - colW - 6, H - 4};
}
std::string Game::settingsProbe(const std::string& what) {
    SettingsPanel& sp = gamePanel();
    if (what == "page") return std::to_string(sp.page);
    if (what == "focus") return std::to_string(sp.focus);
    if (what == "combo") return std::to_string(sp.openCombo);
    if (what == "combosel") return std::to_string(sp.comboSel);
    if (what == "theme") return std::to_string(settings ? settings->theme : -1);
    bool ok = sp.focus >= 0 && sp.focus < (int)sp.ctls.size();
    if (what == "kind") return ok ? std::to_string(sp.ctls[sp.focus].kind) : "-1";
    if (what == "x") return ok ? std::to_string(sp.ctls[sp.focus].r.x) : "-1";
    if (what == "y") return ok ? std::to_string(sp.ctls[sp.focus].r.y) : "-1";
    return "?";
}
void Game::updateSettingsScreen(InputState& in) {
    SettingsPanel& sp = gamePanel();
    if (sp.waiting < 0 && sp.openCombo < 0 && actP(in, A_CANCEL)) {   // Esc: back to the game, keeping the changes (they already apply)
        settings->save(saveDir + "/settings.ini"); popMode(); in.eatKeys(); gAudio.play("ui_cancel");
    }
}
void Game::drawSettingsScreen(Canvas& cv, InputState& in) {
    SettingsPanel& sp = gamePanel();
    Theme::NoMap nm; Pen p{&cv, ctx.font, ctx.ts};
    Rect r = settingsRect();
    uiRect(r.x, r.y, r.w, r.h);
    Rect inner = r;
    if (Theme::native()) { cv.dim(VIEW_X, VIEW_Y, VIEW_W, VIEW_H, 115); inner = Theme::windowChrome(p, r.x, r.y, r.w, r.h, "Settings", true); }
    else if (Theme::onebit()) { inner =Theme::windowChrome(p, r.x, r.y, r.w, r.h, "Settings", true); }
    else { Color8 deep{20, 22, 31, 255}; Theme::well(cv, r.x - 2, r.y - 2, r.w + 4, r.h + 4, &deep); Theme::panel(cv, r.x, r.y, r.w, r.h); inner = {r.x + 4, r.y + 2, r.w - 8, r.h - 4}; }
    int res = sp.draw(p, in, inner, *settings, req, true);
    if (res == SettingsPanel::R_APPLY) { settings->save(saveDir + "/settings.ini"); notify("Settings saved."); }
    else if (res == SettingsPanel::R_OK) { settings->save(saveDir + "/settings.ini"); popMode(); }
    else if (res == SettingsPanel::R_CANCEL) popMode();
}
