// Desktop toys: Minesweeper, Solitaire (Klondike), My Computer (lore files), Achievements window, screensaver.
#include "desktop.h"
#include "../core/audio.h"
#include <filesystem>
#include <algorithm>
#include <cmath>

static const Color8 FACE = RGB(192, 192, 192), SHADOW = RGB(128, 128, 128), HILITE = RGB(255, 255, 255), DKSHADOW = RGB(0, 0, 0);
static const Color8 TITLE_A = RGB(0, 0, 128);
static const int TASKBAR_H = 28;

// ======================================================================= Minesweeper
void Desktop::mineReset(int difficulty) {
    MineState& m = mine; m = MineState();
    m.w = difficulty == 0 ? 9 : difficulty == 1 ? 16 : 24; m.h = difficulty == 0 ? 9 : 16; m.mines = difficulty == 0 ? 10 : difficulty == 1 ? 40 : 80; m.diff = difficulty;
    m.cell.assign((size_t)m.w * m.h, 0); m.state.assign((size_t)m.w * m.h, 0); m.started = false; m.dead = false; m.won = false; m.time = 0; m.flags = 0;
}
void Desktop::minePlace(int sx, int sy) {
    MineState& m = mine; Rng r((uint64_t)(ctx.time * 1000) + 17);
    int placed = 0; int guard = 0;
    while (placed < m.mines && guard++ < 100000) { int x = r.range(0, m.w - 1), y = r.range(0, m.h - 1); if (std::abs(x - sx) <= 1 && std::abs(y - sy) <= 1) continue; if (m.cell[(size_t)y * m.w + x] == 9) continue; m.cell[(size_t)y * m.w + x] = 9; placed++; }
    for (int y = 0; y < m.h; y++) for (int x = 0; x < m.w; x++) { if (m.cell[(size_t)y * m.w + x] == 9) continue; int n = 0; for (int dy = -1; dy <= 1; dy++) for (int dx = -1; dx <= 1; dx++) { int nx = x + dx, ny = y + dy; if (nx < 0 || ny < 0 || nx >= m.w || ny >= m.h) continue; if (m.cell[(size_t)ny * m.w + nx] == 9) n++; } m.cell[(size_t)y * m.w + x] = (uint8_t)n; }
    m.started = true;
}
void Desktop::mineReveal(int x, int y) {
    MineState& m = mine; if (x < 0 || y < 0 || x >= m.w || y >= m.h) return; size_t i = (size_t)y * m.w + x;
    if (m.state[i] != 0) return; m.state[i] = 1;
    if (m.cell[i] == 9) { m.dead = true; return; }
    if (m.cell[i] == 0) for (int dy = -1; dy <= 1; dy++) for (int dx = -1; dx <= 1; dx++) if (dx || dy) mineReveal(x + dx, y + dy);
}
void Desktop::drawMinesweeper(Canvas& cv, DeskWindow& w, InputState& in, bool active) {
    MineState& m = mine; if (m.w == 0) mineReset(0);
    Font& f = *ctx.font; const int CS = 16;
    int needW = std::max(204, m.w * CS + 24), needH = m.h * CS + 80;   // 1.1.5 (#26): 4 px more under the footer hint
    if (w.w != needW || w.h != needH) { w.w = needW; w.h = needH; w.x = clampi(w.x, 0, UI::SCREEN_W - w.w); w.y = clampi(w.y, 0, UI::SCREEN_H - TASKBAR_H - w.h); }
    int x0 = w.x + (w.w - m.w * CS) / 2, y0 = w.y + 60;   // 1.7.6 (#154): grid centred (Beginner's window is wider than its grid)
    if (m.started && !m.dead && !m.won) m.time += ctx.time - m.lastT; m.lastT = ctx.time;
    // menu row: difficulty
    int bx = w.x + 6, by = w.y + 24;
    if (win98Button(cv, in, bx, by, 60, 16, "Beginner", true, m.diff == 0, active)) mineReset(0);
    if (win98Button(cv, in, bx + 64, by, 70, 16, "Intermed.", true, m.diff == 1, active)) mineReset(1);
    if (win98Button(cv, in, bx + 138, by, 54, 16, "Expert", true, m.diff == 2, active)) mineReset(2);
    // counters + smiley
    bevelBox(cv, w.x + 6, w.y + 42, w.w - 12, 16, true);
    cv.fillRect(w.x + 10, w.y + 44, 34, 12, C::Black); f.draw(cv, *ctx.ts, w.x + 12, w.y + 44, fmt("%03d", std::max(0, m.mines - m.flags)), C::Red);
    cv.fillRect(w.x + w.w - 44, w.y + 44, 34, 12, C::Black); f.draw(cv, *ctx.ts, w.x + w.w - 42, w.y + 44, fmt("%03d", std::min(999, (int)m.time)), C::Red);
    int sx = w.x + w.w / 2 - 8;
    if (win98Button(cv, in, sx, w.y + 42, 16, 16, m.dead ? "X" : m.won ? "B" : ":", true, false, active)) mineReset(m.diff);
    // grid
    bevelBox(cv, x0 - 3, y0 - 3, m.w * CS + 6, m.h * CS + 6, true);
    bool over = active && in.mouseX >= x0 && in.mouseY >= y0 && in.mouseX < x0 + m.w * CS && in.mouseY < y0 + m.h * CS && !mouseUsed;
    int hx = (in.mouseX - x0) / CS, hy = (in.mouseY - y0) / CS;
    static const Color8 NUMC[] = {C::Black, RGB(0, 0, 255), RGB(0, 128, 0), RGB(255, 0, 0), RGB(0, 0, 128), RGB(128, 0, 0), RGB(0, 128, 128), RGB(0, 0, 0), RGB(128, 128, 128)};
    for (int y = 0; y < m.h; y++) for (int x = 0; x < m.w; x++) {
        size_t i = (size_t)y * m.w + x; int cx = x0 + x * CS, cy = y0 + y * CS; uint8_t st = m.state[i], c = m.cell[i];
        if (active) hot(cx, cy, CS, CS);   // the D-pad / arrow keys hop from cell to cell
        bool show = st == 1 || (m.dead && c == 9);
        if (!show) { bevelBox(cv, cx, cy, CS, CS, false); if (st == 2) { f.draw(cv, *ctx.ts, cx + 4, cy + 2, "F", C::Red); if (m.dead && c != 9) cv.line(cx + 2, cy + 2, cx + CS - 3, cy + CS - 3, C::Black); } if (st == 3) f.draw(cv, *ctx.ts, cx + 5, cy + 2, "?", DKSHADOW); }
        else { cv.fillRect(cx, cy, CS, CS, (m.dead && c == 9 && st == 1) ? C::Red : FACE); cv.hline(cx, cy, CS, SHADOW); cv.vline(cx, cy, CS, SHADOW); if (c == 9) { cv.fillRect(cx + 4, cy + 4, 8, 8, C::Black); cv.fillRect(cx + 6, cy + 2, 4, 12, C::Black); cv.fillRect(cx + 2, cy + 6, 12, 4, C::Black); cv.put(cx + 5, cy + 5, C::White); } else if (c > 0) f.draw(cv, *ctx.ts, cx + 5, cy + 2, std::to_string(c), NUMC[c]); }
    }
    if (over && !m.dead && !m.won) {
        if (in.mousePressed[0]) { mouseUsed = true; if (!m.started) minePlace(hx, hy); size_t i = (size_t)hy * m.w + hx; if (m.state[i] == 0) { mineReveal(hx, hy); gAudio.play(m.dead ? "mine_boom" : "mine_click"); }
            else if (m.state[i] == 1 && m.cell[i] > 0) { // chord: reveal neighbours when flags match
                int fl = 0; for (int dy = -1; dy <= 1; dy++) for (int dx = -1; dx <= 1; dx++) { int nx = hx + dx, ny = hy + dy; if (nx < 0 || ny < 0 || nx >= m.w || ny >= m.h) continue; if (m.state[(size_t)ny * m.w + nx] == 2) fl++; }
                if (fl == m.cell[i]) { for (int dy = -1; dy <= 1; dy++) for (int dx = -1; dx <= 1; dx++) { int nx = hx + dx, ny = hy + dy; if (nx < 0 || ny < 0 || nx >= m.w || ny >= m.h) continue; if (m.state[(size_t)ny * m.w + nx] == 0) mineReveal(nx, ny); } gAudio.play(m.dead ? "mine_boom" : "mine_click"); } } }
        if (in.mousePressed[2]) { size_t i = (size_t)hy * m.w + hx; if (m.state[i] == 0) { m.state[i] = 2; m.flags++; } else if (m.state[i] == 2) { m.state[i] = 3; m.flags--; } else if (m.state[i] == 3) m.state[i] = 0; gAudio.play("mine_click"); }
        if (!m.dead && m.started) { int hidden = 0; for (size_t i = 0; i < m.state.size(); i++) if (m.state[i] != 1) hidden++; if (hidden == m.mines) { m.won = true; showToast(fmt("Minesweeper cleared in %d seconds!", (int)m.time)); gAudio.play("achievement"); if (game) game->extra["minesweeper_won"] = "1"; } }
    }
    if (over && !m.dead && !m.won && (in.gamepadActive || padNavWindows)) cv.rect(x0 + hx * CS, y0 + hy * CS, CS, CS, C::Red);
    f.draw(cv, *ctx.ts, w.x + 6, w.y + w.h - 17, m.dead ? "Boom! Click the face." : m.won ? "You win!" : in.gamepadActive ? "A reveal  X flag  D-pad move" : padNavWindows ? "Enter reveal  F flag  arrows" : (m.w > 9 ? "L reveal  R flag  click a number to chord" : "L reveal  R flag"), SHADOW);
}

// ======================================================================= Solitaire
static const char* SUITS = "HDCS";
void Desktop::solReset() {
    SolState& s = sol; s = SolState();
    std::vector<int> deck; for (int i = 0; i < 52; i++) deck.push_back(i);
    Rng r((uint64_t)(ctx.time * 1000) + 99); for (int i = 51; i > 0; i--) std::swap(deck[i], deck[r.range(0, i)]);
    int k = 0;
    for (int c = 0; c < 7; c++) { for (int n = 0; n <= c; n++) { s.tab[c].push_back(deck[k++]); } s.faceUp[c] = 1; }
    while (k < 52) s.stock.push_back(deck[k++]);
    s.moves = 0; s.won = false; s.sel = -1; s.selPile = -1;
}
static int rankOf(int c) { return c % 13; }     // 0 = A .. 12 = K
static int suitOf(int c) { return c / 13; }
static bool isRed(int c) { return suitOf(c) < 2; }
void Desktop::solDrawCard(Canvas& cv, int x, int y, int card, bool faceUp, bool hi) {
    Font& f = *ctx.font; const int CW = 28, CH = 38;
    if (card < 0) { cv.rect(x, y, CW, CH, RGB(0, 90, 0)); return; }
    if (!faceUp) { cv.fillRect(x, y, CW, CH, RGB(30, 60, 160)); cv.rect(x, y, CW, CH, C::Black); for (int yy = 3; yy < CH - 3; yy += 3) for (int xx = 3; xx < CW - 3; xx += 3) if (((xx + yy) / 3) % 2 == 0) cv.put(x + xx, y + yy, RGB(90, 120, 220)); return; }
    cv.fillRect(x, y, CW, CH, hi ? RGB(255, 255, 180) : C::White); cv.rect(x, y, CW, CH, hi ? C::Red : C::Black);
    static const char* R[] = {"A", "2", "3", "4", "5", "6", "7", "8", "9", "10", "J", "Q", "K"};
    Color8 col = isRed(card) ? RGB(200, 0, 0) : C::Black;
    f.draw(cv, *ctx.ts, x + 2, y + 1, R[rankOf(card)], col);
    // suit glyph (pixel art)
    int sx = x + CW - 9, sy = y + 2; int su = suitOf(card);
    if (su == 0) { cv.fillRect(sx + 1, sy, 2, 2, col); cv.fillRect(sx + 4, sy, 2, 2, col); cv.fillRect(sx, sy + 1, 7, 2, col); cv.fillRect(sx + 1, sy + 3, 5, 1, col); cv.fillRect(sx + 2, sy + 4, 3, 1, col); cv.put(sx + 3, sy + 5, col); }
    else if (su == 1) { cv.put(sx + 3, sy, col); cv.fillRect(sx + 2, sy + 1, 3, 1, col); cv.fillRect(sx + 1, sy + 2, 5, 1, col); cv.fillRect(sx, sy + 3, 7, 1, col); cv.fillRect(sx + 1, sy + 4, 5, 1, col); cv.fillRect(sx + 2, sy + 5, 3, 1, col); cv.put(sx + 3, sy + 6, col); }
    else if (su == 2) { cv.fillRect(sx + 2, sy, 3, 2, col); cv.fillRect(sx, sy + 2, 7, 2, col); cv.fillRect(sx + 2, sy + 2, 3, 1, col); cv.fillRect(sx + 3, sy + 4, 1, 3, col); cv.fillRect(sx + 2, sy + 6, 3, 1, col); }
    else { cv.put(sx + 3, sy, col); cv.fillRect(sx + 2, sy + 1, 3, 1, col); cv.fillRect(sx + 1, sy + 2, 5, 1, col); cv.fillRect(sx, sy + 3, 7, 2, col); cv.fillRect(sx + 3, sy + 5, 1, 2, col); cv.fillRect(sx + 2, sy + 6, 3, 1, col); }
    // big center glyph as letter
    f.bold = true; f.draw(cv, *ctx.ts, x + CW / 2 - 4, y + CH / 2 - 2, std::string(1, SUITS[su]), col); f.bold = false;
}
bool Desktop::solCanStack(int moving, int onto, bool foundation) {
    if (foundation) { if (onto < 0) return rankOf(moving) == 0; return suitOf(onto) == suitOf(moving) && rankOf(moving) == rankOf(onto) + 1; }
    if (onto < 0) return rankOf(moving) == 12;
    return isRed(moving) != isRed(onto) && rankOf(moving) == rankOf(onto) - 1;
}
void Desktop::drawSolitaire(Canvas& cv, DeskWindow& w, InputState& in, bool active) {
    SolState& s = sol; if (!s.init) { solReset(); s.init = true; }
    Font& f = *ctx.font; const int CW = 28, CH = 38, GAP = 8, OFF = 10;
    int needW = 7 * (CW + GAP) + 34, needH = 24 + 20 + CH + 16 + CH + 18 * OFF + 24;
    if (w.w != needW || w.h != needH) { w.w = needW; w.h = needH; w.x = clampi(w.x, 0, UI::SCREEN_W - w.w); w.y = clampi(w.y, 0, UI::SCREEN_H - TASKBAR_H - w.h); }
    int gx = w.x + 9, gy = w.y + 24;
    cv.fillRect(w.x + 3, gy, w.w - 6, w.h - gy + w.y - 3, RGB(0, 128, 0));
    if (win98Button(cv, in, gx, gy + 2, 60, 16, "New deal", true, false, active)) solReset();
    f.draw(cv, *ctx.ts, gx + 70, gy + 4, fmt("moves %d", s.moves), C::White);
    int ty = gy + 22;                     // top row: stock, waste, [gap], 4 foundations
    auto colX = [&](int c) { return gx + c * (CW + GAP); };
    // piles identifiers: 0-6 tableau, 7 waste, 8-11 foundations, 12 stock
    auto click = [&](int x, int y, int ww, int hh) { return active && !mouseUsed && in.mousePressed[0] && in.mouseX >= x && in.mouseY >= y && in.mouseX < x + ww && in.mouseY < y + hh; };
    auto dbl = [&](int x, int y, int ww, int hh) { return active && in.mouseX >= x && in.mouseY >= y && in.mouseX < x + ww && in.mouseY < y + hh && s.dblT > 0 && in.mousePressed[0]; };
    // stock
    if (active) { hot(colX(0), ty, CW, CH); hot(colX(1), ty, CW, CH); for (int i = 0; i < 4; i++) hot(colX(3 + i), ty, CW, CH); }
    solDrawCard(cv, colX(0), ty, s.stock.empty() ? -1 : 0, false, false);
    if (s.stock.empty()) f.draw(cv, *ctx.ts, colX(0) + 6, ty + 10, "O", C::White);
    if (click(colX(0), ty, CW, CH)) { mouseUsed = true; s.sel = -1; if (!s.stock.empty()) { s.waste.push_back(s.stock.back()); s.stock.pop_back(); } else { while (!s.waste.empty()) { s.stock.push_back(s.waste.back()); s.waste.pop_back(); } } gAudio.play("card"); s.moves++; }
    // waste
    solDrawCard(cv, colX(1), ty, s.waste.empty() ? -1 : s.waste.back(), true, s.selPile == 7);
    // foundations
    for (int i = 0; i < 4; i++) { int x = colX(3 + i); solDrawCard(cv, x, ty, s.found[i].empty() ? -1 : s.found[i].back(), true, s.selPile == 8 + i); if (s.found[i].empty()) f.draw(cv, *ctx.ts, x + 9, ty + 12, std::string(1, SUITS[i]), RGB(0, 200, 0)); }
    // tableau
    int by = ty + CH + 10;
    for (int c = 0; c < 7; c++) {
        int x = colX(c); auto& pile = s.tab[c];
        if (pile.empty()) solDrawCard(cv, x, by, -1, false, false);
        for (size_t k = 0; k < pile.size(); k++) { bool up = (int)k >= (int)pile.size() - s.faceUp[c]; bool hi = s.selPile == c && (int)k >= s.sel; solDrawCard(cv, x, by + (int)k * OFF, pile[k], up, hi); }
        // controller / arrow keys: every face-up card (and an empty pile or the face-down card to turn) is a stop
        if (active) { if (pile.empty()) hot(x, by, CW, CH); else for (size_t k = 0; k < pile.size(); k++) { bool up = (int)k >= (int)pile.size() - s.faceUp[c]; bool last = k + 1 == pile.size(); if (up || (last && s.faceUp[c] == 0)) hot(x, by + (int)k * OFF, CW, last ? CH : OFF); } }
    }
    // ---- interaction ----
    auto tryMove = [&](int fromPile, int fromIdx, int toPile) -> bool {
        std::vector<int> moving;
        if (fromPile == 7) { if (s.waste.empty()) return false; moving.push_back(s.waste.back()); }
        else if (fromPile >= 8 && fromPile < 12) { auto& fp = s.found[fromPile - 8]; if (fp.empty()) return false; moving.push_back(fp.back()); }
        else { auto& p = s.tab[fromPile]; if (fromIdx < 0 || fromIdx >= (int)p.size()) return false; moving.assign(p.begin() + fromIdx, p.end()); }
        bool ok = false;
        if (toPile >= 8 && toPile < 12) { if (moving.size() != 1) return false; auto& fp = s.found[toPile - 8]; int onto = fp.empty() ? -1 : fp.back(); ok = solCanStack(moving[0], onto, true) && (onto >= 0 || suitOf(moving[0]) == toPile - 8); if (ok) fp.push_back(moving[0]); }
        else if (toPile >= 0 && toPile < 7) { auto& tp = s.tab[toPile]; int onto = tp.empty() ? -1 : tp.back(); ok = solCanStack(moving[0], onto, false); if (ok) { for (int c2 : moving) tp.push_back(c2); s.faceUp[toPile] += (int)moving.size(); } }
        if (!ok) return false;
        if (fromPile == 7) s.waste.pop_back(); else if (fromPile >= 8) s.found[fromPile - 8].pop_back(); else { auto& p = s.tab[fromPile]; p.resize(fromIdx); s.faceUp[fromPile] -= (int)moving.size(); if (s.faceUp[fromPile] <= 0 && !p.empty()) s.faceUp[fromPile] = 1; if (p.empty()) s.faceUp[fromPile] = 0; }
        s.moves++; gAudio.play("card"); return true;
    };
    auto autoFoundation = [&](int fromPile, int fromIdx) { for (int fi = 0; fi < 4; fi++) if (tryMove(fromPile, fromIdx, 8 + fi)) return true; return false; };
    if (s.dblT > 0) s.dblT -= 1.0f / 60.0f;
    if (active && in.mousePressed[0] && !mouseUsed) {
        int hitPile = -1, hitIdx = -1;
        if (in.mouseX >= colX(1) && in.mouseX < colX(1) + CW && in.mouseY >= ty && in.mouseY < ty + CH) hitPile = 7;
        for (int i = 0; i < 4; i++) if (in.mouseX >= colX(3 + i) && in.mouseX < colX(3 + i) + CW && in.mouseY >= ty && in.mouseY < ty + CH) hitPile = 8 + i;
        for (int c = 0; c < 7; c++) if (in.mouseX >= colX(c) && in.mouseX < colX(c) + CW && in.mouseY >= by) {
            auto& p = s.tab[c]; int k = (in.mouseY - by) / OFF; if (p.empty()) { hitPile = c; hitIdx = 0; break; }
            k = std::min(k, (int)p.size() - 1); if (in.mouseY >= by + (int)p.size() * OFF + CH - OFF) break;
            bool up = k >= (int)p.size() - s.faceUp[c]; if (!up) { if (k == (int)p.size() - 1 - s.faceUp[c] && s.faceUp[c] == 0) { s.faceUp[c] = 1; s.moves++; gAudio.play("card"); } break; }
            hitPile = c; hitIdx = k; break;
        }
        if (hitPile >= 0) {
            mouseUsed = true;
            bool isDouble = s.dblT > 0 && s.dblPile == hitPile && s.dblIdx == hitIdx;
            if (isDouble && hitPile != 12) { int idx = hitPile < 7 ? (int)s.tab[hitPile].size() - 1 : -1; if (hitPile < 7 && hitIdx != idx) idx = -1; if (idx >= 0 || hitPile >= 7) autoFoundation(hitPile, hitPile < 7 ? idx : 0); s.sel = -1; s.selPile = -1; s.dblT = 0; }
            else if (s.selPile < 0) { if (!(hitPile < 7 && s.tab[hitPile].empty()) && !(hitPile == 7 && s.waste.empty()) && !(hitPile >= 8 && s.found[hitPile - 8].empty())) { s.selPile = hitPile; s.sel = hitPile < 7 ? hitIdx : 0; } s.dblT = 0.35f; s.dblPile = hitPile; s.dblIdx = hitIdx; }
            else { if (!(s.selPile == hitPile)) tryMove(s.selPile, s.sel, hitPile); s.sel = -1; s.selPile = -1; s.dblT = 0.35f; s.dblPile = hitPile; s.dblIdx = hitIdx; }
        } else if (active && in.mouseX >= w.x && in.mouseX < w.x + w.w && in.mouseY >= gy) { s.sel = -1; s.selPile = -1; }
    }
    if (active && in.mousePressed[2]) {   // right click / pad X / F: send the card under the pointer to the foundations (else clear the selection)
        bool sent = false;
        if (in.mouseX >= colX(1) && in.mouseX < colX(1) + CW && in.mouseY >= ty && in.mouseY < ty + CH) sent = autoFoundation(7, 0);
        for (int c = 0; c < 7 && !sent; c++) if (in.mouseX >= colX(c) && in.mouseX < colX(c) + CW && in.mouseY >= by && !s.tab[c].empty() && in.mouseY < by + (int)s.tab[c].size() * OFF + CH - OFF) sent = autoFoundation(c, (int)s.tab[c].size() - 1);
        s.sel = -1; s.selPile = -1;
    }
    if (!s.won) { int n = 0; for (auto& fp : s.found) n += (int)fp.size(); if (n == 52) { s.won = true; showToast(fmt("Solitaire won in %d moves!", s.moves)); gAudio.play("achievement"); if (game) game->extra["solitaire_won"] = "1"; } }
    if (s.won) { f.bold = true; f.drawShadow(cv, *ctx.ts, w.x + w.w / 2 - 40, by + 60, "YOU WIN!", C::Yellow, C::Black, 2); f.bold = false; }
    f.draw(cv, *ctx.ts, w.x + 6, w.y + w.h - 17, in.gamepadActive ? "A pick / drop   X send up   B cancel" : padNavWindows ? "Enter pick / drop   F send up" : "click card, then target - dbl/right-click sends up", RGB(200, 255, 200));
    (void)dbl;
}

// ======================================================================= My Computer (drives)
void Desktop::drawMyComputer(Canvas& cv, DeskWindow& w, InputState& in, bool active) {
    // 1.6.1 (#133): My Computer is the PC's drives (the lore files it used to list are gone since 1.6.2, #137). The CD in D: is
    // Triffall: opening it opens the launcher, like a game disc's autorun.
    Font& f = *ctx.font;
    int lx = w.x + 6, ly = w.y + 26, lw = w.w - 12, lh = w.h - 50;
    bevelBox(cv, lx, ly, lw, lh, true); cv.fillRect(lx + 2, ly + 2, lw - 4, lh - 4, HILITE);
    struct Drive { const char* label; int ic, ir; };
    static const Drive drives[] = {{"3.5 Floppy (A:)", 0, 3}, {"TRIFFALL-HDD (C:)", 0, 5}, {"TRIFFALL (D:)", 2, 7}};
    for (int i = 0; i < 3; i++) {
        int ix = lx + 14 + i * 102, iy = ly + 10; bool sel = w.sel == i;
        bool hv = active && !mouseUsed && in.mouseX >= ix && in.mouseX < ix + 94 && in.mouseY >= iy && in.mouseY < iy + 62;
        if (active) hot(ix, iy, 94, 62);
        int ic = drives[i].ic, ir = drives[i].ir; if (i == 2) iconCell("launcher", ic, ir);
        drawIcon(cv, ic, ir, ix + 31, iy, 1);
        std::string label = drives[i].label;
        int tw = f.width(label); if (sel) cv.fillRect(ix + 47 - tw / 2 - 2, iy + 36, tw + 4, 14, TITLE_A);
        f.draw(cv, *ctx.ts, ix + 47 - tw / 2, iy + 37, label, sel ? HILITE : DKSHADOW);
        if (hv && in.mousePressed[0]) { mouseUsed = true;
            if (sel && dblTimer > 0 && dblIcon == 400 + i) { dblTimer = 0;
                if (i == 0) { showDialog("A:\\", "A:\\ is not accessible. The device is not ready."); return; }
                if (i == 1) { showDialog("TRIFFALL-HDD (C:)", "Local Disk - 2.1 GB, 1.3 GB free. Nothing in here you need: Triffall is on the CD in drive D:."); return; }
                runAction("launcher"); return; }
            w.sel = i; dblTimer = 0.4f; dblIcon = 400 + i; gAudio.play("desktop_click"); }
    }
    const char* status = w.sel == 0 ? "3.5 inch floppy drive - no disk" : w.sel == 1 ? "Local Disk, 1.3 GB free" : w.sel == 2 ? "CD-ROM: Triffall - double-click to open" : "3 object(s)";
    bevelBox(cv, w.x + 6, w.y + w.h - 22, w.w - 12, 16, true);
    f.draw(cv, *ctx.ts, w.x + 10, w.y + w.h - 21, status, DKSHADOW);
}

// ======================================================================= Screensaver
void Desktop::drawScreensaver(Canvas& cv, float dt) {
    if (stars.empty()) { Rng r(5); for (int i = 0; i < 120; i++) stars.push_back({(float)r.range(-320, 320), (float)r.range(-240, 240), (float)r.range(10, 100) / 100.0f}); }
    cv.clear(C::Black);
    for (auto& s : stars) {
        s[2] -= dt * 0.35f; if (s[2] <= 0.02f) { Rng r((uint64_t)(ctx.time * 1000)); s[0] = (float)r.range(-320, 320); s[1] = (float)r.range(-240, 240); s[2] = 1.0f; }
        float k = 1.0f / s[2], sx = cv.w / 640.0f, sy = cv.h / 480.0f; int x = (int)(cv.w / 2 + s[0] * sx * k * 0.5f), y = (int)(cv.h / 2 + s[1] * sy * k * 0.5f);   // 1.2.2: from the middle of any screen
        uint8_t b = (uint8_t)clampi((int)(255 * (1.0f - s[2])), 40, 255); int sz = s[2] < 0.3f ? 2 : 1;
        if (x >= 0 && y >= 0 && x < cv.w && y < cv.h) cv.fillRect(x, y, sz, sz, RGB(b, b, b));
    }
    // a bouncing "flying window" logo
    static float bx = 100, by = 100, vx = 60, vy = 45;
    bx += vx * dt; by += vy * dt; if (bx < 0 || bx > cv.w - 96) vx = -vx; if (by < 0 || by > cv.h - 60) vy = -vy;
    bevelBox(cv, (int)bx, (int)by, 96, 60, false); cv.fillRect((int)bx + 3, (int)by + 3, 90, 14, TITLE_A); ctx.font->draw(cv, *ctx.ts, (int)bx + 6, (int)by + 4, "Triffall", HILITE);
    ctx.ts->draw(cv, T(104, 0), (int)bx + 36, (int)by + 24, 2);
    ctx.font->draw(cv, *ctx.ts, 8, cv.h - 14, "move the mouse or press a key", RGB(60, 60, 60));
}
