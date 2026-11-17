// 1.8.3 (#170): UI themes. Settings > General > Display "Theme" picks one of three looks for the desktop and the game:
//   frame   - Adventurer's Frame (the default): a carved slate frame round the world, brass for selection and headings
//   native  - Desktop-native: the game is a program window on the 90s desktop, grey controls, navy selection
//   onebit  - One-Bit Desktop: black windows with paper frames and striped title bars, a lantern-amber focus glow
// The old navy/gold look is gone; there is no "classic" theme. The Editor keeps its own UI and is never themed.
//
// Every screen draws with the primitives below (panel, well, button, checkbox, slider, choice group, tabs, keycap, list
// row, window chrome, tool window, scrollbar, title text, bars, pips, tooltip). Meaning colours (HP, MP, foes, good news)
// come from the theme and still swap to the colour-blind palette when Settings > Access asks for it.
//
// Older screens that were written with fixed colours (C::Gold, C::Panel, white text...) are brought into the theme by the
// UI colour map (Theme::beginUiMap / endUiMap): while it is on, Canvas::fillRect / rect and Font::draw pass their colour
// through the theme's map. The game turns it on for the HUD and its menu screens, never for the world.
#pragma once
#include "gfx.h"
#include "input.h"
#include <string>
#include <vector>

enum class ThemeId : int { Frame = 0, Native = 1, OneBit = 2, Count = 3 };

struct UiTheme {
    ThemeId id = ThemeId::Frame;
    const char* key = "frame";        // settings.ini theme=
    const char* name = "Adventurer's Frame";
    // surfaces
    Color8 back;                      // behind everything (the frame's canvas, a native window face, one-bit ink)
    Color8 panel, hi, lo, hi2, lo2;   // panel face, its outer bevel light / shadow, inner bevel light / shadow
    Color8 well;                      // an inset well's fill
    Color8 face;                      // a button's face
    Color8 rowSel;                    // a chosen list row / card fill
    Color8 selFill, selHi, selLo;     // a selected (brass / navy / amber) button: fill, light and dark edge
    Color8 keyCell;                   // the small key cell on a command button
    // text
    Color8 text, dim, faint, selText, accent, accentHi, title;
    // meaning colours (colour-blind swaps are done by the functions below)
    Color8 hp, mp, move, good, foeC, achieve, gold, barBack, pipOff, info;
    Color8 tooltip, tooltipText;
};

namespace Theme {
const UiTheme& th();                  // the active theme
ThemeId id();
void set(ThemeId t);
ThemeId fromKey(const std::string& key);   // unknown / empty -> Frame
const char* key(ThemeId t);
const char* name(ThemeId t);
inline bool frame() { return id() == ThemeId::Frame; }
inline bool native() { return id() == ThemeId::Native; }
inline bool onebit() { return id() == ThemeId::OneBit; }

extern bool colorblind;               // mirrors Settings > Access (UI::gColorblind)
extern bool reduceFlashing;
extern float time;                    // seconds, for the few things that pulse (one-bit glow, waiting cells)
Color8 hp(); Color8 mp(); Color8 good(); Color8 foe(); Color8 friendly();

// ---- UI colour map for older fixed-colour screens ----
void beginUiMap();                    // fillRect / rect / Font::draw colours go through the theme's map until endUiMap
void endUiMap();
bool uiMapOn();
Color8 mapFill(Color8 c);
Color8 mapText(Color8 c);
struct NoMap { bool was; NoMap(); ~NoMap(); };   // primitives draw their own exact colours
struct YesMap { Color8 (*was)(Color8, int, int, int); YesMap(); ~YesMap(); };   // an older drawing inside a primitive-drawn panel
void beginFrame();                    // forgets last frame's inverted boxes
// one-bit desktop windows drawn with the 90s grey palette (My Computer, About, dialogs...): grey faces and white wells
// become black, black text paper, navy amber
void beginDeskMap();
void endDeskMap();
void invert(Rect r);                  // native / onebit: mapped text starting in r takes the selection's text colour (a navy row's white)

// ---- drawing context ----
struct Pen { Canvas* cv = nullptr; const Font* font = nullptr; const Tileset* ts = nullptr; };

// text faces: body (Spleen 6x12), small (Spleen 5x8), title (the tileset's Classic glyphs; frame: bold brass with a
// shadow, onebit: regular paper, native: bold body text)
int text(const Pen& p, int x, int y, const std::string& s, Color8 col, int scale = 1);
int textW(const Pen& p, const std::string& s, int scale = 1);
int small(const Pen& p, int x, int y, const std::string& s, Color8 col, int scale = 1);
int smallW(const Pen& p, const std::string& s, int scale = 1);
int title(const Pen& p, int x, int y, const std::string& s, int scale = 1, Color8* col = nullptr);
int titleW(const Pen& p, const std::string& s, int scale = 1);
int textRight(const Pen& p, int rx, int y, const std::string& s, Color8 col);
int smallRight(const Pen& p, int rx, int y, const std::string& s, Color8 col);
std::string fit(const Pen& p, const std::string& s, int w);   // cut with ".." to fit

// ---- primitives ----
void bevel(Canvas& cv, int x, int y, int w, int h, Color8 hi, Color8 lo, const Color8* fill = nullptr);
void frameRect(Canvas& cv, int x, int y, int w, int h, Color8 c);
void panel(Canvas& cv, int x, int y, int w, int h);                 // carved panel / group face / black window body
void panelTitled(const Pen& p, int x, int y, int w, int h, const std::string& t, const std::string& right = "");
void well(Canvas& cv, int x, int y, int w, int h, const Color8* fill = nullptr);
void rule(Canvas& cv, int x, int y, int w);                         // etched (frame / native) or dotted (onebit) rule
void glow(Canvas& cv, int x, int y, int w, int h);                  // one-bit amber focus ring (others: an accent outline)
enum ButtonState { BS_NORMAL = 0, BS_SELECTED = 1, BS_DISABLED = 2, BS_DEFAULT = 3 };
// a button with an optional key label at the right (keyRight) or a key cell at the left (keyLeft)
void button(const Pen& p, int x, int y, int w, int h, const std::string& label, int state, bool hover = false,
            const std::string& keyRight = "", const std::string& keyLeft = "", const std::string& sub = "");
void checkbox(const Pen& p, int x, int y, bool on, const std::string& label, bool focused = false, bool enabled = true);
void slider(const Pen& p, int x, int y, int w, int value, int lo, int hi, bool focused = false);   // track + knob; value text at the right
int choiceW(const Pen& p, const std::vector<std::string>& opts);
void choice(const Pen& p, int x, int y, const std::vector<std::string>& opts, int sel, bool focused = false, int hover = -1);
int choiceHit(const Pen& p, int x, int y, const std::vector<std::string>& opts, int mx, int my);   // -1 none
void tabs(const Pen& p, int x, int y, int w, int h, const std::vector<std::string>& names, int sel, int gap = 6, const std::vector<std::string>& keys = {});
int tabHit(int x, int y, int w, int h, int n, int gap, int mx, int my);
int keycap(const Pen& p, int x, int y, const std::string& k, bool draw = true);   // returns its width
enum RowState { RS_IDLE = 0, RS_SELECTED = 1, RS_WAITING = 2, RS_HOVER = 3 };
void listRow(const Pen& p, int x, int y, int w, int h, int state);
Rect windowChrome(const Pen& p, int x, int y, int w, int h, const std::string& title, bool active = true, bool closeBox = true);
Rect toolWindow(const Pen& p, int x, int y, int w, int h, const std::string& title, bool active = true);
void scrollbar(Canvas& cv, int x, int y, int w, int h, int first, int visible, int total);
void bar(Canvas& cv, int x, int y, int w, int h, int value, int maxValue, Color8 fill);
void pips(Canvas& cv, int x, int y, int n, int on, Color8 onC, int pw = 9, int ph = 7, int gap = 2);
void tooltip(const Pen& p, int x, int y, const std::vector<std::pair<std::string, Color8>>& lines, bool boldFirst = false);
void dither(Canvas& cv, int x, int y, int w, int h, Color8 c, int step = 2);   // every other pixel
}
