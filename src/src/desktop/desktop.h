// The fake Windows 98 desktop that serves as the main menu.
#pragma once
#include <map>
#include <array>
#include <string>
#include <vector>
#include <functional>
#include <array>
#include "../core/gfx.h"
#include "../core/input.h"
#include "../game/game.h"

struct IconDef { std::string label; int ic, ir; std::string action; int dc = -1, dr = -1; };   // dc,dr: the default icon (ic,ir may be the player's pick)

struct DeskWindow {
    std::string type, title;
    int x, y, w, h;
    bool wantClose = false;
    int sel = 0;          // generic selection index
    int sub = 0;
    std::string text;     // text field content
    std::string status;   // status message inside window
    bool dragging = false; int dragDX = 0, dragDY = 0;
    int page = 0;
    std::function<void()> onYes;
    float timer = 0;
    std::string campaign = "triffall";   // New Game window: which campaign starts
    int focusX = -1, focusY = -1;        // controller: move the cursor here on the next frame (a window just opened, a slot was picked)
    bool focusFirst = true;              // controller: put the cursor on the window's first control once it has been drawn
    bool oskShift = true;                // New Game on-screen keyboard: capitals
};

// toy state (toys.cpp)
struct MineState { int w = 0, h = 0, mines = 0, diff = 0, flags = 0; std::vector<uint8_t> cell, state; bool started = false, dead = false, won = false; float time = 0, lastT = 0; };
struct SolState { std::vector<int> tab[7], found[4], stock, waste; int faceUp[7] = {}; int sel = -1, selPile = -1, moves = 0; bool won = false, init = false; float dblT = 0; int dblPile = -1, dblIdx = -1; };


class Desktop {
public:
    Ctx ctx;
    Image8 icons;
    Game* game = nullptr;
    GameSettings* settings = nullptr;
    PlatformRequests* req = nullptr;
    std::string saveDir;
    std::vector<IconDef> iconDefs;
    std::vector<std::array<int, 2>> iconPositions;
    std::vector<DeskWindow> windows;
    int wallpaper = 0;
    bool contextOpen = false; int contextX = 0, contextY = 0, contextSel = 0;
    bool startOpen = false; int startBtnW = 70; int startSel = 0; bool padOnStart = false;   // start menu highlight (separate from the desktop icon selection)
    std::vector<Rect> sliderRects, sliderRectsPrev;   // sliders drawn last frame: left/right adjusts them instead of hopping away
    float sliderRepeat = 0;
    int hoverIcon = -1, selIcon = -1;
    float dblTimer = 0; int dblIcon = -1;
    float clockT = 0;
    bool launchGame = false;      // request: switch to game screen
    bool launchEditor = false;    // request: open the editor
    bool returnToGame = false;    // after the settings window closes, go back to the game
    bool quit = false;
    std::string toast; float toastT = 0;
    int cursorX = 320, cursorY = 240; // keyboard/gamepad-driven cursor overlay (follows mouse when mouse moves)
    std::vector<Rect> hots, hotsPrev;   // clickable things drawn last frame: the D-pad / arrow keys hop the cursor between them
    void hot(int x, int y, int w, int h) { hots.push_back({x, y, w, h}); }
    bool snapCursor(int dx, int dy);    // move the cursor to the nearest hotspot in a direction; false if none
    bool padNavWindows = false;         // this frame: a window is open and the pad/keyboard is steering the cursor
    bool cursorHand = false;
    int iconPress = -1, iconDrag = -1;
    int iconPressX = 0, iconPressY = 0, iconDragOffsetX = 0, iconDragOffsetY = 0;
    bool iconPressPad = false;
    float bootTime = 0; bool booted = false;
    float startMenuT = 0;
    std::string lastMessage;
    SaveSlotInfo slots[4];   // 3 save slots + the autosave
    std::string assetDir;
    MineState mine; SolState sol; std::vector<std::array<float, 3>> stars;
    float idleT = 0; bool screensaver = false; float ssT = 0;

    bool init(Tileset* ts, Font* font, GameData* gd, Game* g, GameSettings* s, PlatformRequests* r, const std::string& saveDirectory, const std::string& iconPath);
    void update(InputState& in, float dt, Canvas& cv);
    void draw(Canvas& cv, InputState& in);
    void drawBoot(Canvas& cv, float t);
    static constexpr float BOOT_SECONDS = 3.2f;   // 1.1.1: one boot screen (the logo splash is gone)
    bool firstRun() const;
    bool loadFile(const std::string& path);   // load any save file into the game and switch to it
    void offerCrashRecovery();                // the last session ended in a crash: offer its recovery save
    std::string discordUrl() const;             // settings.ini discord= or the built-in invite                      // no saves/installed.ini yet: show the setup wizard once
    void markInstalled();
    void drawCursor(Canvas& cv, int x, int y);
    void refreshSlots();
    void openWindow(const std::string& type);
    void showDialog(const std::string& title, const std::string& text, std::function<void()> onYes = nullptr, bool yesNo = false);
    bool loadSlot(int slot);
    void runAction(const std::string& action);
    void showToast(const std::string& s) { toast = s; toastT = 2.5f; }
    size_t fixedIcons = 0;                 // built-in icons; campaign icons follow them
    // the player's desktop, kept in saves/desktop.ini: wallpaper, every icon's place, and icons they changed (by action)
    std::map<std::string, std::array<int, 2>> savedPos, customIcon;
    int contextIcon = -1;                  // right-click menu on an icon (-1 = the desktop's own menu)
    void iconCell(const std::string& action, int& c, int& r) const;   // the icon shown for an action (start menu too)
    void setIconPicture(int index, int c, int r);                      // c < 0: back to the default
    void refreshCampaignIcons();           // one desktop icon per campaign folder in assets/campaigns
    void openNewGame(const std::string& campaignId);

private:
    void drawWindowFrame(Canvas& cv, DeskWindow& w, InputState& in, bool active);
    void updateWindow(DeskWindow& w, InputState& in, float dt, bool active);
    void drawWindowContent(Canvas& cv, DeskWindow& w, InputState& in, bool active);
    void bevelBox(Canvas& cv, int x, int y, int w, int h, bool sunken = false);
    bool win98Button(Canvas& cv, InputState& in, int x, int y, int w, int h, const std::string& label, bool enabled = true, bool focus = false, bool active = true);
    bool checkbox(Canvas& cv, InputState& in, int x, int y, const std::string& label, bool& value, bool active = true);
    void slider(Canvas& cv, InputState& in, int x, int y, int w, const std::string& label, int& value, int lo, int hi, bool active = true);
    void drawIcon(Canvas& cv, int ic, int ir, int x, int y, int scale = 1);
    void drawTaskbar(Canvas& cv, InputState& in);
    void drawStartMenu(Canvas& cv, InputState& in);
    void drawContextMenu(Canvas& cv, InputState& in);
    void drawWallpaper(Canvas& cv);
    void contextAction(int action);
    void arrangeIcons();
    void saveDesktopSettings();
    // toys (toys.cpp)
    void mineReset(int difficulty); void minePlace(int sx, int sy); void mineReveal(int x, int y);
    void drawMinesweeper(Canvas& cv, DeskWindow& w, InputState& in, bool active);
    void solReset(); void solDrawCard(Canvas& cv, int x, int y, int card, bool faceUp, bool hi); bool solCanStack(int moving, int onto, bool foundation);
    void drawSolitaire(Canvas& cv, DeskWindow& w, InputState& in, bool active);
    void drawMyComputer(Canvas& cv, DeskWindow& w, InputState& in, bool active);
    void drawAchievementsWindow(Canvas& cv, DeskWindow& w, InputState& in, bool active);
public:
    void drawScreensaver(Canvas& cv, float dt);
    void screenChanged();   // 1.2.2: the screen size changed (App::setScreen)
private:
    int iconAt(int x, int y) const;
    bool snapIconCursor(int dx, int dy);
    void setIconPosition(int index, int x, int y);
    void dropIcon(int index);
    std::string slotPath(int slot) const { return slot == 3 ? saveDir + "/autosave.sav" : saveDir + "/slot" + std::to_string(slot + 1) + ".sav"; }

    int activeWindow() const { return windows.empty() ? -1 : (int)windows.size() - 1; }
    bool mouseInWindow(const InputState& in, const DeskWindow& w) const { return in.mouseX >= w.x && in.mouseY >= w.y && in.mouseX < w.x + w.w && in.mouseY < w.y + w.h; }
    bool mouseUsed = false; // consumed this frame
};
