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
    bool startPlaytest();                   // editor "Play here" (not compiled with TRIFFALL_NO_EDITOR): start or move the running game to editor.playMap at playX/playY
    void frame(InputState& in, float dt);   // update + draw into canvas
    bool wantsQuit() const { return quit; }
};
