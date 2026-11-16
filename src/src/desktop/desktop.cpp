#include "desktop.h"
#include "../core/audio.h"
#include "../core/platform_services.h"
#include "../game/tiles.h"
#include "../core/version.h"
#include <algorithm>
#include <ctime>
#include <filesystem>
#include <cmath>
#include <cmath>

static const Color8 FACE = RGB(192, 192, 192), SHADOW = RGB(128, 128, 128), HILITE = RGB(255, 255, 255), DKSHADOW = RGB(0, 0, 0);
static const Color8 TITLE_A = RGB(0, 0, 128), TITLE_B = RGB(16, 132, 208), DESK = RGB(0, 128, 128);
static const int TASKBAR_H = 28;
static const char* DISCORD_INVITE = "https://discord.gg/BnPjBWZMaS";


bool Desktop::init(Tileset* ts, Font* font, GameData* gd, Game* g, GameSettings* s, PlatformRequests* r, const std::string& saveDirectory, const std::string& iconPath) {
    ctx.ts = ts; ctx.font = font; ctx.gd = gd; game = g; settings = s; req = r; saveDir = saveDirectory;
    if (!icons.load(iconPath)) return false;
    iconDefs = {
        {"New Game", 0, 7, "new"},
        {"Load Game", 4, 0, "load"},
        {"Settings", 4, 3, "settings"},
        {"Discord", 1, 6, "discord"},
        {"Readme.txt", 2, 3, "readme"},
#ifndef TRIFFALL_NO_EDITOR
        {"Editor", 0, 8, "editor"},
#endif
        {"Shut Down", 0, 0, "shutdown"},
        {"My Computer", 1, 12, "mycomputer"},
        {"Minesweeper", 0, 14, "minesweeper"},
        {"Solitaire", 1, 4, "solitaire"},
        {"Achievements", 4, 4, "achievements"},
    };
    // demo build: the full game's store page (1.3.0: only once demo.txt's store= names one - no icon that opens nothing)
    if (game && game->demoMode && !game->storeUrl.empty()) iconDefs.push_back({"Wishlist Triffall", 0, 7, "wishlist"});
    fixedIcons = iconDefs.size();
    { size_t p = iconPath.rfind('/'); assetDir = p == std::string::npos ? "assets" : iconPath.substr(0, p); }
    for (auto& d : iconDefs) { d.dc = d.ic; d.dr = d.ir; }
    std::string desktopConfig;
    if (readFile(saveDir + "/desktop.ini", desktopConfig)) for (const auto& sec : parseConfig(desktopConfig)) {
        if (sec.name == "desktop") wallpaper = clampi(sec.geti("wallpaper", 0), 0, 3);
        if (sec.name == "icons") for (auto& kv : sec.kv) {   // pos.<action>=x,y  icon.<action>=col,row
            auto p = split(kv.second, ','); if (p.size() != 2) continue; std::array<int, 2> v{atoi(p[0].c_str()), atoi(p[1].c_str())};
            if (startsWith(kv.first, "pos.")) savedPos[kv.first.substr(4)] = v; else if (startsWith(kv.first, "icon.")) customIcon[kv.first.substr(5)] = v;
        }
    }
    refreshCampaignIcons();
    refreshSlots();
    return true;
}

int Desktop::iconAt(int x, int y) const {
    for (size_t i = 0; i < iconPositions.size(); i++) {
        int ix = iconPositions[i][0], iy = iconPositions[i][1];
        if (x >= ix - 4 && x < ix + 72 && y >= iy && y < iy + 54) return (int)i;
    }
    return -1;
}

bool Desktop::snapIconCursor(int dx, int dy) {
    int current = iconAt(cursorX, cursorY);
    if (current < 0) current = selIcon;
    int sx = current >= 0 ? iconPositions[current][0] + 34 : cursorX;
    int sy = current >= 0 ? iconPositions[current][1] + 20 : cursorY;
    int best = -1; float bestScore = 1e9f;
    for (size_t i = 0; i < iconPositions.size(); i++) {
        if ((int)i == current) continue;
        int cx = iconPositions[i][0] + 34, cy = iconPositions[i][1] + 20;
        int ox = cx - sx, oy = cy - sy;
        int along = dx * ox + dy * oy; if (along <= 0) continue;
        int across = abs(dx ? oy : ox);
        bool inLine = dx ? (sy >= iconPositions[i][1] - 2 && sy < iconPositions[i][1] + 56)
                         : (sx >= iconPositions[i][0] - 2 && sx < iconPositions[i][0] + 78);
        float score = (float)along + (inLine ? 0.0f : across * 2.5f + 40.0f);
        if (score < bestScore) { bestScore = score; best = (int)i; }
    }
    if (best < 0) return false;
    cursorX = iconPositions[best][0] + 34; cursorY = iconPositions[best][1] + 20;
    return true;
}

void Desktop::setIconPosition(int index, int x, int y) {
    if (index < 0 || index >= (int)iconPositions.size()) return;
    iconPositions[index] = {clampi(x, 0, UI::SCREEN_W - 76), clampi(y, 0, UI::SCREEN_H - TASKBAR_H - 54)};
}

// 1.2.2 (#39): the screen changed size (window resized, fullscreen, Classic 4:3): keep icons, windows and the pointer on it
void Desktop::screenChanged() {
    for (auto& p : iconPositions) if (p[0] >= 0) { p[0] = clampi(p[0], 0, UI::SCREEN_W - 76); p[1] = clampi(p[1], 0, UI::SCREEN_H - TASKBAR_H - 54); }
    for (auto& w : windows) { w.x = clampi(w.x, 0, std::max(0, UI::SCREEN_W - w.w)); w.y = clampi(w.y, 0, std::max(0, UI::SCREEN_H - TASKBAR_H - w.h)); }
    cursorX = clampi(cursorX, 0, UI::SCREEN_W - 1); cursorY = clampi(cursorY, 0, UI::SCREEN_H - 1);
}

void Desktop::arrangeIcons() {
    for (size_t i = 0; i < iconPositions.size(); i++) iconPositions[i] = {16 + (int)(i / 8) * 84, 8 + (int)(i % 8) * 54};
    saveDesktopSettings(); showToast("Icons arranged.");
}

void Desktop::saveDesktopSettings() {
    for (size_t i = 0; i < iconDefs.size() && i < iconPositions.size(); i++) savedPos[iconDefs[i].action] = iconPositions[i];
    std::string o = fmt("[desktop]\nwallpaper=%d\n\n[icons]\n", wallpaper);
    for (auto& kv : savedPos) o += fmt("pos.%s=%d,%d\n", kv.first.c_str(), kv.second[0], kv.second[1]);
    for (auto& kv : customIcon) o += fmt("icon.%s=%d,%d\n", kv.first.c_str(), kv.second[0], kv.second[1]);
    writeFile(saveDir + "/desktop.ini", o);
}
void Desktop::iconCell(const std::string& action, int& c, int& r) const {
    for (auto& d : iconDefs) if (d.action == action) { c = d.ic; r = d.ir; return; }
}
void Desktop::setIconPicture(int index, int c, int r) {
    if (index < 0 || index >= (int)iconDefs.size()) return;
    IconDef& d = iconDefs[index];
    if (c < 0) { customIcon.erase(d.action); d.ic = d.dc; d.ir = d.dr; } else { customIcon[d.action] = {c, r}; d.ic = c; d.ir = r; }
    saveDesktopSettings();
}

void Desktop::contextAction(int action) {
    contextOpen = false;
    if (contextIcon >= 0) {   // the menu of one icon
        int ic = contextIcon; contextIcon = -1; if (ic >= (int)iconDefs.size()) return;
        if (action == 0) runAction(iconDefs[ic].action);
        else if (action == 1) { openWindow("iconpick"); windows.back().text = iconDefs[ic].action; windows.back().title = "Change Icon - " + iconDefs[ic].label; }
        else { setIconPicture(ic, -1, -1); showToast("Icon reset."); }
        return;
    }
    if (action == 0) arrangeIcons();
    else if (action == 1) { refreshSlots(); refreshCampaignIcons(); showToast("Desktop refreshed."); }
    else if (action == 2) openWindow("properties");
}

void Desktop::drawWallpaper(Canvas& cv) {
    static const Color8 bases[] = {DESK, RGB(0, 24, 72), RGB(48, 12, 58), RGB(35, 55, 42)};
    static const Color8 accents[] = {RGB(0, 128, 128), RGB(0, 42, 112), RGB(74, 22, 86), RGB(44, 68, 50)};
    cv.clear(bases[clampi(wallpaper, 0, 3)]);
    if (wallpaper == 1) {
        for (int y = 20; y < UI::SCREEN_H - TASKBAR_H; y += 40) cv.hline(0, y, UI::SCREEN_W, accents[1]);
    } else if (wallpaper == 2) {
        for (int y = 8; y < UI::SCREEN_H - TASKBAR_H; y += 32) for (int x = (y / 32 % 2) * 16; x < UI::SCREEN_W; x += 32) cv.fillRect(x, y, 8, 8, accents[2]);
    } else if (wallpaper == 3) {
        for (int x = 16; x < UI::SCREEN_W; x += 32) cv.vline(x, 0, UI::SCREEN_H - TASKBAR_H, accents[3]);
        for (int y = 16; y < UI::SCREEN_H - TASKBAR_H; y += 32) cv.hline(0, y, UI::SCREEN_W, accents[3]);
    }
}

void Desktop::dropIcon(int index) {
    if (index < 0 || index >= (int)iconPositions.size()) return;
    const int cols = (UI::SCREEN_W - 16 - 76) / 84 + 1;
    const int rows = (UI::SCREEN_H - TASKBAR_H - 8 - 54) / 54 + 1;
    int wantCol = clampi((iconPositions[index][0] - 16 + 42) / 84, 0, cols - 1);
    int wantRow = clampi((iconPositions[index][1] - 8 + 27) / 54, 0, rows - 1);
    int bestCol = wantCol, bestRow = wantRow, bestScore = 0x7fffffff;
    for (int row = 0; row < rows; row++) for (int col = 0; col < cols; col++) {
        bool occupied = false;
        for (size_t i = 0; i < iconPositions.size(); i++) if ((int)i != index && iconPositions[i][0] == 16 + col * 84 && iconPositions[i][1] == 8 + row * 54) occupied = true;
        if (occupied) continue;
        int dx = col - wantCol, dy = row - wantRow, score = dx * dx + dy * dy;
        if (score < bestScore) { bestScore = score; bestCol = col; bestRow = row; }
    }
    setIconPosition(index, 16 + bestCol * 84, 8 + bestRow * 54);
    saveDesktopSettings();   // the icon stays where it was put, next launch too
}

void Desktop::refreshSlots() {
    for (int i = 0; i < 4; i++) {
        slots[i] = SaveSlotInfo();
        std::string t; if (!readFile(slotPath(i), t)) continue;
        auto secs = parseConfig(t); if (secs.empty() || secs[0].name != "game") continue;
        slots[i] = readSaveSlotInfo(slotPath(i)); continue;
        slots[i].exists = true; slots[i].hero = secs[0].get("hero", "?"); slots[i].level = secs[0].geti("level", 1); slots[i].playtime = secs[0].geti("playtime", 0); slots[i].mapName = secs[0].get("mapname", secs[0].get("map"));
    }
}

bool Desktop::loadSlot(int slot) { return loadFile(slotPath(slot)); }
bool Desktop::loadFile(const std::string& path) {
    if (!game->loadSaveFile(path)) { showDialog("Load Game", game->loadError.empty() ? "Save file is corrupt." : game->loadError); return false; }
    launchGame = true; windows.clear(); startOpen = false;
    return true;
}
std::string Desktop::discordUrl() const { return settings && !settings->discord.empty() ? settings->discord : DISCORD_INVITE; }
bool Desktop::firstRun() const { return !fileExists(saveDir + "/installed.ini"); }
void Desktop::markInstalled() { writeFile(saveDir + "/installed.ini", std::string("[install]\nversion=") + TRIFFALL_VERSION + "\n"); }
void Desktop::offerCrashRecovery() {
    // the recovery save is offered once: it is moved aside first, so a later unclean exit can't offer it again as new
    std::string rec = saveDir + "/crash_recovery_last.sav";
    { std::error_code ec; std::filesystem::remove(rec, ec); std::filesystem::rename(saveDir + "/crash_recovery.sav", rec, ec); }
    SaveSlotInfo si = readSaveSlotInfo(rec);
    if (!si.exists) { showDialog("Triffall.exe", "Triffall closed unexpectedly last time. A report was written to saves/crash.log - please send it to the developer. Your autosave is in Load Game."); return; }
    showDialog("Triffall.exe", fmt("Triffall closed unexpectedly last time (report: saves/crash.log). Restore %s (Lv %d) at %s?", si.hero.c_str(), si.level, si.mapName.c_str()), [this, rec]() { loadFile(rec); }, true);
}

void Desktop::drawIcon(Canvas& cv, int ic, int ir, int x, int y, int scale) { cv.blit(icons, 32 + ic * 32, 32 + ir * 32, 32, 32, x, y, scale); }

void Desktop::bevelBox(Canvas& cv, int x, int y, int w, int h, bool sunken) {
    cv.fillRect(x, y, w, h, FACE);
    Color8 tl = sunken ? SHADOW : HILITE, br = sunken ? HILITE : DKSHADOW, tl2 = sunken ? DKSHADOW : FACE, br2 = sunken ? FACE : SHADOW;
    cv.hline(x, y, w, tl); cv.vline(x, y, h, tl);
    cv.hline(x, y + h - 1, w, br); cv.vline(x + w - 1, y, h, br);
    cv.hline(x + 1, y + 1, w - 2, tl2); cv.vline(x + 1, y + 1, h - 2, tl2);
    cv.hline(x + 1, y + h - 2, w - 2, br2); cv.vline(x + w - 2, y + 1, h - 2, br2);
}

bool Desktop::win98Button(Canvas& cv, InputState& in, int x, int y, int w, int h, const std::string& label, bool enabled, bool focus, bool active) {
    bool hover = active && enabled && in.mouseX >= x && in.mouseY >= y && in.mouseX < x + w && in.mouseY < y + h && !mouseUsed;
    if (active && enabled) hot(x, y, w, h);
    bool pressed = hover && in.mouseDown[0];
    bevelBox(cv, x, y, w, h, pressed);
    if (hover && (in.gamepadActive || padNavWindows)) cv.rect(x + 1, y + 1, w - 2, h - 2, RGB(0, 0, 128));
    if (focus) cv.rect(x + 3, y + 3, w - 6, h - 6, RGB(0, 0, 0, 90));
    int tw = ctx.font->width(label);
    ctx.font->draw(cv, *ctx.ts, x + (w - tw) / 2 + (pressed ? 1 : 0), y + (h - 12) / 2 + (pressed ? 1 : 0), label, enabled ? DKSHADOW : SHADOW);
    if (!enabled) ctx.font->draw(cv, *ctx.ts, x + (w - tw) / 2 + 1, y + (h - 12) / 2 + 1, label, HILITE);
    bool clicked = hover && in.mousePressed[0];
    if (clicked) mouseUsed = true;
    return clicked;
}

bool Desktop::checkbox(Canvas& cv, InputState& in, int x, int y, const std::string& label, bool& value, bool active) {
    bevelBox(cv, x, y, 13, 13, true); cv.fillRect(x + 2, y + 2, 9, 9, HILITE);
    if (value) { cv.line(x + 3, y + 6, x + 5, y + 8, DKSHADOW); cv.line(x + 5, y + 8, x + 9, y + 4, DKSHADOW); cv.line(x + 3, y + 5, x + 5, y + 7, DKSHADOW); cv.line(x + 5, y + 7, x + 9, y + 3, DKSHADOW); }
    ctx.font->draw(cv, *ctx.ts, x + 18, y, label, DKSHADOW);
    int w = 18 + ctx.font->width(label);
    if (active) hot(x, y, w, 13);
    bool hover = active && in.mouseX >= x && in.mouseY >= y && in.mouseX < x + w && in.mouseY < y + 13 && !mouseUsed;
    if (hover && (in.gamepadActive || padNavWindows)) cv.rect(x - 2, y - 2, w + 4, 17, RGB(0, 0, 128));
    if (hover && in.mousePressed[0]) { value = !value; mouseUsed = true; return true; }
    return false;
}

void Desktop::slider(Canvas& cv, InputState& in, int x, int y, int w, const std::string& label, int& value, int lo, int hi, bool active) {
    ctx.font->draw(cv, *ctx.ts, x, y, label, DKSHADOW);
    int sx = x + 110, sy = y + 4;
    bevelBox(cv, sx, sy, w, 6, true);
    int kx = sx + (w - 10) * (value - lo) / std::max(1, hi - lo);
    bevelBox(cv, kx, sy - 5, 10, 16, false);
    ctx.font->draw(cv, *ctx.ts, sx + w + 8, y, std::to_string(value), DKSHADOW);
    bool hover = active && !mouseUsed && in.mouseX >= sx - 4 && in.mouseX < sx + w + 4 && in.mouseY >= sy - 6 && in.mouseY < sy + 12;
    if (active) { hot(sx - 4, sy - 6, w + 8, 18); sliderRects.push_back({sx - 4, sy - 6, w + 8, 18}); }
    if (hover && (in.gamepadActive || padNavWindows)) {
        // left / right: one step per press, then a steady repeat while held (so any exact value can be set)
        cv.rect(sx - 6, sy - 8, w + 12, 22, RGB(0, 0, 128));
        int d = 0;
        if (actP(in, A_LEFT)) { d = -1; sliderRepeat = 18; } else if (actP(in, A_RIGHT)) { d = 1; sliderRepeat = 18; }
        else if (actD(in, A_LEFT) || actD(in, A_RIGHT)) { if (--sliderRepeat <= 0) { d = actD(in, A_LEFT) ? -1 : 1; sliderRepeat = 3; } }
        if (d) value = clampi(value + d, lo, hi);
    }
    if (hover && in.wheel) value = clampi(value + in.wheel, lo, hi);
    // dragging is for a real mouse: a controller / keyboard "click" on the slider must not jump the value to the cursor
    if (hover && in.mouseDown[0] && !in.gamepadActive && !(padNavWindows && actD(in, A_CONFIRM))) { value = clampi(lo + (in.mouseX - sx) * (hi - lo) / std::max(1, w - 10), lo, hi); }
}

void Desktop::openWindow(const std::string& type) {
    for (size_t i = 0; i < windows.size(); i++) if (windows[i].type == type) { DeskWindow w = windows[i]; windows.erase(windows.begin() + i); windows.push_back(w); return; }
    DeskWindow w; w.type = type;
    int n = (int)windows.size();
    if (type == "new") { w.title = "New Game - Triffall"; w.w = 420; w.h = 386; w.text = gPlatform->userName().substr(0, 14); w.page = 1; }
    else if (type == "load") { w.title = "Load Game"; w.w = 380; w.h = 274; }
    else if (type == "settings") { w.title = "Settings"; w.w = 420; w.h = 420; }
    else if (type == "readme") { w.title = "Readme.txt - Notepad"; w.w = 460; w.h = 340; }
    else if (type == "discord") { w.title = "Discord"; w.w = 320; w.h = 150; }
    else if (type == "about") { w.title = "About Triffall"; w.w = 320; w.h = 232; }
    else if (type == "setup") { w.title = "Triffall Setup"; w.w = 420; w.h = 300; w.sel = 1; }
    else if (type == "properties") { w.title = "Display Properties"; w.w = 360; w.h = 250; }
    else if (type == "iconpick") { w.title = "Change Icon"; w.w = 12 * 36 + 28; w.h = 9 * 36 + 76; }
    else if (type == "mycomputer") { w.title = "My Computer"; w.w = 460; w.h = 320; w.sel = -1; }
    else if (type == "minesweeper") { w.title = "Minesweeper"; w.w = 168; w.h = 220; }
    else if (type == "solitaire") { w.title = "Solitaire"; w.w = 286; w.h = 340; }
    else if (type == "achievements") { w.title = "Achievements"; w.w = 420; w.h = 330; }
    else { w.title = type; w.w = 300; w.h = 150; }
    w.x = (UI::SCREEN_W - w.w) / 2 + n * 12; w.y = (UI::SCREEN_H - TASKBAR_H - w.h) / 2 + n * 12;
    windows.push_back(w);
    startOpen = false;
    gAudio.play("desktop_open");
}

void Desktop::showDialog(const std::string& title, const std::string& text, std::function<void()> onYes, bool yesNo) {
    DeskWindow w; w.type = yesNo ? "yesno" : "msg"; w.title = title; w.text = text; w.onYes = onYes;
    auto lines = ctx.font->wrap(text, 280);
    w.w = 330; w.h = 90 + (int)lines.size() * 13; w.x = (UI::SCREEN_W - w.w) / 2; w.y = (UI::SCREEN_H - TASKBAR_H - w.h) / 2;
    windows.push_back(w); gAudio.play(yesNo ? "desktop_open" : "ui_error");
}

void Desktop::refreshCampaignIcons() {
    // remember where every icon is now (by action), rebuild the list, then put each icon back: where it is, where it was
    // saved, or (a new campaign) in the first free spot of the grid
    std::map<std::string, std::array<int, 2>> now = savedPos;
    for (size_t i = 0; i < iconDefs.size() && i < iconPositions.size(); i++) now[iconDefs[i].action] = iconPositions[i];
    iconDefs.resize(fixedIcons);
    for (auto& c : listCampaigns(assetDir)) if (c.id != "triffall") { IconDef d{c.name, c.iconC, c.iconR, "campaign:" + c.id}; d.dc = d.ic; d.dr = d.ir; iconDefs.push_back(d); }
    for (auto& d : iconDefs) { auto it = customIcon.find(d.action); if (it != customIcon.end()) { d.ic = it->second[0]; d.ir = it->second[1]; } else { d.ic = d.dc; d.ir = d.dr; } }
    iconPositions.assign(iconDefs.size(), {-1, -1});
    for (size_t i = 0; i < iconDefs.size(); i++) { auto it = now.find(iconDefs[i].action); if (it != now.end()) iconPositions[i] = {std::max(0, it->second[0]), std::max(0, it->second[1])}; }   // 1.2.2: kept where they were (a wide screen may show them); screenChanged pulls them into view
    for (size_t i = 0; i < iconDefs.size(); i++) {
        if (iconPositions[i][0] >= 0) continue;
        for (int slot = 0; slot < 64; slot++) {
            std::array<int, 2> p{16 + (slot / 8) * 84, 8 + (slot % 8) * 54}; bool used = false;
            for (size_t j = 0; j < iconDefs.size(); j++) if (j != i && iconPositions[j][0] >= 0 && abs(iconPositions[j][0] - p[0]) < 60 && abs(iconPositions[j][1] - p[1]) < 40) used = true;
            if (!used) { iconPositions[i] = p; break; }
        }
        if (iconPositions[i][0] < 0) iconPositions[i] = {16, 8};
    }
}
// New Game for a campaign: its data is loaded right away so the class list and description match it
void Desktop::openNewGame(const std::string& id) {
    auto go = [this, id]() {
        game->running = false;
        if (!game->switchCampaign(id)) { showDialog("New Game", "That campaign could not be loaded."); return; }
        for (size_t i = 0; i < windows.size(); i++) if (windows[i].type == "new") { windows.erase(windows.begin() + i); break; }
        openWindow("new"); windows.back().campaign = id; windows.back().title = "New Game - " + game->campaign.name;
    };
    if (game->running) showDialog("New Game", "A game is already running. Starting a new game will discard unsaved progress. Continue?", go, true); else go();
}
void Desktop::runAction(const std::string& a) {
    startOpen = false;
    if (a == "new") openNewGame("triffall");
    else if (startsWith(a, "campaign:")) openNewGame(a.substr(9));
    else if (a == "load") openWindow("load");
    else if (a == "settings") openWindow("settings");
    else if (a == "wishlist") { if (req && game && !game->storeUrl.empty()) req->openURL = game->storeUrl; showToast("Opening the Triffall store page..."); }
    else if (a == "discord") { if (req) req->openURL = discordUrl(); openWindow("discord"); }
    else if (a == "readme") openWindow("readme");
    else if (a == "about") openWindow("about");
    else if (a == "mycomputer" || a == "minesweeper" || a == "solitaire" || a == "achievements") openWindow(a);
    else if (a == "worldmap") { if (!game->running) showDialog("World Map", "The map shows the world of a running game. Start or load a game first."); else { game->openWorldMap(); launchGame = true; } }
    else if (a == "editor") { launchEditor = true; windows.clear(); }
    else if (a == "resume") { if (game->running) launchGame = true; }
    else if (a == "shutdown") showDialog("Shut Down Windows", "Are you sure you want to shut down the computer? Unsaved progress will be lost.", [this]() { quit = true; }, true);
}

// ---------------- update ----------------
void Desktop::update(InputState& in, float dt, Canvas& cv) {
    ctx.cv = &cv; ctx.frame++; ctx.time += dt; clockT += dt; mouseUsed = false; bool clickInWin = false;
    if (toastT > 0) toastT -= dt;
    if (dblTimer > 0) dblTimer -= dt;
    // screensaver after 90s idle on the desktop
    { bool anyKey = false; for (int k = 0; k < K_COUNT; k++) if (in.pressed[k]) anyKey = true;
      bool input = anyKey || in.mouseDX || in.mouseDY || in.mousePressed[0] || in.mousePressed[1] || in.mousePressed[2] || in.wheel || fabsf(in.stickX) > 0.2f || fabsf(in.stickY) > 0.2f;
      if (screensaver) { if (input) { screensaver = false; idleT = 0; in.eatMouse(); in.eatKeys(); } ssT += dt; return; }
      idleT = input ? 0 : idleT + dt; if (idleT > 90) { screensaver = true; ssT = 0; return; } }
    // gamepad / keyboard cursor control
    hotsPrev = hots; hots.clear(); sliderRectsPrev = sliderRects; sliderRects.clear();
    // controller: a window that just opened puts the cursor on its first control; a window can also ask for a spot
    if (!windows.empty()) { DeskWindow& tw = windows.back();
        if (tw.focusX >= 0) { if (in.gamepadActive) { cursorX = tw.focusX; cursorY = tw.focusY; } tw.focusX = tw.focusY = -1; tw.focusFirst = false; }
        else if (tw.focusFirst && tw.timer > 0) { tw.focusFirst = false;
            if (in.gamepadActive) { int best = -1; for (size_t i = 0; i < hotsPrev.size(); i++) { const Rect& r = hotsPrev[i]; if (r.x < tw.x || r.y < tw.y + 22 || r.x >= tw.x + tw.w || r.y >= tw.y + tw.h) continue; if (best < 0 || r.y < hotsPrev[best].y - 2 || (abs(r.y - hotsPrev[best].y) <= 2 && r.x < hotsPrev[best].x)) best = (int)i; }
                if (best >= 0) { cursorX = hotsPrev[best].x + hotsPrev[best].w / 2; cursorY = hotsPrev[best].y + hotsPrev[best].h / 2; } } } }
    bool onSlider = false; for (auto& r : sliderRectsPrev) if (r.contains(cursorX, cursorY)) onSlider = true;
    bool bindingKey = !windows.empty() && windows.back().type == "settings" && windows.back().page == 1 && windows.back().sub >= 0;
    // keyboard users steer the same cursor with the arrow keys inside windows that have no list of their own
    std::string topType = windows.empty() ? "" : windows.back().type;
    bool kbNav = !in.gamepadActive && !windows.empty() && !startOpen && (topType == "settings" || topType == "about" || topType == "discord" || topType == "mycomputer" || topType == "achievements" || topType == "minesweeper" || topType == "solitaire");
    padNavWindows = (in.gamepadActive || kbNav) && !windows.empty();
    bool padActivated = false;   // the pad's confirm was consumed by a shortcut (icon / start menu), do not also click
    if (in.gamepadActive) {
        cursorX = clampi(cursorX + (int)(in.stickX * 320 * dt), 0, UI::SCREEN_W - 1); cursorY = clampi(cursorY + (int)(in.stickY * 220 * dt), 0, UI::SCREEN_H - 1);
        if (!windows.empty() && !startOpen && !bindingKey) { if (actP(in, A_UP)) snapCursor(0, -1); if (actP(in, A_DOWN)) snapCursor(0, 1); if (!onSlider) { if (actP(in, A_LEFT)) snapCursor(-1, 0); if (actP(in, A_RIGHT)) snapCursor(1, 0); } }
        // toys: X flags a Minesweeper cell (right click), Y sends a Solitaire card to the foundations
        if (!windows.empty() && !startOpen && (topType == "minesweeper" || topType == "solitaire") && actP(in, A_INTERACT)) { in.mousePressed[2] = true; }
        if (windows.empty() && !startOpen) {
            if (iconPress >= 0 && iconPressPad) {
                if (actP(in, A_UP)) cursorY -= 16; if (actP(in, A_DOWN)) cursorY += 16;
                if (actP(in, A_LEFT)) cursorX -= 16; if (actP(in, A_RIGHT)) cursorX += 16;
                cursorX = clampi(cursorX, 0, UI::SCREEN_W - 1); cursorY = clampi(cursorY, 0, UI::SCREEN_H - 1);
                if (iconDrag < 0 && (abs(cursorX - iconPressX) > 4 || abs(cursorY - iconPressY) > 4)) {
                    iconDrag = iconPress;
                    iconDragOffsetX = iconPressX - iconPositions[iconDrag][0]; iconDragOffsetY = iconPressY - iconPositions[iconDrag][1];
                    dblTimer = 0;
                }
                if (iconDrag >= 0) setIconPosition(iconDrag, cursorX - iconDragOffsetX, cursorY - iconDragOffsetY);
                if (in.released[K_ENTER] || in.released[K_PAD_A]) {
                    if (iconDrag < 0) runAction(iconDefs[iconPress].action); else dropIcon(iconDrag);
                    iconPress = iconDrag = -1; iconPressPad = false;
                }
                padActivated = true;
            } else {
                bool moved = false;
                bool stickMoving = fabsf(in.stickX) > 0.2f || fabsf(in.stickY) > 0.2f;
                if (stickMoving) {
                    int target = iconAt(cursorX, cursorY);
                    if (target >= 0 && target != selIcon) { selIcon = target; moved = true; }
                } else if (padOnStart && (actP(in, A_DOWN) || actP(in, A_LEFT) || actP(in, A_RIGHT))) {
                    // on the Start button: only up leads back to the icons
                } else if (selIcon < 0 && !padOnStart && (actP(in, A_UP) || actP(in, A_DOWN) || actP(in, A_LEFT) || actP(in, A_RIGHT))) {
                    selIcon = 0; cursorX = iconPositions[0][0] + 34; cursorY = iconPositions[0][1] + 20; moved = true;
                } else {
                    if (actP(in, A_UP)) moved = snapIconCursor(0, -1) || moved;
                    if (actP(in, A_DOWN)) moved = snapIconCursor(0, 1) || moved;
                    if (actP(in, A_LEFT)) moved = snapIconCursor(-1, 0) || moved;
                    if (actP(in, A_RIGHT)) moved = snapIconCursor(1, 0) || moved;
                    if (moved) { int target = iconAt(cursorX, cursorY); if (target >= 0) selIcon = target; padOnStart = false; }
                    // past the bottom of the icons: down to the Start button (A opens the menu)
                    if (!moved && actP(in, A_DOWN) && !padOnStart) { cursorX = 2 + startBtnW / 2; cursorY = UI::SCREEN_H - TASKBAR_H / 2; selIcon = -1; padOnStart = true; moved = true; }
                }
                if (moved) gAudio.play("ui_move");
                if (actP(in, A_CONFIRM)) {
                    int target = iconAt(cursorX, cursorY); if (target < 0) target = selIcon;
                    if (target >= 0) { selIcon = target; iconPress = target; iconDrag = -1; iconPressPad = true; iconPressX = cursorX; iconPressY = cursorY; padActivated = true; }
                }
            }
        }
        in.mouseX = cursorX; in.mouseY = cursorY;
        if (!bindingKey && !padActivated && !startOpen) { if (actP(in, A_CONFIRM)) { in.mousePressed[0] = true; in.mouseDown[0] = true; } if (actD(in, A_CONFIRM)) in.mouseDown[0] = true; }
        if (in.released[K_ENTER] || in.released[K_PAD_A]) in.mouseReleased[0] = true;
    } else if (kbNav) {
        bool hopped = false;
        if (actP(in, A_UP)) hopped = snapCursor(0, -1); if (actP(in, A_DOWN)) hopped = snapCursor(0, 1) || hopped; if (!onSlider) { if (actP(in, A_LEFT)) hopped = snapCursor(-1, 0) || hopped; if (actP(in, A_RIGHT)) hopped = snapCursor(1, 0) || hopped; }
        if ((topType == "minesweeper" || topType == "solitaire") && in.pressed[K_F]) in.mousePressed[2] = true;   // keyboard: F flags / sends up
        if (hopped) gAudio.play("ui_move");
        bool onHot = false; for (auto& r : hotsPrev) if (r.contains(cursorX, cursorY)) onHot = true;
        if (in.mouseX != cursorX || in.mouseY != cursorY) { cursorX = in.mouseX; cursorY = in.mouseY; }
        else { in.mouseX = cursorX; in.mouseY = cursorY; if (onHot && !bindingKey && actP(in, A_CONFIRM) && !in.typed.size()) { in.mousePressed[0] = true; in.mouseDown[0] = true; in.eatKeys(); } }
    }
    else { cursorX = in.mouseX; cursorY = in.mouseY; }
    if (actP(in, A_CANCEL) && !bindingKey) {
        if (startOpen) startOpen = false;
        else if (!windows.empty() && windows.back().type == "new" && windows.back().sub == 2) windows.back().sub = 0;   // close the on-screen keyboard first
        else if (!windows.empty() && windows.back().type == "solitaire" && sol.selPile >= 0) { sol.selPile = -1; sol.sel = -1; }
        else if (!windows.empty()) windows.back().wantClose = true; else if (game->running) launchGame = true; }
    if (bindingKey) { if (in.pressed[K_ESCAPE]) { /* handled by the window */ } }
    // taskbar / start button
    int sbW = startBtnW;
    bool overStart = in.mouseX >= 2 && in.mouseX < 2 + sbW && in.mouseY >= UI::SCREEN_H - TASKBAR_H + 3 && in.mouseY < UI::SCREEN_H - 3;
    bool justOpened = false;
    if (in.mousePressed[0] && overStart) { startOpen = !startOpen; mouseUsed = true; gAudio.play("desktop_click"); if (startOpen) { startSel = 0; justOpened = true; } }
    if (startOpen) padOnStart = false;
    if (actP(in, A_MENU) && !bindingKey) { contextOpen = false; startOpen = !startOpen; if (startOpen) { startSel = 0; justOpened = true; } gAudio.play("desktop_click"); in.eatKeys(); }
    if (contextOpen) {
        const int rowTop[] = {contextY + 4, contextY + 26, contextY + 55};
        if (in.mouseDX || in.mouseDY) for (int i = 0; i < 3; i++) if (in.mouseX >= contextX + 4 && in.mouseX < contextX + 164 && in.mouseY >= rowTop[i] && in.mouseY < rowTop[i] + 22) contextSel = i;
        if (actP(in, A_UP)) contextSel = (contextSel + 2) % 3;
        if (actP(in, A_DOWN)) contextSel = (contextSel + 1) % 3;
        if (in.mousePressed[2] || actP(in, A_CANCEL)) { contextOpen = false; contextIcon = -1; in.eatKeys(); in.eatMouse(); return; }
        if (in.mousePressed[0]) {
            int hit = -1; for (int i = 0; i < 3; i++) if (in.mouseX >= contextX + 4 && in.mouseX < contextX + 164 && in.mouseY >= rowTop[i] && in.mouseY < rowTop[i] + 22) hit = i;
            in.eatMouse(); if (hit >= 0) contextAction(hit); else { contextOpen = false; contextIcon = -1; } return;
        }
        if (actP(in, A_CONFIRM)) { contextAction(contextSel); in.eatKeys(); return; }
        return;
    }
    if (startOpen) {
        std::vector<std::pair<std::string, std::string>> items = {{"Resume Triffall", "resume"}, {"New Game", "new"}, {"Load Game", "load"}, {"Settings", "settings"}, {"Discord", "discord"}, {"Readme", "readme"}, {"Editor", "editor"}, {"My Computer", "mycomputer"}, {"Minesweeper", "minesweeper"}, {"Solitaire", "solitaire"}, {"Achievements", "achievements"}, {"About", "about"}, {"Shut Down...", "shutdown"}};
    #ifdef TRIFFALL_NO_EDITOR
        items.erase(std::remove_if(items.begin(), items.end(), [](const std::pair<std::string, std::string>& p) { return p.second == "editor"; }), items.end());
#endif
        int mx = 2, mh = (int)items.size() * 22 + 8, my = UI::SCREEN_H - TASKBAR_H - mh;
        int n = (int)items.size();
        // the menu keeps the controller in: up/down wrap around it, left/right do nothing; Start or Back closes it
        if (actP(in, A_UP)) startSel = (startSel + n - 1) % n; if (actP(in, A_DOWN)) startSel = (startSel + 1) % n;
        startSel = clampi(startSel, 0, n - 1);
        for (int i = 0; i < n; i++) { int iy = my + 4 + i * 22; if (in.mouseX >= mx + 24 && in.mouseX < mx + 180 && in.mouseY >= iy && in.mouseY < iy + 22) { if (in.mouseDX || in.mouseDY) startSel = i; if (in.mousePressed[0] && !justOpened) { mouseUsed = true; runAction(items[i].second); return; } } }
        if (in.gamepadActive && (justOpened || actP(in, A_UP) || actP(in, A_DOWN))) { cursorX = mx + 100; cursorY = my + 4 + startSel * 22 + 11; }
        if (actP(in, A_CONFIRM) && !justOpened) { runAction(items[startSel].second); return; }
        if (in.mousePressed[0] && !overStart && !(in.mouseX >= mx && in.mouseX < mx + 180 && in.mouseY >= my && in.mouseY < my + mh)) startOpen = false;
        if (startOpen) { mouseUsed = true; }
    }
    // taskbar app button (resume game)
    if (game->running) { int bx = 2 + sbW + 6; if (in.mousePressed[0] && in.mouseX >= bx && in.mouseX < bx + 160 && in.mouseY >= UI::SCREEN_H - TASKBAR_H + 3 && in.mouseY < UI::SCREEN_H - 3) { launchGame = true; mouseUsed = true; } }
    // windows: topmost gets input
    for (int i = (int)windows.size() - 1; i >= 0; i--) {
        bool active = i == activeWindow();
        updateWindow(windows[i], in, dt, active);
        if (active && in.mousePressed[0] && mouseInWindow(in, windows[i])) clickInWin = true;
    }
    // clicking a non-active window brings it to front
    if (in.mousePressed[0] && !mouseUsed && !clickInWin) {
        for (int i = (int)windows.size() - 1; i >= 0; i--) if (mouseInWindow(in, windows[i])) { DeskWindow w = windows[i]; windows.erase(windows.begin() + i); windows.push_back(w); mouseUsed = true; break; }
    }
    for (size_t i = 0; i < windows.size(); i++) if (windows[i].wantClose) { windows.erase(windows.begin() + i); i--; gAudio.play("desktop_close"); }
    // desktop icons
    // nothing behind an open menu, a window or the taskbar reacts to the pointer
    bool overAnyWindow = false; for (const auto& w : windows) if (mouseInWindow(in, w)) { overAnyWindow = true; break; }
    bool overTaskbar = in.mouseY >= UI::SCREEN_H - TASKBAR_H;
    hoverIcon = (startOpen || contextOpen || overAnyWindow || overTaskbar) ? -1 : iconAt(in.mouseX, in.mouseY);
    if (iconPress >= 0 && !iconPressPad) {
        if (in.mouseDown[0]) {
            if (iconDrag < 0 && (abs(in.mouseX - iconPressX) > 4 || abs(in.mouseY - iconPressY) > 4)) {
                iconDrag = iconPress;
                iconDragOffsetX = iconPressX - iconPositions[iconDrag][0]; iconDragOffsetY = iconPressY - iconPositions[iconDrag][1];
                dblTimer = 0;
            }
            if (iconDrag >= 0) { setIconPosition(iconDrag, in.mouseX - iconDragOffsetX, in.mouseY - iconDragOffsetY); hoverIcon = iconDrag; mouseUsed = true; }
        } else if (in.mouseReleased[0]) {
            if (iconDrag >= 0) { setIconPosition(iconDrag, in.mouseX - iconDragOffsetX, in.mouseY - iconDragOffsetY); dropIcon(iconDrag); }
            else if (dblTimer > 0 && dblIcon == iconPress) { runAction(iconDefs[iconPress].action); dblTimer = 0; }
            else { dblTimer = 0.4f; dblIcon = iconPress; gAudio.play("desktop_click"); }
            selIcon = iconPress; iconPress = iconDrag = -1; mouseUsed = true;
        }
    }
    if (windows.empty() && !startOpen && !in.gamepadActive && !kbNav) {
        bool moved = false;
        if (selIcon < 0 && (actP(in, A_UP) || actP(in, A_DOWN) || actP(in, A_LEFT) || actP(in, A_RIGHT))) {
            selIcon = 0; cursorX = iconPositions[0][0] + 34; cursorY = iconPositions[0][1] + 20; moved = true;
        } else {
            if (actP(in, A_UP)) moved = snapIconCursor(0, -1) || moved;
            if (actP(in, A_DOWN)) moved = snapIconCursor(0, 1) || moved;
            if (actP(in, A_LEFT)) moved = snapIconCursor(-1, 0) || moved;
            if (actP(in, A_RIGHT)) moved = snapIconCursor(1, 0) || moved;
            if (moved) { int target = iconAt(cursorX, cursorY); if (target >= 0) selIcon = target; }
        }
        if (moved) gAudio.play("ui_move");
        if (actP(in, A_CONFIRM) && selIcon >= 0 && !in.gamepadActive && !kbNav) runAction(iconDefs[selIcon].action);
    }
    if (in.mousePressed[0] && !mouseUsed && !clickInWin) {
        if (hoverIcon >= 0) {
            iconPress = hoverIcon; iconDrag = -1; iconPressPad = false; iconPressX = in.mouseX; iconPressY = in.mouseY; selIcon = hoverIcon;
            mouseUsed = true;
        } else selIcon = -1;
    }
    // right click on an icon (or E / pad X on the selected one): Open, Change Icon..., Default Icon
    auto openIconMenu = [&](int ic) { contextOpen = true; contextSel = 0; contextIcon = ic; selIcon = ic;
        contextX = clampi(iconPositions[ic][0] + 40, 0, UI::SCREEN_W - 172); contextY = clampi(iconPositions[ic][1] + 20, 0, UI::SCREEN_H - TASKBAR_H - 82); mouseUsed = true; };
    if (in.mousePressed[2] && hoverIcon >= 0 && !startOpen) { openIconMenu(hoverIcon); contextX = clampi(in.mouseX, 0, UI::SCREEN_W - 172); contextY = clampi(in.mouseY, 0, UI::SCREEN_H - TASKBAR_H - 82); }
    else if (windows.empty() && !startOpen && selIcon >= 0 && iconPress < 0 && actP(in, A_INTERACT)) { openIconMenu(selIcon); in.eatKeys(); }
    else if (in.mousePressed[2] && !startOpen && !mouseUsed) {
        bool overWindow = false; for (const auto& w : windows) if (mouseInWindow(in, w)) { overWindow = true; break; }
        if (!overWindow && !overStart) {
            contextOpen = true; contextSel = 0; contextIcon = -1;
            contextX = clampi(in.mouseX, 0, UI::SCREEN_W - 172);
            contextY = clampi(in.mouseY, 0, UI::SCREEN_H - TASKBAR_H - 82);
            mouseUsed = true;
        }
    }
    // shortcuts
    if (in.pressed[K_F1]) openWindow("readme");
    if (in.pressed[K_F9]) openWindow("load");
}

void Desktop::updateWindow(DeskWindow& w, InputState& in, float dt, bool active) {
    (void)dt;
    w.timer += dt;
    if (!active) return;
    // drag
    bool overTitle = in.mouseX >= w.x + 3 && in.mouseX < w.x + w.w - 22 && in.mouseY >= w.y + 3 && in.mouseY < w.y + 21;
    if (in.mousePressed[0] && overTitle) { w.dragging = true; w.dragDX = in.mouseX - w.x; w.dragDY = in.mouseY - w.y; mouseUsed = true; }
    if (w.dragging) { if (!in.mouseDown[0]) w.dragging = false; else { w.x = clampi(in.mouseX - w.dragDX, -w.w + 40, UI::SCREEN_W - 40); w.y = clampi(in.mouseY - w.dragDY, 0, UI::SCREEN_H - TASKBAR_H - 20); } }
    // close button
    int cx = w.x + w.w - 20, cy = w.y + 5;
    if (in.mousePressed[0] && in.mouseX >= cx && in.mouseX < cx + 16 && in.mouseY >= cy && in.mouseY < cy + 14) { w.wantClose = true; mouseUsed = true; return; }
    // keyboard for text windows
    if (w.type == "new") {
        if (w.sub == 3) { std::string t; bool cancelled = false; if (gPlatform->pollTextInput(t, cancelled)) { if (!cancelled) { std::string clean; for (char c : t) if (clean.size() < 14 && (isalnum((unsigned char)c) || c == ' ' || c == '-' || c == '\'')) clean += c; w.text = clean; } w.sub = 1; } return; }
        if (in.pressed[K_TAB]) w.sub = w.sub == 0 ? 1 : 0;
        if (w.sub == 0 || w.sub == 2) { // name field focused (sub 2 = the on-screen keyboard is open too)
            for (char c : in.typed) if (w.text.size() < 14 && (isalnum((unsigned char)c) || c == ' ' || c == '-' || c == '\'')) w.text += c;
            if (in.pressed[K_BACKSPACE] && !w.text.empty()) { if (in.ctrl()) { size_t n = w.text.size(); while (n > 0 && w.text[n - 1] == ' ') n--; while (n > 0 && w.text[n - 1] != ' ') n--; w.text.resize(n); } else w.text.pop_back(); }   // 1.1.11: Ctrl+Backspace = a word
        }
        int nClasses = (int)ctx.gd->classOrder.size();
        if (w.sub == 1 && !in.gamepadActive) {
            if (actP(in, A_UP)) w.sel = (w.sel + nClasses - 1) % std::max(1, nClasses);
            if (actP(in, A_DOWN)) w.sel = (w.sel + 1) % std::max(1, nClasses);
        }
        // only Enter starts from the keyboard: Space is a letter in a name
        if (in.pressed[K_ENTER] && !w.text.empty() && !in.gamepadActive && w.sub != 2) { w.status = "start"; }
    }
    if (w.type == "achievements") {
        int n = (int)game->achievements.size(), maxShow = std::max(1, (w.h - 58) / 30);
        if (actP(in, A_UP)) w.page = std::max(0, w.page - 1); if (actP(in, A_DOWN)) w.page = std::min(std::max(0, n - maxShow), w.page + 1);
        if (actP(in, A_PAGEUP)) w.page = std::max(0, w.page - maxShow); if (actP(in, A_PAGEDOWN)) w.page = std::min(std::max(0, n - maxShow), w.page + maxShow);
        if (in.gamepadActive && fabsf(in.stickY) > 0.5f) { w.timer += 0; }
    }
    if (w.type == "load") {
        if (actP(in, A_UP)) w.sel = (w.sel + 3) % 4; if (actP(in, A_DOWN)) w.sel = (w.sel + 1) % 4;
        if (actP(in, A_CONFIRM) && !in.gamepadActive) w.status = "go";
    }
    if (w.type == "yesno" || w.type == "msg") { if (actP(in, A_CONFIRM) && !in.gamepadActive) w.status = "yes"; }
    if (w.type == "readme") { if (in.wheel) w.page = clampi(w.page - in.wheel * 2, 0, 200); if (actP(in, A_UP)) w.page = std::max(0, w.page - 1); if (actP(in, A_DOWN)) w.page = std::min(200, w.page + 1); if (actP(in, A_PAGEDOWN)) w.page = std::min(200, w.page + 15); if (actP(in, A_PAGEUP)) w.page = std::max(0, w.page - 15); }
}

// ---------------- drawing ----------------
void Desktop::drawWindowFrame(Canvas& cv, DeskWindow& w, InputState& in, bool active) {
    bevelBox(cv, w.x, w.y, w.w, w.h, false);
    // title bar gradient
    for (int i = 0; i < w.w - 8; i++) {
        float f = (float)i / (w.w - 8);
        Color8 c = active ? RGB((uint8_t)(TITLE_A.r + (TITLE_B.r - TITLE_A.r) * f), (uint8_t)(TITLE_A.g + (TITLE_B.g - TITLE_A.g) * f), (uint8_t)(TITLE_A.b + (TITLE_B.b - TITLE_A.b) * f)) : RGB(128, 128, 128);
        cv.vline(w.x + 4 + i, w.y + 4, 18, c);
    }
    ctx.font->bold = false;
    ctx.font->draw(cv, *ctx.ts, w.x + 8, w.y + 7, w.title, active ? HILITE : FACE);
    // close button
    int cx = w.x + w.w - 20, cy = w.y + 5;
    if (active) hot(cx, cy, 16, 14);
    bool closeHover = active && !startOpen && !contextOpen && in.mouseX >= cx && in.mouseX < cx + 16 && in.mouseY >= cy && in.mouseY < cy + 14;
    bevelBox(cv, cx, cy, 16, 14, closeHover && in.mouseDown[0]);
    if (closeHover) cv.rect(cx + 1, cy + 1, 14, 12, RGB(0, 0, 128));
    cv.line(cx + 4, cy + 3, cx + 10, cy + 9, DKSHADOW); cv.line(cx + 5, cy + 3, cx + 11, cy + 9, DKSHADOW);
    cv.line(cx + 10, cy + 3, cx + 4, cy + 9, DKSHADOW); cv.line(cx + 11, cy + 3, cx + 5, cy + 9, DKSHADOW);
}

void Desktop::drawWindowContent(Canvas& cv, DeskWindow& w, InputState& in, bool active) {
    int x = w.x + 10, y = w.y + 30;
    Font& f = *ctx.font;
    if (w.type == "new") {
        // ---- campaign: the icons open this window with theirs picked; it can be changed here ----
        { auto all = listCampaigns(assetDir); int cur = 0; for (size_t i = 0; i < all.size(); i++) if (all[i].id == w.campaign) cur = (int)i;
          f.draw(cv, *ctx.ts, x, y, "Campaign:", DKSHADOW);
          int step = 0;
          if (win98Button(cv, in, x + 80, y - 4, 20, 18, "<", all.size() > 1, false, active)) step = -1;
          bevelBox(cv, x + 104, y - 3, 216, 16, true); cv.fillRect(x + 106, y - 1, 212, 12, HILITE);
          std::string cn = all.empty() ? w.campaign : all[cur].name + (all[cur].id == "triffall" ? "  (main story)" : ""); f.draw(cv, *ctx.ts, x + 110, y - 1, cn, DKSHADOW);
          if (win98Button(cv, in, x + 324, y - 4, 20, 18, ">", all.size() > 1, false, active)) step = 1;
          if (step && !all.empty()) { int ni = (cur + step + (int)all.size()) % (int)all.size(); std::string id = all[ni].id;
              if (game->switchCampaign(id)) { w.campaign = id; w.title = "New Game - " + game->campaign.name; w.sel = 0; gAudio.play("desktop_click"); } else showToast("That campaign could not be loaded."); } }
        y += 24;
        f.draw(cv, *ctx.ts, x, y, "Hero name:", DKSHADOW);
        bevelBox(cv, x + 80, y - 3, 180, 18, true); cv.fillRect(x + 82, y - 1, 176, 14, HILITE);
        f.draw(cv, *ctx.ts, x + 84, y, w.text + (((w.sub == 0 || w.sub == 2) && (ctx.frame / 20) % 2) ? "_" : ""), DKSHADOW);
        if (active) hot(x + 80, y - 3, 180, 18);
        if (active && !mouseUsed && in.mousePressed[0] && in.mouseX >= x + 80 && in.mouseX < x + 260 && in.mouseY >= y - 3 && in.mouseY < y + 15) {
            mouseUsed = true;
            // with a controller (or no keyboard) the field opens the on-screen keyboard
            if (in.gamepadActive && gPlatform->showTextInput("Hero name", w.text, 14)) w.sub = 3;   // the device's own keyboard is up
            else if (in.gamepadActive) { w.sub = 2; w.focusX = x + 17; w.focusY = y + 42 + 10; } else w.sub = 0;
        }
        if (win98Button(cv, in, x + 266, y - 4, 60, 18, "Random", true, false, active)) {
            static const char* names[] = {"Aldric", "Brynn", "Cael", "Dagny", "Edda", "Fenn", "Garrick", "Hesper", "Isolde", "Jory", "Kestrel", "Lio", "Maren", "Nyssa", "Osric", "Perrin",
                                          "Quill", "Rhosyn", "Soren", "Tamsin", "Ulric", "Vesna", "Wren", "Yorick", "Zaria", "Ash", "Corwin", "Ilsa", "Merrit", "Thane", "Odile", "Rook"};
            std::string pick; do { pick = names[rand() % (sizeof(names) / sizeof(names[0]))]; } while (pick == w.text);
            w.text = pick; gAudio.play("desktop_click");
        }
        if (win98Button(cv, in, x + 330, y - 4, 70, 18, w.sub == 2 ? "Close kb" : "Keyboard", true, w.sub == 2, active)) { w.sub = w.sub == 2 ? 0 : 2; if (w.sub == 2 && in.gamepadActive) { w.focusX = x + 17; w.focusY = y + 42 + 10; } }
        f.draw(cv, *ctx.ts, x, y + 26, "Class:", DKSHADOW);
        int ly = y + 42;
        bevelBox(cv, x, ly - 2, 150, 110, true); cv.fillRect(x + 2, ly, 146, 106, HILITE);
        if (w.sub == 1) cv.rect(x, ly - 2, 150, 110, TITLE_B);
        if (w.sub != 2 && in.mousePressed[0] && in.mouseX >= x && in.mouseX < x + 150 && in.mouseY >= ly - 2 && in.mouseY < ly + 108) w.sub = 1;
        int n = (int)ctx.gd->classOrder.size();
        for (int i = 0; i < n && i < 6 && w.sub != 2; i++) {
            const ClassDef* c = ctx.gd->cls(ctx.gd->classOrder[i]);
            bool sel = w.sel == i;
            hot(x + 2, ly + 2 + i * 17, 146, 17);
            if (in.mouseX >= x + 2 && in.mouseX < x + 148 && in.mouseY >= ly + 2 + i * 17 && in.mouseY < ly + 19 + i * 17) { if (in.mousePressed[0]) { w.sub = 1; w.sel = i; if (dblTimer > 0 && dblIcon == 100 + i && !w.text.empty()) w.status = "start"; dblTimer = 0.4f; dblIcon = 100 + i; } }
            if (sel) cv.fillRect(x + 3, ly + 2 + i * 17, 144, 16, TITLE_A);
            ctx.ts->draw(cv, T(c->spriteCol, c->spriteRow), x + 6, ly + 4 + i * 17, 1);
            f.draw(cv, *ctx.ts, x + 22, ly + 4 + i * 17, c->name, sel ? HILITE : DKSHADOW);
        }
        // 1.5.0 (#90): one fixed difficulty - the game is meant to be hard; no choice here
        {
            int dy = ly + 118;
            f.draw(cv, *ctx.ts, x, dy + 5, "Triffall is a hard game: use cover, flank, and bring", SHADOW);
            f.draw(cv, *ctx.ts, x, dy + 19, "the right weapon. A wipe teaches you something.", SHADOW);
        }
        if (w.sub == 2) {   // ---- on-screen keyboard (controller / touch): every key is a button the D-pad can reach ----
            int kx = x, ky = ly - 2; bevelBox(cv, kx, ky, 400, 112, false);
            static const char* rows[3] = {"ABCDEFGHIJ", "KLMNOPQRST", "UVWXYZ-'"};
            for (int r = 0; r < 3; r++) for (int i = 0; rows[r][i]; i++) {
                char ch = rows[r][i]; if (!w.oskShift && isalpha((unsigned char)ch)) ch = (char)tolower(ch);
                if (win98Button(cv, in, kx + 6 + i * 39, ky + 6 + r * 25, 36, 22, std::string(1, ch), true, false, active) && w.text.size() < 14) { w.text += ch; if (w.oskShift && isalpha((unsigned char)ch) && w.text.size() == 1) w.oskShift = false; }
            }
            if (win98Button(cv, in, kx + 6 + 8 * 39, ky + 6 + 2 * 25, 75, 22, "Space", true, false, active) && w.text.size() < 14 && !w.text.empty()) w.text += ' ';
            int by2 = ky + 6 + 3 * 25;
            if (win98Button(cv, in, kx + 6, by2, 75, 22, w.oskShift ? "abc" : "ABC", true, false, active)) w.oskShift = !w.oskShift;
            if (win98Button(cv, in, kx + 84, by2, 75, 22, "Delete", !w.text.empty(), false, active)) { w.text.pop_back(); if (w.text.empty()) w.oskShift = true; }
            if (win98Button(cv, in, kx + 162, by2, 75, 22, "Clear", !w.text.empty(), false, active)) { w.text.clear(); w.oskShift = true; }
            if (win98Button(cv, in, kx + 318, by2, 75, 22, "Done", !w.text.empty(), true, active)) { w.sub = 1; if (in.gamepadActive) { w.focusX = w.x + w.w - 180 + 40; w.focusY = w.y + w.h - 34 + 12; } }
        }
        if (n > 0 && w.sub != 2) {
            const ClassDef* c = ctx.gd->cls(ctx.gd->classOrder[clampi(w.sel, 0, n - 1)]);
            bevelBox(cv, x + 160, ly - 2, 240, 110, true); cv.fillRect(x + 162, ly, 236, 106, HILITE);
            ctx.ts->draw(cv, T(c->spriteCol, c->spriteRow), x + 168, ly + 4, 3);
            f.draw(cv, *ctx.ts, x + 210, ly + 6, c->name, DKSHADOW);
            std::string st; for (int s = 0; s < ST_COUNT; s++) st += fmt("%s %d  ", statName(s), c->stats[s]);
            f.drawWrapped(cv, *ctx.ts, x + 168, ly + 44, 228, c->desc, DKSHADOW);
            f.drawWrapped(cv, *ctx.ts, x + 168, ly + 84, 228, st, RGB(0, 0, 128));
        }
        bool canStart = !w.text.empty();
        if (win98Button(cv, in, w.x + w.w - 180, w.y + w.h - 34, 80, 24, "Start", canStart, true, active) || (w.status == "start" && canStart)) {
            w.status.clear();
            std::string cls = n > 0 ? ctx.gd->classOrder[clampi(w.sel, 0, n - 1)] : "";
            game->switchCampaign(w.campaign); game->newGame(w.text, cls); launchGame = true; windows.clear(); return;
        }
        if (win98Button(cv, in, w.x + w.w - 90, w.y + w.h - 34, 80, 24, "Cancel", true, false, active)) w.wantClose = true;
        f.draw(cv, *ctx.ts, x, w.y + w.h - 46, in.gamepadActive ? "A on the name opens a keyboard; Random picks a name." : "Tab: name / class. Enter: start.", SHADOW);
        f.draw(cv, *ctx.ts, x, w.y + w.h - 60, "Every level you pick a class rank: multiclass freely.", SHADOW);
        { const CampaignInfo& c = game->campaign; std::string cd = c.desc.empty() ? c.name : c.name + ": " + c.desc; if (!c.author.empty() && c.id != "triffall") cd += "  (by " + c.author + ")";
          auto ls = f.wrap(cd, w.w - 24); for (size_t i = 0; i < ls.size() && i < 2; i++) f.draw(cv, *ctx.ts, x, ly + 166 + (int)i * 12, ls[i], RGB(0, 0, 128)); }
    } else if (w.type == "load") {
        for (int i = 0; i < 4; i++) {
            int sy = y + i * 44; bool sel = w.sel == i;
            bool hover = in.mouseX >= x && in.mouseX < x + w.w - 20 && in.mouseY >= sy && in.mouseY < sy + 40; hot(x, sy, w.w - 20, 40);
            if (hover && in.mousePressed[0]) { if (w.sel == i && dblTimer > 0 && dblIcon == 200 + i && !in.gamepadActive) w.status = "go"; w.sel = i; dblTimer = 0.4f; dblIcon = 200 + i; mouseUsed = true;
                if (in.gamepadActive) { bool has = slots[i].exists; w.focusX = has ? w.x + w.w - 270 + 40 : w.x + w.w - 90 + 40; w.focusY = w.y + w.h - 34 + 12; } }   // picked: straight to Load (or Cancel)
            bevelBox(cv, x, sy, w.w - 20, 40, true);
            cv.fillRect(x + 2, sy + 2, w.w - 24, 36, sel ? TITLE_A : HILITE);
            Color8 tc = sel ? HILITE : DKSHADOW;
            drawIcon(cv, slots[i].exists ? 3 : 1, slots[i].exists ? 10 : 1, x + 6, sy + 4, 1);
            std::string label = i == 3 ? std::string("Autosave") : fmt("Slot %d", i + 1);
            if (slots[i].exists) {
                f.draw(cv, *ctx.ts, x + 44, sy + 5, label + fmt(": %s  (Lv %d)", slots[i].hero.c_str(), slots[i].level) + (slots[i].campaign.empty() ? "" : "  [" + slots[i].campaign + "]"), tc);
                f.draw(cv, *ctx.ts, x + 44, sy + 20, fmt("%s - %02d:%02d played", slots[i].mapName.c_str(), slots[i].playtime / 3600, (slots[i].playtime / 60) % 60), tc);
            } else { f.draw(cv, *ctx.ts, x + 44, sy + 12, label + (i == 3 ? ": <none yet - made when you travel>" : ": <empty>"), tc); }
        }
        bool ok = slots[w.sel].exists;
        if (win98Button(cv, in, w.x + w.w - 270, w.y + w.h - 34, 80, 24, "Load", ok, true, active) || (w.status == "go" && ok)) {
            w.status.clear();
            int slot = w.sel;
            if (game->running) { showDialog("Load Game", "Loading will discard unsaved progress in the running game. Continue?", [this, slot]() { loadSlot(slot); }, true); } else loadSlot(slot);
            return;
        }
        if (win98Button(cv, in, w.x + w.w - 180, w.y + w.h - 34, 80, 24, "Delete", slots[w.sel].exists, false, active)) { int slot = w.sel; showDialog("Delete Save", slot == 3 ? std::string("Delete the autosave permanently?") : fmt("Delete slot %d permanently?", slot + 1), [this, slot]() { remove(slotPath(slot).c_str()); remove((slotPath(slot) + ".bak").c_str()); refreshSlots(); }, true); }
        if (win98Button(cv, in, w.x + w.w - 90, w.y + w.h - 34, 80, 24, "Cancel", true, false, active)) w.wantClose = true;
    } else if (w.type == "iconpick") {
        // every picture in the icon sheet; click one to use it for that icon (saved at once)
        int idx = -1; for (size_t i = 0; i < iconDefs.size(); i++) if (iconDefs[i].action == w.text) idx = (int)i;
        if (idx < 0) { w.wantClose = true; return; }
        static std::vector<std::array<int, 2>> cells;
        if (cells.empty()) for (int r = 0; (r + 2) * 32 <= icons.h; r++) for (int c = 0; (c + 2) * 32 <= icons.w; c++) {
            if ((r == 9 && c <= 3) || (r == 10 && c <= 2)) continue;   // mouse pointers, not icons
            bool any = false; for (int yy = 0; yy < 32 && !any; yy++) for (int xx = 0; xx < 32; xx++) if (icons.get(32 + c * 32 + xx, 32 + r * 32 + yy).a > 0) { any = true; break; }
            if (any) cells.push_back({c, r});
        }
        const int COLS = 12, ROWS = 9, per = COLS * ROWS; int maxScroll = std::max(0, ((int)cells.size() - 1) / COLS - ROWS + 1);
        if (in.wheel && mouseInWindow(in, w)) w.sel = clampi(w.sel - in.wheel, 0, maxScroll);
        bevelBox(cv, x, y, COLS * 36 + 6, ROWS * 36 + 6, true); cv.fillRect(x + 2, y + 2, COLS * 36 + 2, ROWS * 36 + 2, RGB(0, 128, 128));
        for (int k = 0; k < per; k++) {
            int ci = w.sel * COLS + k; if (ci >= (int)cells.size()) break;
            int cx = x + 4 + (k % COLS) * 36, cy = y + 4 + (k / COLS) * 36; auto& cell = cells[ci];
            bool cur = cell[0] == iconDefs[idx].ic && cell[1] == iconDefs[idx].ir;
            bool hv = active && in.mouseX >= cx && in.mouseX < cx + 34 && in.mouseY >= cy && in.mouseY < cy + 34; hot(cx, cy, 34, 34);
            if (cur || hv) cv.fillRect(cx, cy, 34, 34, cur ? TITLE_A : RGB(40, 160, 160));
            drawIcon(cv, cell[0], cell[1], cx + 1, cy + 1, 1);
            if (hv && in.mousePressed[0]) { setIconPicture(idx, cell[0], cell[1]); mouseUsed = true; gAudio.play("desktop_click"); showToast("Icon changed."); w.wantClose = true; }
        }
        if (maxScroll > 0) f.draw(cv, *ctx.ts, x, y + ROWS * 36 + 12, "Scroll for more.", SHADOW);
        if (win98Button(cv, in, w.x + w.w - 270, w.y + w.h - 34, 80, 24, "Default", true, false, active)) { setIconPicture(idx, -1, -1); showToast("Icon reset."); w.wantClose = true; }
        if (win98Button(cv, in, w.x + w.w - 90, w.y + w.h - 34, 80, 24, "Cancel", true, false, active)) w.wantClose = true;
    } else if (w.type == "properties") {
        static const char* names[] = {"Classic Teal", "Blue Scanlines", "Plum Checker", "Forest Grid"};
        static const Color8 colors[] = {RGB(0, 128, 128), RGB(0, 24, 72), RGB(48, 12, 58), RGB(35, 55, 42)};
        f.draw(cv, *ctx.ts, x, y, "Desktop background:", DKSHADOW);
        for (int i = 0; i < 4; i++) {
            int ry = y + 28 + i * 36;
            cv.fillRect(x + 4, ry + 2, 46, 20, colors[i]);
            if (i == wallpaper) cv.rect(x + 3, ry + 1, 48, 22, TITLE_A);
            if (win98Button(cv, in, x + 62, ry, 220, 24, names[i], true, i == wallpaper, active)) {
                wallpaper = i; saveDesktopSettings(); showToast(std::string(names[i]) + " selected.");
            }
        }
        f.draw(cv, *ctx.ts, x + 4, y + 180, "Right-click the desktop for more options.", SHADOW);
        if (win98Button(cv, in, w.x + w.w - 90, w.y + w.h - 34, 80, 24, "OK", true, true, active)) w.wantClose = true;
    } else if (w.type == "settings") {
        GameSettings& s = *settings;
        int cy = y;
        // tabs: General / Controls
        { static const char* tabs[] = {"General", "Controls", "Access"}; for (int t = 0; t < 3; t++) { if (win98Button(cv, in, x + t * 90, cy - 4, 86, 20, tabs[t], true, w.page == t, active)) { w.page = t; w.sub = -1; } } }
        cy += 24;
        if (w.page == 2) {
            // ---- accessibility ----
            f.draw(cv, *ctx.ts, x, cy, "Reading", RGB(0, 0, 128)); cy += 16;
            // 1.1.1: the clear font is the only text face (the owner's call); the tileset's letters stay only in the logo
            checkbox(cv, in, x + 10, cy, "Large text (dialogue, messages, hints, story)", s.largeText, active); cy += 18;
            { static const char* ts[] = {"Text speed: slow", "Text speed: normal", "Text speed: fast", "Text speed: instant"}; if (win98Button(cv, in, x + 10, cy, 190, 18, ts[clampi(s.textSpeed, 0, 3)], true, false, active)) s.textSpeed = (s.textSpeed + 1) % 4; cy += 26; }
            f.draw(cv, *ctx.ts, x, cy, "Colour", RGB(0, 0, 128)); cy += 16;
            checkbox(cv, in, x + 10, cy, "Colour-blind friendly colours (blue / orange)", s.colorblind, active); cy += 14;
            f.draw(cv, *ctx.ts, x + 28, cy, "Your side, hit chances and cover stop using red/green.", SHADOW); cy += 20;
            f.draw(cv, *ctx.ts, x, cy, "Comfort", RGB(0, 0, 128)); cy += 16;
            checkbox(cv, in, x + 10, cy, "Reduce flashing (no lightning flashes)", s.reduceFlashing, active); cy += 18;
            checkbox(cv, in, x + 10, cy, "Screen shake", s.screenShake, active); cy += 18;
            checkbox(cv, in, x + 10, cy, "Fast combat animations", s.fastCombat, active); cy += 24;
            f.drawWrapped(cv, *ctx.ts, x, cy, w.w - 30, "Small screens (Steam Deck): with integer scaling on, the game uses a sharp 2x when it fits and a smooth fit to the screen when it doesn't.", SHADOW);
            if (win98Button(cv, in, w.x + w.w - 180, w.y + w.h - 34, 80, 24, "OK", true, true, active)) { s.save(saveDir + "/settings.ini"); showToast("Settings saved."); w.wantClose = true; }
            if (win98Button(cv, in, w.x + w.w - 90, w.y + w.h - 34, 80, 24, "Apply", true, false, active)) { s.save(saveDir + "/settings.ini"); showToast("Settings saved."); }
            return;
        }
        if (w.page == 1) {
            // ---- controls ----
            f.draw(cv, *ctx.ts, x, cy, "Click a binding, then press a key or pad button. Esc clears.", SHADOW); cy += 16;
            int rows = std::max(1, (w.y + w.h - 44 - (cy + 14)) / 15); int maxScroll = std::max(0, A_COUNT - rows);
            if (in.wheel && mouseInWindow(in, w)) w.sel = clampi(w.sel - in.wheel * 3, 0, maxScroll);
            w.sel = clampi(w.sel, 0, maxScroll);
            f.draw(cv, *ctx.ts, x + 150, cy, "Key", RGB(0, 0, 128)); f.draw(cv, *ctx.ts, x + 230, cy, "Alt key", RGB(0, 0, 128)); f.draw(cv, *ctx.ts, x + 310, cy, "Gamepad", RGB(0, 0, 128)); cy += 14;
            for (int r = 0; r < rows && w.sel + r < A_COUNT; r++) {
                int a = w.sel + r; int ry = cy + r * 15;
                f.draw(cv, *ctx.ts, x, ry, actionLabel(a), DKSHADOW);
                for (int col = 0; col < 3; col++) {
                    uint8_t& slot = col == 0 ? gKeys.key1[a] : col == 1 ? gKeys.key2[a] : gKeys.pad[a];
                    int bx = x + 150 + col * 80; bool waiting = (w.sub == a * 3 + col);
                    bool hover = in.mouseX >= bx && in.mouseX < bx + 74 && in.mouseY >= ry - 2 && in.mouseY < ry + 12; hot(bx, ry - 2, 74, 14);
                    bevelBox(cv, bx, ry - 2, 74, 14, waiting);
                    if (hover && (in.gamepadActive || padNavWindows) && !waiting) cv.rect(bx + 1, ry - 1, 72, 12, RGB(0, 0, 128));
                    if (waiting) cv.fillRect(bx + 2, ry, 70, 10, RGB(255, 255, 180));
                    f.draw(cv, *ctx.ts, bx + 4, ry - 1, waiting ? "press..." : keyName(slot), waiting ? C::Red : DKSHADOW);
                    if (hover && in.mousePressed[0] && active) { w.sub = a * 3 + col; mouseUsed = true; }
                }
            }
            if (maxScroll > 0) { int sbx = w.x + w.w - 16, sby = cy - 2, sbh = rows * 15; cv.fillRect(sbx, sby, 10, sbh, RGB(220, 220, 220)); int th = std::max(12, sbh * rows / A_COUNT), ty = sby + (sbh - th) * w.sel / maxScroll; bevelBox(cv, sbx, ty, 10, th, false); f.draw(cv, *ctx.ts, x + 118, w.y + w.h - 28, fmt("%d-%d / %d", w.sel + 1, std::min((int)A_COUNT, w.sel + rows), (int)A_COUNT), SHADOW); }
            if (w.sub >= 0 && in.lastKey != K_NONE) {
                int a = w.sub / 3, col = w.sub % 3; int k = in.lastKey;
                if (k == K_ESCAPE) k = K_NONE;
                bool padSlot = col == 2;
                if (k == K_NONE || isPadKey(k) == padSlot) { (col == 0 ? gKeys.key1[a] : col == 1 ? gKeys.key2[a] : gKeys.pad[a]) = (uint8_t)k; w.sub = -1; in.eatKeys(); }
            }
            if (win98Button(cv, in, x, w.y + w.h - 34, 110, 24, "Reset defaults", true, false, active)) gKeys.reset();
            if (win98Button(cv, in, w.x + w.w - 180, w.y + w.h - 34, 80, 24, "OK", true, true, active)) { s.save(saveDir + "/settings.ini"); showToast("Controls saved."); w.wantClose = true; }
            if (win98Button(cv, in, w.x + w.w - 90, w.y + w.h - 34, 80, 24, "Apply", true, false, active)) { s.save(saveDir + "/settings.ini"); showToast("Controls saved."); }
            return;
        }
        f.draw(cv, *ctx.ts, x, cy, "Display", RGB(0, 0, 128)); cy += 16;
        if (checkbox(cv, in, x + 10, cy, "Fullscreen (F11)", s.fullscreen, active)) { if (req) { req->setFullscreen = true; req->fullscreenValue = s.fullscreen; } }
        // 1.2.2 (#39): the game fills the screen; this brings back the 640x480 picture with bars at the sides
        if (checkbox(cv, in, x + 190, cy, "Classic 4:3 screen", s.classic43, active)) { if (req) req->setScale = std::max(1, s.scale); } cy += 18;
        if (checkbox(cv, in, x + 10, cy, "Pixel-perfect integer scaling", s.integerScaling, active)) { if (req) { req->setIntegerScaling = true; req->integerScalingValue = s.integerScaling; } } cy += 18;
        checkbox(cv, in, x + 10, cy, "CRT display effect", s.crt, active); cy += 18;
        if (checkbox(cv, in, x + 10, cy, "V-Sync", s.vsync, active)) { if (req) { req->setVsync = true; req->vsyncValue = s.vsync; } } cy += 18;
        f.draw(cv, *ctx.ts, x + 10, cy, "Window scale:", DKSHADOW);
        for (int sc = 1; sc <= 3; sc++) { if (win98Button(cv, in, x + 110 + (sc - 1) * 40, cy - 4, 36, 20, std::to_string(sc) + "x", true, s.scale == sc, active)) { s.scale = sc; if (req) req->setScale = sc; } } cy += 26;
        f.draw(cv, *ctx.ts, x, cy, "Audio", RGB(0, 0, 128)); cy += 16;
        slider(cv, in, x + 10, cy, 150, "Master volume", s.masterVolume, 0, 100, active); cy += 18;
        slider(cv, in, x + 10, cy, 150, "Music volume", s.musicVolume, 0, 100, active); cy += 18;
        slider(cv, in, x + 10, cy, 150, "SFX volume", s.sfxVolume, 0, 100, active); cy += 18;
        slider(cv, in, x + 10, cy, 150, "Ambience volume", s.ambientVolume, 0, 100, active); cy += 22;
        f.draw(cv, *ctx.ts, x, cy, "Gameplay", RGB(0, 0, 128)); cy += 16;
        checkbox(cv, in, x + 10, cy, "Gamepad enabled", s.gamepad, active); cy += 18;
        checkbox(cv, in, x + 10, cy, "Screen shake", s.screenShake, active); cy += 18;
        checkbox(cv, in, x + 10, cy, "Fast combat animations", s.fastCombat, active); cy += 18;
        checkbox(cv, in, x + 10, cy, "Interact while moving", s.interactWhileMoving, active); cy += 18;
        checkbox(cv, in, x + 10, cy, "Edge scrolling in combat", s.edgeScroll, active); cy += 18;
        { static const char* bl[] = {"Companion chatter: off", "Companion chatter: rare", "Companion chatter: normal", "Companion chatter: often"}; if (win98Button(cv, in, x + 10, cy, 190, 18, bl[clampi(s.banter, 0, 3)], true, false, active)) s.banter = (s.banter + 1) % 4; cy += 22; }
        if (win98Button(cv, in, w.x + w.w - 180, w.y + w.h - 34, 80, 24, "OK", true, true, active)) { s.save(saveDir + "/settings.ini"); showToast("Settings saved."); w.wantClose = true; }
        if (win98Button(cv, in, w.x + w.w - 90, w.y + w.h - 34, 80, 24, "Apply", true, false, active)) { s.save(saveDir + "/settings.ini"); showToast("Settings saved."); }
    } else if (w.type == "readme") {
        bevelBox(cv, w.x + 4, w.y + 26, w.w - 8, w.h - 30, true); cv.fillRect(w.x + 6, w.y + 28, w.w - 12, w.h - 34, HILITE);
        static const char* lines[] = {
            "TRIFFALL - README", "=================", "",
            "An old-school open-world RPG with X-COM style tactical combat.", "",
            "HOW TO PLAY - THE FIRST TEN MINUTES", "  New to Triffall? Double-click Tutorial first: a short adventure", "  with old Wick that teaches everything below by doing it.", "", "  1. Double-click New Game, name your hero, pick a class. (You can", "     add ranks in any class later, so don't agonise.)",
            "  2. Walk out of the cottage and talk to Elder Mara at the north-west", "     edge of the village. Your journal (J) always says where to go", "     next; M shows it on the world map.",
            "  3. Recruit Bram at the inn. Buy torches and potions - caves are dark.", "  4. When you meet monsters the fight happens right where you stand.",
            "     Move, attack, end turn. Stand next to walls for cover; hit things", "     from two sides to flank them. Fleeing is allowed and often smart.",
            "  5. Save at shrines (in towns and in most dungeons). The game", "     also autosaves whenever you reach a new place (Load Game >", "     Autosave), so a bad fight never costs more than a few minutes.",
            "  6. Talk to everyone. Most towns have three or four people who need", "     help, and a few choices in the story change how the world ends.",
            "  Stuck? Settings > Access has large text, slower or instant text,", "  colour-blind friendly colours and no flashing. Settings > Controls", "  rebinds everything, keyboard or gamepad.", "",
            "DESKTOP", "  Double-click an icon to open it. Right-click it (or E / pad X)", "  to change its picture. Drag icons anywhere: the desktop keeps", "  their places, pictures and the wallpaper for next time.", "  Use the Start menu for the same options.", "  Every campaign in assets/campaigns gets its own icon: double-click", "  it to start that adventure. Make your own in the Editor.", "",
            "EXPLORING (defaults - rebind in Settings > Controls)", "  Arrow keys / WASD or left stick ........ move", "  E / Enter / A button .................... talk, open, interact", "  Tab / Start button ....................... party, items, spells, quests", "  B / Q ..................................... spell menu / item menu", "  1-8 ....................................... favorites hotbar", "  Esc / B button ........................... pause (save, load, settings)", "  F5 ....................................... quick save (on a save shrine)", "  Mouse: click a neighbouring tile to step or interact.", "",
            "YOUR CHARACTER", "  Every level gives 3 stat points and one CLASS RANK. Put the rank", "  in any class (multiclassing): each rank adds that class's HP and", "  MP and teaches its spells at the rank listed in classes.txt.", "  STR  melee damage with STR weapons (+1 per 2 points over 8)", "  DEX  +2% to hit, +1 evasion, crits; +1 AP per 5, +1 Move per 3;", "       damage with bows, guns, daggers and rapiers", "  CON  +3 max HP per point", "  INT  +2 MP; arcane/tech spell damage (+1 per 2 over 8)", "  WIS  +2 MP; healing power (+1 per point over 8)", "  CHA  shop prices (the hero's CHA counts)", "  The level-up screen previews HP, MP, AP, Move, damage and more", "  before you confirm. Changed your mind? Brother Caul (Brasswick)", "  and Abbot Tyr (Frosthold) perform the Rite of Renewal for gold,", "  and a Draught of Forgetting does it anywhere.", "",
            "COMBAT (turn based, fought in place)", "  Each character has AP (actions) and MV (tiles of movement).", "  Move: pick a highlighted tile (arrows or mouse) and confirm.", "  Attack: pick an enemy in range; hit chance is shown.", "  A toggles normal / aimed attack. S spells, Q items, D defend,", "  F flee, T end turn, R action menu, C center camera, 1-8 favorites.", "  The box in the top-left describes the tile under the cursor.", "  AREA SPELLS AND BOMBS HIT EVERYONE in the blast - you too.", "  The preview flashes any ally (or you) who would be caught.", "  Hitting a monster with an element shows WEAK / resists / immune", "  and the info box remembers it. Weapon oils change your element.", "  Other monsters hear a fight and join it if they're close by.", "  Drinking or applying an item costs 4 AP; throwing one costs 3.", "",
            "DARKNESS", "  Caves, crypts and ruins are dark. Your party only lights a few", "  tiles around itself; past that you see the shape of the room but", "  not who stands in it, and you can't aim at what you can't see.", "  Attacks into shadow are 25% harder unless you see in the dark", "  (undead, machines and many cave dwellers do). Torches (use them", "  from the Items menu), the Light spell, flares, burning foes,", "  braziers and wall torches all push the dark back.", "",
            "CRAFTING", "  Smiths (forge), alchemists (alchemy) and tinkers (workbench) in", "  towns open a crafting screen; field recipes work from the Items", "  menu (C). Each station has a skill that grows as you use it.", "  Recipes have a difficulty: above your skill they can fail and", "  ruin half the materials; well below it they can come out fine or", "  masterwork. Forges and workbenches also Reinforce gear (+1 to +3)", "  and Salvage it for parts; alchemy benches Enchant it with rift", "  dust. Boss trophies unlock masterwork recipes; formulas teach more.", "",
            "THE WORLD", "  Rubble, vines, ice and boulders block some paths: dig with a", "  shovel, blast with a charge (light it and RUN) or a fire spell,", "  burn, melt or shove. Levers open doors and drain water. Glinting", "  spots are ore veins and herb patches - they grow back in a few days.", "  Notice boards post three bounties a day; take up to three at once.", "  Shops sell stacks: Left/Right sets the quantity, E buys the most.", "  Weapons and armour are in limited stock and restock every 3 days.", "",
            "FAVORITES", "  In the Spells or Items menu, highlight something and press 1-8.", "  Utility spells (Light, Knock, Recall, Sense Treasure, Camouflage,", "  healing) can be cast from the Spells menu outside combat.", "",
            "EDITOR", "  Edits maps (the overworld and every dungeon are plain map files", "  you can paint), tile kinds, items, monsters, spells, classes, shops,", "  quests, dungeons, recipes and dialogue. The Campaign button picks", "  which campaign you edit or creates a new one.", "",
            "Made with raylib. Art: Urizen 1-bit tileset (vurmux, CC0) and", "  Cute Desktop Icons 32x32 by Mayke-Arth (mayke-arth.itch.io).", "Music: Pixel Fantasy by AlkaKrab.", "Sound effects: Pixel Combat by Helton Yan (CC BY 4.0).",
        };
        int n = sizeof(lines) / sizeof(lines[0]);
        int maxShow = (w.h - 56) / 13;
        w.page = clampi(w.page, 0, std::max(0, n - maxShow));
        for (int i = 0; i < maxShow && w.page + i < n; i++) f.draw(cv, *ctx.ts, w.x + 10, w.y + 30 + i * 13, lines[w.page + i], DKSHADOW);
        { std::string hint = fmt("%d / %d  -  wheel or arrows to scroll", std::min(n, w.page + maxShow), n); cv.fillRect(w.x + 6, w.y + w.h - 20, w.w - 12, 14, FACE); f.draw(cv, *ctx.ts, w.x + w.w - 10 - f.width(hint), w.y + w.h - 18, hint, SHADOW); }
    } else if (w.type == "discord") {
        drawIcon(cv, 1, 6, x + 6, y, 1);
        f.drawWrapped(cv, *ctx.ts, x + 48, y, w.w - 70, std::string("Opening the Triffall Discord in your browser:\n\n") + discordUrl(), DKSHADOW);
        if (win98Button(cv, in, w.x + w.w - 90, w.y + w.h - 34, 80, 24, "OK", true, true, active)) w.wantClose = true;
    } else if (w.type == "mycomputer") { drawMyComputer(cv, w, in, active);
    } else if (w.type == "minesweeper") { drawMinesweeper(cv, w, in, active);
    } else if (w.type == "solitaire") { drawSolitaire(cv, w, in, active);
    } else if (w.type == "achievements") { drawAchievementsWindow(cv, w, in, active);
    } else if (w.type == "setup") {
        // first launch: a Win98 setup wizard. The components are for fun; the two options at the end are real settings.
        GameSettings& s = *settings;
        cv.fillRect(w.x + 4, w.y + 24, 110, w.h - 64, RGB(0, 0, 128));
        for (int k = 0; k < 8; k++) { uint32_t h = (uint32_t)(k * 2654435761u); cv.put(w.x + 10 + (int)(h % 98), w.y + 30 + (int)((h >> 9) % 80), RGB(200, 200, 255)); }
        drawIcon(cv, 0, 7, w.x + 27, w.y + 120, 2);
        f.bold = true; f.draw(cv, *ctx.ts, w.x + 4 + (110 - f.width("TRIFFALL")) / 2, w.y + 200, "TRIFFALL", RGB(255, 210, 90)); f.bold = false;
        int tx = w.x + 126, ty = w.y + 32, tw = w.w - 140;
        // Enter presses the default button (Next / Finish), as in a real setup wizard, so a keyboard alone gets through it
        bool kbNext = active && !in.gamepadActive && in.pressed[K_ENTER];
        auto next = [&](const char* label, bool enabled = true) { bool c = win98Button(cv, in, w.x + w.w - 180, w.y + w.h - 34, 80, 24, label, enabled, true, active); if (enabled && kbNext) { kbNext = false; in.pressed[K_ENTER] = false; c = true; } return c; };
        auto cancel = [&]() { return win98Button(cv, in, w.x + w.w - 90, w.y + w.h - 34, 80, 24, "Cancel", w.page < 3, false, active); };
        auto back = [&]() { return win98Button(cv, in, w.x + w.w - 266, w.y + w.h - 34, 80, 24, "< Back", w.page > 0 && w.page < 2, false, active); };
        if (w.page == 0) {
            f.bold = true; f.draw(cv, *ctx.ts, tx, ty, "Welcome to Triffall Setup", DKSHADOW); f.bold = false;
            f.drawWrapped(cv, *ctx.ts, tx, ty + 22, tw, std::string("This program will install Triffall ") + TRIFFALL_VERSION + " on your computer.\n\nIt is strongly recommended that you exit all Windows programs before running this Setup program. (It is 1998. You have plenty of time.)\n\nClick Next to continue.", DKSHADOW);
            if (next("Next >")) { w.page = 1; gAudio.play("ui_click"); }
        } else if (w.page == 1) {
            f.bold = true; f.draw(cv, *ctx.ts, tx, ty, "Select Components", DKSHADOW); f.bold = false;
            static bool c0 = true, c1 = true, c2 = true, c3 = true;
            int cy = ty + 22; bool req = true;
            checkbox(cv, in, tx, cy, "Triffall (required)", req, false); cy += 18;
            checkbox(cv, in, tx, cy, "Chiptune soundtrack", c1, active); cy += 18;
            checkbox(cv, in, tx, cy, "Desktop toys", c2, active); cy += 18;
#ifndef TRIFFALL_NO_EDITOR
            checkbox(cv, in, tx, cy, "Level editor", c3, active); cy += 18;
#endif
            (void)c0; (void)c3; cy += 8;
            f.bold = true; f.draw(cv, *ctx.ts, tx, cy, "How do you like to play?", DKSHADOW); f.bold = false; cy += 18;
            checkbox(cv, in, tx, cy, "Large text", s.largeText, active); cy += 18;
            f.draw(cv, *ctx.ts, tx, cy + 2, "(all can be changed in Settings > Access)", SHADOW);
            if (back()) w.page = 0;
            if (next("Next >")) { w.page = 2; w.timer = 0; s.save(saveDir + "/settings.ini"); gAudio.play("ui_click"); }
        } else if (w.page == 2) {
            f.bold = true; f.draw(cv, *ctx.ts, tx, ty, "Installing...", DKSHADOW); f.bold = false;
            static const char* files[] = {"TRIFFALL.EXE", "TILES.PNG", "ICONS.PNG", "DATA\\ITEMS.TXT", "DATA\\CREATURES.TXT", "DATA\\SPELLS.TXT", "DATA\\QUESTS.TXT", "MAPS\\HOLLOWMERE.TXT", "MAPS\\BRASSWICK.TXT", "MAPS\\ASHFALL.TXT", "MAPS\\THE_SCAR.TXT", "DIALOGUE\\ELDER_MARA.TXT", "AUDIO\\MUSIC\\TITLE.OGG", "AUDIO\\MUSIC\\BOSS.OGG", "EPILOGUE.TXT", "README.TXT"};
            int nf = sizeof(files) / sizeof(files[0]); float p = std::min(1.0f, w.timer / 2.6f);
            f.draw(cv, *ctx.ts, tx, ty + 24, "Copying files:", DKSHADOW);
            f.draw(cv, *ctx.ts, tx, ty + 40, std::string("C:\\GAMES\\TRIFFALL\\") + files[std::min(nf - 1, (int)(p * nf))], RGB(0, 0, 128));
            bevelBox(cv, tx, ty + 62, tw - 10, 18, true); for (int k = 0; k + 9 <= (int)((tw - 14) * p); k += 11) cv.fillRect(tx + 3 + k, ty + 65, 9, 12, RGB(0, 0, 128));
            f.draw(cv, *ctx.ts, tx, ty + 88, fmt("%d%% complete", (int)(p * 100)), DKSHADOW);
            next("Next >", false);
            if (p >= 1.0f) { w.page = 3; markInstalled(); gAudio.play("desktop_open"); }
        } else {
            f.bold = true; f.draw(cv, *ctx.ts, tx, ty, "Setup Complete", DKSHADOW); f.bold = false;
            f.drawWrapped(cv, *ctx.ts, tx, ty + 22, tw, "Triffall has been installed on your computer.\n\nThe Readme on the desktop explains how to play. Settings has controls, text size and colours.", DKSHADOW);
            static bool startNow = true; checkbox(cv, in, tx, ty + 110, "Start a new game now", startNow, active);
            // new players start with the tutorial (a short campaign that teaches by playing) when the game has one
            bool hasTutorial = false; for (auto& c : listCampaigns(assetDir)) if (c.id == "tutorial") hasTutorial = true;
            static bool tutorialFirst = true; if (hasTutorial && startNow) checkbox(cv, in, tx + 16, ty + 128, "Begin with the Tutorial (recommended)", tutorialFirst, active);
            if (next("Finish")) { w.wantClose = true; if (startNow) openNewGame(hasTutorial && tutorialFirst ? std::string("tutorial") : game ? game->campaignId : "triffall"); }
        }
        if (cancel()) { markInstalled(); w.wantClose = true; }
    } else if (w.type == "about") {
        drawIcon(cv, 0, 7, x + 6, y + 4, 2);
        f.bold = true; f.draw(cv, *ctx.ts, x + 80, y + 4, "Triffall", RGB(0, 0, 128)); f.bold = false;
        f.drawWrapped(cv, *ctx.ts, x + 80, y + 22, w.w - 100, std::string("Version ") + TRIFFALL_VERSION + "\nAn open-world RPG with tactical turn-based combat, wrapped in a nostalgic desktop.\n\nEngine: C++ software renderer + raylib shell.\nArt: Urizen 1-bit tileset (vurmux) and Cute Desktop Icons by Mayke-Arth.\nMusic: Pixel Fantasy by AlkaKrab.", DKSHADOW);
        if (win98Button(cv, in, w.x + w.w - 90, w.y + w.h - 34, 80, 24, "OK", true, true, active)) w.wantClose = true;
    } else if (w.type == "msg" || w.type == "yesno") {
        drawIcon(cv, w.type == "yesno" ? 2 : 2, w.type == "yesno" ? 10 : 3, x + 4, y, 1);
        f.drawWrapped(cv, *ctx.ts, x + 44, y + 2, w.w - 64, w.text, DKSHADOW);
        if (w.type == "yesno") {
            if (win98Button(cv, in, w.x + w.w / 2 - 86, w.y + w.h - 34, 80, 24, "Yes", true, true, active) || w.status == "yes") { auto fn = w.onYes; w.wantClose = true; w.status.clear(); if (fn) fn(); return; }
            if (win98Button(cv, in, w.x + w.w / 2 + 6, w.y + w.h - 34, 80, 24, "No", true, false, active)) w.wantClose = true;
        } else {
            if (win98Button(cv, in, w.x + w.w / 2 - 40, w.y + w.h - 34, 80, 24, "OK", true, true, active) || w.status == "yes") { w.wantClose = true; w.status.clear(); }
        }
    }
}

void Desktop::drawTaskbar(Canvas& cv, InputState& in) {
    int y = UI::SCREEN_H - TASKBAR_H;
    cv.fillRect(0, y, UI::SCREEN_W, TASKBAR_H, FACE);
    cv.hline(0, y, UI::SCREEN_W, FACE); cv.hline(0, y + 1, UI::SCREEN_W, HILITE);
    // start button
    ctx.font->bold = true; int startW = 14 + 12 + ctx.font->width("Start") + 8; ctx.font->bold = false; startBtnW = startW;
    bevelBox(cv, 2, y + 4, startW, 22, startOpen);
    // a little rift: purple diamond with a bright tear through it (not a flag, not anyone's logo)
    int fx = 8, fy = y + 8;
    static const char* rift[] = {".....X.....", "....XPX....", "...XPPPX...", "..XPPWPPX..", ".XPPPWPPPX.", "XPPPPWPPPPX", ".XPPPWPPPX.", "..XPPWPPX..", "...XPPPX...", "....XPX....", ".....X....."};
    for (int ry = 0; ry < 11; ry++) for (int rx = 0; rx < 11; rx++) { char c = rift[ry][rx]; if (c == '.') continue; cv.put(fx + rx, fy + ry, c == 'X' ? RGB(40, 10, 70) : c == 'P' ? RGB(140, 60, 220) : RGB(240, 230, 255)); }
    ctx.font->bold = true; ctx.font->draw(cv, *ctx.ts, fx + 16, y + 9, "Start", DKSHADOW); ctx.font->bold = false;
    cv.vline(startW + 6, y + 5, 20, SHADOW); cv.vline(startW + 7, y + 5, 20, HILITE);
    if (game->running) {
        int bx = 2 + startW + 6; bevelBox(cv, bx, y + 4, 160, 22, false);
        ctx.ts->draw(cv, T(ctx.gd->cls(game->party.members[0].classId) ? ctx.gd->cls(game->party.members[0].classId)->spriteCol : 104, game->party.members[0].tile.r), bx + 6, y + 9, 1);
        ctx.font->draw(cv, *ctx.ts, bx + 24, y + 9, "Triffall - " + game->heroName, DKSHADOW);
    }
    // tray with clock
    int tw = 70;
    bevelBox(cv, UI::SCREEN_W - tw - 4, y + 4, tw, 22, true);
    time_t t = time(nullptr); struct tm* lt = localtime(&t);
    char buf[16]; if (lt) strftime(buf, sizeof buf, "%H:%M", lt); else snprintf(buf, sizeof buf, "00:00");
    ctx.ts->draw(cv, TL::UI_SPEAKER, UI::SCREEN_W - tw + 2, y + 9, 1, DKSHADOW);
    ctx.font->draw(cv, *ctx.ts, UI::SCREEN_W - 42, y + 9, buf, DKSHADOW);
    (void)in;
}

void Desktop::drawStartMenu(Canvas& cv, InputState& in) {
    std::vector<std::pair<std::string, std::pair<int, int>>> items = {{"Resume Triffall", {1, 7}}, {"New Game", {0, 7}}, {"Load Game", {4, 0}}, {"Settings", {4, 3}}, {"Discord", {1, 6}}, {"Readme", {2, 3}}, {"Editor", {0, 8}}, {"My Computer", {1, 12}}, {"Minesweeper", {0, 14}}, {"Solitaire", {1, 4}}, {"Achievements", {4, 4}}, {"About", {3, 3}}, {"Shut Down...", {0, 0}}};
#ifdef TRIFFALL_NO_EDITOR
    items.erase(items.begin() + 6);   // "Editor" (keep in sync with the list in update())
#endif
    int mx = 2, mh = (int)items.size() * 22 + 8, my = UI::SCREEN_H - TASKBAR_H - mh;
    bevelBox(cv, mx, my, 180, mh, false);
    cv.fillRect(mx + 3, my + 3, 20, mh - 6, TITLE_A);
    // vertical text strip
    for (int i = 0; i < mh - 20; i += 4) cv.fillRect(mx + 5 + (i / 4) % 3, my + mh - 8 - i, 2, 2, TITLE_B);
    for (size_t i = 0; i < items.size(); i++) {
        int iy = my + 4 + (int)i * 22;
        bool sel = (int)i == startSel;
        bool enabled = !(i == 0 && !game->running);
        if (sel) cv.fillRect(mx + 24, iy, 153, 22, TITLE_A);
        // icon drawn at half scale: blit 32px icon scaled down by sampling
        int ic = items[i].second.first, ir = items[i].second.second;   // the same picture as its desktop icon (the player may have changed it)
        { static const std::pair<const char*, const char*> act[] = {{"New Game", "new"}, {"Load Game", "load"}, {"Settings", "settings"}, {"Discord", "discord"}, {"Readme", "readme"}, {"Editor", "editor"}, {"My Computer", "mycomputer"}, {"Minesweeper", "minesweeper"}, {"Solitaire", "solitaire"}, {"Achievements", "achievements"}};
          for (auto& a : act) if (items[i].first == a.first) iconCell(a.second, ic, ir); }
        for (int yy = 0; yy < 16; yy++) for (int xx = 0; xx < 16; xx++) { Color8 c = icons.get(32 + ic * 32 + xx * 2, 32 + ir * 32 + yy * 2); if (c.a) cv.put(mx + 28 + xx, iy + 3 + yy, c); }
        ctx.font->draw(cv, *ctx.ts, mx + 50, iy + 5, items[i].first, !enabled ? SHADOW : sel ? HILITE : DKSHADOW);
        if (i == 11) { cv.hline(mx + 26, iy + 21, 150, SHADOW); cv.hline(mx + 26, iy + 22, 150, HILITE); }
    }
}

void Desktop::drawContextMenu(Canvas& cv, InputState& in) {
    static const char* deskItems[] = {"Arrange Icons", "Refresh", "Properties"};
    static const char* iconItems[] = {"Open", "Change Icon...", "Default Icon"};
    const char* const* items = contextIcon >= 0 ? iconItems : deskItems;
    int h = 82;
    bevelBox(cv, contextX, contextY, 172, h, false);
    const int rowTop[] = {contextY + 4, contextY + 26, contextY + 55};
    for (int i = 0; i < 3; i++) {
        int ry = rowTop[i]; bool hover = in.mouseX >= contextX + 4 && in.mouseX < contextX + 168 && in.mouseY >= ry && in.mouseY < ry + 22;
        if (hover) contextSel = i;
        bool selected = contextSel == i;
        hot(contextX + 4, ry, 164, 22);
        if (selected) cv.fillRect(contextX + 4, ry, 164, 22, TITLE_A);
        ctx.font->draw(cv, *ctx.ts, contextX + 12, ry + 5, items[i], selected ? HILITE : DKSHADOW);
        if (hover && in.mousePressed[0]) { mouseUsed = true; contextAction(i); return; }
    }
    cv.hline(contextX + 6, contextY + 50, 160, SHADOW);
    cv.hline(contextX + 6, contextY + 51, 160, HILITE);
}

void Desktop::drawCursor(Canvas& cv, int x, int y) {
    // arrow cursor from the icon sheet (0,9); hand (0,10)
    if (cursorHand) cv.blit(icons, 32 + 0 * 32, 32 + 10 * 32, 32, 32, x - 10, y - 2, 1);
    else cv.blit(icons, 32, 32 + 9 * 32, 32, 32, x - 1, y - 1, 1);
}

void Desktop::draw(Canvas& cv, InputState& in) {
    ctx.cv = &cv;
    if (screensaver) { drawScreensaver(cv, 1.0f / 60.0f); return; }
    drawWallpaper(cv);
    // desktop icons. A name too long for its spot is cut with "..."; the one under the pointer (or selected) shows in full,
    // drawn last so it sits on top of its neighbours
    const int LABEL_W = 84;   // one icon column
    auto fitLabel = [&](const std::string& t) { if (ctx.font->width(t) <= LABEL_W) return t; std::string s = t; while (!s.empty() && ctx.font->width(s + "...") > LABEL_W) s.pop_back(); while (!s.empty() && s.back() == ' ') s.pop_back(); return s + "..."; };
    int fullLabel = iconDrag >= 0 ? iconDrag : hoverIcon >= 0 ? hoverIcon : windows.empty() ? selIcon : -1;
    auto drawLabel = [&](size_t i, bool full) {
        int ix = iconPositions[i][0], iy = iconPositions[i][1];
        bool sel = (int)i == selIcon && windows.empty();
        std::string label = full ? iconDefs[i].label : fitLabel(iconDefs[i].label);
        int tw = ctx.font->width(label);
        int lx = clampi(ix + 34 - tw / 2, 1, UI::SCREEN_W - tw - 2);
        if (sel || full) cv.fillRect(lx - 2, iy + 34, tw + 4, 14, sel ? TITLE_A : RGB(0, 90, 90));
        ctx.font->draw(cv, *ctx.ts, lx + 1, iy + 36, label, DKSHADOW);
        ctx.font->draw(cv, *ctx.ts, lx, iy + 35, label, HILITE);
    };
    for (size_t i = 0; i < iconDefs.size(); i++) {
        drawIcon(cv, iconDefs[i].ic, iconDefs[i].ir, iconPositions[i][0] + 18, iconPositions[i][1], 1);
        if ((int)i != fullLabel) drawLabel(i, false);
    }
    if (fullLabel >= 0 && fullLabel < (int)iconDefs.size()) drawLabel((size_t)fullLabel, ctx.font->width(iconDefs[fullLabel].label) > LABEL_W);
    // welcome text when idle
    if (!game->running && windows.empty()) {
        { int keep = ctx.font->style; ctx.font->style = Font::STYLE_CLASSIC;   // the logo always keeps the game's own letters
          ctx.font->bold = true; ctx.font->drawShadow(cv, *ctx.ts, UI::SCREEN_W - 300, UI::SCREEN_H - TASKBAR_H - 60, "TRIFFALL", RGB(255, 230, 120), RGB(0, 60, 60), 2); ctx.font->bold = false; ctx.font->style = keep; }
        ctx.font->draw(cv, *ctx.ts, UI::SCREEN_W - 300, UI::SCREEN_H - TASKBAR_H - 28, "Double-click New Game to begin.", RGB(200, 240, 240));
    }
    // windows
    for (size_t i = 0; i < windows.size(); i++) {
        bool active = (int)i == activeWindow();
        drawWindowFrame(cv, windows[i], in, active);
        drawWindowContent(cv, windows[i], in, active);
    }
    drawTaskbar(cv, in);
    if (startOpen) drawStartMenu(cv, in);
    if (contextOpen) drawContextMenu(cv, in);
    if (toastT > 0) { int tw = ctx.font->width(toast) + 20; bevelBox(cv, UI::SCREEN_W - tw - 8, UI::SCREEN_H - TASKBAR_H - 30, tw, 22, false); ctx.font->draw(cv, *ctx.ts, UI::SCREEN_W - tw + 2, UI::SCREEN_H - TASKBAR_H - 25, toast, DKSHADOW); }
    cursorHand = hoverIcon >= 0 && windows.empty();
    if (!startOpen && !contextOpen) for (const Rect& r : hots) if (r.contains(cursorX, cursorY)) { cursorHand = true; break; }
    if (in.mouseInWindow || in.gamepadActive) drawCursor(cv, cursorX, cursorY);
}

// Boot (1.1.1: one screen, the owner's call): a DOS / BIOS start-up with the Triffall title at its top, the way a
// 1998 PC showed its maker's logo above the memory test. The lines come in one by one, then the desktop.
// App::frame moves on after BOOT_SECONDS or on any key / click.
void Desktop::drawBoot(Canvas& cv, float t) {
    cv.clear(C::Black);
    Font& f = *ctx.font;
    const Color8 GREY = RGB(200, 200, 200), DIMG = RGB(120, 120, 120), AMBER = RGB(255, 205, 90);
    // the title, in the game's own letters (the logo keeps them)
    { int keep = f.style; f.style = Font::STYLE_CLASSIC; f.bold = true;
      f.draw(cv, *ctx.ts, 19, 19, "TRIFFALL", RGB(90, 50, 0), 4); f.draw(cv, *ctx.ts, 16, 16, "TRIFFALL", AMBER, 4);
      f.bold = false; f.style = keep; }
    f.draw(cv, *ctx.ts, 18, 70, "an old-school RPG", RGB(200, 190, 150));
    f.draw(cv, *ctx.ts, cv.w - 16 - f.width(TRIFFALL_VERSION_LABEL), 20, TRIFFALL_VERSION_LABEL, DIMG);
    cv.hline(16, 90, cv.w - 32, RGB(70, 70, 70));
    const char* lines[] = {"Triffall BIOS v1.0 (C) 1998", "CPU: Pentium II 266 MHz", "Memory Test: 65536K OK", "", "Detecting IDE drives...", "  Primary Master: TRIFFALL-HDD", "  Secondary Master: CD-ROM", "", "Loading Aldmoor...", "Starting Triffall..."};
    int n = sizeof(lines) / sizeof(lines[0]);
    int show = std::min(n, (int)(t / 0.2f));
    for (int i = 0; i < show; i++) f.draw(cv, *ctx.ts, 16, 104 + i * 14, lines[i], GREY);
    if (((int)(t * 3)) % 2 == 0) f.draw(cv, *ctx.ts, 16, 104 + std::min(show, n) * 14, "_", GREY);
    f.draw(cv, *ctx.ts, 16, cv.h - 24, "Press any key to continue", DIMG);
}

bool Desktop::snapCursor(int dx, int dy) {
    if (dy) {   // up/down: the nearest row of controls in that direction, then the control in it closest to the cursor
        int minAlong = 1 << 30;
        for (auto& r : hotsPrev) { if (r.contains(cursorX, cursorY)) continue; int along = dy * (r.y + r.h / 2 - cursorY); if (along > 3 && along < minAlong) minAlong = along; }
        if (minAlong == (1 << 30)) return false;
        int best = -1, bestAcross = 1 << 30;
        for (size_t i = 0; i < hotsPrev.size(); i++) { const Rect& r = hotsPrev[i]; if (r.contains(cursorX, cursorY)) continue; int along = dy * (r.y + r.h / 2 - cursorY); if (along <= 3 || along > minAlong + 8) continue;
            int across = cursorX < r.x ? r.x - cursorX : cursorX >= r.x + r.w ? cursorX - (r.x + r.w - 1) : 0; if (across < bestAcross) { bestAcross = across; best = (int)i; } }
        if (best < 0) return false;
        cursorX = hotsPrev[best].x + hotsPrev[best].w / 2; cursorY = hotsPrev[best].y + hotsPrev[best].h / 2;
        return true;
    }
    // left/right: the hotspot whose centre lies most directly in the requested direction from the cursor
    int best = -1; float bestScore = 1e9f;
    for (size_t i = 0; i < hotsPrev.size(); i++) {
        const Rect& r = hotsPrev[i]; int cx = r.x + r.w / 2, cy = r.y + r.h / 2;
        if (r.contains(cursorX, cursorY)) continue;
        int ox = cx - cursorX, oy = cy - cursorY;
        int along = dx * ox + dy * oy; if (along <= 0) continue;
        int across = abs(dx ? oy : ox);
        // a hotspot that overlaps the cursor's row/column (for horizontal/vertical moves) wins over a far-off diagonal one
        bool inLine = dx ? (cursorY >= r.y - 2 && cursorY < r.y + r.h + 2) : (cursorX >= r.x - 2 && cursorX < r.x + r.w + 2);
        float score = (float)along + (inLine ? 0.0f : across * 2.5f + 40.0f);
        if (score < bestScore) { bestScore = score; best = (int)i; }
    }
    if (best < 0) return false;
    cursorX = hotsPrev[best].x + hotsPrev[best].w / 2; cursorY = hotsPrev[best].y + hotsPrev[best].h / 2;
    return true;
}
