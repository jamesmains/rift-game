// Top-level application: boot screen -> desktop <-> game.
#pragma once
#include <string>
#include "core/gfx.h"
#include "core/input.h"
#include "core/audio.h"
#include "game/data.h"
#include "game/game.h"
#include "desktop/desktop.h"
#ifndef TRIFFALL_NO_EDITOR
#include "editor/editor.h"
#endif

enum class AppState { BOOT, DESKTOP, GAME, EDITOR };

class App {
public:
    Tileset tileset;
    Font font;
    GameData data;
    GameSettings settings;
    PlatformRequests req;
    Game game;
    Desktop desktop;
#ifndef TRIFFALL_NO_EDITOR
    Editor editor;
#endif
    Canvas canvas;
    AppState state = AppState::BOOT;
    float bootT = 0;
    bool quit = false;
    std::string assetDir, saveDir, error;
    bool skipBoot = false;
    float shutdownT = 0; bool shuttingDown = false, bootChimed = false;

    bool init(const std::string& assets, const std::string& saves, AudioBackend* audio = nullptr);
    bool deleteCampaign(const std::string& id, std::string& err);   // 1.7.0 (#145): files + saves, and nothing still points at it
    std::string updateToldFor;              // 1.7.0 (#140): the update version the running game was told about
    bool startPlaytest();                   // editor "Play here" (not compiled with TRIFFALL_NO_EDITOR): start or move the running game to editor.playMap at playX/playY
    void frame(InputState& in, float dt);   // update + draw into canvas
    bool wantsQuit() const { return quit; }
    // 1.2.2 (#39): the canvas the game and desktop draw into for an output area of sw x sh pixels (a window's inside or
    // the monitor): the area's shape, 480 high (up to 600 when that makes a whole-number scale with integer scaling),
    // at least 640 wide (taller than 480 on a screen narrower than 4:3). classic43 = always 640x480 (bars).
    static void screenSizeFor(int sw, int sh, bool classic43, bool integer, int& w, int& h, int baseW = UI::BASE_W, int baseH = UI::BASE_H);
    static void editorSizeFor(int sw, int sh, bool classic43, bool integer, int& w, int& h);   // the same for the Editor (960x720 base)
    // Make the game/desktop canvas w x h (UI::SCREEN_W/H). The Editor keeps its own canvas; it takes the size on return.
    void setScreen(int w, int h);
    void setEditorScreen(int w, int h);   // the Editor's canvas (Editor::W/H)
    int screenW = UI::BASE_W, screenH = UI::BASE_H;   // the size the game/desktop canvas should be
    bool screenSet = false;
};
