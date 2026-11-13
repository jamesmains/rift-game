// 1.1.11 (owner's #35: "a better text editor ... if it would appear in Notepad it needs it ... add what you think is good
// for the script editor ... a way to extend the text box"): the multi-line editor of the Dialogue and Scripts tabs and of
// the big box single-line fields open (Ctrl+E / "..."). Word wrap, word / line selection by mouse, Ctrl+arrows by word,
// undo + redo in steps, find / replace / go to line, line tools (move, duplicate, delete, comment, indent), a right-click
// menu, a status line, the "Wide" toggle (the owner of the TextEdit lays it out full width), and for scripts: indents
// that follow the blocks, word suggestions (Tab) and the live check's gutter dots. Keys: guide topic edit.text.
#include "editor.h"
#include "../game/script.h"
#include <algorithm>
#include <functional>
#include <filesystem>
#include <cmath>

namespace fs = std::filesystem;
using namespace ED;

namespace {
struct VRow { int line, a, b; bool last; };   // one row on screen: line's characters [a, b)
const std::set<std::string>& blockOpeners() { static const std::set<std::string> s = {"if", "elif", "elseif", "else", "while", "for"}; return s; }
const std::set<std::string>& blockClosers() { static const std::set<std::string> s = {"end", "endif", "endwhile", "endfor", "else", "elif", "elseif"}; return s; }
std::string firstWord(const std::string& l) { std::string w; size_t i = 0; while (i < l.size() && l[i] == ' ') i++; for (; i < l.size(); i++) { char ch = l[i]; if (isalnum((unsigned char)ch) || ch == '_') w += (char)tolower((unsigned char)ch); else break; } return w; }
}

void TextEdit::load(const std::string& p) {
    path = p; lines.clear(); std::string t; readFile(p, t);
    for (auto& l : split(t, '\n')) { std::string s = l; while (!s.empty() && s.back() == '\r') s.pop_back(); for (auto& c : s) if (c == '\t') c = ' '; lines.push_back(s); }
    if (lines.size() > 1 && lines.back().empty()) lines.pop_back();   // the file's last newline is not an extra line
    if (lines.empty()) lines.push_back("");
    cx = cy = scroll = scrollSub = hscroll = 0; ax = ay = -1; goalX = -1; dirty = false;
    undo.clear(); redo.clear(); lastKind = -1; marks.clear(); checkedHash = 0; editT = -10; acOpen = false; dragging = false;
}

void Editor::textEditWidget(TextEdit& t, int x, int y, int w, int h, InputState& in, int syntax) {
    int id = ui.nextId();
    if (t.refocus) { ui.focus = id; t.refocus = false; }
    t.fixCaret();
    bool f = ui.focus == id;
    const int rowH = 13, gutter = t.single ? 6 : 40, statusH = t.single ? 0 : 15, barH = t.bar ? (t.bar == 2 ? 40 : 21) : 0;
    // character widths (the font has no kerning: a string's width is the sum of its characters')
    int cw[128]; for (int c = 0; c < 128; c++) cw[c] = (c >= 32 && c < 127) ? font->width(std::string(1, (char)c)) : 5;
    auto chw = [&](char c) { unsigned char u = (unsigned char)c; return u < 128 ? cw[u] : 5; };
    auto spanW = [&](const std::string& s, int a, int b) { int px = 0; for (int i = a; i < b && i < (int)s.size(); i++) px += chw(s[i]); return px; };
    // ---- find / replace / go to line bar (above the text) ----
    int ty = y + barH, th = h - barH - statusH;            // text area
    int tx = x + gutter, tw = w - gutter - 4;              // text column (left edge of the characters, width)
    int visRows = std::max(1, (th - 4) / rowH);
    bool changedText = false, caretMoved = false;
    auto markEdit = [&]() { t.dirty = true; t.editT = ui.time; changedText = true; caretMoved = true; t.goalX = -1; };
    // ---- the rows on screen (word wrap breaks after spaces; a word longer than the row is cut) ----
    std::vector<VRow> rows; std::vector<int> firstRow(t.lines.size());
    auto layout = [&]() {
        rows.clear(); firstRow.assign(t.lines.size(), 0);
        int maxW = std::max(40, tw - 6);
        for (int li = 0; li < (int)t.lines.size(); li++) {
            const std::string& L = t.lines[li]; firstRow[li] = (int)rows.size();
            if (!t.wrap || spanW(L, 0, (int)L.size()) <= maxW) { rows.push_back({li, 0, (int)L.size(), true}); continue; }
            int a = 0, px = 0, lastBrk = -1;
            for (int i = 0; i < (int)L.size(); i++) {
                int c = chw(L[i]);
                if (px + c > maxW && i > a) {
                    int b = (lastBrk > a) ? lastBrk : i;
                    rows.push_back({li, a, b, false}); a = b; px = spanW(L, a, i); lastBrk = -1;
                }
                px += c; if (L[i] == ' ') lastBrk = i + 1;
            }
            rows.push_back({li, a, (int)L.size(), true});
        }
    };
    layout();
    auto rowOf = [&](int line, int col) { int r = firstRow[clampi(line, 0, (int)firstRow.size() - 1)]; while (r + 1 < (int)rows.size() && rows[r + 1].line == line && col >= rows[r + 1].a) r++; return r; };
    auto colAtX = [&](int r, int px) { const VRow& R = rows[r]; const std::string& L = t.lines[R.line]; int acc = 0, best = R.a; int end = R.last ? R.b : std::max(R.a, R.b - 1);
        for (int i = R.a; i <= end; i++) { if (acc - 2 <= px) best = i; if (i < (int)L.size()) acc += chw(L[i]); } return best; };
    auto topRow = [&]() { int fr = firstRow[clampi(t.scroll, 0, (int)firstRow.size() - 1)]; int n = 0; while (fr + n + 1 < (int)rows.size() && rows[fr + n + 1].line == rows[fr].line) n++; return fr + clampi(t.scrollSub, 0, n); };
    auto setTop = [&](int r) { r = clampi(r, 0, std::max(0, (int)rows.size() - visRows)); t.scroll = rows[r].line; t.scrollSub = r - firstRow[t.scroll]; };
    auto posFromMouse = [&](int& ox, int& oy) {
        int r = topRow() + (int)floorf((in.mouseY - ty - 2) / (float)rowH); r = clampi(r, 0, (int)rows.size() - 1);
        oy = rows[r].line; ox = colAtX(r, in.mouseX - tx + (t.wrap ? 0 : t.hscroll));
    };
    auto caretPx = [&]() { int r = rowOf(t.cy, t.cx); return spanW(t.lines[t.cy], rows[r].a, t.cx); };
    // selection lines [l0, l1] (a selection ending at column 0 of a line leaves that line out)
    auto selLines = [&](int& l0, int& l1) { if (!t.hasSel()) { l0 = l1 = t.cy; return; } int x0, y0, x1, y1; t.selRange(x0, y0, x1, y1); l0 = y0; l1 = (x1 == 0 && y1 > y0) ? y1 - 1 : y1; };
    auto setSel = [&](int x0, int y0, int x1, int y1) { t.ax = x0; t.ay = y0; t.cx = x1; t.cy = y1; t.fixCaret(); caretMoved = true; };
    auto selectLines = [&](int l0, int l1) { if (l1 + 1 < (int)t.lines.size()) setSel(0, l0, 0, l1 + 1); else setSel(0, l0, (int)t.lines[l1].size(), l1); };
    // ---- find ----
    auto countMatches = [&](int& index) { int n = 0; index = 0; int sx0 = 0, sy0 = -1, sx1, sy1; if (t.hasSel()) t.selRange(sx0, sy0, sx1, sy1);
        for (int li = 0; li < (int)t.lines.size(); li++) { int p = -1; while ((p = txt::findIn(t.lines[li], t.findText, p + 1, t.findCase, t.findWord)) >= 0) { n++; if (li == sy0 && p == sx0) index = n; } } return n; };
    auto findNext = [&](bool back, bool fromSelStart) {
        if (t.findText.empty()) return false;
        int x0 = t.cx, y0 = t.cy, x1, y1; if (t.hasSel()) { t.selRange(x0, y0, x1, y1); if (!back && !fromSelStart) { x0 = x1; y0 = y1; } }
        int n = (int)t.lines.size(), qlen = (int)t.findText.size();
        for (int k = 0; k <= n; k++) {
            int li = back ? ((y0 - k) % n + n) % n : (y0 + k) % n; const std::string& L = t.lines[li];
            int p = back ? txt::rfindIn(L, t.findText, k == 0 ? x0 : (int)L.size() + 1, t.findCase, t.findWord) : txt::findIn(L, t.findText, k == 0 ? x0 : 0, t.findCase, t.findWord);
            if (k == n && p >= 0 && !back && li == y0 && p >= x0) p = -1;   // came round to where it started
            if (p >= 0) { setSel(p, li, p + qlen, li); t.goalX = -1; return true; }
        }
        return false;
    };
    auto selIsMatch = [&]() { if (!t.hasSel()) return false; int x0, y0, x1, y1; t.selRange(x0, y0, x1, y1); return y0 == y1 && txt::findIn(t.lines[y0], t.findText, x0, t.findCase, t.findWord) == x0 && x1 - x0 == (int)t.findText.size(); };
    auto openBar = [&](int kind) {
        t.bar = kind; t.barFocus = true;
        if (kind != 3 && t.hasSel()) { int x0, y0, x1, y1; t.selRange(x0, y0, x1, y1); if (y0 == y1 && x1 > x0) t.findText = t.lines[y0].substr(x0, x1 - x0); }
        else if (kind != 3 && t.findText.empty()) { int a, b; txt::wordAt(t.lines[t.cy], t.cx, a, b); if (b > a && txt::cls(t.lines[t.cy][a]) == 1) t.findText = t.lines[t.cy].substr(a, b - a); }
        if (kind == 3) t.gotoText = std::to_string(t.cy + 1);
        t.findLast = t.findText;
    };
    bool barEnter = false, barShiftEnter = false;
    if (t.bar) {
        ui.cv->fillRect(x, y, w, barH, PANEL2); ui.cv->rect(x, y, w, barH, BORDER);
        int bx = x + 4, by = y + 3;
        auto field = [&](std::string& v, int fw) {   // a bar field; Enter in it is the bar's Enter, Esc closes the bar
            int fid = ui.idCounter + 1;
            if (t.barFocus) { ui.focus = fid; ui.selAnchor = 0; ui.caret = (int)v.size(); t.barFocus = false; }
            bool mine = ui.focus == fid;
            if (mine && in.pressed[K_ENTER]) { barEnter = true; barShiftEnter = in.shift(); in.pressed[K_ENTER] = false; }
            if (mine && in.pressed[K_ESCAPE]) { t.bar = 0; in.pressed[K_ESCAPE] = false; ui.focus = id; f = true; }
            if (mine && (in.pressed[K_F3])) { barEnter = true; barShiftEnter = in.shift(); }
            ui.textField(bx, by, fw, v, 120); bx += fw + 4; return mine;
        };
        if (t.bar == 3) {
            ui.label(bx, by + 2, "Go to line:"); bx += font->width("Go to line:") + 6;
            field(t.gotoText, 70);
            ui.text(bx, by + 2, fmt("(1 - %d)", (int)t.lines.size()), DIM); bx += 70;
            if (ui.button(bx, by, 40, 16, "Go") ) barEnter = true;
            if (barEnter) { std::string d; for (char c : t.gotoText) if (isdigit((unsigned char)c)) d += c; if (!d.empty()) { int n = clampi(atoi(d.c_str()), 1, (int)t.lines.size()); t.cy = n - 1; t.cx = 0; t.ay = -1; caretMoved = true; } t.bar = 0; ui.focus = id; f = true; }
        } else {
            ui.label(bx, by + 2, "Find:"); bx += 40;
            int fw = std::max(80, std::min(220, w - 330));
            field(t.findText, fw);
            if (ui.button(bx, by, 22, 16, "Aa", true, t.findCase)) t.findCase = !t.findCase;
            if (ui.hover(bx, by, 22, 16)) ui.tip("Match case");
            bx += 24;
            if (ui.button(bx, by, 22, 16, "W", true, t.findWord)) t.findWord = !t.findWord;
            if (ui.hover(bx, by, 22, 16)) ui.tip("Whole words only");
            bx += 24;
            bool prev = ui.button(bx, by, 20, 16, "<"); if (ui.hover(bx, by, 20, 16)) ui.tip("Previous (Shift+Enter / Shift+F3)"); bx += 22;
            bool next = ui.button(bx, by, 20, 16, ">"); if (ui.hover(bx, by, 20, 16)) ui.tip("Next (Enter / F3)"); bx += 24;
            if (t.findText != t.findLast) { t.findLast = t.findText; if (!t.findText.empty()) findNext(false, true); }   // as you type: from the current match
            if (barEnter || next) findNext(barShiftEnter && !next, false);
            if (prev) findNext(true, false);
            int idx = 0, n = t.findText.empty() ? 0 : countMatches(idx);
            ui.text(bx, by + 2, t.findText.empty() ? "" : n == 0 ? "no matches" : idx ? fmt("%d of %d", idx, n) : fmt("%d found", n), n == 0 && !t.findText.empty() ? DANGER : DIM);
            if (t.bar == 2) {
                int rx2 = x + 4, ry2 = y + 22;
                ui.label(rx2, ry2 + 2, "With:"); bx = rx2 + 40; by = ry2;
                field(t.replText, fw);
                if (ui.button(bx, by, 60, 16, "Replace", !t.findText.empty())) {
                    if (selIsMatch()) { int x0, y0, x1, y1; t.selRange(x0, y0, x1, y1); t.snapshot(); t.lines[y0].replace(x0, x1 - x0, t.replText); t.ay = -1; t.cx = x0 + (int)t.replText.size(); t.cy = y0; markEdit(); }
                    findNext(false, false);
                }
                if (ui.button(bx + 64, by, 80, 16, "Replace all", !t.findText.empty())) {
                    t.snapshot(); int cnt = 0;
                    for (auto& L : t.lines) { int p = 0; while ((p = txt::findIn(L, t.findText, p, t.findCase, t.findWord)) >= 0) { L.replace(p, t.findText.size(), t.replText); p += (int)t.replText.size(); cnt++; } }
                    if (cnt) { t.ay = -1; t.fixCaret(); markEdit(); } else t.undo.pop_back();
                    say(cnt ? fmt("Replaced %d.", cnt) : "Nothing to replace.");
                }
            }
        }
        if (ui.button(x + w - 20, y + 3, 16, 16, "x")) { t.bar = 0; ui.focus = id; f = true; }
        if (ui.hover(x + w - 20, y + 3, 16, 16)) ui.tip("Close (Esc)");
    }
    // ---- mouse ----
    ui.cv->fillRect(x, ty, w, th, FIELD); ui.cv->rect(x, ty, w, th, f ? ACCENT : BORDER);
    bool overText = ui.hover(x, ty, w, th);
    bool overSb = overText && (int)rows.size() > visRows && in.mouseX >= x + w - 8;   // the scroll bar takes its own clicks
    if (overSb && in.mousePressed[0] && !ui.mouseUsed) { ui.mouseUsed = true; ui.focus = id; f = true; }
    if (overText) {
        if (in.wheel) {
            if (in.shift() && !t.wrap) t.hscroll = std::max(0, t.hscroll - in.wheel * 40);
            else setTop(topRow() - in.wheel * 3);
        }
        if (in.mousePressed[0] && !ui.mouseUsed) {
            ui.mouseUsed = true; bool wasF = f; ui.focus = id; f = true; t.acOpen = false; t.goalX = -1;
            int ox, oy; posFromMouse(ox, oy);
            t.dragWord = t.dragLine = false;
            if (wasF && ui.clicks >= 3) { t.dragLine = true; t.dwy = oy; selectLines(oy, oy); }
            else if (wasF && ui.clicks == 2) { int a, b; txt::wordAt(t.lines[oy], ox, a, b); t.dragWord = true; t.dwy = oy; t.dwx0 = a; t.dwx1 = b; setSel(a, oy, b, oy); }
            else { if (in.shift()) { if (t.ay < 0) { t.ax = t.cx; t.ay = t.cy; } } else { t.ax = ox; t.ay = oy; } t.cx = ox; t.cy = oy; }
            t.dragging = true; t.lastKind = -1;
        }
        if (in.mousePressed[2] && !ui.mouseUsed) {   // right-click: a selection under the pointer stays
            ui.mouseUsed = true; ui.focus = id; f = true; t.acOpen = false;
            int ox, oy; posFromMouse(ox, oy);
            bool inSel = false; if (t.hasSel()) { int x0, y0, x1, y1; t.selRange(x0, y0, x1, y1); inSel = (oy > y0 || (oy == y0 && ox >= x0)) && (oy < y1 || (oy == y1 && ox <= x1)); }
            if (!inSel) { t.cx = ox; t.cy = oy; t.ay = -1; }
            bool s = t.hasSel(), multi = !t.single;
            std::vector<EdUI::MenuItem> m = {{EdUI::CM_UNDO, "Undo", "Ctrl+Z", !t.undo.empty()}, {EdUI::CM_REDO, "Redo", "Ctrl+Y", !t.redo.empty()}, EdUI::sep(),
                {EdUI::CM_CUT, "Cut", "Ctrl+X", true}, {EdUI::CM_COPY, "Copy", s ? "Ctrl+C" : "Ctrl+C (line)", true}, {EdUI::CM_PASTE, "Paste", "Ctrl+V", true}, {EdUI::CM_DELETE, "Delete", "Del", s}, EdUI::sep(),
                {EdUI::CM_SELWORD, "Select word", "double-click", true}, {EdUI::CM_SELLINE, "Select line", "Ctrl+L", true}, {EdUI::CM_SELALL, "Select all", "Ctrl+A", true}, EdUI::sep(),
                {EdUI::CM_FIND, "Find...", "Ctrl+F", true}, {EdUI::CM_REPLACE, "Replace...", "Ctrl+H", true}};
            if (multi) m.push_back({EdUI::CM_GOTO, "Go to line...", "Ctrl+G", true});
            m.push_back(EdUI::sep());
            if (multi) { m.push_back({EdUI::CM_COMMENT, "Comment / uncomment", "Ctrl+/", true}); m.push_back({EdUI::CM_INDENT, "Indent", "Tab", true}); m.push_back({EdUI::CM_OUTDENT, "Outdent", "Shift+Tab", true});
                         m.push_back({EdUI::CM_DUPLINE, "Duplicate line", "Ctrl+D", true}); m.push_back({EdUI::CM_DELLINE, "Delete line", "Ctrl+Shift+K", true}); }
            m.push_back({EdUI::CM_UPPER, "UPPER CASE", "Ctrl+Shift+U", s}); m.push_back({EdUI::CM_LOWER, "lower case", "Ctrl+U", s});
            if (!t.single) { m.push_back(EdUI::sep()); m.push_back({EdUI::CM_WRAP, t.wrap ? "Word wrap: on" : "Word wrap: off", "Alt+Z", true}); m.push_back({EdUI::CM_WIDE, t.wide ? "Wide editor: on" : "Wide editor: off", "Ctrl+E", true}); }
            ui.openMenu(id, m);
        }
        // hovering a gutter dot shows what the check found on that line
        if (!t.single && in.mouseX < tx && !t.marks.empty()) { int r = topRow() + (in.mouseY - ty - 2) / rowH; if (r >= 0 && r < (int)rows.size()) { auto it = t.marks.find(rows[r].line); if (it != t.marks.end()) ui.tip((it->second.first ? "Error: " : "Warning: ") + it->second.second); } }
    } else if (in.mousePressed[0] && f && !ui.modal && !ui.hover(x, y, w, h)) { ui.focus = -1; f = false; t.acOpen = false; }
    if (t.dragging) {
        if (!in.mouseDown[0]) t.dragging = false;
        else {
            // past the top / bottom edge: scroll while the button is held
            if (in.mouseY < ty) setTop(topRow() - 1); else if (in.mouseY >= ty + th) setTop(topRow() + 1);
            if (in.mouseDX || in.mouseDY || in.mouseY < ty || in.mouseY >= ty + th) {
                int ox, oy; posFromMouse(ox, oy);
                if (t.dragLine) { if (oy < t.dwy) { int l1 = t.dwy; t.ax = l1 + 1 < (int)t.lines.size() ? 0 : (int)t.lines[l1].size(); t.ay = l1 + 1 < (int)t.lines.size() ? l1 + 1 : l1; t.cx = 0; t.cy = oy; } else selectLines(t.dwy, oy); }
                else if (t.dragWord) { int a, b; txt::wordAt(t.lines[oy], ox, a, b); if (oy < t.dwy || (oy == t.dwy && ox < t.dwx0)) setSel(t.dwx1, t.dwy, a, oy); else setSel(t.dwx0, t.dwy, (oy == t.dwy ? std::max(b, t.dwx1) : b), oy); }
                else { t.cx = ox; t.cy = oy; }
                caretMoved = true;
            }
        }
    }
    // ---- keys ----
    int cmd = ui.takeCmd(id);
    if (cmd) { f = true; ui.focus = id; }
    if (f && !ui.modal) {
        bool C = in.ctrl(), S = in.shift(), A = in.down[K_ALT];
        auto P = [&](int k) { return in.pressed[k]; };
        auto startMove = [&]() { if (S) { if (t.ay < 0) { t.ax = t.cx; t.ay = t.cy; } } else t.ay = -1; caretMoved = true; t.lastKind = -1; t.acOpen = false; };
        auto cleanText = [&](const std::string& s0) { std::string o; for (char c : s0) { if (c == '\r') continue; if (c == '\t') { o += "  "; continue; } if (t.single && c == '\n') c = ' '; if (c == '\n' || (c >= 32 && c < 127)) o += c; } return o; };
        // -- suggestions box (scripts): Up/Down choose, Tab takes, Esc closes; everything else goes on as usual --
        bool acTook = false;
        if (t.acOpen && !t.ac.empty()) {
            if (P(K_DOWN)) { t.acSel = (t.acSel + 1) % (int)t.ac.size(); in.pressed[K_DOWN] = false; }
            if (P(K_UP)) { t.acSel = (t.acSel + (int)t.ac.size() - 1) % (int)t.ac.size(); in.pressed[K_UP] = false; }
            if (P(K_ESCAPE)) { t.acOpen = false; in.pressed[K_ESCAPE] = false; }
            if (P(K_TAB) && !S) { std::string& L = t.lines[t.cy]; t.snapshot(); L.replace(t.acStart, t.cx - t.acStart, t.ac[t.acSel]); t.cx = t.acStart + (int)t.ac[t.acSel].size(); t.ay = -1; t.acOpen = false; in.pressed[K_TAB] = false; markEdit(); acTook = true; }
        }
        (void)acTook;
        // -- commands (keys and the right-click menu) --
        auto doCopy = [&](bool cut) {
            if (t.hasSel()) { ui.copyText(t.selText()); if (cut) { t.snapshot(); t.deleteSel(); markEdit(); } return; }
            if (t.single) return;
            ui.copyText(t.lines[t.cy] + "\n");   // nothing selected: the whole line (as in VS Code)
            if (cut) { t.snapshot(); if (t.lines.size() > 1) t.lines.erase(t.lines.begin() + t.cy); else t.lines[0].clear(); t.cx = 0; t.fixCaret(); markEdit(); }
        };
        auto lineEdit = [&](const std::function<void(int, int)>& fn) { int l0, l1; selLines(l0, l1); t.snapshot(); fn(l0, l1); t.fixCaret(); markEdit(); };
        auto indentLines = [&](bool out) { lineEdit([&](int l0, int l1) {
            for (int li = l0; li <= l1; li++) { std::string& L = t.lines[li]; int d = 0;
                if (out) { d = -std::min(2, txt::indentOf(L)); L.erase(0, -d); } else if (!L.empty() || l0 == l1) { L.insert(0, "  "); d = 2; }
                if (li == t.cy) t.cx = std::max(0, t.cx + d); if (li == t.ay && t.ax > 0) t.ax = std::max(0, t.ax + d); } }); };
        auto toggleComment = [&]() { lineEdit([&](int l0, int l1) {
            bool all = true; int ind = 1 << 30;
            for (int li = l0; li <= l1; li++) { std::string tl = trim(t.lines[li]); if (tl.empty()) continue; ind = std::min(ind, txt::indentOf(t.lines[li])); if (tl[0] != '#') all = false; }
            if (ind == (1 << 30)) { ind = txt::indentOf(t.lines[l0]); all = false; }
            for (int li = l0; li <= l1; li++) { std::string& L = t.lines[li]; if (trim(L).empty() && l0 != l1) continue;
                int d; if (all) { int p = txt::indentOf(L); int n = (p + 1 < (int)L.size() && L[p + 1] == ' ') ? 2 : 1; L.erase(p, n); d = -n; } else { L.insert(std::min(ind, (int)L.size()), "# "); d = 2; }
                if (li == t.cy) t.cx = std::max(0, t.cx + d); if (li == t.ay && t.ax > 0) t.ax = std::max(0, t.ax + d); } }); };
        auto moveLines = [&](int dir) { int l0, l1; selLines(l0, l1); if ((dir < 0 && l0 == 0) || (dir > 0 && l1 + 1 >= (int)t.lines.size())) return;
            t.snapshot(); if (dir < 0) { std::string ab = t.lines[l0 - 1]; t.lines.erase(t.lines.begin() + l0 - 1); t.lines.insert(t.lines.begin() + l1, ab); } else { std::string be = t.lines[l1 + 1]; t.lines.erase(t.lines.begin() + l1 + 1); t.lines.insert(t.lines.begin() + l0, be); }
            t.cy += dir; if (t.ay >= 0) t.ay += dir; markEdit(); };
        auto changeCase = [&](bool up) { if (!t.hasSel()) return; int x0, y0, x1, y1; t.selRange(x0, y0, x1, y1); t.snapshot();
            for (int li = y0; li <= y1; li++) { int a = li == y0 ? x0 : 0, b = li == y1 ? x1 : (int)t.lines[li].size(); for (int i = a; i < b; i++) t.lines[li][i] = (char)(up ? toupper((unsigned char)t.lines[li][i]) : tolower((unsigned char)t.lines[li][i])); }
            markEdit(); caretMoved = false; };
        auto paste = [&]() { std::string p = cleanText(ui.pasteText()); if (p.empty()) return; t.snapshot(); t.deleteSel(); t.insert(p); markEdit(); };
        if (cmd == EdUI::CM_UNDO || (C && !S && P(K_Z))) { if (t.undoStep()) { caretMoved = true; t.editT = ui.time; changedText = true; } }
        if (cmd == EdUI::CM_REDO || (C && P(K_Y)) || (C && S && P(K_Z))) { if (t.redoStep()) { caretMoved = true; t.editT = ui.time; changedText = true; } }
        if (cmd == EdUI::CM_COPY || (C && (P(K_C) || P(K_INSERT)))) doCopy(false);
        if (cmd == EdUI::CM_CUT || (C && P(K_X)) || (S && !C && P(K_DELETE))) { doCopy(true); in.pressed[K_DELETE] = false; }
        if (cmd == EdUI::CM_PASTE || (C && P(K_V)) || (S && !C && P(K_INSERT))) paste();
        if (cmd == EdUI::CM_DELETE && t.hasSel()) { t.snapshot(); t.deleteSel(); markEdit(); }
        if (cmd == EdUI::CM_SELALL || (C && P(K_A))) { t.ax = 0; t.ay = 0; t.cy = (int)t.lines.size() - 1; t.cx = (int)t.lines[t.cy].size(); }
        if (cmd == EdUI::CM_SELWORD) { int a, b; txt::wordAt(t.lines[t.cy], t.cx, a, b); setSel(a, t.cy, b, t.cy); }
        if (cmd == EdUI::CM_SELLINE || (C && !S && P(K_L))) { int l0, l1; selLines(l0, l1); if (t.hasSel() && t.ax == 0 && t.cx == 0 && t.cy == l1 + 1 && l1 + 1 < (int)t.lines.size()) l1++; selectLines(t.hasSel() ? l0 : t.cy, l1); }
        if (cmd == EdUI::CM_FIND || (C && P(K_F))) openBar(1);
        if (cmd == EdUI::CM_REPLACE || (C && P(K_H))) openBar(2);
        if (!t.single && (cmd == EdUI::CM_GOTO || (C && P(K_G)))) openBar(3);
        if (P(K_F3)) { if (t.findText.empty()) openBar(1); else findNext(S, false); }
        if (!t.single) {
            if (cmd == EdUI::CM_COMMENT || (C && P(K_SLASH))) toggleComment();
            if (cmd == EdUI::CM_INDENT || (C && P(K_RBRACKET))) indentLines(false);
            if (cmd == EdUI::CM_OUTDENT || (C && P(K_LBRACKET))) indentLines(true);
            if (cmd == EdUI::CM_DUPLINE || (C && !S && P(K_D))) { lineEdit([&](int l0, int l1) { std::vector<std::string> blk(t.lines.begin() + l0, t.lines.begin() + l1 + 1); t.lines.insert(t.lines.begin() + l1 + 1, blk.begin(), blk.end()); int n = l1 - l0 + 1; t.cy += n; if (t.ay >= 0) t.ay += n; }); }
            if (cmd == EdUI::CM_DELLINE || (C && S && P(K_K))) { lineEdit([&](int l0, int l1) { t.lines.erase(t.lines.begin() + l0, t.lines.begin() + l1 + 1); if (t.lines.empty()) t.lines.push_back(""); t.cy = std::min(l0, (int)t.lines.size() - 1); t.cx = std::min(t.cx, (int)t.lines[t.cy].size()); t.ay = -1; }); }
            if (A && P(K_UP)) { moveLines(-1); in.pressed[K_UP] = false; }
            if (A && P(K_DOWN)) { moveLines(1); in.pressed[K_DOWN] = false; }
            if (cmd == EdUI::CM_WRAP || (A && P(K_Z))) { t.wrap = !t.wrap; t.hscroll = 0; caretMoved = true; say(t.wrap ? "Word wrap on." : "Word wrap off."); }
            if (cmd == EdUI::CM_WIDE || (C && P(K_E))) { t.wide = !t.wide; t.refocus = true; caretMoved = true; }
        }
        if (cmd == EdUI::CM_UPPER || (C && S && P(K_U))) changeCase(true);
        else if (cmd == EdUI::CM_LOWER || (C && !S && P(K_U))) changeCase(false);
        if (C && P(K_S) && !t.single && !t.path.empty()) { if (t.save()) { changed = true; say("Saved " + t.path); } }
        if (P(K_INSERT) && !C && !S) t.overwrite = !t.overwrite;
        // -- typing --
        if (!C && !A) {
            bool typedAny = false; for (char c : in.typed) if (c >= 32 && c < 127) typedAny = true;
            if (typedAny) {
                if (t.hasSel()) { t.snapshot(); t.deleteSel(); }
                for (char c : in.typed) if (c >= 32 && c < 127) {
                    t.snapshot(1); std::string& L = t.lines[t.cy];
                    if (t.overwrite && t.cx < (int)L.size()) L[t.cx] = c; else L.insert(L.begin() + t.cx, c);
                    t.cx++; if (c == ' ') t.lastKind = -1;
                }
                markEdit();
                // scripts: "end" / "else" / "elif" typed on their own line step back to the block's indent
                if (syntax == 1) { std::string& L = t.lines[t.cy]; std::string tl = trim(L); int ind = txt::indentOf(L);
                    if (t.cx == (int)L.size() && ind > 0 && (tl == "end" || tl == "else" || tl == "elif" || tl == "endif" || tl == "endwhile" || tl == "endfor")) {
                        int pl = t.cy - 1; while (pl >= 0 && trim(t.lines[pl]).empty()) pl--;
                        if (pl >= 0) { int pind = txt::indentOf(t.lines[pl]); int target = blockOpeners().count(firstWord(t.lines[pl])) ? pind : pind - 2;
                            if (target >= 0 && target < ind) { L.erase(0, ind - target); t.cx -= ind - target; } } } }
            }
            if (P(K_TAB)) {
                int l0, l1; selLines(l0, l1);
                if (S || (t.hasSel() && l1 > l0)) indentLines(S);
                else { t.snapshot(); t.deleteSel(); int n = 2 - (t.cx % 2); t.lines[t.cy].insert(t.cx, std::string(n, ' ')); t.cx += n; markEdit(); }
            }
            if (P(K_ENTER) && !t.single) {
                t.snapshot(); t.deleteSel(); std::string& L = t.lines[t.cy];
                std::string left = L.substr(0, t.cx), rest = L.substr(t.cx);
                int ind = std::min(txt::indentOf(left), (int)left.size()); if (trim(left).empty()) ind = (int)left.size();
                if (syntax == 1 && blockOpeners().count(firstWord(left)) && !trim(left).empty()) ind += 2;
                while (!rest.empty() && rest[0] == ' ' && ind > 0 && !trim(left).empty()) rest.erase(0, 1);
                L = left; t.lines.insert(t.lines.begin() + t.cy + 1, std::string(ind, ' ') + rest); t.cy++; t.cx = ind; markEdit(); t.acOpen = false;
            }
            if (P(K_BACKSPACE)) {
                if (t.hasSel()) { t.snapshot(); t.deleteSel(); markEdit(); }
                else { std::string& L = t.lines[t.cy];
                    if (t.cx > 0) { int a = t.cx - 1; if (txt::indentOf(L) >= t.cx && t.cx >= 2) a = t.cx - (t.cx % 2 ? 1 : 2); t.snapshot(2); L.erase(a, t.cx - a); t.cx = a; markEdit(); }   // in the indent: back to the previous step
                    else if (t.cy > 0) { t.snapshot(); int nl = (int)t.lines[t.cy - 1].size(); t.lines[t.cy - 1] += L; t.lines.erase(t.lines.begin() + t.cy); t.cy--; t.cx = nl; markEdit(); } }
            }
            if (P(K_DELETE)) {
                if (t.hasSel()) { t.snapshot(); t.deleteSel(); markEdit(); }
                else { std::string& L2 = t.lines[t.cy]; if (t.cx < (int)L2.size()) { t.snapshot(2); L2.erase(t.cx, 1); markEdit(); } else if (t.cy + 1 < (int)t.lines.size()) { t.snapshot(); L2 += t.lines[t.cy + 1]; t.lines.erase(t.lines.begin() + t.cy + 1); markEdit(); } }
            }
        } else if (C) {
            // Ctrl+Backspace / Ctrl+Delete: a word
            if (P(K_BACKSPACE)) { if (t.hasSel()) { t.snapshot(); t.deleteSel(); markEdit(); } else if (t.cx > 0) { int a = txt::wordLeft(t.lines[t.cy], t.cx); t.snapshot(); t.lines[t.cy].erase(a, t.cx - a); t.cx = a; markEdit(); } else if (t.cy > 0) { t.snapshot(); t.cx = (int)t.lines[t.cy - 1].size(); t.lines[t.cy - 1] += t.lines[t.cy]; t.lines.erase(t.lines.begin() + t.cy); t.cy--; markEdit(); } }
            if (P(K_DELETE) && !S) { if (t.hasSel()) { t.snapshot(); t.deleteSel(); markEdit(); } else if (t.cx < (int)t.lines[t.cy].size()) { int b = txt::wordRight(t.lines[t.cy], t.cx); t.snapshot(); t.lines[t.cy].erase(t.cx, b - t.cx); markEdit(); } else if (t.cy + 1 < (int)t.lines.size()) { t.snapshot(); t.lines[t.cy] += t.lines[t.cy + 1]; t.lines.erase(t.lines.begin() + t.cy + 1); markEdit(); } }
        }
        // -- moving the caret (Shift selects; Ctrl = by word / to the ends) --
        if (changedText) { t.fixCaret(); layout(); }
        if (!A) {
            int r = rowOf(t.cy, t.cx);
            if (P(K_LEFT)) { if (t.hasSel() && !S && !C) { int x0, y0, x1, y1; t.selRange(x0, y0, x1, y1); t.ay = -1; t.cx = x0; t.cy = y0; caretMoved = true; }
                else { startMove(); if (C) { if (t.cx == 0 && t.cy > 0) { t.cy--; t.cx = (int)t.lines[t.cy].size(); } else t.cx = txt::wordLeft(t.lines[t.cy], t.cx); } else if (t.cx > 0) t.cx--; else if (t.cy > 0) { t.cy--; t.cx = (int)t.lines[t.cy].size(); } }
                t.goalX = -1; }
            if (P(K_RIGHT)) { if (t.hasSel() && !S && !C) { int x0, y0, x1, y1; t.selRange(x0, y0, x1, y1); t.ay = -1; t.cx = x1; t.cy = y1; caretMoved = true; }
                else { startMove(); if (C) { if (t.cx == (int)t.lines[t.cy].size() && t.cy + 1 < (int)t.lines.size()) { t.cy++; t.cx = 0; } else t.cx = txt::wordRight(t.lines[t.cy], t.cx); } else if (t.cx < (int)t.lines[t.cy].size()) t.cx++; else if (t.cy + 1 < (int)t.lines.size()) { t.cy++; t.cx = 0; } }
                t.goalX = -1; }
            auto vmove = [&](int dr) { startMove(); if (t.goalX < 0) t.goalX = caretPx(); int nr = clampi(r + dr, 0, (int)rows.size() - 1); if (nr == r) { if (dr < 0) t.cx = 0; else t.cx = (int)t.lines[t.cy].size(); return; } t.cy = rows[nr].line; t.cx = colAtX(nr, t.goalX); };
            if (C && !S && P(K_UP)) setTop(topRow() - 1);           // Ctrl+Up / Down: scroll, the caret stays
            else if (P(K_UP)) vmove(-1);
            if (C && !S && P(K_DOWN)) setTop(topRow() + 1);
            else if (P(K_DOWN)) vmove(1);
            if (P(K_PAGEUP)) { int keep = r - topRow(); vmove(-visRows); setTop(rowOf(t.cy, t.cx) - keep); }
            if (P(K_PAGEDOWN)) { int keep = r - topRow(); vmove(visRows); setTop(rowOf(t.cy, t.cx) - keep); }
            if (P(K_HOME)) { startMove(); if (C) { t.cy = 0; t.cx = 0; } else { const VRow& R = rows[r]; int ind = txt::indentOf(t.lines[t.cy]); if (R.a > 0) t.cx = R.a; else t.cx = (t.cx == ind) ? 0 : ind; } t.goalX = -1; }   // Home: first letter, again: column 0
            if (P(K_END)) { startMove(); if (C) { t.cy = (int)t.lines.size() - 1; t.cx = (int)t.lines[t.cy].size(); } else { const VRow& R = rows[r]; t.cx = R.last ? R.b : std::max(R.a, R.b - 1); } t.goalX = -1; }
        }
        if (P(K_ESCAPE)) { if (t.acOpen) t.acOpen = false; else if (t.bar) t.bar = 0; else if (t.hasSel()) t.ay = -1; else if (!t.single) ui.focus = -1; }
        t.fixCaret();
        if (t.ay >= 0) { t.ay = clampi(t.ay, 0, (int)t.lines.size() - 1); t.ax = clampi(t.ax, 0, (int)t.lines[t.ay].size()); }
        // -- script suggestions: after typing part of a word (2+ letters), words that start with it --
        if (syntax == 1 && changedText) {
            const std::string& L = t.lines[t.cy]; int a = t.cx; while (a > 0 && txt::cls(L[a - 1]) == 1) a--;
            bool inComment = trim(L).size() && trim(L)[0] == '#';
            std::string pre = L.substr(a, t.cx - a);
            bool typedWord = false; for (char c : in.typed) if (txt::cls(c) == 1) typedWord = true;
            if (typedWord && !inComment && pre.size() >= 2 && !isdigit((unsigned char)pre[0]) && (t.cx >= (int)L.size() || txt::cls(L[t.cx]) != 1)) { t.ac = scriptWords(t, pre); t.acStart = a; t.acSel = 0; t.acOpen = !t.ac.empty(); }
            else if (!typedWord) t.acOpen = false;
        }
    }
    if (changedText || caretMoved) layout();
    // ---- keep the caret on screen after it moved ----
    if (caretMoved) {
        int cr = rowOf(t.cy, t.cx), top = topRow();
        if (cr < top) setTop(cr); else if (cr >= top + visRows) setTop(cr - visRows + 1);
        if (!t.wrap) { int cp = caretPx(); if (cp - t.hscroll > tw - 12) t.hscroll = cp - tw + 40; if (cp < t.hscroll) t.hscroll = std::max(0, cp - 40); }
    }
    if (t.wrap) t.hscroll = 0;
    setTop(topRow());   // clamps scroll after edits / external jumps
    // ---- draw ----
    int top = topRow(); int hoff = t.wrap ? 0 : t.hscroll;
    int sx0 = 0, sy0 = -1, sx1 = 0, sy1 = -1; if (t.hasSel()) t.selRange(sx0, sy0, sx1, sy1);
    // a selected whole word: its other copies get a faint box (as in most code editors)
    std::string hiWord; if (sy0 >= 0 && sy0 == sy1 && sx1 - sx0 >= 2) { int a, b; txt::wordAt(t.lines[sy0], sx0, a, b); if (a == sx0 && b == sx1 && txt::cls(t.lines[sy0][a]) == 1) hiWord = t.lines[sy0].substr(a, b - a); }
    // bracket under / before the caret and its partner on the same line
    int brA = -1, brB = -1;
    { const std::string& L = t.lines[t.cy]; static const std::string open = "([{", close = ")]}";
      auto tryAt = [&](int p) { if (p < 0 || p >= (int)L.size()) return false; size_t o = open.find(L[p]), c = close.find(L[p]); if (o == std::string::npos && c == std::string::npos) return false;
          int dir = o != std::string::npos ? 1 : -1; char me = L[p], other = o != std::string::npos ? close[o] : open[c]; int depth = 0;
          for (int i = p; i >= 0 && i < (int)L.size(); i += dir) { if (L[i] == me) depth++; else if (L[i] == other && --depth == 0) { brA = p; brB = i; return true; } } return false; };
      if (f && !tryAt(t.cx)) tryAt(t.cx - 1); }
    Color8 curLine = RGB(40, 44, 56);
    ui.cv->setClip({x + 1, ty + 1, w - 2, th - 2});
    static const std::set<std::string> ctl = {"if", "elif", "elseif", "else", "end", "endif", "endwhile", "endfor", "while", "for", "break", "continue", "stop", "return", "call", "start", "wait"};
    static const std::set<std::string> talk = {"say", "speaker", "option", "ask", "pick"};
    for (int r = 0; r < visRows && top + r < (int)rows.size(); r++) {
        const VRow& R = rows[top + r]; int i = R.line; int ry = ty + 2 + r * rowH; const std::string& L = t.lines[i];
        if (i == t.cy && f) ui.cv->fillRect(x + 1, ry - 1, w - 2, rowH, curLine);
        Color8 col = TEXT; std::string tl = trim(L);
        if (syntax == 1) {   // 1.1.10 scripts: control words, talk, variables, actions
            std::string w0 = firstWord(L);
            if (!tl.empty() && tl[0] == '#') col = DIM; else if (ctl.count(w0)) col = ACCENT2; else if (talk.count(w0)) col = RGB(150, 220, 150); else if (w0 == "let" || w0 == "keep") col = ACCENT; else if (isScriptAction(w0)) col = C::White;
        }
        else if (t.single) col = TEXT;
        else if (!tl.empty() && tl[0] == '[') col = ACCENT2; else if (!tl.empty() && tl[0] == '#') col = DIM; else if (startsWith(tl, "option=")) col = RGB(150, 220, 150); else if (startsWith(tl, "text=")) col = C::White; else if (startsWith(tl, "speaker=")) col = ACCENT;
        ui.cv->setClip({tx - 2, ty + 1, w - gutter - 1 + 2, th - 2});
        int bx0 = tx - hoff;   // x of the row's first character
        auto xAt = [&](int c) { return bx0 + spanW(L, R.a, c); };
        // find matches and copies of the selected word
        auto boxAll = [&](const std::string& q, bool cs, bool ww, Color8 c, bool fill) { if (q.empty()) return; int p = R.a - 1;
            while ((p = txt::findIn(L, q, p + 1, cs, ww)) >= 0 && p < R.b) { int xa = xAt(p), xb = xAt(std::min(R.b, p + (int)q.size())); if (fill) ui.cv->fillRect(xa, ry - 1, std::max(2, xb - xa), rowH, c); else ui.cv->rect(xa - 1, ry - 1, xb - xa + 2, rowH, c); } };
        if (t.bar == 1 || t.bar == 2) boxAll(t.findText, t.findCase, t.findWord, RGB(110, 90, 30), true);
        if (!hiWord.empty()) boxAll(hiWord, true, true, RGB(90, 100, 130), false);
        if (sy0 >= 0 && i >= sy0 && i <= sy1) {
            int a = i == sy0 ? std::max(sx0, R.a) : R.a, b = i == sy1 ? std::min(sx1, R.b) : R.b;
            bool past = i != sy1 && R.last;   // the selection goes on past this line's end
            if (b > a || past || (a == b && i > sy0 && i < sy1)) { int xa = xAt(a), xb = xAt(b) + (past ? 6 : 0); if (b >= a) ui.cv->fillRect(xa, ry - 1, std::max(2, xb - xa), rowH, SEL); }
        }
        if (brA >= 0 && i == t.cy) for (int p : {brA, brB}) if (p >= R.a && p < R.b) ui.cv->rect(xAt(p) - 1, ry - 1, chw(L[p]) + 2, rowH, ACCENT);
        font->draw(*ui.cv, *ts, bx0, ry, L.substr(R.a, R.b - R.a), col);
        if (i == t.cy && f && rowOf(t.cy, t.cx) == top + r && ((int)(ui.time * 2) % 2 == 0)) {
            int cxp = xAt(t.cx);
            if (t.overwrite) ui.cv->fillRect(cxp, ry + 10, t.cx < (int)L.size() ? chw(L[t.cx]) : 5, 2, C::White); else ui.cv->fillRect(cxp, ry, 1, 12, C::White);
        }
        if (!t.single) {
            ui.cv->setClip({x + 1, ty + 1, w - 2, th - 2});
            ui.cv->fillRect(x + 1, ry - 1, gutter - 3, rowH, i == t.cy && f ? curLine : FIELD);
            if (R.a == 0) font->draw(*ui.cv, *ts, x + 9, ry, std::to_string(i + 1), i == t.cy ? TEXT : DIM);
            else font->draw(*ui.cv, *ts, x + gutter - 12, ry, "-", RGB(80, 86, 100));   // wrapped on from the line above
            auto mk = t.marks.find(i); if (mk != t.marks.end() && R.a == 0) ui.cv->fillRect(x + 3, ry + 3, 4, 5, mk->second.first ? DANGER : ACCENT2);
        }
    }
    ui.cv->resetClip();
    // vertical scroll bar (when the text is longer than the box)
    if ((int)rows.size() > visRows) {
        int sbx = x + w - 5, sh = th - 4, kh = std::max(12, sh * visRows / (int)rows.size()), ky = ty + 2 + (sh - kh) * top / std::max(1, (int)rows.size() - visRows);
        ui.cv->fillRect(sbx, ty + 2, 3, sh, PANEL); ui.cv->fillRect(sbx, ky, 3, kh, DIM);
        for (auto& mk : t.marks) { if (mk.first >= (int)t.lines.size()) continue; int my = ty + 2 + sh * firstRow[mk.first] / std::max(1, (int)rows.size()); ui.cv->fillRect(sbx - 2, my, 5, 2, mk.second.first ? DANGER : ACCENT2); }
        if (ui.hover(sbx - 3, ty, 9, th) && in.mouseDown[0] && !t.dragging) setTop((in.mouseY - ty) * (int)rows.size() / std::max(1, th) - visRows / 2);
    }
    // suggestions box under the caret
    if (t.acOpen && f && !t.ac.empty()) {
        int cr = rowOf(t.cy, t.cx) - top;
        if (cr >= 0 && cr < visRows) {
            int bw = font->width("Tab: take  Esc: close") + 12; for (auto& s : t.ac) bw = std::max(bw, font->width(s) + 16);
            int n = std::min(8, (int)t.ac.size()), bh = n * 13 + 16, bx = std::min(x + w - bw - 4, tx - hoff + spanW(t.lines[t.cy], rows[rowOf(t.cy, t.cx)].a, t.acStart)), by = ty + 2 + (cr + 1) * rowH;
            if (by + bh > ty + th) by = ty + 2 + cr * rowH - bh;
            int first = std::max(0, std::min(t.acSel - n + 1, (int)t.ac.size() - n));
            ui.cv->fillRect(bx, by, bw, bh, PANEL2); ui.cv->rect(bx, by, bw, bh, ACCENT);
            for (int k = 0; k < n; k++) { int ii = first + k, iy = by + 2 + k * 13; bool sel = ii == t.acSel;
                if (sel) ui.cv->fillRect(bx + 1, iy, bw - 2, 13, SEL);
                font->draw(*ui.cv, *ts, bx + 6, iy, t.ac[ii], sel ? C::White : TEXT);
                if (ui.hover(bx, iy, bw, 13) && in.mousePressed[0]) { std::string& L = t.lines[t.cy]; t.snapshot(); L.replace(t.acStart, t.cx - t.acStart, t.ac[ii]); t.cx = t.acStart + (int)t.ac[ii].size(); t.acOpen = false; markEdit(); in.mousePressed[0] = false; } }
            font->draw(*ui.cv, *ts, bx + 6, by + bh - 13, "Tab: take  Esc: close", DIM);
        }
    }
    // ---- status line: where the caret is, the check's note on this line, wrap / wide toggles ----
    if (statusH) {
        int sy = y + h - statusH;
        ui.cv->fillRect(x, sy, w, statusH, PANEL2);
        std::string st = fmt("Ln %d, Col %d", t.cy + 1, t.cx + 1);
        if (t.hasSel()) { std::string s = t.selText(); int nl = 0; for (char c : s) if (c == '\n') nl++; st += nl ? fmt("  (%d lines)", nl + 1) : fmt("  (%d chars)", (int)s.size()); }
        if (t.overwrite) st += "  OVR";
        ui.text(x + 4, sy + 2, st, DIM);
        int bx = x + w - 4;
        if (!t.single) {
            bx -= 46; if (ui.button(bx, sy + 1, 44, 13, t.wide ? "Narrow" : "Wide", true, t.wide)) { t.wide = !t.wide; t.refocus = true; }
            if (ui.hover(bx, sy + 1, 44, 13)) ui.tip(t.wide ? "Back to the normal layout (Ctrl+E)" : "Make the editor as wide as the window (Ctrl+E)");
            bx -= 40; if (ui.button(bx, sy + 1, 38, 13, "Wrap", true, t.wrap)) { t.wrap = !t.wrap; t.hscroll = 0; ui.focus = id; }
            if (ui.hover(bx, sy + 1, 38, 13)) ui.tip("Word wrap: long lines continue on the next row (Alt+Z)");
            bx -= 40; if (ui.button(bx, sy + 1, 38, 13, "Find")) { openBar(1); }
            if (ui.hover(bx, sy + 1, 38, 13)) ui.tip("Find (Ctrl+F), replace (Ctrl+H), go to line (Ctrl+G). Right-click the text for everything else.");
        }
        auto mk = t.marks.find(t.cy);
        int lx = x + 6 + font->width(st) + 12, lw = bx - lx - 6;
        if (mk != t.marks.end() && lw > 40) { ui.cv->setClip({lx, sy, lw, statusH}); ui.text(lx, sy + 2, mk->second.second, mk->second.first ? DANGER : ACCENT2); ui.cv->resetClip(); if (ui.hover(lx, sy, lw, statusH)) ui.tip(mk->second.second); }
        else if (lw > 120 && !f) ui.text(lx, sy + 2, "right-click: menu", RGB(120, 125, 138));
    }
}

// words a script might want at the caret: keywords, actions, built-in values, functions, and words already in the file
std::vector<std::string> Editor::scriptWords(const TextEdit& t, const std::string& prefix) {
    std::string p = txt::lower(prefix);
    std::set<std::string> all;
    for (auto* v : {&scriptKeywords(), &scriptActionKeys(), &scriptBuiltins(), &scriptFunctions()}) for (auto& s : *v) all.insert(s);
    for (const char* s : {"let", "keep", "say", "speaker", "option", "ask", "pick", "wait", "call", "start", "return", "stop", "break", "continue", "while", "for", "in", "end", "else", "elif", "and", "or", "not"}) all.insert(s);
    for (int li = 0; li < (int)t.lines.size(); li++) { const std::string& L = t.lines[li]; std::string tl = trim(L); if (!tl.empty() && tl[0] == '#') continue;
        for (size_t i = 0; i < L.size();) { if (txt::cls(L[i]) != 1) { i++; continue; } size_t j = i; while (j < L.size() && txt::cls(L[j]) == 1) j++;
            if (j - i >= 3 && !isdigit((unsigned char)L[i]) && !(li == t.cy && (int)i == t.acStart)) all.insert(txt::lower(L.substr(i, j - i))); i = j; } }
    std::vector<std::string> out;
    for (auto& s : all) if (s.size() > p.size() && s.compare(0, p.size(), p) == 0) out.push_back(s);
    std::stable_sort(out.begin(), out.end(), [](const std::string& a, const std::string& b) { return a.size() < b.size(); });
    if (out.size() > 30) out.resize(30);
    return out;
}

// a moment after the text stops changing, the tab's Check runs and its findings become the gutter dots
void Editor::liveCheck(TextEdit& t, int syntax) {
    if (t.path.empty() || ui.time - t.editT < 0.5f) return;
    std::string body = t.text(); size_t hsh = std::hash<std::string>()(body);
    if (hsh == t.checkedHash) return;
    t.checkedHash = hsh; t.marks.clear();
    std::string id = fs::path(t.path).stem().string();
    auto found = syntax == 1 ? lintScript(body, id) : lintDialogue(body, id);
    for (auto& it : found) { if (it.line < 0 || it.line >= (int)t.lines.size()) continue; auto m = t.marks.find(it.line); if (m == t.marks.end() || (!it.warn && !m->second.first)) t.marks[it.line] = {!it.warn, it.msg}; }
}

// ---- the big box: a single-line field's text in a wrapped editor ----
void Editor::openBigText(int fieldId, const std::string& value, int maxLen) {
    popOpenedFrame = frameNo; pop = P_BIGTEXT; ui.modal = true;
    bigTE = TextEdit(); bigTE.single = true; bigTE.wrap = true; bigTE.lines = {value}; bigTE.cx = (int)value.size();
    bigTarget = fieldId; bigMax = maxLen; ui.focus = 7001;
}
void Editor::drawBigText(InputState& in, bool& close) {
    int w = 620, h = 270, x = (W - w) / 2, y = (H - h) / 2;
    ui.panel(x, y, w, h); ui.header(x, y, w, "Edit text  (one line - it wraps here so all of it shows)");
    ui.idCounter = 7000; if (ui.focus < 7001) ui.focus = 7001;
    bool ok = false;
    if (ui.focus == 7001 && in.pressed[K_ENTER] && !bigTE.acOpen) { ok = true; in.pressed[K_ENTER] = false; }
    if (ui.focus == 7001 && in.pressed[K_ESCAPE] && !bigTE.bar && !bigTE.hasSel()) close = true;
    textEditWidget(bigTE, x + 8, y + 22, w - 16, h - 60, in, 0);
    std::string v; for (size_t i = 0; i < bigTE.lines.size(); i++) v += (i ? " " : "") + bigTE.lines[i];
    bool over = (int)v.size() > bigMax;
    ui.text(x + 10, y + h - 30, fmt("%d / %d characters", (int)v.size(), bigMax) + (over ? " - the end will be cut off" : ""), over ? DANGER : DIM);
    ui.text(x + 10, y + h - 16, "Enter: OK   Esc: cancel   right-click: menu", DIM);
    if (ui.button(x + w - 170, y + h - 30, 70, 22, "OK", true, false, OK)) ok = true;
    if (ui.button(x + w - 90, y + h - 30, 70, 22, "Cancel")) close = true;
    if (ok && !close) { ui.bigApplyId = bigTarget; ui.bigApplyValue = v.substr(0, (size_t)std::max(0, bigMax)); close = true; bigRefocus = true; }
    if (close && !ok) bigRefocus = true;
}
