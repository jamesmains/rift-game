// 1.8.3 (#180-#183): the One-Bit Desktop HUD - tool windows under a paper menu bar.
//   explore: the menu bar (Start | Triffall | Party Items Spells Journal Map Craft | place, day, time, gold), and three tool
//            windows along the bottom: Party, News (the log), Belt; a small prompt box over the leader.
//   combat:  the menu bar holds the round, the turn order as chips and whose turn it is; a window for the acting unit (big
//            AP / MV, the action list with key caps and the torch pointer) and a Target window at the right.
//   pause:   the Start menu, a "Menu" tool window that drops from the Start button; Settings opens beside it.
//   Party:   one big "Party" window (the Party tab): member cards, the hero, a ledger of attributes, equipment, class ranks.
// Layout numbers follow the design mockup (960x540; the 4:3 / 480-high screens stack the bottom windows 2+1).
#include "game.h"
#include "tiles.h"
#include <algorithm>
using namespace UI;
using Theme::Pen;

const char* statLine(int s);                          // hud_party.cpp
std::vector<std::string> classOrder(const GameData& gd);

namespace {
std::string bindName(int a) { return gKeys.key1[a] ? keyName(gKeys.key1[a]) : gKeys.key2[a] ? keyName(gKeys.key2[a]) : ""; }
std::string lower(std::string s) { for (auto& c : s) c = (char)tolower((unsigned char)c); return s; }
std::string keyLow(int a) { return lower(bindName(a)); }
std::string classLine(const Character& m, const GameData& gd) {
    const ClassDef* c = gd.cls(m.classId);
    std::string n = c ? c->name : m.classId; if (!n.empty()) n[0] = (char)toupper((unsigned char)n[0]);
    int r = m.rank(m.classId); return n + " " + std::to_string(r > 0 ? r : m.level);
}
const Character& viewOf(const Game& g, int i) {
    if (g.mode == GMode::COMBAT || g.inCombat()) for (auto& u : g.cb.units) if (u.partyIndex == i) return u.ch;
    return g.party.members[i];
}
void sprite(const Tileset& ts, Canvas& cv, const Character& m, int x, int y, int scale) {
    if (!m.alive) { ts.draw(cv, TL::UI_SKULL, x, y, scale); return; }
    if (m.recolor) ts.drawRecolor(cv, m.tile, x, y, scale, m.color); else ts.draw(cv, m.tile, x, y, scale);
}
// the torch: the pointer next to the chosen entry and the Start button's picture
void torch(Game& g, Canvas& cv, int x, int y) {
    if (const ItemDef* d = g.gd->item("torch_item")) drawItemTile(*g.ctx.ts, cv, *d, x, y, 1);
    else { cv.fillRect(x + 5, y + 4, 2, 7, Theme::th().accent); cv.fillRect(x + 4, y + 1, 4, 3, Theme::foe()); }
}
// a dotted frame / rule (every other pixel)
void dotted(Canvas& cv, int x, int y, int w, int h, Color8 c) {
    for (int i = 0; i < w; i += 2) { cv.put(x + i, y, c); cv.put(x + i, y + h - 1, c); }
    for (int i = 0; i < h; i += 2) { cv.put(x, y + i, c); cv.put(x + w - 1, y + i, c); }
}
void hdots(Canvas& cv, int x, int y, int w, Color8 c) { for (int i = 0; i < w; i += 2) cv.put(x + i, y, c); }
// a bar: an ash edge round the fill
void obBar(Canvas& cv, int x, int y, int w, int h, int v, int mx, Color8 fill) {
    const UiTheme& t = Theme::th();
    cv.rect(x, y, w, h, t.dim); cv.fillRect(x + 1, y + 1, w - 2, h - 2, t.back);
    int fw = mx > 0 ? (w - 2) * std::clamp(v, 0, mx) / mx : 0;
    if (fw > 0) cv.fillRect(x + 1, y + 1, fw, h - 2, fill);
}
// a key cap: a box with clipped corners (16 high)
int obKey(const Pen& p, int x, int y, const std::string& k, int minW, Color8 edge, Color8 tc, const Color8* fill = nullptr) {
    Canvas& cv = *p.cv; const UiTheme& t = Theme::th();
    int w = std::max(minW, Theme::textW(p, k) + 10);
    if (fill) cv.fillRect(x, y, w, 16, *fill);
    cv.rect(x, y, w, 16, edge);
    cv.put(x, y, t.back); cv.put(x + w - 1, y, t.back); cv.put(x, y + 15, t.back); cv.put(x + w - 1, y + 15, t.back);
    Theme::text(p, x + (w - Theme::textW(p, k)) / 2, y + 2, k, tc);
    return w;
}
// what the leader is next to (as the frame's "Near:" line)
std::string nearText(Game& g, std::string& verb) {
    verb.clear(); if (!g.map) return "";
    int fx = g.px + (g.facing == 0 ? -1 : g.facing == 1 ? 1 : 0), fy = g.py + (g.facing == 2 ? -1 : g.facing == 3 ? 1 : 0);
    std::vector<std::pair<int, int>> spots = {{fx, fy}, {g.px - 1, g.py}, {g.px + 1, g.py}, {g.px, g.py - 1}, {g.px, g.py + 1}, {g.px, g.py}};
    for (auto& s : spots) {
        int x = s.first, y = s.second; if (!g.map->inBounds(x, y)) continue;
        if (NPC* n = g.map->npcAt(x, y)) if (!n->hidden) { verb = "talk to"; return n->name; }
        if (Chest* c = g.map->chestAt(x, y)) { verb = c->opened ? "look in" : "open"; return "the chest"; }
        if (SignPost* sp = g.map->signAt(x, y)) { verb = "read"; return sp->title.empty() ? "the sign" : sp->title; }
        if (g.map->saveAt(x, y) && x == g.px && y == g.py) { verb = "save"; return "here"; }
        if (Door* d = g.map->doorAt(x, y)) { if (!d->open) { verb = "open"; return "the door"; } }
        if (Lever* lv = g.map->leverAt(x, y)) { verb = "pull"; return lv->name.empty() ? "the lever" : lv->name; }
        if (GatherNode* gn = g.map->gatherAt(x, y)) { verb = "gather"; return gn->name.empty() ? "it" : gn->name; }
    }
    return "";
}
// a belt slot's picture and count; returns the entry's name (empty = nothing there)
std::string beltSlot(Game& g, Canvas& cv, int member, int slot, int x, int y, bool combat, bool draw = true) {
    if (member < 0 || member >= (int)g.party.members.size()) return "";
    const std::string& f = g.party.members[member].favs[slot]; if (f.empty()) return "";
    std::string kind = f.substr(0, f.find(':')), id = f.substr(f.find(':') + 1), name;
    bool ok = true; int n = -1;
    const Character& m = viewOf(g, member);
    if (kind == "spell") { if (auto* s = g.gd->spell(id)) { if (draw) g.ctx.ts->draw(cv, s->tile, x, y, 1); ok = m.mp >= s->cost && (combat || g.spellUsableOutside(*s)); name = s->name; } }
    else if (auto* d = g.gd->item(id)) { if (draw) drawItemTile(*g.ctx.ts, cv, *d, x, y, 1); n = g.party.countItem(id); ok = n > 0; name = d->name; }
    if (!draw) return name;
    if (!ok) cv.fillRect(x, y, 12, 12, RGB(0, 0, 0, 160));
    if (n > 1) { std::string c = std::to_string(std::min(n, 99)); g.ctx.font->drawSmall(cv, x + 16 - g.ctx.font->widthSmall(c), y + 9, c, Theme::th().text); }
    return name;
}

// where the bottom tool windows go: one row at 900+ wide, else Party + News at the bottom and the Belt above, at the left
struct ObLayout { bool stacked; Rect party, news, belt; int colTop, colBottom; };
ObLayout obLayout() {
    const int W = SCREEN_W, H = SCREEN_H, y0 = H - 112;
    ObLayout L;
    if (W >= 900) {
        L.stacked = false;
        L.party = {8, y0, 452, 104};
        L.belt = {W - 8 - 246, y0, 246, 104};
        L.news = {468, y0, L.belt.x - 8 - 468, 104};
    } else {
        L.stacked = true;
        int newsW = std::max(200, W - 24 - 452), partyW = W - 24 - newsW;
        L.party = {8, y0, partyW, 104};
        L.news = {8 + partyW + 8, y0, newsW, 104};
        L.belt = {8, y0 - 112, 246, 104};
    }
    L.colTop = H >= 540 ? 32 : 28; L.colBottom = y0 - 6;
    return L;
}
}

bool oneBitPartyFits(const Rect& a) { return a.w >= 820 && a.h >= 440; }

// ---------------- the HUD ----------------
void Game::drawOneBitHud(Canvas& cv, InputState& in) {
    Theme::NoMap nm; const UiTheme& t = Theme::th(); Pen p{&cv, ctx.font, ctx.ts};
    const int W = SCREEN_W, H = SCREEN_H;
    const bool combat = mode == GMode::COMBAT || inCombat();
    const bool settingsOpen = mode == GMode::SETTINGS;
    const bool paused = mode == GMode::PAUSE || (settingsOpen && std::find(modeStack.begin(), modeStack.end(), GMode::PAUSE) != modeStack.end());
    const bool menuOver = subViewMode(mode);   // a menu window covers the bottom of the screen: only the menu bar shows
    const bool myTurn = combat && cb.playerTurn && !cb.ending && cb.selectedUnit >= 0 && cb.selectedUnit < (int)cb.units.size() && !autoCombat;
    const int n = (int)party.members.size(), lead = activeLeader();
    const ObLayout L = obLayout();
    Font& font = *ctx.font;
    auto hover = [&](int x, int y, int w, int h) { return in.mouseX >= x && in.mouseX < x + w && in.mouseY >= y && in.mouseY < y + h; };

    // ---------------- the bottom tool windows ----------------
    auto partyWin = [&](const Rect& r) {
        uiRect(r.x, r.y, r.w + 4, r.h + 4); hudHit(r.x, r.y, r.w, r.h, HH_TOOL, 0);
        Theme::toolWindow(p, r.x, r.y, r.w, r.h, "Party");
        int pitch = std::min(110, (r.w - 12) / 4), bw = pitch - 14;
        for (int i = 0; i < n && i < 4; i++) {
            const Character& m = viewOf(*this, i);
            int cx = r.x + 10 + i * pitch, cy = r.y + 24;
            int ui = -1; if (combat) for (size_t k = 0; k < cb.units.size(); k++) if (cb.units[k].partyIndex == i) ui = (int)k;
            bool on = combat ? (ui >= 0 && ui == cb.selectedUnit) : i == lead;
            if (on) Theme::glow(cv, cx, cy, 28, 28); else cv.rect(cx, cy, 28, 28, t.dim);
            sprite(*ctx.ts, cv, m, cx + 2, cy + 2, 2);
            Theme::text(p, cx + 34, cy - 1, Theme::fit(p, m.name, pitch - 38), !m.alive ? t.dim : on ? t.accent : t.text);
            if (combat) Theme::small(p, cx + 34, cy + 15, fmt("ap %d  mv %d", m.ap, m.move), t.accent);
            else if (m.wounds > 0) Theme::small(p, cx + 34, cy + 15, fmt("wounded x%d", m.wounds), Theme::foe());
            else Theme::small(p, cx + 34, cy + 15, lower(classLine(m, *gd)), t.dim);
            int mhp = m.maxHP(*gd), mmp = m.maxMP(*gd);
            obBar(cv, cx, cy + 36, bw, 7, m.hp, mhp, Theme::hp());
            Theme::smallRight(p, cx + bw, cy + 45, fmt("%d/%d", m.hp, mhp), t.text);
            if (mmp > 0) obBar(cv, cx, cy + 54, bw, 5, m.mp, mmp, Theme::mp());
            if (combat && ui >= 0) hudHit(cx - 2, cy - 2, pitch - 4, 64, HH_UNIT, ui);
            else if (!combat) hudHit(cx - 2, cy - 2, pitch - 4, 64, HH_MEMBER, i);
        }
    };
    auto newsWin = [&](const Rect& r) {
        uiRect(r.x, r.y, r.w + 4, r.h + 4); hudHit(r.x, r.y, r.w, r.h, HH_TOOL, 0);
        Theme::toolWindow(p, r.x, r.y, r.w, r.h, "News");
        std::vector<std::pair<std::string, Color8>> lines;
        std::string hint;
        if (combat && cb.playerTurn && cb.mode == 0) hint = "right-drag pans, " + keyLow(A_CENTER) + " centers";
        int fromLog = 5 - (hint.empty() ? 0 : 1);
        for (int i = std::max(0, (int)msgLog.size() - fromLog); i < (int)msgLog.size(); i++) lines.push_back({msgLog[i], logColor(msgLog[i])});
        int ly = r.y + 20;
        for (auto& l : lines) { Theme::text(p, r.x + 8, ly, Theme::fit(p, l.first, r.w - 16), l.second); ly += 16; }
        if (!hint.empty()) Theme::text(p, r.x + 8, r.y + 20 + 4 * 16, Theme::fit(p, hint, r.w - 16), t.dim);
    };
    auto beltWin = [&](const Rect& r) {
        uiRect(r.x, r.y, r.w + 4, r.h + 4); hudHit(r.x, r.y, r.w, r.h, HH_TOOL, 0);
        Theme::toolWindow(p, r.x, r.y, r.w, r.h, "Belt");
        int member = combat && cb.selectedUnit >= 0 && cb.selectedUnit < (int)cb.units.size() && cb.units[cb.selectedUnit].partyIndex >= 0 ? cb.units[cb.selectedUnit].partyIndex : lead;
        int pointed = -1, first = -1;
        for (int i = 0; i < 8; i++) {
            int cx = r.x + 8 + i * 29, cy = r.y + 24;
            if (hover(cx, cy, 25, 25)) pointed = i;
            if (first < 0 && !beltSlot(*this, cv, member, i, 0, 0, combat, false).empty()) first = i;
        }
        int chosen = combat ? pointed : (pointed >= 0 ? pointed : first);
        std::string chosenName;
        for (int i = 0; i < 8; i++) {
            int cx = r.x + 8 + i * 29, cy = r.y + 24;
            std::string nm2 = beltSlot(*this, cv, member, i, cx + 7, cy + 8, combat, false);
            if (i == chosen) Theme::glow(cv, cx, cy, 25, 25); else cv.rect(cx, cy, 25, 25, nm2.empty() ? t.dim : t.text);
            std::string k = bindName(A_FAV1 + i); Theme::small(p, cx + 2, cy + 2, k.empty() ? std::to_string(i + 1) : k.substr(0, 1), t.dim);
            beltSlot(*this, cv, member, i, cx + 7, cy + 8, combat);
            if (i == chosen) chosenName = nm2;
            hudHit(cx, cy, 25, 25, HH_BELT, i);
        }
        std::string k1 = keyLow(A_FAV1), k8 = keyLow(A_FAV8);
        std::string range = k1.empty() || k8.empty() ? "1-8" : k1 + "-" + k8;
        if (combat) {
            std::string terr;   // the ground under the combat cursor
            if (map && map->inBounds(cb.selX, cb.selY) && (map->at(cb.selX, cb.selY).explored || visible(cb.selX, cb.selY))) {
                const Cell& c = map->at(cb.selX, cb.selY); const TileKind& k = allTileKinds()[c.kind];
                terr = k.name; for (auto& ch : terr) if (ch == '_') ch = ' ';
                if (!c.walkable) terr += c.blocksSight ? ", full cover" : c.water ? ", water" : ", half cover";
                if (k.flammable || k.burns) terr += ", burns";
            }
            if (!chosenName.empty()) Theme::text(p, r.x + 8, r.y + 58, Theme::fit(p, chosenName, r.w - 16), t.accent);
            else Theme::text(p, r.x + 8, r.y + 58, Theme::fit(p, terr.empty() ? std::string("") : "Terrain: " + lower(terr), r.w - 16), t.dim);
            Theme::small(p, r.x + 8, r.y + 76, range + " use an item in combat", t.dim);
        } else {
            if (!chosenName.empty()) Theme::text(p, r.x + 8, r.y + 58, Theme::fit(p, chosenName, r.w - 16), t.accent);
            else Theme::text(p, r.x + 8, r.y + 58, "Nothing on the belt yet", t.dim);
            Theme::small(p, r.x + 8, r.y + 76, Theme::fit(p, range + " use   " + keyLow(A_MENU) + " menu   " + keyLow(A_INTERACT) + " act", r.w - 16), t.dim);
        }
    };

    // ---------------- the acting unit and the target (a fight) ----------------
    auto unitWins = [&]() {
        const int uw = W >= 900 ? 208 : 176, ux = W - 8 - uw;
        const int avail = L.colBottom - L.colTop;
        const bool roomy = avail >= 388;
        const int pitch = roomy ? 19 : 15;
        const int uh = 80 + 9 * pitch + 8;
        int uy = L.colTop;
        int au = cb.selectedUnit >= 0 && cb.selectedUnit < (int)cb.units.size() ? cb.selectedUnit : -1;
        uiRect(ux, uy, uw + 4, uh + 4); hudHit(ux, uy, uw, uh, HH_TOOL, 0);
        Theme::windowChrome(p, ux, uy, uw, uh, au >= 0 ? Theme::fit(p, cb.units[au].ch.name, uw - 80) : "Fight", true);
        if (au >= 0) {
            const Character& m = cb.units[au].ch;
            bool foe = !cb.units[au].friendly();
            cv.rect(ux + 8, uy + 28, 40, 40, foe ? Theme::foe() : t.dim);
            sprite(*ctx.ts, cv, m, ux + 10, uy + 30, 3);
            int nx = ux + 56;
            std::string aps = std::to_string(m.ap), mvs = std::to_string(m.move);
            Theme::small(p, nx, uy + 28, aps, t.accent, 2); nx += Theme::smallW(p, aps, 2) + 6;
            Theme::text(p, nx, uy + 32, "AP", t.accent); nx += Theme::textW(p, "AP") + 18;
            Theme::small(p, nx, uy + 28, mvs, t.text, 2); nx += Theme::smallW(p, mvs, 2) + 4;
            Theme::text(p, nx, uy + 32, "MV", t.dim);
            obBar(cv, ux + 56, uy + 52, uw - 64, 7, m.hp, m.maxHP(*gd), Theme::hp());
            Theme::smallRight(p, ux + uw - 8, uy + 61, fmt("%d/%d", m.hp, m.maxHP(*gd)), t.text);
        }
        hdots(cv, ux + 8, uy + 74, uw - 16, t.dim);
        struct A { const char* label; int cmd; int key; };
        static const A acts[] = {{"Attack", 0, A_ATTACKMODE}, {"Aimed shot", 1, A_ATTACKMODE}, {"Spell", 2, A_SPELLS}, {"Item", 3, A_ITEMS}, {"Defend", 4, A_DEFEND}, {"Overwatch", 5, A_OVERWATCH}, {"More actions", 8, A_ACTIONMENU}, {"Flee", 7, A_FLEE}, {"End turn", 6, A_ENDTURN}};
        for (int i = 0; i < 9; i++) {
            const A& a = acts[i]; int ry = uy + 80 + i * pitch, rh = pitch - 2;
            bool on = myTurn && ((a.cmd == 0 && !cb.aimed && cb.mode == 0) || (a.cmd == 1 && cb.aimed) || (a.cmd == 2 && (cb.mode == 4 || cb.mode == 1)) || (a.cmd == 3 && (cb.mode == 5 || cb.mode == 2)) || (a.cmd == 8 && cb.mode == 3));
            if (myTurn && cb.mode == 3 && a.cmd < 8) on = cb.menuSel == a.cmd;
            if (myTurn && cb.mode == 3 && a.cmd == 8) on = false;
            bool ho = myTurn && hover(ux + 8, ry, uw - 16, rh);
            if (on) { Theme::glow(cv, ux + 8, ry, uw - 16, rh); torch(*this, cv, ux + 12, ry + (rh - 12) / 2); }
            else if (ho) dotted(cv, ux + 8, ry, uw - 16, rh, t.accent);
            std::string label = a.label; if (a.cmd == 1 && cb.aimed) label += "  on";
            int ty = ry + (rh - 12) / 2;
            Theme::text(p, ux + 28, ty, label, !myTurn ? t.dim : on ? t.accent : t.text);
            std::string k = keyLow(a.key); if (k.size() > 3) k = k.substr(0, 3);
            if (!k.empty()) {
                int kw = std::max(16, Theme::textW(p, k) + 8), ky = ry + (rh - 16) / 2;
                if (rh < 16) ky = ry - 1;
                obKey(p, ux + uw - 12 - kw, ky, k, kw, on ? t.accent : t.dim, !myTurn ? t.dim : on ? t.accent : t.text);
            }
            if (myTurn) hudHit(ux + 8, ry, uw - 16, rh, HH_COMBAT, a.cmd);
        }
        // the target window under it
        int ty0 = uy + uh + 6, th0 = roomy ? 120 : std::max(84, L.colBottom - ty0);
        uiRect(ux, ty0, uw + 4, th0 + 4); hudHit(ux, ty0, uw, th0, HH_TOOL, 0);
        Rect in2 = Theme::toolWindow(p, ux, ty0, uw, th0, "Target");
        (void)in2;
        TargetOdds o = targetOdds();
        int y = ty0 + 20, bottom = ty0 + th0 - 4;
        std::vector<std::pair<std::string, Color8>> info;
        if (myTurn || au >= 0) info = tileInfoLines(cb.selX, cb.selY);
        if (o.unit >= 0) {
            auto& tu = cb.units[o.unit]; const Character& m = tu.ch;
            Color8 nc = o.foe ? Theme::foe() : Theme::good();
            cv.rect(ux + 8, ty0 + 22, 40, 40, o.foe ? Theme::foe() : t.dim);
            sprite(*ctx.ts, cv, m, ux + 10, ty0 + 24, 3);
            Theme::text(p, ux + 56, y, Theme::fit(p, m.name, uw - 96), nc);
            Theme::smallRight(p, ux + uw - 8, y + 3, fmt("lv %d", m.level), t.dim);
            if (o.foe && o.inRange) {
                std::string hs = std::to_string(o.hit);
                Theme::small(p, ux + 56, y + 16, hs, t.accent, 2);
                Theme::text(p, ux + 58 + Theme::smallW(p, hs, 2), y + 20, "% to hit", t.accent);
                std::string cs = fmt("%d%% crit", o.crit);
                std::string note = cb.aimed ? "aimed" : o.cover >= 2 ? "full cover" : o.cover == 1 ? "half cover" : o.flanked ? "flanked" : o.exposed ? "exposed" : "";
                Theme::text(p, ux + 56, y + 34, cs, t.text);
                if (!note.empty()) Theme::text(p, ux + 64 + Theme::textW(p, cs), y + 34, Theme::fit(p, note, uw - 72 - Theme::textW(p, cs)), o.cover ? Theme::mapText(coverColor(o.cover)) : t.dim);
            } else if (o.foe) {
                Theme::text(p, ux + 56, y + 18, "out of range", t.dim);
                Theme::text(p, ux + 56, y + 32, fmt("%d / %d tiles", o.dist, o.range), t.dim);
            } else {
                Theme::text(p, ux + 56, y + 18, tu.partyIndex >= 0 ? classLine(m, *gd) : "on your side", t.dim);
                Theme::text(p, ux + 56, y + 32, fmt("ap %d  mv %d", m.ap, m.maxMove(*gd)), t.accent);
            }
            obBar(cv, ux + 8, ty0 + 68, uw - 16, 7, m.hp, m.maxHP(*gd), Theme::hp());
            Theme::smallRight(p, ux + uw - 8, ty0 + 77, fmt("%d/%d", m.hp, m.maxHP(*gd)), t.text);
            int ly = ty0 + 86;
            for (size_t i = 2; i < info.size() && ly + 12 <= bottom; i++) {
                Theme::text(p, ux + 8, ly, Theme::fit(p, info[i].first, uw - 16), i == 2 ? t.text : Theme::mapText(info[i].second)); ly += 14;
            }
        } else {
            int ly = y + 2;
            if (info.empty()) Theme::text(p, ux + 8, ly, "Point at a foe to see your odds.", t.dim);
            for (size_t i = 0; i < info.size() && ly + 12 <= bottom; i++) { Theme::text(p, ux + 8, ly, Theme::fit(p, info[i].first, uw - 16), i == 0 ? t.text : Theme::mapText(info[i].second)); ly += 14; }
        }
    };

    // ---------------- the context prompt over the leader ----------------
    auto prompt = [&]() {
        if (!map || party.members.empty() || mode != GMode::EXPLORE) return;
        std::string verb, what = nearText(*this, verb);
        std::string k = bindName(A_INTERACT); if (k.empty()) return;
        std::string s = what.empty() ? "look around" : verb + " " + what;
        int sx = VIEW_X + px * TILE - camX, sy = VIEW_Y + py * TILE - camY;
        int bw = std::min(260, Theme::textW(p, k) + 12 + Theme::textW(p, s) + 30), bh = 20;
        s = Theme::fit(p, s, bw - 30 - Theme::textW(p, k) - 12);
        int bx = std::clamp(sx - 32, 4, W - bw - 4), by = sy - 24;
        if (by < VIEW_Y + 4) by = sy + TILE + 4;
        cv.fillRect(bx, by, bw, bh, t.back); cv.rect(bx, by, bw, bh, t.text);
        torch(*this, cv, bx + 5, by + 4);
        Theme::text(p, bx + 20, by + 4, k, t.text);
        Theme::text(p, bx + 20 + Theme::textW(p, k) + 12, by + 4, s, t.text);
    };

    // ---------------- the Start menu (pause) ----------------
    auto startMenu = [&]() {
        auto entries = pauseEntries();
        int ne = (int)entries.size(), mw = 170, mh = 20 + ne * 24 + 8 + (inCombat() ? 14 : 0);
        int mx = 6, my = 26;
        uiRect(mx, my, mw + 4, mh + 4); hudHit(mx, my, mw, mh, HH_TOOL, 0);
        Theme::toolWindow(p, mx, my, mw, mh, "Menu");
        for (int i = 0; i < ne; i++) {
            int ey = my + 20 + i * 24; auto& e = entries[i];
            bool sel = settingsOpen ? e.id == 5 : i == pauseSel;
            if (sel) { Theme::glow(cv, mx + 6, ey, mw - 12, 20); torch(*this, cv, mx + 10, ey + 4); }
            else if (!settingsOpen && e.enabled && hover(mx + 6, ey, mw - 12, 20)) dotted(cv, mx + 6, ey, mw - 12, 20, t.dim);
            Theme::text(p, mx + 26, ey + 4, e.label, !e.enabled ? t.dim : sel ? t.accent : t.text);
            if (!settingsOpen) hudHit(mx + 6, ey, mw - 12, 20, HH_PAUSE, i);
        }
        if (inCombat()) Theme::small(p, mx + 10, my + 20 + ne * 24 + 4, "some need the fight over", t.dim);
    };

    // ---------------- the menu bar ----------------
    auto menuBar = [&]() {
        uiRect(0, 0, W, 23);
        cv.fillRect(0, 0, W, 22, t.text); cv.fillRect(0, 22, W, 1, t.back);
        // Start: a black button with the torch
        bool startDown = paused;
        cv.fillRect(6, 3, 64, 16, t.back);
        if (startDown) cv.rect(6, 3, 64, 16, t.accent);
        torch(*this, cv, 10, 5);
        Theme::text(p, 26, 5, "Start", startDown ? t.accent : t.text);
        if (paused) hudHit(6, 3, 64, 16, HH_PAUSE, 0);                 // Start again = Resume (entry 0)
        else if (combat) hudHit(6, 3, 64, 16, HH_PAUSE, -1);         // a fight: open the Start menu
        else if (mode == GMode::EXPLORE) hudHit(6, 3, 64, 16, HH_ACTION, A_CANCEL);
        auto boldText = [&](int x, const std::string& s) { bool b = font.bold; font.bold = true; int w = font.draw(cv, *ctx.ts, x, 5, s, t.back); font.bold = b; return w; };
        auto boldW = [&](const std::string& s) { bool b = font.bold; font.bold = true; int w = font.width(s); font.bold = b; return w; };
        std::string place = map ? map->name : "";
        std::string when = fmt("day %d  %02d:%02d", day, clockMin / 60, clockMin % 60);
        if (paused) {
            boldText(84, "Paused");
            std::string right = place + "   " + when;
            if (W - 10 - Theme::textW(p, right) < 84 + boldW("Paused") + 20) right = when;
            Theme::textRight(p, W - 10, 5, right, t.back);
            return;
        }
        if (combat) {
            int x = 84; x += boldText(x, fmt("Round %d", cb.round)) + 18;
            std::string who;
            if (cb.ending) who = cb.victory ? "Victory!" : cb.fled ? "Escaped!" : "Defeat...";
            else if (cb.selectedUnit >= 0 && cb.selectedUnit < (int)cb.units.size()) who = cb.units[cb.selectedUnit].ch.name + "'s turn";
            int rightX = W - 10 - Theme::textW(p, who);
            Theme::text(p, rightX, 5, who, cb.ending && !cb.victory ? Theme::foe() : t.back);
            int shown = 0;
            for (int k = 0; k < (int)cb.order.size(); k++) {
                if (x + 16 > rightX - 12) break;
                int idx = cb.order[(cb.turnPos + k) % cb.order.size()]; auto& u = cb.units[idx];
                if (!u.ch.alive || u.gone) continue;
                bool cur = idx == cb.selectedUnit;
                cv.fillRect(x, 3, 16, 16, cur ? t.back : t.text);
                cv.rect(x, 3, 16, 16, u.friendly() ? t.back : Theme::foe());
                sprite(*ctx.ts, cv, u.ch, x + 2, 5, 1);
                hudHit(x, 3, 16, 16, HH_UNIT, idx);
                x += 20; shown++;
            }
            if (shown > 0 && x + 12 + Theme::textW(p, "<- turn order") < rightX - 12) Theme::text(p, x + 12, 5, "<- turn order", t.dim);
            return;
        }
        // exploring (and the menus over it): the program's menus
        int x = 84; x += boldText(x, "Triffall") + 18;
        struct M { const char* label; int kind, arg; };
        static const M items[] = {{"Party", HH_ACTION, A_MENU}, {"Items", HH_ACTION, A_ITEMS}, {"Spells", HH_ACTION, A_SPELLS}, {"Journal", HH_ACTION, A_JOURNAL}, {"Map", HH_ACTION, A_WORLDMAP}, {"Craft", HH_CRAFT, 0}};
        int openIdx = -1;
        if (mode == GMode::MENU) openIdx = menuTab == 1 ? 1 : menuTab == 2 ? 2 : menuTab == 3 ? 3 : 0;
        else if (mode == GMode::JOURNAL) openIdx = 3; else if (mode == GMode::WORLDMAP) openIdx = 4; else if (mode == GMode::CRAFT) openIdx = 5;
        std::string right = place + "   " + when + "   " + fmt("%d gold", party.gold);
        int rightW = Theme::textW(p, right);
        int menusEnd = x; for (auto& m : items) menusEnd += Theme::textW(p, m.label) + 18;
        if (W - 10 - rightW < menusEnd) { right = when + "   " + fmt("%d gold", party.gold); rightW = Theme::textW(p, right); }
        if (W - 10 - rightW < menusEnd) { right = fmt("%d gold", party.gold); rightW = Theme::textW(p, right); }
        for (int i = 0; i < 6; i++) {
            const M& m = items[i]; int w = Theme::textW(p, m.label);
            bool inv = i == openIdx || (mode == GMode::EXPLORE && hover(x - 5, 2, w + 10, 18));
            if (inv) cv.fillRect(x - 5, 2, w + 10, 18, t.back);
            Theme::text(p, x, 5, m.label, inv ? t.text : t.back);
            if (mode == GMode::EXPLORE) hudHit(x - 5, 2, w + 10, 18, m.kind, m.arg);
            x += w + 18;
        }
        Theme::textRight(p, W - 10, 5, right, t.back);
    };

    // ---------------- put it together ----------------
    if (settingsOpen) {
        cv.dim(0, 22, W, H - 22, 153);
        menuBar();
        if (paused) startMenu();
        return;
    }
    if (!menuOver) {
        if (!combat) prompt();
        partyWin(L.party); newsWin(L.news); beltWin(L.belt);
        if (combat) unitWins();
    }
    menuBar();
    if (paused) startMenu();
}

// ---------------- the Party window (#181) ----------------
void Game::drawOneBitParty(Canvas& cv, InputState& in) {
    Theme::NoMap nm; const UiTheme& t = Theme::th(); Pen p{&cv, ctx.font, ctx.ts};
    Rect a = menuArea();
    const int ww = a.w, wh = std::min(478, a.h - (a.h >= 490 ? 12 : 0));
    const int wx = a.x, wy = a.y + (a.h >= 490 ? 12 : 0);
    cv.dim(0, 22, SCREEN_W, SCREEN_H - 22, 140);
    uiRect(wx, wy, ww + 4, wh + 4);
    Theme::windowChrome(p, wx, wy, ww, wh, "Party", true);
    auto hover = [&](int x, int y, int w, int h) { return in.mouseX >= x && in.mouseX < x + w && in.mouseY >= y && in.mouseY < y + h; };
    const int nM = (int)party.members.size();
    const int mi = clampi(menuMember, 0, nM - 1);
    Character& m = party.members[mi];
    const bool narrow = ww < 900;
    // tabs
    {
        static const char* TABN[] = {"Party", "Items", "Spells", "Quests"};
        int x = wx + 16, y = wy + 32;
        for (int i = 0; i < 4; i++) {
            int w = Theme::titleW(p, TABN[i]) + 20; bool on = menuTab == i;
            if (on) { Color8 ink = t.back; cv.fillRect(x, y, w, 20, t.text); Theme::title(p, x + 10, y + 4, TABN[i], 1, &ink); }
            else { cv.rect(x, y, w, 20, hover(x, y, w, 20) ? t.accent : t.dim); Theme::title(p, x + 10, y + 4, TABN[i]); }
            hudHit(x, y, w, 20, HH_PTAB, i);
            x += w + 6;
        }
        Theme::textRight(p, wx + ww - 16, y + 5, lower(bindName(A_PAGEDOWN)) + ": next", t.dim);
        hdots(cv, wx + 16, wy + 60, ww - 32, t.dim);
    }
    // member cards
    const int cardW = narrow ? 170 : 200;
    {
        int pitch = nM > 4 ? std::max(40, 236 / nM) : 58, ch = std::min(50, pitch - 8);
        for (int i = 0; i < nM; i++) {
            const Character& c = party.members[i];
            int y = wy + 74 + i * pitch; bool on = i == mi;
            if (on && !menuSubFocus) Theme::glow(cv, wx + 16, y, cardW, ch);
            else if (on) cv.rect(wx + 16, y, cardW, ch, t.accent);
            else dotted(cv, wx + 16, y, cardW, ch, hover(wx + 16, y, cardW, ch) ? t.accent : t.dim);
            sprite(*ctx.ts, cv, c, wx + 24, y + (ch - 24) / 2, 2);
            Theme::text(p, wx + 60, y + 9, Theme::fit(p, c.name, cardW - 100), !c.alive ? t.dim : on ? t.accent : t.text);
            Theme::text(p, wx + 60, y + 26, classLine(c, *gd), on ? t.text : t.dim);
            if (i == leader) Theme::smallRight(p, wx + 16 + cardW - 8, y + 10, "leader", t.accent);
            if (c.wounds > 0) Theme::smallRight(p, wx + 16 + cardW - 8, y + 28, fmt("wounded x%d", c.wounds), Theme::foe());
            hudHit(wx + 16, y, cardW, ch, HH_MEMBER, i);
        }
        if (nM <= 4) {
            Theme::text(p, wx + 16, wy + 312, lower(bindName(A_UP)) + "/" + lower(bindName(A_DOWN)) + ": member", t.dim);
            Theme::text(p, wx + 16, wy + 326, lower(bindName(A_INTERACT)) + ": make leader", t.dim);
        }
    }
    // the hero
    const int hx = wx + 16 + cardW + 20;
    {
        cv.rect(hx, wy + 74, 132, 132, t.dim);
        if (m.recolor) ctx.ts->drawRecolor(cv, m.tile, hx + 18, wy + 92, 8, m.color); else ctx.ts->draw(cv, m.tile, hx + 18, wy + 92, 8);
        if (!m.alive) Theme::small(p, hx + 6, wy + 196, "fallen", Theme::foe());
        Color8 am = t.accent; Theme::title(p, hx, wy + 216, Theme::fit(p, m.name, 132), 2, &am);
        Theme::text(p, hx, wy + 244, classLine(m, *gd), t.text);
        Theme::text(p, hx, wy + 258, fmt("XP %d / %d", m.xp, m.xpToNext()), t.dim);
        obBar(cv, hx, wy + 272, 132, 6, m.xp, m.xpToNext(), t.accent);
    }
    // attributes: a ledger with dotted leaders, then the six stats as tiles
    const int lx = hx + 150;
    static int statFocus = 0;
    {
        Theme::title(p, lx, wy + 74, "Attributes");
        int lo, hi, ty; m.attackDamage(*gd, lo, hi, ty);
        struct R { std::string l, v; Color8 c; };
        std::vector<R> rows = {{"Hit points", fmt("%d / %d", m.hp, m.maxHP(*gd)), Theme::hp()}, {"Mana", fmt("%d / %d", m.mp, m.maxMP(*gd)), Theme::mp()},
                               {"Action points", std::to_string(m.maxAP(*gd)), t.accent}, {"Move", std::to_string(m.maxMove(*gd)), t.text},
                               {"Damage", fmt("%d-%d %s", lo, hi, lower(dmgTypeName(ty)).c_str()), t.text}, {"Range", std::to_string(m.attackRange(*gd)), t.text},
                               {"Defense", std::to_string(m.defense(*gd)), t.text}, {"Evasion", std::to_string(m.evasion(*gd)), t.text}};
        for (size_t i = 0; i < rows.size(); i++) {
            int y = wy + 96 + (int)i * 16;
            int lw = Theme::textW(p, rows[i].l), vw = Theme::textW(p, rows[i].v);
            Theme::text(p, lx, y, rows[i].l, t.text);
            Theme::textRight(p, lx + 190, y, rows[i].v, rows[i].c);
            hdots(cv, lx + lw + 4, y + 9, 190 - lw - vw - 8, t.dim);
        }
        for (int s = 0; s < 6 && s < ST_COUNT; s++) if (hover(lx + (s % 3) * 66, wy + 234 + (s / 3) * 40, 60, 34)) statFocus = s;
        for (int s = 0; s < 6 && s < ST_COUNT; s++) {
            int x = lx + (s % 3) * 66, y = wy + 234 + (s / 3) * 40; bool on = s == statFocus;
            int v = m.stat(s, *gd), b = m.baseStats[s];
            if (on) Theme::glow(cv, x, y, 60, 34); else cv.rect(x, y, 60, 34, t.dim);
            Theme::text(p, x + 5, y + 4, statName(s), on ? t.accent : t.text);
            std::string vs = std::to_string(v);
            Theme::small(p, x + 55 - Theme::smallW(p, vs, 2), y + 14, vs, v > b ? Theme::good() : v < b ? Theme::foe() : t.text, 2);
        }
        std::string sl = statLine(statFocus); if (!sl.empty()) sl[0] = (char)tolower((unsigned char)sl[0]);
        Theme::text(p, lx, wy + 318, Theme::fit(p, std::string(statName(statFocus)) + ": " + sl + ".", 200), t.dim);
    }
    // equipment
    const int ex = lx + 214, ew = std::min(236, wx + ww - 16 - ex);
    {
        Theme::title(p, ex, wy + 74, "Equipment");
        for (int s = 0; s < SL_COUNT; s++) {
            int y = wy + 96 + s * 34; bool on = menuSubFocus && menuSub == s;
            if (on) { Theme::glow(cv, ex, y, ew, 28); torch(*this, cv, ex + 4, y + 8); }
            else dotted(cv, ex, y, ew, 28, hover(ex, y, ew, 28) ? t.accent : t.dim);
            Theme::small(p, ex + 20, y + 3, lower(slotName(s)), t.dim);
            const ItemDef* it = m.equip[s].empty() ? nullptr : gd->item(m.equip[s]);
            std::string ks;
            if (it) {
                if (it->dmgMax > 0) ks = fmt("%d-%d", it->dmgMin, it->dmgMax);
                else if (it->def > 0) ks = fmt("def %d", it->def);
                else if (it->evasion > 0) ks = fmt("eva %d", it->evasion);
                else if (it->light > 0) ks = fmt("light %d", it->light);
                drawItemTile(*ctx.ts, cv, *it, ex + 20, y + 13, 1);
                int ksw = ks.empty() ? 0 : Theme::textW(p, ks) + 8;
                Theme::text(p, ex + 36, y + 13, Theme::fit(p, it->name, ew - 44 - ksw), on ? t.accent : t.text);
                if (!ks.empty()) Theme::textRight(p, ex + ew - 6, y + 13, ks, on ? t.text : t.dim);
            } else Theme::text(p, ex + 20, y + 13, "empty", t.dim);
            hudHit(ex, y, ew, 28, HH_SLOT, s);
        }
        if (const ItemDef* it = menuSubFocus && !m.equip[menuSub].empty() ? gd->item(m.equip[menuSub]) : nullptr) {
            std::string wv = gearWarning(*it, m);
            if (!wv.empty()) ctx.font->drawWrapped(cv, *ctx.ts, ex, wy + 96 + SL_COUNT * 34 + 4, ew, wv, t.accent);
        }
    }
    // class ranks
    {
        int cy = wy + 348;
        Theme::title(p, hx, cy, "Class ranks");
        bool can = m.rankPoints > 0 && !levelUpQueue.empty();
        if (m.rankPoints > 0) {
            std::string s = fmt("%d rank point%s free", m.rankPoints, m.rankPoints == 1 ? "" : "s") + (can ? ": level up" : "");
            int sx = hx + Theme::titleW(p, "Class ranks") + 16;
            Theme::text(p, sx, cy + 1, s, t.accent);
            if (can) hudHit(sx, cy - 2, Theme::textW(p, s), 16, HH_RANKUP, 0);
        }
        auto cls = classOrder(*gd);
        int pitch = std::min(128, (wx + ww - 16 - hx) / 5);
        for (size_t i = 0; i < cls.size() && i < 5; i++) {
            int x = hx + (int)i * pitch, y = cy + 22; int r = m.rank(cls[i]);
            const ClassDef* cd = gd->cls(cls[i]);
            Theme::text(p, x, y, cd ? cd->name : cls[i], r > 0 ? t.accent : t.text);
            int dw = std::min(9, (pitch - 20) / 10 - 2);
            for (int k = 0; k < 10; k++) cv.fillRect(x + k * (dw + 2), y + 16, dw, 4, k < r ? t.accent : t.pipOff);
        }
    }
    // key hints along the bottom
    {
        hdots(cv, wx + 16, wy + wh - 40, ww - 32, t.dim);
        struct KH { std::string k, l; int kind; };
        std::vector<KH> hints;
        if (menuPrompt == "equip") hints = {{bindName(A_CONFIRM), "equip", 0}, {bindName(A_UP) + "/" + bindName(A_DOWN), "choose", 0}, {bindName(A_CANCEL), "cancel", 0}};
        else if (menuSubFocus) hints = {{bindName(A_CONFIRM), "change", 0}, {bindName(A_ITEMS), "unequip", 0}, {bindName(A_LEFT), "members", 0}, {bindName(A_CANCEL), "back", 0}};
        else hints = {{bindName(A_CONFIRM), "equipment", 0}, {bindName(A_INTERACT), leader == mi ? "leading" : "lead", HH_LEAD}, {bindName(A_PAGEDOWN), "next tab", 0}, {bindName(A_CANCEL), "back", 0}};
        int kx = wx + 16, ky = wy + wh - 30;
        for (auto& h : hints) {
            if (h.k.empty()) continue;
            int w = obKey(p, kx, ky, h.k, 0, t.text, t.text);
            Theme::text(p, kx + w + 6, ky + 2, h.l, t.dim);
            if (h.kind) hudHit(kx, ky, w + 6 + Theme::textW(p, h.l), 16, h.kind, 0);
            kx += w + Theme::textW(p, h.l) + 28;
        }
        Theme::textRight(p, wx + ww - 16, ky + 2, fmt("%d gold", party.gold), t.gold);
    }
    // the equip picker: a tool window over the attributes
    if (menuPrompt == "equip") {
        auto cands = equipCandidates(menuSub);
        int nC = (int)cands.size() + 1, vis = std::min(nC, 12);
        int pw = 214, px0 = lx - 8, py0 = wy + 70, ph = 24 + vis * 20 + 26;
        uiRect(px0, py0, pw + 4, ph + 4);
        Theme::toolWindow(p, px0, py0, pw, ph, std::string("Equip ") + lower(slotName(menuSub)));
        int first = clampi(menuSel - vis + 1, 0, std::max(0, nC - vis));
        for (int k = 0; k < vis; k++) {
            int i = first + k, ry = py0 + 22 + k * 20; bool sel = i == menuSel;
            if (sel) { Theme::glow(cv, px0 + 6, ry, pw - 12, 18); torch(*this, cv, px0 + 9, ry + 3); }
            if (i == 0) Theme::text(p, px0 + 26, ry + 3, "(take off)", sel ? t.accent : t.dim);
            else if (const ItemDef* d = gd->item(cands[i - 1])) {
                drawItemTile(*ctx.ts, cv, *d, px0 + 26, ry + 3, 1);
                std::string ks = d->dmgMax > 0 ? fmt("%d-%d", d->dmgMin, d->dmgMax) : d->def > 0 ? fmt("def %d", d->def) : "";
                int ksw = ks.empty() ? 0 : Theme::textW(p, ks) + 6;
                Theme::text(p, px0 + 42, ry + 3, Theme::fit(p, d->name, pw - 56 - ksw), sel ? t.accent : t.text);
                if (!ks.empty()) Theme::textRight(p, px0 + pw - 10, ry + 3, ks, t.dim);
            }
            hudHit(px0 + 6, ry, pw - 12, 18, HH_PICK, i);
        }
        if (nC > vis) Theme::scrollbar(cv, px0 + pw - 6, py0 + 22, 4, vis * 20, first, vis, nC);
        if (menuSel > 0 && menuSel - 1 < (int)cands.size()) if (const ItemDef* d = gd->item(cands[menuSel - 1])) {
            std::string wv = gearWarning(*d, m);
            Theme::text(p, px0 + 8, py0 + ph - 18, Theme::fit(p, wv.empty() ? d->name : wv, pw - 16), wv.empty() ? t.dim : t.accent);
        }
    }
}
