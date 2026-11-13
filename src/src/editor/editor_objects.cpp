// 1.1.0: Objects tab (object templates, objects.txt, plus every placed object on every map with bulk changes) and
// Resources tab (resource node types, resources.txt: what a gather node gives and how it looks).
// A placed object with template=<id> takes the template's type and every key it doesn't set itself; a [gather] node
// takes the keys it doesn't set from its resource type (node=, else the type giving its item). The game does the same
// in loadMapText (world.cpp), so what the Editor shows here is what the game builds.
#include "editor.h"
#include "../game/tiles.h"
#include <filesystem>
#include <algorithm>

namespace fs = std::filesystem;
using namespace ED;
extern int gEdTop;
static const int BOTTOM_BAR = 18;

static bool nodeKey(const std::string& k) { return k == "name" || k == "item" || k == "count" || k == "tool" || k == "respawn" || k == "sprite" || k == "color" || k == "spent" || k == "spentsprite"; }

// ---------------------------------------------------------------- resolving
const EdSection* Editor::templateSec(const std::string& id) const {
    const EdSection* out = nullptr; if (id.empty()) return out;
    for (auto& s : gens[15].secs) if (s.name == "template" && s.get("id") == id) out = &s;   // a later one wins, as in the game
    return out;
}
const EdSection* Editor::typeSec(const std::string& type) const {
    const EdSection* out = nullptr; if (type.empty()) return out;
    for (auto& s : gens[15].secs) if (s.name == "type" && s.get("id") == type) out = &s;
    return out;
}
// 1.1.1: the look / use keys each object type reads besides its own data (world.cpp loadMapText, Game::drawWorld)
std::vector<std::string> edObjLookKeys(const std::string& t) {
    if (t == "savepoint") return {"sprite", "color", "sparkle", "sparklecolor", "text", "sound", "onuse"};
    if (t == "chest") return {"sprite", "opensprite", "color", "sound", "onuse"};
    if (t == "sign") return {"sprite", "color", "title", "sound", "onuse"};
    if (t == "door") return {"sprite", "opensprite", "sound", "text", "onuse"};
    if (t == "lever") return {"sprite", "onsprite", "sound", "onuse"};
    return edObjKeys(t);   // the rest: any of the keys the type reads can get a default here
}
const EdSection* Editor::resourceSec(const std::string& node, const std::string& item) const {
    auto find = [&](const std::vector<EdSection>& secs) -> const EdSection* {
        if (!node.empty()) { const EdSection* out = nullptr; for (auto& s : secs) if (s.name == "resource" && s.get("id") == node) out = &s; return out; }
        if (!item.empty()) for (auto& s : secs) if (s.name == "resource" && s.get("item") == item) return &s;
        return nullptr;
    };
    if (const EdSection* s = find(gens[16].secs)) return s;
    // 1.1.7: a campaign with inherit=1 also gets the base game's resource types (the game merges both files)
    if (!campaignInherit || dataRoot == baseRoot) return nullptr;
    if (!baseResLoaded) { baseResLoaded = true; baseRes.clear(); std::string bt; if (readFile(baseRoot + "/resources.txt", bt)) for (auto& s : parseConfig(bt)) if (s.name == "resource") { EdSection e; e.name = s.name; e.kv = s.kv; baseRes.push_back(e); } }   // read again after a campaign switch
    return find(baseRes);
}
EdObj Editor::resolvedObj(const EdObj& o) const {
    EdObj r = o;
    if (const EdSection* t = templateSec(o.get("template"))) {
        std::string ty = t->get("type"); if (!ty.empty()) r.type = ty;
        for (auto& kv : t->kv) if (kv.first != "id" && kv.first != "type" && kv.first != "x" && kv.first != "y" && !r.has(kv.first)) r.kv.push_back(kv);
    }
    // 1.1.9 (#33): an NPC placement takes what it doesn't set (or leaves empty) from its NPC (npcs.txt), before its type
    if (r.type == "npc") if (const EdSection* n = npcSec(r.get("npc"))) {
        for (auto& kv : n->kv) { if (kv.first == "id") continue; if (!r.has(kv.first)) r.kv.push_back(kv); else if (trim(r.get(kv.first)).empty()) r.set(kv.first, kv.second); }
        if (r.get("id").empty()) r.set("id", trim(r.get("npc")));
    }
    // 1.1.1: then its type's entry; an empty value (in the type, or on the object) counts as not set, as in the game
    if (const EdSection* ty = typeSec(r.type)) for (auto& kv : ty->kv) { if (kv.first == "id" || kv.first == "x" || kv.first == "y" || kv.first == "template" || trim(kv.second).empty()) continue;
        if (!r.has(kv.first)) r.kv.push_back(kv); else if (trim(r.get(kv.first)).empty()) r.set(kv.first, kv.second); }
    if (r.type == "gather") if (const EdSection* rs = resourceSec(r.get("node"), r.get("item"))) {
        if (!r.has("node")) r.kv.push_back({"node", rs->get("id")});
        for (auto& kv : rs->kv) if (nodeKey(kv.first) && !r.has(kv.first)) r.kv.push_back(kv);
    }
    return r;
}
std::vector<std::pair<std::string, std::string>> Editor::inheritedKeys(const EdObj& o, std::string& from) const {
    std::vector<std::pair<std::string, std::string>> out;
    EdObj r = resolvedObj(o);
    for (auto& kv : r.kv) if (!o.has(kv.first) || (trim(o.get(kv.first)).empty() && !trim(kv.second).empty())) out.push_back(kv);
    from.clear();
    if (templateSec(o.get("template"))) from = "template " + o.get("template");
    if (r.type == "npc" && npcSec(r.get("npc"))) from += (from.empty() ? "" : " + ") + std::string("NPC ") + trim(r.get("npc"));
    if (const EdSection* ty = typeSec(r.type)) { bool any = false; for (auto& kv : ty->kv) if (kv.first != "id" && !trim(kv.second).empty()) any = true; if (any) from += (from.empty() ? "" : " + ") + std::string("type ") + r.type; }
    if (r.type == "gather") if (const EdSection* rs = resourceSec(r.get("node"), r.get("item"))) from += (from.empty() ? "" : " + ") + std::string("resource ") + rs->get("id");
    return out;
}

// draws a gather node the way the game does (Game::drawWorld): its own picture, else the map tile under it, recoloured
void Editor::drawGatherNode(const EdObj& o, int sx, int sy, int scale, const std::string& cellSpec, bool spent) {
    const TileKind* k = tileKind(trim(split(cellSpec, '|')[0]));
    Color8 col = !o.get("color").empty() ? parseColor(o.get("color")) : gatherGuessColor(o.get("item"));
    if (spent) col = !o.get("spent").empty() ? parseColor(o.get("spent")) : RGB(85, 85, 85);
    Tile pic = parseTile(o.get("sprite")); if (spent && parseTile(o.get("spentsprite")).valid()) pic = parseTile(o.get("spentsprite"));
    if (pic.valid()) { ui.cv->fillRect(sx, sy, 12 * scale, 12 * scale, k ? k->bg : C::Black); ts->drawBacking(*ui.cv, pic, sx, sy, scale); ts->drawRecolor(*ui.cv, pic, sx, sy, scale, col); return; }
    if (!k) return;
    Tile t = k->prop; if (!t.valid()) for (auto& g : k->ground) if (g.valid()) { t = g; break; }
    if (t.valid()) ts->drawRecolor(*ui.cv, t, sx, sy, scale, col);
}

// ---------------------------------------------------------------- placed objects (Objects tab, right side)
bool Editor::placedMatch(const std::string& mapId, const EdObj& raw) const {
    EdObj r = resolvedObj(raw);
    if (poType > 0 && poType <= (int)edObjTypes().size() && r.type != edObjTypes()[poType - 1]) return false;
    if (poThisTmpl) { auto& g = gens[15]; const EdSection* ss = g.sel >= 0 && g.sel < (int)g.secs.size() ? &g.secs[g.sel] : nullptr; std::string id = ss ? ss->get("id") : "";
        if (id.empty()) return false; if (ss->name == "type" ? r.type != id : raw.get("template") != id) return false; }   // 1.1.1: a [type] entry picks every object of its type
    if (!trim(poFilter).empty()) {
        std::string hay = toLower(mapId + " " + r.type); for (auto& kv : r.kv) hay += " " + toLower(kv.first + "=" + kv.second);
        for (auto& w : split(toLower(poFilter), ' ')) if (!trim(w).empty() && hay.find(trim(w)) == std::string::npos) return false;
    }
    return true;
}
std::vector<Editor::PlacedRef> Editor::placedMatches() {
    if (poNeedScan || mapScan.empty()) { scanMaps(); poNeedScan = false; }
    std::vector<PlacedRef> out;
    for (auto& ms : mapScan) for (size_t i = 0; i < ms.raw.size(); i++) if (placedMatch(ms.id, ms.raw[i])) out.push_back({ms.id, (int)i});
    return out;
}
// runs fn on every placed object the filter picks (on every map) and saves each map it changed. The open map is changed
// in place: saved too when it had no unsaved changes, else left for the Maps tab's Save.
int Editor::bulkObjects(const std::function<bool(EdObj&)>& fn, int& mapsChanged) {
    int n = 0; mapsChanged = 0; bool openLeftDirty = false;
    for (auto& f : mapFiles) {
        bool open = mapLoaded && em.id == f;
        EdMap m; EdMap* mp = &em;
        if (!open) { if (!m.load(readPath("maps/" + f + ".txt"))) continue; m.path = dataRoot + "/maps/" + f + ".txt"; mp = &m; }
        std::vector<int> hits; for (size_t i = 0; i < mp->objs.size(); i++) if (placedMatch(f, mp->objs[i])) hits.push_back((int)i);
        if (hits.empty()) continue;
        if (open) mapPushUndo();
        int here = 0; for (int i : hits) if (fn(mp->objs[i])) here++;
        if (!here) { if (open) mapUndo.pop_back(); continue; }
        n += here; mapsChanged++;
        if (open) { if (mapDirty) openLeftDirty = true; else if (em.save()) mapDirty = false; else mapDirty = true; }
        else m.save();
    }
    poNeedScan = true; flagIndexDirty = true; if (n) changed = true;
    if (openLeftDirty) say(fmt("Changed %d object(s) on %d map(s). The open map had unsaved changes: Save it in the Maps tab.", n, mapsChanged));
    return n;
}
void Editor::templateFromObject(const EdObj& o0, const std::string& mapId, int objIndex) {
    EdObj o = o0;
    std::string sugg = o.get("name").empty() ? o.type : o.get("name"); std::string id0; for (char ch : toLower(sugg)) { if (isalnum((unsigned char)ch)) id0 += ch; else if (!id0.empty() && id0.back() != '_') id0 += '_'; }
    openTextPrompt("New template id (Objects tab)", id0.empty() ? "template" : id0, [this, o, mapId, objIndex](const std::string& nm) {
        std::string id = trim(nm); if (id.empty()) return;
        if (templateSec(id)) { say("A template '" + id + "' already exists."); return; }
        EdSection s; s.name = "template"; s.kv = {{"id", id}, {"type", o.type}};
        for (auto& kv : o.kv) { const std::string& k = kv.first; if (k == "x" || k == "y" || k == "template" || k == "flag" || (o.type == "npc" && k == "id")) continue; s.kv.push_back(kv); }   // what belongs to the spot stays on the map
        gens[15].secs.push_back(s); gens[15].sel = (int)gens[15].secs.size() - 1; gens[15].filter.clear();
        if (!gens[15].save()) { say("Could not save objects.txt"); return; }
        changed = true; poNeedScan = true;
        auto link = [&](EdObj& t) { for (auto& kv : s.kv) if (kv.first != "id" && kv.first != "type") t.erase(kv.first); t.kv.insert(t.kv.begin(), {"template", id}); };
        if (mapLoaded && em.id == mapId) { if (objIndex >= 0 && objIndex < (int)em.objs.size()) { mapPushUndo(); link(em.objs[objIndex]); mapDirty = true; say("Template '" + id + "' made from this object and linked to it (Save the map). Edit it in the Objects tab."); } }
        else { EdMap m; if (m.load(readPath("maps/" + mapId + ".txt")) && objIndex >= 0 && objIndex < (int)m.objs.size()) { m.path = dataRoot + "/maps/" + mapId + ".txt"; link(m.objs[objIndex]); m.save(); } say("Template '" + id + "' made and linked to the object on " + mapId + "."); }
    });
}

// 1.1.1: a chest, sign, save point, lever or door drawn the way the game draws it (Game::drawWorld): its sprite= (else
// the built-in picture), color= recolours it; used = opened / pulled. Other types have no picture of their own.
void Editor::drawObjectLook(const EdObj& o, int sx, int sy, int scale, bool used) {
    auto t = [&](const char* k) { return parseTile(trim(o.get(k))); };
    std::string cs = trim(o.get("color")); bool rec = !cs.empty(); Color8 col = rec ? parseColor(cs) : C::White;
    Tile pic;
    if (o.type == "chest") pic = used ? (t("opensprite").valid() ? t("opensprite") : t("sprite").valid() ? t("sprite") : TL::CHEST_OPEN) : (t("sprite").valid() ? t("sprite") : TL::CHEST);
    else if (o.type == "sign") pic = t("sprite").valid() ? t("sprite") : TL::SIGN;
    else if (o.type == "savepoint") pic = t("sprite").valid() ? t("sprite") : TL::ALTAR;
    else if (o.type == "lever") { rec = false; Tile off = t("sprite").valid() ? t("sprite") : TL::LEVER; pic = used && t("onsprite").valid() ? t("onsprite") : off; }
    else if (o.type == "door") { rec = false; pic = used ? (t("opensprite").valid() ? t("opensprite") : o.get("metal") == "1" ? TL::DOOR_METAL : TL::DOOR_OPEN) : (t("sprite").valid() ? t("sprite") : TL::DOOR_WOOD); }
    if (!pic.valid()) return;
    ts->drawBacking(*ui.cv, pic, sx, sy, scale);
    if (rec) ts->drawRecolor(*ui.cv, pic, sx, sy, scale, col); else ts->draw(*ui.cv, pic, sx, sy, scale);
    if (o.type == "savepoint" && trim(o.get("sparkle")) != "0") { std::string sc = trim(o.get("sparklecolor")); ts->draw(*ui.cv, TL::SPARKLE, sx, sy - 4 * scale, scale, sc.empty() ? C::Cyan : parseColor(sc)); }
}

void Editor::drawObjectsTab(InputState& in) {
    int uw = 350, h = H - gEdTop - BOTTOM_BAR - 8, infoH = 170;
    drawGeneric(gens[15], 4, gEdTop + 4, W - 8 - uw - 6, h, in);
    drawObjectTypeInfo(W - 4 - uw, gEdTop + 4, uw, infoH, in);
    drawPlacedObjects(W - 4 - uw, gEdTop + 4 + infoH + 4, uw, h - infoH - 4, in);
}

// the selected [type] entry or template: how its objects look (as placed / used) and what using one does
void Editor::drawObjectTypeInfo(int x, int y, int w, int h, InputState& in) {
    (void)in;
    GenericEditor& g = gens[15];
    ui.panel(x, y, w, h); ui.header(x, y, w, "How it looks and what it does"); helpBtn(x + w - 16, y + 1, "objtype", "tab.objects");
    const EdSection* s = g.sel >= 0 && g.sel < (int)g.secs.size() ? &g.secs[g.sel] : nullptr;
    if (!s) { font->drawWrapped(*ui.cv, *ts, x + 8, y + 24, w - 16, "Pick an entry on the left. The \"every savepoint\", \"every chest\"... entries set what all objects of that type use (picture, colour, sound, what happens on use); templates are reusable objects you place yourself.", DIM); return; }
    EdObj probe;
    if (s->name == "type") { probe.type = s->get("id"); for (auto& kv : s->kv) if (kv.first != "id" && !trim(kv.second).empty()) probe.kv.push_back(kv); }
    else { probe.type = s->get("type"); probe.set("template", s->get("id")); probe = resolvedObj(probe); }
    std::string ty = probe.type;
    bool hasLook = ty == "chest" || ty == "sign" || ty == "savepoint" || ty == "lever" || ty == "door";
    int cy = y + 24;
    if (hasLook) {
        bool two = ty == "chest" || ty == "lever" || ty == "door";
        for (int u = 0; u < (two ? 2 : 1); u++) {
            int px = x + 12 + u * 150; ui.cv->fillRect(px - 2, cy - 2, 52, 52, BORDER);
            drawMapCell(px, cy, "floor_stone", 4);
            drawObjectLook(probe, px, cy, 4, u == 1);
            ui.text(px + 56, cy + 16, u == 0 ? (ty == "lever" ? "off" : ty == "door" ? "closed" : "as placed") : (ty == "lever" ? "pulled" : "opened"), u ? DIM : OK);
        }
        cy += 56;
    } else if (ty == "npc" || ty == "enemy" || ty == "gather") { ui.text(x + 8, cy, ty == "gather" ? "Looks: set by its resource type (Resources tab)." : ty == "npc" ? "Looks: its own sprite= and color= keys." : "Looks: its creatures' pictures.", DIM); cy += 16; }
    else { ui.text(x + 8, cy, "No picture: an invisible area on the map.", DIM); cy += 16; }
    auto val = [&](const char* k, const char* def) { std::string v = trim(probe.get(k)); return v.empty() ? std::string(def) : v; };
    std::string onuse = trim(probe.get("onuse")), use;
    if (ty == "savepoint") use = "Step on it: \"" + val("text", "A save shrine.") + " Press E or F5 to save.\"  E: " + (trim(probe.get("sound")).empty() ? "" : "sound " + trim(probe.get("sound")) + ", ") + (onuse.empty() ? "" : "runs " + onuse + ", ") + "opens the save window.";
    else if (ty == "chest") use = "Opened: sound " + val("sound", "chest_open") + ", gives its items and gold" + (onuse.empty() ? "" : ", then runs " + onuse) + ".";
    else if (ty == "sign") use = "Read: a \"" + val("title", "Sign") + "\" window with its text" + (onuse.empty() ? "" : ", then runs " + onuse) + ".";
    else if (ty == "door") use = "Opened: sound " + val("sound", "door_open (unlock with a key)") + ", log \"" + val("text", "The door opens.") + "\"" + (onuse.empty() ? "" : ", then runs " + onuse) + ". No sprite= = the map tile.";
    else if (ty == "lever") use = "Pulled: sound " + val("sound", "unlock") + ", toggles its flag" + (onuse.empty() ? "" : ", then runs " + onuse) + ".";
    else use = "Every key set here is the default for every " + ty + " that doesn't set it itself.";
    for (auto& wl : font->wrap(use, w - 16)) { if (cy > y + h - 30) break; font->draw(*ui.cv, *ts, x + 8, cy, wl, TEXT); cy += 13; }
    // how many placed objects follow it
    if (poNeedScan || mapScan.empty()) { scanMaps(); poNeedScan = false; }
    int n = 0; std::set<std::string> maps;
    for (auto& ms : mapScan) for (auto& raw : ms.raw) { bool hit = s->name == "type" ? resolvedObj(raw).type == ty : raw.get("template") == s->get("id"); if (hit) { n++; maps.insert(ms.id); } }
    ui.text(x + 8, y + h - 16, fmt("%d placed on %d map(s)", n, (int)maps.size()) + (s->name == "type" && (ty == "chest" || ty == "savepoint") ? " (+ generated ones)" : "") + (g.dirty ? " - Save (Ctrl+S) to use it" : ""), g.dirty ? ACCENT2 : DIM);
}

void Editor::drawPlacedObjects(int x, int y, int w, int h, InputState& in) {
    GenericEditor& g = gens[15];
    const EdSection* tsel = g.sel >= 0 && g.sel < (int)g.secs.size() ? &g.secs[g.sel] : nullptr;
    std::string tid = tsel ? tsel->get("id") : "";
    ui.panel(x, y, w, h); ui.header(x, y, w, "Placed objects on every map"); helpBtn(x + w - 16, y + 1, "objects.placed", "tab.objects");
    int cy = y + 22;
    // filter
    ui.label(x + 6, cy + 2, "type");
    std::vector<std::string> types{"any"}; for (auto& t : edObjTypes()) types.push_back(t);
    poType = clampi(poType, 0, (int)types.size() - 1);
    if (ui.choice(x + 40, cy, 90, types[poType])) openListPicker("Object type", types, poType, [this](int i) { poType = i; poSel = -1; });
    if (!tsel) poThisTmpl = false;
    { bool v = poThisTmpl; std::string lbl = tid.empty() ? "copies of the selected template" : tsel->name == "type" ? "only every " + tid : "only copies of '" + tid + "'"; while (font->width(lbl) > w - 160 && lbl.size() > 4) lbl.pop_back();
      if (ui.checkbox(x + 140, cy + 2, lbl, v) && tsel) { poThisTmpl = v; poSel = -1; } }
    cy += 20;
    ui.label(x + 6, cy + 2, "find"); if (ui.searchField(x + 40, cy, w - 112, poFilter, "id, name, map, any value...")) poSel = -1;
    if (ui.hover(x + 40, cy, w - 112, 16)) ui.tip("Words that must all appear: map id, type, key=value (e.g. \"iron_ore\" or \"goblin_cave chest\")");
    if (ui.button(x + w - 66, cy, 60, 16, "Rescan")) { poNeedScan = true; poSel = -1; }
    if (ui.hover(x + w - 66, cy, 60, 16)) ui.tip("Read every map again (after saving maps elsewhere)");
    cy += 20;
    auto matches = placedMatches();
    std::set<std::string> mapsHit; for (auto& m : matches) mapsHit.insert(m.map);
    auto rawOf = [&](const PlacedRef& p) -> const EdObj* { for (auto& ms : mapScan) if (ms.id == p.map && p.obj < (int)ms.raw.size()) return &ms.raw[p.obj]; return nullptr; };
    std::vector<std::string> labels;
    for (auto& m : matches) { const EdObj* o = rawOf(m); if (!o) { labels.push_back("?"); continue; } EdObj r = resolvedObj(*o);
        std::string what; for (const char* k : {"id", "name", "node", "item", "group", "to", "items", "text", "script"}) if (!r.get(k).empty()) { what = r.get(k); break; }
        labels.push_back(fmt("%s %s,%s  %s", m.map.c_str(), o->get("x").c_str(), o->get("y").c_str(), r.type.c_str()) + (o->get("template").empty() ? "" : " [" + o->get("template") + "]") + "  " + what); }
    int listH = h - (cy - y) - 250;
    if (poSel >= (int)matches.size()) poSel = -1;
    int hit = ui.listBox(x + 4, cy, w - 8, listH, labels, poSel, poScroll, 14);
    static float lastT = -10; static int lastHit = -1;
    if (hit >= 0) { if (hit == lastHit && ui.time - lastT < 0.35f) { FlagUse u; u.tab = 0; u.file = matches[hit].map; u.obj = matches[hit].obj; jumpToUse(u); } lastT = ui.time; lastHit = hit; }
    cy += listH + 4;
    ui.text(x + 6, cy + 2, fmt("%d object(s) on %d map(s)", (int)matches.size(), (int)mapsHit.size()), ACCENT2);
    if (ui.button(x + w - 96, cy, 90, 16, "Open on map", poSel >= 0)) { FlagUse u; u.tab = 0; u.file = matches[poSel].map; u.obj = matches[poSel].obj; jumpToUse(u); return; }
    if (ui.hover(x + w - 96, cy, 90, 16)) ui.tip("Show the selected object in the Maps tab (or double-click it in the list)");
    cy += 20;
    // the selected object: its own keys bright, the ones it takes from its template / resource type dim
    int boxH = 96; ui.cv->fillRect(x + 4, cy, w - 8, boxH, FIELD); ui.cv->rect(x + 4, cy, w - 8, boxH, BORDER);
    const EdObj* selRaw = poSel >= 0 ? rawOf(matches[poSel]) : nullptr;
    if (selRaw) {
        EdObj r = resolvedObj(*selRaw); ui.cv->setClip({x + 5, cy + 1, w - 10, boxH - 2});
        int ly = cy + 3, col = 0; for (auto& kv : r.kv) { bool own = selRaw->has(kv.first); std::string t = kv.first + "=" + kv.second; while (font->width(t) > (w - 20) / 2 && t.size() > 3) t.pop_back();
            font->draw(*ui.cv, *ts, x + 9 + col * ((w - 16) / 2), ly, t, own ? TEXT : DIM); if (++col == 2) { col = 0; ly += 13; } }
        ui.cv->resetClip();
    } else font->drawWrapped(*ui.cv, *ts, x + 10, cy + 6, w - 20, "Pick an object above to see its keys (dim = taken from its template or resource type). Double-click opens it on its map.", DIM);
    cy += boxH + 6;
    // bulk changes to everything listed
    ui.header(x, cy, w, fmt("Change all %d listed", (int)matches.size())); cy += 20;
    ui.label(x + 6, cy + 2, "key"); ui.textField(x + 34, cy, 90, poKey, 30);
    ui.label(x + 132, cy + 2, "value"); ui.textField(x + 170, cy, w - 176, poVal, 200);
    cy += 20;
    std::string k = trim(poKey); bool keyOk = !k.empty() && k != "x" && k != "y" && k != "template"; int n = (int)matches.size();
    auto run = [this](const std::string& what, std::function<bool(EdObj&)> fn) {
        int nm = (int)placedMatches().size(); std::set<std::string> ms; for (auto& m : placedMatches()) ms.insert(m.map);
        openConfirm(what + fmt(" - %d object(s) on %d map(s). Every changed map is saved.", nm, (int)ms.size()), [this, fn]() { int maps = 0; int done = bulkObjects(fn, maps); say(fmt("Changed %d object(s) on %d map(s).", done, maps)); });
    };
    if (ui.button(x + 6, cy, 110, 18, "Set key", keyOk && n > 0)) { std::string kk = k, vv = poVal; run("Set " + kk + "=" + vv, [kk, vv](EdObj& o) { if (o.has(kk) && o.get(kk) == vv) return false; o.set(kk, vv); return true; }); }
    if (ui.hover(x + 6, cy, 110, 18)) ui.tip("Give every listed object this key and value (on a template copy it becomes that copy's own value)");
    if (ui.button(x + 122, cy, 110, 18, "Remove key", keyOk && n > 0)) { std::string kk = k; run("Remove " + kk, [kk](EdObj& o) { if (!o.has(kk)) return false; o.erase(kk); return true; }); }
    if (ui.hover(x + 122, cy, 110, 18)) ui.tip("Take the key off every listed object (template copies then use the template's value again)");
    cy += 22;
    bool canLink = tsel && tsel->name == "template" && !tid.empty() && n > 0;
    if (ui.button(x + 6, cy, 170, 18, tid.empty() || tsel->name == "type" ? "Link to template" : "Link to '" + tid.substr(0, 14) + "'", canLink)) {
        EdSection t = *tsel; std::string tt = t.get("type");
        run("Make every listed " + (tt.empty() ? std::string("object") : tt) + " a copy of template '" + tid + "' (keys with the template's value are dropped, others stay as each copy's own)",
            [this, t, tt, tid](EdObj& o) { if (resolvedObj(o).type != tt || o.get("template") == tid) return false;
                for (auto& kv : t.kv) if (kv.first != "id" && kv.first != "type" && o.has(kv.first) && o.get(kv.first) == kv.second) o.erase(kv.first);
                o.erase("template"); o.kv.insert(o.kv.begin(), {"template", tid}); o.type = tt; return true; });
    }
    if (ui.hover(x + 6, cy, 170, 18)) ui.tip("Turn the listed objects of the template's type into copies of the template selected on the left");
    if (ui.button(x + 182, cy, w - 188, 18, "Unlink (keep values)", n > 0)) {
        run("Unlink every listed copy from its template (the template's values are copied into each one)", [this](EdObj& o) { const EdSection* t = templateSec(o.get("template")); if (!t) return false;
            o.type = t->get("type", o.type); for (auto& kv : t->kv) if (kv.first != "id" && kv.first != "type" && !o.has(kv.first)) o.kv.push_back(kv); o.erase("template"); return true; });
    }
    cy += 22;
    if (ui.button(x + 6, cy, 170, 18, "Template from selected", selRaw && selRaw->get("template").empty())) { PlacedRef p = matches[poSel]; EdObj raw = *selRaw; templateFromObject(raw, p.map, p.obj); }
    if (ui.hover(x + 6, cy, 170, 18)) ui.tip("Make a new template from the selected object's values and link that object to it");
    if (dataRoot != baseRoot) ui.text(x + 184, cy + 3, "(base maps are copied into the campaign)", DIM);
}

// ---------------------------------------------------------------- Resources tab
void Editor::drawResourcesTab(InputState& in) {
    int uw = 310, h = H - gEdTop - BOTTOM_BAR - 8;
    drawGeneric(gens[16], 4, gEdTop + 4, W - 8 - uw - 6, h, in);
    drawResourceInfo(W - 4 - uw, gEdTop + 4, uw, h, in);
}

void Editor::drawResourceInfo(int x, int y, int w, int h, InputState& in) {
    GenericEditor& g = gens[16];
    ui.panel(x, y, w, h); ui.header(x, y, w, "How it looks and where it is"); helpBtn(x + w - 16, y + 1, "tab.resources", "obj.gather");
    if (g.sel < 0 || g.sel >= (int)g.secs.size()) { font->drawWrapped(*ui.cv, *ts, x + 8, y + 24, w - 16, "Pick a resource node type on the left. Gather nodes on the maps name their type with node=; everything they don't set themselves (item, amount, tool, days to grow back, picture, colours) comes from here.", DIM); return; }
    const EdSection& s = g.secs[g.sel]; std::string id = s.get("id");
    // preview: ready and picked clean, standing on its tile kind (or plain floor), at 4x
    EdObj probe; probe.type = "gather"; probe.set("node", id); EdObj r = resolvedObj(probe);
    std::string under = s.get("kind").empty() ? "floor_cave" : s.get("kind"); if (!tileKind(under)) under = "floor_cave";
    int cy = y + 24;
    for (int sp = 0; sp < 2; sp++) {
        int px = x + 16 + sp * 150; ui.cv->fillRect(px - 2, cy - 2, 52, 52, BORDER);
        drawMapCell(px, cy, under, 4);   // the tile, then the node over it, as the game draws them
        drawGatherNode(r, px, cy, 4, under, sp == 1);
        ui.text(px + 56, cy + 10, sp ? "picked clean" : "ready", sp ? DIM : OK);
        if (!sp && parseTile(r.get("sprite")).valid() == false) ui.text(px + 56, cy + 24, "(map tile)", DIM);
    }
    cy += 58;
    ui.text(x + 8, cy, "on " + under + (s.get("kind").empty() ? " (no kind= set)" : ""), DIM); cy += 16;
    const ItemDef* it = gd->item(s.get("item")); const ItemDef* tool = s.get("tool").empty() ? nullptr : gd->item(s.get("tool"));
    std::vector<std::pair<std::string, Color8>> lines;
    lines.push_back({"Gives " + s.get("count", "1") + " x " + (it ? it->name : (s.get("item").empty() ? "(no item!)" : s.get("item") + " (unknown item!)")) + " per harvest", it ? TEXT : DANGER});
    lines.push_back({s.get("tool").empty() ? "Bare hands - no tool needed" : "Needs a " + (tool ? tool->name : s.get("tool") + " (unknown item!)"), tool || s.get("tool").empty() ? TEXT : DANGER});
    lines.push_back({"Grows back after " + s.get("respawn", "3") + " day(s)", TEXT});
    lines.push_back({parseTile(s.get("sprite")).valid() ? "Own picture (sprite=); it replaces the map tile under it" : "No sprite=: the map tile under the node is drawn in its colour", DIM});
    if (s.get("styles").empty()) lines.push_back({"Not scattered in generated dungeons (styles= empty)", DIM});
    else lines.push_back({"Generated dungeons: " + s.get("styles"), TEXT});
    for (auto& l : lines) for (auto& wl : font->wrap(l.first, w - 16)) { font->draw(*ui.cv, *ts, x + 8, cy, wl, l.second); cy += 13; }
    cy += 6;
    // where it is placed
    if (poNeedScan || mapScan.empty()) { scanMaps(); poNeedScan = false; }
    struct Use { std::string map; int obj; bool byItem; std::string text; };
    std::vector<Use> uses; int byItem = 0, own = 0;
    for (auto& ms : mapScan) for (size_t i = 0; i < ms.raw.size(); i++) { EdObj ro = resolvedObj(ms.raw[i]); if (ro.type != "gather" || ro.get("node") != id) continue;
        bool bi = ms.raw[i].get("node").empty(); if (bi) byItem++; int ok = 0; for (auto& kv : ms.raw[i].kv) if (nodeKey(kv.first) && kv.first != "item") ok++; if (ok) own++;
        std::string t = ms.id + " " + ms.raw[i].get("x") + "," + ms.raw[i].get("y"); if (bi) t += "  (by item)"; for (auto& kv : ms.raw[i].kv) if (nodeKey(kv.first) && kv.first != "item") t += "  " + kv.first + "=" + kv.second;
        uses.push_back({ms.id, (int)i, bi, t}); }
    ui.header(x, cy, w, fmt("Placed on maps (%d)", (int)uses.size())); cy += 18;
    int listH = h - (cy - y) - 76; std::vector<std::string> labels; for (auto& u : uses) labels.push_back(u.text);
    static int sel = -1, scroll = 0; if (sel >= (int)uses.size()) sel = -1;
    int hit = ui.listBox(x + 4, cy, w - 8, listH, labels, sel, scroll, 14);
    if (hit >= 0) { FlagUse u; u.tab = 0; u.file = uses[hit].map; u.obj = uses[hit].obj; jumpToUse(u); return; }
    cy += listH + 4;
    font->drawWrapped(*ui.cv, *ts, x + 8, cy, w - 16, own ? fmt("%d node(s) set some keys themselves (listed) - theirs win. Click one to open it.", own) : "Click a node to open it on its map.", DIM);
    cy += 46;
    if (ui.button(x + 6, cy, w - 12, 18, byItem ? fmt("Link the %d node(s) found by item (node=%s)", byItem, id.c_str()) : "Link nodes found by item", byItem > 0)) {
        int pt = poType; bool pth = poThisTmpl; std::string pf = poFilter; poType = 0; poThisTmpl = false; poFilter.clear();   // bulkObjects works on the Objects tab filter: match all, test inside
        std::string item = s.get("item"); EdSection rs = s; int maps = 0;
        int done = bulkObjects([this, id, item, rs](EdObj& o) { EdObj ro = resolvedObj(o); if (ro.type != "gather" || !o.get("node").empty() || ro.get("node") != id) return false;
            o.set("node", id); if (o.get("item") == item) o.erase("item"); for (auto& kv : rs.kv) if (nodeKey(kv.first) && o.has(kv.first) && o.get(kv.first) == kv.second) o.erase(kv.first); return true; }, maps);
        poType = pt; poThisTmpl = pth; poFilter = pf;
        say(fmt("Linked %d node(s) on %d map(s) to %s (keys equal to the type's were dropped).", done, maps, id.c_str()));
    }
    if (ui.hover(x + 6, cy, w - 12, 18)) ui.tip("Nodes without node= find their type by item; linking writes node= and drops keys that match this type, so edits here reach them");
}
