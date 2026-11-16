#include <filesystem>
// Headless test driver: runs the app with a scripted sequence of inputs and writes screenshots.
// Usage: triffall_headless script.txt [outdir]
#include "../app.h"
#include "bot.h"
#include <cstdio>
#include <map>
#include <chrono>
#include <algorithm>

static std::map<std::string, Key> keyMap = {
    {"UP", K_UP}, {"DOWN", K_DOWN}, {"LEFT", K_LEFT}, {"RIGHT", K_RIGHT}, {"ENTER", K_ENTER}, {"ESC", K_ESCAPE}, {"TAB", K_TAB}, {"SPACE", K_SPACE},
    {"E", K_E}, {"Q", K_Q}, {"T", K_T}, {"A", K_A}, {"S", K_S}, {"D", K_D}, {"F", K_F}, {"R", K_R}, {"C", K_C}, {"W", K_W}, {"I", K_I}, {"V", K_V}, {"X", K_X}, {"Z", K_Z}, {"K", K_K}, {"P", K_P}, {"B", K_B}, {"J", K_J}, {"M", K_M}, {"O", K_O}, {"GRAVE", K_GRAVE}, {"L", K_L}, {"LEFT2", K_LEFT}, {"L", K_L}, {"N", K_N}, {"F1", K_F1},
    {"BS", K_BACKSPACE}, {"DEL", K_DELETE}, {"F1", K_F1}, {"F2", K_F2}, {"F5", K_F5}, {"F9", K_F9}, {"F11", K_F11},
    {"1", K_1}, {"2", K_2}, {"3", K_3}, {"4", K_4}, {"5", K_5}, {"6", K_6}, {"7", K_7}, {"8", K_8}, {"PGUP", K_PAGEUP}, {"PGDN", K_PAGEDOWN}, {"SHIFT", K_SHIFT}, {"PADSTART", K_PAD_START}, {"PADA", K_PAD_A}, {"PADB", K_PAD_B}, {"PADX", K_PAD_X}, {"PADY", K_PAD_Y}, {"PADLB", K_PAD_LB}, {"PADRB", K_PAD_RB}, {"PADLT", K_PAD_LT}, {"PADRT", K_PAD_RT}, {"PADBACK", K_PAD_BACK}, {"PADLS", K_PAD_LS}, {"PADRS", K_PAD_RS}, {"PADUP", K_PAD_UP}, {"PADDOWN", K_PAD_DOWN}, {"PADLEFT", K_PAD_LEFT}, {"PADRIGHT", K_PAD_RIGHT},
    {"HOME", K_HOME}, {"END", K_END}, {"INS", K_INSERT}, {"F3", K_F3}, {"G", K_G}, {"H", K_H}, {"U", K_U}, {"Y", K_Y}, {"SLASH", K_SLASH}, {"LBRACKET", K_LBRACKET}, {"RBRACKET", K_RBRACKET}, {"9", K_9}, {"0", K_0}};

int main(int argc, char** argv) {
    if (argc < 2) { printf("usage: %s script [outdir]\n", argv[0]); return 1; }
    std::string outdir = argc > 2 ? argv[2] : ".";
    App app;
    if (!app.init("assets", outdir)) { printf("init failed: %s\n", app.error.c_str()); return 1; }
    app.skipBoot = true;
    app.game.fixedSeed = 20261022;   // the same dice every run
    InputState in; in.mouseX = 320; in.mouseY = 240;
    std::string script; if (!readFile(argv[1], script)) { printf("no script\n"); return 1; }
    const float dt = 1.0f / 60.0f;
    std::string clip; bool padMode = false; int fails = 0;
    auto step = [&](int n) { for (int i = 0; i < n; i++) { if (padMode) in.gamepadActive = true; app.frame(in, dt); if (app.req.hasClipboardOut) { clip = app.req.setClipboard; app.req.hasClipboardOut = false; } bool c = in.down[K_CTRL], sh = in.down[K_SHIFT]; in.beginFrame(); in.down[K_CTRL] = c; in.down[K_SHIFT] = sh; if (c) in.clipboard = clip; if (app.wantsQuit()) break; } };
    Bot bot(app, in, step, fails);
    // 1.1.11: more key / mouse commands (kept out of the long else-if chain below: MSVC limits its depth)
    auto moreCommand = [&](const std::string& cmd, const std::vector<std::string>& p) -> bool {
            if (cmd == "ctrlshift") { in.down[K_CTRL] = true; in.down[K_SHIFT] = true; in.clipboard = clip; in.press(keyMap[p[1]]); step(1); in.release(keyMap[p[1]]); in.down[K_CTRL] = false; in.down[K_SHIFT] = false; in.clipboard.clear(); step(1);  return true; }
            if (cmd == "rtap") { int nx = atoi(p[1].c_str()), ny = atoi(p[2].c_str()); in.mouseX = nx; in.mouseY = ny; in.mouseDX = in.mouseDY = 0; step(2); in.mousePressed[2] = true; in.mouseDown[2] = true; step(1); in.mouseDown[2] = false; in.mouseReleased[2] = true; step(3); return true; }   // 1.2.0: right click (press + release)
            if (cmd == "unitplace") { int i = atoi(p[1].c_str()); auto& u = app.game.cb.units; if (i >= 0 && i < (int)u.size()) { u[i].x = atoi(p[2].c_str()); u[i].y = atoi(p[3].c_str()); app.game.cb.reachDirty = true; } step(1); return true; }   // 1.3.4: unitplace <unit> <x> <y> (combat)
            if (cmd == "unitkill") { int i = atoi(p[1].c_str()); auto& u = app.game.cb.units; if (i >= 0 && i < (int)u.size()) { u[i].ch.hp = 0; u[i].ch.alive = false; app.game.cb.reachDirty = true; } step(1); return true; }   // 1.3.4: unitkill <unit> (no end-of-fight check)
            if (cmd == "cb") { std::string r = app.game.combatDo(p); printf("  cb %s\n", r.c_str()); step(1); return true; }   // 1.4.0: combat test hooks (combat.cpp combatDo)
            if (cmd == "drop") { std::string path; for (size_t i = 1; i < p.size(); i++) path += (i > 1 ? " " : "") + p[i]; in.dropped = {path}; step(1); in.dropped.clear(); step(2); return true; }   // 1.2.0: a file dropped on the window
#ifndef TRIFFALL_NO_EDITOR
            if (cmd == "sheetimport") {   // sheetimport <png> <name> <tw> <th> <ox> <oy> <gx> <gy> [black|alpha|r,g,b]
                SheetGrid g; g.tw = atoi(p[3].c_str()); g.th = atoi(p[4].c_str()); g.ox = atoi(p[5].c_str()); g.oy = atoi(p[6].c_str()); g.gx = atoi(p[7].c_str()); g.gy = atoi(p[8].c_str());
                if (p.size() > 9) { if (p[9] == "alpha") g.clear = 1; else if (p[9] != "black") { auto c = split(p[9], ','); if (c.size() >= 3) { g.clear = 2; g.key = Color8{(uint8_t)atoi(c[0].c_str()), (uint8_t)atoi(c[1].c_str()), (uint8_t)atoi(c[2].c_str()), 255}; } } }
                std::string err; int sh = app.editor.sheetImport(p[1], p[2], g, err); printf("sheetimport %s -> %d %s\n", p[2].c_str(), sh, err.c_str()); step(2); return true; }
            if (cmd == "sheetcopy") { std::string err; bool ok = app.editor.copyBaseSheets(err); printf("sheetcopy ok=%d %s\n", ok ? 1 : 0, err.c_str()); step(2); return true; }
            if (cmd == "edsetcampaign") { app.editor.setCampaign(p[1]); step(2); return true; }
            if (cmd == "contracheck") { app.editor.open(); app.editor.runCampaignCheck(); int n = 0; for (auto& f : app.editor.report) if (startsWith(f.text, "Contradiction")) { printf("  %s\n", f.text.c_str()); n++; } printf("contracheck: %d contradiction(s)\n", n); return true; }   // 1.5.0 (#89): Check all's contradiction findings
#endif
            // 1.5.0 (#89): assetreplace FILE OLD|NEW - edit a file under assets\ (run the test from a copy!); \n in OLD / NEW = a line break
            if (cmd == "assetreplace" && p.size() > 2) { std::string rest; for (size_t i = 2; i < p.size(); i++) rest += (i > 2 ? " " : "") + p[i];
                size_t bar = rest.find('|'); std::string from = rest.substr(0, bar), to = bar == std::string::npos ? "" : rest.substr(bar + 1);
                auto unesc = [](std::string v) { size_t at; while ((at = v.find("\\n")) != std::string::npos) v.replace(at, 2, "\n"); return v; }; from = unesc(from); to = unesc(to);
                std::string t, path = app.assetDir + "/" + p[1]; bool ok = readFile(path, t); size_t at = ok ? t.find(from) : std::string::npos;
                if (at != std::string::npos) { t.replace(at, from.size(), to); writeFile(path, t); }
                printf("assetreplace %s: %s\n", p[1].c_str(), at != std::string::npos ? "replaced" : "NOT FOUND"); if (at == std::string::npos) fails++; return true; }
            if (cmd == "screen") { app.setScreen(atoi(p[1].c_str()), atoi(p[2].c_str())); step(2); printf("screen %dx%d\n", app.canvas.w, app.canvas.h); return true; }   // 1.2.2: screen W H (a wide / tall screen)
            if (cmd == "edscreen") { app.setEditorScreen(atoi(p[1].c_str()), atoi(p[2].c_str())); step(2); return true; }   // 1.2.2: the Editor's canvas size
            if (cmd == "alt") { in.down[K_ALT] = true; in.press(keyMap[p[1]]); step(1); in.release(keyMap[p[1]]); in.down[K_ALT] = false; step(1);  return true; }
            if (cmd == "shiftclick") { int nx = atoi(p[1].c_str()), ny = atoi(p[2].c_str()); in.mouseX = nx; in.mouseY = ny; step(2); in.down[K_SHIFT] = true; in.mousePressed[0] = true; in.mouseDown[0] = true; step(1); in.mouseDown[0] = false; in.mouseReleased[0] = true; step(1); in.down[K_SHIFT] = false; step(2);  return true; }
            if (cmd == "ctrlclick") { int nx = atoi(p[1].c_str()), ny = atoi(p[2].c_str()); in.mouseX = nx; in.mouseY = ny; step(2); in.down[K_CTRL] = true; in.mousePressed[0] = true; in.mouseDown[0] = true; step(1); in.mouseDown[0] = false; in.mouseReleased[0] = true; step(1); in.down[K_CTRL] = false; step(2);  return true; }   // 1.2.0
            if (cmd == "tripleclick") { int nx = atoi(p[1].c_str()), ny = atoi(p[2].c_str()); in.mouseDX = nx - in.mouseX; in.mouseDY = ny - in.mouseY; in.mouseX = nx; in.mouseY = ny; step(2); in.mouseDX = in.mouseDY = 0; for (int k = 0; k < 3; k++) { in.mousePressed[0] = true; in.mouseDown[0] = true; step(1); in.mouseDown[0] = false; in.mouseReleased[0] = true; step(3); }  return true; }
        return false;
    };
    for (auto& raw : split(script, '\n')) {
        std::string line = trim(raw); if (line.empty() || line[0] == '#') continue;
        auto p = split(line, ' ');
        std::string cmd = p[0];
        if (bot.command(cmd, p, line) || moreCommand(cmd, p)) {}
        else if (cmd == "frames") step(atoi(p[1].c_str()));
        else if (cmd == "key") { in.press(keyMap[p[1]]); step(1); in.release(keyMap[p[1]]); step(1); }
        else if (cmd == "ctrl") { in.down[K_CTRL] = true; in.clipboard = clip; in.press(keyMap[p[1]]); step(1); in.release(keyMap[p[1]]); in.down[K_CTRL] = false; in.clipboard.clear(); step(1); printf("ctrl+%s clip=[%s]\n", p[1].c_str(), clip.substr(0, 40).c_str()); }
        else if (cmd == "shiftkey") { in.down[K_SHIFT] = true; in.press(keyMap[p[1]]); step(1); in.release(keyMap[p[1]]); in.down[K_SHIFT] = false; step(1); }
        else if (cmd == "keys") { for (size_t i = 1; i < p.size(); i++) { in.press(keyMap[p[i]]); step(1); in.release(keyMap[p[i]]); step(8); } }
        else if (cmd == "hold") { float oldX = in.stickX, oldY = in.stickY; if (p.size() > 3) in.stickX = (float)atof(p[3].c_str()); if (p.size() > 4) in.stickY = (float)atof(p[4].c_str()); in.press(keyMap[p[1]]); step(atoi(p[2].c_str())); in.stickX = oldX; in.stickY = oldY; in.release(keyMap[p[1]]); step(1); }
        else if (cmd == "press") { in.press(keyMap[p[1]]); step(1); }
        else if (cmd == "release") { in.release(keyMap[p[1]]); step(1); }
        else if (cmd == "type") { std::string t = line.substr(5); for (char c : t) { in.typed = std::string(1, c); step(1); } }
        else if (cmd == "move") { int nx = atoi(p[1].c_str()), ny = atoi(p[2].c_str()); in.mouseDX = nx - in.mouseX; in.mouseDY = ny - in.mouseY; in.mouseX = nx; in.mouseY = ny; step(2); }
        else if (cmd == "warp") { in.mouseX = atoi(p[1].c_str()); in.mouseY = atoi(p[2].c_str()); in.mouseDX = in.mouseDY = 0; step(2); }
        else if (cmd == "click" || cmd == "dblclick") { int nx = atoi(p[1].c_str()), ny = atoi(p[2].c_str()); in.mouseDX = nx - in.mouseX; in.mouseDY = ny - in.mouseY; in.mouseX = nx; in.mouseY = ny; step(2);
            int times = cmd == "click" ? 1 : 2; for (int k = 0; k < times; k++) { in.mousePressed[0] = true; in.mouseDown[0] = true; step(1); in.mouseDown[0] = false; in.mouseReleased[0] = true; step(3); } }
        else if (cmd == "rclick") { int nx = atoi(p[1].c_str()), ny = atoi(p[2].c_str()); in.mouseX = nx; in.mouseY = ny; step(1); in.mousePressed[2] = true; step(1); }
        else if (cmd == "shot") { savePNG(app.canvas, outdir + "/" + p[1]); printf("shot %s state=%d mode=%d map=%s pos=%d,%d\n", p[1].c_str(), (int)app.state, (int)app.game.mode, app.game.mapId.c_str(), app.game.px, app.game.py); }
        else if (cmd == "rdrag") { int x0 = atoi(p[1].c_str()), y0 = atoi(p[2].c_str()), x1 = atoi(p[3].c_str()), y1 = atoi(p[4].c_str()); in.mouseX = x0; in.mouseY = y0; step(1); in.mousePressed[2] = true; in.mouseDown[2] = true; step(1); for (int i = 1; i <= 10; i++) { int nx = x0 + (x1 - x0) * i / 10, ny = y0 + (y1 - y0) * i / 10; in.mouseDX = nx - in.mouseX; in.mouseDY = ny - in.mouseY; in.mouseX = nx; in.mouseY = ny; in.mouseDown[2] = true; step(1); } in.mouseDown[2] = false; in.mouseReleased[2] = true; step(1); }
        else if (cmd == "drag") { int x0 = atoi(p[1].c_str()), y0 = atoi(p[2].c_str()), x1 = atoi(p[3].c_str()), y1 = atoi(p[4].c_str()); in.mouseX = x0; in.mouseY = y0; step(1); in.mousePressed[0] = true; in.mouseDown[0] = true; step(1); for (int i = 1; i <= 10; i++) { int nx = x0 + (x1 - x0) * i / 10, ny = y0 + (y1 - y0) * i / 10; in.mouseDX = nx - in.mouseX; in.mouseDY = ny - in.mouseY; in.mouseX = nx; in.mouseY = ny; in.mouseDown[0] = true; step(1); } in.mouseDown[0] = false; in.mouseReleased[0] = true; step(1); }
        else if (cmd == "mouseunit") {   // mouseunit DX DY: move the pointer to the tile DX,DY from the acting combat unit
            auto& G = app.game; if (G.cb.selectedUnit >= 0) { auto& u = G.cb.units[G.cb.selectedUnit]; int nx = (u.x + atoi(p[1].c_str())) * UI::TILE - G.camX + UI::TILE / 2, ny = (u.y + atoi(p[2].c_str())) * UI::TILE - G.camY + UI::TILE / 2; in.mouseDX = nx - in.mouseX; in.mouseDY = ny - in.mouseY; in.mouseX = nx; in.mouseY = ny; } step(2); }
        else if (cmd == "mousein") { in.mouseInWindow = atoi(p[1].c_str()) != 0; step(1); }   // the pointer is over the window (a real mouse)
        else if (cmd == "wheel") { in.wheel = atoi(p[1].c_str()); step(1); }
        else if (cmd == "say") printf("%s\n", line.substr(4).c_str());
        else if (cmd == "teleport") { app.game.modeStack.clear(); app.game.mode = GMode::EXPLORE; app.game.enterMap(p[1], atoi(p[2].c_str()), atoi(p[3].c_str()), false); step(2); }
        else if (cmd == "saveload") {   // round-trip the game through its save format (what a save + load does)
            auto cnt = [&]() { int n = 0; if (app.game.map) for (auto& c : app.game.map->cells) n += c.explored; return n; };
            int before = cnt(); std::string s = app.game.serialize(); bool ok = app.game.deserialize(s); step(2);
            printf("saveload ok=%d explored %d -> %d map=%s\n", ok ? 1 : 0, before, cnt(), app.game.mapId.c_str());
        }
        else if (cmd == "god") { app.game.godMode = true; }
        else if (cmd == "reloaddesktop") {   // a fresh desktop, as on the next launch: it reads saves/desktop.ini back
            App& A = app; A.desktop.~Desktop(); new (&A.desktop) Desktop(); A.desktop.init(&A.tileset, &A.font, &A.data, &A.game, &A.settings, &A.req, A.saveDir, "assets/icons.png"); step(2); }
        else if (cmd == "campaign") { bool ok = app.game.switchCampaign(p[1]); printf("campaign %s ok=%d\n", p[1].c_str(), ok ? 1 : 0); }   // the next newgame plays this campaign
        else if (cmd == "setting") {   // setting NAME VALUE: accessibility and other options as Settings would set them
            GameSettings& st = app.settings; int v = p.size() > 2 ? atoi(p[2].c_str()) : 1;
            if (p[1] == "largetext") st.largeText = v; else if (p[1] == "colorblind") st.colorblind = v; else if (p[1] == "textspeed") st.textSpeed = v; else if (p[1] == "reduceflash") st.reduceFlashing = v; else if (p[1] == "font") st.fontStyle = v; else if (p[1] == "ambient") st.ambientVolume = v;
            else printf("unknown setting %s\n", p[1].c_str()); }
        else if (cmd == "boot") { app.desktop.drawBoot(app.canvas, (float)atof(p[1].c_str())); }   // render the boot screen at time T (then 'shot')
        else if (cmd == "setupwizard") { app.desktop.openWindow("setup"); step(2); }
        else if (cmd == "perf") {   // perf N [LABEL]: run N frames and report the time one frame takes (update + software render)
            int n = atoi(p[1].c_str()); std::vector<double> ms; ms.reserve(n);
            for (int i = 0; i < n; i++) { auto t0 = std::chrono::steady_clock::now(); step(1); ms.push_back(std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count()); }
            std::sort(ms.begin(), ms.end()); double sum = 0; for (double v : ms) sum += v;
            printf("PERF %-28s frames=%d avg=%.2fms p95=%.2fms max=%.2fms (60 fps = 16.7ms)\n", p.size() > 2 ? p[2].c_str() : "", n, sum / n, ms[(size_t)(n * 0.95)], ms.back()); }
        else if (cmd == "segfault") { volatile int* bad = nullptr; *bad = 1; }   // test the crash handler for real (not in the suite)
        else if (cmd == "saveslot") { app.game.saveToSlot(atoi(p[1].c_str()) - 1); }
        else if (cmd == "loadslot") { int sl = p[1] == "auto" ? Game::AUTOSAVE_SLOT : atoi(p[1].c_str()) - 1; bool ok = app.game.loadFromSlot(sl); printf("loadslot %s ok=%d %s\n", p[1].c_str(), ok ? 1 : 0, app.game.loadError.c_str()); step(2); }
        else if (cmd == "filereplace") {   // filereplace NAME OLD NEW: edit a file in the save directory (damage / age a save)
            std::string t, path = outdir + "/" + p[1]; readFile(path, t); size_t at = t.find(p[2]); if (at != std::string::npos) t.replace(at, p[2].size(), p.size() > 3 ? p[3] : ""); writeFile(path, t); }
        else if (cmd == "rmfile") { std::error_code ec; std::filesystem::remove(outdir + "/" + p[1], ec); }
        else if (cmd == "truncate") { std::string t, path = outdir + "/" + p[1]; readFile(path, t); writeFile(path, t.substr(0, t.size() / 3)); }
        else if (cmd == "clock") { app.game.clockMin = atoi(p[1].c_str()); }
        else if (cmd == "con") { app.game.runConsole(line.substr(4)); for (auto& o : app.game.consoleOut) printf("  con: %s\n", o.c_str()); app.game.consoleOut.clear(); step(2); }
        else if (cmd == "weather") { app.game.weather = p[1]; app.game.weatherSteps = 9999; app.game.precip.clear(); }
        else if (cmd == "interactmoving") { app.settings.interactWhileMoving = atoi(p[1].c_str()) != 0; }
        else if (cmd == "cheststate") { int cx = atoi(p[1].c_str()), cy = atoi(p[2].c_str()); Chest* c = app.game.map ? app.game.map->chestAt(cx, cy) : nullptr; printf("chest %d,%d found=%d opened=%d\n", cx, cy, c != nullptr, c && c->opened); }
        else if (cmd == "editor") { app.desktop.launchEditor = true; app.state = AppState::DESKTOP; step(2); }
#ifndef TRIFFALL_NO_EDITOR
        else if (cmd == "guide") { app.editor.openGuide(p.size() > 1 ? p[1] : "start"); step(2); }
        else if (cmd == "tab") { app.editor.tab = atoi(p[1].c_str()); step(2); }
        // 1.1.6: newcampaign NAME... | edimport KIND GEN SOURCE all|id,id (KIND gen/maps/dlg/kinds/camp/flags) | edsearch GEN [text] | edfilter GEN KEY VALUE | edsave GEN
        else if (cmd == "newcampaign") { std::string nm; for (size_t i = 1; i < p.size(); i++) nm += (i > 1 ? " " : "") + p[i]; app.editor.newCampaign(nm); step(2); }
        else if (cmd == "edimport") {
            std::string k = p[1]; int kind = k == "maps" ? Editor::IMP_MAPS : k == "dlg" ? Editor::IMP_DLG : k == "kinds" ? Editor::IMP_KINDS : k == "camp" ? Editor::IMP_CAMP : k == "flags" ? Editor::IMP_FLAGS : Editor::IMP_GEN;
            app.editor.openImport(kind, atoi(p[2].c_str()));
            for (size_t i = 0; i < app.editor.impSrcs.size(); i++) if (app.editor.impSrcs[i].id == p[3]) app.editor.impSelectSource((int)i);
            std::vector<std::string> ids = split(p.size() > 4 ? p[4] : "all", ',');
            for (auto& e : app.editor.impEntries) if (ids[0] == "all" || std::find(ids.begin(), ids.end(), e.id) != ids.end()) e.on = true;
            int n = app.editor.doImport(); printf("edimport: %d of %d entries\n", n, (int)app.editor.impEntries.size()); app.editor.closePopup(); step(2);
        }
        else if (cmd == "edsearch") { std::string q; for (size_t i = 2; i < p.size(); i++) q += (i > 2 ? " " : "") + p[i]; app.editor.genSearch(atoi(p[1].c_str()), q); }
        else if (cmd == "edfilter") app.editor.genSearch(atoi(p[1].c_str()), "", p[2], p.size() > 3 ? p[3] : "\x01has");
        else if (cmd == "edsave") { printf("edsave %s: %d\n", p[1].c_str(), app.editor.genSave(atoi(p[1].c_str())) ? 1 : 0); }
        else if (cmd == "checkall") {   // the Editor's Check all over the whole campaign; errors count as failures
            app.editor.open(); app.editor.runCampaignCheck(); int errs = 0;
            for (auto& f : app.editor.report) { printf("  check %s: %s\n", f.err ? "ERROR" : "warn", f.text.c_str()); if (f.err) errs++; }
            printf("checkall: %d findings, %d errors\n", (int)app.editor.report.size(), errs);
            if (p.size() > 1 && p[1] == "strict") fails += errs; }
#endif
        else if (cmd == "flag") { app.game.setFlag(p[1]); }
        else if (cmd == "audio") { gAudio.update(5.0f); printf("music=%s ambient=%s recent:", gAudio.currentMusic.c_str(), gAudio.currentAmbient.c_str()); for (auto& r : gAudio.recent) printf(" %s", r.c_str()); printf("\n"); if (!gAudio.missing.empty()) { printf("MISSING:"); for (auto& m : gAudio.missing) printf(" %s", m.c_str()); printf("\n"); } gAudio.recent.clear(); }
        else if (cmd == "fuzz" || cmd == "fuzzed") { bool ed = cmd == "fuzzed"; int n = atoi(p[1].c_str()); Rng fr(p.size() > 2 ? atoi(p[2].c_str()) : 7); static const Key ks[] = {K_UP, K_DOWN, K_LEFT, K_RIGHT, K_ENTER, K_ESCAPE, K_TAB, K_E, K_Q, K_T, K_1, K_2, K_3, K_4, K_5, K_F5, K_PAGEUP, K_PAGEDOWN, K_A, K_S, K_D, K_F, K_R, K_C, K_W, K_SPACE, K_K, K_P, K_DELETE, K_Z, K_J, K_M, K_O, K_B, K_GRAVE, K_L};
            for (int i = 0; i < n; i++) { int r = fr.range(0, 9); if (r < 6) { Key k = ks[fr.range(0, 35)]; in.press(k); step(1); in.release(k); step(fr.range(1, 6)); } else if (r < 8) { in.mouseX = fr.range(0, app.canvas.w - 1); in.mouseY = fr.range(0, app.canvas.h - 1); in.mousePressed[0] = true; in.mouseDown[0] = true; step(1); in.mouseDown[0] = false; step(2); } else step(10); if (ed && app.state == AppState::GAME) { app.state = AppState::DESKTOP; app.desktop.windows.clear(); app.desktop.launchEditor = true; step(2); } if (app.state == AppState::DESKTOP) { if (ed) { app.desktop.windows.clear(); app.desktop.launchEditor = true; step(2); } else if (app.game.running) { app.state = AppState::GAME; app.desktop.windows.clear(); } else break; } if (app.wantsQuit()) break; }
            printf("fuzz done: state=%d mode=%d map=%s\n", (int)app.state, (int)app.game.mode, app.game.mapId.c_str()); }
        else if (cmd == "pad") { padMode = atoi(p[1].c_str()) != 0; in.gamepadActive = padMode; }
        else if (cmd == "fuzzdesk") { int n = atoi(p[1].c_str()); Rng fr(p.size() > 2 ? atoi(p[2].c_str()) : 3); static const Key dk[] = {K_UP, K_DOWN, K_LEFT, K_RIGHT, K_ENTER, K_ESCAPE, K_TAB, K_F1, K_F9, K_SPACE, K_A, K_B};
            for (int i = 0; i < n; i++) { int r = fr.range(0, 9); if (r < 3) { Key k = dk[fr.range(0, 11)]; in.press(k); step(1); in.release(k); step(2); } else if (r < 9) { in.mouseX = fr.range(0, 639); in.mouseY = fr.range(0, 479); int b = fr.range(0, 5) == 0 ? 2 : 0; in.mousePressed[b] = true; in.mouseDown[b] = true; step(1); in.mouseDown[b] = false; step(fr.range(1, 3)); if (fr.range(0, 3) == 0) { in.mousePressed[0] = true; in.mouseDown[0] = true; step(1); in.mouseDown[0] = false; step(1); } } else step(5);
                if (app.state == AppState::GAME) { app.game.requestDesktop = true; step(2); } if (app.state == AppState::EDITOR) {
#ifndef TRIFFALL_NO_EDITOR
                    app.editor.wantsClose = true;
#endif
                    step(2); } if (app.desktop.quit || app.shuttingDown) { app.desktop.quit = false; app.shuttingDown = false; app.desktop.windows.clear(); } if (app.wantsQuit()) break; }
            printf("fuzzdesk done: windows=%d\n", (int)app.desktop.windows.size()); }
        else if (cmd == "target") {   // aim at the enemy nearest the unit whose turn it is, and confirm
            auto& G = app.game; int best = -1, bd = 1 << 20; int ax = 0, ay = 0; if (G.cb.selectedUnit >= 0) { ax = G.cb.units[G.cb.selectedUnit].x; ay = G.cb.units[G.cb.selectedUnit].y; }
            for (size_t i = 0; i < G.cb.units.size(); i++) { auto& u = G.cb.units[i]; if (!u.ch.alive || u.friendly()) continue; int d = std::max(abs(u.x - ax), abs(u.y - ay)); if (d < bd) { bd = d; best = (int)i; } }
            if (best >= 0) { G.cb.selX = G.cb.units[best].x; G.cb.selY = G.cb.units[best].y; }
            in.press(K_ENTER); step(1); in.release(K_ENTER); step(1); }
        else if (cmd == "combat") { std::vector<std::string> cs; for (auto& c : split(p[1], ',')) cs.push_back(c); app.game.startCombat(cs, app.game.px + 3, app.game.py, 0, {}, ""); step(2); }
        else if (cmd == "partywipe") { if (app.game.mode == GMode::COMBAT) { for (auto& u : app.game.cb.units) if (u.partyIndex >= 0) { u.ch.hp = 0; u.ch.alive = false; } app.game.cb.ending = true; app.game.cb.victory = false; app.game.cb.fled = false; app.game.cb.endTimer = 0; step(150); } }   // 1.1.7: lose the fight now
        else if (cmd == "forceflee") { if (app.game.mode == GMode::COMBAT) { app.game.cb.ending = true; app.game.cb.fled = true; app.game.cb.endTimer = 0; step(2); } }
        else if (cmd == "gold") { app.game.party.gold += atoi(p[1].c_str()); }
        else if (cmd == "give") { app.game.giveItem(p[1], p.size() > 2 ? atoi(p[2].c_str()) : 1); }
        else if (cmd == "level") { app.game.party.members[0].xp += atoi(p[1].c_str()); app.game.checkLevelUps(); }
        else if (cmd == "log") { for (auto& l : app.game.msgLog) printf("  log: %s\n", l.c_str()); for (auto& l : app.game.cb.log) printf("  cb: %s\n", l.c_str()); }
        else if (cmd == "expect") {   // expect KEY OP VALUE - a check; any failure makes the run exit with code 1
            if (p.size() < 4) { printf("EXPECT bad line: %s\n", line.c_str()); fails++; continue; }
            std::string key = p[1], op = p[2], want = p[3]; for (size_t i = 4; i < p.size(); i++) want += " " + p[i];
            Game& g = app.game; std::string got; bool num = true;
            auto edExpect = [&]() -> bool {   // 1.1.11 editor text checks (outside the else-if chain: MSVC limits its depth)
                // 1.2.4 (#41): map enemy groups - "groups" = x,y,c(hasing) of each one on the map, "enemydist" = steps to the nearest one (-1 none)
                if (key == "groups") { got.clear(); if (g.map) for (auto& e : g.map->enemies) if (!e.defeated && !e.hidden) got += (got.empty() ? "" : ";") + fmt("%d,%d", e.x, e.y) + (e.chasing ? ",c" : "") + (e.temp ? ",t" : ""); if (got.empty()) got = "none"; num = false; return true; }
                if (key == "enemydist") { int best = -1; if (g.map) for (auto& e : g.map->enemies) if (!e.defeated && !e.hidden) { int d = chebyshev(e.x, e.y, g.px, g.py); if (best < 0 || d < best) best = d; } got = std::to_string(best); return true; }
                // 1.3.3: heard:<event> = 1 if that sound played since the last "audio" command (which clears the list)
                // 1.5.0 (#102): npcshown:<placement id> = 1 if that NPC on this map is there now; epilogue = the slides the ending
                // would show right now (title: text, " || " between), the same choice as Game::startEnding
                if (startsWith(key, "npcshown:")) { got = "0"; if (g.map) for (auto& n : g.map->npcs) if (n.id == key.substr(9)) { bool h = g.extra.count("hidenpc_" + g.mapId + "_" + n.id) > 0 || (!n.showFlag.empty() && !g.evalCond(n.showFlag)) || (!n.hideFlag.empty() && g.evalCond(n.hideFlag)); got = h ? "0" : "1"; } return true; }
                if (key == "epilogue") { got.clear(); std::set<std::string> done; for (auto& sec : g.mergedConfig("epilogue.txt")) { if (sec.name != "slide") continue; std::string grp = sec.get("group"); if (!grp.empty() && done.count(grp)) continue; if (sec.has("if") && !g.evalCond(sec.get("if"))) continue; if (!grp.empty()) done.insert(grp); got += (got.empty() ? "" : " || ") + g.endingText(sec.get("title")) + ": " + g.endingText(sec.get("text")); } num = false; return true; }
                if (startsWith(key, "cb:")) { got = app.game.combatProbe(key.substr(3)); num = !got.empty() && (isdigit((unsigned char)got[0]) || (got[0] == '-' && got.size() > 1)) && got.find_first_of(",;:") == std::string::npos; return true; }   // 1.4.0

                // 1.4.0 (#75-#77): obstacle:X,Y = 1 while an uncleared obstacle covers it; kind:X,Y = its tile kind; crate:ID = x,y;
                // plate:X,Y = 1 while down; lorefile:NAME = 1 once unlocked; inputtext = what the text box holds
                if (startsWith(key, "obstacle:") || startsWith(key, "kind:") || startsWith(key, "plate:")) { auto q = split(key.substr(key.find(':') + 1), ','); int qx = q.size() > 1 ? atoi(q[0].c_str()) : -1, qy = q.size() > 1 ? atoi(q[1].c_str()) : -1;
                    if (startsWith(key, "obstacle:")) got = g.map && g.map->obstacleAt(qx, qy) ? "1" : "0";
                    else if (startsWith(key, "plate:")) { Plate* pl = g.map ? g.map->plateAt(qx, qy) : nullptr; got = pl && pl->down ? "1" : "0"; }
                    else { got = g.map && g.map->inBounds(qx, qy) && g.map->at(qx, qy).kind < allTileKinds().size() ? allTileKinds()[g.map->at(qx, qy).kind].name : "none"; num = false; }
                    return true; }
                if (startsWith(key, "crate:")) { got = "none"; if (g.map) for (auto& c : g.map->crates) if (c.id == key.substr(6)) got = fmt("%d,%d", c.x, c.y); num = false; return true; }
                if (startsWith(key, "lorefile:")) { got = g.foundFiles.count(key.substr(9)) ? "1" : "0"; return true; }
                if (key == "inputtext") { got = g.inputBuf; num = false; return true; }
                if (key == "deskfiles") { got.clear(); for (auto& f : app.desktop.loreFiles()) got += (got.empty() ? "" : ",") + f; num = false; return true; }   // My Computer's list
                if (startsWith(key, "heard:")) { std::string ev = key.substr(6); got = std::find(gAudio.recent.begin(), gAudio.recent.end(), ev) != gAudio.recent.end() ? "1" : "0"; return true; }
                // 1.4.0: spells:<member> = the spells member n knows (comma list); answers = the talk box's answers as shown
                // (only those whose conditions hold, with their [CHA 12] tags), joined by " | "
                if (startsWith(key, "spells:")) { int mi = atoi(key.substr(7).c_str()); got.clear(); if (mi >= 0 && mi < (int)g.party.members.size()) for (auto& sp : g.party.members[mi].spells) got += (got.empty() ? "" : ",") + sp; if (got.empty()) got = "none"; num = false; return true; }
                if (key == "answers") { got.clear(); if (g.mode == GMode::DIALOGUE) { auto it = g.dialogues.find(g.dlgId); if (it != g.dialogues.end()) { auto nt = it->second.nodes.find(g.dlgNode); if (nt != it->second.nodes.end()) for (auto& o : nt->second.options) { bool ok = true; for (auto& c : o.conds) if (!g.evalCond(c)) ok = false; if (ok) got += (got.empty() ? "" : " | ") + dialogueCheckTag(o.conds) + o.text; } } } if (got.empty()) got = "none"; num = false; return true; }
#ifndef TRIFFALL_NO_EDITOR
                if (key == "contradictions") { app.editor.open(); app.editor.runCampaignCheck(); got.clear(); for (auto& f : app.editor.report) if (startsWith(f.text, "Contradiction")) got += (got.empty() ? "" : " || ") + f.text; if (got.empty()) got = "none"; num = false; return true; }   // 1.5.0 (#89)
                if (key == "sheets") { got.clear(); for (int i : app.editor.visibleSheets()) got += (got.empty() ? "" : ",") + app.tileset.sheets[i].name; if (got.empty()) got = "none"; num = false; return true; }   // 1.2.0: the sheets the pickers offer
                if (startsWith(key, "sheetcat:")) { auto q = split(key.substr(9), ':'); app.editor.syncBooks(); Tile t = q.size() >= 2 ? parseTile(q[1] + "@" + q[0]) : Tile{}; got = std::string(1, app.editor.cellCat(t)); num = false; return true; }   // sheetcat:<sheet>:c,r
                if (startsWith(key, "sheetgroups:")) { app.editor.syncBooks(); int sh = sheetIndex(key.substr(12)); got = sh < (int)app.editor.books.size() ? std::to_string(app.editor.books[sh].groups.size()) : "0"; return true; }
                if (startsWith(key, "sheetdraws:")) { auto q = split(key.substr(11), ':'); Tile t = q.size() >= 2 ? parseTile(q[1] + "@" + q[0]) : Tile{}; got = app.tileset.cellEmpty(t) ? "0" : "1"; return true; }   // the tile has a picture
                if (key == "pickcat") { got = app.editor.pkCat ? std::string(1, app.editor.pkCat) : std::string("all"); num = false; return true; }
                if (key == "pickshown") { got = std::to_string(app.editor.pickerCount(app.editor.pkSheet, app.editor.pkCat, app.editor.pkGroup)); return true; }
                if (key == "picksheet") { got = sheetName(app.editor.pkSheet); num = false; return true; }
                if (startsWith(key, "edtext:")) { got = app.editor.edText(key.substr(7), 0); num = false; return true; }     // te / ste / big: the lines joined by |
                if (startsWith(key, "edsel:")) { got = app.editor.edText(key.substr(6), 1); num = false; return true; }      // selected text
                if (startsWith(key, "edcaret:")) { got = app.editor.edText(key.substr(8), 2); num = false; return true; }    // line,col (1-based)
                if (startsWith(key, "edstate:")) { got = app.editor.edText(key.substr(8), 3); num = false; return true; }    // wrap= wide= bar= ac= pop=
                if (startsWith(key, "edfield:")) { auto q = split(key.substr(8), ':'); got = q.size() >= 3 ? app.editor.genValue(atoi(q[0].c_str()), atoi(q[1].c_str()), q[2]) : ""; num = false; return true; }
#endif
                return false;
            };
            static const char* MODES[] = {"EXPLORE", "DIALOGUE", "COMBAT", "MENU", "SHOP", "MESSAGE", "TRANSITION", "GAMEOVER", "LEVELUP", "NAME", "VICTORY", "PAUSE", "CUTSCENE", "SAVELOAD", "JOURNAL", "WORLDMAP", "CRAFT", "CONSOLE", "BOUNTY", "ENDING"};
            if (edExpect()) {}
            else if (key == "gold") got = std::to_string(g.party.gold);
            else if (key == "x") got = std::to_string(g.px); else if (key == "y") got = std::to_string(g.py);
            else if (key == "hp") got = std::to_string(g.party.members.empty() ? 0 : g.party.members[0].hp);
            else if (key == "level") got = std::to_string(g.party.members.empty() ? 0 : g.party.members[0].level);
            else if (key == "members") got = std::to_string(g.party.members.size());
            else if (key == "running") got = g.running ? "1" : "0"; else if (key == "faceright") got = g.faceRight ? "1" : "0";
            else if (key == "state") got = std::to_string((int)app.state);
            else if (key == "bounties") got = std::to_string(g.bounties.size());
            else if (key == "windows") got = std::to_string(app.desktop.windows.size());
            else if (startsWith(key, "flag:")) { auto it = g.flags.find(key.substr(5)); got = std::to_string(it == g.flags.end() ? 0 : it->second); }
            else if (startsWith(key, "item:")) got = std::to_string(g.party.countItem(key.substr(5)));
            else if (startsWith(key, "cond:")) got = g.evalCond(key.substr(5)) ? "1" : "0";   // 1.1.7: any condition (lit, equipped:x, map:y...)
            else if (startsWith(key, "savefile:")) { std::error_code ec; got = std::filesystem::exists(app.saveDir + "/" + key.substr(9), ec) ? "1" : "0"; }
            else if (key == "cursorundermouse") { const int T = UI::TILE; got = (g.cb.selX == (in.mouseX + g.camX) / T && g.cb.selY == (in.mouseY + g.camY) / T) ? "1" : "0"; }
            else if (key == "cursoronunit") { got = "0"; if (g.cb.selectedUnit >= 0) { auto& u = g.cb.units[g.cb.selectedUnit]; got = (g.cb.selX == u.x && g.cb.selY == u.y) ? "1" : "0"; } }
            else if (key == "cblog") { got.clear(); for (auto& l : g.cb.log) got += l + "\n"; num = false; }
            else if (key == "allies" || key == "summons") { int c = 0; for (auto& u : g.cb.units) if (u.ch.alive && (key == "allies" ? u.ally : u.summoned)) c++; got = std::to_string(c); }
            else if (startsWith(key, "approval:")) got = std::to_string(g.approvalOf(key.substr(9)));
            else if (startsWith(key, "cskill:")) got = std::to_string(g.craftSkill(key.substr(7)));
#ifndef TRIFFALL_NO_EDITOR
            else if (startsWith(key, "edcount:")) got = std::to_string(app.editor.genCount(atoi(key.substr(8).c_str())));
            else if (startsWith(key, "edshown:")) got = std::to_string(app.editor.genShown(atoi(key.substr(8).c_str())));
            else if (startsWith(key, "asset:")) got = fileExists(app.assetDir + "/" + key.substr(6)) ? "1" : "0";
#endif
            else if (startsWith(key, "rep:")) { auto it = g.reputation.find(key.substr(4)); got = std::to_string(it == g.reputation.end() ? 0 : it->second); }
            else if (startsWith(key, "queststatus:")) { got = ""; for (auto& q : g.quests) if (q.id == key.substr(12)) got = g.questStatus(q); }
            else if (startsWith(key, "price:")) { const ItemDef* d = g.gd->item(key.substr(6)); got = d ? std::to_string(g.itemBuyPrice(*d)) : "-1"; }
            else if (startsWith(key, "quest:")) { got = "0"; for (auto& q : g.quests) if (q.id == key.substr(6)) got = std::to_string(g.questStageIndex(q)); }
            else { num = false;
                if (key == "mode") got = (int)g.mode >= 0 && (int)g.mode < 20 ? MODES[(int)g.mode] : "?";
                else if (key == "map") got = g.mapId;
                else if (key == "campaign") got = g.campaignId;
                else if (key == "music") got = gAudio.currentMusic;
                else if (key == "ambient") got = gAudio.currentAmbient;
                else if (key == "weather") got = g.weather;
                else if (startsWith(key, "iconpos:") || startsWith(key, "icon:")) { got = "none"; bool pos = startsWith(key, "iconpos:"); std::string act = key.substr(pos ? 8 : 5);
                    for (size_t i = 0; i < app.desktop.iconDefs.size(); i++) if (app.desktop.iconDefs[i].action == act) got = pos ? fmt("%d,%d", app.desktop.iconPositions[i][0], app.desktop.iconPositions[i][1]) : fmt("%d,%d", app.desktop.iconDefs[i].ic, app.desktop.iconDefs[i].ir); }
                else if (key == "wallpaper") got = std::to_string(app.desktop.wallpaper);
                else if (startsWith(key, "npc:")) { got = "none"; if (g.map) for (auto& n : g.map->npcs) if (n.id == key.substr(4)) got = n.name + ";" + fmt("%d,%d", n.tile.c, n.tile.r) + ";" + n.dialogue + ";" + n.def; }   // 1.1.9: a placement on this map
                else if (startsWith(key, "member:")) { size_t i = (size_t)atoi(key.substr(7).c_str()); got = i < g.party.members.size() ? g.party.members[i].name + ";" + g.party.members[i].classId + ";" + fmt("%d,%d", g.party.members[i].tile.c, g.party.members[i].tile.r) + ";" + std::to_string(g.party.members[i].level) : "none"; }
                else if (key == "hint") { int h = g.currentHint(); got = h >= 0 ? g.hintDefs[h].text : ""; }
                else if (startsWith(key, "var:")) got = g.keptVar(key.substr(4));   // 1.1.10: a script's keep variable
                else if (key == "scripterror") got = g.lastScriptError;
                else if (key == "scripts") { int c = 0; for (auto& r : g.scriptRuns) if (!r->done) c++; got = std::to_string(c); }
                else if (key == "talk") { got = ""; if (g.mode == GMode::DIALOGUE) { auto it = g.dialogues.find(g.dlgId); if (it != g.dialogues.end()) { auto nt = it->second.nodes.find(g.dlgNode); if (nt != it->second.nodes.end()) { got = nt->second.text; for (auto& o : nt->second.options) got += " | " + o.text; } } } }   // the text (and answers) in the talk box
                else if (startsWith(key, "fav:")) { int f = atoi(key.substr(4).c_str()) - 1; got = g.party.members.empty() || f < 0 || f > 7 ? "" : g.party.members[0].favs[f]; }
                else if (key == "window") got = app.desktop.windows.empty() ? "none" : app.desktop.windows.back().type;
                else if (key == "log") { got = ""; for (auto& l : g.msgLog) got += l + "\n"; }
                else { printf("EXPECT unknown key %s\n", key.c_str()); fails++; continue; } }
            bool ok;
            if (op == "contains") ok = got.find(want) != std::string::npos;
            else if (op == "lacks") ok = got.find(want) == std::string::npos;
            else if (num) { long a = atol(got.c_str()), b = atol(want.c_str()); ok = op == "==" ? a == b : op == "!=" ? a != b : op == ">=" ? a >= b : op == "<=" ? a <= b : op == ">" ? a > b : op == "<" ? a < b : false; }
            else ok = op == "==" ? got == want : op == "!=" ? got != want : false;
            if (!ok) { fails++; printf("EXPECT FAIL: %s %s %s (got %s)\n", key.c_str(), op.c_str(), want.c_str(), got.c_str()); } else printf("expect ok: %s %s %s\n", key.c_str(), op.c_str(), want.c_str());
        }
        else if (cmd == "dump") { printf("mode=%d stack=%d map=%s pos=%d,%d hp=%d gold=%d flags:", (int)app.game.mode, (int)app.game.modeStack.size(), app.game.mapId.c_str(), app.game.px, app.game.py, app.game.party.members.empty() ? 0 : app.game.party.members[0].hp, app.game.party.gold); for (auto& f : app.game.flags) printf(" %s=%d", f.first.c_str(), f.second); printf("\n"); }
#ifndef TRIFFALL_NO_EDITOR
        else if (cmd == "bakeall") {   // bake every generated dungeon and the overworld into editable map files (dev tool)
            app.editor.open(); std::string err; int n = 0;
            for (auto& sec : app.editor.dungeonSections()) { if (fileExists(app.assetDir + "/data/maps/" + sec.get("id") + ".txt")) { printf("  skip %s (exists)\n", sec.get("id").c_str()); continue; } if (app.editor.bakeDungeon(sec, err)) { n++; printf("  baked %s\n", sec.get("id").c_str()); } else printf("  FAIL %s: %s\n", sec.get("id").c_str(), err.c_str()); }
            if (!fileExists(app.assetDir + "/data/maps/overworld.txt")) { app.editor.bakeOverworld(); printf("  baked overworld\n"); }
            printf("baked %d dungeons\n", n);
        }
#endif
        else if (cmd == "combatdump") { for (auto& u : app.game.cb.units) printf("  unit %s party=%d pos=%d,%d hp=%d alive=%d ap=%d\n", u.ch.name.c_str(), u.partyIndex, u.x, u.y, u.ch.hp, u.ch.alive, u.ch.ap); }
        if (app.wantsQuit()) { printf("app quit\n"); break; }
        if (bot.strict && fails) { printf("stopping at the first failure (strict)\n"); break; }
    }
    if (fails) { printf("%d EXPECT FAILURE(S)\n", fails); return 1; }
    return 0;
}
