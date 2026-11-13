// Editor cross-tab tools: map <-> tile kind jumping, dungeon baking, playtesting, dialogue linting, the Sounds tab.
#include "editor.h"
#include "../game/tiles.h"
#include <filesystem>
#include <algorithm>

namespace fs = std::filesystem;
using namespace ED;
extern int gEdTop;
#define TOP gEdTop
static const int BOTTOM = 18;

// ---------------- map view -> Tiles tab ----------------
void Editor::jumpToKind(const std::string& name) {
    auto& kinds = editableTileKinds();
    for (size_t i = 0; i < kinds.size(); i++) if (kinds[i].name == name) { kindSel = (int)i; kindFilter.clear(); kindScroll = std::max(0, kindSel - 5); kindReturnTab = tab; tab = 1; ui.blur(); say("Editing tile kind '" + name + "' - press Back (or Esc) to return to the map."); return; }
    say("Unknown tile kind '" + name + "'.");
}

// ---------------- generated map -> map file ----------------
static void mapToEd(const Map& m, EdMap& e, const std::string& id, bool skipExits) {
    e.w = m.w; e.h = m.h; e.cells.resize((size_t)m.w * m.h);
    const auto& kinds = allTileKinds();
    for (int y = 0; y < m.h; y++) for (int x = 0; x < m.w; x++) { const Cell& c = m.at(x, y); e.cells[(size_t)y * m.w + x] = (c.kind < kinds.size()) ? kinds[c.kind].name : "void"; }
    auto xy = [](EdObj& o, int x, int y) { o.set("x", std::to_string(x)); o.set("y", std::to_string(y)); };
    for (auto& n : m.npcs) { EdObj o; o.type = "npc"; o.set("id", n.id); o.set("name", n.name); xy(o, n.x, n.y); o.set("sprite", fmt("%d,%d", n.tile.c, n.tile.r)); if (!n.dialogue.empty()) o.set("dialogue", n.dialogue); if (!n.shop.empty()) o.set("shop", n.shop); if (n.recolor) o.set("color", fmt("%d,%d,%d", n.color.r, n.color.g, n.color.b)); e.objs.push_back(o); }
    for (auto& g : m.enemies) { EdObj o; o.type = "enemy"; std::string grp; for (size_t i = 0; i < g.creatures.size(); i++) grp += (i ? "," : "") + g.creatures[i]; o.set("group", grp); xy(o, g.x, g.y); o.set("aggro", std::to_string(g.aggro)); if (g.levelBonus) o.set("level", std::to_string(g.levelBonus)); if (!g.flag.empty()) o.set("flag", g.flag); if (!g.onDefeatScript.empty()) o.set("ondefeat", g.onDefeatScript); if (!g.link.empty()) o.set("link", g.link); e.objs.push_back(o); }
    for (auto& c : m.chests) { EdObj o; o.type = "chest"; xy(o, c.x, c.y); std::string items; for (size_t i = 0; i < c.items.size(); i++) items += (i ? "," : "") + c.items[i].first + ":" + std::to_string(c.items[i].second); o.set("items", items); if (c.gold) o.set("gold", std::to_string(c.gold)); if (!c.flag.empty()) o.set("flag", c.flag); else o.set("flag", fmt("%s_chest_%d_%d", id.c_str(), c.x, c.y)); if (!c.locked.empty()) o.set("locked", c.locked); e.objs.push_back(o); }
    if (!skipExits) for (auto& x : m.exits) { EdObj o; o.type = "exit"; xy(o, x.x, x.y); if (x.w > 1) o.set("w", std::to_string(x.w)); if (x.h > 1) o.set("h", std::to_string(x.h)); o.set("to", x.to); o.set("tx", std::to_string(x.tx)); o.set("ty", std::to_string(x.ty)); if (!x.requireFlag.empty()) o.set("require", x.requireFlag); if (!x.requireItem.empty()) o.set("item", x.requireItem); if (!x.lockedMsg.empty()) o.set("locked", x.lockedMsg); e.objs.push_back(o); }
    for (auto& t : m.triggers) { EdObj o; o.type = "trigger"; xy(o, t.x, t.y); if (t.w > 1) o.set("w", std::to_string(t.w)); if (t.h > 1) o.set("h", std::to_string(t.h)); o.set("script", t.script); if (!t.flag.empty()) o.set("flag", t.flag); if (!t.once) o.set("once", "0"); if (!t.requireFlag.empty()) o.set("require", t.requireFlag); e.objs.push_back(o); }
    for (auto& s : m.signs) { EdObj o; o.type = "sign"; xy(o, s.x, s.y); o.set("text", s.text); e.objs.push_back(o); }
    for (auto& d : m.doors) { EdObj o; o.type = "door"; xy(o, d.x, d.y); if (!d.key.empty()) o.set("key", d.key); if (!d.flag.empty()) o.set("flag", d.flag); if (d.openTile.c == TL::DOOR_METAL.c && d.openTile.r == TL::DOOR_METAL.r) o.set("metal", "1"); e.objs.push_back(o); }
    for (auto& s : m.savepoints) { EdObj o; o.type = "savepoint"; xy(o, s.x, s.y); e.objs.push_back(o); }
    for (auto& g : m.gathers) { EdObj o; o.type = "gather"; xy(o, g.x, g.y); const ResourceDef* r = g.node.empty() ? nullptr : resourceFor(g.node, "");
        if (r) { o.set("node", r->id); if (r->lo != g.lo || r->hi != g.hi) o.set("count", fmt("%d-%d", g.lo, g.hi)); e.objs.push_back(o); continue; }   // 1.1.0: linked to its resource type
        o.set("item", g.item); o.set("count", fmt("%d-%d", g.lo, g.hi)); if (!g.tool.empty()) o.set("tool", g.tool); o.set("respawn", std::to_string(g.respawn)); e.objs.push_back(o); }
}
bool Editor::bakeDungeon(const EdSection& sec, std::string& err) {
    std::string id = sec.get("id"); if (id.empty()) { err = "dungeon has no id"; return false; }
    Map m;
    std::vector<std::string> pool; for (auto& p : split(sec.get("monsters"), ',')) if (!trim(p).empty()) pool.push_back(trim(p));
    auto ex = split(sec.get("exit"), ',');
    std::string style = sec.get("style", "cave");
    int seed = worldSeed + atoi(sec.get("seed", "1").c_str());
    generateDungeon(m, id, style, atoi(sec.get("w", "48").c_str()), atoi(sec.get("h", "40").c_str()), seed, atoi(sec.get("level", "1").c_str()),
                    ex.size() > 0 ? trim(ex[0]) : "overworld", ex.size() > 2 ? atoi(ex[1].c_str()) : 0, ex.size() > 2 ? atoi(ex[2].c_str()) : 0,
                    pool, sec.get("boss"), sec.get("bossflag"), sec.get("loot", "common"));
    if (!sec.get("bossdefeat").empty()) for (auto& g : m.enemies) if (g.flag == sec.get("bossflag")) g.onDefeatScript = sec.get("bossdefeat");
    // the boss's escorts and the closest groups come as one
    EdMap e; e.id = id; e.path = dataRoot + "/maps/" + id + ".txt";
    e.header = {{"id", id}, {"name", sec.get("name", id)}, {"kind", "dungeon"}, {"spawn", fmt("%d,%d", m.spawnX, m.spawnY)}, {"dark", m.dark ? "1" : "0"}, {"light", std::to_string(m.lightRadius)}, {"bg", "0,0,0"},
                {"music", sec.get("music", style == "tech" || style == "ship" ? "tech" : "dungeon")}};
    if (!sec.get("table").empty()) e.header.push_back({"table", sec.get("table")});
    mapToEd(m, e, id, false);
    if (!e.save()) { err = "could not write " + e.path; return false; }
    refreshFiles();
    for (size_t i = 0; i < mapFiles.size(); i++) if (mapFiles[i] == id) mapListSel = (int)i;
    loadMapFile(id);
    changed = true;
    return true;
}
// The generated overworld terrain, roads, roaming monsters and wild chests -> maps/overworld.txt. Location entrances
// stay in overworld.txt (World tab) and are stamped onto the map when it loads, so moving one never needs repainting.
void Editor::bakeOverworld() {
    Map m; generateOverworld(m, worldSeed, readPath("overworld.txt"));
    EdMap e; e.id = "overworld"; e.path = dataRoot + "/maps/overworld.txt";
    e.header = {{"id", "overworld"}, {"name", "Aldmoor"}, {"kind", "overworld"}, {"spawn", fmt("%d,%d", m.spawnX, m.spawnY)}, {"encounter", std::to_string(m.encounterRate)}, {"table", m.encounterTable}, {"bg", "58,112,52"}};
    mapToEd(m, e, "overworld", true);
    std::error_code ec; fs::create_directories(dataRoot + "/maps", ec);
    if (!e.save()) { say("Could not write " + e.path); return; }
    refreshFiles(); for (size_t i = 0; i < mapFiles.size(); i++) if (mapFiles[i] == "overworld") mapListSel = (int)i;
    loadMapFile("overworld"); changed = true; worldNeedsGen = true;
    say("Baked the overworld into maps/overworld.txt - paint it in the Maps tab.");
}

// ---------------- playtest ----------------
bool Editor::saveAllDirty() {
    bool ok = true;
    if (mapLoaded && mapDirty) { if (em.save()) { mapDirty = false; changed = true; } else ok = false; }
    if (kindsDirty) { rebuildTileKindIndex(); if (saveTileKinds(kindsPath())) { kindsDirty = false; changed = true; } else ok = false; }
    for (auto& g : gens) if (g.dirty) { if (g.save()) changed = true; else ok = false; }
    if (te.dirty && !te.path.empty()) { if (te.save()) changed = true; else ok = false; }
    if (sndDirty) { if (gAudio.saveDefs()) sndDirty = false; else ok = false; }
    if (flagDocsDirty && !saveFlagDocs()) ok = false;
    if (play.dirty && !savePlayState()) ok = false;
    return ok;
}
void Editor::requestPlay(int x, int y) {
    if (!mapLoaded) { say("Open a map first."); return; }
    if (!saveAllDirty()) { say("Could not save everything - not starting the playtest."); return; }
    playRequest = true; playMap = em.id; playX = x; playY = y; changed = true; wantsClose = true;
}

// ---------------- dialogue lint ----------------
// every flag something in the campaign can set (the Flags tab's index: dialogue, maps, dungeons, campaign start...)
std::set<std::string> Editor::knownFlags() {
    ensureIndex();
    std::set<std::string> flags;
    for (auto& kv : flagIndex) for (auto& u : kv.second) if (u.set) { flags.insert(kv.first); break; }
    // the open dialogue file as it is right now (the index may predate the last keystrokes)
    if (!te.path.empty()) for (auto& l0 : te.lines) { std::string l = trim(l0); size_t eq = l.find('='); if (eq == std::string::npos) continue; std::string k = trim(l.substr(0, eq)), v = trim(l.substr(eq + 1));
        if (k == "set" || k == "clear") { for (auto& f : split(v, ',')) { std::string n = trim(split(trim(f), '=')[0]); if (!n.empty()) flags.insert(n); } } else if (k == "flag" || k == "inc") flags.insert(v); }
    for (auto& c : gd->classes) { flags.insert("joined_" + c.first); flags.insert("left_" + c.first); }   // left_: a companion who walked out (1.1.2)
    flags.insert("room");   // condition: a free place in the party
    flags.insert("game_won"); flags.insert("reinforced3");
    return flags;
}

std::vector<Editor::LintItem> Editor::lintDialogue(const std::string& text, const std::string& id) {
    std::vector<LintItem> out;
    auto err = [&](int line, const std::string& m) { out.push_back({line, m, false}); };
    auto warn = [&](int line, const std::string& m) { out.push_back({line, m, true}); };
    auto lines = split(text, '\n');
    std::set<std::string> nodes; std::map<std::string, int> nodeLine; std::set<std::string> targeted;
    std::string cur; int curLine = -1; bool curHasOpt = false, curHasNext = false, curClosed = false;
    auto endNode = [&]() { if (cur.empty()) return; if (!curHasOpt && !curHasNext && !curClosed) warn(curLine, "node [" + cur + "] has no option= or next= (it will just end the conversation)"); };
    for (size_t i = 0; i < lines.size(); i++) { std::string l = trim(lines[i]); if (l.size() >= 2 && l[0] == '[' && l.back() == ']') { std::string n = l.substr(1, l.size() - 2); if (nodes.count(n)) err((int)i, "duplicate node [" + n + "]"); nodes.insert(n); nodeLine[n] = (int)i; } }
    if (!nodes.count("start")) err(0, "no [start] node - the dialogue cannot begin");
    std::set<std::string> flags = knownFlags();
    std::vector<std::string> mapIds{"overworld"}; for (auto& m : mapFiles) mapIds.push_back(m); for (auto& d : gens[7].secs) mapIds.push_back(d.get("id"));
    auto isMap = [&](const std::string& m) { return std::find(mapIds.begin(), mapIds.end(), m) != mapIds.end(); };
    auto checkCond = [&](int line, const std::string& cond) {
        for (auto& part : split(cond, '&')) { std::string c = trim(part); if (c.empty()) continue; if (c[0] == '!') c = c.substr(1);
            if (startsWith(c, "item:")) { if (!gd->item(c.substr(5))) err(line, "condition uses unknown item '" + c.substr(5) + "'"); }
            else if (startsWith(c, "gold:") || startsWith(c, "level:")) { if (atoi(c.substr(c.find(':') + 1).c_str()) <= 0) warn(line, "condition '" + c + "' has no positive number"); }
            else if (startsWith(c, "approval:") || startsWith(c, "kills:") || startsWith(c, "itemn:")) { auto p = split(c.substr(c.find(':') + 1), ':'); if (startsWith(c, "approval:") && !gd->cls(p[0])) err(line, "approval: uses unknown class '" + p[0] + "'"); if (startsWith(c, "kills:") && !gd->creature(p[0])) err(line, "kills: uses unknown creature '" + p[0] + "'"); if (startsWith(c, "itemn:") && !gd->item(p[0])) err(line, "itemn: uses unknown item '" + p[0] + "'"); }
            else if (startsWith(c, "rep:")) { auto p = split(c.substr(4), ':'); if (!knownFaction(p[0])) warn(line, "rep: uses faction '" + p[0] + "' that is not in factions.txt or a board id"); }
            else if (startsWith(c, "party:")) { std::string p = c.substr(6); bool ok = gd->cls(p) != nullptr; if (!ok) warn(line, "condition party:" + p + " is not a class id (it will match a member's name only)"); }
            else if (startsWith(c, "map:")) { if (!isMap(c.substr(4))) warn(line, "condition map: names a map that does not exist ('" + c.substr(4) + "')"); }
            else if (startsWith(c, "equipped:")) { if (!gd->item(c.substr(9))) err(line, "condition uses unknown item '" + c.substr(9) + "'"); }
            else if (startsWith(c, "mode:") || startsWith(c, "fav:") || startsWith(c, "weather:") || c == "lit" || c == "night" || c == "day" || c == "room") { }   // 1.1.7: game states, not flags
            else { std::string f = c; size_t ge = f.find(">="); if (ge != std::string::npos) f = trim(f.substr(0, ge)); if (!flags.count(f)) warn(line, "flag '" + f + "' is never set by any dialogue, trigger, chest, enemy or dungeon"); } }
    };
    for (size_t i = 0; i < lines.size(); i++) {
        std::string l = trim(lines[i]); int ln = (int)i;
        if (l.empty() || l[0] == '#') continue;
        if (l[0] == '[' && l.back() == ']') { endNode(); cur = l.substr(1, l.size() - 2); curLine = ln; curHasOpt = curHasNext = curClosed = false; continue; }
        size_t eq = l.find('='); if (eq == std::string::npos) { err(ln, "not a key=value line"); continue; }
        if (cur.empty()) { err(ln, "key=value before the first [node]"); continue; }
        std::string k = trim(l.substr(0, eq)), v = trim(l.substr(eq + 1));
        if (k == "option") {
            curHasOpt = true; auto p = split(v, '|');
            if (p.size() < 2) { err(ln, "option needs Label|target (got '" + v + "')"); continue; }
            std::string tgt = trim(p[1]); if (tgt != "end" && !nodes.count(tgt)) err(ln, "option leads to unknown node '" + tgt + "'"); else targeted.insert(tgt);
            if (p.size() > 2) checkCond(ln, p[2]);
            if (p.size() > 3) warn(ln, "option has more than 3 fields (extra '|' in the label?)");
        } else if (k == "next") { curHasNext = true; if (v != "end" && !nodes.count(v)) err(ln, "next leads to unknown node '" + v + "'"); else targeted.insert(v); }
        else if (k == "speaker" || k == "text" || k == "log" || k == "notify" || k == "quest" || k == "message" || k == "cutscene" || k == "win" || k == "rest" || k == "heal" || k == "savepoint") { if (k == "win" || k == "cutscene") curClosed = true; if (v.empty() && k != "rest" && k != "heal") warn(ln, k + "= is empty"); }
        else if (k == "set" || k == "flag" || k == "inc" || k == "clear") { if (v.empty()) err(ln, k + "= needs a flag name"); }
        else if (k == "give" || k == "take" || k == "item") { for (auto& it : split(v, ',')) { std::string idn = trim(split(trim(it), ':')[0]); if (idn.empty()) continue; if (!gd->item(idn)) err(ln, k + "= uses unknown item '" + idn + "'"); } }
        else if (k == "gold" || k == "xp") { if (atoi(v.c_str()) == 0) warn(ln, k + "= is zero"); }
        else if (k == "shop") { bool ok = false; for (auto& s : gens[4].secs) if (s.get("id") == v) ok = true; if (!ok) err(ln, "unknown shop '" + v + "'"); curClosed = true; }
        else if (k == "join") { std::string c = joinClassOf(v); if (trim(v).empty()) {} else   // 1.1.9: join=<npc id> (an NPC with companion=) or class:Name:col,row; empty = the NPC being talked to
            if (trim(v).find(':') == std::string::npos && npcSec(v) && c == trim(v)) err(ln, "join=" + trim(v) + ": that NPC has no companion= class (NPCs tab)");
            else if (!gd->cls(c)) err(ln, trim(v).find(':') == std::string::npos ? "join= names no companion NPC or class '" + trim(v) + "'" : "join= uses unknown class '" + c + "'"); }
        else if (k == "leave") { if (!gd->cls(v)) err(ln, "leave= uses unknown class '" + v + "'"); }
        else if (k == "combat") { curClosed = true; for (auto& c : split(v, ',')) if (!trim(c).empty() && !gd->creature(trim(c))) err(ln, "combat= uses unknown creature '" + trim(c) + "'"); }
        else if (k == "spawn") { auto p = split(v, ','); if (p.size() < 3) err(ln, "spawn needs creature,x,y"); else if (!gd->creature(trim(p[0]))) err(ln, "spawn= uses unknown creature '" + trim(p[0]) + "'"); }
        else if (k == "teleport") { curClosed = true; auto p = split(v, ','); if (p.size() < 3) err(ln, "teleport needs map,x,y"); else if (!isMap(trim(p[0]))) err(ln, "teleport to unknown map '" + trim(p[0]) + "'"); }
        else if (k == "dialogue") { curClosed = true; if (std::find(dlgFiles.begin(), dlgFiles.end(), v) == dlgFiles.end() && v != id) err(ln, "dialogue= refers to missing file '" + v + "'"); }
        else if (k == "hidenpc" || k == "shownpc") { if (v.empty()) err(ln, k + "= needs an npc id"); }
        else if (k == "rep") { for (auto& part : split(v, ',')) { auto p = split(trim(part), ':'); if (p.size() < 2 || atoi(p[1].c_str()) == 0) err(ln, "rep needs faction:amount (amount not zero)"); else if (!knownFaction(trim(p[0]))) warn(ln, "rep= faction '" + trim(p[0]) + "' is not in factions.txt or a board id"); } }
        else if (k == "amends") { auto p = split(v, ':'); if (p.size() < 2 || !gd->cls(trim(p[0]))) err(ln, "amends needs class:amount with a known class"); }
        else if (k == "craftxp") { auto p = split(v, ':'); std::string st = trim(p[0]); if (p.size() < 2 || (st != "forge" && st != "alchemy" && st != "workbench" && st != "field")) err(ln, "craftxp needs forge|alchemy|workbench|field:amount"); }
        else if (k == "approve") { for (auto& part : split(v, ',')) { auto p = split(trim(part), ':'); if (p.size() < 2) err(ln, "approve needs class:amount"); else if (trim(p[0]) != "all" && !gd->cls(trim(p[0]))) err(ln, "approve= uses unknown class '" + trim(p[0]) + "'"); } }
        else if (k == "craft") { curClosed = true; if (v != "forge" && v != "alchemy" && v != "workbench" && v != "any" && v != "1") warn(ln, "craft= should be forge, alchemy, workbench or any"); }
        else if (k == "board") { curClosed = true; }
        else if (k == "finish") { curClosed = true; std::string c = trim(split(v, '|')[0]); bool known = c.empty(); for (auto& ci : listCampaigns(assetDir)) if (ci.id == c) known = true; if (!known) err(ln, "finish= names unknown campaign '" + c + "'"); }
        else if (k == "travel" || k == "respec" || k == "healer") { curClosed = true; }
        else if (k == "note") { if (v.empty()) warn(ln, "note= is empty"); }
        else if (k == "npcmove") { if (split(v, ',').size() < 3) err(ln, "npcmove needs npc_id,x,y"); }
        else if (k == "revealmap") { auto p = split(v, ','); std::string m = trim(p[0]); if (!m.empty() && m != "here" && !isMap(m)) err(ln, "revealmap= unknown map '" + m + "'"); if (p.size() != 1 && p.size() != 4) warn(ln, "revealmap needs map or map,x,y,radius"); }
        else if (k == "mapset") { auto p = split(v, ','); if (p.size() < 3) err(ln, "mapset needs x,y,tilekind"); else if (!tileKind(trim(p[2]))) err(ln, "mapset uses unknown tile kind '" + trim(p[2]) + "'"); }
        else warn(ln, "unknown key '" + k + "'");
    }
    endNode();
    for (auto& n : nodes) if (n != "start" && n != "camp" && !targeted.count(n)) warn(nodeLine[n], "node [" + n + "] is never reached from this file (ok if another dialogue jumps here)");
    // NPC hookup
    bool used = false; std::string text2;
    for (auto& f : mapFiles) if (readFile(readPath("maps/" + f + ".txt"), text2)) if (text2.find("dialogue=" + id + "\n") != std::string::npos || text2.find("dialogue=" + id + "\r") != std::string::npos) used = true;
    // scripts (map triggers, ondefeat=, dungeon bossdefeat=) open dialogues with dialogue:ID
    for (auto& f : mapFiles) if (readFile(readPath("maps/" + f + ".txt"), text2)) { size_t at = 0; while ((at = text2.find("dialogue:" + id, at)) != std::string::npos) { size_t e = at + 9 + id.size(); if (e >= text2.size() || !(isalnum((unsigned char)text2[e]) || text2[e] == '_')) used = true; at = e; } }
    if (readFile(readPath("dungeons.txt"), text2) && text2.find("dialogue:" + id) != std::string::npos) used = true;
    for (auto& f : dlgFiles) if (f != id && readFile(readPath("dialogue/" + f + ".txt"), text2)) if (text2.find("dialogue=" + id) != std::string::npos) used = true;
    { std::string ct; if (readFile(readPath("campaign.txt"), ct) && (ct.find("dialogue=" + id + "\n") != std::string::npos || ct.find("dialogue=" + id + "\r") != std::string::npos)) used = true;   // the campaign's opening dialogue
      size_t at = 0; while ((at = ct.find("dialogue:" + id, at)) != std::string::npos) { size_t e = at + 9 + id.size(); if (e >= ct.size() || !(isalnum((unsigned char)ct[e]) || ct[e] == '_')) used = true; at = e; } }   // 1.1.7: or its defeat= script
    if (!used && id != "start" && id != "intro") warn(0, "no NPC on any map (and no other dialogue) uses '" + id + "'");
    std::stable_sort(out.begin(), out.end(), [](const LintItem& a, const LintItem& b) { return a.warn < b.warn; });
    return out;
}

// ---------------- Sounds tab ----------------
void Editor::drawSounds(InputState& in) {
    int x = 4, y = TOP + 4, h = H - TOP - BOTTOM - 8, lw = 250;
    ui.panel(x, y, lw, h); ui.header(x, y, lw, fmt("Sounds & music (%d)", (int)gAudio.order.size())); helpBtn(x + lw - 16, y + 1, "tab.sounds");
    ui.searchField(x + 4, y + 20, lw - 8, sndFilter, "search: music, ambient, a name...");
    std::vector<int> idx; std::vector<std::string> labels;
    for (size_t i = 0; i < gAudio.order.size(); i++) { auto it = gAudio.defs.find(gAudio.order[i]); if (it == gAudio.defs.end()) continue; if (edMatch(it->first + " " + it->second.kind + " " + it->second.desc, sndFilter)) { idx.push_back((int)i); labels.push_back((it->second.kind == "music" ? "[music] " : it->second.kind == "ambient" ? "[ambient] " : "") + it->first); } }
    int lsel = -1; for (size_t i = 0; i < idx.size(); i++) if (idx[i] == sndSel) lsel = (int)i;
    int hit = ui.listBox(x + 4, y + 40, lw - 8, h - 124, labels, lsel, sndScroll);
    if (hit >= 0) { sndSel = idx[hit]; ui.blur(); }
    int by = y + h - 80;
    auto newDef = [&](const std::string& kind) { openTextPrompt(kind == "music" ? "New music track id" : "New sound event id", kind == "music" ? "my_track" : "my_sound", [this, kind](const std::string& nm) { std::string n = nm; for (auto& c : n) if (c == ' ') c = '_'; if (gAudio.defs.count(n)) { say("A sound with that id already exists."); return; } SoundDef d; d.id = n; d.kind = kind; d.volume = kind == "music" ? 70 : 90; gAudio.defs[n] = d; gAudio.order.push_back(n); sndSel = (int)gAudio.order.size() - 1; sndScroll = std::max(0, sndSel - 8); sndFilter.clear(); sndDirty = true; }); };
    if (ui.button(x + 4, by, 76, 20, "New sound")) newDef("sfx");
    if (ui.button(x + 84, by, 76, 20, "New music")) newDef("music");
    bool have = sndSel >= 0 && sndSel < (int)gAudio.order.size() && gAudio.defs.count(gAudio.order[sndSel]);
    if (ui.button(x + 164, by, 80, 20, "Delete", have, false, DANGER)) { std::string id = gAudio.order[sndSel]; openConfirm("Delete sound '" + id + "'? Game events that ask for it will be silent.", [this, id]() { gAudio.defs.erase(id); gAudio.order.erase(std::remove(gAudio.order.begin(), gAudio.order.end(), id), gAudio.order.end()); sndSel = std::min(sndSel, (int)gAudio.order.size() - 1); sndDirty = true; }); }
    if (ui.button(x + 4, by + 26, 70, 20, "Save", sndDirty, false, OK)) { if (gAudio.saveDefs()) { sndDirty = false; changed = true; say("Saved sounds.txt"); } else say("Save failed!"); }
    if (ui.button(x + 80, by + 26, 70, 20, "Revert", sndDirty)) { gAudio.loadDefs(); sndDirty = false; say("Reverted."); }
    if (ui.button(x + 156, by + 26, 88, 20, "Stop music", !gAudio.currentMusic.empty())) { gAudio.music(""); previewingMusic = false; }
    if (in.ctrl() && in.pressed[K_S] && sndDirty && pop == P_NONE) { if (gAudio.saveDefs()) { sndDirty = false; changed = true; say("Saved sounds.txt"); } }
    ui.text(x + 4, by + 52, "Files live in assets/audio (wav/ogg/mp3).", DIM);
    // form
    int fx = x + lw + 6, fw = W - fx - 4;
    ui.panel(fx, y, fw, h);
    if (!have) { ui.text(fx + 10, y + 10, "Select a sound on the left. Every game event plays whatever file(s) its entry points at; music tracks are chosen by map kind, the map's music= field, or a dungeon's music= field.", DIM); return; }
    SoundDef& d = gAudio.defs[gAudio.order[sndSel]];
    ui.header(fx, y, fw, (d.kind == "music" ? "Music track: " : d.kind == "ambient" ? "Ambient loop: " : "Sound event: ") + d.id);
    int cy = y + 26, keyW = 110;
    auto sh = [&](const char* key, int yy) { helpBtn(fx + keyW - 18, yy + 1, std::string("sound.") + key, "tab.sounds"); };
    sh("id", cy); sh("kind", cy + 24); sh("desc", cy + 48); sh("files", cy + 72);
    ui.label(fx + 10, cy + 2, "id"); { std::string nid = d.id; if (ui.textField(fx + keyW, cy, 200, nid, 40)) { std::string n = nid; for (auto& c : n) if (c == ' ') c = '_'; if (!n.empty() && !gAudio.defs.count(n)) { SoundDef copy = d; copy.id = n; gAudio.defs.erase(d.id); gAudio.defs[n] = copy; gAudio.order[sndSel] = n; sndDirty = true; return; } } }
    ui.text(fx + keyW + 210, cy + 2, "game code and items/spells/maps refer to this id", DIM); cy += 24;
    ui.label(fx + 10, cy + 2, "kind"); if (ui.choice(fx + keyW, cy, 100, d.kind)) { d.kind = d.kind == "sfx" ? "music" : d.kind == "music" ? "ambient" : "sfx"; gAudio.refresh(d); sndDirty = true; } cy += 24;
    ui.label(fx + 10, cy + 2, "description"); if (ui.textField(fx + keyW, cy, fw - keyW - 20, d.desc, 120)) sndDirty = true; cy += 24;
    ui.label(fx + 10, cy + 2, d.kind == "music" ? "file" : "files"); ui.text(fx + keyW, cy + 2, d.kind == "music" ? "(several = a random one each time it starts)" : "(several = a random one per play)", DIM); cy += 18;
    for (size_t i = 0; i < d.files.size(); i++) {
        bool exists = fileExists(gAudio.audioDir() + "/" + d.files[i]);
        ui.text(fx + keyW, cy + 2, d.files[i], exists ? TEXT : DANGER); if (!exists) ui.text(fx + keyW + font->width(d.files[i]) + 8, cy + 2, "missing!", DANGER);
        if (ui.button(fx + fw - 150, cy, 60, 16, "change")) { auto files = gAudio.availableFiles(""); int cur = 0; for (size_t k = 0; k < files.size(); k++) if (files[k] == d.files[i]) cur = (int)k; size_t ii = i; openListPicker("audio file", files, cur, [this, ii, files](int k) { SoundDef& dd = gAudio.defs[gAudio.order[sndSel]]; if (ii < dd.files.size()) { dd.files[ii] = files[k]; gAudio.refresh(dd); sndDirty = true; } }); }
        if (ui.button(fx + fw - 84, cy, 20, 16, "x", true, false, DANGER)) { d.files.erase(d.files.begin() + i); gAudio.refresh(d); sndDirty = true; break; }
        cy += 18;
    }
    if (ui.button(fx + keyW, cy, 110, 18, "+ add existing")) { auto files = gAudio.availableFiles(""); openListPicker("audio file", files, 0, [this, files](int k) { SoundDef& dd = gAudio.defs[gAudio.order[sndSel]]; dd.files.push_back(files[k]); gAudio.refresh(dd); sndDirty = true; }); }
    if (ui.button(fx + keyW + 116, cy, 130, 18, "+ import new file...")) { std::string sub = d.kind == "music" ? "music" : "sfx"; openTextPrompt("Full path of a .wav/.ogg/.mp3 to copy into assets/audio/" + sub, "", [this, sub](const std::string& p) { std::string rel; if (gAudio.importFile(trim(p), sub, rel)) { SoundDef& dd = gAudio.defs[gAudio.order[sndSel]]; dd.files.push_back(rel); gAudio.refresh(dd); sndDirty = true; say("Imported " + rel); } else say(gAudio.lastError); }); }
    ui.text(fx + keyW + 254, cy + 3, "(or drop files into assets/audio, then '+ add existing')", DIM);
    cy += 28;
    ui.label(fx + 10, cy + 2, "volume %"); sh("volume", cy); if (ui.intField(fx + keyW, cy, 70, d.volume, 0, 200)) sndDirty = true; cy += 22;
    if (d.kind == "music") { ui.label(fx + 10, cy + 2, "loop"); sh("loop", cy); if (ui.checkbox(fx + keyW, cy, "loop forever (off = play once, e.g. a jingle)", d.loop)) sndDirty = true; cy += 22; }
    else { ui.label(fx + 10, cy + 2, "pitch var %"); sh("pitchvar", cy); if (ui.intField(fx + keyW, cy, 70, d.pitchVar, 0, 50)) sndDirty = true; ui.text(fx + keyW + 80, cy + 2, "random +/- pitch so repeats don't sound identical", DIM); cy += 22; }
    cy += 6;
    if (ui.button(fx + keyW, cy, 90, 22, d.kind == "music" ? "Play track" : "Play", !d.files.empty(), false, OK)) { if (d.kind == "music") { previewingMusic = true; gAudio.music(d.id); } else gAudio.play(d.id); }
    if (d.kind == "music" && ui.button(fx + keyW + 96, cy, 60, 22, "Stop", !gAudio.currentMusic.empty())) { gAudio.music(""); previewingMusic = false; }
    ui.text(fx + keyW + 170, cy + 5, d.kind == "music" && gAudio.currentMusic == d.id ? "playing (volume follows the Settings sliders)" : "", DIM);
    cy += 36;
    // where it is used
    ui.header(fx, cy, fw, "Used by"); cy += 20;
    std::string users;
    for (auto& g : gens) for (auto& s : g.secs) { if (s.get("sound") == d.id || s.get("attacksound") == d.id) users += "[" + s.name + "] " + s.get("id") + "  "; }
    if (d.kind == "music") { for (auto& m : mapFiles) { std::string t; if (readFile(readPath("maps/" + m + ".txt"), t) && t.find("music=" + d.id) != std::string::npos) users += "map " + m + "  "; } for (auto& s : gens[7].secs) if (s.get("music") == d.id) users += "dungeon " + s.get("id") + "  "; }
    static const std::map<std::string, std::string> builtin = {
        {"ui_move", "menu cursor"}, {"ui_confirm", "menu confirm (keys / pad)"}, {"ui_click", "mouse clicks in game menus"}, {"ui_open", "any game screen opening"}, {"ui_close", "closing it"}, {"ui_error", "not enough gold, desktop error boxes"}, {"text_blip", "each dialogue node"},
        {"boot", "boot screen"}, {"shutdown", "Shut Down"}, {"desktop_click", "desktop icons / start button"}, {"desktop_open", "desktop windows"}, {"desktop_close", "closing desktop windows"},
        {"step", "every step"}, {"door_open", "doors"}, {"door_locked", "locked doors, chests and exits"}, {"chest_open", "chests"}, {"pickup", "items received"}, {"gold", "gold received"}, {"equip", "equip / unequip"}, {"potion", "consumables without their own sound="},
        {"save", "saving and loading"}, {"levelup", "level ups"}, {"quest", "quest= in dialogue"}, {"teleport", "map transitions, recall"}, {"rest", "heal= / rest= in dialogue"}, {"shop_buy", "buying"}, {"shop_sell", "selling"}, {"ambush", "random encounters"},
        {"combat_start", "combat begins"}, {"turn_player", "party turns"}, {"turn_enemy", "enemy turns"}, {"attack_melee", "melee weapons / monsters without attacksound="}, {"attack_bow", "bows and ranged monsters"}, {"attack_gun", "guns"}, {"attack_laser", "energy guns"},
        {"hit", "damage"}, {"crit", "critical hits"}, {"miss", "misses"}, {"block", "zero damage"}, {"spell_cast", "spells without a type"}, {"spell_fire", "fire spells"}, {"spell_ice", "cold spells"}, {"spell_lightning", "shock spells"}, {"spell_poison", "poison spells"}, {"spell_heal", "healing / revive / cure"}, {"spell_buff", "buffs and debuffs"},
        {"explosion", "grenades"}, {"death_enemy", "enemies without sound="}, {"death_party", "party knocked out"}, {"flee", "fleeing"}, {"victory", "combat won"}, {"defeat", "party wiped"}, {"unlock", "keys / unlock spell"}, {"light", "light spell"}, {"stealth", "stealth spell"}, {"detect", "detect spell"},
        {"title", "the desktop"}, {"overworld", "overworld"}, {"town", "kind=town / interior maps"}, {"forest", "other hand-made maps"}, {"dungeon", "dungeons (default)"}, {"tech", "tech / ship dungeons"}, {"combat", "fights"}, {"boss", "fights with a boss"}, {"gameover", "game over"}};
    auto bi = builtin.find(d.id);
    if (bi != builtin.end()) users = "built-in: " + bi->second + "   " + users;
    if (users.empty()) users = "nothing yet - set sound=" + d.id + " on an item, spell or creature (attacksound= for monsters), or music=" + d.id + " on a map / dungeon.";
    font->drawWrapped(*ui.cv, *ts, fx + 10, cy, fw - 20, users, DIM); cy += 40;
    if (!gAudio.missing.empty()) { std::string ms = "events requested by the game but not defined: "; for (auto& m : gAudio.missing) ms += m + " "; font->drawWrapped(*ui.cv, *ts, fx + 10, cy, fw - 20, ms, DANGER); }
}
