// 1.1.6 (owner's #29): a new campaign starts empty; every tab that holds campaign content has an Import button that
// copies entries in from the base game or another campaign: data entries (items, creatures, quests... by id), whole
// map and dialogue files, tile kinds, flag notes and Campaign-tab settings. The window lists what the source has with
// a tick box each; entries this campaign already has are marked and get replaced. Data entries land unsaved (Save the
// tab to keep them, Revert drops them); map and dialogue files are copied straight into the campaign's folders.
// Nothing it uses comes along by itself (an imported creature's spells, a map's dialogue): Check all lists what's missing.
#include "editor.h"
#include <filesystem>
#include <algorithm>
#include <set>

namespace fs = std::filesystem;
using namespace ED;
extern int gEdTop;

std::string Editor::impReadPath(const CampaignInfo& c, const std::string& rel) const {
    std::string p = impRoot(c) + "/" + rel;
    if (!c.dir.empty() && c.inherit && !fileExists(p)) return baseRoot + "/" + rel;
    return p;
}
std::vector<std::string> Editor::impFiles(const CampaignInfo& c, const std::string& sub) const {
    std::vector<std::string> out; std::error_code ec;
    std::vector<std::string> roots{impRoot(c)}; if (!c.dir.empty() && c.inherit) roots.push_back(baseRoot);
    for (auto& r : roots) for (auto& e : fs::directory_iterator(r + "/" + sub, ec))
        if (e.path().extension() == ".txt") { std::string id = e.path().stem().string(); if (std::find(out.begin(), out.end(), id) == out.end()) out.push_back(id); }
    std::sort(out.begin(), out.end());
    return out;
}

void Editor::openImport(int kind, int gen) {
    impKind = kind; impGen = gen; impFilter.clear(); impScroll = impSrcScroll = 0; impEntries.clear(); impSrc = -1;
    impSrcs.clear(); for (auto& c : listCampaigns(assetDir)) if (c.id != campaignId) impSrcs.push_back(c);
    std::string what = kind == IMP_MAPS ? "maps" : kind == IMP_DLG ? "dialogue files" : kind == IMP_SCRIPTS ? "scripts" : kind == IMP_KINDS ? "tile kinds" : kind == IMP_CAMP ? "campaign settings" : kind == IMP_FLAGS ? "flag notes" : (gen >= 0 && gen < (int)gens.size() ? toLower(gens[gen].title) : "entries");
    impTitle = "Import " + what + " from another campaign";
    if (!impSrcs.empty()) impSelectSource(0);
    popOpenedFrame = frameNo; pop = P_IMPORT; ui.modal = true; ui.blur();
}

void Editor::impSelectSource(int i) {
    impSrc = i; impEntries.clear(); impScroll = 0;
    if (i < 0 || i >= (int)impSrcs.size()) return;
    const CampaignInfo& c = impSrcs[i];
    if (impKind == IMP_GEN && impGen >= 0 && impGen < (int)gens.size()) {
        GenericEditor& g = gens[impGen];
        std::string file = fs::path(g.path).filename().string(), t;
        if (!readFile(impReadPath(c, file), t)) return;
        for (auto& sc : parseConfig(t)) {
            if (sc.name.empty()) continue;
            ImpEntry e; e.sec.name = sc.name; e.sec.kv = sc.kv; e.id = e.sec.get(g.listKey);
            std::string nm = e.sec.get("name");
            e.label = e.id.empty() ? (nm.empty() ? "(" + sc.name + ")" : nm) : e.id + (nm.empty() || nm == e.id ? "" : "  -  " + nm);
            if (!g.altSection.empty()) e.label = (sc.name == "type" && impGen == 15 ? "every " : sc.name == g.altSection ? "notice: " : impGen == 15 ? "template: " : "board: ") + e.label;
            for (auto& s : g.secs) if (s.name == sc.name && s.get(g.listKey) == e.id && !s.inherited) e.have = true;
            for (auto& kv : sc.kv) if (kv.first != g.listKey) e.detail += kv.first + "=" + kv.second + " ";
            impEntries.push_back(e);
        }
    } else if (impKind == IMP_MAPS || impKind == IMP_DLG || impKind == IMP_SCRIPTS) {
        std::string sub = impKind == IMP_MAPS ? "maps" : impKind == IMP_SCRIPTS ? "scripts" : "dialogue";
        for (auto& id : impFiles(c, sub)) {
            ImpEntry e; e.id = e.label = id; e.have = fileExists(dataRoot + "/" + sub + "/" + id + ".txt");
            if (impKind == IMP_MAPS) { std::string t; if (readFile(impReadPath(c, "maps/" + id + ".txt"), t)) for (auto& sc : parseConfig(t)) if (sc.name == "map") { std::string nm = sc.get("name"); if (!nm.empty() && nm != id) e.label += "  -  " + nm; e.detail = sc.get("kind"); break; } }
            impEntries.push_back(e);
        }
    } else if (impKind == IMP_KINDS) {
        std::string t; if (!readFile(impRoot(c) + "/tilekinds.txt", t)) return;   // the base game's are always there; a campaign's own add to them
        std::set<std::string> have; for (auto& k : allTileKinds()) have.insert(k.name);
        for (auto& sc : parseConfig(t)) {
            if (sc.name != "kind") continue;
            ImpEntry e; e.sec.name = sc.name; e.sec.kv = sc.kv; e.id = e.label = e.sec.get("name"); if (e.id.empty()) continue;
            e.have = have.count(e.id) > 0; e.detail = e.sec.get("biome");
            impEntries.push_back(e);
        }
    } else if (impKind == IMP_FLAGS) {
        std::string t; if (!readFile(impRoot(c) + "/flags.txt", t)) return;
        for (auto& sc : parseConfig(t)) if (sc.name == "flag" && !sc.get("id").empty()) {
            ImpEntry e; e.id = sc.get("id"); e.label = e.id + "  -  " + sc.get("desc"); e.detail = sc.get("desc"); e.have = flagDocs.count(e.id) > 0;
            impEntries.push_back(e);
        }
    } else if (impKind == IMP_CAMP) {
        auto add = [&](const std::string& id, const std::string& label, const std::string& val, bool have) { ImpEntry e; e.id = id; e.label = label + ": " + (val.empty() ? "(none)" : val); e.detail = val; e.have = have; impEntries.push_back(e); };
        std::string intro; for (auto& l : c.intro) intro += (intro.empty() ? "" : " / ") + l;
        add("desc", "Description", c.desc, !camp.desc.empty());
        add("icon", "Desktop icon", fmt("%d,%d", c.iconC, c.iconR), !camp.icon.empty());
        add("start", "Where new games begin", c.startMap + fmt(" %d,%d", c.startX, c.startY), !camp.startMap.empty());
        add("intro", fmt("Intro cutscene (%d lines)", (int)c.intro.size()), intro, !camp.intro.empty());
        add("dialogue", "Dialogue after the intro", c.startDialogue, !camp.dialogue.empty());
        add("gold", "Starting gold", std::to_string(c.gold), true);
        add("items", "Starting items", c.startItems, !camp.items.empty());
        add("flags", "Starting flags", c.startFlags, !camp.flags.empty());
        add("worldmap", "World map (M)", c.worldMap, !camp.worldMap.empty());
        add("defeat", "If the party falls", c.defeat, !camp.defeat.empty());
        add("combatmusic", "Fight music", c.combatMusic, !camp.combatMusic.empty());
    }
}

int Editor::doImport() {
    if (impSrc < 0 || impSrc >= (int)impSrcs.size()) return 0;
    const CampaignInfo& c = impSrcs[impSrc];
    int n = 0, replaced = 0;
    if (impKind == IMP_GEN && impGen >= 0 && impGen < (int)gens.size()) {
        GenericEditor& g = gens[impGen]; int last = -1;
        for (auto& e : impEntries) {
            if (!e.on) continue;
            EdSection s = e.sec; s.inherited = false; s.origKv.clear(); s.lead.clear();
            int at = -1; for (size_t i = 0; i < g.secs.size(); i++) if (g.secs[i].name == s.name && g.secs[i].get(g.listKey) == e.id) at = (int)i;
            if (at >= 0) { g.secs[at].kv = s.kv; g.secs[at].inherited = false; g.secs[at].origKv.clear(); replaced++; last = at; }
            else { g.secs.push_back(s); last = (int)g.secs.size() - 1; }
            n++;
        }
        if (n) { g.dirty = true; g.sel = last; g.filter.clear(); g.filterKey.clear(); g.filterVal.clear(); flagIndexDirty = true; if (impGen == 8) worldNeedsGen = true;
                 say(fmt("Imported %d from %s into %s (%d replaced) - Save the tab to keep them.", n, c.name.c_str(), g.title.c_str(), replaced)); }
    } else if (impKind == IMP_MAPS || impKind == IMP_DLG || impKind == IMP_SCRIPTS) {
        std::string sub = impKind == IMP_MAPS ? "maps" : impKind == IMP_SCRIPTS ? "scripts" : "dialogue"; std::string first; std::error_code ec;
        fs::create_directories(dataRoot + "/" + sub, ec);
        for (auto& e : impEntries) {
            if (!e.on) continue;
            std::string t; if (!readFile(impReadPath(c, sub + "/" + e.id + ".txt"), t)) continue;
            std::string dst = dataRoot + "/" + sub + "/" + e.id + ".txt";
            if (!writeFile(dst, t)) { say("Could not write " + dst); continue; }
            if (e.have) replaced++; n++; if (first.empty()) first = e.id;
            if (impKind == IMP_MAPS && mapLoaded && em.id == e.id) loadMapFile(e.id);   // the open map was replaced
            if (impKind == IMP_DLG && te.path == dst) te.load(dst);
            if (impKind == IMP_SCRIPTS && ste.path == dst) ste.load(dst);
        }
        if (n) {
            refreshFiles(); mapScan.clear(); flagIndexDirty = true; changed = true;
            if (impKind == IMP_MAPS) { if (!mapLoaded) for (size_t i = 0; i < mapFiles.size(); i++) if (mapFiles[i] == first) { mapListSel = (int)i; loadMapFile(first); } if (first == "overworld") worldNeedsGen = true; }
            else if (te.path.empty()) { te.load(dataRoot + "/dialogue/" + first + ".txt"); for (size_t i = 0; i < dlgFiles.size(); i++) if (dlgFiles[i] == first) dlgSel = (int)i; }
            say(fmt("Copied %d %s from %s (%d replaced). Check all lists anything they use that this campaign lacks.", n, impKind == IMP_MAPS ? "map(s)" : "dialogue file(s)", c.name.c_str(), replaced));
        }
    } else if (impKind == IMP_KINDS) {
        std::string t;
        for (auto& e : impEntries) if (e.on) { t += "[kind]\n"; for (auto& kv : e.sec.kv) t += kv.first + "=" + kv.second + "\n"; t += "\n"; if (e.have) replaced++; n++; }
        if (n) {
            std::string tmp = dataRoot + "/.import_tilekinds.txt";
            if (writeFile(tmp, t)) { loadTileKindsOverlay(tmp); std::error_code ec; fs::remove(tmp, ec); rebuildTileKindIndex(); kindsDirty = true; }
            say(fmt("Imported %d tile kind(s) from %s (%d replaced) - Save the tab to keep them.", n, c.name.c_str(), replaced));
        }
    } else if (impKind == IMP_FLAGS) {
        for (auto& e : impEntries) if (e.on) { if (e.have) replaced++; flagDocs[e.id] = e.detail; n++; }
        if (n) { flagDocsDirty = true; say(fmt("Imported %d flag note(s) from %s - Save to keep them.", n, c.name.c_str())); }
    } else if (impKind == IMP_CAMP) {
        for (auto& e : impEntries) {
            if (!e.on) continue; n++;
            if (e.id == "desc") camp.desc = c.desc; else if (e.id == "icon") camp.icon = fmt("%d,%d", c.iconC, c.iconR);
            else if (e.id == "start") { camp.startMap = c.startMap; camp.startX = c.startX; camp.startY = c.startY; }
            else if (e.id == "intro") camp.intro = c.intro; else if (e.id == "dialogue") camp.dialogue = c.startDialogue;
            else if (e.id == "gold") camp.gold = c.gold; else if (e.id == "items") camp.items = c.startItems;
            else if (e.id == "flags") camp.flags = c.startFlags; else if (e.id == "worldmap") camp.worldMap = c.worldMap; else if (e.id == "defeat") camp.defeat = c.defeat;
            else if (e.id == "combatmusic") camp.combatMusic = c.combatMusic;
        }
        if (n) { camp.dirty = true; say(fmt("Imported %d setting(s) from %s - Save to keep them.", n, c.name.c_str())); }
    }
    return n;
}

bool Editor::importBtn(int x, int y, int kind, int gen) {
    bool hit = ui.button(x, y, 48, 14, "Import", true, false, ACCENT);
    if (ui.hover(x, y, 48, 14)) ui.tip("Copy entries in from the base game or another campaign");
    if (hit && pop == P_NONE) { openImport(kind, gen); return true; }
    return false;
}
void Editor::listHeader(int x, int y, int w, const std::string& title, int kind, int gen) {
    ui.header(x, y, w, "");
    ui.cv->setClip({x, y, w - 90, 16}); ui.text(x + 6, y + 2, title, ACCENT2); ui.cv->resetClip();   // the last 18 px hold the ? button
    importBtn(x + w - 86, y + 1, kind, gen);
}

void Editor::drawImport(InputState& in, bool& close) {
    Canvas& cv = *ui.cv;
    int w = 760, h = 540, x = (W - w) / 2, y = (H - h) / 2;
    ui.panel(x, y, w, h); ui.header(x, y, w, impTitle);
    ui.textRight(x + w - 8, y + 2, impKind == IMP_MAPS || impKind == IMP_DLG || impKind == IMP_SCRIPTS ? "copies the files straight in" : "arrives unsaved: Save the tab to keep it", DIM);
    // sources
    int lw = 200;
    ui.label(x + 8, y + 22, "From:");
    std::vector<std::string> names; for (auto& c : impSrcs) names.push_back(c.id == "triffall" ? c.name + " (base game)" : c.name);
    if (names.empty()) { ui.text(x + 8, y + 40, "There is no other campaign to import from.", DIM); }
    else { int hit = ui.listBox(x + 8, y + 38, lw, h - 80, names, impSrc, impSrcScroll); if (hit >= 0) impSelectSource(hit); }
    // entries
    int ex = x + lw + 16, ew = w - lw - 24;
    ui.label(ex, y + 22, "Find:"); ui.idCounter = 8000; ui.searchField(ex + 40, y + 20, ew - 196, impFilter, "search names and values...");
    std::vector<int> idx; for (size_t i = 0; i < impEntries.size(); i++) if (edMatch(impEntries[i].label + " " + impEntries[i].detail, impFilter)) idx.push_back((int)i);
    if (ui.button(ex + ew - 150, y + 20, 72, 16, "Tick shown", !idx.empty())) for (int i : idx) impEntries[i].on = true;
    if (ui.button(ex + ew - 74, y + 20, 74, 16, "Untick all")) for (auto& e : impEntries) e.on = false;
    int ly = y + 42, lh = h - 108, rowH = 16, rows = lh / rowH;
    cv.fillRect(ex, ly, ew, lh, FIELD); cv.rect(ex, ly, ew, lh, BORDER);
    if (ui.hover(ex, ly, ew, lh) && in.wheel) impScroll -= in.wheel * 3;
    impScroll = clampi(impScroll, 0, std::max(0, (int)idx.size() - rows));
    cv.setClip({ex + 1, ly + 1, ew - 2, lh - 2});
    for (int r = 0; r < rows && impScroll + r < (int)idx.size(); r++) {
        ImpEntry& e = impEntries[idx[impScroll + r]]; int ry = ly + 1 + r * rowH;
        bool hv = ui.hover(ex, ry, ew - 8, rowH);
        if (hv) cv.fillRect(ex + 1, ry, ew - 9, rowH, HOVER);
        cv.fillRect(ex + 5, ry + 2, 12, 12, FIELD); cv.rect(ex + 5, ry + 2, 12, 12, BORDER); if (e.on) cv.fillRect(ex + 8, ry + 5, 6, 6, ACCENT);
        std::string tag = e.have ? (impKind == IMP_CAMP ? "  (replaces yours)" : "  (you have one - replaces it)") : "";
        int tw = tag.empty() ? 0 : font->width(tag);
        cv.setClip({ex + 22, ry, ew - 34 - tw, rowH}); ui.text(ex + 22, ry + 2, e.label, TEXT); cv.setClip({ex + 1, ly + 1, ew - 2, lh - 2});
        if (!tag.empty()) ui.text(ex + ew - 10 - tw, ry + 2, tag, ACCENT2);
        if (hv && in.mousePressed[0] && !ui.mouseUsed) { ui.mouseUsed = true; e.on = !e.on; }
    }
    cv.resetClip();
    if ((int)idx.size() > rows) { int th = std::max(10, lh * rows / (int)idx.size()), ty = ly + (lh - th) * impScroll / std::max(1, (int)idx.size() - rows); cv.fillRect(ex + ew - 7, ly + 1, 6, lh - 2, PANEL); cv.fillRect(ex + ew - 6, ty, 4, th, DIM); }
    if (impSrc >= 0 && impEntries.empty()) ui.text(ex + 10, ly + 8, impKind == IMP_KINDS ? "This campaign adds no tile kinds of its own." : "Nothing of this kind there.", DIM);
    int on = 0, rep = 0; for (auto& e : impEntries) if (e.on) { on++; if (e.have) rep++; }
    int by = y + h - 30;
    ui.text(ex, by + 4, fmt("%d of %d shown, %d ticked", (int)idx.size(), (int)impEntries.size(), on) + (rep ? fmt(" (%d replace yours)", rep) : ""), DIM);
    if (ui.button(x + w - 200, by, 110, 22, on ? fmt("Import %d", on) : "Import", on > 0, false, OK)) {
        if (rep && (impKind == IMP_MAPS || impKind == IMP_DLG || impKind == IMP_SCRIPTS)) openConfirm(fmt("%d of the files replace this campaign's own. Copy anyway?", rep), [this]() { doImport(); });
        else doImport();
        close = true;
    }
    if (ui.button(x + w - 84, by, 76, 22, "Cancel") || (in.pressed[K_ESCAPE] && !ui.typing())) close = true;
    (void)gEdTop;
}
