// 1.8.3 (#176-#178): the Desktop-native HUD - the game as a maximized program window on the 90s desktop.
//   The window (0,0,W,H-28): title bar ("Hollowmere - Triffall", "- Round 1 -" in a fight, "- Paused -"), a menu bar
//   (Game Party Items Spells Journal Map Craft Help; Alt + the underlined letter works too), a toolbar (the belt and
//   Talk / Party / Items / Journal / Map, or the fight's action buttons), the world in a sunken well, a status bar.
//   Below it the desktop's taskbar is the party bar: Start (= the Game menu), a button per member, the tray (weather,
//   the Messages toggle, the time). Over the world: small tool windows (Messages; in a fight Turn order and the target's
//   properties) that can be dragged by their title bar and closed. Esc opens the Game menu as a drop-down.
//   The Party tab is the "Party Properties" dialog (drawNativeParty). Other menus are the old screens inside a dialog
//   chrome (drawMenuBackdrop, hud.cpp).
#include "game.h"
#include "tiles.h"
#include <algorithm>

using namespace UI;
using Theme::Pen;
std::vector<std::string> classOrder(const GameData& gd);   // hud_party.cpp

namespace {
// the 90s desktop colours (the native theme's own, by name)
const Color8 FACE = RGB(192, 192, 192), HI = RGB(255, 255, 255), LT = RGB(223, 223, 223), SH = RGB(128, 128, 128),
             DK = RGB(0, 0, 0), NAVY = RGB(0, 0, 128), TIP = RGB(255, 255, 225), WHITE = RGB(255, 255, 255);

// ---- tool windows over the world: where they are and whether they are open (this session only, never saved) ----
enum { TW_MSG = 0, TW_MSGC, TW_TURN, TW_TARGET, TW_COUNT };   // Messages (exploring / in a fight), Turn order, target
struct ToolWin { int x = -10000, y = -10000; bool open = true; };
ToolWin gWin[TW_COUNT];
bool gMsgOpen = true;                 // Messages: one switch for both places (the tray's log icon toggles it)
int gDrag = -1, gDragDX = 0, gDragDY = 0;   // the tool window being dragged and where it was grabbed
int gTargetUnit = -1;                 // the unit the target window shows (Inspect centres on it)

// HH_TOOL hot spot args (handled by Game::nativeHudInput)
enum {
    NT_NONE = 0,                      // a tool window's body: eats the click so it never reaches the map
    NT_MENU = 1,                      // + 0..7: Game Party Items Spells Journal Map Craft Help
    NT_START = 20, NT_TRAYLOG, NT_DLG_OK, NT_DLG_CLOSE, NT_EQUIP, NT_INSPECT, NT_BESTIARY,
    NT_DRAG = 100,                    // + tool window
    NT_CLOSE = 200                    // + tool window
};
const char* MENU_NAMES[8] = {"Game", "Party", "Items", "Spells", "Journal", "Map", "Craft", "Help"};

std::string bindName(int a) { return UI::bindLabel(a); }   // 1.8.5 (#200): the pad button while a controller is in use
int letterAt(const std::string& s, char c) { for (size_t i = 0; i < s.size(); i++) if (toupper((unsigned char)s[i]) == toupper((unsigned char)c)) return (int)i; return -1; }
// the letter key bound to action a that works right now (outside fights the movement keys win), as an index into label
int boundLetter(const std::string& label, int a, bool combat) {
    if (gPadGlyphs) return -1;   // 1.8.5 (#200): on a controller the button's pad key shows at its right instead
    for (int k : {(int)gKeys.key1[a], (int)gKeys.key2[a]}) {
        if (k < K_A || k > K_Z) continue;
        if (!combat && gKeys.isDirectionKey(k)) continue;
        int i = letterAt(label, (char)('A' + (k - K_A))); if (i >= 0) return i;
    }
    return -1;
}
int pressedLetter(const InputState& in) { for (int k = K_A; k <= K_Z; k++) if (in.pressed[k]) return k; return K_NONE; }

void raised(Canvas& cv, int x, int y, int w, int h) { Theme::bevel(cv, x, y, w, h, HI, DK, &FACE); Theme::bevel(cv, x + 1, y + 1, w - 2, h - 2, LT, SH); }
void sunk(Canvas& cv, int x, int y, int w, int h, const Color8* fill) { Theme::bevel(cv, x, y, w, h, SH, HI, fill); }   // a 1 px sunken pane
void pressedBox(Canvas& cv, int x, int y, int w, int h) { Theme::bevel(cv, x, y, w, h, DK, HI, &LT); Theme::bevel(cv, x + 1, y + 1, w - 2, h - 2, SH, LT); }
void underline(const Pen& p, int x, int y, const std::string& s, int i, Color8 c) {
    if (i < 0 || i >= (int)s.size()) return;
    int ox = Theme::textW(p, s.substr(0, i)), cw = Theme::textW(p, s.substr(i, 1));
    p.cv->fillRect(x + ox, y + 11, std::max(1, cw - 1), 1, c);
}
int ulText(const Pen& p, int x, int y, const std::string& s, int i, Color8 c) { int w = Theme::text(p, x, y, s, c); underline(p, x, y, s, i, c); return w; }
int boldText(const Pen& p, int x, int y, const std::string& s, Color8 c) { Font& f = const_cast<Font&>(*p.font); bool b = f.bold; f.bold = true; int w = f.draw(*p.cv, *p.ts, x, y, s, c); f.bold = b; return w; }
int boldW(const Pen& p, const std::string& s) { Font& f = const_cast<Font&>(*p.font); bool b = f.bold; f.bold = true; int w = f.width(s); f.bold = b; return w; }
std::string fitScaled(const Font& f, const std::string& s0, int w, int scale) {
    if (f.width(s0, scale) <= w) return s0;
    std::string s = s0; while (!s.empty() && f.width(s + "..", scale) > w) s.pop_back();
    return s + "..";
}
// a push button with an underlined access letter (Theme::button centres the label the same way)
void ulButton(const Pen& p, int x, int y, int w, int h, const std::string& label, int ul, int state, bool hover, const std::string& keyRight = "") {
    Theme::button(*&p, x, y, w, h, label, state, hover, keyRight);
    int o = state == Theme::BS_SELECTED ? 1 : 0;
    int krw = keyRight.empty() ? 0 : Theme::textW(p, keyRight) + 8;   // 1.8.5: as Theme::button lays it out
    int lx = x + (w - krw - Theme::textW(p, label)) / 2 + o, ty = y + (h - 12) / 2 + o;
    if (state == Theme::BS_DEFAULT) { lx = x + 1 + (w - 2 - Theme::textW(p, label)) / 2; ty = y + 1 + (h - 2 - 12) / 2; }
    underline(p, lx, ty, label, ul, state == Theme::BS_DISABLED ? SH : DK);
}
bool over(const InputState& in, int x, int y, int w, int h) { return in.mouseX >= x && in.mouseX < x + w && in.mouseY >= y && in.mouseY < y + h; }

void drawSprite(const Ctx& ctx, Canvas& cv, const Character& m, int x, int y, int s) {
    if (!m.alive) { ctx.ts->draw(cv, TL::UI_SKULL, x, y, s); return; }
    if (m.size == 2 && s >= 2) {   // a big creature's 2x2 block, at half the scale
        int s2 = std::max(1, s / 2);
        for (int yy = 0; yy < 2; yy++) for (int xx = 0; xx < 2; xx++) { Tile t = tileAt(m.tile, xx, yy);
            if (m.recolor) ctx.ts->drawRecolor(cv, t, x + xx * 12 * s2, y + yy * 12 * s2, s2, m.color); else ctx.ts->draw(cv, t, x + xx * 12 * s2, y + yy * 12 * s2, s2); }
        return;
    }
    if (m.recolor) ctx.ts->drawRecolor(cv, m.tile, x, y, s, m.color); else ctx.ts->draw(cv, m.tile, x, y, s);
}
std::string classLine(const Character& m, const GameData& gd) {
    const ClassDef* c = gd.cls(m.classId);
    std::string n = c ? c->name : m.classId; if (!n.empty()) n[0] = (char)toupper((unsigned char)n[0]);
    int r = m.rank(m.classId); return n + " " + std::to_string(r > 0 ? r : m.level);
}
// what a piece of gear gives, short (the equipment list's Gives column)
std::string givesText(const ItemDef& d) {
    std::vector<std::string> g;
    if (d.dmgMax > 0) { std::string ty = dmgTypeName(d.dmgType); if (ty.size() > 4) ty = ty.substr(0, 4); g.push_back(fmt("%d-%d %s", d.dmgMin, d.dmgMax, ty.c_str())); }
    if (d.def) g.push_back(fmt("Defense %+d", d.def));
    if (d.evasion) g.push_back(fmt("Evasion %+d", d.evasion));
    for (int s = 0; s < ST_COUNT; s++) if (d.bonus[s]) g.push_back(fmt("%s %+d", statName(s), d.bonus[s]));
    if (d.bonusHP) g.push_back(fmt("HP %+d", d.bonusHP));
    if (d.bonusMP) g.push_back(fmt("MP %+d", d.bonusMP));
    if (d.bonusAP) g.push_back(fmt("AP %+d", d.bonusAP));
    if (d.bonusMove) g.push_back(fmt("Move %+d", d.bonusMove));
    for (int t = 0; t < DT_COUNT; t++) if (d.res[t]) { std::string ty = dmgTypeName(t); if (!ty.empty()) ty[0] = (char)toupper((unsigned char)ty[0]); g.push_back(fmt("%s res %d%%", ty.c_str(), d.res[t])); }
    if (d.light) g.push_back(fmt("Light %d", d.light));
    std::string s; for (auto& x : g) s += (s.empty() ? "" : ", ") + x;
    return s;
}
}   // namespace

bool nativePartyFits(const Rect& a) { return a.w >= 620 && SCREEN_H >= 470; }

// ---------------- input: the menu bar, Alt keys, tool windows (called from Game::update before the mode's update) ----------------
void Game::nativeHudInput(InputState& in) {
    if (!Theme::native()) { gDrag = -1; return; }
    const bool combat = mode == GMode::COMBAT || inCombat();
    // a tool window follows the pointer while its title bar is held
    if (gDrag >= 0) { if (in.mouseDown[0]) { gWin[gDrag].x = in.mouseX - gDragDX; gWin[gDrag].y = in.mouseY - gDragDY; } else gDrag = -1; }

    auto menuCmd = [&](int i) {   // a menu bar entry (0 Game ... 7 Help)
        if (i == 0) { if (mode == GMode::PAUSE) popMode(); else if (mode == GMode::EXPLORE || mode == GMode::COMBAT) openPause(); return; }
        if (mode == GMode::PAUSE) popMode();   // another entry closes the Game menu first
        if (mode != GMode::EXPLORE && mode != GMode::COMBAT) return;
        const bool fight = mode == GMode::COMBAT;
        switch (i) {
            case 1: if (fight) log("Not during a fight."); else injectAction = A_MENU; break;
            case 2: injectAction = A_ITEMS; break;          // in a fight: the item list
            case 3: injectAction = A_SPELLS; break;         // in a fight: the spell list
            case 4: jTab = 0; jSel = 0; jScroll = 0; pushMode(GMode::JOURNAL); break;
            case 5: if (fight) log("Not during a fight."); else injectAction = A_WORLDMAP; break;
            case 6: if (fight) log("Not during a fight."); else openCraft("any"); break;
            case 7: jTab = 6; jSel = 0; jScroll = 0; pushMode(GMode::JOURNAL); break;   // Help = the Journal's Tactics page
        }
    };

    // ---- keys: Alt + an underlined letter (menu bar, the Party dialog), plain letters in the open Game menu ----
    int letter = pressedLetter(in);
    if (letter != K_NONE) {
        const char ch = (char)('A' + (letter - K_A));
        const bool alt = in.down[K_ALT];
        if (alt && (mode == GMode::EXPLORE || mode == GMode::COMBAT || mode == GMode::PAUSE)) {
            for (int i = 0; i < 8; i++) if (MENU_NAMES[i][0] == ch) { in.eatKeys(); menuCmd(i); return; }
            if (mode == GMode::COMBAT && gTargetUnit >= 0 && gTargetUnit < (int)cb.units.size()) {
                if (ch == 'N') { in.eatKeys(); camFree = true; camCenterOn(cb.units[gTargetUnit].x, cb.units[gTargetUnit].y); return; }
                if (ch == 'B') { in.eatKeys(); jTab = 2; jSel = 0; jScroll = 0; pushMode(GMode::JOURNAL); return; }
            }
        }
        if (alt && mode == GMode::MENU && menuPrompt != "respec") {
            static const char TABL[4] = {'P', 'I', 'S', 'Q'};
            for (int i = 0; i < 4; i++) if (TABL[i] == ch) { in.eatKeys(); menuTab = i; menuSel = 0; spellCol = 0; menuScroll = 0; menuSubFocus = false; menuPrompt.clear(); return; }
            if (menuTab == 0 && themedParty()) {
                if (ch == 'C') { in.eatKeys(); menuPrompt.clear(); popMode(); return; }
                if (ch == 'E') { in.eatKeys(); menuSubFocus = true; menuSub = clampi(menuSub, 0, SL_COUNT - 1); menuPrompt = "equip"; menuSel = 0; menuScroll = 0; return; }
                if (ch == 'L') { in.eatKeys(); if (party.members[menuMember].alive) { leader = menuMember; notify(party.members[leader].name + " takes the lead."); } return; }
            }
        }
        if (!alt && mode == GMode::PAUSE) {   // the Game menu's underlined letters pick that entry
            static const struct { char c; int id; } PK[] = {{'R', 0}, {'P', 1}, {'L', 2}, {'M', 3}, {'J', 4}, {'E', 5}, {'B', 7}, {'X', 6}};
            auto entries = pauseEntries();
            for (auto& k : PK) if (k.c == ch) for (size_t i = 0; i < entries.size(); i++) if (entries[i].id == k.id) {
                in.pressed[letter] = false; pauseSel = (int)i; in.pressed[K_ENTER] = true; return;
            }
        }
    }

    if (!in.mousePressed[0]) return;
    // a dialog's close box (the old screens inside the native dialog chrome; the Party dialog has its own hot spot)
    if ((mode == GMode::MENU || mode == GMode::JOURNAL) && !themedParty()) {
        Rect a = menuArea();
        if (a.w >= BASE_W + 8 && over(in, a.x + a.w - 21, a.y + 5, 16, 14)) { in.eatMouse(); menuPrompt.clear(); popMode(); return; }
    }
    const HudHit* h = hudHitAt(in.mouseX, in.mouseY);
    if (!h || h->kind != HH_TOOL) return;
    in.eatMouse();
    const int a = h->arg;
    if (a >= NT_DRAG && a < NT_DRAG + TW_COUNT) { gDrag = a - NT_DRAG; gDragDX = in.mouseX - gWin[gDrag].x; gDragDY = in.mouseY - gWin[gDrag].y; return; }
    if (a >= NT_CLOSE && a < NT_CLOSE + TW_COUNT) { int w = a - NT_CLOSE; if (w == TW_MSG || w == TW_MSGC) gMsgOpen = false; else gWin[w].open = false; return; }
    if (a >= NT_MENU && a < NT_MENU + 8) { menuCmd(a - NT_MENU); return; }
    switch (a) {
        case NT_START: menuCmd(0); break;
        case NT_TRAYLOG: gMsgOpen = !gMsgOpen; break;
        case NT_DLG_OK: case NT_DLG_CLOSE: if (mode == GMode::MENU) { menuPrompt.clear(); popMode(); } break;
        case NT_EQUIP: if (mode == GMode::MENU && menuTab == 0) { menuSubFocus = true; menuSub = clampi(menuSub, 0, SL_COUNT - 1); menuPrompt = "equip"; menuSel = 0; menuScroll = 0; } break;
        case NT_INSPECT: if (combat && gTargetUnit >= 0 && gTargetUnit < (int)cb.units.size()) { camFree = true; camCenterOn(cb.units[gTargetUnit].x, cb.units[gTargetUnit].y); } break;
        case NT_BESTIARY: if (mode == GMode::COMBAT || mode == GMode::EXPLORE) { jTab = 2; jSel = 0; jScroll = 0; pushMode(GMode::JOURNAL); } break;
        default: break;
    }
    (void)combat;
}

// ---------------- the window ----------------
void Game::drawNativeHud(Canvas& cv, InputState& in) {
    Theme::NoMap nm; const UiTheme& t = Theme::th(); Pen p{&cv, ctx.font, ctx.ts};
    const int W = SCREEN_W, H = SCREEN_H, TB = H - 28;   // the taskbar's top; the window is (0, 0, W, TB)
    const bool combat = mode == GMode::COMBAT || inCombat();
    const bool paused = mode == GMode::PAUSE || std::find(modeStack.begin(), modeStack.end(), GMode::PAUSE) != modeStack.end();
    const bool dialog = subViewMode(mode) || mode == GMode::SETTINGS || mode == GMode::SAVELOAD;   // a dialog owns the window: it goes inactive
    const bool live = mode == GMode::EXPLORE || mode == GMode::COMBAT || mode == GMode::PAUSE;      // the window's controls take clicks
    const bool myTurn = mode == GMode::COMBAT && cb.playerTurn && !autoCombat && cb.selectedUnit >= 0 && !cb.ending;
    const int n = (int)party.members.size(), lead = activeLeader();
    if (!combat) { gWin[TW_TURN].open = gWin[TW_TARGET].open = true; gTargetUnit = -1; }   // closed fight windows come back next fight
    { static int lastW = 0, lastH = 0;   // a new screen size puts the tool windows back in their default spots
      if (lastW != W || lastH != H) { for (auto& w : gWin) w.x = w.y = -10000; lastW = W; lastH = H; } }

    // ---- the frame and the title bar (the inside is already the face grey: Game::draw cleared the screen to it) ----
    Theme::bevel(cv, 0, 0, W, TB, LT, DK); Theme::bevel(cv, 1, 1, W - 2, TB - 2, HI, SH);
    {
        std::string title = (map ? map->name : std::string("Triffall")) + (paused ? " - Paused" : combat ? fmt(" - Round %d", std::max(1, cb.round)) : "") + " - Triffall";
        const int tx = 3, ty = 3, tw = W - 6, th = 18;
        Color8 a = !dialog ? NAVY : SH, b = !dialog ? RGB(16, 132, 208) : FACE;
        for (int i = 0; i < tw; i++) { int k = tw > 1 ? i * 256 / (tw - 1) : 0; cv.fillRect(tx + i, ty, 1, th, RGB((uint8_t)(a.r + (b.r - a.r) * k / 256), (uint8_t)(a.g + (b.g - a.g) * k / 256), (uint8_t)(a.b + (b.b - a.b) * k / 256))); }
        boldText(p, tx + 5, ty + 3, Theme::fit(p, title, tw - 70), !dialog ? WHITE : RGB(212, 208, 200));
        int bx = W - 21, by = 5;   // close, maximize (restore look: the window is maximized), minimize
        raised(cv, bx, by, 16, 14); for (int i = 0; i < 6; i++) { cv.put(bx + 4 + i, by + 3 + i, DK); cv.put(bx + 5 + i, by + 3 + i, DK); cv.put(bx + 9 - i, by + 3 + i, DK); cv.put(bx + 10 - i, by + 3 + i, DK); }
        raised(cv, bx - 18, by, 16, 14); cv.rect(bx - 18 + 6, by + 3, 6, 5, DK); cv.fillRect(bx - 18 + 6, by + 3, 6, 1, DK); cv.fillRect(bx - 18 + 4, by + 6, 6, 5, FACE); cv.rect(bx - 18 + 4, by + 6, 6, 5, DK); cv.fillRect(bx - 18 + 4, by + 6, 6, 1, DK);
        raised(cv, bx - 34, by, 16, 14); cv.fillRect(bx - 34 + 4, by + 9, 6, 2, DK);
        uiRect(0, 0, W, 72);
        hudHit(0, 0, W, 21, HH_TOOL, NT_NONE);
    }

    // ---- the menu bar ----
    {
        int mx = 9, my = 24;
        for (int i = 0; i < 8; i++) {
            std::string s = MENU_NAMES[i]; int w = Theme::textW(p, s);
            bool open = i == 0 && mode == GMode::PAUSE;
            bool hover = live && over(in, mx - 5, 22, w + 10, 17);
            if (open) Theme::bevel(cv, mx - 5, 22, w + 10, 17, SH, HI);
            else if (hover) Theme::bevel(cv, mx - 5, 22, w + 10, 17, HI, SH);
            ulText(p, mx + (open ? 1 : 0), my + (open ? 1 : 0), s, 0, dialog ? SH : DK);
            if (live) { if (i == 0 && mode == GMode::COMBAT) hudHit(mx - 5, 22, w + 10, 17, HH_PAUSE, -1);   // in a fight Game pauses it (the fight's own Menu hot spot)
                        else hudHit(mx - 5, 22, w + 10, 17, HH_TOOL, NT_MENU + i); }
            mx += w + 16;
        }
        cv.fillRect(3, 39, W - 6, 1, SH); cv.fillRect(3, 40, W - 6, 1, HI);
    }

    // ---- the toolbar ----
    const int TY = 43, TH = 26;
    // the favorites (1.8.4 #192: in a fight too, the acting member's): 8 toolbar buttons; returns the separator's x
    auto favorites = [&](int member, bool fight, bool clickable) -> int {
        for (int i = 0; i < 8; i++) {
            int sx = 8 + i * 30; bool hover = clickable && over(in, sx, TY, 28, TH);
            if (fight && i == favLitSlot(member)) pressedBox(cv, sx, TY, 28, TH); else raised(cv, sx, TY, 28, TH);   // 1.8.6 (#201): held down while it is being aimed
            if (hover) { for (int xx = sx + 3; xx < sx + 25; xx += 2) { cv.put(xx, TY + 2, SH); cv.put(xx, TY + TH - 3, SH); } }
            std::string k = bindName(A_FAV1 + i); Theme::small(p, sx + 3, TY + 2, k.empty() ? std::to_string(i + 1) : k.substr(0, 2), SH);
            if (member >= 0 && member < n) {
                const std::string& f = party.members[member].favs[i];
                if (!f.empty()) {
                    const Character& m = fight ? [&]() -> const Character& { for (auto& u : cb.units) if (u.partyIndex == member) return u.ch; return party.members[member]; }() : party.members[member];
                    std::string kind = f.substr(0, f.find(':')), id = f.substr(f.find(':') + 1);
                    bool ok = true; int cnt = -1;
                    if (kind == "spell") { if (auto* s = gd->spell(id)) { ctx.ts->draw(cv, s->tile, sx + 8, TY + 6, 1); ok = m.mp >= s->cost && (fight || spellUsableOutside(*s)); } }
                    else if (auto* d = gd->item(id)) { drawItemTile(*ctx.ts, cv, *d, sx + 8, TY + 6, 1); cnt = party.countItem(id); ok = cnt > 0; }
                    if (!ok) Theme::dither(cv, sx + 8, TY + 6, 12, 12, FACE);
                    if (cnt > 1) Theme::smallRight(p, sx + 26, TY + 17, "x" + std::to_string(std::min(cnt, 99)), DK);
                }
            }
            if (clickable) hudHit(sx, TY, 28, TH, HH_BELT, i);
        }
        int sepX = 8 + 8 * 30 + 4;
        cv.fillRect(sepX, TY + 1, 1, 24, SH); cv.fillRect(sepX + 1, TY + 1, 1, 24, HI);
        return sepX;
    };
    if (!combat) {
        int sepX = favorites(lead, false, live);
        std::string gold = fmt("%d gold", party.gold);
        int goldW = Theme::textW(p, gold);
        Theme::textRight(p, W - 10, TY + 7, gold, DK);
        struct B { const char* label; int kind, arg, act; };
        static const B btns[] = {{"Talk", HH_ACTION, A_INTERACT, A_INTERACT}, {"Party", HH_ACTION, A_MENU, A_MENU}, {"Items", HH_ACTION, A_ITEMS, A_ITEMS}, {"Journal", HH_ACTION, A_JOURNAL, A_JOURNAL}, {"Map", HH_ACTION, A_WORLDMAP, A_WORLDMAP}};
        int bx0 = sepX + 8, room = W - 10 - goldW - 10 - bx0;
        int bw = std::min(74, room / 5 - 4);
        for (int i = 0; i < 5 && bw >= 36; i++) {
            const B& b = btns[i]; int bx = bx0 + i * (bw + 4);
            // 1.8.5 (#197): the first button says what Interact would do here ("Talk", "Open", "Read"...) and greys
            // out when there is nothing in reach
            std::string label = b.label; bool can = true;
            if (b.act == A_INTERACT) { std::string v = interactVerb(); if (v.empty()) can = false; else label = v; }
            int ul = can ? boundLetter(label, b.act, false) : -1;
            std::string kr; if (ul < 0) { kr = UI::bindLabel(b.act); if (kr.size() > 3 || bw - Theme::textW(p, label) < 2 * (Theme::textW(p, kr) + 8)) kr.clear(); }
            bool hover = live && can && over(in, bx, TY, bw, TH);
            ulButton(p, bx, TY, bw, TH, label, ul, can ? Theme::BS_NORMAL : Theme::BS_DISABLED, hover, kr);
            if (live && can) hudHit(bx, TY, bw, TH, b.kind, b.arg);
        }
    } else {
        struct A { const char* label; int cmd, act; };
        static const A acts[] = {{"Attack", 0, A_ATTACKMODE}, {"Aimed", 1, A_ATTACKMODE}, {"Spell", 2, A_SPELLS}, {"Item", 3, A_ITEMS}, {"Defend", 4, A_DEFEND},
                                 {"Overwatch", 5, A_OVERWATCH}, {"Actions", 8, A_ACTIONMENU}, {"End turn", 6, A_ENDTURN}, {"Flee", 7, A_FLEE}};
        std::string info;
        int au = cb.selectedUnit >= 0 && cb.selectedUnit < (int)cb.units.size() ? cb.selectedUnit : -1;
        if (au >= 0 && cb.units[au].friendly()) { const Character& m = cb.units[au].ch; info = fmt("%s  AP %d  MV %d", m.name.c_str(), m.ap, m.move); }
        int infoW = info.empty() ? 0 : Theme::textW(p, info) + 16;
        // widths: label + padding (+ a small key at the right when the label has no letter of its key); shrink to fit
        int pad = 20; bool hints = true;
        auto widthOf = [&](const A& a, int pd, bool kh) { int w = Theme::textW(p, a.label) + pd; if (kh && boundLetter(a.label, a.act, true) < 0) { std::string k = UI::bindLabel(a.act); if (k.size() <= 3) w += Theme::textW(p, k) + 8; } return w; };
        auto total = [&](int pd, bool kh) { int s = 0; for (auto& a : acts) s += widthOf(a, pd, kh) + 3; return s; };
        // 1.8.4 (#192): the acting member's favorites first when the buttons still fit after them (a narrow window keeps
        // the buttons only; the 1-8 keys work either way)
        const int favEnd = 8 + 8 * 30 + 12;
        bool favs = favEnd + total(12, false) <= W - 8;
        int x0 = favs ? favEnd : 8;
        if (x0 + total(pad, hints) > W - 8 - infoW) hints = false;
        if (x0 + total(pad, hints) > W - 8 - infoW) pad = 12;
        if (x0 + total(pad, hints) > W - 8 - infoW) info.clear();   // it is in the status bar's first pane too
        if (favs) { int member = au >= 0 && cb.units[au].partyIndex >= 0 ? cb.units[au].partyIndex : lead; favorites(member, true, myTurn); }
        int x = x0;
        for (auto& a : acts) {
            int w = widthOf(a, pad, hints);
            bool on = myTurn && ((a.cmd == 1 && cb.aimed) || (a.cmd == 2 && (cb.mode == 4 || cb.mode == 1)) || (a.cmd == 3 && (cb.mode == 5 || cb.mode == 2)) || (a.cmd == 8 && cb.mode == 3));
            int ul = boundLetter(a.label, a.act, true);
            std::string kr; if (hints && ul < 0) { kr = UI::bindLabel(a.act); if (kr.size() > 3) kr.clear(); }
            bool hover = myTurn && over(in, x, TY, w, TH);
            int bxl = kr.empty() ? x : x - (Theme::textW(p, kr) + 6) / 2;   // keep the label centred on the label's part
            (void)bxl;
            ulButton(p, x, TY, w, TH, a.label, ul, !myTurn ? Theme::BS_DISABLED : on ? Theme::BS_SELECTED : Theme::BS_NORMAL, hover, kr);
            if (myTurn) hudHit(x, TY, w, TH, HH_COMBAT, a.cmd);
            x += w + 3;
        }
        if (!info.empty()) Theme::textRight(p, W - 10, TY + 7, info, DK);
    }

    // ---- the world's well ----
    Theme::bevel(cv, VIEW_X - 2, VIEW_Y - 2, VIEW_W + 4, VIEW_H + 4, SH, HI);
    Theme::bevel(cv, VIEW_X - 1, VIEW_Y - 1, VIEW_W + 2, VIEW_H + 2, DK, LT);

    // ---- the status bar ----
    {
        const int sy = VIEW_Y + VIEW_H + 3;
        std::vector<std::pair<std::string, int>> panes;
        if (combat) {
            std::string who;
            if (cb.ending) who = cb.victory ? "Victory!" : cb.fled ? "Escaped!" : "Defeat...";
            else if (cb.selectedUnit >= 0 && cb.selectedUnit < (int)cb.units.size()) who = cb.units[cb.selectedUnit].ch.name + "'s turn";
            std::string odds;
            TargetOdds o = targetOdds();
            if (o.unit >= 0) {
                const std::string& nm2 = cb.units[o.unit].ch.name;
                if (!o.foe) odds = nm2;
                else if (o.inRange) odds = fmt("%s: %d%% to hit, %d%% crit%s", nm2.c_str(), o.hit, o.crit, cb.aimed ? " (aimed)" : "");
                else odds = fmt("%s: out of range (%d/%d)", nm2.c_str(), o.dist, o.range);
            }
            panes = {{fmt("Round %d - ", std::max(1, cb.round)) + who, 220}, {odds, 380}, {"Right-drag pans, " + UI::bindLabel(A_CENTER) + " centers", 350}};
        } else {
            std::string w = weather; if (!w.empty()) w[0] = (char)toupper((unsigned char)w[0]);
            panes = {{map ? map->name : "", 200}, {clockText() + (outdoors() ? "  " + w : ""), 240}, {fmt("%d gold", party.gold), 160}, {UI::bindLabel(A_MENU) + " Menu   " + UI::bindLabel(A_INTERACT) + " Act", 350}};
        }
        int sum = 0; for (auto& pn : panes) sum += pn.second;
        int sx = 4, avail = W - 10;
        for (size_t i = 0; i < panes.size(); i++) {
            int w = i + 1 == panes.size() ? W - 4 - sx : panes[i].second * avail / sum;
            Color8 f = FACE; sunk(cv, sx, sy, w - 2, 16, &f);
            Theme::text(p, sx + 5, sy + 2, Theme::fit(p, panes[i].first, w - 12), DK);
            sx += w;
        }
        uiRect(0, sy - 2, W, TB - sy + 2);
    }

    // ---- the taskbar: Start, the party, the tray ----
    {
        cv.fillRect(0, TB, W, 28, FACE); cv.fillRect(0, TB, W, 1, LT); cv.fillRect(0, TB + 1, W, 1, HI);
        uiRect(0, TB, W, 28);
        const int by = TB + 4, bh = 21;
        bool startOn = mode == GMode::PAUSE;
        if (startOn) pressedBox(cv, 3, by, 60, bh); else raised(cv, 3, by, 60, bh);
        int so = startOn ? 1 : 0;
        cv.fillRect(9 + so, by + 5 + so, 9, 10, RGB(122, 63, 176)); cv.fillRect(11 + so, by + 7 + so, 5, 6, RGB(232, 197, 71));
        boldText(p, 22 + so, by + 4 + so, "Start", DK);
        if (live) hudHit(3, by, 60, bh, HH_TOOL, NT_START);
        const int trayW = W >= 900 ? 148 : 100, trayX = W - trayW - 4;
        // the members
        int avail = trayX - 68 - 6, bw = n > 0 ? std::min(164, avail / n - 4) : 164;
        const bool compact = bw < 140;
        for (int i = 0; i < n && i < 6; i++) {
            const Character& m = (combat ? [&]() -> const Character& { for (auto& u : cb.units) if (u.partyIndex == i) return u.ch; return party.members[i]; }() : party.members[i]);
            int ui = -1; if (combat) for (size_t k = 0; k < cb.units.size(); k++) if (cb.units[k].partyIndex == i) ui = (int)k;
            bool on = combat ? (ui >= 0 && ui == cb.selectedUnit && cb.playerTurn) : i == lead;
            int x = 68 + i * (bw + 4), o = on ? 1 : 0;
            if (on) pressedBox(cv, x, by, bw, bh); else raised(cv, x, by, bw, bh);
            drawSprite(ctx, cv, m, x + 4 + o, by + 4 + o, 1);
            auto tbar = [&](int bx, int yy, int w, int v, int mx, Color8 c) { cv.fillRect(bx, yy, w, 5, SH); if (mx > 0) cv.fillRect(bx, yy, w * clampi(v, 0, mx) / mx, 5, c); };
            int mhp = m.maxHP(*gd), mmp = m.maxMP(*gd);
            if (!compact) {
                std::string nm2 = Theme::fit(p, m.name, bw - 120);
                if (on) boldText(p, x + 20 + o, by + 4 + o, nm2, DK); else Theme::text(p, x + 20, by + 4, nm2, m.alive ? DK : SH);
                int barX = x + bw - 94 + o;
                tbar(barX, by + 4 + o, 56, m.hp, mhp, Theme::hp()); tbar(barX, by + 11 + o, 56, m.mp, mmp, Theme::mp());
                std::string a1 = combat ? fmt("AP%d", m.ap) : std::to_string(m.hp), a2 = combat ? fmt("MV%d", m.move) : std::to_string(m.mp);
                Theme::smallRight(p, x + bw - 4 + o, by + 3 + o, a1, DK); Theme::smallRight(p, x + bw - 4 + o, by + 10 + o, a2, DK);
            } else {
                int barW = std::max(12, bw - 26);
                tbar(x + 20 + o, by + 4 + o, barW, m.hp, mhp, Theme::hp()); tbar(x + 20 + o, by + 11 + o, barW, m.mp, mmp, Theme::mp());
            }
            if (live) { if (combat && ui >= 0) hudHit(x, by, bw, bh, HH_UNIT, ui); else if (!combat) hudHit(x, by, bw, bh, HH_MEMBER, i); }
        }
        // the tray: the weather (or a fight's red light), the Messages toggle, the time
        Color8 tf = FACE; sunk(cv, trayX, by, trayW, bh, &tf);
        int ix = trayX + 8, iy = by + 5;
        if (combat) cv.fillRect(ix, iy, 10, 10, Theme::hp());
        else if (outdoors()) {
            bool night = nightAmount() > 0.5f;
            if (weather == "clear") { if (night) { cv.fillRect(ix + 2, iy, 6, 10, RGB(232, 228, 190)); cv.fillRect(ix + 5, iy + 1, 4, 8, FACE); } else { cv.fillRect(ix, iy + 1, 10, 8, RGB(232, 197, 71)); cv.fillRect(ix + 2, iy - 1, 6, 12, RGB(232, 197, 71)); } }
            else if (weather == "snow") { for (int k = 0; k < 5; k++) cv.fillRect(ix + (k * 7) % 10, iy + (k * 3) % 10, 2, 2, WHITE); cv.fillRect(ix, iy, 10, 1, SH); }
            else if (weather == "fog") { for (int k = 0; k < 4; k++) cv.fillRect(ix + (k & 1), iy + 1 + k * 3, 9, 1, SH); }
            else { cv.fillRect(ix, iy + 1, 10, 5, weather == "storm" ? RGB(80, 80, 96) : SH); cv.fillRect(ix + 2, iy, 6, 1, SH);
                   if (weather == "storm") { cv.fillRect(ix + 5, iy + 6, 2, 2, RGB(232, 197, 71)); cv.fillRect(ix + 4, iy + 8, 2, 2, RGB(232, 197, 71)); }
                   else for (int k = 0; k < 3; k++) cv.fillRect(ix + 1 + k * 3, iy + 7, 1, 3, RGB(0, 0, 192)); }
        }
        {   // the Messages toggle: a little log sheet, pressed in while the window is open
            int lx = trayX + 24, ly = by + 3;
            if (gMsgOpen) Theme::bevel(cv, lx - 2, ly - 1, 15, 17, SH, HI);
            cv.fillRect(lx + 1, ly + 1, 9, 12, WHITE); cv.rect(lx + 1, ly + 1, 9, 12, DK);
            for (int k = 0; k < 4; k++) cv.fillRect(lx + 3, ly + 3 + k * 2 + (k > 1), 5, 1, k == 3 ? NAVY : SH);
            if (live) hudHit(lx - 2, ly - 1, 15, 17, HH_TOOL, NT_TRAYLOG);
        }
        std::string tm = combat ? fmt("Round %d", std::max(1, cb.round)) : fmt("%02d:%02d", clockMin / 60, clockMin % 60);
        Theme::textRight(p, trayX + trayW - 6, by + 4, tm, DK);
    }

    // ---- tool windows over the world ----
    auto toolWin = [&](int id, int w, int h, int defX, int defY, const std::string& title) -> Rect {
        ToolWin& tw = gWin[id];
        if (tw.x < -9000) { tw.x = defX; tw.y = defY; }
        tw.x = clampi(tw.x, VIEW_X - 2, std::max(VIEW_X - 2, VIEW_X + VIEW_W + 2 - w)); tw.y = clampi(tw.y, VIEW_Y - 2, std::max(VIEW_Y - 2, VIEW_Y + VIEW_H + 2 - h));
        Rect r = Theme::toolWindow(p, tw.x, tw.y, w, h, title, !dialog);
        uiRect(tw.x, tw.y, w, h);
        if (live) { hudHit(tw.x, tw.y, w, h, HH_TOOL, NT_NONE); hudHit(tw.x + 3, tw.y + 3, w - 26, 14, HH_TOOL, NT_DRAG + id); hudHit(tw.x + w - 21, tw.y + 4, 16, 12, HH_TOOL, NT_CLOSE + id); }
        return r;
    };
    // Messages: the log, newest at the bottom
    if (gMsgOpen && (mode != GMode::TRANSITION)) {
        const int ts = textScale(), lh = 12 * ts + 2;
        int w = std::min(combat ? 300 : 320, VIEW_W / 2 - 8), h = combat ? 22 + 8 + 6 * lh : 22 + 8 + 8 * lh;
        h = std::min(h, VIEW_H / 2);
        int id = combat ? TW_MSGC : TW_MSG;
        Rect r = toolWin(id, w, h, combat ? VIEW_X + VIEW_W - w - 4 : VIEW_X + 4, VIEW_Y + VIEW_H - h - 6, "Messages");
        Color8 wf = WHITE; Theme::well(cv, r.x + 2, r.y + 1, r.w - 4, r.h - 3, &wf);
        int lines = std::max(1, (r.h - 10) / lh), ly = r.y + 5;
        for (int i = std::max(0, (int)msgLog.size() - lines); i < (int)msgLog.size(); i++) {
            Theme::text(p, r.x + 7, ly, fitScaled(*ctx.font, msgLog[i], r.w - 16, ts), logColor(msgLog[i]), ts); ly += lh;
        }
    }
    if (combat && mode != GMode::TRANSITION) {
        // Turn order: a docked list, the unit whose turn it is first (navy, "now"), foes in red
        if (gWin[TW_TURN].open && !cb.order.empty()) {
            std::vector<int> rows;
            for (int k = 0; k < (int)cb.order.size(); k++) { int idx = cb.order[(cb.turnPos + k) % cb.order.size()]; if (idx >= 0 && idx < (int)cb.units.size() && cb.units[idx].ch.alive) rows.push_back(idx); }
            const int pitch = 20; int maxRows = std::max(1, (VIEW_H / 2 - 30) / pitch);
            int nr = std::min((int)rows.size(), std::min(8, maxRows));
            int w = 150, h = 22 + 8 + nr * pitch;
            Rect r = toolWin(TW_TURN, w, h, VIEW_X + VIEW_W - w - 4, VIEW_Y + 4, "Turn order");
            Color8 wf = WHITE; Theme::well(cv, r.x + 2, r.y + 1, r.w - 4, r.h - 3, &wf);
            for (int k = 0; k < nr; k++) {
                const Combatant& u = cb.units[rows[k]]; int ry = r.y + 4 + k * pitch;
                bool now = k == 0 && !cb.ending;
                if (now) cv.fillRect(r.x + 4, ry, r.w - 8, pitch - 1, NAVY);
                drawSprite(ctx, cv, u.ch, r.x + 6, ry + 3, 1);
                Color8 c = now ? WHITE : u.friendly() ? DK : Theme::foe();
                Theme::text(p, r.x + 22, ry + 3, Theme::fit(p, u.ch.name, r.w - 52), c);
                if (now) Theme::smallRight(p, r.x + r.w - 7, ry + 6, "now", WHITE);
                if (live) hudHit(r.x + 4, ry, r.w - 8, pitch - 1, HH_UNIT, rows[k]);
            }
        }
        // the target's properties: what the combat cursor is on
        if (gWin[TW_TARGET].open && cb.selectedUnit >= 0 && map && map->inBounds(cb.selX, cb.selY)) {
            TargetOdds o = targetOdds();
            auto lines = tileInfoLines(cb.selX, cb.selY);
            int w = std::min(300, VIEW_W / 2 - 8), h = 154;
            gTargetUnit = o.unit;
            std::string title = o.unit >= 0 ? cb.units[o.unit].ch.name : "Ground";
            Rect r = toolWin(TW_TARGET, w, h, VIEW_X + 4, VIEW_Y + VIEW_H - h - 6, title);
            int gy = r.y + 4;   // where the group box starts
            if (o.unit >= 0) {
                const Character& m = cb.units[o.unit].ch;
                Color8 black = DK; Theme::well(cv, r.x + 5, r.y + 3, 44, 44, &black);
                drawSprite(ctx, cv, m, r.x + 9, r.y + 7, 3);
                int ix = r.x + 57;
                Theme::text(p, ix, r.y + 3, fmt("Level %d   HP %d/%d", m.level, m.hp, m.maxHP(*gd)), DK);
                Theme::bar(cv, ix + 2, r.y + 21, std::max(40, std::min(120, r.w - 160)), 4, m.hp, m.maxHP(*gd), Theme::hp());
                Theme::text(p, ix, r.y + 31, fmt("AP %d   Move %d", m.ap, m.maxMove(*gd)), DK);
                if (o.foe && o.inRange) Theme::text(p, ix, r.y + 45 - 0, "", DK);
                int bw = 90, bx = r.x + r.w - bw - 5;
                bool hov1 = live && over(in, bx, r.y + 1, bw, 22), hov2 = live && over(in, bx, r.y + 27, bw, 22);
                ulButton(p, bx, r.y + 1, bw, 22, "Inspect", 1, live ? Theme::BS_NORMAL : Theme::BS_DISABLED, hov1);
                if (live) hudHit(bx, r.y + 1, bw, 22, HH_TOOL, NT_INSPECT);
                if (o.foe) { ulButton(p, bx, r.y + 27, bw, 22, "Bestiary", 0, live ? Theme::BS_NORMAL : Theme::BS_DISABLED, hov2); if (live) hudHit(bx, r.y + 27, bw, 22, HH_TOOL, NT_BESTIARY); }
                gy = r.y + 52;
            }
            int gh = r.y + r.h - 4 - gy;
            Theme::panelTitled(p, r.x + 5, gy, r.w - 10, gh, o.unit < 0 ? "Terrain" : o.foe ? "Attack" : "Status");
            size_t first = o.unit >= 0 ? 2 : 0;
            int ly = gy + 15, maxY = gy + gh - 14;
            Theme::YesMap ym;   // the info lines carry the old fixed colours: map them onto the grey
            for (size_t i = first; i < lines.size() && ly <= maxY; i++) { Theme::text(p, r.x + 13, ly, Theme::fit(p, lines[i].first, r.w - 26), lines[i].second); ly += 14; }
        }
    }

    // ---- the Game menu (Esc): a drop-down under "Game" ----
    if (mode == GMode::PAUSE) {
        auto entries = pauseEntries();
        static const struct { int id; const char* label; int ul; int key; } LOOK[] = {
            {0, "Resume", 0, A_CANCEL}, {1, "Party & Items", 0, A_MENU}, {2, "Load Game...", 0, -1}, {3, "World Map", 6, A_WORLDMAP},
            {4, "Journal", 0, A_JOURNAL}, {5, "Settings...", 1, -1}, {7, "Back to Editor", 0, -1}, {6, "Exit to Desktop", 1, -1}};
        const int rowH = 18, ne = (int)entries.size();
        int mw = 0;
        for (auto& e : entries) for (auto& l : LOOK) if (l.id == e.id) { std::string k = l.key >= 0 ? UI::bindLabel(l.key) : ""; mw = std::max(mw, Theme::textW(p, l.label) + (k.empty() ? 0 : Theme::textW(p, k) + 24)); }
        mw = std::max(180, mw + 46);
        int mh = 6 + ne * rowH + 8;   // + the separator before Exit
        int mx = 4, my = 39;
        Theme::bevel(cv, mx, my, mw, mh, LT, DK, &FACE); Theme::bevel(cv, mx + 1, my + 1, mw - 2, mh - 2, HI, SH);
        uiRect(mx, my, mw, mh);
        hudHit(mx, my, mw, mh, HH_TOOL, NT_NONE);
        int ry = my + 3;
        for (int i = 0; i < ne; i++) {
            const PauseEntry& e = entries[i];
            if (e.id == 6) { cv.fillRect(mx + 3, ry + 3, mw - 6, 1, SH); cv.fillRect(mx + 3, ry + 4, mw - 6, 1, HI); ry += 8; }
            const char* label = e.label; int ul = -1; std::string key;
            for (auto& l : LOOK) if (l.id == e.id) { label = l.label; ul = l.ul; if (l.key >= 0) key = UI::bindLabel(l.key); }
            bool sel = i == pauseSel;
            if (sel) cv.fillRect(mx + 3, ry, mw - 6, rowH, NAVY);
            Color8 c = sel ? WHITE : DK;
            if (!e.enabled) { if (!sel) { ulText(p, mx + 21, ry + 4, label, ul, HI); } c = SH; ulText(p, mx + 20, ry + 3, label, ul, c); }
            else ulText(p, mx + 20, ry + 3, label, ul, c);
            if (!key.empty()) Theme::textRight(p, mx + mw - 12, ry + 3, key, e.enabled ? c : SH);
            hudHit(mx + 3, ry, mw - 6, rowH, HH_PAUSE, i);
            ry += rowH;
        }
    }
    (void)t;
}

// 1.8.5 (#199): the Party Properties tabs and the sheet under them, on every Party tab (the Party tab's sheet stops
// above its OK / Close buttons); returns where the content starts
int nativeMenuTabs(Game& g, Canvas& cv, InputState& in, const Rect& c) {
    (void)in; Theme::NoMap nm; Pen p{&cv, g.ctx.font, g.ctx.ts};
    static const char* TABN[] = {"Party", "Items", "Spells", "Quests"};
    const int dx = c.x, dy = c.y, dw = c.w, dh = c.h;
    const bool picking = g.menuPrompt == "equip" || g.menuPrompt == "respec";
    int tx = dx + 8; const int ty0 = dy + 26; int openX = tx, openW = 0;
    for (int i = 0; i < 4; i++) {
        int w = Theme::textW(p, TABN[i]) + 22; bool on = g.menuTab == i;
        int y = ty0 - (on ? 2 : 0), h = 20 + (on ? 2 : 0);
        cv.fillRect(tx, y, w, h, FACE); cv.fillRect(tx, y, w, 1, HI); cv.fillRect(tx, y, 1, h, HI); cv.fillRect(tx + w - 1, y, 1, h, DK); cv.fillRect(tx + w - 2, y + 1, 1, h - 1, SH);
        ulText(p, tx + 11, y + 5, TABN[i], gPadGlyphs ? -1 : 0, DK);
        if (!picking) g.hudHit(tx, y, w, h, Game::HH_PTAB, i);
        if (on) { openX = tx; openW = w; }
        tx += w;
    }
    if (gPadGlyphs) Theme::text(p, tx + 8, ty0 + 5, UI::bindLabel(A_PAGEUP) + " / " + UI::bindLabel(A_PAGEDOWN), SH);
    // the sheet under them
    int sx = dx + 8, sy = dy + 46, sw = dw - 16, sh = g.themedParty() ? dh - 90 : dh - 54;
    cv.fillRect(sx, sy, sw, 1, HI); cv.fillRect(sx, sy, 1, sh, HI); cv.fillRect(sx, sy + sh - 1, sw, 1, DK); cv.fillRect(sx + sw - 1, sy, 1, sh, DK);
    cv.fillRect(sx + 1, sy + sh - 2, sw - 2, 1, SH); cv.fillRect(sx + sw - 2, sy + 1, 1, sh - 2, SH);
    if (openW) cv.fillRect(openX + 1, sy, openW - 3, 1, FACE);   // the open tab joins the sheet
    Theme::textRight(p, dx + dw - 16, dy + 30, fmt("%d gold", g.party.gold), DK);
    return dy + 48;
}

// ---------------- #177 the Party Properties dialog ----------------
void Game::drawNativeParty(Canvas& cv, InputState& in) {
    Theme::NoMap nm; Pen p{&cv, ctx.font, ctx.ts};
    const int W = SCREEN_W, H = SCREEN_H;
    const Rect host = menuChromeRect();   // 1.8.5 (#199): the same dialog as every other Party tab and menu
    const int dw = host.w, dh = host.h, dx = host.x, dy = host.y;
    const bool tall = dh >= 456;
    cv.dim(VIEW_X, VIEW_Y, VIEW_W, VIEW_H, 115);   // the game window behind goes inactive, the world dims
    uiRect(0, 0, W, H - 28);
    const int nmem = (int)party.members.size();
    const int mi = clampi(menuMember, 0, nmem - 1);
    const Character& m = party.members[mi];
    const bool picking = menuPrompt == "equip";

    Theme::windowChrome(p, dx, dy, dw, dh, "Party Properties", true, true);
    hudHit(dx, dy, dw, dh, HH_TOOL, NT_NONE);
    hudHit(dx + dw - 21, dy + 5, 16, 14, HH_TOOL, NT_DLG_CLOSE);

    nativeMenuTabs(*this, cv, in, host);   // the tabs and the property sheet

    // ---- the members: a large-icon list ----
    {
        int lx = dx + 16, ly = dy + 56, lw = 120, lh = dh - 110;
        Color8 wf = WHITE; Theme::well(cv, lx, ly, lw, lh, &wf);
        int pitch = std::min(84, (lh - 8) / std::max(4, nmem));
        for (int i = 0; i < nmem; i++) {
            const Character& c = party.members[i]; int y = ly + 8 + i * pitch; bool sel = i == mi;
            if (sel && !menuSubFocus) cv.fillRect(lx + 12, y, 96, pitch - 10, RGB(224, 224, 240));
            int box = std::min(48, pitch - 30);
            int bx = lx + 36 + (48 - box) / 2;
            cv.fillRect(bx, y + 4, box, box, sel ? RGB(0, 0, 64) : DK);
            drawSprite(ctx, cv, c, bx + (box - (box >= 42 ? 36 : 24)) / 2, y + 4 + (box - (box >= 42 ? 36 : 24)) / 2, box >= 42 ? 3 : 2);
            std::string nm2 = Theme::fit(p, c.name, 90); int nw = Theme::textW(p, nm2);
            int labY = y + 4 + box + 2;
            if (sel) cv.fillRect(lx + 60 - nw / 2 - 4, labY, nw + 8, 14, menuSubFocus ? SH : NAVY);
            Theme::text(p, lx + 60 - nw / 2, labY + 1, nm2, sel ? WHITE : c.alive ? DK : SH);
            if (i == leader) Theme::small(p, lx + 60 - Theme::smallW(p, "leader") / 2, labY + 15, "leader", sel ? NAVY : SH);
            if (!picking) hudHit(lx + 4, y, lw - 8, pitch - 2, HH_MEMBER, i);
        }
    }

    // ---- the member's group, attributes ----
    const int gx = dx + 146, gw = dw - 146 - 16;
    const int cw = (gw - 10) * 240 / 488, aw = gw - 10 - cw;
    {
        Theme::panelTitled(p, gx, dy + 52, cw, 128, m.name);
        std::string sub = classLine(m, *gd) + (leader == mi ? " - leads the party" : "");
        Theme::text(p, gx + 10, dy + 70, Theme::fit(p, sub, cw - 20), DK);
        int bw = cw - 100;
        auto row = [&](int y, const char* l, int v, int mx, Color8 c) { Theme::text(p, gx + 10, y, l, DK); Theme::bar(cv, gx + 32, y + 3, bw - 4, 4, v, mx, c); Theme::text(p, gx + 36 + bw, y, fmt("%d/%d", v, mx), DK); };
        row(dy + 86, "XP", m.xp, m.xpToNext(), NAVY);
        row(dy + 104, "HP", m.hp, m.maxHP(*gd), Theme::hp());
        row(dy + 120, "MP", m.mp, m.maxMP(*gd), Theme::mp());
        Theme::text(p, gx + 10, dy + 140, fmt("AP %d   Move %d   Range %d", m.maxAP(*gd), m.maxMove(*gd), m.attackRange(*gd)), DK);
        int lo, hi, ty; m.attackDamage(*gd, lo, hi, ty);
        Theme::text(p, gx + 10, dy + 156, fmt("Damage %d-%d %s", lo, hi, dmgTypeName(ty)), DK);
        if (m.wounds > 0) Theme::textRight(p, gx + cw - 10, dy + 156, fmt("Wounded x%d", m.wounds), Theme::foe());

        int ax = gx + cw + 10;
        Theme::panelTitled(p, ax, dy + 52, aw, 128, "Attributes");
        int hoverStat = -1, colW = (aw - 24) / 2;
        for (int s = 0; s < ST_COUNT && s < 6; s++) {
            int x = ax + 12 + (s % 2) * colW, y = dy + 70 + (s / 2) * 20;
            int v = m.stat(s, *gd), b = m.baseStats[s];
            Theme::text(p, x, y + 2, statName(s), DK);
            Color8 ff = WHITE; Theme::well(cv, x + 30, y, 40, 17, &ff);
            Theme::textRight(p, x + 64, y + 3, std::to_string(v), v > b ? Theme::good() : v < b ? Theme::foe() : DK);
            if (over(in, x - 2, y - 1, colW - 4, 19)) hoverStat = s;
        }
        Theme::text(p, ax + 12, dy + 136, fmt("Defense %d   Evasion %d", m.defense(*gd), m.evasion(*gd)), DK);
        std::string res; for (int d = 1; d < DT_COUNT; d++) { int r = m.resist(d, *gd); if (r) res += std::string(res.empty() ? "" : ", ") + dmgTypeName(d) + fmt(" %d%%", r); }
        Theme::text(p, ax + 12, dy + 152, Theme::fit(p, "Resist: " + (res.empty() ? std::string("none") : res), aw - 24), res.empty() ? SH : DK);
        if (hoverStat >= 0 && !picking) {
            std::vector<std::pair<std::string, Color8>> tip = {{std::string(statName(hoverStat)), DK}, {statHelp(hoverStat), DK}};
            Theme::tooltip(p, in.mouseX + 12, in.mouseY + 16, tip, true);
        }
    }

    // ---- equipment: a details list (Slot | Item | Gives) ----
    const int eqY = dy + 188, rowH = tall ? 20 : 18, eqH = 34 + SL_COUNT * rowH + 12;
    {
        Theme::panelTitled(p, gx, eqY, gw, eqH, "Equipment");
        int lx = gx + 10, ly = eqY + 16, lw = gw - 20, lh = 18 + SL_COUNT * rowH + 6;
        Color8 wf = WHITE; Theme::well(cv, lx, ly, lw, lh, &wf);
        int c1 = 120 * (lw - 4) / 474, c2 = 220 * (lw - 4) / 474, c3 = lw - 4 - c1 - c2;
        int hx = lx + 2, hy = ly + 2;
        raised(cv, hx, hy, c1, 16); Theme::text(p, hx + 4, hy + 2, "Slot", DK);
        raised(cv, hx + c1, hy, c2, 16); Theme::text(p, hx + c1 + 4, hy + 2, "Item", DK);
        raised(cv, hx + c1 + c2, hy, c3, 16); Theme::text(p, hx + c1 + c2 + 4, hy + 2, "Gives", DK);
        for (int s = 0; s < SL_COUNT; s++) {
            int y = hy + 18 + s * rowH; bool sel = menuSub == s;
            bool focus = sel && menuSubFocus;
            if (sel) cv.fillRect(hx, y, lw - 4, rowH - 1, focus ? NAVY : FACE);
            Color8 c = focus ? WHITE : DK;
            int ty = y + (rowH - 12) / 2;
            Theme::text(p, hx + 4, ty, slotName(s), c);
            const ItemDef* it = m.equip[s].empty() ? nullptr : gd->item(m.equip[s]);
            if (it) {
                drawItemTile(*ctx.ts, cv, *it, hx + c1 + 4, y + (rowH - 12) / 2, 1);
                Theme::text(p, hx + c1 + 20, ty, Theme::fit(p, it->name, c2 - 24), c);
                std::string warn = gearWarning(*it, m);
                Theme::text(p, hx + c1 + c2 + 4, ty, Theme::fit(p, warn.empty() ? givesText(*it) : "poor fit", c3 - 8), warn.empty() ? c : (focus ? RGB(255, 200, 200) : Theme::foe()));
            } else Theme::text(p, hx + c1 + 20, ty, "(empty)", focus ? WHITE : SH);
            if (!picking) hudHit(hx, y, lw - 4, rowH - 1, HH_SLOT, s);
        }
    }

    // ---- class ranks ----
    {
        int cy = eqY + eqH + 6, chh = std::max(34, dy + dh - 50 - cy);
        bool can = m.rankPoints > 0 && !levelUpQueue.empty();
        Theme::panelTitled(p, gx, cy, gw, chh, "Class ranks", m.rankPoints > 0 ? fmt("%d point%s free", m.rankPoints, m.rankPoints == 1 ? "" : "s") : "");
        auto cls = classOrder(*gd);
        int room = gw - 24 - (can ? 96 : 0), pitch = std::max(60, room / std::max(1, std::min(5, (int)cls.size())));
        int ry = cy + std::max(15, chh / 2 - 4);
        for (size_t i = 0; i < cls.size() && i < 5; i++) {
            const ClassDef* cd = gd->cls(cls[i]); std::string nm2 = cd ? cd->name : cls[i];
            if (!nm2.empty()) nm2[0] = (char)toupper((unsigned char)nm2[0]);
            int r = m.rank(cls[i]);
            Theme::text(p, gx + 12 + (int)i * pitch, ry, Theme::fit(p, nm2 + " " + std::to_string(r), pitch - 6), r > 0 ? DK : SH);
        }
        if (can) { int bx = gx + gw - 100; bool hov = !picking && over(in, bx, ry - 4, 88, 20); ulButton(p, bx, ry - 4, 88, 20, "Rank up...", 0, Theme::BS_NORMAL, hov); if (!picking) hudHit(bx, ry - 4, 88, 20, HH_RANKUP, 0); }
    }

    // ---- the buttons ----
    {
        int by = dy + dh - 36;
        bool canLead = leader != mi && m.alive;
        ulButton(p, dx + 16, by, 120, 24, "Lead the party", 0, canLead ? Theme::BS_NORMAL : Theme::BS_DISABLED, !picking && canLead && over(in, dx + 16, by, 120, 24));
        if (!picking && canLead) hudHit(dx + 16, by, 120, 24, HH_LEAD, 0);
        int ex = dx + dw - 290, ox = dx + dw - 198, cx = dx + dw - 104;
        ulButton(p, ex, by, 86, 24, "Equip...", 0, Theme::BS_NORMAL, !picking && over(in, ex, by, 86, 24));
        ulButton(p, ox, by, 86, 24, "OK", -1, Theme::BS_DEFAULT, !picking && over(in, ox, by, 86, 24));
        ulButton(p, cx, by, 86, 24, "Close", 0, Theme::BS_NORMAL, !picking && over(in, cx, by, 86, 24));
        if (!picking) { hudHit(ex, by, 86, 24, HH_TOOL, NT_EQUIP); hudHit(ox, by, 86, 24, HH_TOOL, NT_DLG_OK); hudHit(cx, by, 86, 24, HH_TOOL, NT_DLG_CLOSE); }
    }

    // ---- the equip picker: a small dialog over the sheet (the list, and what the pointed item does) ----
    if (picking) {
        auto cands = equipCandidates(menuSub);
        int nrows = (int)cands.size() + 1, vis = std::min(nrows, 12);
        int pw = std::min(dw - 40, 500), ph = std::max(30 + vis * 16 + 16, 200);
        int px = dx + (dw - pw) / 2, py = dy + 70;
        Rect r = Theme::windowChrome(p, px, py, pw, ph, std::string("Equip ") + slotName(menuSub) + " - " + m.name, true, false);
        hudHit(px, py, pw, ph, HH_TOOL, NT_NONE);
        int listW = 210;
        Color8 wf = WHITE; Theme::well(cv, r.x + 6, r.y + 6, listW, std::max(vis * 16 + 6, r.h - 32), &wf);
        int first = clampi(menuSel - vis + 1, 0, std::max(0, nrows - vis));
        for (int k = 0; k < vis; k++) {
            int i = first + k, ry = r.y + 9 + k * 16; bool sel = i == menuSel;
            if (sel) cv.fillRect(r.x + 8, ry, listW - 4, 16, NAVY);
            Color8 c = sel ? WHITE : DK;
            if (i == 0) Theme::text(p, r.x + 13, ry + 2, "(remove)", sel ? WHITE : SH);
            else if (const ItemDef* d = gd->item(cands[i - 1])) { drawItemTile(*ctx.ts, cv, *d, r.x + 11, ry + 2, 1); Theme::text(p, r.x + 27, ry + 2, Theme::fit(p, d->name, listW - 26), c); }
            hudHit(r.x + 8, ry, listW - 4, 16, HH_PICK, i);
        }
        Theme::scrollbar(cv, r.x + 6 + listW - 14, r.y + 8, 12, vis * 16 + 2, first, vis, nrows);
        int ix = r.x + listW + 16, iw = r.x + r.w - 8 - ix;
        Theme::panelTitled(p, ix, r.y + 2, iw, r.h - 8, "Details");
        const ItemDef* d = partyPointedItem();
        if (d && menuSel > 0) {
            Theme::text(p, ix + 10, r.y + 18, Theme::fit(p, d->name, iw - 20), DK);
            Theme::YesMap ym; drawItemDetails(cv, *d, ix + 10, r.y + 34, iw - 20, r.y + r.h - 10, &m);
        } else Theme::text(p, ix + 10, r.y + 18, menuSel == 0 ? "Take it off." : "Nothing to show.", SH);
        int hy = r.y + r.h - 18;
        Theme::text(p, r.x + 8, hy - 2, fmt("%s pick   %s equip   %s back", UI::keyText("Up/Down").c_str(), UI::bindLabel(A_CONFIRM).c_str(), UI::keyText("Esc").c_str()), SH);
    }
}

// ---------------- #176 pickups and other notifications: yellow balloons pointing at the tray ----------------
void Game::drawNativeNotes(Canvas& cv) {
    if (notifications.empty()) return;
    Theme::NoMap nm; Pen p{&cv, ctx.font, ctx.ts};
    const int W = SCREEN_W;
    int bottom = VIEW_Y + VIEW_H - 8 - (int)achPopups.size() * 48;   // above the achievement balloons
    for (size_t i = 0; i < notifications.size() && i < 3; i++) {
        const std::string& s = notifications[i];
        int w = std::min(W - 40, Theme::textW(p, s) + 20), h = 22;
        int x = W - w - 12, y = bottom - h - (int)i * (h + 6);
        if (y < VIEW_Y + 4) break;
        cv.fillRect(x, y, w, h, TIP); cv.rect(x, y, w, h, DK);
        if (i == 0) for (int k = 0; k < 6; k++) { cv.fillRect(x + w - 30 + k, y + h - 1 + k, 6 - k, 1, TIP); cv.put(x + w - 30 + k, y + h - 1 + k, DK); cv.put(x + w - 24, y + h - 1 + k, DK); }
        Theme::text(p, x + 10, y + 5, Theme::fit(p, s, w - 20), DK);
    }
}
