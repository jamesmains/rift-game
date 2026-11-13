// 1.1.9 (owner's #33: "a better NPC editor; convert all NPC objects to NPCs, companions too"): the NPCs tab.
// npcs.txt [npc] entries are the characters (look, name, what talking to them does, and for companions the class they
// join as, their party picture, level, stats, gear and spells). A map [npc] object is a placement: npc=<id> plus its
// spot, its own id on that map and when it is there (show / hide / moveif); every other key it doesn't set, or leaves
// empty, comes from the NPC. The game does the same in applyObjectTemplate (world.cpp).
#include "editor.h"
#include <filesystem>
#include <algorithm>

namespace fs = std::filesystem;
using namespace ED;

namespace {
bool placeKey(const std::string& k) { return k == "id" || k == "npc" || k == "template" || k == "x" || k == "y" || k == "show" || k == "hide" || k == "moveif" || k == "movex" || k == "movey"; }
std::string slugOf(const std::string& name) { std::string o; for (char ch : toLower(name)) { if (isalnum((unsigned char)ch)) o += ch; else if (!o.empty() && o.back() != '_') o += '_'; } while (!o.empty() && o.back() == '_') o.pop_back(); return o; }
std::string clipTo(Font* f, std::string s, int w) { if (f->width(s) <= w) return s; while (!s.empty() && f->width(s + "...") > w) s.pop_back(); return s + "..."; }
}

// every key an NPC can have, in the order the quick-add row offers them
const std::vector<std::string>& edNpcKeys() {
    static const std::vector<std::string> k = {"name", "sprite", "color", "dialogue", "shop", "craft", "board", "travel", "hostile", "wander", "notes", "companion", "partysprite", "level", "stats", "gear", "spells"};
    return k;
}

const EdSection* Editor::npcSec(const std::string& id0) const {
    std::string id = trim(id0); if (id.empty()) return nullptr;
    const EdSection* out = nullptr;
    for (auto& s : gens[17].secs) if (s.name == "npc" && trim(s.get("id")) == id) out = &s;   // a later one wins, as in the game
    if (out || !campaignInherit || dataRoot == baseRoot) return out;
    // a campaign with inherit=1 also gets the base game's NPCs (the game merges both files)
    if (!baseNpcsLoaded) { baseNpcsLoaded = true; baseNpcs.clear(); std::string bt; if (readFile(baseRoot + "/npcs.txt", bt)) for (auto& s : parseConfig(bt)) if (s.name == "npc") { EdSection e; e.name = s.name; e.kv = s.kv; baseNpcs.push_back(e); } }
    for (auto& s : baseNpcs) if (trim(s.get("id")) == id) out = &s;
    return out;
}
// the class a join= value adds: an NPC id with companion= (1.1.9), else class:Name:col,row
std::string Editor::joinClassOf(const std::string& v0) const {
    std::string v = trim(v0); if (v.empty()) return "";
    if (v.find(':') == std::string::npos) if (const EdSection* s = npcSec(v)) if (!trim(s->get("companion")).empty()) return trim(s->get("companion"));
    auto p = split(v, ':'); return p.empty() ? "" : trim(p[0]);
}
std::string Editor::newNpcId(const std::string& name) const {
    std::string base = slugOf(name); if (base.empty()) base = "npc";
    std::string id = base; for (int n = 2; npcSec(id); n++) id = base + "_" + std::to_string(n);
    return id;
}
// a new NPC in npcs.txt (from a placed object's own keys, or a plain villager); saved at once so the map can point at it
std::string Editor::newNpc(const EdObj* from) {
    std::string name = from ? trim(from->get("name")) : ""; if (name.empty()) name = "Villager";
    std::string id = newNpcId(name);
    EdSection s; s.name = "npc"; s.kv = {{"id", id}, {"name", name}};
    if (from) { for (auto& kv : from->kv) if (!placeKey(kv.first) && kv.first != "name" && !trim(kv.second).empty()) s.kv.push_back(kv); if (!s.has("sprite")) s.kv.push_back({"sprite", "104,0"}); }
    else { s.kv.push_back({"sprite", "104,0"}); s.kv.push_back({"dialogue", ""}); }
    auto& g = gens[17]; bool hadEdits = g.dirty;
    g.secs.push_back(s); g.dirty = true;
    if (g.save()) { changed = true; if (hadEdits) say("NPC '" + id + "' made. (Your other unsaved NPC changes were saved with it.)"); }
    else say("Could not save npcs.txt");
    poNeedScan = true; npcJoinsDirty = true;
    return id;
}
// the id a new placement of an NPC gets on a map: the NPC's id, or id_2, id_3... when that's taken there
std::string Editor::npcPlacementId(const EdMap& m, const std::string& npcId) const {
    auto taken = [&](const std::string& id) { for (auto& o : m.objs) if (o.type == "npc" && (o.get("id") == id || (o.get("id").empty() && o.get("npc") == id))) return true; return false; };
    std::string id = npcId.empty() ? "npc" : npcId; for (int n = 2; taken(id); n++) id = npcId + "_" + std::to_string(n);
    return id;
}
void Editor::startPlacingNpc(const std::string& id) {
    placeNpc = id; placeTmpl.clear(); objType = 0; mapTool = 3; tab = 0; ui.blur();
    const EdSection* s = npcSec(id);
    say("Click the map to place " + (s ? s->get("name", id) : id) + ". (Objects tool, npc - the NPC is picked under the type buttons.)");
}
// renames an NPC: its entry, every placement on every map and every join=<id> in the dialogue
void Editor::renameNpc(const std::string& from, const std::string& to0) {
    std::string to = slugOf(to0);
    if (to.empty() || to == from) return;
    if (npcSec(to)) { say("There already is an NPC '" + to + "'."); return; }
    auto& g = gens[17]; for (auto& s : g.secs) if (s.name == "npc" && trim(s.get("id")) == from) { s.set("id", to); s.inherited = false; }
    if (!g.save()) { say("Could not save npcs.txt"); return; }
    int maps = 0, objs = 0, dlgs = 0; bool openLeftDirty = false;
    for (auto& f : mapFiles) {
        bool open = mapLoaded && em.id == f; EdMap m; EdMap* mp = &em;
        if (!open) { if (!m.load(readPath("maps/" + f + ".txt"))) continue; m.path = dataRoot + "/maps/" + f + ".txt"; mp = &m; }
        int here = 0; for (auto& o : mp->objs) if (o.type == "npc" && trim(o.get("npc")) == from) { o.set("npc", to); here++; }
        if (!here) continue;
        objs += here; maps++;
        if (open) { if (mapDirty) openLeftDirty = true; else if (!em.save()) mapDirty = true; }
        else m.save();
    }
    for (auto& d : dlgFiles) {
        std::string p = readPath("dialogue/" + d + ".txt"), t; if (!readFile(p, t)) continue;
        auto lines = split(t, '\n'); bool ch = false;
        for (auto& l : lines) { std::string tl = trim(l); if (tl == "join=" + from) { l = "join=" + to + (!l.empty() && l.back() == '\r' ? "\r" : ""); ch = true; } }
        if (!ch) continue;
        std::string out; for (size_t i = 0; i < lines.size(); i++) { out += lines[i]; if (i + 1 < lines.size()) out += "\n"; }
        writeFile(dataRoot + "/dialogue/" + d + ".txt", out); dlgs++;
        if (!te.path.empty() && fs::path(te.path).stem().string() == d && !te.dirty) te.load(dataRoot + "/dialogue/" + d + ".txt");
    }
    if (placeNpc == from) placeNpc = to;
    changed = true; poNeedScan = true; npcJoinsDirty = true; flagIndexDirty = true;
    say(fmt("Renamed NPC %s to %s: %d placement(s) on %d map(s), %d dialogue file(s) with join=.", from.c_str(), to.c_str(), objs, maps, dlgs) + (openLeftDirty ? " The open map had unsaved changes: Save it in the Maps tab." : ""));
}
// which dialogue files add each companion (join= values, resolved to a class)
void Editor::scanNpcJoins() {
    npcJoins.clear(); npcJoinsDirty = false;
    for (auto& d : dlgFiles) {
        std::string t; if (!readFile(readPath("dialogue/" + d + ".txt"), t)) continue;
        for (auto& l : split(t, '\n')) { std::string tl = trim(l); if (!startsWith(tl, "join=")) continue;
            std::string v = trim(tl.substr(5)); std::string key = v.empty() ? "(speaker)" : v.find(':') == std::string::npos ? v : "class:" + joinClassOf(v);
            auto& lst = npcJoins[key]; if (std::find(lst.begin(), lst.end(), d) == lst.end()) lst.push_back(d); }
    }
}
// what's wrong with an NPC entry (shown in the NPCs tab and by Check all)
void Editor::npcProblems(const EdSection& s, std::vector<std::pair<bool, std::string>>& out) {
    auto has = [](const std::vector<std::string>& v, const std::string& x) { return std::find(v.begin(), v.end(), x) != v.end(); };
    std::string id = trim(s.get("id"));
    if (id.empty()) out.push_back({true, "has no id"});
    int same = 0; for (auto& o : gens[17].secs) if (o.name == "npc" && trim(o.get("id")) == id) same++;
    if (same > 1) out.push_back({true, "the id is used by another NPC too (the later one wins)"});
    std::string dlg = trim(s.get("dialogue")); if (!dlg.empty() && !has(dlgFiles, dlg)) out.push_back({true, "dialogue '" + dlg + "' does not exist"});
    std::string shop = trim(s.get("shop")); if (!shop.empty()) { bool ok = false; for (auto& sh : gens[4].secs) if (sh.get("id") == shop) ok = true; if (!ok) out.push_back({true, "shop '" + shop + "' does not exist"}); }
    std::string craft = trim(s.get("craft")); if (!craft.empty() && craft != "forge" && craft != "workbench" && craft != "alchemy" && craft != "any") out.push_back({true, "craft= should be forge, workbench, alchemy or any"});
    for (auto& c : split(s.get("hostile"), ',')) if (!trim(c).empty() && !gd->creature(trim(c))) out.push_back({true, "hostile= names unknown creature '" + trim(c) + "'"});
    if (!trim(s.get("sprite")).empty() && !parseTile(s.get("sprite")).valid()) out.push_back({true, "sprite is not col,row"});
    std::string cls = trim(s.get("companion"));
    if (!cls.empty()) {
        if (!gd->cls(cls)) out.push_back({true, "companion= names unknown class '" + cls + "'"});
        for (auto& o : gens[17].secs) if (&o != &s && o.name == "npc" && trim(o.get("companion")) == cls) { out.push_back({false, "NPC '" + o.get("id") + "' is a " + cls + " companion too: one companion per class can be in the party"}); break; }
        if (npcJoinsDirty) scanNpcJoins();
        if (!npcJoins.count(id) && (dlg.empty() || !npcJoins.count("(speaker)"))) out.push_back({false, "no dialogue has join=" + id + " yet, so they can't join"});
        for (auto& it : split(s.get("gear"), ',')) { std::string iid = trim(split(trim(it), ':')[0]); if (!iid.empty() && !gd->item(iid)) out.push_back({true, "gear= has unknown item '" + iid + "'"}); }
        for (auto& sp : split(s.get("spells"), ',')) if (!trim(sp).empty() && !gd->spell(trim(sp))) out.push_back({true, "spells= has unknown spell '" + trim(sp) + "'"});
        if (!trim(s.get("partysprite")).empty() && !parseTile(s.get("partysprite")).valid()) out.push_back({true, "partysprite is not col,row"});
    } else if (!trim(s.get("partysprite")).empty() || !trim(s.get("gear")).empty() || !trim(s.get("level")).empty() || !trim(s.get("stats")).empty() || !trim(s.get("spells")).empty())
        out.push_back({false, "partysprite / level / stats / gear / spells only count for a companion (companion= a class)"});
}

// ---------------------------------------------------------------- the box under the fields: where the NPC stands, what it does
void Editor::drawNpcInfo(int x, int y, int w, int h, InputState& in) {
    auto& g = gens[17];
    if (poNeedScan || mapScan.empty()) { scanMaps(); poNeedScan = false; }
    if (npcJoinsDirty) scanNpcJoins();
    EdSection* s = g.sel >= 0 && g.sel < (int)g.secs.size() ? &g.secs[g.sel] : nullptr;
    ui.cv->fillRect(x, y, w, h, PANEL2); ui.cv->rect(x, y, w, h, BORDER);
    std::map<std::string, int> placed; int total = 0, loose = 0;
    for (auto& ms : mapScan) for (size_t i = 0; i < ms.objs.size(); i++) if (ms.objs[i].type == "npc") { std::string n = trim(ms.objs[i].get("npc")); if (n.empty()) loose++; else placed[n]++; total++; }
    if (!s) {   // nothing picked: a summary and the companions
        int cy = y + 6;
        ui.text(x + 8, cy, fmt("%d NPCs, %d of them placed on the maps (%d placements).", (int)g.secs.size(), (int)placed.size(), total), TEXT); cy += 16;
        if (loose) { ui.text(x + 8, cy, fmt("%d map npc object(s) are not linked to an NPC: Maps tab > the object > make NPC.", loose), ACCENT2); cy += 16; }
        cy += 6; ui.text(x + 8, cy, "Companions (click one):", ACCENT); cy += 16;
        for (size_t i = 0; i < g.secs.size(); i++) { auto& c = g.secs[i]; if (trim(c.get("companion")).empty()) continue;
            std::string l = c.get("name", c.get("id")) + "  - " + c.get("companion"); bool hv = ui.hover(x + 6, cy - 1, w - 12, 14);
            if (hv) ui.cv->fillRect(x + 6, cy - 1, w - 12, 14, HOVER); ui.text(x + 12, cy, clipTo(font, l, w - 20), TEXT);
            if (ui.clicked(x + 6, cy - 1, w - 12, 14)) { g.sel = (int)i; g.fieldScroll = 0; ui.blur(); }
            cy += 15; if (cy > y + h - 16) break; }
        return;
    }
    std::string id = trim(s->get("id")), name = s->get("name", id);
    int cw = (w - 12) / 2, lx = x + 4, rx = x + 8 + cw;
    // ---- left column: placements
    struct P { std::string map; int obj; EdObj raw; };
    std::vector<P> ps;
    for (auto& ms : mapScan) for (size_t i = 0; i < ms.objs.size(); i++) if (ms.objs[i].type == "npc" && trim(ms.objs[i].get("npc")) == id) ps.push_back({ms.id, (int)i, ms.raw[i]});
    int cy = y + 4;
    ui.text(lx + 4, cy, fmt("Where %s stands (%d)", clipTo(font, name, cw - 120).c_str(), (int)ps.size()), ACCENT); helpBtn(lx + cw - 16, cy - 1, "tab.npcs"); cy += 15;
    int rowH = 28, listH = std::max(rowH, ((h - 44) / rowH) * rowH);
    ui.cv->fillRect(lx, cy, cw, listH, FIELD); ui.cv->rect(lx, cy, cw, listH, BORDER);
    if (ui.hover(lx, cy, cw, listH) && in.wheel) npcInfoScroll -= in.wheel;
    npcInfoScroll = clampi(npcInfoScroll, 0, std::max(0, (int)ps.size() - listH / rowH));
    if (ps.empty()) { ui.text(lx + 6, cy + 4, "Not on any map yet.", DIM); ui.text(lx + 6, cy + 16, "Place on map... puts it on one.", DIM); }
    for (int r = 0; r < listH / rowH && npcInfoScroll + r < (int)ps.size(); r++) {
        auto& p = ps[npcInfoScroll + r]; int ry = cy + 1 + r * rowH;
        bool hv = ui.hover(lx + 1, ry, cw - 2, rowH - 1);
        if (hv) ui.cv->fillRect(lx + 1, ry, cw - 2, rowH - 1, HOVER);
        std::string pid = p.raw.get("id").empty() ? id : p.raw.get("id");
        ui.text(lx + 5, ry + 1, clipTo(font, p.map + fmt(" at %d,%d", p.raw.geti("x"), p.raw.geti("y")) + "  (id " + pid + ")", cw - 10), TEXT);
        std::string d; auto addD = [&](const std::string& t) { d += (d.empty() ? "" : "; ") + t; };
        if (!trim(p.raw.get("show")).empty()) addD("only if " + p.raw.get("show"));
        if (!trim(p.raw.get("hide")).empty()) addD("gone when " + p.raw.get("hide"));
        if (!trim(p.raw.get("moveif")).empty()) addD("moves when " + p.raw.get("moveif"));
        std::string own; for (auto& kv : p.raw.kv) if (!placeKey(kv.first) && !trim(kv.second).empty()) own += (own.empty() ? "" : ", ") + kv.first;
        if (!own.empty()) addD("own " + own);
        if (!p.raw.get("template").empty()) addD("template " + p.raw.get("template"));
        ui.text(lx + 13, ry + 13, clipTo(font, d.empty() ? "always there" : d, cw - 18), DIM);
        if (hv) ui.tip("Open " + p.map + " in the Maps tab with this placement selected" + (d.empty() ? "" : ". " + d));
        if (ui.clicked(lx + 1, ry, cw - 2, rowH - 1)) { FlagUse u; u.tab = 0; u.file = p.map; u.obj = p.obj; jumpToUse(u); }
    }
    int by = cy + listH + 4;
    int bw = (cw - 8) / 3;
    if (ui.button(lx, by, bw, 18, "Place on map...", !id.empty(), false, ACCENT2)) {
        std::vector<std::string> ms = mapFiles; std::string nid = id; int cur = 0; for (size_t i = 0; i < ms.size(); i++) if (mapLoaded && ms[i] == em.id) cur = (int)i;
        if (ms.empty()) say("No maps yet - make one in the Maps tab.");
        else openListPicker("Place " + name + " on which map? (then click the spot)", ms, cur, [this, ms, nid](int i) {
            std::string f = ms[i];
            auto go = [this, f, nid]() { if (!mapLoaded || em.id != f) { for (size_t k = 0; k < mapFiles.size(); k++) if (mapFiles[k] == f) mapListSel = (int)k; loadMapFile(f); } startPlacingNpc(nid); };
            if (mapDirty && em.id != f) openConfirm("Discard unsaved changes to the current map?", go); else go(); });
    }
    if (ui.hover(lx, by, bw, 18)) ui.tip("Open a map with the Objects tool set to place this NPC: click where it should stand");
    std::string dlg = trim(s->get("dialogue")); bool dlgOk = !dlg.empty() && std::find(dlgFiles.begin(), dlgFiles.end(), dlg) != dlgFiles.end();
    if (ui.button(lx + bw + 4, by, bw, 18, "Open dialogue", dlgOk)) { FlagUse u; u.tab = 11; u.file = dlg; u.line = 0; jumpToUse(u); }
    if (ui.hover(lx + bw + 4, by, bw, 18)) ui.tip(dlgOk ? "Edit " + dlg + " in the Dialogue tab" : "Give it a dialogue= first (New in the Dialogue tab makes a file)");
    if (ui.button(lx + 2 * (bw + 4), by, cw - 2 * (bw + 4), 18, "Rename id...", !id.empty() && !s->inherited)) { std::string from = id; openTextPrompt("New id for " + name + " (maps and join= follow)", id, [this, from](const std::string& nm) { renameNpc(from, nm); }); }
    if (ui.hover(lx + 2 * (bw + 4), by, cw - 2 * (bw + 4), 18)) ui.tip("Changes the id here, on every placement and in every join=. (Each placement's own id stays: saved games use it.)");
    // ---- right column: what talking to it does, companion, problems
    cy = y + 4;
    // Game::interactAt: hostile > board > travel > craft without dialogue > dialogue > shop
    ui.text(rx, cy, "Talking to " + clipTo(font, name, cw - 80) + ":", ACCENT); cy += 14;
    std::string act;
    if (!trim(s->get("hostile")).empty()) act = "a fight with " + s->get("hostile");
    else if (!trim(s->get("board")).empty()) act = "the notice board of " + s->get("board");
    else if (s->get("travel") == "1") act = "coach travel (the world map)";
    else if (!trim(s->get("craft")).empty() && dlg.empty()) act = "the " + s->get("craft") + " crafting station";
    else if (!dlg.empty()) act = "the dialogue " + dlg + (trim(s->get("craft")).empty() ? "" : " (craft= only counts without one)");
    else if (!trim(s->get("shop")).empty()) act = "the shop " + s->get("shop");
    else act = "\"" + name + " has nothing to say.\"";
    ui.text(rx + 8, cy, clipTo(font, "opens " + act + (s->get("wander") == "1" ? "; wanders" : ""), cw - 10), TEXT); cy += 18;
    std::string cls = trim(s->get("companion"));
    if (!cls.empty()) {
        const ClassDef* cd = gd->cls(cls);
        Tile pt = parseTile(trim(s->get("partysprite")).empty() ? s->get("sprite") : s->get("partysprite"));
        ui.cv->fillRect(rx, cy, 28, 28, C::Black); ui.cv->rect(rx - 1, cy - 1, 30, 30, BORDER); if (pt.valid()) { ts->drawBacking(*ui.cv, pt, rx + 2, cy + 2, 2); ts->draw(*ui.cv, pt, rx + 2, cy + 2, 2); }
        if (ui.hover(rx, cy, 28, 28)) ui.tip("In the party and in fights (partysprite=, else sprite=)");
        std::string lv = trim(s->get("level"));
        ui.text(rx + 36, cy, clipTo(font, "Companion: joins as " + (cd ? cd->name : cls) + (lv.empty() || atoi(lv.c_str()) <= 0 ? ", hero's level" : ", level " + lv), cw - 38), ACCENT);
        std::string gear = trim(s->get("gear")); ui.text(rx + 36, cy + 14, clipTo(font, gear.empty() ? "with the class's starting gear" : "wearing " + gear, cw - 38), DIM);
        cy += 32;
        auto it = npcJoins.find(id);
        if (it != npcJoins.end()) { std::string l; for (auto& d : it->second) l += (l.empty() ? "" : ", ") + d;
            bool hv = ui.hover(rx, cy - 1, cw, 14); if (hv) ui.cv->fillRect(rx, cy - 1, cw, 14, HOVER);
            ui.text(rx + 8, cy, clipTo(font, "join=" + id + " in " + l, cw - 10), DIM);
            if (hv) ui.tip("Click: open " + it->second[0] + " at its join= line");
            if (ui.clicked(rx, cy - 1, cw, 14)) { FlagUse u; u.tab = 11; u.file = it->second[0]; u.line = 0; std::string t; if (readFile(readPath("dialogue/" + u.file + ".txt"), t)) { auto ls = split(t, '\n'); for (size_t i = 0; i < ls.size(); i++) if (trim(ls[i]) == "join=" + id) { u.line = (int)i; break; } } jumpToUse(u); }
        } else ui.text(rx + 8, cy, clipTo(font, "joins with the dialogue action join=" + id, cw - 10), ACCENT2);
        cy += 14;
        ui.text(rx + 8, cy, clipTo(font, "party talk (T): [camp] in " + (dlg.empty() ? "its dialogue" : dlg), cw - 10), DIM); cy += 18;
    } else { ui.text(rx, cy, clipTo(font, "Not a companion (companion= a class makes one).", cw), DIM); cy += 18; }
    std::vector<std::pair<bool, std::string>> probs; npcProblems(*s, probs);
    if (probs.empty()) { ui.text(rx, cy, "No problems found.", OK); return; }
    for (auto& p : probs) { if (cy > y + h - 14) break; ui.text(rx, cy, clipTo(font, (p.first ? "! " : "- ") + p.second, cw), p.first ? DANGER : ACCENT2); if (ui.hover(rx, cy, cw, 14)) ui.tip(p.second); cy += 14; }
}
