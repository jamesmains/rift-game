#include "ui.h"
#include "tiles.h"

namespace UI {

void panel(Ctx& c, int x, int y, int w, int h, Color8 fill, Color8 border) {
    Canvas& cv = *c.cv;
    cv.fillRect(x, y, w, h, fill);
    cv.rect(x, y, w, h, border);
    cv.rect(x + 2, y + 2, w - 4, h - 4, RGB(border.r / 2, border.g / 2, border.b / 2));
    // corner studs
    Color8 st = border;
    cv.fillRect(x + 1, y + 1, 3, 3, st); cv.fillRect(x + w - 4, y + 1, 3, 3, st);
    cv.fillRect(x + 1, y + h - 4, 3, 3, st); cv.fillRect(x + w - 4, y + h - 4, 3, 3, st);
}

void panelTitle(Ctx& c, int x, int y, int w, int h, const std::string& title) {
    panel(c, x, y, w, h);
    Canvas& cv = *c.cv;
    // 1.5.1 (#103): measure the title as it is drawn (bold is 1 px wider per letter), and shorten one that is wider
    // than the panel, so the text always sits inside its box.
    c.font->bold = true;
    std::string t = title;
    while (t.size() > 4 && c.font->width(t) + 12 > w - 24) t = t.substr(0, t.size() - 4) + "..";
    int tw = c.font->width(t) + 12;
    cv.fillRect(x + (w - tw) / 2, y - 2, tw, 16, C::Panel);
    cv.rect(x + (w - tw) / 2, y - 2, tw, 16, C::Gold);
    textCenter(c, x + w / 2, y, t, C::Gold);
    c.font->bold = false;
}

void bar(Ctx& c, int x, int y, int w, int h, int value, int maxValue, Color8 fill, Color8 back) {
    Canvas& cv = *c.cv;
    cv.fillRect(x, y, w, h, back);
    int fw = maxValue > 0 ? (w - 2) * std::max(0, std::min(value, maxValue)) / maxValue : 0;
    cv.fillRect(x + 1, y + 1, fw, h - 2, fill);
    cv.rect(x, y, w, h, C::Black);
}

void text(Ctx& c, int x, int y, const std::string& s, Color8 col, int scale) { c.font->draw(*c.cv, *c.ts, x, y, s, col, scale); }
void textCenter(Ctx& c, int cx, int y, const std::string& s, Color8 col, int scale) { c.font->draw(*c.cv, *c.ts, cx - c.font->width(s, scale) / 2, y, s, col, scale); }
void textRight(Ctx& c, int rx, int y, const std::string& s, Color8 col, int scale) { c.font->draw(*c.cv, *c.ts, rx - c.font->width(s, scale), y, s, col, scale); }
void textShadow(Ctx& c, int x, int y, const std::string& s, Color8 col, int scale) { c.font->drawShadow(*c.cv, *c.ts, x, y, s, col, C::Black, scale); }
int wrapped(Ctx& c, int x, int y, int w, const std::string& s, Color8 col) { return c.font->drawWrapped(*c.cv, *c.ts, x, y, w, s, col); }

bool listRow(Ctx& c, const InputState& in, int x, int y, int w, int h, bool selected) {
    bool hover = in.mouseX >= x && in.mouseY >= y && in.mouseX < x + w && in.mouseY < y + h;
    if (selected) { c.cv->fillRect(x, y, w, h, RGB(90, 70, 20)); c.cv->rect(x, y, w, h, C::Gold); selCursor(c, x, y, h); }
    else if (hover) c.cv->fillRect(x, y, w, h, C::PanelHi);
    return hover;
}
void selCursor(Ctx& c, int x, int y, int h) {
    bool bright = ((int)(c.time * 3)) % 2 == 0;
    Color8 col = bright ? C::White : C::Gold;
    int cy = y + h / 2; int ax = x - 7;
    if (ax < 0) return;
    for (int i = 0; i < 4; i++) c.cv->vline(ax + i, cy - (3 - i), 2 * (3 - i) + 1, col);
}

bool button(Ctx& c, const InputState& in, int x, int y, int w, int h, const std::string& label, bool enabled, bool selected) {
    bool hover = enabled && in.mouseX >= x && in.mouseY >= y && in.mouseX < x + w && in.mouseY < y + h;
    Color8 fill = !enabled ? RGB(40, 40, 46) : (selected || hover) ? RGB(100, 80, 30) : RGB(50, 50, 66);
    c.cv->fillRect(x, y, w, h, fill);
    c.cv->rect(x, y, w, h, enabled ? C::Gold : C::DkGray);
    if (selected && enabled) { bool bright = ((int)(c.time * 3)) % 2 == 0; c.cv->rect(x - 1, y - 1, w + 2, h + 2, bright ? C::White : C::Gold); selCursor(c, x, y, h); }
    textCenter(c, x + w / 2, y + (h - 12) / 2, label, enabled ? C::White : C::Gray);
    return hover && in.mousePressed[0];
}

void icon(Ctx& c, Tile t, int x, int y, int scale, Color8 tint) { c.ts->draw(*c.cv, t, x, y, scale, tint); }

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
    if (draw) { c.cv->fillRect(x, y, kw, 14, C::LtGray); c.cv->rect(x, y, kw, 14, C::White); c.font->draw(*c.cv, *c.ts, x + 3, y + 1, key, C::Black); }
    return kw;
}
int keyHint(Ctx& c, int x, int y, const std::string& keyIn, const std::string& label) {
    std::string key = gPadGlyphs ? padEquivalent(keyIn) : keyIn;
    int kw;
    if (gPadGlyphs && key != keyIn) kw = drawPadGlyph(c, x, y, key);
    else if (gPadGlyphs && startsWith(key, "Pad ")) kw = drawPadGlyph(c, x, y, key);
    else {
        kw = c.font->width(key) + 6;
        c.cv->fillRect(x, y, kw, 14, C::LtGray);
        c.cv->rect(x, y, kw, 14, C::White);
        c.font->draw(*c.cv, *c.ts, x + 3, y + 1, key, C::Black);
    }
    c.font->draw(*c.cv, *c.ts, x + kw + 4, y + 1, label, C::LtGray);
    return x + kw + 4 + c.font->width(label) + 14;   // where the next hint can start
}

bool gColorblind = false;
Color8 friendColor() { return gColorblind ? RGB(90, 170, 255) : RGB(60, 200, 90); }
Color8 foeColor() { return gColorblind ? RGB(255, 150, 20) : RGB(220, 40, 40); }
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
