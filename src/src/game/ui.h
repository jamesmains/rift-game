// Shared drawing helpers for the in-game (RPG) UI style.
#pragma once
#include "../core/gfx.h"
#include "../core/input.h"
#include "data.h"
#include <string>
#include <vector>

struct Ctx {
    Tileset* ts = nullptr;
    Font* font = nullptr;
    GameData* gd = nullptr;
    Canvas* cv = nullptr;
    int frame = 0;
    float time = 0;
};

namespace UI {
// 1.2.2 (#39): the screen's size in canvas pixels follows the monitor / window shape (App::setScreen), so the game
// fills a wide screen. 480 high (or a little more for crisp whole-number scaling), 640 wide or more. BASE_W x BASE_H is
// the classic 4:3 size the menu screens are laid out for (Game::subViewMode draws those centred).
constexpr int BASE_W = 640, BASE_H = 480;
inline int SCREEN_W = BASE_W, SCREEN_H = BASE_H;
constexpr int TILE = 24; // 12px tiles drawn at 2x in the world view
// 1.8.3 (#171 #176 #180): where the world is drawn on the screen this frame. Each UI theme frames it its own way
// (Game::computeLayout); the world is drawn into a canvas of this size (world coordinates start at 0,0 there) and put
// at VIEW_X, VIEW_Y. Mouse positions over the world are screen minus VIEW_X / VIEW_Y.
inline int VIEW_X = 0, VIEW_Y = 0, VIEW_W = BASE_W, VIEW_H = BASE_H - 72;

void panel(Ctx& c, int x, int y, int w, int h, Color8 fill = C::Panel, Color8 border = C::Gold);
void panelTitle(Ctx& c, int x, int y, int w, int h, const std::string& title);
void bar(Ctx& c, int x, int y, int w, int h, int value, int maxValue, Color8 fill, Color8 back = C::DkGray);
void text(Ctx& c, int x, int y, const std::string& s, Color8 col = C::White, int scale = 1);
void textCenter(Ctx& c, int cx, int y, const std::string& s, Color8 col = C::White, int scale = 1);
void textRight(Ctx& c, int rx, int y, const std::string& s, Color8 col = C::White, int scale = 1);
void textShadow(Ctx& c, int x, int y, const std::string& s, Color8 col = C::White, int scale = 1);
int  wrapped(Ctx& c, int x, int y, int w, const std::string& s, Color8 col = C::White);
// A selectable list row; returns true if hovered by mouse.
bool listRow(Ctx& c, const InputState& in, int x, int y, int w, int h, bool selected);
void selCursor(Ctx& c, int x, int y, int h);   // pulsing marker to the left of the focused row (clear focus for keyboard/controller users)
bool button(Ctx& c, const InputState& in, int x, int y, int w, int h, const std::string& label, bool enabled = true, bool selected = false);
// 1.8.4 (#189): a row of tabs in the theme's own look (native: property-sheet tabs on a white base line from lineX0 to
// lineX1; frame: brass / slate bevels; one-bit: the chosen tab inverted). Each tab is {x, w, label}.
struct TabSpec { int x, w; std::string label; };
void tabRow(Ctx& c, const std::vector<TabSpec>& tabs, int y, int h, int sel, int lineX0 = -1, int lineX1 = -1);
// 1.8.4 (#194): the notice board's colours. Frame keeps the cork board and paper notices; native and one-bit draw it
// as their own panel with white (native) or black (one-bit) cards.
struct BoardLook {
    bool cork = true;                              // frame: the cork board, pins on the notices
    Color8 head, sub, note, ink, body, soft, good, bad, warn, reward, card, cardSel, cardShut, edge, edgeSel;
    Color8 selText;                                // native: every line of the chosen card in this (on its navy fill)
};
BoardLook boardLook();
void boardPanel(Ctx& c, int x, int y, int w, int h);                          // the board's background
void boardCard(Ctx& c, int x, int y, int w, int h, bool sel, bool open);      // one notice
Color8 boardInk(const BoardLook& L, bool sel, Color8 c);                      // a card's text colour (the chosen native card: white)
// a filter chip (the item category bar): chosen = pressed in (native), inverted (one-bit), brass (frame)
void chip(Ctx& c, int x, int y, int w, int h, const std::string& label, bool on);
void icon(Ctx& c, Tile t, int x, int y, int scale = 1, Color8 tint = C::White);
int keyHint(Ctx& c, int x, int y, const std::string& key, const std::string& label);
// The key (or controller button) bound to an action, drawn as a keyHint chip; returns its width (0 = unbound).
// draw=false only measures it.
int keyChip(Ctx& c, int x, int y, int action, bool draw = true);
Color8 dmgColor(int dmgType);
// Side / hit-chance / cover colours. With gColorblind (Settings > Access) they come from a palette that stays apart
// for red-green colour blindness (blue for your side and good odds, orange for foes and bad odds).
extern bool gColorblind;
Color8 friendColor(); Color8 foeColor(); Color8 allyColor();
Color8 friendBack(); Color8 foeBack();
Color8 hitColor(int pct);            // good / fair / poor odds
Color8 coverColor(int level);        // 1 half, 2 full
std::string itemLabel(const ItemDef& d, int count);
}
