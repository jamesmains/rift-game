// Small immediate-mode widget kit for the editor (software rendered, dark theme).
#pragma once
#include <string>
#include <vector>
#include <functional>
#include "../core/gfx.h"
#include "../core/input.h"
#include "textops.h"

namespace ED {
constexpr Color8 BG = {28, 30, 36, 255}, PANEL = {38, 41, 50, 255}, PANEL2 = {46, 50, 60, 255}, BORDER = {70, 76, 90, 255};
constexpr Color8 TEXT = {220, 222, 228, 255}, DIM = {140, 145, 158, 255}, ACCENT = {90, 160, 255, 255}, ACCENT2 = {255, 190, 70, 255};
constexpr Color8 FIELD = {20, 22, 27, 255}, SEL = {60, 90, 140, 255}, HOVER = {56, 62, 76, 255}, DANGER = {200, 70, 70, 255}, OK = {80, 190, 110, 255};
}

// 1.1.6 (#30): search matching for every search box - case-insensitive, every word typed must appear somewhere
inline bool edMatch(const std::string& hay, const std::string& query) {
    auto low = [](std::string v) { for (auto& c : v) c = (char)tolower((unsigned char)c); return v; };
    std::string h = low(hay), q = low(query); size_t i = 0;
    while (i < q.size()) {
        while (i < q.size() && q[i] == ' ') i++;
        size_t j = q.find(' ', i); if (j == std::string::npos) j = q.size();
        if (j > i && h.find(q.substr(i, j - i)) == std::string::npos) return false;
        i = j;
    }
    return true;
}

struct EdUI {
    Canvas* cv = nullptr; Tileset* ts = nullptr; Font* font = nullptr;
    const InputState* in = nullptr; InputState* inw = nullptr;   // inw: the same input, for eating keys / clicks
    int idCounter = 0, focus = -1, caret = 0;
    bool mouseUsed = false;          // a widget consumed the click this frame
    bool modal = false;              // a popup is open: widgets below ignore the mouse
    float time = 0;
    std::string tooltip; int tipX = 0, tipY = 0;

    void begin(Canvas* c, Tileset* t, Font* f, InputState* i, float dt) {
        cv = c; ts = t; font = f; in = i; inw = i; idCounter = 0; mouseUsed = false; time += dt; tooltip.clear();
        menuInput();   // an open right-click menu takes its clicks / keys before any widget sees them
        // 1.1.11 (#35): click count at the same spot (2 = double-click: a word, 3 = triple: the line / everything)
        if (in->mousePressed[0]) { if (time - clickT < 0.45f && abs(in->mouseX - clickX) <= 4 && abs(in->mouseY - clickY) <= 4) clicks = std::min(3, clicks + 1); else clicks = 1; clickT = time; clickX = in->mouseX; clickY = in->mouseY; }
    }
    int clicks = 0, clickX = -100, clickY = -100; float clickT = -10;
    // small choice button: shows the current value, opens nothing by itself (caller opens a list picker)
    bool choice(int x, int y, int w, const std::string& value) { cv->fillRect(x, y, w, 16, ED::FIELD); cv->rect(x, y, w, 16, hover(x, y, w, 16) ? ED::ACCENT : ED::BORDER); cv->setClip({x + 1, y, w - 14, 16}); font->draw(*cv, *ts, x + 4, y + 2, value, ED::TEXT); cv->resetClip(); font->draw(*cv, *ts, x + w - 10, y + 2, "v", ED::DIM); return clicked(x, y, w, 16); }
    int nextId() { return ++idCounter; }
    bool hover(int x, int y, int w, int h) const { return !modal && in->mouseX >= x && in->mouseY >= y && in->mouseX < x + w && in->mouseY < y + h; }
    bool clicked(int x, int y, int w, int h) { if (hover(x, y, w, h) && in->mousePressed[0] && !mouseUsed) { mouseUsed = true; return true; } return false; }
    void blur() { focus = -1; selAnchor = -1; dragSel = false; dragWord = false; }
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

    // ---- 1.1.11 (#35): right-click menu for text boxes. The menu stores the command picked; the text box it belongs to
    // runs it the next time it is drawn (takeCmd), so the menu needs no access to the text itself. ----
    enum { CM_NONE, CM_UNDO, CM_REDO, CM_CUT, CM_COPY, CM_PASTE, CM_DELETE, CM_SELALL, CM_SELWORD, CM_SELLINE, CM_BIGBOX,
           CM_WIDE, CM_WRAP, CM_FIND, CM_REPLACE, CM_GOTO, CM_COMMENT, CM_DUPLINE, CM_DELLINE, CM_UPPER, CM_LOWER, CM_INDENT, CM_OUTDENT };
    struct MenuItem { int cmd; std::string label, keys; bool on; };
    std::vector<MenuItem> menuItems; bool menuOpen = false; int menuId = -1, menuX = 0, menuY = 0, menuHot = -1, menuCmd = 0, menuCmdId = -1;
    static constexpr int MENU_W = 214, MENU_ROW = 15;
    static MenuItem sep() { return {CM_NONE, "", "", false}; }
    void openMenu(int id, std::vector<MenuItem> items) {
        menuItems = std::move(items); menuOpen = true; menuId = id; menuHot = -1;
        int h = (int)menuItems.size() * MENU_ROW + 4;
        menuX = std::max(0, std::min(in->mouseX, cv->w - MENU_W - 2)); menuY = std::max(0, std::min(in->mouseY, cv->h - h - 2));
    }
    int menuRowAt(int mx, int my) const { if (mx < menuX || mx >= menuX + MENU_W || my < menuY + 2) return -1; int r = (my - menuY - 2) / MENU_ROW; return r < (int)menuItems.size() ? r : -1; }
    void menuInput() {
        if (!menuOpen) return;
        int h = (int)menuItems.size() * MENU_ROW + 4;
        bool over = in->mouseX >= menuX && in->mouseX < menuX + MENU_W && in->mouseY >= menuY && in->mouseY < menuY + h;
        int r = menuRowAt(in->mouseX, in->mouseY); if (r >= 0 && (in->mouseDX || in->mouseDY)) menuHot = r;
        auto pick = [&](int i) { if (i >= 0 && i < (int)menuItems.size() && menuItems[i].on) { menuCmd = menuItems[i].cmd; menuCmdId = menuId; menuOpen = false; } };
        if (in->mousePressed[0] || in->mousePressed[2] || in->mousePressed[1]) {
            if (over) { if (in->mousePressed[0]) pick(r); inw->eatMouse(); }
            else menuOpen = false;   // a click elsewhere closes it and still does what it does
            return;
        }
        if (in->wheel) { menuOpen = false; return; }
        auto step = [&](int d) { int n = (int)menuItems.size(); for (int k = 0; k < n; k++) { menuHot = ((menuHot < 0 ? (d > 0 ? -1 : 0) : menuHot) + d + n) % n; if (menuItems[menuHot].on) break; } };
        if (in->pressed[K_DOWN]) { step(1); inw->pressed[K_DOWN] = false; }
        if (in->pressed[K_UP]) { step(-1); inw->pressed[K_UP] = false; }
        if (in->pressed[K_ENTER]) { pick(menuHot); menuOpen = false; inw->pressed[K_ENTER] = false; }
        if (in->pressed[K_ESCAPE]) { menuOpen = false; inw->pressed[K_ESCAPE] = false; }
        if (!in->typed.empty() || in->pressed[K_BACKSPACE] || in->pressed[K_DELETE] || in->pressed[K_LEFT] || in->pressed[K_RIGHT] || in->pressed[K_TAB]) menuOpen = false;
    }
    int takeCmd(int id) { if (menuCmdId != id || !menuCmd) return 0; int c = menuCmd; menuCmd = 0; menuCmdId = -1; return c; }
    void drawMenu() {
        if (!menuOpen) return;
        int h = (int)menuItems.size() * MENU_ROW + 4;
        cv->fillRect(menuX + 3, menuY + 3, MENU_W, h, RGB(0, 0, 0, 120));
        cv->fillRect(menuX, menuY, MENU_W, h, ED::PANEL2); cv->rect(menuX, menuY, MENU_W, h, ED::ACCENT);
        for (size_t i = 0; i < menuItems.size(); i++) {
            auto& it = menuItems[i]; int ry = menuY + 2 + (int)i * MENU_ROW;
            if (it.cmd == CM_NONE) { cv->hline(menuX + 6, ry + MENU_ROW / 2, MENU_W - 12, ED::BORDER); continue; }
            if ((int)i == menuHot && it.on) cv->fillRect(menuX + 2, ry, MENU_W - 4, MENU_ROW, ED::SEL);
            font->draw(*cv, *ts, menuX + 10, ry + 1, it.label, it.on ? ED::TEXT : ED::DIM);
            if (!it.keys.empty()) font->draw(*cv, *ts, menuX + MENU_W - 8 - font->width(it.keys), ry + 1, it.keys, ED::DIM);
        }
    }

    // ---- 1.1.11 (#35): "Edit in a big box" - a single-line field asks for it (Ctrl+E, the ... button, the menu); the Editor opens
    // the popup and on OK hands the text back here: the field with bigApplyId takes it the next frame. ----
    int bigReqId = -1, bigReqMax = 200; std::string bigReqValue;
    int bigApplyId = -1; std::string bigApplyValue;
    bool bigAllowed = true;          // off while a popup draws its fields (the big box is a popup itself)

    // selection state for the focused single-line field
    int selAnchor = -1; bool dragSel = false, dragWord = false; int wordA = 0, wordB = 0;
    int tabTarget = -1;              // focus this id next (Tab / Shift+Tab), selecting its contents
    struct FieldSnap { std::string v; int caret; };
    std::vector<FieldSnap> fUndo, fRedo; int fHistId = -1, fLastKind = -1, fOff = 0;   // the focused field's own undo, its scroll (px)
    bool hasSel() const { return selAnchor >= 0 && selAnchor != caret; }
    // Single-line text field: mouse / Shift selection, double-click a word, triple-click all, Ctrl+arrows by word,
    // Ctrl+Backspace/Delete a word, Ctrl+A/C/X/V (and Shift+Del / Ctrl+Ins / Shift+Ins), Ctrl+Z / Ctrl+Y, right-click menu,
    // Ctrl+E (or the ... button on long text) edits it in a big box. Returns true when the value changed.
    bool textField(int x, int y, int w, std::string& v, int maxLen = 200) {
        int id = nextId();
        bool arrived = false, changed = false;
        auto resetHist = [&]() { if (fHistId != id) { fUndo.clear(); fRedo.clear(); fHistId = id; fLastKind = -1; fOff = 0; } };
        if (bigApplyId == id) {
            std::string nv = bigApplyValue.substr(0, (size_t)std::max(0, maxLen)); bigApplyId = -1; focus = id; resetHist();
            if (nv != v) { fUndo.push_back({v, (int)v.size()}); fRedo.clear(); v = nv; changed = true; }
            caret = (int)v.size(); selAnchor = -1; fLastKind = -1; arrived = true;
        }
        if (tabTarget == id) { focus = id; tabTarget = -1; selAnchor = 0; caret = (int)v.size(); dragSel = false; arrived = true; }
        bool f = focus == id;
        if (f) resetHist();
        bool hv = hover(x, y, w, 16);
        cv->fillRect(x, y, w, 16, ED::FIELD); cv->rect(x, y, w, 16, f ? ED::ACCENT : hv ? ED::DIM : ED::BORDER);
        int inner = w - 10;
        bool bigBtn = bigAllowed && w >= 60 && font->width(v) > inner && (f || hv);
        int off = f ? fOff : 0;
        auto caretFromMouse = [&](int o) { int best = 0; for (size_t i = 0; i <= v.size(); i++) { int wpx = font->width(v.substr(0, i)); if (x + 4 - o + wpx - 2 <= in->mouseX) best = (int)i; } return best; };
        auto selText = [&]() { if (!hasSel()) return std::string(); int a = std::min(selAnchor, caret), b = std::max(selAnchor, caret); return v.substr(a, b - a); };
        auto delSel = [&]() { if (!hasSel()) return false; int a = std::min(selAnchor, caret), b = std::max(selAnchor, caret); v.erase(a, b - a); caret = a; selAnchor = -1; changed = true; return true; };
        auto askBig = [&]() { if (!bigAllowed) return; bigReqId = id; bigReqValue = v; bigReqMax = maxLen; };
        if (bigBtn && clicked(x + w - 16, y + 1, 15, 14)) { focus = id; f = true; resetHist(); askBig(); }
        if (clicked(x, y, w, 16)) {
            bool wasF = focus == id; int pc = caretFromMouse(off);
            focus = id; f = true; resetHist(); dragWord = false; fLastKind = -1;
            if (wasF && clicks >= 3) { selAnchor = 0; caret = (int)v.size(); dragSel = false; }
            else if (wasF && clicks == 2) { txt::wordAt(v, pc, wordA, wordB); selAnchor = wordA; caret = wordB; dragSel = true; dragWord = true; }
            else { if (in->shift() && wasF) { if (selAnchor < 0) selAnchor = caret; } else selAnchor = pc; caret = pc; dragSel = true; }
        }
        if (hv && in->mousePressed[2] && !mouseUsed) {   // right-click: the menu (a selection under the pointer stays)
            mouseUsed = true; bool wasF = focus == id; focus = id; f = true; resetHist();
            int pc = caretFromMouse(off), a = std::min(selAnchor, caret), b = std::max(selAnchor, caret);
            if (!wasF || !hasSel() || pc < a || pc > b) { caret = pc; selAnchor = -1; }
            openMenu(id, {{CM_UNDO, "Undo", "Ctrl+Z", !fUndo.empty()}, {CM_REDO, "Redo", "Ctrl+Y", !fRedo.empty()}, sep(),
                          {CM_CUT, "Cut", "Ctrl+X", hasSel()}, {CM_COPY, "Copy", "Ctrl+C", hasSel()}, {CM_PASTE, "Paste", "Ctrl+V", true},
                          {CM_DELETE, "Delete", "Del", hasSel()}, sep(),
                          {CM_SELWORD, "Select word", "double-click", !v.empty()}, {CM_SELALL, "Select all", "Ctrl+A", !v.empty()}, sep(),
                          {CM_BIGBOX, "Edit in a big box...", "Ctrl+E", bigAllowed}});
        }
        if (f && dragSel) {
            if (!in->mouseDown[0]) { dragSel = false; dragWord = false; if (!hasSel()) selAnchor = -1; }
            else if (in->mouseDX || in->mouseDY) { int pc = caretFromMouse(off); if (dragWord) { int a, b; txt::wordAt(v, pc, a, b); if (pc < wordA) { selAnchor = wordB; caret = a; } else { selAnchor = wordA; caret = std::max(wordB, b); } } else caret = pc; }
        }
        if (f && !arrived) {
            caret = std::min(caret, (int)v.size()); if (selAnchor > (int)v.size()) selAnchor = (int)v.size();
            int cmd = takeCmd(id);
            bool C = in->ctrl(), S = in->shift();
            auto P = [&](int k) { return in->pressed[k]; };
            // undo steps: a run of typing (to the next space) or of Backspace/Delete is one step; anything else is its own
            auto push = [&](int kind) { if (kind < 0 || kind != fLastKind || fUndo.empty()) { fUndo.push_back({v, caret}); if (fUndo.size() > 100) fUndo.erase(fUndo.begin()); } fRedo.clear(); fLastKind = kind; };
            auto move = [&](int nc) { if (S) { if (selAnchor < 0 || !hasSel()) selAnchor = caret; } else selAnchor = -1; caret = clampi(nc, 0, (int)v.size()); fLastKind = -1; };
            if (cmd == CM_BIGBOX || (C && P(K_E))) askBig();
            if (cmd == CM_SELWORD) { int a, b; txt::wordAt(v, caret, a, b); selAnchor = a; caret = b; }
            if (cmd == CM_SELALL || (C && P(K_A))) { selAnchor = 0; caret = (int)v.size(); }
            if ((cmd == CM_COPY || (C && (P(K_C) || P(K_INSERT)))) && hasSel()) copyText(selText());
            if ((cmd == CM_CUT || (C && P(K_X)) || (S && !C && P(K_DELETE))) && hasSel()) { copyText(selText()); push(-1); delSel(); }
            if (cmd == CM_DELETE && hasSel()) { push(-1); delSel(); }
            if (cmd == CM_PASTE || (C && P(K_V)) || (S && !C && P(K_INSERT))) {
                std::string t = pasteText(), clean; for (char c : t) { if (c == '\n' || c == '\t') c = ' '; if (c >= 32 && c < 127) clean += c; }
                if (!clean.empty() || hasSel()) { push(-1); delSel(); if ((int)(v.size() + clean.size()) > maxLen) clean = clean.substr(0, std::max(0, maxLen - (int)v.size())); v.insert(caret, clean); caret += (int)clean.size(); changed = true; selAnchor = -1; fLastKind = -1; }
            }
            if ((cmd == CM_UNDO || (C && !S && P(K_Z))) && !fUndo.empty()) { fRedo.push_back({v, caret}); v = fUndo.back().v; caret = std::min(fUndo.back().caret, (int)v.size()); fUndo.pop_back(); selAnchor = -1; changed = true; fLastKind = -1; }
            if ((cmd == CM_REDO || (C && P(K_Y)) || (C && S && P(K_Z))) && !fRedo.empty()) { fUndo.push_back({v, caret}); v = fRedo.back().v; caret = std::min(fRedo.back().caret, (int)v.size()); fRedo.pop_back(); selAnchor = -1; changed = true; fLastKind = -1; }
            if (!C) for (char c : in->typed) if (c >= 32 && c < 127) {
                if (hasSel()) { push(-1); delSel(); }
                if ((int)v.size() < maxLen) { push(1); v.insert(v.begin() + caret, c); caret++; changed = true; if (c == ' ') fLastKind = -1; }
                selAnchor = -1;
            }
            if (P(K_BACKSPACE)) { if (hasSel()) { push(-1); delSel(); } else if (caret > 0) { int a = C ? txt::wordLeft(v, caret) : caret - 1; push(C ? -1 : 2); v.erase(a, caret - a); caret = a; changed = true; } selAnchor = -1; }
            if (P(K_DELETE) && !(S && !C)) { if (hasSel()) { push(-1); delSel(); } else if (caret < (int)v.size()) { int b = C ? txt::wordRight(v, caret) : caret + 1; push(C ? -1 : 2); v.erase(caret, b - caret); changed = true; } selAnchor = -1; }
            if (P(K_LEFT)) { if (hasSel() && !S && !C) { caret = std::min(selAnchor, caret); selAnchor = -1; } else move(C ? txt::wordLeft(v, caret) : caret - 1); }
            if (P(K_RIGHT)) { if (hasSel() && !S && !C) { caret = std::max(selAnchor, caret); selAnchor = -1; } else move(C ? txt::wordRight(v, caret) : caret + 1); }
            if (P(K_HOME)) move(0); if (P(K_END)) move((int)v.size());
            if (P(K_TAB)) { tabTarget = S ? id - 1 : id + 1; focus = -1; selAnchor = -1; }
            else if (P(K_ENTER) || P(K_ESCAPE)) { focus = -1; selAnchor = -1; }
            if (in->mousePressed[0] && !hv) { focus = -1; selAnchor = -1; }
        }
        // horizontal scroll: keeps its place until the caret leaves the box (Notepad-like), never shows space past the end
        int cw = font->width(v.substr(0, std::min(caret, (int)v.size())));
        if (f) { if (cw - fOff > inner) fOff = cw - inner; if (cw < fOff) fOff = std::max(0, cw - inner / 3); int tot = font->width(v); if (tot - fOff < inner) fOff = std::max(0, tot - inner); off = fOff; } else off = 0;
        cv->setClip({x + 2, y, w - 4, 16});
        if (f && hasSel()) { int a = std::min(selAnchor, caret), b = std::max(selAnchor, caret); int xa = x + 4 - off + font->width(v.substr(0, a)), xb = x + 4 - off + font->width(v.substr(0, b)); cv->fillRect(xa, y + 2, xb - xa, 12, ED::SEL); }
        font->draw(*cv, *ts, x + 4 - off, y + 2, v, ED::TEXT);
        if (f && ((int)(time * 2) % 2 == 0)) cv->fillRect(x + 4 - off + cw, y + 2, 1, 12, ED::TEXT);
        cv->resetClip();
        if (bigBtn) {   // the ... button: edit long text in a big box
            bool bh = hover(x + w - 16, y + 1, 15, 14);
            cv->fillRect(x + w - 16, y + 1, 15, 14, bh ? ED::HOVER : ED::PANEL2); cv->vline(x + w - 17, y + 1, 14, ED::BORDER);
            font->draw(*cv, *ts, x + w - 13, y + 1, "...", bh ? C::White : ED::ACCENT);
            if (bh) tip("Edit this text in a big box (Ctrl+E)");
        }
        return changed;
    }
    // 1.1.6 (#30): a search box - a text field that says what it searches when empty, with an x that clears it
    bool searchField(int x, int y, int w, std::string& v, const std::string& hint = "search...", int maxLen = 60) {
        bool ch = textField(x, y, v.empty() ? w : w - 16, v, maxLen);
        if (v.empty() && focus != idCounter) { cv->setClip({x + 2, y, w - 4, 16}); font->draw(*cv, *ts, x + 4, y + 2, hint, ED::DIM); cv->resetClip(); }
        if (!v.empty() && button(x + w - 15, y, 15, 16, "x")) { v.clear(); ch = true; blur(); }
        return ch;
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
    void drawTooltip() { if (tooltip.empty() || menuOpen) return; int w = font->width(tooltip) + 8; int x = std::min(tipX, cv->w - w - 2); cv->fillRect(x, tipY, w, 16, RGB(0, 0, 0, 220)); cv->rect(x, tipY, w, 16, ED::BORDER); font->draw(*cv, *ts, x + 4, tipY + 1, tooltip, C::White); }
};
