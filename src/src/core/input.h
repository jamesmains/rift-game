// Platform-independent input snapshot, filled once per frame by the platform layer,
// plus the rebindable action map used by the game and the desktop.
#pragma once
#include <cstdint>
#include <string>
#include <vector>

// Physical keys (and gamepad buttons as virtual keys). Order matters for saved keybinds: append only.
enum Key : uint8_t {
    K_NONE = 0,
    K_UP, K_DOWN, K_LEFT, K_RIGHT,
    K_ENTER, K_ESCAPE, K_SPACE, K_TAB, K_BACKSPACE, K_DELETE,
    K_PAGEUP, K_PAGEDOWN, K_HOME, K_END, K_INSERT,
    K_SHIFT, K_CTRL, K_ALT,
    K_F1, K_F2, K_F3, K_F4, K_F5, K_F6, K_F7, K_F8, K_F9, K_F10, K_F11, K_F12,
    K_0, K_1, K_2, K_3, K_4, K_5, K_6, K_7, K_8, K_9,
    K_A, K_B, K_C, K_D, K_E, K_F, K_G, K_H, K_I, K_J, K_K, K_L, K_M,
    K_N, K_O, K_P, K_Q, K_R, K_S, K_T, K_U, K_V, K_W, K_X, K_Y, K_Z,
    K_MINUS, K_EQUALS, K_LBRACKET, K_RBRACKET, K_COMMA, K_PERIOD, K_SLASH, K_GRAVE,
    K_PAD_A, K_PAD_B, K_PAD_X, K_PAD_Y, K_PAD_LB, K_PAD_RB, K_PAD_LT, K_PAD_RT,
    K_PAD_START, K_PAD_BACK, K_PAD_UP, K_PAD_DOWN, K_PAD_LEFT, K_PAD_RIGHT, K_PAD_LS, K_PAD_RS,
    K_COUNT
};
const char* keyName(int k);
int keyFromName(const std::string& s);
inline bool isPadKey(int k) { return k >= K_PAD_A && k <= K_PAD_RS; }

// Game actions that can be rebound.
enum Action : uint8_t {
    A_UP = 0, A_DOWN, A_LEFT, A_RIGHT,
    A_CONFIRM, A_CANCEL, A_MENU, A_INTERACT,
    A_ENDTURN, A_ATTACKMODE, A_SPELLS, A_ITEMS, A_DEFEND, A_FLEE, A_ACTIONMENU, A_CENTER,
    A_FAV1, A_FAV2, A_FAV3, A_FAV4, A_FAV5, A_FAV6, A_FAV7, A_FAV8,
    A_QUICKSAVE, A_PAGEUP, A_PAGEDOWN, A_FULLSCREEN,
    A_OVERWATCH, A_WORLDMAP, A_JOURNAL, A_CONSOLE,
    A_CATNEXT, A_CATPREV, A_SORT,   // item lists: next / previous category, change the sort order
    A_FAVBAR,                       // 1.8.6 (#206): open the favorites picker (a controller has no number keys)
    A_TARGETPREV, A_TARGETNEXT,     // 1.8.7: fights - the combat cursor jumps to the previous / next foe in sight
    A_COUNT
};
const char* actionName(int a);   // identifier (saved in settings)
const char* actionLabel(int a);  // human label

struct InputState {
    bool down[K_COUNT] = {};
    bool pressed[K_COUNT] = {};
    bool released[K_COUNT] = {};
    int mouseX = 0, mouseY = 0;   // in framebuffer coordinates
    int mouseDX = 0, mouseDY = 0;
    bool mouseDown[3] = {};
    bool mousePressed[3] = {};
    bool mouseReleased[3] = {};
    int wheel = 0;
    bool mouseInWindow = true;
    bool cursorOnWindow = true;   // 1.8.4 (#191): the pointer is over the window at all (mouseInWindow is false over the letterbox bars)
    std::string typed;            // text characters typed this frame
    bool gamepadActive = false;   // true if the last input came from a gamepad
    float stickX = 0, stickY = 0; // left stick
    float stick2X = 0, stick2Y = 0; // right stick (camera)
    bool quitRequested = false;
    int lastKey = K_NONE;         // last key pressed this frame (for rebinding)
    std::string clipboard;        // OS clipboard contents, filled by the platform while Ctrl is held
    std::vector<std::string> dropped;   // 1.2.0: files dropped on the window this frame (the Editor imports sprite sheets)
    bool ctrl() const { return down[K_CTRL]; }
    bool shift() const { return down[K_SHIFT]; }

    void beginFrame() {
        for (int i = 0; i < K_COUNT; i++) { pressed[i] = released[i] = false; }
        for (int i = 0; i < 3; i++) { mousePressed[i] = mouseReleased[i] = false; }
        wheel = 0; typed.clear(); dropped.clear(); mouseDX = mouseDY = 0; lastKey = K_NONE;
    }
    void press(Key k) { if (!down[k]) pressed[k] = true; down[k] = true; lastKey = k; }
    void release(Key k) { if (down[k]) released[k] = true; down[k] = false; }
    // Consume a click so nothing behind the UI element that handled it reacts to it too.
    void eatMouse() { mousePressed[0] = mousePressed[1] = mousePressed[2] = false; }
    void eatKeys() { for (int i = 0; i < K_COUNT; i++) pressed[i] = false; }
};

// Rebindable action map: each action has up to two keyboard keys and one gamepad button.
struct Keybinds {
    uint8_t key1[A_COUNT] = {}, key2[A_COUNT] = {}, pad[A_COUNT] = {};
    Keybinds() { reset(); }
    void reset();
    std::string serialize() const;
    void deserialize(const std::string& text);
    // 'combat' disables the WASD-style secondary keys of the direction actions so letters can be hotkeys.
    bool pressed(const InputState& in, int a, bool combat = false) const;
    bool down(const InputState& in, int a, bool combat = false) const;
    std::string label(int a) const; // e.g. "E / A"
    bool isDirectionKey(int k) const;
    // 1.8.7: gamepad layouts (Settings > Controls > Gamepad). Only the pad column changes; keyboard keys stay.
    //   Classic: the layout up to 1.8.6 (Y aimed, LB spells, RB end turn, LT items, RT favorites, Back action menu).
    //   Tactics: LB / RB jump between foes, LT spells, RT items, Y action menu (aimed, defend, overwatch, flee...),
    //            Back favorites (Journal outside fights), LS end turn (world map outside fights).
    // The A / B swap is kept apart (padSwapped) so a layout can be picked with either.
    enum { PAD_CLASSIC = 0, PAD_TACTICS = 1, PAD_LAYOUTS = 2 };
    static void padLayout(int layout, uint8_t out[A_COUNT]);   // a layout's pad column, unswapped
    void applyPadLayout(int layout);   // keeps the A / B swap as it is
    int padLayoutNow() const;          // which layout the pad column is (-1 = changed by hand)
    bool padSwapped() const { return pad[A_CONFIRM] == K_PAD_B && pad[A_CANCEL] == K_PAD_A; }
    void setPadSwap(bool on);          // confirm on B and back on A (and every other A / B binding swapped)
};
extern Keybinds gKeys;
extern bool gPadGlyphs;   // true while a gamepad is the active input: labels/hints show controller buttons
inline bool actP(const InputState& in, int a, bool combat = false) { return gKeys.pressed(in, a, combat); }
inline bool actD(const InputState& in, int a, bool combat = false) { return gKeys.down(in, a, combat); }

// Things the game asks the platform to do.
struct PlatformRequests {
    bool toggleFullscreen = false;
    bool setFullscreen = false; bool fullscreenValue = false;
    bool quit = false;
    std::string openURL;
    bool setVsync = false; bool vsyncValue = true;
    int setScale = 0;
    bool setIntegerScaling = false; bool integerScalingValue = true;
    int canvasW = 0, canvasH = 0;  // non-zero: the app's canvas changed size (editor); -1 = back to the game's screen size (1.2.2)
    std::string setClipboard; bool hasClipboardOut = false;
    bool startUpdate = false;      // 1.7.0 (#140): the player chose Update: start the updater (core/updater.h) and quit
};
