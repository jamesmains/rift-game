// Small immediate-mode widget kit for the editor (software rendered, dark theme).
#pragma once
#include <string>
#include <vector>
#include <functional>
#include "../core/gfx.h"
#include "../core/input.h"

namespace ED {
constexpr Color8 BG = {28, 30, 36, 255}, PANEL = {38, 41, 50, 255}, PANEL2 = {46, 50, 60, 255}, BORDER = {70, 76, 90, 255};
constexpr Color8 TEXT = {220, 222, 228, 255}, DIM = {140, 145, 158, 255}, ACCENT = {90, 160, 255, 255}, ACCENT2 = {255, 190, 70, 255};
constexpr Color8 FIELD = {20, 22, 27, 255}, SEL = {60, 90, 140, 255}, HOVER = {56, 62, 76, 255}, DANGER = {200, 70, 70, 255}, OK = {80, 190, 110, 255};
}

struct EdUI {
    Canvas* cv = nullptr; Tileset* ts = nullptr; Font* font = nullptr;
    const InputState* in = nullptr;
    int idCounter = 0, focus = -1, caret = 0;
    bool mouseUsed = false;          // a widget consumed the click this frame
    bool modal = false;              // a popup is open: widgets below ignore the mouse
    float time = 0;
    std::string tooltip; int tipX = 0, tipY = 0;

    void begin(Canvas* c, Tileset* t, Font* f, const InputState* i, float dt) { cv = c; ts = t; font = f; in = i; idCounter = 0; mouseUsed = false; time += dt; tooltip.clear(); }
    // small choice button: shows the current value, opens nothing by itself (caller opens a list picker)
    bool choice(int x, int y, int w, const std::string& value) { cv->fillRect(x, y, w, 16, ED::FIELD); cv->rect(x, y, w, 16, hover(x, y, w, 16) ? ED::ACCENT : ED::BORDER); cv->setClip({x + 1, y, w - 14, 16}); font->draw(*cv, *ts, x + 4, y + 2, value, ED::TEXT); cv->resetClip(); font->draw(*cv, *ts, x + w - 10, y + 2, "v", ED::DIM); return clicked(x, y, w, 16); }
    int nextId() { return ++idCounter; }
    bool hover(int x, int y, int w, int h) const { return !modal && in->mouseX >= x && in->mouseY >= y && in->mouseX < x + w && in->mouseY < y + h; }
    bool clicked(int x, int y, int w, int h) { if (hover(x, y, w, h) && in->mousePressed[0] && !mouseUsed) { mouseUsed = true; return true; } return false; }
    void blur() { focus = -1; selAnchor = -1; dragSel = false; }
    bool typing() const { return focus >= 0; }

    void text(int x, int y, const std::string& s, Color8 c = ED::TEXT) { font->draw(*cv, *ts, x, y, s, c); }
    void textRight(int rx, int y, const std::string& s, Color8 c = ED::TEXT) { font->draw(*cv, *ts, rx - font->width(s), y, s, c); }
    void label(int x, int y, const std::string& s) { font->draw(*cv, *ts, x, y, s, ED::DIM); }
    void panel(int x, int y, int w, int h, Color8 fill = ED::PANEL) { cv->fillRect(x, y, w, h, fill); cv->rect(x, y, w, h, ED::BORDER); }
    void header(int x, int y, int w, const std::string& s) { cv->fillRect(x, y, w, 16, ED::PANEL2); font->draw(*cv, *ts, x + 6, y + 2, s, ED::ACCENT2); }

    bool button(int x, int y, int w, int h, const std::string& lbl, bool enabled = true, bool active = false, Color8 col = ED::TEXT) {
        bool hv = enabled && hover(x, y, w, h);
        cv->fillRect(x, y, w, h, !enabled ? ED::PANEL : active ? ED::SEL : hv ? ED::HOVER : ED::PANEL2);
        cv->rect(x, y, w, h, active ? ED::ACCENT : ED::BORDER);
        font->draw(*cv, *ts, x + (w - font->width(lbl)) / 2, y + (h - 12) / 2, lbl, enabled ? col : ED::DIM);
        return enabled && clicked(x, y, w, h);
    }
    bool checkbox(int x, int y, const std::string& lbl, bool& v) {
        cv->fillRect(x, y, 12, 12, ED::FIELD); cv->rect(x, y, 12, 12, ED::BORDER);
        if (v) cv->fillRect(x + 3, y + 3, 6, 6, ED::ACCENT);
        font->draw(*cv, *ts, x + 16, y, lbl, ED::TEXT);
        if (clicked(x, y, 16 + font->width(lbl), 12)) { v = !v; return true; }
        return false;
    }
    // ---- clipboard (internal copy + OS clipboard through PlatformRequests) ----
    std::string clip; PlatformRequests* req = nullptr;
    void copyText(const std::string& t) { clip = t; if (req) { req->setClipboard = t; req->hasClipboardOut = true; } }
    std::string pasteText() const { return (in && !in->clipboard.empty()) ? in->clipboard : clip; }
    // selection state for the focused single-line field
    int selAnchor = -1; bool dragSel = false; float lastClickT = -10; int lastClickId = -1;
    int tabTarget = -1;              // focus this id next (Tab / Shift+Tab), selecting its contents
    bool hasSel() const { return selAnchor >= 0 && selAnchor != caret; }
    // Single-line text field with mouse/shift selection and Ctrl+A/C/X/V. Returns true when the value changed.
    bool textField(int x, int y, int w, std::string& v, int maxLen = 200) {
        int id = nextId();
        bool arrived = false;
        if (tabTarget == id) { focus = id; tabTarget = -1; selAnchor = 0; caret = (int)v.size(); dragSel = false; arrived = true; }
        bool f = focus == id;
        bool hv = hover(x, y, w, 16);
        cv->fillRect(x, y, w, 16, ED::FIELD); cv->rect(x, y, w, 16, f ? ED::ACCENT : hv ? ED::DIM : ED::BORDER);
        auto caretFromMouse = [&](int off) { int best = 0; for (size_t i = 0; i <= v.size(); i++) { int wpx = font->width(v.substr(0, i)); if (x + 4 - off + wpx - 2 <= in->mouseX) best = (int)i; } return best; };
        int off = 0;
        if (f) { int cw = font->width(v.substr(0, std::min(caret, (int)v.size()))); if (cw > w - 10) off = cw - (w - 10); }
        if (clicked(x, y, w, 16)) {
            bool dbl = (focus == id && lastClickId == id && time - lastClickT < 0.35f);
            focus = id; caret = caretFromMouse(off);
            if (dbl) { selAnchor = 0; caret = (int)v.size(); } else { selAnchor = in->shift() && hasSel() ? selAnchor : (in->shift() ? selAnchor : caret); if (!in->shift()) selAnchor = caret; dragSel = true; }
            lastClickT = time; lastClickId = id;
        }
        if (f && dragSel) { if (!in->mouseDown[0]) { dragSel = false; if (!hasSel()) selAnchor = -1; } else if (in->mouseDX) caret = caretFromMouse(off); }
        bool changed = false;
        if (f && !arrived) {
            caret = std::min(caret, (int)v.size()); if (selAnchor > (int)v.size()) selAnchor = (int)v.size();
            auto delSel = [&]() { if (!hasSel()) return false; int a = std::min(selAnchor, caret), b = std::max(selAnchor, caret); v.erase(a, b - a); caret = a; selAnchor = -1; changed = true; return true; };
            auto selText = [&]() { if (!hasSel()) return std::string(); int a = std::min(selAnchor, caret), b = std::max(selAnchor, caret); return v.substr(a, b - a); };
            if (in->ctrl()) {
                if (in->pressed[K_A]) { selAnchor = 0; caret = (int)v.size(); }
                if (in->pressed[K_C] && hasSel()) copyText(selText());
                if (in->pressed[K_X] && hasSel()) { copyText(selText()); delSel(); }
                if (in->pressed[K_V]) { std::string t = pasteText(); std::string clean; for (char c : t) if (c >= 32 && c < 127) clean += c; delSel(); if ((int)(v.size() + clean.size()) > maxLen) clean = clean.substr(0, std::max(0, maxLen - (int)v.size())); v.insert(caret, clean); caret += (int)clean.size(); changed = !clean.empty() || changed; selAnchor = -1; }
            } else {
                for (char c : in->typed) if (c >= 32 && c < 127) { delSel(); if ((int)v.size() < maxLen) { v.insert(v.begin() + caret, c); caret++; changed = true; } selAnchor = -1; }
                if (in->pressed[K_BACKSPACE]) { if (!delSel() && caret > 0) { v.erase(caret - 1, 1); caret--; changed = true; } selAnchor = -1; }
                if (in->pressed[K_DELETE]) { if (!delSel() && caret < (int)v.size()) { v.erase(caret, 1); changed = true; } selAnchor = -1; }
                auto move = [&](int nc) { if (in->shift()) { if (selAnchor < 0 || !hasSel()) selAnchor = caret; } else selAnchor = -1; caret = clampi(nc, 0, (int)v.size()); };
                if (in->pressed[K_LEFT]) { if (hasSel() && !in->shift()) { caret = std::min(selAnchor, caret); selAnchor = -1; } else move(caret - 1); }
                if (in->pressed[K_RIGHT]) { if (hasSel() && !in->shift()) { caret = std::max(selAnchor, caret); selAnchor = -1; } else move(caret + 1); }
                if (in->pressed[K_HOME]) move(0); if (in->pressed[K_END]) move((int)v.size());
            }
            if (in->pressed[K_TAB]) { tabTarget = in->shift() ? id - 1 : id + 1; focus = -1; selAnchor = -1; }
            else if (in->pressed[K_ENTER] || in->pressed[K_ESCAPE]) { focus = -1; selAnchor = -1; }
            if (in->mousePressed[0] && !hv) { focus = -1; selAnchor = -1; }
        }
        // draw with horizontal scroll so the caret stays visible
        off = 0; int cw = font->width(v.substr(0, std::min(caret, (int)v.size())));
        if (f && cw > w - 10) off = cw - (w - 10);
        cv->setClip({x + 2, y, w - 4, 16});
        if (f && hasSel()) { int a = std::min(selAnchor, caret), b = std::max(selAnchor, caret); int xa = x + 4 - off + font->width(v.substr(0, a)), xb = x + 4 - off + font->width(v.substr(0, b)); cv->fillRect(xa, y + 2, xb - xa, 12, ED::SEL); }
        font->draw(*cv, *ts, x + 4 - off, y + 2, v, ED::TEXT);
        if (f && ((int)(time * 2) % 2 == 0)) cv->fillRect(x + 4 - off + cw, y + 2, 1, 12, ED::TEXT);
        cv->resetClip();
        return changed;
    }
    bool intField(int x, int y, int w, int& v, int lo = -1000000, int hi = 1000000) {
        std::string s = std::to_string(v); bool ch = false;
        if (hover(x, y, w, 16) && in->wheel) { int step = in->shift() ? 10 : 1; v = clampi(v + in->wheel * step, lo, hi); ch = true; s = std::to_string(v); }
        if (button(x, y, 14, 16, "-")) { v = std::max(lo, v - 1); ch = true; }
        if (button(x + w - 14, y, 14, 16, "+")) { v = std::min(hi, v + 1); ch = true; }
        if (textField(x + 15, y, w - 30, s, 10)) { std::string d; for (char c : s) if (isdigit((unsigned char)c) || c == '-') d += c; int nv = d.empty() ? v : atoi(d.c_str()); if (nv >= lo && nv <= hi) { v = nv; ch = true; } }
        return ch;
    }
    // Scrollable list. Returns the index clicked this frame or -1. 'sel' is highlighted.
    int listBox(int x, int y, int w, int h, const std::vector<std::string>& items, int& sel, int& scroll, int rowH = 15) {
        int id = nextId(); (void)id;
        cv->fillRect(x, y, w, h, ED::FIELD); cv->rect(x, y, w, h, ED::BORDER);
        int rows = h / rowH; int n = (int)items.size();
        if (hover(x, y, w, h) && in->wheel) scroll -= in->wheel * 3;
        scroll = std::max(0, std::min(scroll, std::max(0, n - rows)));
        int hit = -1;
        cv->setClip({x + 1, y + 1, w - 2, h - 2});
        for (int r = 0; r < rows && scroll + r < n; r++) {
            int i = scroll + r; int ry = y + 1 + r * rowH;
            bool hv = hover(x, ry, w - 8, rowH);
            if (i == sel) cv->fillRect(x + 1, ry, w - 9, rowH, ED::SEL); else if (hv) cv->fillRect(x + 1, ry, w - 9, rowH, ED::HOVER);
            font->draw(*cv, *ts, x + 5, ry + 1, items[i], i == sel ? C::White : ED::TEXT);
            if (hv && in->mousePressed[0] && !mouseUsed) { mouseUsed = true; sel = i; hit = i; }
        }
        cv->resetClip();
        if (n > rows) { // scrollbar
            int th = std::max(10, h * rows / n), ty = y + (h - th) * scroll / std::max(1, n - rows);
            cv->fillRect(x + w - 7, y + 1, 6, h - 2, ED::PANEL); cv->fillRect(x + w - 6, ty, 4, th, ED::DIM);
            if (hover(x + w - 8, y, 8, h) && in->mouseDown[0]) scroll = (in->mouseY - y) * n / h - rows / 2;
        }
        return hit;
    }
    void tile(Tile t, int x, int y, int scale = 2, Color8 tint = C::White, Color8 bg = ED::FIELD) {
        cv->fillRect(x, y, 12 * scale, 12 * scale, bg); cv->rect(x - 1, y - 1, 12 * scale + 2, 12 * scale + 2, ED::BORDER);
        if (t.valid()) ts->draw(*cv, t, x, y, scale, tint);
        else { cv->line(x, y, x + 12 * scale - 1, y + 12 * scale - 1, ED::DANGER); }
    }
    void tip(const std::string& s) { tooltip = s; tipX = in->mouseX + 12; tipY = in->mouseY + 12; }
    void drawTooltip() { if (tooltip.empty()) return; int w = font->width(tooltip) + 8; int x = std::min(tipX, cv->w - w - 2); cv->fillRect(x, tipY, w, 14, RGB(0, 0, 0, 220)); cv->rect(x, tipY, w, 14, ED::BORDER); font->draw(*cv, *ts, x + 4, tipY + 1, tooltip, C::White); }
};
