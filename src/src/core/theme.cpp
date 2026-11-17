// 1.8.3 (#170): the three UI themes and their drawing primitives (see theme.h).
#include "theme.h"
#include "util.h"
#include <algorithm>
#include <cmath>

namespace {
constexpr Color8 H(uint32_t v) { return Color8{(uint8_t)(v >> 16), (uint8_t)(v >> 8), (uint8_t)v, 255}; }

UiTheme makeFrame() {
    UiTheme t; t.id = ThemeId::Frame; t.key = "frame"; t.name = "Adventurer's Frame";
    t.back = H(0x0C0D12); t.panel = H(0x2B2F3E); t.hi = H(0x6E7591); t.lo = H(0x14161F); t.hi2 = H(0x4A5068); t.lo2 = H(0x1E2130);
    t.well = H(0x191B25); t.face = H(0x3A3F54); t.rowSel = H(0x3A3F54);
    t.selFill = H(0x5A4520); t.selHi = H(0xF2CE7A); t.selLo = H(0x7A5A1E); t.keyCell = H(0x22263A);
    t.text = H(0xE9E3D3); t.dim = H(0x9A96A6); t.faint = H(0x6A6E80); t.selText = H(0xFFF3D6); t.accent = H(0xD9A441); t.accentHi = H(0xF2CE7A); t.title = H(0xD9A441);
    t.hp = H(0xC9473D); t.mp = H(0x4F86D9); t.move = H(0x7FA8D9); t.good = H(0x7FC36A); t.foeC = H(0xC9473D); t.achieve = H(0xD9A441); t.gold = H(0xD9A441);
    t.barBack = H(0x0E1018); t.pipOff = H(0x2A2E40); t.info = H(0x7FA8D9); t.tooltip = H(0x2B2F3E); t.tooltipText = H(0xE9E3D3);
    return t;
}
UiTheme makeNative() {
    UiTheme t; t.id = ThemeId::Native; t.key = "native"; t.name = "Desktop-native";
    t.back = H(0xC0C0C0); t.panel = H(0xC0C0C0); t.hi = H(0xFFFFFF); t.lo = H(0x000000); t.hi2 = H(0xDFDFDF); t.lo2 = H(0x808080);
    t.well = H(0xFFFFFF); t.face = H(0xC0C0C0); t.rowSel = H(0x000080);
    t.selFill = H(0x000080); t.selHi = H(0xFFFFFF); t.selLo = H(0x000000); t.keyCell = H(0xC0C0C0);
    t.text = H(0x000000); t.dim = H(0x808080); t.faint = H(0x808080); t.selText = H(0xFFFFFF); t.accent = H(0x000080); t.accentHi = H(0x1084D0); t.title = H(0x000000);
    t.hp = H(0xC00000); t.mp = H(0x0000C0); t.move = H(0x0000C0); t.good = H(0x006400); t.foeC = H(0xC00000); t.achieve = H(0x7A3FB0); t.gold = H(0x000000);
    t.barBack = H(0xFFFFFF); t.pipOff = H(0xFFFFFF); t.info = H(0x008080); t.tooltip = H(0xFFFFE1); t.tooltipText = H(0x000000);
    return t;
}
UiTheme makeOneBit() {
    UiTheme t; t.id = ThemeId::OneBit; t.key = "onebit"; t.name = "One-Bit Desktop";
    t.back = H(0x000000); t.panel = H(0x000000); t.hi = H(0xEDE8DA); t.lo = H(0x000000); t.hi2 = H(0x6E6B78); t.lo2 = H(0x000000);
    t.well = H(0x000000); t.face = H(0x000000); t.rowSel = H(0x000000);
    t.selFill = H(0xF2A33A); t.selHi = H(0xF2A33A); t.selLo = H(0x7A4E14); t.keyCell = H(0x000000);
    t.text = H(0xEDE8DA); t.dim = H(0x6E6B78); t.faint = H(0x6E6B78); t.selText = H(0xF2A33A); t.accent = H(0xF2A33A); t.accentHi = H(0xF2A33A); t.title = H(0xEDE8DA);
    t.hp = H(0xE04A3A); t.mp = H(0x9B6CF2); t.move = H(0xEDE8DA); t.good = H(0x8BD06A); t.foeC = H(0xE04A3A); t.achieve = H(0xF2A33A); t.gold = H(0xF2A33A);
    t.barBack = H(0x24232A); t.pipOff = H(0x24232A); t.info = H(0x9B6CF2); t.tooltip = H(0x000000); t.tooltipText = H(0xEDE8DA);
    return t;
}
const UiTheme kThemes[3] = {makeFrame(), makeNative(), makeOneBit()};
ThemeId gId = ThemeId::Frame;
int gMapDepth = 0;

inline int lum(Color8 c) { return (c.r * 77 + c.g * 150 + c.b * 29) >> 8; }
inline int sat(Color8 c) { return std::max({c.r, c.g, c.b}) - std::min({c.r, c.g, c.b}); }
inline bool same(Color8 a, Color8 b) { return a.r == b.r && a.g == b.g && a.b == b.b; }
inline Color8 withA(Color8 c, uint8_t a) { c.a = a; return c; }

std::vector<Rect> gInverted;   // this frame's selected rows / inverted boxes: text drawn in them takes the selection's text colour
Color8 mapHook(Color8 c, int kind, int x, int y) {
    if (kind == 1) { for (auto& r : gInverted) if (r.contains(x + 1, y + 6)) return Color8{Theme::th().selText.r, Theme::th().selText.g, Theme::th().selText.b, c.a}; return Theme::mapText(c); }
    return Theme::mapFill(c);
}
}

namespace Theme {
bool colorblind = false, reduceFlashing = false;
float time = 0;
const UiTheme& th() { return kThemes[(int)gId]; }
ThemeId id() { return gId; }
void set(ThemeId t) { if ((int)t < 0 || (int)t >= 3) t = ThemeId::Frame; gId = t; }
ThemeId fromKey(const std::string& k0) {
    std::string k = toLower(trim(k0));
    if (k == "native" || k == "desktop-native" || k == "1") return ThemeId::Native;
    if (k == "onebit" || k == "one-bit" || k == "2") return ThemeId::OneBit;
    return ThemeId::Frame;
}
const char* key(ThemeId t) { return kThemes[std::clamp((int)t, 0, 2)].key; }
const char* name(ThemeId t) { return kThemes[std::clamp((int)t, 0, 2)].name; }
Color8 hp() { return th().hp; }
Color8 mp() { return th().mp; }
Color8 good() { return colorblind ? RGB(90, 170, 255) : th().good; }
Color8 foe() { return colorblind ? RGB(255, 150, 20) : th().foeC; }
Color8 friendly() { return colorblind ? RGB(90, 170, 255) : th().good; }

// ---------------- the UI colour map ----------------
void beginUiMap() { if (gMapDepth++ == 0) gCanvasMap = mapHook; }
void endUiMap() { if (gMapDepth > 0 && --gMapDepth == 0) gCanvasMap = nullptr; }
bool uiMapOn() { return gCanvasMap != nullptr; }
void beginFrame() { gInverted.clear(); }
void invert(Rect r) { if (gId != ThemeId::Frame && gInverted.size() < 64) gInverted.push_back(r); }
NoMap::NoMap() : was(gCanvasMap != nullptr) { gCanvasMap = nullptr; }
NoMap::~NoMap() { if (was) gCanvasMap = mapHook; }
YesMap::YesMap() : was(gCanvasMap) { gCanvasMap = mapHook; }
YesMap::~YesMap() { gCanvasMap = was; }

// The old in-game UI's fixed colours, by role. Each theme says what they become.
Color8 mapFill(Color8 c) {
    const UiTheme& t = th();
    const uint8_t a = c.a;
    if (same(c, C::Panel) || same(c, RGB(18, 16, 24)) || same(c, RGB(30, 28, 40)) || same(c, RGB(20, 20, 28)) || same(c, RGB(26, 26, 36))) return withA(t.panel, a);
    if (same(c, C::PanelHi) || same(c, RGB(50, 50, 66)) || same(c, RGB(60, 60, 80)) || same(c, RGB(40, 40, 54)) || same(c, RGB(50, 50, 70))) return withA(t.face, a);
    // one-bit: an old screen's brass "chosen" fill becomes a deep amber well, so its amber (was yellow) text still reads
    if (same(c, RGB(90, 70, 20)) || same(c, RGB(100, 80, 30)) || same(c, RGB(120, 90, 30)) || same(c, RGB(150, 110, 30))) return withA(t.id == ThemeId::OneBit ? H(0x3E280B) : t.selFill, a);
    if (same(c, C::Gold) || same(c, RGB(120, 90, 30))) return withA(t.accent, a);
    if (same(c, C::Yellow)) return withA(t.accentHi, a);
    if (t.id == ThemeId::Native) {
        // a light theme: dark backdrops become the window face, black see-through label boxes become tooltips
        if (c.a < 255 && lum(c) < 40 && sat(c) < 40) return t.tooltip;
        if (lum(c) < 70 && sat(c) < 50) return withA(t.panel, a);
        if (same(c, C::LtGray) || same(c, C::White)) return withA(t.hi2, a);
        if (same(c, C::DkGray) || same(c, C::Gray)) return withA(t.lo2, a);
        return c;
    }
    if (t.id == ThemeId::OneBit) {
        if (lum(c) < 70 && sat(c) < 50) return withA(H(0x000000), a);
        if (same(c, C::LtGray) || same(c, C::White) || same(c, C::Cream)) return withA(t.text, a);
        if (same(c, C::DkGray) || same(c, C::Gray)) return withA(t.dim, a);
        return c;
    }
    if (same(c, C::DkGray)) return withA(t.hi2, a);
    return c;
}
Color8 mapText(Color8 c) {
    const UiTheme& t = th();
    const uint8_t a = c.a;
    if (same(c, C::White) || same(c, C::Cream) || same(c, RGB(230, 230, 230)) || same(c, RGB(220, 220, 220))) return withA(t.text, a);
    if (same(c, C::LtGray) || same(c, RGB(200, 200, 200)) || same(c, RGB(180, 180, 180))) return withA(t.id == ThemeId::Native ? H(0x202020) : t.dim, a);
    if (same(c, C::Gray) || same(c, C::DkGray) || same(c, RGB(110, 110, 110)) || same(c, RGB(140, 140, 140))) return withA(t.faint, a);
    if (same(c, C::Gold)) return withA(t.accent, a);
    if (same(c, C::Yellow) || same(c, RGB(255, 225, 140))) return withA(t.id == ThemeId::Native ? H(0x000080) : t.accentHi, a);
    if (t.id == ThemeId::Native) {
        if (same(c, C::Cyan)) return withA(H(0x008080), a);
        if (same(c, C::Green) || same(c, RGB(60, 200, 60)) || same(c, RGB(110, 230, 110))) return withA(t.good, a);
        if (same(c, C::Orange)) return withA(H(0xA04000), a);
        if (same(c, C::Purple)) return withA(t.achieve, a);
        if (lum(c) > 170) return withA(t.text, a);   // anything else pale would vanish on grey
        return c;
    }
    if (t.id == ThemeId::OneBit) {
        if (same(c, C::Cyan)) return withA(t.mp, a);
        if (same(c, C::Green) || same(c, RGB(60, 200, 60))) return withA(t.good, a);
        if (same(c, C::Red)) return withA(t.hp, a);
        return c;
    }
    if (same(c, C::Green) || same(c, RGB(60, 200, 60))) return withA(t.good, a);
    return c;
}

// ---------------- text ----------------
int text(const Pen& p, int x, int y, const std::string& s, Color8 col, int scale) { return p.font->draw(*p.cv, *p.ts, x, y, s, col, scale); }
int textW(const Pen& p, const std::string& s, int scale) { return p.font->width(s, scale); }
int small(const Pen& p, int x, int y, const std::string& s, Color8 col, int scale) { return p.font->drawSmall(*p.cv, x, y, s, col, scale); }
int smallW(const Pen& p, const std::string& s, int scale) { return p.font->widthSmall(s, scale); }
int title(const Pen& p, int x, int y, const std::string& s, int scale, Color8* col) {
    const UiTheme& t = th();
    if (t.id == ThemeId::Native) {
        Font& f = const_cast<Font&>(*p.font); bool b = f.bold; f.bold = true;
        int w = f.draw(*p.cv, *p.ts, x, y, s, col ? *col : t.title, scale); f.bold = b; return w;
    }
    if (t.id == ThemeId::OneBit) return p.font->drawClassic(*p.cv, *p.ts, x, y, s, col ? *col : t.title, false, scale);
    p.font->drawClassic(*p.cv, *p.ts, x + scale, y + scale, s, t.lo, true, scale);
    return p.font->drawClassic(*p.cv, *p.ts, x, y, s, col ? *col : t.title, true, scale);
}
int titleW(const Pen& p, const std::string& s, int scale) {
    if (native()) { Font& f = const_cast<Font&>(*p.font); bool b = f.bold; f.bold = true; int w = f.width(s, scale); f.bold = b; return w; }
    return p.font->widthClassic(s, frame(), scale);
}
int textRight(const Pen& p, int rx, int y, const std::string& s, Color8 col) { return text(p, rx - textW(p, s), y, s, col); }
int smallRight(const Pen& p, int rx, int y, const std::string& s, Color8 col) { return small(p, rx - smallW(p, s), y, s, col); }
std::string fit(const Pen& p, const std::string& s0, int w) {
    if (textW(p, s0) <= w) return s0;
    std::string s = s0; while (!s.empty() && textW(p, s + "..") > w) s.pop_back();
    return s + "..";
}

// ---------------- primitives ----------------
void bevel(Canvas& cv, int x, int y, int w, int h, Color8 hi, Color8 lo, const Color8* fill) {
    NoMap nm;
    if (fill) cv.fillRect(x, y, w, h, *fill);
    cv.fillRect(x, y, w, 1, hi); cv.fillRect(x, y, 1, h, hi);
    cv.fillRect(x, y + h - 1, w, 1, lo); cv.fillRect(x + w - 1, y, 1, h, lo);
}
void frameRect(Canvas& cv, int x, int y, int w, int h, Color8 c) { NoMap nm; cv.rect(x, y, w, h, c); }
void dither(Canvas& cv, int x, int y, int w, int h, Color8 c, int step) {
    NoMap nm;
    for (int yy = y; yy < y + h; yy++) for (int xx = x + ((yy - y) % 2); xx < x + w; xx += step) cv.put(xx, yy, c);
}

void panel(Canvas& cv, int x, int y, int w, int h) {
    NoMap nm; const UiTheme& t = th();
    if (t.id == ThemeId::Native) {
        cv.fillRect(x, y, w, h, t.panel);
        bevel(cv, x, y, w, h, t.hi2, t.lo); bevel(cv, x + 1, y + 1, w - 2, h - 2, t.hi, t.lo2);
        return;
    }
    if (t.id == ThemeId::OneBit) {
        cv.fillRect(x, y, w, h, t.panel); cv.rect(x, y, w, h, t.hi);
        return;
    }
    cv.fillRect(x, y, w, h, t.panel);
    bevel(cv, x, y, w, h, t.hi, t.lo); bevel(cv, x + 1, y + 1, w - 2, h - 2, t.hi2, t.lo2);
    if (w >= 12 && h >= 12) { cv.fillRect(x + 3, y + 3, 2, 2, t.accent); cv.fillRect(x + w - 5, y + 3, 2, 2, t.accent); cv.fillRect(x + 3, y + h - 5, 2, 2, t.accent); cv.fillRect(x + w - 5, y + h - 5, 2, 2, t.accent); }
}
void panelTitled(const Pen& p, int x, int y, int w, int h, const std::string& tt, const std::string& right) {
    NoMap nm; const UiTheme& t = th(); Canvas& cv = *p.cv;
    if (t.id == ThemeId::Native) {
        // a group box: an etched frame with the title on its top edge
        int ty = y + 6; cv.rect(x, ty, w - 1, h - 7, t.lo2); cv.rect(x + 1, ty + 1, w - 1, h - 7, t.hi);
        int tw = textW(p, tt) + 6; cv.fillRect(x + 7, y, tw, 13, t.panel); text(p, x + 10, y, tt, t.text);
        if (!right.empty()) { int rw = textW(p, right) + 6; cv.fillRect(x + w - 10 - rw, y, rw, 13, t.panel); textRight(p, x + w - 13, y, right, t.dim); }
        return;
    }
    if (t.id == ThemeId::OneBit) {
        title(p, x + 2, y, tt);
        int tw = titleW(p, tt) + 10;
        for (int xx = x + 2 + tw; xx < x + w - (right.empty() ? 0 : textW(p, right) + 10); xx += 2) cv.put(xx, y + 7, t.dim);
        if (!right.empty()) textRight(p, x + w, y, right, t.dim);
        return;
    }
    panel(cv, x, y, w, h);
    title(p, x + 10, y + 7, tt);
    if (!right.empty()) textRight(p, x + w - 12, y + 8, right, t.dim);
}
void well(Canvas& cv, int x, int y, int w, int h, const Color8* fill) {
    NoMap nm; const UiTheme& t = th(); Color8 f = fill ? *fill : t.well;
    if (t.id == ThemeId::Native) { cv.fillRect(x, y, w, h, f); bevel(cv, x, y, w, h, t.lo2, t.hi); bevel(cv, x + 1, y + 1, w - 2, h - 2, t.lo, t.hi2); return; }
    if (t.id == ThemeId::OneBit) { cv.fillRect(x, y, w, h, f); cv.rect(x, y, w, h, t.dim); return; }
    bevel(cv, x, y, w, h, t.lo, t.hi, &f);
}
void rule(Canvas& cv, int x, int y, int w) {
    NoMap nm; const UiTheme& t = th();
    if (t.id == ThemeId::OneBit) { for (int xx = x; xx < x + w; xx += 2) cv.put(xx, y, t.dim); return; }
    if (t.id == ThemeId::Native) { cv.fillRect(x, y, w, 1, t.lo2); cv.fillRect(x, y + 1, w, 1, t.hi); return; }
    cv.fillRect(x, y, w, 1, t.lo); cv.fillRect(x, y + 1, w, 1, t.hi);
}
void glow(Canvas& cv, int x, int y, int w, int h) {
    NoMap nm; const UiTheme& t = th();
    if (t.id == ThemeId::OneBit) {
        // amber edge with a 50% dither ring (every other pixel) in the 3 px outside it
        for (int r = 1; r <= 3; r++) for (int xx = x - r; xx < x + w + r; xx++) for (int yy : {y - r, y + h - 1 + r}) if (((xx + yy) & 1) == 0) cv.put(xx, yy, t.selLo);
        for (int r = 1; r <= 3; r++) for (int yy = y - r + 1; yy < y + h + r - 1; yy++) for (int xx : {x - r, x + w - 1 + r}) if (((xx + yy) & 1) == 0) cv.put(xx, yy, t.selLo);
        cv.rect(x, y, w, h, t.accent); return;
    }
    if (t.id == ThemeId::Native) { for (int xx = x; xx < x + w; xx += 2) { cv.put(xx, y, t.lo); cv.put(xx, y + h - 1, t.lo); } for (int yy = y; yy < y + h; yy += 2) { cv.put(x, yy, t.lo); cv.put(x + w - 1, yy, t.lo); } return; }
    cv.rect(x, y, w, h, t.accent);
}

void button(const Pen& p, int x, int y, int w, int h, const std::string& label, int state, bool hover, const std::string& keyRight, const std::string& keyLeft, const std::string& sub) {
    NoMap nm; const UiTheme& t = th(); Canvas& cv = *p.cv;
    bool sel = state == BS_SELECTED, dis = state == BS_DISABLED, def = state == BS_DEFAULT;
    int ty = sub.empty() ? y + (h - 12) / 2 : y + 4;
    Color8 tc = t.text;
    if (t.id == ThemeId::Native) {
        cv.fillRect(x, y, w, h, sel ? H(0xDCDCDC) : t.face);
        if (def) { cv.rect(x, y, w, h, t.lo); x++; y++; w -= 2; h -= 2; }
        if (sel) { bevel(cv, x, y, w, h, t.lo, t.hi); bevel(cv, x + 1, y + 1, w - 2, h - 2, t.lo2, t.hi2); }
        else { bevel(cv, x, y, w, h, t.hi, t.lo); bevel(cv, x + 1, y + 1, w - 2, h - 2, t.hi2, t.lo2); }
        int o = sel ? 1 : 0; int lx = keyLeft.empty() ? x + (w - textW(p, label)) / 2 : x + 22;
        if (!keyLeft.empty()) { small(p, x + 6 + o, y + (h - 8) / 2 + o, keyLeft, dis ? t.dim : t.text); }
        if (dis) { text(p, lx + 1, ty + 1, label, t.hi); text(p, lx, ty, label, t.dim); }
        else text(p, lx + o, ty + o, label, t.text);
        if (!keyRight.empty()) smallRight(p, x + w - 6 + o, y + (h - 8) / 2 + o, keyRight, dis ? t.dim : t.text);
        if (!sub.empty()) small(p, lx + o, y + h - 11 + o, sub, t.dim);
        if (hover && !dis && !sel) { for (int xx = x + 3; xx < x + w - 3; xx += 2) { cv.put(xx, y + 3, t.lo2); cv.put(xx, y + h - 4, t.lo2); } }
        return;
    }
    if (t.id == ThemeId::OneBit) {
        cv.fillRect(x, y, w, h, t.panel);
        if (sel) { glow(cv, x, y, w, h); tc = t.accent; }
        else cv.rect(x, y, w, h, dis ? t.pipOff : (hover ? t.accent : t.text));
        if (def) cv.rect(x + 2, y + 2, w - 4, h - 4, t.text);
        if (dis) tc = t.dim;
        int lx = keyLeft.empty() ? x + (w - textW(p, label)) / 2 : x + 22;
        if (!keyLeft.empty()) { keycap(p, x + 4, y + (h - 14) / 2, keyLeft); }
        text(p, lx, ty, label, tc);
        if (!keyRight.empty()) smallRight(p, x + w - 6, y + (h - 8) / 2, keyRight, dis ? t.dim : t.text);
        if (!sub.empty()) small(p, lx, y + h - 11, sub, t.dim);
        return;
    }
    // frame: a raised bevel, brass when selected (or the default OK / Done)
    bool brass = sel || def;
    Color8 face = dis ? H(0x2E3242) : brass ? t.selFill : (hover ? H(0x454B63) : t.face);
    bevel(cv, x, y, w, h, brass ? t.selHi : t.hi, brass ? t.selLo : t.lo, &face);
    tc = dis ? t.faint : brass ? t.selText : t.text;
    int lx = x + 6;
    if (!keyLeft.empty()) { int kw = std::max(11, smallW(p, keyLeft) + 4); cv.fillRect(x + 3, y + 3, kw, h - 6, brass ? t.accent : t.keyCell); small(p, x + 3 + (kw - smallW(p, keyLeft)) / 2, y + (h - 8) / 2, keyLeft, brass ? H(0x20160A) : t.accent); lx = x + kw + 7; }
    else if (keyRight.empty() && sub.empty()) lx = x + (w - textW(p, label)) / 2;
    text(p, lx, ty, label, tc);
    if (!keyRight.empty()) smallRight(p, x + w - 6, y + (h - 8) / 2, keyRight, dis ? t.faint : t.accent);
    if (!sub.empty()) small(p, lx, y + h - 11, sub, dis ? H(0x5A5E70) : t.dim);
}

void checkbox(const Pen& p, int x, int y, bool on, const std::string& label, bool focused, bool enabled) {
    NoMap nm; const UiTheme& t = th(); Canvas& cv = *p.cv;
    Color8 tc = enabled ? t.text : t.faint;
    if (t.id == ThemeId::Native) {
        Color8 w = enabled ? H(0xFFFFFF) : t.panel; well(cv, x, y, 13, 13, &w);
        if (on) { Color8 k = enabled ? t.lo : t.lo2;
            static const int TICK[7][2] = {{3, 5}, {4, 6}, {5, 7}, {6, 6}, {7, 5}, {8, 4}, {9, 3}};
            for (auto& q : TICK) for (int k2 = 0; k2 < 3; k2++) cv.put(x + q[0], y + q[1] + k2, k); }
        int lx = x + 19; text(p, lx, y, label, tc);
        if (focused) { int lw = textW(p, label); for (int xx = lx - 2; xx < lx + lw + 2; xx += 2) { cv.put(xx, y - 1, t.lo); cv.put(xx, y + 12, t.lo); } for (int yy = y - 1; yy < y + 13; yy += 2) { cv.put(lx - 2, yy, t.lo); cv.put(lx + lw + 1, yy, t.lo); } }
        return;
    }
    if (t.id == ThemeId::OneBit) {
        cv.rect(x, y, 12, 12, enabled ? t.text : t.dim);
        if (on) for (int i = 2; i < 10; i++) { cv.put(x + i, y + i, t.text); cv.put(x + 11 - i, y + i, t.text); }
        if (focused) { glow(cv, x - 4, y - 3, textW(p, label) + 26, 18); tc = t.accent; }
        text(p, x + 18, y, label, tc);
        return;
    }
    Color8 w = t.barBack; well(cv, x, y - 1, 14, 14, &w);
    if (on) { cv.fillRect(x + 3, y + 2, 8, 8, t.accent); cv.fillRect(x + 3, y + 2, 8, 1, t.accentHi); }
    if (focused) cv.rect(x - 3, y - 4, textW(p, label) + 27, 20, t.accent);
    text(p, x + 20, y, label, tc);
}

void slider(const Pen& p, int x, int y, int w, int value, int lo, int hi, bool focused) {
    NoMap nm; const UiTheme& t = th(); Canvas& cv = *p.cv;
    float f = hi > lo ? (float)(std::clamp(value, lo, hi) - lo) / (hi - lo) : 0;
    std::string vs = std::to_string(value);
    if (t.id == ThemeId::Native) {
        cv.fillRect(x, y + 4, w, 1, t.lo2); cv.fillRect(x, y + 5, w, 1, t.lo); cv.fillRect(x, y + 6, w, 1, t.hi2); cv.fillRect(x, y + 7, w, 1, t.hi);
        for (int i = 0; i <= 10; i++) cv.fillRect(x + (w - 1) * i / 10, y + 13, 1, 3, t.lo);
        int kx = x + (int)std::lround((w - 10) * f);
        Color8 face = t.face; bevel(cv, kx, y - 2, 10, 16, t.hi, t.lo, &face); bevel(cv, kx + 1, y - 1, 8, 14, t.hi2, t.lo2);
        if (focused) for (int xx = kx - 2; xx < kx + 12; xx += 2) { cv.put(xx, y - 4, t.lo); cv.put(xx, y + 15, t.lo); }
        Color8 wf = H(0xFFFFFF); well(cv, x + w + 10, y - 2, 34, 17, &wf); textRight(p, x + w + 40, y + 1, vs, t.text);
        return;
    }
    if (t.id == ThemeId::OneBit) {
        for (int xx = x; xx < x + w; xx += 2) cv.put(xx, y + 6, t.dim);
        int fw = (int)std::lround((w - 8) * f); cv.fillRect(x, y + 5, fw, 3, t.accent);
        cv.fillRect(x + fw, y, 8, 13, t.text);
        if (focused) glow(cv, x - 3, y - 3, w + 6, 19);
        text(p, x + w + 14, y + 1, vs, t.text);
        return;
    }
    Color8 tr = t.barBack; well(cv, x, y + 3, w, 8, &tr);
    cv.fillRect(x + 2, y + 5, (int)std::lround((w - 4) * f), 4, t.accent);
    int kx = x + (int)std::lround((w - 10) * f); Color8 kb = t.accent; bevel(cv, kx, y - 1, 10, 16, t.accentHi, t.selLo, &kb);
    if (focused) cv.rect(x - 3, y - 3, w + 6, 20, t.accent);
    text(p, x + w + 10, y + 1, vs, t.text);
}

int choiceW(const Pen& p, const std::vector<std::string>& opts) {
    int w = 0; for (auto& o : opts) w += textW(p, o) + 16 + 4; return w > 0 ? w - 4 : 0;
}
void choice(const Pen& p, int x, int y, const std::vector<std::string>& opts, int sel, bool focused, int hover) {
    NoMap nm; const UiTheme& t = th(); Canvas& cv = *p.cv;
    int x0 = x;
    for (size_t i = 0; i < opts.size(); i++) {
        int w = textW(p, opts[i]) + 16; bool on = (int)i == sel;
        if (t.id == ThemeId::Native) { button(p, x, y, w, 20, opts[i], on ? BS_SELECTED : BS_NORMAL, (int)i == hover); }
        else if (t.id == ThemeId::OneBit) {
            if (on) { cv.fillRect(x, y, w, 20, t.text); text(p, x + 8, y + 4, opts[i], t.back); }
            else { cv.fillRect(x, y, w, 20, t.back); cv.rect(x, y, w, 20, (int)i == hover ? t.accent : t.dim); text(p, x + 8, y + 4, opts[i], t.text); }
        } else {
            Color8 face = on ? t.selFill : ((int)i == hover ? H(0x454B63) : t.face);
            bevel(cv, x, y, w, 20, on ? t.selHi : t.hi, on ? t.selLo : t.lo, &face);
            text(p, x + 8, y + 4, opts[i], on ? t.selText : t.text);
        }
        x += w + 4;
    }
    if (focused) { if (t.id == ThemeId::OneBit) glow(cv, x0 - 3, y - 3, x - x0 - 4 + 6, 26); else if (t.id == ThemeId::Frame) cv.rect(x0 - 3, y - 3, x - x0 - 4 + 6, 26, t.accent); }
}
int choiceHit(const Pen& p, int x, int y, const std::vector<std::string>& opts, int mx, int my) {
    if (my < y || my >= y + 20) return -1;
    for (size_t i = 0; i < opts.size(); i++) { int w = textW(p, opts[i]) + 16; if (mx >= x && mx < x + w) return (int)i; x += w + 4; }
    return -1;
}

void tabs(const Pen& p, int x, int y, int w, int h, const std::vector<std::string>& names, int sel, int gap, const std::vector<std::string>& keys) {
    NoMap nm; const UiTheme& t = th(); Canvas& cv = *p.cv;
    for (size_t i = 0; i < names.size(); i++) {
        int tx = x + (int)i * (w + gap); bool on = (int)i == sel; std::string k = i < keys.size() ? keys[i] : "";
        if (t.id == ThemeId::Native) {
            int ty = on ? y : y + 2, th2 = on ? h + 2 : h;
            cv.fillRect(tx, ty, w, th2, t.face);
            cv.fillRect(tx, ty, w, 1, t.hi); cv.fillRect(tx, ty, 1, th2, t.hi); cv.fillRect(tx + w - 1, ty, 1, th2, t.lo); cv.fillRect(tx + w - 2, ty + 1, 1, th2 - 1, t.lo2);
            text(p, tx + 8, ty + (h - 12) / 2, names[i], t.text);
            if (on) cv.fillRect(tx + 1, ty + th2 - 1, w - 3, 2, t.face);
            continue;
        }
        if (t.id == ThemeId::OneBit) {
            if (on) { cv.fillRect(tx, y, w, h, t.text); p.font->drawClassic(cv, *p.ts, tx + (w - titleW(p, names[i])) / 2, y + (h - 12) / 2, names[i], t.back, false); }
            else { cv.fillRect(tx, y, w, h, t.back); cv.rect(tx, y, w, h, t.dim); p.font->drawClassic(cv, *p.ts, tx + (w - titleW(p, names[i])) / 2, y + (h - 12) / 2, names[i], t.text, false); }
            continue;
        }
        Color8 face = on ? t.selFill : t.face;
        bevel(cv, tx, y, w, h, on ? t.selHi : t.hi, on ? t.selLo : t.lo, &face);
        if (k.empty()) text(p, tx + (w - textW(p, names[i])) / 2, y + (h - 12) / 2, names[i], on ? t.selText : t.text);
        else { text(p, tx + 12, y + (h - 12) / 2, names[i], on ? t.selText : t.text); smallRight(p, tx + w - 8, y + (h - 8) / 2 + 1, k, t.accent); }
    }
}
int tabHit(int x, int y, int w, int h, int n, int gap, int mx, int my) {
    if (my < y || my >= y + h + 2) return -1;
    for (int i = 0; i < n; i++) { int tx = x + i * (w + gap); if (mx >= tx && mx < tx + w) return i; }
    return -1;
}

int keycap(const Pen& p, int x, int y, const std::string& k, bool draw) {
    const UiTheme& t = th(); int w = textW(p, k) + 10;
    if (!draw) return w;
    NoMap nm; Canvas& cv = *p.cv;
    if (t.id == ThemeId::Native) { button(p, x, y, w, 16, k, BS_NORMAL); return w; }
    if (t.id == ThemeId::OneBit) { cv.fillRect(x, y, w, 15, t.back); cv.rect(x, y, w, 15, t.text); text(p, x + 5, y + 2, k, t.text); return w; }
    Color8 face = t.face; bevel(cv, x, y, w, 16, t.hi, t.lo, &face); text(p, x + 5, y + 2, k, t.text);
    return w;
}

void listRow(const Pen& p, int x, int y, int w, int h, int state) {
    NoMap nm; const UiTheme& t = th(); Canvas& cv = *p.cv;
    if (state == RS_IDLE) return;
    if (t.id == ThemeId::Native) {
        if (state == RS_SELECTED) cv.fillRect(x, y, w, h, t.rowSel);
        else if (state == RS_WAITING) { cv.fillRect(x, y, w, h, t.tooltip); cv.rect(x, y, w, h, t.lo); }
        else if (state == RS_HOVER) { for (int xx = x; xx < x + w; xx += 2) { cv.put(xx, y, t.lo2); cv.put(xx, y + h - 1, t.lo2); } }
        return;
    }
    if (t.id == ThemeId::OneBit) {
        if (state == RS_SELECTED) glow(cv, x, y, w, h);
        else if (state == RS_WAITING) cv.fillRect(x, y, w, h, t.accent);
        else if (state == RS_HOVER) { for (int xx = x; xx < x + w; xx += 2) { cv.put(xx, y, t.dim); cv.put(xx, y + h - 1, t.dim); } }
        return;
    }
    if (state == RS_SELECTED) { cv.fillRect(x, y, w, h, t.rowSel); cv.rect(x, y, w, h, t.accent); }
    else if (state == RS_WAITING) { cv.fillRect(x, y, w, h, t.selFill); cv.rect(x, y, w, h, t.selHi); }
    else if (state == RS_HOVER) cv.fillRect(x, y, w, h, H(0x30344A));
}

static void nativeTitleBar(const Pen& p, int x, int y, int w, int h, const std::string& tt, bool active, bool boxes, bool bold) {
    Canvas& cv = *p.cv; const UiTheme& t = th();
    Color8 a = active ? H(0x000080) : H(0x808080), b = active ? H(0x1084D0) : H(0xC0C0C0);
    for (int i = 0; i < w; i++) { int k = w > 1 ? i * 256 / (w - 1) : 0; cv.fillRect(x + i, y, 1, h, RGB((uint8_t)(a.r + (b.r - a.r) * k / 256), (uint8_t)(a.g + (b.g - a.g) * k / 256), (uint8_t)(a.b + (b.b - a.b) * k / 256))); }
    Font& f = const_cast<Font&>(*p.font); bool ob = f.bold; f.bold = bold;
    std::string s = tt; int maxW = w - (boxes ? 60 : 24); while (s.size() > 3 && f.width(s) > maxW) s = s.substr(0, s.size() - 3) + "..";
    f.draw(cv, *p.ts, x + 4, y + (h - 12) / 2, s, active ? H(0xFFFFFF) : H(0xD4D0C8)); f.bold = ob;
    if (boxes) {
        int bw = 16, bh = h - 4, bx = x + w - bw - 2, by = y + 2;
        Color8 face = t.face; bevel(cv, bx, by, bw, bh, t.hi, t.lo, &face); bevel(cv, bx + 1, by + 1, bw - 2, bh - 2, t.hi2, t.lo2);
        for (int i = 0; i < 6; i++) { cv.put(bx + 5 + i, by + 3 + i, t.lo); cv.put(bx + 6 + i, by + 3 + i, t.lo); cv.put(bx + 10 - i, by + 3 + i, t.lo); cv.put(bx + 11 - i, by + 3 + i, t.lo); }
    }
}
static void onebitTitleBar(const Pen& p, int x, int y, int w, int h, const std::string& tt, int stripes, bool closeBox) {
    Canvas& cv = *p.cv; const UiTheme& t = th();
    cv.fillRect(x, y, w, h, t.text);
    int tw = titleW(p, tt) + 16, tx = x + (w - tw) / 2;
    int sy0 = y + (h - (stripes * 2 - 1)) / 2;
    for (int i = 0; i < stripes; i++) { cv.fillRect(x + 3, sy0 + i * 2, tx - x - 3, 1, t.back); cv.fillRect(tx + tw, sy0 + i * 2, x + w - 3 - tx - tw, 1, t.back); }
    p.font->drawClassic(cv, *p.ts, tx + 8, y + (h - 12) / 2, tt, t.back, false);
    if (closeBox) { int bs = h - 6; cv.fillRect(x + 6, y + 3, bs + 2, bs, t.text); cv.rect(x + 7, y + 3, bs, bs, t.back); }
}

Rect windowChrome(const Pen& p, int x, int y, int w, int h, const std::string& tt, bool active, bool closeBox) {
    NoMap nm; const UiTheme& t = th(); Canvas& cv = *p.cv;
    if (t.id == ThemeId::OneBit) {
        cv.fillRect(x + 4, y + 4, w, h, t.back);   // the 4 px drop shadow
        cv.fillRect(x, y, w, h, t.back); cv.rect(x, y, w, h, t.text);
        onebitTitleBar(p, x + 1, y + 1, w - 2, 20, tt, 7, closeBox);
        cv.fillRect(x + 1, y + 21, w - 2, 1, t.text);
        return {x + 1, y + 22, w - 2, h - 23};
    }
    // native and frame: the 90s window chrome (frame keeps it on the desktop and fills the inside with its own kit)
    Color8 face = H(0xC0C0C0);
    bevel(cv, x, y, w, h, H(0xDFDFDF), H(0x000000), &face); bevel(cv, x + 1, y + 1, w - 2, h - 2, H(0xFFFFFF), H(0x808080));
    nativeTitleBar(p, x + 3, y + 3, w - 6, 18, tt, active, closeBox, true);
    return {x + 4, y + 23, w - 8, h - 27};
}
Rect toolWindow(const Pen& p, int x, int y, int w, int h, const std::string& tt, bool active) {
    NoMap nm; const UiTheme& t = th(); Canvas& cv = *p.cv;
    if (t.id == ThemeId::OneBit) {
        cv.fillRect(x + 4, y + 4, w, h, t.back); cv.fillRect(x, y, w, h, t.back); cv.rect(x, y, w, h, t.text);
        onebitTitleBar(p, x + 1, y + 1, w - 2, 16, tt, 4, true);
        return {x + 1, y + 17, w - 2, h - 18};
    }
    if (t.id == ThemeId::Native) {
        Color8 face = t.face;
        bevel(cv, x, y, w, h, t.hi2, t.lo, &face); bevel(cv, x + 1, y + 1, w - 2, h - 2, t.hi, t.lo2);
        nativeTitleBar(p, x + 3, y + 3, w - 6, 14, tt, active, true, true);
        return {x + 3, y + 19, w - 6, h - 22};
    }
    panel(cv, x, y, w, h); title(p, x + 10, y + 7, tt);
    return {x + 8, y + 24, w - 16, h - 32};
}

void scrollbar(Canvas& cv, int x, int y, int w, int h, int first, int visible, int total) {
    NoMap nm; const UiTheme& t = th();
    if (total <= visible || h <= 8) return;
    int th2 = std::max(10, h * visible / std::max(1, total)), ty = y + (h - th2) * first / std::max(1, total - visible);
    if (t.id == ThemeId::Native) {
        for (int yy = y; yy < y + h; yy++) for (int xx = x; xx < x + w; xx++) cv.put(xx, yy, ((xx + yy) & 1) ? t.hi : t.hi2);
        Color8 face = t.face; bevel(cv, x, ty, w, th2, t.hi2, t.lo, &face); bevel(cv, x + 1, ty + 1, w - 2, th2 - 2, t.hi, t.lo2);
        return;
    }
    if (t.id == ThemeId::OneBit) { cv.fillRect(x + w / 2, y, 1, h, t.dim); cv.fillRect(x + 1, ty, std::max(2, w - 2), th2, t.text); return; }
    Color8 tr = t.barBack; well(cv, x, y, w, h, &tr);
    Color8 f = t.hi2; bevel(cv, x + 1, ty + 1, w - 2, th2 - 2, t.hi, t.lo, &f);
}

void bar(Canvas& cv, int x, int y, int w, int h, int value, int maxValue, Color8 fill) {
    NoMap nm; const UiTheme& t = th();
    int fw = maxValue > 0 ? w * std::clamp(value, 0, maxValue) / maxValue : 0;
    if (t.id == ThemeId::Native) {
        Color8 wf = H(0xFFFFFF); well(cv, x - 2, y - 2, w + 4, h + 4, &wf);
        for (int xx = 0; xx + 3 <= fw; xx += 4) cv.fillRect(x + xx, y, 3, h, fill);
        return;
    }
    cv.fillRect(x, y, w, h, t.barBack); cv.fillRect(x, y, fw, h, fill);
    if (t.id == ThemeId::Frame && fw > 0) cv.fillRect(x, y, fw, 1, RGB(255, 255, 255, 64));
}
void pips(Canvas& cv, int x, int y, int n, int on, Color8 onC, int pw, int ph, int gap) {
    NoMap nm; const UiTheme& t = th();
    for (int i = 0; i < n; i++) cv.fillRect(x + i * (pw + gap), y, pw, ph, i < on ? onC : t.pipOff);
}
void tooltip(const Pen& p, int x, int y, const std::vector<std::pair<std::string, Color8>>& lines, bool boldFirst) {
    NoMap nm; const UiTheme& t = th(); Canvas& cv = *p.cv;
    int w = 0; for (auto& l : lines) w = std::max(w, textW(p, l.first) + (boldFirst ? (int)l.first.size() : 0));
    w += 12; int h = (int)lines.size() * 13 + 6;
    if (x + w > cv.w - 2) x = cv.w - 2 - w;
    if (y + h > cv.h - 2) y = cv.h - 2 - h;
    if (t.id == ThemeId::Native) { cv.fillRect(x, y, w, h, t.tooltip); cv.rect(x, y, w, h, t.lo); }
    else if (t.id == ThemeId::OneBit) { cv.fillRect(x, y, w, h, t.back); cv.rect(x, y, w, h, t.text); }
    else { panel(cv, x, y, w, h); }
    Font& f = const_cast<Font&>(*p.font);
    for (size_t i = 0; i < lines.size(); i++) { bool b = f.bold; if (boldFirst && i == 0) f.bold = true; text(p, x + 6, y + 3 + (int)i * 13, lines[i].first, lines[i].second); f.bold = b; }
}
}
