#include "input.h"
#include "util.h"

Keybinds gKeys;
bool gPadGlyphs = false;

static const char* kKeyNames[K_COUNT] = {
    "None", "Up", "Down", "Left", "Right", "Enter", "Esc", "Space", "Tab", "Backspace", "Delete",
    "PgUp", "PgDn", "Home", "End", "Insert", "Shift", "Ctrl", "Alt",
    "F1", "F2", "F3", "F4", "F5", "F6", "F7", "F8", "F9", "F10", "F11", "F12",
    "0", "1", "2", "3", "4", "5", "6", "7", "8", "9",
    "A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K", "L", "M", "N", "O", "P", "Q", "R", "S", "T", "U", "V", "W", "X", "Y", "Z",
    "-", "=", "[", "]", ",", ".", "/", "`",
    "Pad A", "Pad B", "Pad X", "Pad Y", "LB", "RB", "LT", "RT", "Start", "Back", "DPad Up", "DPad Down", "DPad Left", "DPad Right", "LS", "RS"
};
const char* keyName(int k) { return (k >= 0 && k < K_COUNT) ? kKeyNames[k] : "?"; }
int keyFromName(const std::string& s) { for (int i = 0; i < K_COUNT; i++) if (s == kKeyNames[i]) return i; return K_NONE; }

static const char* kActNames[A_COUNT] = {
    "up", "down", "left", "right", "confirm", "cancel", "menu", "interact",
    "endturn", "attackmode", "spells", "items", "defend", "flee", "actionmenu", "center",
    "fav1", "fav2", "fav3", "fav4", "fav5", "fav6", "fav7", "fav8",
    "quicksave", "pageup", "pagedown", "fullscreen",
    "overwatch", "worldmap", "journal", "console",
    "catnext", "catprev", "sort", "favbar"
};
static const char* kActLabels[A_COUNT] = {
    "Move up / cursor up", "Move down / cursor down", "Move left / cursor left", "Move right / cursor right",
    "Confirm / select", "Cancel / pause", "Party menu", "Interact / talk",
    "End turn", "Toggle attack / aimed", "Spell menu", "Item menu", "Defend", "Flee", "Action menu", "Center camera",
    "Favorite 1", "Favorite 2", "Favorite 3", "Favorite 4", "Favorite 5", "Favorite 6", "Favorite 7", "Favorite 8",
    "Quick save (at shrine)", "Previous tab", "Next tab", "Toggle fullscreen",
    "Overwatch (combat)", "World map", "Journal", "Dev console",
    "Next item category", "Previous item category", "Sort items", "Favorites (pick one)"
};
const char* actionName(int a) { return (a >= 0 && a < A_COUNT) ? kActNames[a] : "?"; }
const char* actionLabel(int a) { return (a >= 0 && a < A_COUNT) ? kActLabels[a] : "?"; }

void Keybinds::reset() {
    for (int i = 0; i < A_COUNT; i++) key1[i] = key2[i] = pad[i] = K_NONE;
    auto set = [&](int a, int k1, int k2, int p) { key1[a] = (uint8_t)k1; key2[a] = (uint8_t)k2; pad[a] = (uint8_t)p; };
    set(A_UP, K_UP, K_W, K_PAD_UP); set(A_DOWN, K_DOWN, K_S, K_PAD_DOWN); set(A_LEFT, K_LEFT, K_A, K_PAD_LEFT); set(A_RIGHT, K_RIGHT, K_D, K_PAD_RIGHT);
    set(A_CONFIRM, K_ENTER, K_SPACE, K_PAD_A); set(A_CANCEL, K_ESCAPE, K_NONE, K_PAD_B); set(A_MENU, K_TAB, K_I, K_PAD_START); set(A_INTERACT, K_E, K_NONE, K_PAD_X);
    set(A_ENDTURN, K_T, K_NONE, K_PAD_RB); set(A_ATTACKMODE, K_A, K_NONE, K_PAD_Y); set(A_SPELLS, K_B, K_S, K_PAD_LB); set(A_ITEMS, K_Q, K_NONE, K_PAD_LT);
    set(A_DEFEND, K_D, K_NONE, K_NONE); set(A_FLEE, K_F, K_NONE, K_NONE); set(A_ACTIONMENU, K_R, K_NONE, K_PAD_BACK); set(A_CENTER, K_C, K_NONE, K_PAD_RS);
    set(A_FAV1, K_1, K_NONE, K_NONE); set(A_FAV2, K_2, K_NONE, K_NONE); set(A_FAV3, K_3, K_NONE, K_NONE); set(A_FAV4, K_4, K_NONE, K_NONE);
    set(A_FAV5, K_5, K_NONE, K_NONE); set(A_FAV6, K_6, K_NONE, K_NONE); set(A_FAV7, K_7, K_NONE, K_NONE); set(A_FAV8, K_8, K_NONE, K_NONE);
    set(A_QUICKSAVE, K_F5, K_NONE, K_NONE); set(A_PAGEUP, K_PAGEUP, K_LBRACKET, K_PAD_LB); set(A_PAGEDOWN, K_PAGEDOWN, K_RBRACKET, K_PAD_RB); set(A_FULLSCREEN, K_F11, K_NONE, K_NONE);
    set(A_OVERWATCH, K_O, K_NONE, K_PAD_LS); set(A_WORLDMAP, K_M, K_NONE, K_PAD_LS); /* LS: overwatch in a fight, world map outside */ set(A_JOURNAL, K_J, K_NONE, K_PAD_BACK); set(A_CONSOLE, K_GRAVE, K_NONE, K_NONE);
    set(A_CATNEXT, K_T, K_NONE, K_PAD_RT); set(A_CATPREV, K_G, K_NONE, K_PAD_LT); set(A_SORT, K_R, K_NONE, K_PAD_Y);
    set(A_FAVBAR, K_NONE, K_NONE, K_PAD_RT);   // 1.8.6 (#206): RT opens the favorites (Defend moved to the action menu on a pad)
}
std::string Keybinds::serialize() const {
    std::string s = "[keys]\n";
    for (int a = 0; a < A_COUNT; a++) s += fmt("%s=%s|%s|%s\n", kActNames[a], keyName(key1[a]), keyName(key2[a]), keyName(pad[a]));
    return s;
}
void Keybinds::deserialize(const std::string& text) {
    bool sawFavbar = false;
    for (auto& sec : parseConfig(text)) {
        if (sec.name != "keys") continue;
        for (auto& kv : sec.kv) {
            int a = -1; for (int i = 0; i < A_COUNT; i++) if (kv.first == kActNames[i]) a = i;
            if (a < 0) continue;
            if (a == A_FAVBAR) sawFavbar = true;
            auto p = split(kv.second, '|');
            if (p.size() > 0) key1[a] = (uint8_t)keyFromName(trim(p[0]));
            if (p.size() > 1) key2[a] = (uint8_t)keyFromName(trim(p[1]));
            if (p.size() > 2) pad[a] = (uint8_t)keyFromName(trim(p[2]));
        }
    }
    // 0.9.0: the world map got a controller button (left stick click); older settings files saved it as none
    // 1.8.6 (#206): RT became the favorites button; an older file still has it on Defend (the default then)
    if (!sawFavbar && pad[A_DEFEND] == K_PAD_RT) pad[A_DEFEND] = K_NONE;
    if (pad[A_WORLDMAP] == K_NONE) { bool used = false; for (int i = 0; i < A_COUNT; i++) if (i != A_OVERWATCH && pad[i] == K_PAD_LS) used = true; if (!used) pad[A_WORLDMAP] = K_PAD_LS; }
}
bool Keybinds::isDirectionKey(int k) const { if (!k) return false; for (int a = A_UP; a <= A_RIGHT; a++) if (key1[a] == k || key2[a] == k) return true; return false; }
bool Keybinds::pressed(const InputState& in, int a, bool combat) const {
    bool dir = a <= A_RIGHT;
    // outside combat the movement keys win: a hotkey that shares a key with a direction action does not fire while walking
    if (key1[a] && in.pressed[key1[a]] && (combat || dir || !isDirectionKey(key1[a]))) return true;
    if (key2[a] && in.pressed[key2[a]] && !(combat && dir) && (combat || dir || !isDirectionKey(key2[a]))) return true;
    if (pad[a] && in.pressed[pad[a]]) return true;
    return false;
}
bool Keybinds::down(const InputState& in, int a, bool combat) const {
    bool dir = a <= A_RIGHT;
    if (key1[a] && in.down[key1[a]]) return true;
    if (key2[a] && in.down[key2[a]] && !(combat && dir)) return true;
    if (pad[a] && in.down[pad[a]]) return true;
    return false;
}
std::string Keybinds::label(int a) const {
    if (gPadGlyphs && pad[a]) return keyName(pad[a]);
    std::string s = key1[a] ? keyName(key1[a]) : "";
    if (key2[a]) s += std::string(s.empty() ? "" : "/") + keyName(key2[a]);
    return s.empty() ? "-" : s;
}
