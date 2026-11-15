// Editor: the Campaign tab - what campaign.txt says, as a form: name and description (desktop icon + New Game window),
// where a new game begins (start map + tile; also shown as START in the Maps tab, B over a tile sets it), the intro
// cutscene, the dialogue that opens after it, and starting gold / items / flags.
#include "editor.h"
#include <algorithm>

using namespace ED;
extern int gEdTop;
static const int TOP_BAR_BOTTOM = 18;

void Editor::loadCampaignForm() {
    camp = CampaignForm(); camp.path = dataRoot + "/campaign.txt";
    std::string t; if (!readFile(camp.path, t)) { camp.name = campaignId; camp.startMap = mapFiles.empty() ? "" : mapFiles[0]; camp.dirty = true; return; }
    bool inSec = false;
    for (auto& raw : split(t, '\n')) {
        std::string l = trim(raw);
        if (!inSec) { if (l == "[campaign]") inSec = true; else camp.headerComments += raw + "\n"; continue; }
        if (l.empty() || l[0] == '#') continue;
        size_t eq = l.find('='); if (eq == std::string::npos) continue;
        std::string k = trim(l.substr(0, eq)), v = trim(l.substr(eq + 1));
        if (k == "name") camp.name = v; else if (k == "desc") camp.desc = v; else if (k == "author") camp.author = v; else if (k == "version") camp.version = v;
        else if (k == "icon") camp.icon = v; else if (k == "gold") camp.gold = atoi(v.c_str()); else if (k == "items") camp.items = v; else if (k == "flags") camp.flags = v;
        else if (k == "dialogue") camp.dialogue = v; else if (k == "intro") camp.intro.push_back(v); else if (k == "worldmap") camp.worldMap = v; else if (k == "defeat") camp.defeat = v;
        else if (k == "combatmusic") camp.combatMusic = v; else if (k == "bossmusic") camp.bossMusic = v;
        else if (k == "start") { auto p = split(v, ','); camp.startMap = trim(p[0]); camp.startX = p.size() > 1 ? atoi(p[1].c_str()) : 0; camp.startY = p.size() > 2 ? atoi(p[2].c_str()) : 0; }
        else camp.extra.push_back({k, v});
    }
}
bool Editor::saveCampaignForm() {
    std::string o = camp.headerComments.empty() ? "# Campaign settings - edited in the Editor's Campaign tab.\n" : camp.headerComments;
    o += "[campaign]\nname=" + camp.name + "\ndesc=" + camp.desc + "\n";
    if (!camp.author.empty()) o += "author=" + camp.author + "\n";
    if (!camp.version.empty()) o += "version=" + camp.version + "\n";
    o += "icon=" + (camp.icon.empty() ? std::string("0,7") : camp.icon) + "\n";
    o += fmt("start=%s,%d,%d\n", camp.startMap.c_str(), camp.startX, camp.startY);
    o += fmt("gold=%d\n", camp.gold);
    if (!camp.items.empty()) o += "items=" + camp.items + "\n";
    if (!camp.flags.empty()) o += "flags=" + camp.flags + "\n";
    for (auto& l : camp.intro) if (!trim(l).empty()) o += "intro=" + l + "\n";
    if (!camp.dialogue.empty()) o += "dialogue=" + camp.dialogue + "\n";
    if (!camp.worldMap.empty() && camp.worldMap != "overworld") o += "worldmap=" + camp.worldMap + "\n";
    if (!trim(camp.defeat).empty()) o += "defeat=" + trim(camp.defeat) + "\n";
    if (!trim(camp.combatMusic).empty()) o += "combatmusic=" + trim(camp.combatMusic) + "\n";
    if (!trim(camp.bossMusic).empty()) o += "bossmusic=" + trim(camp.bossMusic) + "\n";
    for (auto& kv : camp.extra) o += kv.first + "=" + kv.second + "\n";
    if (!writeFile(camp.path, o)) { say("Could not write " + camp.path); return false; }
    camp.dirty = false; changed = true; say("Saved " + camp.path);
    return true;
}
// the tile a new game starts on (0,0 in the file = that map's spawn point)
bool Editor::campaignStart(const std::string& mapId, int& x, int& y) {
    if (camp.startMap != mapId) return false;
    x = camp.startX; y = camp.startY;
    if (!x && !y && mapLoaded && em.id == mapId) { auto sp = split(em.hget("spawn"), ','); if (sp.size() == 2) { x = atoi(sp[0].c_str()); y = atoi(sp[1].c_str()); } }
    return true;
}
void Editor::setCampaignStart(const std::string& mapId, int x, int y) {
    camp.startMap = mapId; camp.startX = x; camp.startY = y; camp.dirty = true;
    if (saveCampaignForm()) say(fmt("New games now start on %s at %d,%d (saved campaign.txt).", mapId.c_str(), x, y));
}

void Editor::drawCampaign(InputState& in) {
    int x = 4, y = gEdTop + 4, w = W - 8, h = H - gEdTop - TOP_BAR_BOTTOM - 8;
    ui.panel(x, y, w, h);
    listHeader(x, y, w, "Campaign settings - " + camp.path + (camp.dirty ? "  (unsaved)" : ""), IMP_CAMP); helpBtn(x + w - 16, y + 1, "tab.campaign");
    int lx = x + 10, fx = x + 150, fw = 420, ry = y + 26, row = 24;
    auto field = [&](const char* label, std::string& v, const char* tip, const char* key) { ui.label(lx, ry + 3, label); helpBtn(fx - 18, ry + 2, std::string("campaign.") + key, "tab.campaign"); if (ui.textField(fx, ry, fw, v, 400)) camp.dirty = true; if (tip && ui.hover(lx, ry, 140, 18)) ui.tip(tip); ry += row; };
    field("Name", camp.name, "Shown under the desktop icon and in the New Game window.", "name");
    field("Description", camp.desc, "One or two sentences for the New Game window.", "desc");
    field("Author", camp.author, nullptr, "author"); field("Version", camp.version, nullptr, "version");
    field("Desktop icon", camp.icon, "col,row in assets/icons.png", "icon");
    // ---- where a new game begins ----
    ry += 6; ui.header(x + 4, ry, 630, "Where a new game begins (after character creation and the intro)"); helpBtn(x + 616, ry + 1, "campaign.start", "tab.campaign"); ry += 22;
    ui.label(lx, ry + 3, "Start map"); helpBtn(fx - 18, ry + 2, "campaign.start", "tab.campaign");
    if (ui.button(fx, ry, 200, 18, camp.startMap.empty() ? "(choose a map)" : camp.startMap, true, false, camp.startMap.empty() || std::find(mapFiles.begin(), mapFiles.end(), camp.startMap) == mapFiles.end() ? DANGER : ACCENT2))
        openListPicker("Map a new game starts on", mapFiles, std::max(0, (int)(std::find(mapFiles.begin(), mapFiles.end(), camp.startMap) - mapFiles.begin())), [this](int i) { if (i >= 0 && i < (int)mapFiles.size()) { camp.startMap = mapFiles[i]; camp.startX = camp.startY = 0; camp.dirty = true; } });
    ui.label(fx + 212, ry + 3, "x"); if (ui.intField(fx + 224, ry, 50, camp.startX, 0, 999)) camp.dirty = true;
    ui.label(fx + 282, ry + 3, "y"); if (ui.intField(fx + 294, ry, 50, camp.startY, 0, 999)) camp.dirty = true;
    if (ui.button(fx + 352, ry, 110, 18, "Show on map", !camp.startMap.empty())) {
        std::string m = camp.startMap; for (size_t i = 0; i < mapFiles.size(); i++) if (mapFiles[i] == m) { mapListSel = (int)i; loadMapFile(m); }
        int sx, sy; if (campaignStart(m, sx, sy)) { int cell = 12 * zoom; viewX = std::max(0, sx * cell - 300); viewY = std::max(0, sy * cell - 200); }
        tab = 0;
    }
    ry += row;
    ui.text(fx, ry, camp.startX || camp.startY ? "The party appears on that tile." : "0,0 = the map's spawn point (the green S in the Maps tab).", DIM); ry += 14;
    ui.text(fx, ry, "Tip: Maps tab, point at a tile, press B: new games start there.", DIM); ry += row;
    // ---- intro ----
    ui.header(x + 4, ry, 630, "Intro cutscene (one screen per line, $hero = the hero's name)"); helpBtn(x + 616, ry + 1, "campaign.intro", "tab.campaign"); ry += 22;
    int del = -1;
    for (size_t i = 0; i < camp.intro.size(); i++) {
        ui.label(lx, ry + 3, fmt("Line %d", (int)i + 1));
        if (ui.textField(fx, ry, 600 - fx, camp.intro[i], 600)) camp.dirty = true;
        if (ui.button(x + 604, ry, 18, 16, "x", true, false, DANGER)) del = (int)i;
        ry += 20; if (ry > y + h - 250) { ui.text(fx, ry, fmt("(+%d more lines)", (int)camp.intro.size() - (int)i - 1), DIM); ry += 16; break; }
    }
    if (del >= 0) { camp.intro.erase(camp.intro.begin() + del); camp.dirty = true; }
    if (ui.button(fx, ry, 90, 18, "+ intro line")) { camp.intro.push_back(""); camp.dirty = true; }
    ry += row + 4;
    // ---- after the intro ----
    ui.header(x + 4, ry, 630, "After the intro - and the world map"); ry += 22;
    ui.label(lx, ry + 3, "Opening dialogue"); helpBtn(fx - 18, ry + 2, "campaign.dialogue", "tab.campaign");
    if (ui.button(fx, ry, 200, 18, camp.dialogue.empty() ? "(none)" : camp.dialogue, true, false, ACCENT2)) {
        std::vector<std::string> opts = dlgFiles; opts.insert(opts.begin(), "(none)");
        openListPicker("Dialogue that opens when play begins", opts, 0, [this, opts](int i) { if (i >= 0 && i < (int)opts.size()) { camp.dialogue = i == 0 ? "" : opts[i]; camp.dirty = true; } });
    }
    ry += row;
    ui.label(lx, ry + 3, "Starting gold"); helpBtn(fx - 18, ry + 2, "campaign.gold", "tab.campaign"); if (ui.intField(fx, ry, 80, camp.gold, 0, 1000000)) camp.dirty = true; ry += row;
    field("Starting items", camp.items, "id:count,id:count - added to the hero's class gear", "items");
    field("Starting flags", camp.flags, "flag,flag=2 - set when the game begins", "flags");
    flagPicker(fx + fw + 6, ry - row, 52, false, [this](const std::string& f) { camp.flags = trim(camp.flags).empty() ? f : trim(camp.flags) + "," + f; camp.dirty = true; }, "+ flag");
    // ---- world map (M) ----
    ui.label(lx, ry + 3, "World map (M)"); helpBtn(fx - 18, ry + 2, "campaign.worldmap", "concepts.worldmap");
    {   std::string wm = camp.worldMap.empty() ? "overworld" : camp.worldMap; std::string lw = toLower(wm);
        std::string shown = lw == "overworld" ? "Overworld (default)" : lw == "off" ? "Off - no world map" : lw == "current" ? "Current map (automap)" : "Map: " + wm;
        bool bad = lw != "overworld" && lw != "off" && lw != "current" && std::find(mapFiles.begin(), mapFiles.end(), wm) == mapFiles.end();
        if (ui.button(fx, ry, 200, 18, shown, true, false, bad ? DANGER : ACCENT2)) {
            std::vector<std::string> opts{"Overworld (default)", "Off - no world map", "Current map (automap)"}; for (auto& m : mapFiles) if (m != "overworld") opts.push_back("Map: " + m);
            int cur = lw == "off" ? 1 : lw == "current" ? 2 : 0; for (size_t i = 3; i < opts.size(); i++) if (opts[i] == "Map: " + wm) cur = (int)i;
            openListPicker("What the M world map shows", opts, cur, [this, opts](int i) { camp.worldMap = i == 0 ? "" : i == 1 ? "off" : i == 2 ? "current" : opts[i].substr(5); camp.dirty = true; });
        }
        const char* expl = lw == "overworld" ? "M shows the overworld; it fills in as the party walks it (or enters places with reveal=)." : lw == "off" ? "No world map: M and the pause-menu entry say there is no map." : lw == "current" ? "M shows whichever map the party is on, filled in as it is explored." : "M always shows this map (e.g. a castle's hall); its exits are the places listed. revealmap= in dialogue fills it in.";
        auto wl = font->wrap(bad ? "that map does not exist in this campaign" : expl, x + 630 - (fx + 210)); for (size_t i = 0; i < wl.size() && i < 3; i++) ui.text(fx + 210, ry + 3 + (int)i * 12, wl[i], bad ? DANGER : DIM);
    }
    ry += row + 12;
    // ---- 1.1.7: what a lost fight does ----
    field("If the party falls", camp.defeat, "Empty: game over (load a save). A script: the party gets back up healed and it runs, e.g. dialogue:my_fallen", "defeat");
    // ---- 1.3.1 (#47): fight music (a map can override the ordinary one: Maps tab, "fight" in the header) ----
    auto musicRow = [&](const char* label, std::string& v, const char* def, const char* key, const char* tip) {
        ui.label(lx, ry + 3, label); helpBtn(fx - 18, ry + 2, std::string("campaign.") + key, "tab.campaign");
        auto di = gAudio.defs.find(v); bool bad = !v.empty() && (di == gAudio.defs.end() || di->second.kind != "music");
        if (ui.button(fx, ry, 200, 18, v.empty() ? std::string(def) + " (default)" : v, true, false, bad ? DANGER : ACCENT2)) {
            std::vector<std::string> ids{""}, labels{std::string(def) + " (default)"};
            for (auto& id : gAudio.order) { auto it = gAudio.defs.find(id); if (it == gAudio.defs.end() || it->second.kind != "music") continue; ids.push_back(id); labels.push_back(it->second.desc.empty() ? id : id + "  - " + it->second.desc); }
            int cur = 0; for (size_t i = 0; i < ids.size(); i++) if (ids[i] == v) cur = (int)i;
            std::string* target = &v; openListPicker(label, labels, cur, [this, ids, target](int i) { *target = ids[i]; camp.dirty = true; }); }
        ui.text(fx + 210, ry + 3, bad ? "not a music track in the Sounds tab" : tip, bad ? DANGER : DIM);
        ry += row; };
    musicRow("Fight music", camp.combatMusic, "combat", "combatmusic", "every fight, unless a map has its own");
    musicRow("Boss fight music", camp.bossMusic, "boss", "bossmusic", "fights with a boss creature");
    // ---- right column: playtest state, export ----
    { int rx = x + 640, rw = w - 646; drawPlayState(rx, y + 22, rw, 300, in); drawExportPanel(rx, y + 328, rw, h - 334 - 30, in); }
    // ---- buttons ----
    int by = y + h - 26;
    if (ui.button(x + 10, by, 90, 20, "Save", camp.dirty, false, OK) || (in.ctrl() && in.pressed[K_S] && camp.dirty && pop == P_NONE)) saveCampaignForm();
    if (ui.button(x + 106, by, 90, 20, "Revert", camp.dirty)) { loadCampaignForm(); say("Reverted."); }
    if (ui.button(x + 202, by, 140, 20, "Play from the start", !camp.startMap.empty(), false, OK)) { if (camp.dirty) saveCampaignForm(); requestNewGame = true; wantsClose = true; }
    if (ui.hover(x + 202, by, 140, 20)) ui.tip("Saves, leaves the editor and opens New Game for this campaign");
}
