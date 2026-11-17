#include "ui.h"
#include "tiles.h"
#include "../core/theme.h"
#include <algorithm>

namespace UI {

// 1.8.3 (#170): every helper draws in the active UI theme (core/theme.h). The old navy/gold look is gone.
static Theme::Pen pen(Ctx& c) { return Theme::Pen{c.cv, c.font, c.ts}; }

void panel(Ctx& c, int x, int y, int w, int h, Color8 fill, Color8 border) {
    Canvas& cv = *c.cv;
    auto same = [](Color8 a, Color8 b) { return a.r == b.r && a.g == b.g && a.b == b.b; };
    if (same(fill, C::Panel) || same(fill, RGB(16, 16, 28))) { Theme::panel(cv, x, y, w, h); return; }
    if (same(fill, RGB(30, 30, 44)) || same(fill, C::PanelHi)) { Theme::well(cv, x, y, w, h); return; }
    // a panel with its own colours (the bounty board's cork, the world map's parchment): those colours, the theme's edge
    Theme::NoMap nm;
    cv.fillRect(x, y, w, h, fill);
    Theme::bevel(cv, x, y, w, h, RGB((uint8_t)std::min(255, border.r + 30), (uint8_t)std::min(255, border.g + 30), (uint8_t)std::min(255, border.b + 30)), RGB(border.r / 3, border.g / 3, border.b / 3));
    cv.rect(x + 2, y + 2, w - 4, h - 4, RGB(border.r / 2, border.g / 2, border.b / 2));
}

void panelTitle(Ctx& c, int x, int y, int w, int h, const std::string& title) {
    // 1.5.1 (#103): a title wider than the panel is shortened with ".." so it always sits inside its box
    Theme::Pen p = pen(c);
    std::string t = title;
    while (t.size() > 4 && Theme::titleW(p, t) > w - 30) t = t.substr(0, t.size() - 4) + "..";
    if (Theme::frame()) { Theme::panel(*c.cv, x, y, w, h); Theme::title(p, x + 10, y + 6, t); return; }
    Theme::toolWindow(p, x, y, w, h, t);
}

void bar(Ctx& c, int x, int y, int w, int h, int value, int maxValue, Color8 fill, Color8 back) {
    (void)back;
    auto same = [](Color8 a, Color8 b) { return a.r == b.r && a.g == b.g && a.b == b.b; };
    // the old HUD's green HP / blue MP bars take the theme's meaning colours
    if (same(fill, RGB(60, 200, 60)) || same(fill, RGB(60, 200, 90))) fill = Theme::hp();
    else if (same(fill, RGB(80, 120, 255))) fill = Theme::mp();
    Theme::bar(*c.cv, x, y, w, h, value, maxValue, fill);
}

void text(Ctx& c, int x, int y, const std::string& s, Color8 col, int scale) { c.font->draw(*c.cv, *c.ts, x, y, s, col, scale); }
void textCenter(Ctx& c, int cx, int y, const std::string& s, Color8 col, int scale) { c.font->draw(*c.cv, *c.ts, cx - c.font->width(s, scale) / 2, y, s, col, scale); }
void textRight(Ctx& c, int rx, int y, const std::string& s, Color8 col, int scale) { c.font->draw(*c.cv, *c.ts, rx - c.font->width(s, scale), y, s, col, scale); }
void textShadow(Ctx& c, int x, int y, const std::string& s, Color8 col, int scale) { c.font->drawShadow(*c.cv, *c.ts, x, y, s, col, Theme::native() && Theme::uiMapOn() ? Theme::th().hi : C::Black, scale); }
int wrapped(Ctx& c, int x, int y, int w, const std::string& s, Color8 col) { return c.font->drawWrapped(*c.cv, *c.ts, x, y, w, s, col); }

bool listRow(Ctx& c, const InputState& in, int x, int y, int w, int h, bool selected) {
    bool hover = in.mouseX >= x && in.mouseY >= y && in.mouseX < x + w && in.mouseY < y + h;
    if (selected) { Theme::listRow(pen(c), x, y, w, h, Theme::RS_SELECTED); Theme::invert({x, y, w, h}); selCursor(c, x, y, h); }
    else if (hover) Theme::listRow(pen(c), x, y, w, h, Theme::RS_HOVER);
    return hover;
}
void selCursor(Ctx& c, int x, int y, int h) {
    if (Theme::native()) return;   // 1.8.4 (#194): the desktop look marks a choice by the navy row / pressed button alone
    Theme::NoMap nm;
    bool bright = ((int)(c.time * 3)) % 2 == 0 && !Theme::reduceFlashing;
    const UiTheme& t = Theme::th();
    Color8 col = bright ? t.accentHi : t.accent;
    int cy = y + h / 2; int ax = x - 7;
    if (ax < 0) return;
    for (int i = 0; i < 4; i++) c.cv->vline(ax + i, cy - (3 - i), 2 * (3 - i) + 1, col);
}

bool button(Ctx& c, const InputState& in, int x, int y, int w, int h, const std::string& label, bool enabled, bool selected) {
    bool hover = enabled && in.mouseX >= x && in.mouseY >= y && in.mouseX < x + w && in.mouseY < y + h;
    Theme::button(pen(c), x, y, w, h, label, !enabled ? Theme::BS_DISABLED : selected ? Theme::BS_SELECTED : Theme::BS_NORMAL, hover);
    if (selected && enabled) selCursor(c, x, y, h);
    return hover && in.mousePressed[0];
}

void tabRow(Ctx& c, const std::vector<TabSpec>& tabs, int y, int h, int sel, int lineX0, int lineX1) {
    Theme::NoMap nm; const UiTheme& t = Theme::th(); Canvas& cv = *c.cv; Theme::Pen p = pen(c);
    if (tabs.empty()) return;
    auto label = [&](const TabSpec& s, int ty, int th, Color8 col) { std::string l = Theme::fit(p, s.label, s.w - 6); Theme::text(p, s.x + (s.w - Theme::textW(p, l)) / 2, ty + (th - 12) / 2, l, col); };
    if (Theme::native()) {
        if (lineX0 < 0) lineX0 = tabs.front().x; if (lineX1 < 0) lineX1 = tabs.back().x + tabs.back().w;
        cv.fillRect(lineX0, y + h, lineX1 - lineX0, 1, t.hi);   // the sheet's top edge; the open tab joins it
        for (int pass = 0; pass < 2; pass++) for (int i = 0; i < (int)tabs.size(); i++) {
            const TabSpec& s = tabs[i]; bool on = i == sel; if (on != (pass == 1)) continue;   // the open tab last, over its neighbours
            int tx = on ? s.x - 2 : s.x, tw = on ? s.w + 4 : s.w, ty = on ? y : y + 2, th = on ? h + 1 : h - 2;
            cv.fillRect(tx, ty, tw, th, t.face);
            cv.fillRect(tx + 1, ty, tw - 2, 1, t.hi); cv.fillRect(tx, ty + 1, 1, th - 1, t.hi);
            cv.fillRect(tx + tw - 1, ty + 1, 1, th - 1, t.lo); cv.fillRect(tx + tw - 2, ty + 1, 1, th - 1, t.lo2);
            label(s, ty, on ? h : h - 2, t.text);
        }
        return;
    }
    for (int i = 0; i < (int)tabs.size(); i++) {
        const TabSpec& s = tabs[i]; bool on = i == sel;
        if (Theme::onebit()) {
            if (on) { cv.fillRect(s.x, y, s.w, h, t.text); label(s, y, h, t.back); }
            else { cv.fillRect(s.x, y, s.w, h, t.back); cv.rect(s.x, y, s.w, h, t.dim); label(s, y, h, t.text); }
            continue;
        }
        Color8 face = on ? t.selFill : t.face;
        Theme::bevel(cv, s.x, y, s.w, h, on ? t.selHi : t.hi, on ? t.selLo : t.lo, &face);
        label(s, y, h, on ? t.selText : t.text);
    }
}
BoardLook boardLook() {
    BoardLook L; const UiTheme& t = Theme::th();
    if (Theme::native()) {
        L.cork = false; L.head = t.text; L.sub = RGB(64, 64, 64); L.note = RGB(64, 64, 64); L.ink = t.text; L.body = RGB(32, 32, 32); L.soft = RGB(64, 64, 64);
        L.good = RGB(0, 110, 0); L.bad = RGB(176, 0, 0); L.warn = RGB(160, 64, 0); L.reward = RGB(0, 0, 128);
        L.card = RGB(255, 255, 255); L.cardSel = RGB(0, 0, 128); L.cardShut = RGB(223, 223, 223); L.edge = t.lo2; L.edgeSel = t.lo; L.selText = RGB(255, 255, 255);
        return L;
    }
    if (Theme::onebit()) {
        L.cork = false; L.head = t.text; L.sub = t.dim; L.note = t.dim; L.ink = t.text; L.body = t.text; L.soft = t.dim;
        L.good = Theme::good(); L.bad = Theme::foe(); L.warn = t.accent; L.reward = t.accent;
        L.card = t.back; L.cardSel = t.back; L.cardShut = t.back; L.edge = t.dim; L.edgeSel = t.accent; L.selText = t.text;
        return L;
    }
    L.head = C::Cream; L.sub = RGB(220, 200, 160); L.note = RGB(170, 150, 115); L.ink = RGB(40, 25, 10); L.body = RGB(60, 45, 25); L.soft = RGB(70, 50, 30);
    L.good = RGB(20, 120, 40); L.bad = RGB(150, 40, 30); L.warn = RGB(120, 60, 20); L.reward = RGB(120, 80, 10);
    L.card = RGB(225, 215, 180); L.cardSel = RGB(250, 240, 205); L.cardShut = RGB(195, 190, 170); L.edge = RGB(120, 100, 60); L.edgeSel = C::Gold; L.selText = L.ink;
    return L;
}
void boardPanel(Ctx& c, int x, int y, int w, int h) {
    if (!Theme::native() && !Theme::onebit()) { panel(c, x, y, w, h, RGB(70, 50, 30), RGB(160, 120, 60)); return; }
    Theme::NoMap nm; Theme::panel(*c.cv, x, y, w, h);
}
void boardCard(Ctx& c, int x, int y, int w, int h, bool sel, bool open) {
    BoardLook L = boardLook(); Canvas& cv = *c.cv;
    if (L.cork) { cv.fillRect(x, y, w, h, sel ? L.cardSel : open ? L.card : L.cardShut); cv.rect(x, y, w, h, sel ? L.edgeSel : L.edge); return; }
    Theme::NoMap nm;
    if (Theme::native()) {
        if (sel) { cv.fillRect(x, y, w, h, L.cardSel); cv.rect(x, y, w, h, L.edgeSel); }
        else { Color8 f = open ? L.card : L.cardShut; Theme::well(cv, x, y, w, h, &f); }
        return;
    }
    cv.fillRect(x, y, w, h, L.card);
    if (sel) Theme::glow(cv, x, y, w, h); else cv.rect(x, y, w, h, open ? L.edge : Theme::th().pipOff);
}
Color8 boardInk(const BoardLook& L, bool sel, Color8 col) { return sel && Theme::native() ? L.selText : col; }

void chip(Ctx& c, int x, int y, int w, int h, const std::string& lab, bool on) {
    Theme::NoMap nm; const UiTheme& t = Theme::th(); Canvas& cv = *c.cv; Theme::Pen p = pen(c);
    if (Theme::native()) {
        if (on) { Color8 f = RGB(223, 223, 223); Theme::bevel(cv, x, y, w, h, t.lo2, t.hi, &f); }
        Theme::text(p, x + 5 + (on ? 1 : 0), y + (h - 12) / 2 + (on ? 1 : 0), lab, t.text);
        return;
    }
    if (Theme::onebit()) {
        if (on) { cv.fillRect(x, y, w, h, t.text); Theme::text(p, x + 5, y + (h - 12) / 2, lab, t.back); }
        else { cv.rect(x, y, w, h, t.pipOff); Theme::text(p, x + 5, y + (h - 12) / 2, lab, t.dim); }
        return;
    }
    Color8 f = on ? t.selFill : t.face;
    cv.fillRect(x, y, w, h, f); cv.rect(x, y, w, h, on ? t.accent : t.hi2);
    Theme::text(p, x + 5, y + (h - 12) / 2, lab, on ? t.selText : t.dim);
}

void icon(Ctx& c, Tile t, int x, int y, int scale, Color8 tint) { c.ts->draw(*c.cv, t, x, y, scale, tint); }

// 1.8.3 (#170): a key name in the theme's keycap look (14 px high, as the layouts expect)
static void keyBox(Ctx& c, int x, int y, int kw, const std::string& key) {
    Theme::NoMap nm; const UiTheme& t = Theme::th(); Canvas& cv = *c.cv;
    if (Theme::native()) { Color8 f = t.face; Theme::bevel(cv, x, y, kw, 14, t.hi, t.lo, &f); c.font->draw(cv, *c.ts, x + 3, y + 1, key, t.text); return; }
    if (Theme::onebit()) { cv.fillRect(x, y, kw, 14, t.back); cv.rect(x, y, kw, 14, t.text); c.font->draw(cv, *c.ts, x + 3, y + 1, key, t.text); return; }
    Color8 f = t.face; Theme::bevel(cv, x, y, kw, 14, t.hi, t.lo, &f); c.font->draw(cv, *c.ts, x + 3, y + 1, key, t.text);
}

// Controller glyphs: when a gamepad is active, keyboard names are swapped for the bound pad button and drawn as a button glyph.
static std::string padEquivalent(const std::string& key) {
    static const std::pair<const char*, const char*> table[] = {{"Enter", "Pad A"}, {"Space", "Pad A"}, {"Esc", "Pad B"}, {"Tab", "Start"}, {"Tab/I", "Start"}, {"L/R", "DPad L/R"}, {"Left/Right", "DPad L/R"}, {"Left", "DPad Left"}, {"Right", "DPad Right"}, {"PgUp/PgDn", "LB/RB"}, {"Arrows", "DPad"}, {"Up/Down", "DPad"}, {"1-8", "1-8"}, {"F5", "F5"}};
    for (auto& t : table) if (key == t.first) return t.second;
    for (int a = 0; a < A_COUNT; a++) { std::string l = gKeys.key1[a] ? keyName(gKeys.key1[a]) : ""; if (gKeys.key2[a]) l += std::string(l.empty() ? "" : "/") + keyName(gKeys.key2[a]); if (l == key && gKeys.pad[a]) return keyName(gKeys.pad[a]); }
    return key;
}
static int drawPadGlyph(Ctx& c, int x, int y, const std::string& name) {
    // face buttons: coloured circle with the letter; shoulders/triggers: small rounded box; others: boxed text
    auto circle = [&](Color8 col, const char* letter) { Canvas& cv = *c.cv; for (int dy = -6; dy <= 6; dy++) for (int dx = -6; dx <= 6; dx++) if (dx * dx + dy * dy <= 40) cv.put(x + 7 + dx, y + 7 + dy, col); for (int dy = -7; dy <= 7; dy++) for (int dx = -7; dx <= 7; dx++) { int d = dx * dx + dy * dy; if (d > 40 && d <= 52) cv.put(x + 7 + dx, y + 7 + dy, C::White); } c.font->draw(cv, *c.ts, x + 4, y + 1, letter, C::Black); return 16; };
    if (name == "Pad A") return circle(RGB(90, 200, 90), "A");
    if (name == "Pad B") return circle(RGB(220, 80, 80), "B");
    if (name == "Pad X") return circle(RGB(90, 140, 240), "X");
    if (name == "Pad Y") return circle(RGB(240, 200, 60), "Y");
    std::string txt = name; if (startsWith(name, "DPad")) txt = "D-pad" + (name.size() > 4 ? name.substr(4) : "");
    int kw = c.font->width(txt) + 6;
    bool shoulder = name == "LB" || name == "RB" || name == "LT" || name == "RT" || name == "LB/RB";
    c.cv->fillRect(x, y + (shoulder ? 2 : 0), kw, shoulder ? 11 : 14, shoulder ? RGB(70, 70, 80) : C::LtGray);
    c.cv->rect(x, y + (shoulder ? 2 : 0), kw, shoulder ? 11 : 14, C::White);
    c.font->draw(*c.cv, *c.ts, x + 3, y + 1, txt, shoulder ? C::White : C::Black);
    return kw;
}
int keyChip(Ctx& c, int x, int y, int action, bool draw) {
    if (action < 0 || action >= A_COUNT) return 0;
    if (gPadGlyphs && gKeys.pad[action]) {
        std::string name = keyName(gKeys.pad[action]);
        if (!draw) { bool face = name == "Pad A" || name == "Pad B" || name == "Pad X" || name == "Pad Y"; std::string txt = startsWith(name, "DPad") ? "D-pad" + name.substr(4) : name; return face ? 16 : c.font->width(txt) + 6; }
        return drawPadGlyph(c, x, y, name);
    }
    if (gPadGlyphs || !gKeys.key1[action]) return 0;   // on a controller, an action with no pad button shows no key
    std::string key = keyName(gKeys.key1[action]); int kw = c.font->width(key) + 6;
    if (draw) keyBox(c, x, y, kw, key);
    return kw;
}
int keyHint(Ctx& c, int x, int y, const std::string& keyIn, const std::string& label) {
    std::string key = gPadGlyphs ? padEquivalent(keyIn) : keyIn;
    int kw;
    if (gPadGlyphs && key != keyIn) kw = drawPadGlyph(c, x, y, key);
    else if (gPadGlyphs && startsWith(key, "Pad ")) kw = drawPadGlyph(c, x, y, key);
    else {
        kw = c.font->width(key) + 6;
        keyBox(c, x, y, kw, key);
    }
    { Theme::NoMap nm; c.font->draw(*c.cv, *c.ts, x + kw + 4, y + 1, label, Theme::th().dim); }
    return x + kw + 4 + c.font->width(label) + 14;   // where the next hint can start
}

bool gColorblind = false;
Color8 friendColor() { return gColorblind ? RGB(90, 170, 255) : Theme::th().id == ThemeId::Native ? RGB(60, 200, 90) : Theme::th().good; }
Color8 foeColor() { return gColorblind ? RGB(255, 150, 20) : Theme::th().id == ThemeId::Native ? RGB(220, 40, 40) : Theme::th().foeC; }
Color8 allyColor() { return gColorblind ? RGB(200, 200, 255) : RGB(80, 170, 255); }
Color8 friendBack() { return gColorblind ? RGB(0, 25, 70, 190) : RGB(0, 50, 20, 190); }
Color8 foeBack() { return gColorblind ? RGB(80, 40, 0, 190) : RGB(70, 0, 0, 190); }
Color8 hitColor(int pct) { if (gColorblind) return pct >= 70 ? RGB(110, 190, 255) : pct >= 40 ? RGB(240, 228, 66) : RGB(255, 120, 30); return pct >= 70 ? RGB(110, 230, 110) : pct >= 40 ? RGB(255, 220, 60) : RGB(255, 90, 70); }
Color8 coverColor(int level) { if (gColorblind) return level >= 2 ? RGB(110, 190, 255) : RGB(240, 228, 66); return level >= 2 ? RGB(80, 160, 255) : RGB(255, 220, 60); }
Color8 dmgColor(int t) {
    switch (t) { case DT_FIRE: return C::Orange; case DT_COLD: return C::Cyan; case DT_SHOCK: return C::Yellow; case DT_POISON: return C::Green; case DT_MAGIC: return C::Purple; default: return C::White; }
}

std::string itemLabel(const ItemDef& d, int count) { return count > 1 ? d.name + " x" + std::to_string(count) : d.name; }

}
