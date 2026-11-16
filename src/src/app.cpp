#include "app.h"
#include "game/tiles.h"
#include "core/crash.h"

bool App::init(const std::string& assets, const std::string& saves, AudioBackend* audio) {
    assetDir = assets; saveDir = saves; gAssetDir = assets;
    gAudio.init(audio, assets);
    canvas.init(UI::SCREEN_W, UI::SCREEN_H);
    if (!tileset.load(assets + "/tiles.png")) { error = "Missing " + assets + "/tiles.png"; return false; }
    tileset.useCampaign(assets + "/data", "", true);   // 1.2.0: the base game's sprite sheets (assets/data/sheets)
    font.build(tileset);
    if (!data.load(assets + "/data")) { error = "Data error: " + data.lastError; return false; }
    ensureTileKindsFile(assets + "/data/tilekinds.txt");
    loadTileKinds(assets + "/data/tilekinds.txt");
    settings.load(saves + "/settings.ini");
    game.saveDir = saves;
    game.init(&data, &settings, assets, &tileset, &font);
#ifndef TRIFFALL_NO_EDITOR
    editor.init(&tileset, &font, &data, assets, &req); editor.exportDir = saves + "/exports"; editor.prefsPath = saves + "/editor.ini";
    editor.onSwitchCampaign = [this](const std::string& id) { if (game.running && game.campaignId != id) game.running = false; return game.switchCampaign(id); };
#endif
    if (!desktop.init(&tileset, &font, &data, &game, &settings, &req, saves, assets + "/icons.png")) { error = "Missing " + assets + "/icons.png"; return false; }
    crashInstall(saves, [this]() { return game.crashState(); }, [this]() { game.emergencySave(); });   // saves/crash.log + crash_recovery.sav
    state = AppState::BOOT;
    return true;
}

#ifndef TRIFFALL_NO_EDITOR
bool App::startPlaytest() {
    const PlayState& ps = editor.play;
    if (ps.on || editor.playFresh) {   // Play from a state: always a fresh game with the chosen party
        editor.playFresh = false;
        std::string cls = !ps.cls.empty() && data.cls(ps.cls) ? ps.cls : data.classOrder.empty() ? "" : data.classOrder[0];
        game.newGame("Tester", cls); game.party.gold = ps.gold;
        for (auto& f : split(ps.flags, ',')) { auto kv = split(trim(f), '='); if (trim(kv[0]).empty()) continue; game.flags[trim(kv[0])] = kv.size() > 1 ? atoi(kv[1].c_str()) : 1; }
        for (auto& it : split(ps.items, ',')) { auto p = split(trim(it), ':'); if (!trim(p[0]).empty() && data.item(trim(p[0]))) game.giveItem(trim(p[0]), p.size() > 1 ? std::max(1, atoi(p[1].c_str())) : 1, true); }
        for (auto& c : split(ps.party, ',')) if (!trim(c).empty()) game.runScript("join:" + trim(c));
        for (auto& m : game.party.members) while (m.level < ps.level && m.level < 30) { m.xp += m.xpToNext(); game.checkLevelUps(); }
        for (auto& m : game.party.members) m.fullHeal(data);
        game.log(fmt("Playtest from a state: level %d %s, %d gold%s.", ps.level, cls.c_str(), ps.gold, ps.flags.empty() ? "" : (", flags " + ps.flags).c_str()));
    } else if (!game.running || game.gameOver) {
        std::string cls = data.classOrder.empty() ? "" : data.classOrder[0];
        game.newGame("Tester", cls);
        game.party.gold += 500;
        game.log("Playtest from the Editor: a fresh level-1 " + cls + " with 500 gold. Save points work as usual.");
    }
    game.modeStack.clear(); game.mode = GMode::EXPLORE;
    game.enterMap(editor.playMap, editor.playX, editor.playY, true);
    game.playtest = true;   // the pause menu offers Back to Editor
    game.log("Esc > Back to Editor returns to the Editor.");
    desktop.windows.clear();
    return true;
}
#endif

void App::screenSizeFor(int sw, int sh, bool classic43, bool integer, int& w, int& h, int baseW, int baseH) {
    w = baseW; h = baseH;
    if (classic43 || sw <= 0 || sh <= 0) return;
    if ((long long)sw * baseH >= (long long)sh * baseW) {   // 4:3 or wider: baseH (480) high, as wide as the screen's shape
        int k = sh / baseH;
        if (integer && k >= 1 && sh / k <= baseH * 5 / 4 && sw / k >= baseW) { w = sw / k; h = sh / k; }   // up to 600 high when that is exactly k screen pixels per canvas pixel
        else { h = baseH; w = (int)((long long)sw * baseH / sh); }
    } else {                                                  // narrower than 4:3 (a 5:4 or portrait screen): baseW (640) wide, taller
        int k = sw / baseW;
        if (integer && k >= 1 && sw / k <= baseW * 5 / 4 && sh / k >= baseH) { w = sw / k; h = sh / k; }
        else { w = baseW; h = (int)((long long)sh * baseW / sw); }
    }
    w = clampi(w, baseW, baseW * 4); h = clampi(h, baseH, baseH * 3);
}
void App::editorSizeFor(int sw, int sh, bool classic43, bool integer, int& w, int& h) {
#ifndef TRIFFALL_NO_EDITOR
    screenSizeFor(sw, sh, classic43, integer, w, h, Editor::BASE_W, Editor::BASE_H);
#else
    (void)sw; (void)sh; (void)classic43; (void)integer; w = 960; h = 720;
#endif
}
void App::setEditorScreen(int w, int h) {
#ifndef TRIFFALL_NO_EDITOR
    Editor::W = w; Editor::H = h;   // the Editor re-makes its canvas when the size changes (Editor::update)
#else
    (void)w; (void)h;
#endif
}

void App::setScreen(int w, int h) {
    w = std::max(w, (int)UI::BASE_W); h = std::max(h, (int)UI::BASE_H);
    screenW = w; screenH = h;
    if (screenSet && UI::SCREEN_W == w && UI::SCREEN_H == h && (state == AppState::EDITOR || (canvas.w == w && canvas.h == h))) return;
    screenSet = true;
    UI::SCREEN_W = w; UI::SCREEN_H = h;
    if (state != AppState::EDITOR) canvas.init(w, h);
    desktop.screenChanged();
    game.screenChanged();
}

void App::frame(InputState& in, float dt) {
    if (dt > 0.1f) dt = 0.1f;
    if (in.quitRequested) { quit = true; return; }
    if (!screenSet) setScreen(screenW, screenH);   // the platform sets the size before the first frame; headless stays 640x480
    gAudio.setVolumes(settings.masterVolume, settings.musicVolume, settings.sfxVolume, settings.ambientVolume);
    font.style = Font::STYLE_CLEAR;   // 1.1.1: the only text face; the classic glyphs are kept for the logo
    gAudio.update(dt);
    if (shuttingDown) {
        shutdownT -= dt; canvas.fillRect(0, 0, canvas.w, canvas.h, C::Black);
        { std::string msg = "It's now safe to turn off your computer."; font.draw(canvas, tileset, (canvas.w - font.width(msg)) / 2, canvas.h / 2 - 6, msg, RGB(255, 170, 40)); }
        if (shutdownT <= 0) quit = true;
        return;
    }
    if (in.pressed[K_F11]) { settings.fullscreen = !settings.fullscreen; req.setFullscreen = true; req.fullscreenValue = settings.fullscreen; settings.save(saveDir + "/settings.ini"); }
    switch (state) {
        case AppState::BOOT:
            bootT += dt; if (!bootChimed && bootT > 0.3f) { bootChimed = true; gAudio.play("boot"); }
            desktop.drawBoot(canvas, bootT);
            if (bootT > Desktop::BOOT_SECONDS || skipBoot || ((actP(in, A_CONFIRM) || actP(in, A_CANCEL) || in.mousePressed[0]) && bootT > 0.3f)) {
                state = AppState::DESKTOP; if (settings.fullscreen) { req.setFullscreen = true; req.fullscreenValue = true; }
                if (!skipBoot && desktop.firstRun()) desktop.openWindow("setup");   // first launch: the setup wizard
            }
            break;
        case AppState::DESKTOP:
            gAudio.music(""); gAudio.ambient("");
            desktop.update(in, dt, canvas);
            desktop.draw(canvas, in);
            game.drawAchievementPopups(canvas, dt);
            if (desktop.returnToGame) { bool open = false; for (auto& w : desktop.windows) if (w.type == "settings") open = true; if (!open) { desktop.returnToGame = false; if (game.running) desktop.launchGame = true; } }
            if (desktop.launchGame) { desktop.launchGame = false; if (game.running) state = AppState::GAME; }
#ifndef TRIFFALL_NO_EDITOR
            if (desktop.launchEditor) { desktop.launchEditor = false; state = AppState::EDITOR; canvas.init(Editor::W, Editor::H); req.canvasW = Editor::W; req.canvasH = Editor::H; if (editor.campaignId != game.campaignId) editor.setCampaign(game.campaignId); editor.open(); }
#else
            desktop.launchEditor = false;
#endif
            if (desktop.quit) { desktop.quit = false; shuttingDown = true; shutdownT = 1.4f; gAudio.music(""); gAudio.play("shutdown"); }
            break;
        case AppState::GAME:
            gAudio.music(game.musicWanted());
            gAudio.ambient(game.ambientWanted());
            {
                // with a controller in hand the (hidden) mouse must not hover anything: a pointer resting on a dialogue
                // option or list row would otherwise keep grabbing the selection back from the D-pad
                InputState gin = in;
                if (in.gamepadActive) { gin.mouseX = gin.mouseY = -10000; gin.mouseDX = gin.mouseDY = 0; gin.mouseInWindow = false; gin.wheel = 0; }
                game.update(gin, dt, canvas);
                game.draw(canvas, gin);
            }
            game.drawAchievementPopups(canvas, dt);   // last, so no screen or menu can cover a popup
            if (game.requestDesktop) { game.requestDesktop = false; game.running = false; game.playtest = false; state = AppState::DESKTOP; desktop.windows.clear(); desktop.refreshSlots(); if (!game.desktopAction.empty()) { std::string act = game.desktopAction; game.desktopAction.clear(); desktop.runAction(act); } if (game.gameOver) { desktop.showDialog("Triffall.exe", "Your party has fallen. Load a saved game to try again."); } }
            if (game.requestEditor) {   // 1.1.3: pause menu > Back to Editor (playtests only); the game keeps running for the next Play
                game.requestEditor = false;
#ifndef TRIFFALL_NO_EDITOR
                state = AppState::EDITOR; desktop.windows.clear(); canvas.init(Editor::W, Editor::H); req.canvasW = Editor::W; req.canvasH = Editor::H;
                if (editor.campaignId != game.campaignId) editor.setCampaign(game.campaignId);
                editor.open();
#endif
            }
            if (game.requestSettings) { game.requestSettings = false; state = AppState::DESKTOP; desktop.windows.clear(); desktop.openWindow("settings"); desktop.returnToGame = true; }
            break;
        case AppState::EDITOR:
#ifndef TRIFFALL_NO_EDITOR
            if (!editor.previewingMusic) gAudio.music("");
            gAudio.ambient("");   // 1.3.1 (#49): a playtest's ambient loop stops in the Editor (the game keeps running for the next Play)
            editor.update(in, dt, canvas);
            if (editor.wantsClose) {
                editor.wantsClose = false; state = AppState::DESKTOP; canvas.init(UI::SCREEN_W, UI::SCREEN_H); req.canvasW = -1; req.canvasH = -1;   // -1: the game's screen size (1.2.2)
                gAudio.loadDefs(); editor.previewingMusic = false;
                if (editor.changed) { editor.changed = false; game.reloadContent(); desktop.showToast("Game data reloaded."); }
                desktop.refreshCampaignIcons();
                if (editor.playRequest) { editor.playRequest = false; if (startPlaytest()) state = AppState::GAME; }
                if (editor.requestNewGame) { editor.requestNewGame = false; desktop.openNewGame(editor.campaignId); }
            }
#else
            state = AppState::DESKTOP;
#endif
            break;
    }
}
