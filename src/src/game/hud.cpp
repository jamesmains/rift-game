// 1.8.3 (#170-#183): the game's HUD in each UI theme, and where the world and the menus go on the screen.
//   frame  (Adventurer's Frame): the world in a well at the top left, a party column at the right, the Chronicle and
//          the Belt along the bottom; in a fight the column holds the turn order, the acting unit and the Actions list,
//          the bottom the target card and the log. Pause and Settings live in the column / the viewport.
//   native (Desktop-native): hud_native.cpp     onebit (One-Bit Desktop): hud_onebit.cpp
#include "game.h"
#include "settings_ui.h"
#include "../core/audio.h"
#include "tiles.h"
#include <algorithm>

using namespace UI;
using Theme::Pen;

// ---------------- layout ----------------
static int frameColW() { return SCREEN_W >= 853 ? 236 : 200; }
static int frameBotH() { return SCREEN_H >= 540 ? 122 : 100; }

void Game::computeLayout() {
    const int W = SCREEN_W, H = SCREEN_H;
    if (Theme::native()) { VIEW_X = 6; VIEW_Y = 74; VIEW_W = W - 12; VIEW_H = H - 74 - 52; }
    else if (Theme::onebit()) { VIEW_X = 0; VIEW_Y = 22; VIEW_W = W; VIEW_H = H - 22; }
    else { VIEW_X = 2; VIEW_Y = 2; VIEW_W = W - frameColW() - 6; VIEW_H = H - frameBotH() - 6; }
    VIEW_W = std::max(VIEW_W, 64); VIEW_H = std::max(VIEW_H, 64);
}

Rect Game::menuArea() const {
    const int W = SCREEN_W, H = SCREEN_H;
    // 1.8.5 (#199): on a short screen (under 540 high) the native dialog and the one-bit window cover the taskbar / the
    // menu bar, so every menu gets about the height it was laid out for
    if (Theme::native()) { int w = std::min(W - 8, 660); if (H < 540) return {(W - w) / 2, 2, w, H - 4}; int h = std::min(H - 32, 512); return {(W - w) / 2, std::max(0, (H - 28 - h) / 2), w, h}; }
    if (Theme::onebit()) { int w = std::min(W - 16, 912); if (H < 540) return {(W - w) / 2, 2, w, H - 4}; return {(W - w) / 2, 24, w, H - 28}; }
    // frame: beside the column; when a menu would not fit there, a panel in the middle over the dimmed screen
    int w = W - frameColW() - 2;
    if (w < BASE_W + 12 && W >= BASE_W + 12) return {(W - (BASE_W + 12)) / 2, 0, BASE_W + 12, H};
    return {0, 0, w, H};
}
// 1.8.5 (#196 #199): the host every full-screen menu sits in (the themed Party tab draws in the same rect)
Rect Game::menuChromeRect() const {
    Rect a = menuArea();
    if (Theme::onebit()) { int top = a.h >= 490 ? 12 : 0; return {a.x, a.y + top, a.w, std::min(478, a.h - top)}; }
    return a;
}
static Rect hostInside(const Rect& c) {
    if (Theme::native()) return {c.x + 4, c.y + 23, c.w - 8, c.h - 27};
    if (Theme::onebit()) return {c.x + 1, c.y + 22, c.w - 2, c.h - 23};
    return {c.x + 4, c.y + 4, c.w - 8, c.h - 8};
}
// the Party screen's content (under its tabs): top and bottom
static int menuTabsTop(const Rect& c) { return Theme::native() ? c.y + 48 : Theme::onebit() ? c.y + 64 : c.y + 42; }
static int menuTabsBottom(const Rect& c) { return Theme::native() ? c.y + c.h - 10 : c.y + c.h - 6; }
Rect Game::menuHostedView(GMode m) const {
    Rect c = menuChromeRect(), in = hostInside(c);
    int top = in.y, bottom = in.y + in.h;
    if (m == GMode::MENU) { top = menuTabsTop(c) - 36; bottom = menuTabsBottom(c); }   // the old layouts start their content at y 36, under the tabs
    int x = clampi(in.x + (in.w - BASE_W) / 2, 0, std::max(0, SCREEN_W - BASE_W));
    return {x, top, BASE_W, std::min(BASE_H, bottom - top)};
}
bool Game::menuHosted(GMode m) const {
    if (!subViewMode(m) || (m == GMode::MENU && themedParty())) return false;
    Rect in = hostInside(menuChromeRect());
    return SCREEN_W >= BASE_W && in.w >= BASE_W - 4 && menuHostedView(m).h >= 420;
}
std::string Game::menuHostTitle(GMode m) const {
    switch (m) {
        case GMode::MENU: return Theme::native() ? "Party Properties" : "Party";
        case GMode::SHOP: return "Shop";
        case GMode::CRAFT: return craftStation == "forge" ? "Forge" : craftStation == "alchemy" ? "Alchemy Bench" : craftStation == "workbench" ? "Workbench" : craftStation == "all" ? "Crafting" : "Field Crafting";
        case GMode::JOURNAL: return "Journal";
        case GMode::BOUNTY: { std::string tn = boardTown; for (auto& l : locations) if (l.mapId == boardTown || l.id == boardTown) tn = l.name; if (const BoardDef* bd = board(boardTown)) if (!bd->name.empty()) tn = bd->name; return "Notice Board - " + tn; }
        case GMode::LEVELUP: return "Level Up!";
        case GMode::WORLDMAP: return wmPick && !wmPickPrompt.empty() ? wmPickPrompt : wmTravel ? "Where to?" : "World Map";
        case GMode::SETTINGS: return "Settings";
        default: return "";
    }
}
void Game::hostTitle(int x, int y, const std::string& s) {
    if (!menuHostedNow || !Theme::frame()) return;
    Theme::NoMap nm; Pen p{ctx.cv, ctx.font, ctx.ts}; Theme::title(p, x, y, s);
}
int nativeMenuTabs(Game& g, Canvas& cv, InputState& in, const Rect& c);   // hud_native.cpp
int oneBitMenuTabs(Game& g, Canvas& cv, InputState& in, const Rect& c);   // hud_onebit.cpp
// the Party screen's tabs (and gold), the same on every tab; returns where the content starts
int Game::drawMenuTabs(Canvas& cv, InputState& in, const Rect& c) {
    if (Theme::native()) return nativeMenuTabs(*this, cv, in, c);
    if (Theme::onebit()) return oneBitMenuTabs(*this, cv, in, c);
    Theme::NoMap nm; const UiTheme& t = Theme::th(); Pen p{&cv, ctx.font, ctx.ts};
    const int X = c.x + 2, Y = c.y + 2, Wd = c.w - 4;
    static const char* TABN[] = {"Party", "Items", "Spells", "Quests"};
    static const int TABK[] = {A_MENU, A_ITEMS, A_SPELLS, A_JOURNAL};
    const bool picking = menuPrompt == "respec";
    for (int i = 0; i < 4; i++) {
        int tx = X + 12 + i * 112; bool on = menuTab == i;
        bool hover = in.mouseX >= tx && in.mouseX < tx + 106 && in.mouseY >= Y + 8 && in.mouseY < Y + 32;
        Theme::button(p, tx, Y + 8, 106, 24, TABN[i], on ? Theme::BS_SELECTED : Theme::BS_NORMAL, hover, gPadGlyphs ? std::string(" ") : UI::bindLabel(TABK[i]));   // a controller pages with LB / RB (shown at the right)
        if (!picking) hudHit(tx, Y + 8, 106, 24, HH_PTAB, i);
    }
    std::string gold = fmt("%d gold", party.gold);
    Theme::textRight(p, X + Wd - 12, Y + 14, gold, t.gold);
    if (gPadGlyphs) Theme::textRight(p, X + Wd - 22 - Theme::textW(p, gold), Y + 14, UI::bindLabel(A_PAGEUP) + " / " + UI::bindLabel(A_PAGEDOWN) + " tabs", t.dim);
    Theme::rule(cv, X + 10, Y + 36, Wd - 20);
    return menuTabsTop(c);
}
Rect Game::popupArea() const {
    const int W = SCREEN_W, H = SCREEN_H;
    if (Theme::native()) return {4, 72, W - 8, H - 72 - 50};
    if (Theme::onebit()) return {0, 22, W, H - 22 - 112};
    return {0, 0, W - frameColW() - 2, H - frameBotH() - 2};
}
// a full-screen menu (laid out for 640x480) goes in the theme's host when it fits there (1.8.5), else in the middle
bool Game::subViewOrigin(GMode m, int& x, int& y) const {
    x = y = 0;
    if (!subViewMode(m) || (m == GMode::MENU && themedParty())) return false;
    if (menuHosted(m)) { Rect v = menuHostedView(m); x = v.x; y = v.y; return true; }
    x = (SCREEN_W - BASE_W) / 2; y = (SCREEN_H - BASE_H) / 2;
    return !(x == 0 && y == 0 && SCREEN_W == BASE_W && SCREEN_H == BASE_H);
}
bool Game::modeArea(GMode m, Rect& r) const {
    int x = 0, y = 0;
    if (subViewOrigin(m, x, y)) { r = menuHosted(m) ? menuHostedView(m) : Rect{x, y, BASE_W, BASE_H}; return true; }
    if (m == GMode::DIALOGUE || m == GMode::MESSAGE || m == GMode::VICTORY || m == GMode::SAVELOAD || m == GMode::NAME) {
        // the popups sit over the world, inside the theme's frame (the column, the status bar and taskbar, the bottom windows stay)
        r = popupArea();
        if (r.w >= 400 && r.h >= 300 && !(r.x == 0 && r.y == 0 && r.w == SCREEN_W && r.h == SCREEN_H)) return true;
    }
    r = {0, 0, SCREEN_W, SCREEN_H};
    return false;
}
// 1.8.5 (#196 #199): the host behind a menu screen: the frame's carved panel, the native dialog (title + close box),
// the one-bit window; the Party screen's tabs on every Party tab. Too little room: the old dimmed full-screen look.
void Game::drawMenuBackdrop(Canvas& cv, GMode m, InputState& in) {
    Theme::NoMap nm; Pen p{&cv, ctx.font, ctx.ts};
    if (m == GMode::SETTINGS) {
        if (Theme::native() || Theme::onebit()) return;   // settings_ui.cpp draws its own dialog / window
        Rect a = menuArea();
        Color8 deep{20, 22, 31, 255}; Theme::well(cv, a.x, a.y, a.w, a.h, &deep); Theme::panel(cv, a.x + 2, a.y + 2, a.w - 4, a.h - 4);
        return;
    }
    if (!menuHosted(m)) { cv.dim(0, 0, SCREEN_W, SCREEN_H, 150); return; }
    Rect c = menuChromeRect();
    if (Theme::native()) {
        cv.dim(VIEW_X, VIEW_Y, VIEW_W, VIEW_H, 115);
        Theme::windowChrome(p, c.x, c.y, c.w, c.h, menuHostTitle(m), true, true);
        hudHit(c.x + c.w - 21, c.y + 5, 16, 14, HH_HOSTCLOSE, 0);
    } else if (Theme::onebit()) {
        cv.dim(0, 22, SCREEN_W, SCREEN_H - 22, 140);
        Theme::windowChrome(p, c.x, c.y, c.w, c.h, menuHostTitle(m), true, true);
        hudHit(c.x + 6, c.y + 3, 18, 16, HH_HOSTCLOSE, 0);
    } else {
        if (c.x > 0) cv.dim(0, 0, SCREEN_W, SCREEN_H, 140);   // over the column too: the rest of the screen dims
        Color8 deep{20, 22, 31, 255}; Theme::well(cv, c.x, c.y, c.w, c.h, &deep); Theme::panel(cv, c.x + 2, c.y + 2, c.w - 4, c.h - 4);
    }
    uiRect(c.x, c.y, c.w, c.h);
    if (m == GMode::MENU) drawMenuTabs(cv, in, c);
}

// ---------------- shared HUD helpers ----------------
Color8 Game::logColor(const std::string& l) const {
    const UiTheme& t = Theme::th();
    if (startsWith(l, "Achievement unlocked")) return t.achieve;
    if (l.find("joined the party") != std::string::npos || startsWith(l, "Victory") || l.find("level ") != std::string::npos && l.find("reached") != std::string::npos) return Theme::good();
    if (startsWith(l, "Combat begins") || startsWith(l, "Round ")) return t.accent;
    if (l.find(" dies") != std::string::npos || l.find(" falls") != std::string::npos || l.find("Not enough") != std::string::npos) return Theme::th().id == ThemeId::Native ? RGB(160, 0, 0) : Theme::foe();
    return t.text;
}
std::string Game::hudPlaceTime() const { return clockText(); }

static std::string classLine(const Character& m, const GameData& gd) {
    const ClassDef* c = gd.cls(m.classId);
    std::string n = c ? c->name : m.classId; if (!n.empty()) n[0] = (char)toupper((unsigned char)n[0]);
    int r = m.rank(m.classId); return n + " " + std::to_string(r > 0 ? r : m.level);
}
static const Character& viewOf(const Game& g, int i) {
    if (g.mode == GMode::COMBAT || g.inCombat()) for (auto& u : g.cb.units) if (u.partyIndex == i) return u.ch;
    return g.party.members[i];
}

// what the leader is next to (the frame's "Near:" line, the one-bit prompt)
static std::string nearText(Game& g, std::string& verb) {
    verb.clear(); if (!g.map) return "";
    int fx = g.px + (g.facing == 0 ? -1 : g.facing == 1 ? 1 : 0), fy = g.py + (g.facing == 2 ? -1 : g.facing == 3 ? 1 : 0);
    std::vector<std::pair<int, int>> spots = {{fx, fy}, {g.px - 1, g.py}, {g.px + 1, g.py}, {g.px, g.py - 1}, {g.px, g.py + 1}, {g.px, g.py}};
    for (auto& s : spots) {
        int x = s.first, y = s.second; if (!g.map->inBounds(x, y)) continue;
        if (NPC* n = g.map->npcAt(x, y)) if (!n->hidden) { verb = "Talk"; return n->name; }
        if (Chest* c = g.map->chestAt(x, y)) { verb = c->opened ? "Look" : "Open"; return "a chest"; }
        if (SignPost* sp = g.map->signAt(x, y)) { verb = "Read"; return sp->title.empty() ? "a sign" : sp->title; }
        if (g.map->saveAt(x, y) && x == g.px && y == g.py) { verb = "Save"; return "a save shrine"; }
        if (Door* d = g.map->doorAt(x, y)) { if (!d->open) { verb = "Open"; return "a door"; } }
        if (Lever* lv = g.map->leverAt(x, y)) { verb = "Pull"; return lv->name.empty() ? "a lever" : lv->name; }
        if (GatherNode* gn = g.map->gatherAt(x, y)) { verb = "Gather"; return gn->name.empty() ? "something to gather" : gn->name; }
        if (Obstacle* o = g.map->obstacleAt(x, y)) { if (!o->cleared) { verb = "Use"; return o->name.empty() ? "something in the way" : o->name; } }
        if (Crate* c = g.map->crateAt(x, y)) { verb = "Look"; return c->name.empty() ? "a crate" : c->name; }
    }
    return "";
}
// 1.8.5 (#197): what Interact would do now, for the Talk / Use buttons (empty: nothing in reach, the button greys out)
std::string Game::interactVerb(std::string* what) {
    std::string verb; std::string w = (mode == GMode::EXPLORE || mode == GMode::PAUSE) ? nearText(*this, verb) : std::string();
    if (what) *what = w;
    return verb;
}

// a belt slot's picture, count and whether it can be used now
static void beltSlot(Game& g, Canvas& cv, int member, int slot, int x, int y, int scale, bool combat, std::string* name = nullptr) {
    if (member < 0 || member >= (int)g.party.members.size()) return;
    const std::string& f = g.party.members[member].favs[slot]; if (f.empty()) return;
    std::string kind = f.substr(0, f.find(':')), id = f.substr(f.find(':') + 1);
    bool ok = true; int n = -1;
    const Character& m = viewOf(g, member);
    if (kind == "spell") { if (auto* s = g.gd->spell(id)) { g.ctx.ts->draw(cv, s->tile, x, y, scale); ok = m.mp >= s->cost && (combat || g.spellUsableOutside(*s)); if (name) *name = s->name; } }
    else if (auto* d = g.gd->item(id)) { drawItemTile(*g.ctx.ts, cv, *d, x, y, scale); n = g.party.countItem(id); ok = n > 0; if (name) *name = d->name; }
    if (!ok) { Theme::NoMap nm; cv.fillRect(x, y, 12 * scale, 12 * scale, RGB(0, 0, 0, 150)); }
    if (n > 1) { Theme::NoMap nm; std::string c = "x" + std::to_string(std::min(n, 99)); int cx = x + 12 * scale + (scale == 1 ? 6 : 2) - g.ctx.font->widthSmall(c), cy = y + 12 * scale + (scale == 1 ? 0 : -6);
        if (scale > 1) cv.fillRect(cx - 1, cy - 1, g.ctx.font->widthSmall(c) + 2, 10, RGB(0, 0, 0, 170));   // 1.8.5: inside the slot, readable over the picture
        g.ctx.font->drawSmall(cv, cx, cy, c, Theme::th().text); }
}

static std::string bindName(int a) { return UI::bindLabel(a); }   // 1.8.5 (#200): the pad button while a controller is in use

// ---------------- 1.8.6 (#206): the favorites picker ----------------
// A controller has no number keys, so its Favorites button (RT) opens a strip of the 8 slots over the world (or over the
// Spells / Items screen, to put something on a slot). It is the same in every theme: the HUD's own favorites may be
// hidden (one-bit rolled up, native's toolbar in a narrow fight).
void Game::openFavPicker(int member, const std::string& assign) {
    if (member < 0 || member >= (int)party.members.size()) return;
    favPickMember = member; favAssign = assign; favPick = 0;
    const auto& favs = party.members[member].favs;
    // putting something on a slot starts on the slot that already has it, else the first empty one; using starts on the first filled one
    int has = -1, empty = -1, filled = -1;
    for (int i = 0; i < 8; i++) { if (has < 0 && !assign.empty() && favs[i] == assign) has = i; if (empty < 0 && favs[i].empty()) empty = i; if (filled < 0 && !favs[i].empty()) filled = i; }
    favPick = !assign.empty() ? (has >= 0 ? has : empty >= 0 ? empty : 0) : (filled >= 0 ? filled : 0);
    gAudio.play("ui_open");
}
void Game::updateFavPicker(InputState& in) {
    const bool fight = mode == GMode::COMBAT;
    const bool ok = (mode == GMode::EXPLORE && favAssign.empty()) || mode == GMode::MENU || (fight && cb.playerTurn && !autoCombat && cb.selectedUnit >= 0 && !cb.ending);
    if (!ok || favPickMember < 0 || favPickMember >= (int)party.members.size()) { favPick = -1; favAssign.clear(); return; }
    int choose = -1; bool close = false;
    if (actP(in, A_LEFT, fight) || actP(in, A_UP, fight)) { favPick = (favPick + 7) % 8; gAudio.play("ui_move"); }
    else if (actP(in, A_RIGHT, fight) || actP(in, A_DOWN, fight)) { favPick = (favPick + 1) % 8; gAudio.play("ui_move"); }
    for (int f = 0; f < 8; f++) if (actP(in, A_FAV1 + f, fight)) choose = f;
    if (actP(in, A_CONFIRM, fight)) choose = favPick;
    if (actP(in, A_CANCEL, fight) || actP(in, A_FAVBAR, fight)) close = true;
    if (in.mousePressed[0]) {
        const HudHit* h = hudHitAt(in.mouseX, in.mouseY);
        if (h && h->kind == HH_FAVPICK) choose = h->arg; else if (!(h && h->kind == HH_TOOL)) close = true;
    }
    if (in.mouseDX || in.mouseDY) if (const HudHit* h = hudHitAt(in.mouseX, in.mouseY)) if (h->kind == HH_FAVPICK) favPick = h->arg;
    // whatever happens, the screen underneath sees no input this frame (no step, no pause, no click on the map)
    in.eatKeys(); in.eatMouse();
    for (int k = 0; k < K_COUNT; k++) in.down[k] = false;
    in.stickX = in.stickY = 0;
    if (choose >= 0) {
        const int m = favPickMember; std::string assign = favAssign;
        favPick = -1; favAssign.clear();
        if (!assign.empty()) {
            party.members[m].favs[choose] = assign;
            std::string id = assign.substr(assign.find(':') + 1), nm = id;
            if (startsWith(assign, "spell:")) { if (auto* s = gd->spell(id)) nm = s->name; } else if (auto* d = gd->item(id)) nm = d->name;
            log(fmt("%s set as favorite %d for %s.", nm.c_str(), choose + 1, party.members[m].name.c_str())); gAudio.play("ui_click");
        } else if (fight) combatUseFav(choose);
        else useFavorite(m, choose);
    } else if (close) { favPick = -1; favAssign.clear(); gAudio.play("ui_close"); }
}
void Game::drawFavPicker(Canvas& cv, InputState& in) {
    if (favPick < 0 || favPickMember < 0 || favPickMember >= (int)party.members.size()) return;
    Theme::NoMap nm; const UiTheme& t = Theme::th(); Pen p{&cv, ctx.font, ctx.ts};
    const Character& m = party.members[favPickMember];
    const bool fight = mode == GMode::COMBAT;
    const int cell = 34, gap = 4, stripW = 8 * cell + 7 * gap;
    const int w = std::min(SCREEN_W - 8, std::max(stripW + 28, 360)), h = Theme::onebit() ? 112 : Theme::native() ? 110 : 116;
    const bool overMenu = mode == GMode::MENU;
    int x = overMenu ? (SCREEN_W - w) / 2 : VIEW_X + (VIEW_W - w) / 2;
    int y = overMenu ? (SCREEN_H - h) / 2 : Theme::onebit() ? VIEW_Y + (VIEW_H - h) / 2 - 30 : VIEW_Y + VIEW_H - h - 10;   // one-bit: above its bottom windows
    std::string title = favAssign.empty() ? m.name + "'s favorites" : "Put it on which favorite?";
    uiRect(x, y, w + 4, h + 4);
    hudHit(x, y, w, h, HH_TOOL, 0);
    Rect r = Theme::toolWindow(p, x, y, w, h, title);
    int sx = r.x + (r.w - stripW) / 2, sy = r.y + 4;
    std::string selName;
    for (int i = 0; i < 8; i++) {
        int cx = sx + i * (cell + gap);
        Color8 wf = t.well; Theme::well(cv, cx, sy, cell, cell, &wf);
        std::string nm2; beltSlot(*this, cv, favPickMember, i, cx + 5, sy + 5, 2, fight, &nm2);
        Theme::small(p, cx + 2, sy + 1, std::to_string(i + 1), t.dim);
        if (i == favPick) { Theme::glow(cv, cx - 2, sy - 2, cell + 4, cell + 4); selName = nm2; }
        hudHit(cx, sy, cell, cell, HH_FAVPICK, i);
    }
    std::string line;
    if (!favAssign.empty()) {
        std::string id = favAssign.substr(favAssign.find(':') + 1), what = id;
        if (startsWith(favAssign, "spell:")) { if (auto* s = gd->spell(id)) what = s->name; } else if (auto* d = gd->item(id)) what = d->name;
        line = fmt("%d: %s", favPick + 1, what.c_str()) + (selName.empty() || selName == what ? std::string() : " (instead of " + selName + ")");
    } else line = fmt("%d: ", favPick + 1) + (selName.empty() ? std::string("empty") : selName);
    Theme::text(p, r.x + 10, sy + cell + 6, Theme::fit(p, line, r.w - 20), selName.empty() && favAssign.empty() ? t.dim : t.accent);
    std::string keys = (gKeys.pad[A_LEFT] == K_PAD_LEFT && gKeys.pad[A_RIGHT] == K_PAD_RIGHT ? std::string("D-pad") : UI::bindLabel(A_LEFT) + "/" + UI::bindLabel(A_RIGHT)) + ": choose   " + UI::bindLabel(A_CONFIRM) + (favAssign.empty() ? ": use   " : ": put here   ") + UI::bindLabel(A_CANCEL) + ": close";
    if (!gPadGlyphs) keys = std::string("Left/Right: choose   1-8 or Enter: ") + (favAssign.empty() ? "use" : "put here") + "   Esc: close";
    Theme::small(p, r.x + 10, sy + cell + 22, Theme::fit(p, keys, r.w - 20), t.dim);
    (void)in;
}

// ---------------- clicks on the HUD (explore, pause) ----------------
bool Game::hudClick(InputState& in) {
    const HudHit* h = hudHitAt(in.mouseX, in.mouseY); if (!h) return false;
    switch (h->kind) {
        case HH_ACTION: injectAction = h->arg; in.eatMouse(); return true;
        case HH_CRAFT: in.eatMouse(); openCraft("any"); return true;
        case HH_MEMBER:   // 1.8.4 (#186): clicking a member while roaming makes them the party leader (the Party screen has its own button)
            if (h->arg >= 0 && h->arg < (int)party.members.size()) {
                if (!party.members[h->arg].alive) log(party.members[h->arg].name + " can't lead the party while fallen.");
                else if (leader != h->arg) { leader = h->arg; notify(party.members[leader].name + " takes the lead."); gAudio.play("ui_click"); }
            }
            in.eatMouse(); return true;
        case HH_BELT: in.eatMouse(); useFavorite(activeLeader(), h->arg); return true;
        case HH_TOOL: in.eatMouse(); return true;
        default: in.eatMouse(); return true;
    }
}

// ---------------- Adventurer's Frame ----------------
void Game::drawFrameHud(Canvas& cv, InputState& in) {
    Theme::NoMap nm; const UiTheme& t = Theme::th(); Pen p{&cv, ctx.font, ctx.ts};
    const int W = SCREEN_W, H = SCREEN_H, colW = frameColW(), botH = frameBotH(), colX = W - colW, y0 = H - botH;
    const bool big = H >= 540, wide = colW >= 236;
    const bool combat = mode == GMode::COMBAT || inCombat();
    const bool paused = mode == GMode::PAUSE || ((mode == GMode::SETTINGS) && std::find(modeStack.begin(), modeStack.end(), GMode::PAUSE) != modeStack.end());
    const bool settingsOpen = mode == GMode::SETTINGS;
    // the world's well
    Theme::bevel(cv, VIEW_X - 2, VIEW_Y - 2, VIEW_W + 4, VIEW_H + 4, t.lo, t.hi);
    Theme::bevel(cv, VIEW_X - 1, VIEW_Y - 1, VIEW_W + 2, VIEW_H + 2, t.lo2, t.hi2);
    uiRect(colX - 2, 0, colW + 2, H); uiRect(0, y0 - 2, colX, botH + 2);
    const int n = (int)party.members.size();
    const int lead = activeLeader();
    auto hpLabel = [&](int x, int y, const char* l) { Theme::small(p, x, y, l, t.dim); };

    // ---- right column ----
    auto plaque = [&](int h, bool showPaused) {
        Theme::panel(cv, colX, 0, colW, h);
        Theme::title(p, colX + 12, 9, Theme::fit(p, map ? map->name : "", colW - (wide ? 90 : 70)));
        std::string tm = hudPlaceTime();
        Theme::text(p, colX + 12, 28, tm, t.text);
        if (outdoors()) {
            int sx = colX + 12 + Theme::textW(p, tm) + 10;
            bool rain = weather == "rain" || weather == "storm";
            if (weather == "clear") { cv.fillRect(sx + 3, 30, 2, 2, t.accent); cv.fillRect(sx + 3, 36, 2, 2, t.accent); cv.fillRect(sx, 33, 2, 2, t.accent); cv.fillRect(sx + 6, 33, 2, 2, t.accent); cv.fillRect(sx + 2, 32, 4, 4, t.accentHi); }
            else { cv.fillRect(sx, 32, 8, 3, t.dim); if (rain) for (int k = 0; k < 3; k++) cv.fillRect(sx + 1 + k * 3, 36, 1, 2, t.mp); }
            std::string wn = weather; if (!wn.empty()) wn[0] = (char)toupper((unsigned char)wn[0]);
            if (sx + 14 + Theme::textW(p, wn) < W - (wide ? 66 : 8)) Theme::text(p, sx + 12, 28, wn, t.dim);
        }
        if (showPaused) Theme::text(p, colX + 12, 46, "Paused", t.accent);
        else Theme::text(p, colX + 12, 46, fmt("%d gold", party.gold), t.gold);
        if (!showPaused && lead >= 0 && lead < n) Theme::textRight(p, W - 12, 46, Theme::fit(p, "Ld: " + party.members[lead].name, colW - 90), t.dim);
        if (wide && h >= 70) {   // a small compass well
            Color8 wf = t.well; Theme::well(cv, W - 62, 8, 52, 32, &wf);
            Theme::small(p, W - 38, 10, "N", t.accent);
            cv.fillRect(W - 37, 19, 2, 12, t.text); cv.fillRect(W - 42, 24, 12, 2, t.dim);
        }
    };
    auto partyCards = [&](int y, int h, int pitch, bool showMP) {
        Theme::panel(cv, colX, y, colW, h);
        Theme::title(p, colX + 12, y + 9, "The Party");
        Theme::textRight(p, W - 12, y + 10, bindName(A_MENU), t.dim);
        int barW = colW - 126;
        for (int i = 0; i < n && i < 4; i++) {
            const Character& m = viewOf(*this, i);
            int cy = y + 28 + i * pitch; bool sel = i == lead;
            if (sel) { cv.fillRect(colX + 6, cy - 2, colW - 12, pitch - 2, t.rowSel); cv.rect(colX + 6, cy - 2, colW - 12, pitch - 2, t.accent); }
            Color8 wf = t.well; Theme::well(cv, colX + 12, cy + 2, 34, 34, &wf);
            if (m.alive) drawUnitSprite(cv, m, colX + 17, cy + 7, false); else ctx.ts->draw(cv, TL::UI_SKULL, colX + 17, cy + 7, 2);
            Theme::text(p, colX + 54, cy + 1, Theme::fit(p, m.name, colW - 140), sel ? t.accent : t.text);
            Theme::textRight(p, W - 12, cy + 1, classLine(m, *gd), t.dim);
            int mhp = m.maxHP(*gd), mmp = m.maxMP(*gd);
            hpLabel(colX + 54, cy + 17, "HP"); Theme::bar(cv, colX + 68, cy + 18, barW, 4, m.hp, mhp, Theme::hp()); Theme::smallRight(p, W - 12, cy + 17, fmt("%d/%d", m.hp, mhp), t.text);
            if (showMP && mmp > 0) { hpLabel(colX + 54, cy + 29, "MP"); Theme::bar(cv, colX + 68, cy + 30, barW, 4, m.mp, mmp, Theme::mp()); Theme::smallRight(p, W - 12, cy + 29, fmt("%d/%d", m.mp, mmp), t.text); }
            if (sel && pitch >= 52) Theme::small(p, colX + 12, cy + 40, "leading", t.accent);
            if (m.wounds > 0 && pitch >= 52) Theme::small(p, colX + 54, cy + 40, fmt("wounded x%d", m.wounds), Theme::foe());
            hudHit(colX + 6, cy - 2, colW - 12, pitch - 2, HH_MEMBER, i);
        }
    };
    auto compactParty = [&](int y, int h, bool combatRows) {
        Theme::panel(cv, colX, y, colW, h);
        Theme::title(p, colX + 12, y + 9, combatRows ? "Party" : "The Party");
        int pitch = std::min(35, (h - 30) / std::max(1, std::min(n, 4)));
        for (int i = 0; i < n && i < 4; i++) {
            const Character& m = viewOf(*this, i);
            int cy = y + 28 + i * pitch;
            int ui = -1; if (combat) for (size_t k = 0; k < cb.units.size(); k++) if (cb.units[k].partyIndex == i) ui = (int)k;
            bool act = combatRows && ui >= 0 && ui == cb.selectedUnit && cb.playerTurn;
            if (act) { cv.fillRect(colX + 6, cy - 2, colW - 12, pitch - 2, t.rowSel); cv.rect(colX + 6, cy - 2, colW - 12, pitch - 2, t.accent); }
            Color8 wf = t.well; int ws = std::min(26, pitch - 6); Theme::well(cv, colX + 10, cy, ws, ws, &wf);
            if (m.alive) { if (m.recolor) ctx.ts->drawRecolor(cv, m.tile, colX + 11, cy + 1, ws >= 26 ? 2 : 1, m.color); else ctx.ts->draw(cv, m.tile, colX + 11, cy + 1, ws >= 26 ? 2 : 1); }
            else ctx.ts->draw(cv, TL::UI_SKULL, colX + 11, cy + 1, ws >= 26 ? 2 : 1);
            if (pitch < 30) {   // a short screen (480 high): name and numbers on one line, a thin bar under them
                Theme::text(p, colX + 42, cy - 1, Theme::fit(p, m.name, colW - 130), act ? t.accent : t.text);
                Theme::bar(cv, colX + 42, cy + 12, colW - 110, 3, m.hp, m.maxHP(*gd), Theme::hp());
                if (combatRows) { Theme::smallRight(p, W - 12, cy, fmt("AP %d", m.ap), t.accent); Theme::smallRight(p, W - 12, cy + 10, fmt("MV %d", m.move), t.move); }
                else Theme::smallRight(p, W - 12, cy + 1, fmt("%d/%d", m.hp, m.maxHP(*gd)), t.text);
            } else {
            Theme::text(p, colX + 42, cy, Theme::fit(p, m.name, colW - 110), act ? t.accent : t.text);
            int bw = colW - 140; Theme::bar(cv, colX + 42, cy + 15, bw, 4, m.hp, m.maxHP(*gd), Theme::hp());
            Theme::small(p, colX + 46 + bw, cy + 13, fmt("%d/%d", m.hp, m.maxHP(*gd)), t.text);
            if (combatRows) { Theme::smallRight(p, W - 12, cy + 1, fmt("AP %d", m.ap), t.accent); Theme::smallRight(p, W - 12, cy + 14, fmt("MV %d", m.move), t.move); }
            }
            if (combatRows && ui >= 0) hudHit(colX + 6, cy - 2, colW - 12, pitch - 2, HH_UNIT, ui);
            else hudHit(colX + 6, cy - 2, colW - 12, pitch - 2, HH_MEMBER, i);
        }
    };

    if (paused) {
        plaque(74, true);
        auto entries = pauseEntries();
        int ne = (int)entries.size(), bh = big ? 26 : 20, pitch = big ? 32 : 24;
        int mh = 30 + ne * pitch + 26;
        Theme::panel(cv, colX, 76, colW, mh);
        Theme::title(p, colX + 12, 85, "Menu");
        static const int pauseKey[8] = {A_CANCEL, A_MENU, -1, A_WORLDMAP, A_JOURNAL, -1, -1, -1};
        for (int i = 0; i < ne; i++) {
            int by = 106 + i * pitch; auto& e = entries[i];
            bool sel = (settingsOpen ? e.id == 5 : i == pauseSel);
            bool hover = in.mouseX >= colX + 10 && in.mouseX < colX + colW - 10 && in.mouseY >= by && in.mouseY < by + bh;
            std::string k = e.id >= 0 && e.id < 8 && pauseKey[e.id] >= 0 ? bindName(pauseKey[e.id]) : "";
            if (k.empty()) k = " ";   // 1.8.4 (#185): a blank key keeps the label at the left like the entries that have one
            Theme::button(p, colX + 10, by, colW - 20, bh, e.label, !e.enabled ? Theme::BS_DISABLED : sel ? Theme::BS_SELECTED : Theme::BS_NORMAL, hover && !settingsOpen, k);
            if (!settingsOpen) hudHit(colX + 10, by, colW - 20, bh, HH_PAUSE, i);
        }
        int ly = 106 + ne * pitch + 4;
        if (inCombat()) Theme::text(p, colX + 12, ly, "Not during a fight.", t.dim);
        else Theme::text(p, colX + 12, ly, "Save at any save shrine.", t.dim);
        int py0 = 76 + mh + 2;
        if (H - py0 > 60) compactParty(py0, H - py0, false);
    } else if (combat) {
        // ---- the fight: round + turn order, the acting unit, the Actions list, the party ----
        Theme::panel(cv, colX, 0, colW, 62);
        Theme::title(p, colX + 12, 9, fmt("Round %d", cb.round));
        std::string who;
        if (cb.ending) who = cb.victory ? "Victory!" : cb.fled ? "Escaped!" : "Defeat...";
        else if (cb.selectedUnit >= 0 && cb.selectedUnit < (int)cb.units.size()) who = cb.units[cb.selectedUnit].ch.name + "'s turn";
        bool mine = cb.playerTurn && cb.selectedUnit >= 0;
        Theme::textRight(p, W - 12, 10, Theme::fit(p, who, colW - 90), cb.ending ? (cb.victory ? Theme::good() : Theme::foe()) : mine ? Theme::good() : Theme::foe());
        {   // the turn order as chips
            int cx = colX + 10, shown = 0, maxChips = (colW - 20) / 27;
            for (int k = 0; k < (int)cb.order.size() && shown < maxChips; k++) {
                int idx = cb.order[(cb.turnPos + k) % cb.order.size()]; auto& u = cb.units[idx]; if (!u.ch.alive) continue;
                bool fr = u.friendly(); Color8 fill{16, 18, 25, 255};
                Color8 hi = fr ? Theme::friendly() : Theme::foe(), lo = fr ? RGB(46, 90, 37) : RGB(90, 30, 26);
                if (Theme::colorblind) lo = RGB(hi.r / 3, hi.g / 3, hi.b / 3);
                Theme::bevel(cv, cx, 30, 25, 25, hi, lo, &fill);
                if (u.ch.recolor) ctx.ts->drawRecolor(cv, u.ch.tile, cx + 1, 31, 2, u.ch.color); else ctx.ts->draw(cv, u.ch.tile, cx + 1, 31, 2);
                if (k == 0) cv.rect(cx - 1, 29, 27, 27, t.accent);
                hudHit(cx, 30, 25, 25, HH_UNIT, idx);
                cx += 27; shown++;
            }
        }
        // the acting (or looked-at) unit
        int au = cb.selectedUnit >= 0 && cb.selectedUnit < (int)cb.units.size() ? cb.selectedUnit : -1;
        Theme::panel(cv, colX, 64, colW, 74);
        if (au >= 0) {
            const Character& m = cb.units[au].ch;
            Color8 wf = t.well; Theme::well(cv, colX + 10, 74, 40, 40, &wf);
            if (m.recolor) ctx.ts->drawRecolor(cv, m.tile, colX + 12, 76, 3, m.color); else ctx.ts->draw(cv, m.tile, colX + 12, 76, 3);
            Theme::text(p, colX + 58, 72, Theme::fit(p, m.name, colW - 140), t.accent);
            if (cb.units[au].partyIndex >= 0) Theme::textRight(p, W - 12, 72, classLine(m, *gd), t.dim);
            int bw = colW - 126;
            hpLabel(colX + 58, 88, "HP"); Theme::bar(cv, colX + 72, 89, bw, 4, m.hp, m.maxHP(*gd), Theme::hp()); Theme::smallRight(p, W - 12, 88, fmt("%d/%d", m.hp, m.maxHP(*gd)), t.text);
            int pipN = std::max(1, (bw + 2) / 11);
            hpLabel(colX + 58, 102, "AP"); Theme::pips(cv, colX + 72, 102, std::min(pipN, std::max(m.ap, m.maxAP(*gd))), m.ap, t.accent); Theme::smallRight(p, W - 12, 102, std::to_string(m.ap), t.text);
            hpLabel(colX + 58, 116, "MV"); Theme::pips(cv, colX + 72, 116, std::min(pipN, std::max(m.move, m.maxMove(*gd))), m.move, t.move); Theme::smallRight(p, W - 12, 116, std::to_string(m.move), t.text);
        }
        // the Actions list (the Menu button is in it; Esc still pauses)
        int ay = 140, ah = big ? 224 : std::min(200, H - ay - 4);
        Theme::panel(cv, colX, ay, colW, ah);
        Theme::title(p, colX + 12, ay + 9, "Actions");
        struct A { const char* label; int cmd; int key; };
        static const A acts[] = {{"Attack", 0, A_ATTACKMODE}, {"Aimed shot", 1, A_ATTACKMODE}, {"Spell", 2, A_SPELLS}, {"Item", 3, A_ITEMS}, {"Defend", 4, A_DEFEND}, {"Overwatch", 5, A_OVERWATCH}, {"More actions", 8, A_ACTIONMENU}, {"Flee", 7, A_FLEE}, {"End turn", 6, A_ENDTURN}, {"Menu", 9, A_CANCEL}};
        int pitch = std::max(14, std::min(19, (ah - 30) / 10)), bh = pitch - 1;
        for (int i = 0; i < 10; i++) {
            const A& a = acts[i]; int by = ay + 26 + i * pitch;
            if (by + bh > ay + ah - 4) break;
            bool on = mine && ((a.cmd == 0 && !cb.aimed && cb.mode == 0) || (a.cmd == 1 && cb.aimed) || (a.cmd == 2 && (cb.mode == 4 || cb.mode == 1)) || (a.cmd == 3 && (cb.mode == 5 || cb.mode == 2)) || (a.cmd == 8 && cb.mode == 3));
            if (mine && cb.mode == 3 && a.cmd < 8) on = cb.menuSel == a.cmd;   // walking the list with the pad (More actions)
            bool enabled = a.cmd == 9 || mine;
            std::string label = a.label; if (a.cmd == 1 && cb.aimed) label += "  (on)";
            std::string key = bindName(a.key);
            if (key.size() > 5) key = key.substr(0, 5);   // 1.8.5: "Start", "Back" whole
            if (key.empty()) key = " ";   // no key (Flee on a pad): an empty cell, so the label still lines up
            bool hover = enabled && in.mouseX >= colX + 10 && in.mouseX < colX + colW - 10 && in.mouseY >= by && in.mouseY < by + bh;
            Theme::button(p, colX + 10, by, colW - 20, bh, label, !enabled ? Theme::BS_DISABLED : on ? Theme::BS_SELECTED : Theme::BS_NORMAL, hover, "", key);
            if (a.cmd == 9) hudHit(colX + 10, by, colW - 20, bh, HH_PAUSE, -1);
            else if (enabled) hudHit(colX + 10, by, colW - 20, bh, HH_COMBAT, a.cmd);
        }
        int py0 = ay + ah + 2;
        if (H - py0 > 50) compactParty(py0, H - py0, true);
    } else {
        plaque(74, false);
        int partyH = big ? 258 : 220, pitch = big ? 56 : 48;
        partyCards(76, partyH, pitch, true);
        int oy = 76 + partyH + 2, oh = H - oy;
        if (mode == GMode::MENU) {   // #173: the Party screen's Compare panel
            Theme::panel(cv, colX, oy, colW, oh);
            Theme::title(p, colX + 12, oy + 9, "Compare");
            if (const ItemDef* d = partyPointedItem()) {
                Theme::text(p, colX + 12, oy + 30, Theme::fit(p, d->name, colW - 24), t.text);
                const Character* who = party.members.empty() ? nullptr : &party.members[clampi(menuMember, 0, n - 1)];
                Theme::YesMap ym; drawItemDetails(cv, *d, colX + 12, oy + 46, colW - 24, H - 8, who);
            } else Theme::text(p, colX + 12, oy + 30, "Pick a slot to see what fits.", t.dim);
        } else {
        // Orders
        Theme::panel(cv, colX, oy, colW, oh);
        Theme::title(p, colX + 12, oy + 9, "Orders");
        struct O { const char* label; int kind, arg; int key; };
        static const O orders[] = {{"Party", HH_ACTION, A_MENU, A_MENU}, {"Items", HH_ACTION, A_ITEMS, A_ITEMS}, {"Spells", HH_ACTION, A_SPELLS, A_SPELLS}, {"Journal", HH_ACTION, A_JOURNAL, A_JOURNAL},
                                    {"World map", HH_ACTION, A_WORLDMAP, A_WORLDMAP}, {"Craft", HH_CRAFT, 0, -1}, {"Talk / Use", HH_ACTION, A_INTERACT, A_INTERACT}, {"Menu", HH_ACTION, A_CANCEL, A_CANCEL}};
        bool twoCol = wide;
        int bw = twoCol ? (colW - 24) / 2 : colW - 20, bh = twoCol ? 24 : std::max(14, std::min(18, (oh - 34) / 8 - 1)), opitch = bh + (twoCol ? 4 : 1);
        int lastY = oy;
        for (int i = 0; i < 8; i++) {
            int bx = colX + 10 + (twoCol ? (i % 2) * (bw + 4) : 0), by = oy + 28 + (twoCol ? i / 2 : i) * opitch;
            if (by + bh > H - 4) break;
            const O& o = orders[i]; std::string key = o.key >= 0 ? bindName(o.key) : "";
            if (key.empty()) key = " ";   // 1.8.4 (#185): every label starts at the same place
            bool hover = in.mouseX >= bx && in.mouseX < bx + bw && in.mouseY >= by && in.mouseY < by + bh;
            std::string label = o.label; int state = Theme::BS_NORMAL;
            if (o.kind == HH_ACTION && o.arg == A_INTERACT) { std::string v = interactVerb(); label = v.empty() ? "Talk / Use" : v; if (v.empty()) { state = Theme::BS_DISABLED; hover = false; } }   // 1.8.5 (#197)
            Theme::button(p, bx, by, bw, bh, label, state, hover, key);
            hudHit(bx, by, bw, bh, o.kind, o.arg);
            lastY = by + bh;
        }
        int ty = lastY + 10;
        std::string verb, what = nearText(*this, verb);
        if (!what.empty() && ty + 26 < H) { Theme::text(p, colX + 12, ty, Theme::fit(p, "Near: " + what, colW - 24), t.dim); Theme::text(p, colX + 12, ty + 14, "[" + bindName(A_INTERACT) + "] " + verb, t.accent); ty += 34; }
        else ty += 4;
        if (!autosavedAt.empty() && ty + 12 < H - 4) Theme::text(p, colX + 12, ty, Theme::fit(p, "Autosaved: " + autosavedAt, colW - 24), t.dim);
        else if (map && map->saveAt(px, py) && ty + 12 < H - 4) Theme::text(p, colX + 12, ty, "[" + gKeys.label(A_QUICKSAVE) + "] Save here", t.info);
        }
    }

    // ---- bottom strip ----
    auto chronicle = [&](int x, int w, const std::vector<std::pair<std::string, Color8>>& extra) {
        Theme::panel(cv, x, y0, w, botH);
        Theme::title(p, x + 10, y0 + 7, "Chronicle");
        Color8 wf = t.well; Theme::well(cv, x + 8, y0 + 24, w - 16, botH - 32, &wf);
        int lines = std::max(1, (botH - 38) / 14);
        std::vector<std::pair<std::string, Color8>> all;
        int fromLog = lines - (int)extra.size();
        for (int i = std::max(0, (int)msgLog.size() - fromLog); i < (int)msgLog.size(); i++) all.push_back({msgLog[i], logColor(msgLog[i])});
        for (auto& e : extra) all.push_back(e);
        int ly = y0 + 29;
        for (auto& l : all) { Theme::text(p, x + 14, ly, Theme::fit(p, l.first, w - 30), l.second); ly += 14; }
    };
    // 1.8.4 (#192): the favorites (was "Belt"): 4 x 2 slots; in a fight the acting member's, in a smaller panel
    auto favorites = [&](int bx, int bw, bool small) {
        Theme::panel(cv, bx, y0, bw, botH);
        Theme::title(p, bx + 10, y0 + 7, "Favorites");
        if (gPadGlyphs && gKeys.pad[A_FAVBAR]) Theme::textRight(p, bx + bw - 10, y0 + 8, bindName(A_FAVBAR), t.dim);   // 1.8.6 (#206): the pad opens them with this
        int member = lead;
        if (combat && cb.selectedUnit >= 0 && cb.selectedUnit < (int)cb.units.size() && cb.units[cb.selectedUnit].partyIndex >= 0) member = cb.units[cb.selectedUnit].partyIndex;
        // 1.8.5 (#195): every slot shows its picture at 2x, centred under the slot's number (the fight's smaller slots
        // used to draw it at 1x in a corner, under the number)
        int sw = small ? 30 : 46, sh = small ? (big ? 34 : 33) : (big ? 40 : 32), pitchX = small ? 31 : 52, pitchY = small ? sh + 2 : (big ? 46 : 36);
        for (int i = 0; i < 8; i++) {
            int cx = bx + 10 + (i % 4) * pitchX, cy = y0 + 26 + (i / 4) * pitchY;
            Color8 wf = t.well; Theme::well(cv, cx, cy, sw, sh, &wf);
            bool hover = in.mouseX >= cx && in.mouseX < cx + sw && in.mouseY >= cy && in.mouseY < cy + sh;
            if (hover) cv.rect(cx, cy, sw, sh, t.accent);
            if (i == favLitSlot(member)) Theme::glow(cv, cx - 1, cy - 1, sw + 2, sh + 2);   // 1.8.6 (#201): being aimed (press it again to go back to moving)
            std::string k = bindName(A_FAV1 + i); if (gPadGlyphs || k.empty()) k = std::to_string(i + 1);
            Theme::small(p, cx + 3, cy + 2, k.substr(0, small ? 1 : 2), t.dim);
            if (small) beltSlot(*this, cv, member, i, cx + 3, cy + sh - 26, 2, combat);
            else beltSlot(*this, cv, member, i, cx + (sw - 24) / 2 + 2, cy + (sh - 24) / 2 + (big ? 1 : 2), 2, combat);
            hudHit(cx, cy, sw, sh, HH_BELT, i);
        }
    };
    if (combat) {
        // the target card: what the combat cursor is on
        const int favW = 138;
        int tw = std::min(370, (colX - 2 - favW - 2) / 2 + 10);
        Theme::panel(cv, 0, y0, tw, botH);
        Theme::title(p, 10, y0 + 7, "Target");
        TargetOdds o = targetOdds();
        if (o.unit >= 0) {
            auto& tu = cb.units[o.unit]; const Character& m = tu.ch;
            Theme::textRight(p, tw - 10, y0 + 8, fmt("Lv %d", m.level), t.dim);
            Color8 wf = t.well; Theme::well(cv, 10, y0 + 26, 40, 40, &wf);
            if (m.recolor) ctx.ts->drawRecolor(cv, m.tile, 12, y0 + 28, 3, m.color); else ctx.ts->draw(cv, m.tile, 12, y0 + 28, 3);
            int infoW = o.foe && tw >= 300 ? tw - 150 : tw - 70;
            Theme::text(p, 58, y0 + 26, Theme::fit(p, m.name, infoW - 6), o.foe ? Theme::foe() : Theme::good());
            hpLabel(58, y0 + 42, "HP"); int bw = std::max(30, infoW - 104); Theme::bar(cv, 72, y0 + 43, bw, 4, m.hp, m.maxHP(*gd), Theme::hp()); Theme::small(p, 78 + bw, y0 + 42, fmt("%d/%d", m.hp, m.maxHP(*gd)), t.text);
            Theme::text(p, 58, y0 + 54, fmt("AP %d  Move %d", m.ap, m.maxMove(*gd)), t.dim);
            if (o.foe && tw >= 300) {
                Color8 wf2 = t.well; int hx = tw - 138; Theme::well(cv, hx, y0 + 24, 128, 46, &wf2);
                if (o.inRange) { Theme::text(p, hx + 8, y0 + 28, fmt("%d%% to hit", o.hit), t.accent); Theme::text(p, hx + 8, y0 + 42, fmt("%d%% crit", o.crit), t.text); }
                else Theme::text(p, hx + 8, y0 + 28, fmt("out of range %d/%d", o.dist, o.range), t.dim);
                std::string note = cb.aimed ? "Aimed: +2 AP" : o.cover >= 2 ? "in full cover" : o.cover == 1 ? "in half cover" : o.flanked ? "flanked" : o.exposed ? "exposed" : "";
                if (!note.empty()) Theme::text(p, hx + 8, y0 + 56, note, o.cover ? coverColor(o.cover) : t.dim);
            }
            auto lines = tileInfoLines(cb.selX, cb.selY);
            int ly = y0 + 74, maxLines = std::max(1, (H - 6 - ly) / 14);
            for (size_t i = 2; i < lines.size() && (int)i - 2 < maxLines; i++) { Theme::text(p, 10, ly, Theme::fit(p, lines[i].first, tw - 20), lines[i].second); ly += 14; }
        } else {
            auto lines = tileInfoLines(cb.selX, cb.selY);
            int ly = y0 + 28, maxLines = std::max(1, (H - 6 - ly) / 14);
            for (size_t i = 0; i < lines.size() && (int)i < maxLines; i++) { Theme::text(p, 10, ly, Theme::fit(p, lines[i].first, tw - 20), lines[i].second); ly += 14; }
        }
        std::vector<std::pair<std::string, Color8>> extra;
        if (cb.playerTurn && cb.mode == 0) extra.push_back({"Right-drag or edges: pan.  " + bindName(A_CENTER) + ": center", t.dim});
        int cw = colX - 2 - tw - 2 - favW - 2;
        chronicle(tw + 2, cw, extra);
        favorites(tw + 2 + cw + 2, favW, true);
    } else {
        int beltW = 226, cw = colX - 2 - beltW - 2;
        chronicle(0, cw, {});
        favorites(cw + 2, beltW, false);
    }
}
