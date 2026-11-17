#include "editor.h"
#include "../game/tiles.h"
#include <filesystem>
#include <algorithm>
#include <set>
#include <cstring>

namespace fs = std::filesystem;
using namespace ED;

// Tab numbers are fixed (tests, the headless "tab N" command and FlagUse::tab use them): new tabs only go at the end.
static const char* TABS[] = {"Maps", "Tiles", "World", "Items", "Creatures", "Spells", "Classes", "Shops", "Encounters", "Quests", "Dungeons", "Dialogue", "Sounds", "Tileset", "Recipes", "Affixes", "Banter", "Achiev.", "Campaign", "Flags", "Bounties", "Factions", "Objects", "Resources", "NPCs", "Scripts",
                             "Lore", "Epilogue", "Credits", "Hints", "Contradict.", "Dun.styles", "Healers", "Combat", "Formulas"};   // 26-34: 1.6.0
// 1.6.0 (#108): the names the workspace sub-tab row shows (room for whole words there)
static const char* TAB_LONG[] = {"Maps", "Tile kinds", "World map", "Items", "Creatures", "Spells", "Classes", "Shops", "Encounters", "Quests", "Dungeons", "Dialogue", "Sounds", "Tileset", "Recipes", "Affixes", "Banter", "Achievements", "Campaign", "Flags", "Bounties", "Factions", "Objects", "Resources", "NPCs", "Scripts",
                                 "Lore files", "Epilogue", "Credits", "Hints", "Contradictions", "Dungeon styles", "Healers", "Combat rules", "Formulas"};
static const int NTABS = 35;
// 1.6.1: the 1.6.0 tabs (26-34) are still stubs, so the bars leave them out until they're built: raise NSHOWN as they land
static const int NSHOWN = 26;
// 1.6.0 (#108): the tabs grouped into workspaces (first row of the header); each lists its tabs in sub-tab order
struct EdWorkspace { const char* name; std::vector<int> tabs; const char* tip; };
static const std::vector<EdWorkspace>& workspaces() {
    static const std::vector<EdWorkspace> ws = {
        {"Campaign", {18, 28, 29}, "The campaign itself: its name, where a new game starts, how it ends, the end credits and the Training hints."},
        {"World", {0, 2, 10, 31, 1, 13}, "Places: maps, the world map, generated dungeons and their styles, tile kinds and the sprite sheets."},
        {"People", {24, 11, 7, 32, 16}, "Characters: NPCs, what they say, their shops, healers, and the party's banter."},
        {"Story", {9, 19, 20, 21, 17, 27, 30}, "Quests, story flags, notice boards, towns' reputation, achievements, the epilogue and story contradictions."},   // 1.6.2 (#137): no Lore tab (26) - the desktop lore files are gone
        {"Fights", {4, 8, 5, 6, 33, 34}, "Monsters, random encounters, spells, classes, combos and statuses, and the core formulas."},
        {"Things", {3, 14, 15, 23, 22}, "Items, crafting recipes, item affixes, resource nodes and object types."},
        {"Sound & Scripts", {12, 25}, "Music and sound effects, and scripts."},
    };
    static std::vector<EdWorkspace> shown;
    if (shown.empty()) for (auto w : ws) { std::vector<int> t; for (int x : w.tabs) if (x < NSHOWN) t.push_back(x); w.tabs = t; shown.push_back(w); }
    return shown;
}
static int workspaceOf(int t) { auto& ws = workspaces(); for (size_t i = 0; i < ws.size(); i++) for (int x : ws[i].tabs) if (x == t) return (int)i; return 0; }
int gEdTop = 28;                      // tab bar height: 28 for one row of tabs, 50 when they need two rows
#define TOP gEdTop
static const int BOTTOM = 18;

// ============================================================ GenericEditor
bool GenericEditor::load() {
    undo.clear(); redo.clear(); shadowInit = false;
    secs.clear(); std::string text; dirty = false; status.clear();
    bool own = readFile(path, text);
    tail.clear();
    if (own) {
        for (auto& s : parseConfig(text)) { if (s.name.empty()) continue; EdSection e; e.name = s.name; e.kv = s.kv; secs.push_back(e); }
        // 1.1.3: keep the file's comments. Comment lines go with the [section] below them (the file's header with the
        // first one, "# ==== RANGED ====" headings with the entry they introduce); comments after the last entry are
        // the tail. (A comment between two keys of one entry ends up above the next entry.)
        std::vector<std::string> pend; size_t si = 0;
        auto flush = [&](std::string& into, bool isTail) {
            if (isTail) while (!pend.empty() && trim(pend.back()).empty()) pend.pop_back();   // (a lead keeps a blank line before its [section])
            size_t b = 0; while (b < pend.size() && trim(pend[b]).empty()) b++;
            into.clear(); for (size_t i = b; i < pend.size(); i++) into += pend[i] + "\n";
            pend.clear();
        };
        for (auto& raw : split(text, '\n')) {
            std::string line = raw; while (!line.empty() && line.back() == '\r') line.pop_back();
            std::string t = trim(line);
            if (t.empty()) { if (!pend.empty()) pend.push_back(""); continue; }
            if (t[0] == '#') { pend.push_back(line); continue; }
            if (t[0] == '[') {
                std::string nm = trim(t.substr(1, t.find(']') - 1));
                if (nm.empty()) { pend.clear(); continue; }
                if (si < secs.size()) flush(secs[si].lead, false); else pend.clear();
                si++; continue;
            }
            while (!pend.empty() && trim(pend.back()).empty()) pend.pop_back();   // a key line: blank lines before it don't matter
        }
        flush(tail, true);
    }
    // a campaign that builds on the base game: show the base entries too (dimmed, "(base)"); editing one makes a campaign copy
    std::string btext;
    if (!basePath.empty() && basePath != path && (!replaceWhole || !own) && readFile(basePath, btext)) {
        std::set<std::string> have; for (auto& e : secs) have.insert(e.get(listKey));
        for (auto& s : parseConfig(btext)) { if (s.name.empty()) continue; EdSection e; e.name = s.name; e.kv = s.kv; if (have.count(e.get(listKey))) continue; e.inherited = true; e.origKv = e.kv; secs.push_back(e); }
        status = own ? "" : "base only";
    } else if (!own) status = "new file";
    if (sel >= (int)secs.size()) sel = (int)secs.size() - 1;
    return own;
}
std::string GenericEditor::toText() const {
    bool headed = false; for (auto& s : secs) if (!(s.inherited && !replaceWhole)) { headed = !s.lead.empty(); break; }
    std::string o = headed ? "" : "# " + title + " - edited with the Editor\n\n";   // a file's own header comment is kept instead (1.1.3)
    for (auto& s : secs) { if (s.inherited && !replaceWhole) continue; o += s.lead + "[" + s.name + "]\n"; for (auto& kv : s.kv) o += kv.first + "=" + kv.second + "\n"; o += "\n"; }
    o += tail;
    return o;
}
bool GenericEditor::save() {
    bool anyInherited = false; for (auto& s : secs) if (s.inherited) anyInherited = true;
    std::string o = toText();
    { std::error_code ec; fs::create_directories(fs::path(path).parent_path(), ec); }
    if (!writeFile(path, o)) { status = "write failed!"; return false; }
    if (replaceWhole && anyInherited) for (auto& s : secs) { s.inherited = false; s.origKv.clear(); }   // the whole file now lives in the campaign
    dirty = false; status = "saved"; return true;
}
// the values a filter can pick for one key=value: the whole value, plus each comma-separated part without its ":n" tail
std::set<std::string> edFilterTokens(const std::string& v) {
    std::set<std::string> out; std::string t = trim(v); if (t.empty()) return out; out.insert(t);
    bool letters = false; for (char c : t) if (isalpha((unsigned char)c)) letters = true;
    if (letters && (t.find(',') != std::string::npos || t.find(':') != std::string::npos)) for (auto& part : split(t, ',')) { std::string id = trim(split(part, ':')[0]); if (!id.empty()) out.insert(id); }
    return out;
}
std::vector<int> GenericEditor::filtered() const {
    // 1.1.6 (#30): the search box looks at every value of an entry, every word must match; the field filter keeps
    // entries whose key has the value (or one item of a comma list, ids before any ":count")
    std::vector<int> out;
    for (size_t i = 0; i < secs.size(); i++) {
        const EdSection& s = secs[i];
        if (!filter.empty()) { std::string all = s.get(listKey) + " " + s.get("name") + " " + s.name; for (auto& kv : s.kv) all += " " + kv.second; if (!edMatch(all, filter)) continue; }
        if (!filterKey.empty()) {
            bool has = false, hit = false;
            for (auto& kv : s.kv) if (kv.first == filterKey) { if (!trim(kv.second).empty()) has = true; if (edFilterTokens(kv.second).count(filterVal)) hit = true; }
            if (filterVal == "\x01has" ? !has : filterVal == "\x01none" ? has : !hit) continue;
        }
        out.push_back((int)i);
    }
    return out;
}

// ============================================================ EdMap
static const char* LEGEND_POOL = ".,#=:;+-*~^%&$@!?/|<>()TtrfFhmDWowbBACSKnMusipxGagcHk%&QLZRejyvlgzPpVEevIYJUNOSuC";
void EdMap::init(int W, int H, const std::string& spec) { w = W; h = H; cells.assign((size_t)W * H, spec); }
void EdMap::resize(int W, int H, const std::string& fillSpec) {
    std::vector<std::string> nc((size_t)W * H, fillSpec);
    for (int y = 0; y < std::min(h, H); y++) for (int x = 0; x < std::min(w, W); x++) nc[(size_t)y * W + x] = cells[(size_t)y * w + x];
    cells = nc; w = W; h = H;
}
bool EdMap::load(const std::string& p) {
    path = p; header.clear(); objs.clear(); cells.clear(); w = h = 0;
    std::string text; if (!readFile(p, text)) return false;
    std::map<char, std::string> legend; std::vector<std::string> rows; std::string section;
    for (auto& raw : split(text, '\n')) {
        std::string line = raw; while (!line.empty() && line.back() == '\r') line.pop_back();
        std::string t = trim(line);
        bool legendEntry = (section == "legend" && t.size() >= 3 && (t[0] == '#' || t[0] == '=') && t[1] == ' ' && t[2] == '=');
        if (section != "rows" && !legendEntry && (t.empty() || t[0] == '#')) continue;
        if (!t.empty() && t[0] == '[' && t.back() == ']') { section = t.substr(1, t.size() - 2); if (section != "map" && section != "legend" && section != "rows") objs.push_back(EdObj{section, {}}); continue; }
        if (section == "rows") { if (t.empty() && rows.empty()) continue; rows.push_back(line); continue; }
        size_t eq = legendEntry ? 2 : t.find('='); if (eq == std::string::npos) continue;
        std::string k = legendEntry ? std::string(1, t[0]) : trim(t.substr(0, eq)), v = trim(t.substr(eq + 1));
        if (section == "map") { header.push_back({k, v}); if (k == "id") id = v; }
        else if (section == "legend") { if (!k.empty()) legend[k[0]] = v; }
        else if (!objs.empty()) objs.back().kv.push_back({k, v});
    }
    while (!rows.empty() && trim(rows.back()).empty()) rows.pop_back();
    int W = 0; for (auto& r : rows) W = std::max(W, (int)r.size()); int H = (int)rows.size();
    if (W == 0 || H == 0) { init(20, 15, "floor_grass"); return true; }
    init(W, H, "void");
    for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) { char ch = x < (int)rows[y].size() ? rows[y][x] : ' '; auto it = legend.find(ch); at(x, y) = it == legend.end() ? "void" : it->second; }
    if (id.empty()) id = fs::path(p).stem().string();
    return true;
}
bool EdMap::save() { return writeFile(path, toText()); }
std::string EdMap::toText() const {
    // assign legend chars: '.' = most common walkable spec, '#' = most common wall, then a readable pool
    std::map<std::string, int> freq; for (auto& c : cells) freq[c]++;
    std::vector<std::pair<int, std::string>> order; for (auto& f : freq) order.push_back({-f.second, f.first});
    std::sort(order.begin(), order.end());
    std::map<std::string, char> chars; std::string used = "[ ";
    auto take = [&](const std::string& spec, char c) { if (used.find(c) != std::string::npos) return false; used += c; chars[spec] = c; return true; };
    for (auto& o : order) { const TileKind* k = tileKind(trim(split(o.second, '|')[0])); if (k && k->walkable && chars.find(o.second) == chars.end() && used.find('.') == std::string::npos) take(o.second, '.'); }
    for (auto& o : order) { if (startsWith(o.second, "wall_") && chars.find(o.second) == chars.end() && used.find('#') == std::string::npos) take(o.second, '#'); }
    if (freq.count("void") && !chars.count("void")) take("void", 'x');
    // readable letters for common kinds, so the text file can be edited by hand too
    static const std::map<std::string, char> pref = {
        {"grass", '.'}, {"meadow", ','}, {"tallgrass", '"'}, {"bush", 'b'}, {"forest", 'f'}, {"forest2", 'F'}, {"pine", 'p'}, {"deepforest", 'P'},
        {"water", '~'}, {"deepwater", 'W'}, {"shallows", 'w'}, {"river", 'r'}, {"bridge", '='}, {"road", '+'}, {"dirt", '_'}, {"sand", 's'}, {"dune", 'd'},
        {"cactus", 'c'}, {"hill", 'h'}, {"hill2", 'H'}, {"mountain", '^'}, {"snowpeak", 'A'}, {"volcano", 'V'}, {"swamp", '%'}, {"deadtree", 't'}, {"deadtree2", 'T'},
        {"wasteland", 'x'}, {"rubble", 'X'}, {"snow", 'o'}, {"snowpine", 'O'}, {"riftscar", 'z'}, {"riftcrystal", 'Z'},
        {"loc_village", '1'}, {"loc_town", '2'}, {"loc_city", '3'}, {"loc_cave", '4'}, {"loc_tent", '5'}, {"loc_crypt", '6'}, {"loc_temple", '7'}, {"loc_citymod", '8'},
        {"loc_lab", '9'}, {"loc_ship", '0'}, {"loc_ruin", '!'}, {"loc_house", '$'}, {"loc_farm", '&'}, {"loc_mine", '?'}, {"loc_tower", '|'}, {"loc_castle", '@'}, {"loc_shrine", '}'},
        {"rocks", 'r'}, {"bones", 'b'}, {"mushroom", 'm'}, {"mushroom2", 'M'}, {"crystal", 'c'}, {"web", 'w'}, {"stairs_up", '<'}, {"stairs_down", '>'}, {"skull", 's'},
        {"torch_wall", 'T'}, {"pillar", 'I'}, {"coffin", 'C'}, {"door_wood", 'D'}, {"door_metal", 'D'}, {"door_arch", 'D'}, {"rubble_pile", 'R'}, {"vines", 'v'}, {"mire", '~'},
        {"barrel", 'o'}, {"crate", 'k'}, {"table", 'A'}, {"bookshelf", 'K'}, {"brazier", 'O'}, {"terminal", 't'}, {"terminal2", 't'}, {"server", 'S'}, {"box", 'B'}, {"cryopod", 'c'}};
    for (auto& o : order) { if (chars.count(o.second)) continue; auto it = pref.find(trim(split(o.second, '|')[0])); if (it != pref.end()) take(o.second, it->second); }
    for (auto& o : order) {
        if (chars.count(o.second)) continue;
        bool ok = false;
        for (const char* p = LEGEND_POOL; *p && !ok; p++) if (*p != '.' && *p != '#') ok = take(o.second, *p);
        for (int ch = 33; ch < 127 && !ok; ch++) ok = take(o.second, (char)ch);
    }
    auto alloc = [&](const std::string& spec) -> char { return chars[spec]; };
    std::string o = "[map]\n";
    bool hasId = false; for (auto& kv : header) { if (kv.first == "id") hasId = true; o += kv.first + "=" + kv.second + "\n"; }
    if (!hasId) o += "id=" + id + "\n";
    o += "\n[legend]\n";
    std::vector<std::string> rowsOut;
    for (int y = 0; y < h; y++) { std::string r; for (int x = 0; x < w; x++) r += alloc(cells[(size_t)y * w + x]); rowsOut.push_back(r); }
    std::vector<std::pair<char, std::string>> leg; for (auto& kv : chars) leg.push_back({kv.second, kv.first});
    std::sort(leg.begin(), leg.end());
    for (auto& l : leg) o += std::string(1, l.first) + " = " + l.second + "\n";
    o += "\n[rows]\n"; for (auto& r : rowsOut) o += r + "\n"; o += "\n";
    for (auto& ob : objs) { o += "[" + ob.type + "]\n"; for (auto& kv : ob.kv) o += kv.first + "=" + kv.second + "\n"; o += "\n"; }
    return o;
}

// ============================================================ TextEdit
std::string TextEdit::text() const { std::string o; for (size_t i = 0; i < lines.size(); i++) { o += lines[i]; if (i + 1 < lines.size()) o += "\n"; } return o + "\n"; }
bool TextEdit::save() { if (!writeFile(path, text())) return false; dirty = false; return true; }
std::string TextEdit::selText() const {
    if (!hasSel()) return ""; int x0, y0, x1, y1; selRange(x0, y0, x1, y1); std::string o;
    for (int y = y0; y <= y1; y++) { int a = y == y0 ? x0 : 0, b = y == y1 ? x1 : (int)lines[y].size(); o += lines[y].substr(a, b - a); if (y != y1) o += "\n"; }
    return o;
}
void TextEdit::deleteSel() {
    if (!hasSel()) { ay = -1; return; } int x0, y0, x1, y1; selRange(x0, y0, x1, y1);
    std::string merged = lines[y0].substr(0, x0) + lines[y1].substr(x1);
    lines.erase(lines.begin() + y0, lines.begin() + y1 + 1); lines.insert(lines.begin() + y0, merged);
    cx = x0; cy = y0; ay = -1; dirty = true;
}
void TextEdit::insert(const std::string& t) {
    auto parts = split(t, '\n');
    for (size_t i = 0; i < parts.size(); i++) {
        std::string clean; for (char c : parts[i]) if (c >= 32 && c < 127) clean += c;
        if (i > 0) { std::string rest = lines[cy].substr(cx); lines[cy] = lines[cy].substr(0, cx); lines.insert(lines.begin() + cy + 1, rest); cy++; cx = 0; }
        lines[cy].insert(cx, clean); cx += (int)clean.size();
    }
    dirty = true;
}

// ============================================================ Editor
bool Editor::init(Tileset* t, Font* f, GameData* g, const std::string& assets, PlatformRequests* r) {
    ts = t; font = f; gd = g; assetDir = assets; ui.req = r; baseRoot = dataRoot = assetDir + "/data";
    te.wrap = true;   // 1.1.11: dialogue text= lines are long - wrapped by default (Alt+Z / the Wrap button)
    auto mk = [&](const std::string& file, const std::string& title, const std::string& section, const std::string& listKey, std::vector<std::string> tmpl) { GenericEditor ge; ge.path = dataRoot + "/" + file; ge.title = title; ge.sectionName = section; ge.listKey = listKey; ge.templateKeys = tmpl; gens.push_back(ge); };
    mk("items.txt", "Items", "item", "id", {"id", "name", "type", "tile", "value", "desc", "dmg", "dmgtype", "range", "ap", "stat", "def", "evasion", "res", "bonus", "heal", "mana", "effect", "light", "salvage", "tech", "sound", "category", "quest", "unique", "trait", "fx", "family", "weight", "req", "wornby"});
    mk("creatures.txt", "Creatures", "creature", "id", {"id", "name", "sprite", "hp", "stats", "dmg", "dmgtype", "range", "apmax", "move", "def", "evasion", "res", "ai", "spells", "xp", "level", "gold", "loot", "faction", "size", "darkvision", "undead", "mechanical", "sound", "attacksound", "boss", "music", "role", "windup", "ward", "snuff", "fx"});
    mk("spells.txt", "Spells", "spell", "id", {"id", "name", "desc", "tile", "cost", "ap", "range", "area", "dmg", "dmgtype", "stat", "heal", "effect", "duration", "target", "utility", "ff", "level", "sound", "shape", "chain", "knockback", "smoke", "ground", "upgrade"});
    mk("classes.txt", "Classes", "class", "id", {"id", "name", "desc", "role", "stats", "hd", "md", "weapon", "armor", "items", "spells", "spriterow", "spritecol", "learn", "weapons", "wears", "passive", "perk4", "perk8", "perk12"});
    mk("shops.txt", "Shops", "shop", "id", {"id", "hours", "faction", "item"});
    mk("encounters.txt", "Encounter tables", "table", "id", {"id", "level", "group"});
    mk("quests.txt", "Quests", "quest", "id", {"id", "name", "stage"});
    mk("dungeons.txt", "Dungeons", "dungeon", "id", {"id", "name", "style", "w", "h", "seed", "level", "exit", "monsters", "boss", "bossflag", "bossdefeat", "loot", "music", "combatmusic", "ambient", "table", "touch"});
    mk("overworld.txt", "Overworld locations", "location", "id", {"id", "name", "x", "y", "kind", "map", "tx", "ty", "clear", "road", "require", "locked", "reveal"});
    mk("recipes.txt", "Recipes", "recipe", "id", {"id", "name", "result", "count", "station", "materials", "gold", "level", "difficulty", "skill", "known", "reveal"});          // gens[9]
    mk("affixes.txt", "Affixes", "affix", "id", {"id", "name", "pos", "tier", "types", "mods", "dmgtype", "value"});                   // gens[10]
    mk("banter.txt", "Banter", "banter", "class", {"class", "when", "map", "if", "flag", "text"});                                    // gens[11]
    mk("achievements.txt", "Achievements", "achievement", "id", {"id", "name", "desc", "cond", "tile", "hidden"});                   // gens[12]
    mk("bounties.txt", "Notice boards & bounties", "board", "id", {"id", "name", "map", "location", "offers", "kinds", "kill", "creatures", "fetch", "deliver", "reward"});   // gens[13]
    mk("factions.txt", "Factions (reputation)", "faction", "id", {"id", "name", "text", "reward", "project"});                                          // gens[14]
    mk("objects.txt", "Object types & templates", "template", "id", {"id", "type", "name"});                                       // gens[15] (1.1.0)
    mk("resources.txt", "Resource nodes", "resource", "id", {"id", "name", "item", "count", "tool", "respawn", "sprite", "color", "spentsprite", "spent", "kind", "styles", "sound"});   // gens[16] (1.1.0; sound 1.3.3)
    mk("npcs.txt", "NPCs", "npc", "id", {"id", "name", "sprite", "dialogue"});   // gens[17] (1.1.9 #33): tab 24
    gens[13].altSection = "bounty"; gens[13].altTemplateKeys = {"id", "board", "kind", "target", "need", "where", "reward", "xp", "title", "text", "require", "repeat"};
    gens[15].altSection = "type"; gens[15].altTemplateKeys = {"id"};   // 1.1.1: [type] = what every object of a type uses
    loadGuide();
    return true;
}
void Editor::refreshFiles() {
    mapFiles.clear(); dlgFiles.clear(); std::error_code ec;
    std::vector<std::string> roots{dataRoot};   // a campaign is its own game: only its own files are listed
    for (auto& root : roots) {
        for (auto& e : fs::directory_iterator(root + "/maps", ec)) if (e.path().extension() == ".txt" && std::find(mapFiles.begin(), mapFiles.end(), e.path().stem().string()) == mapFiles.end()) mapFiles.push_back(e.path().stem().string());
        for (auto& e : fs::directory_iterator(root + "/dialogue", ec)) if (e.path().extension() == ".txt" && std::find(dlgFiles.begin(), dlgFiles.end(), e.path().stem().string()) == dlgFiles.end()) dlgFiles.push_back(e.path().stem().string());
    }
    scriptFiles.clear(); for (auto& e : fs::directory_iterator(dataRoot + "/scripts", ec)) if (e.path().extension() == ".txt") scriptFiles.push_back(e.path().stem().string());   // 1.1.10
    std::sort(scriptFiles.begin(), scriptFiles.end());
    // the campaign's own files first, then the ones it inherits from the base game
    auto ownFirst = [&](std::vector<std::string>& v, const std::string& sub) { std::sort(v.begin(), v.end()); if (dataRoot == baseRoot) return; std::stable_partition(v.begin(), v.end(), [&](const std::string& n) { return fileExists(dataRoot + "/" + sub + "/" + n + ".txt"); }); };
    ownFirst(mapFiles, "maps"); ownFirst(dlgFiles, "dialogue");
}
void Editor::setCampaign(const std::string& id) {
    if (onSwitchCampaign && !onSwitchCampaign(id)) { say("Could not load campaign " + id); return; }
    auto all = listCampaigns(assetDir); const CampaignInfo* c = nullptr; for (auto& x : all) if (x.id == id) c = &x;
    if (!c) return;
    campaignId = c->id; campaignInherit = c->inherit; dataRoot = c->dir.empty() ? baseRoot : c->dir; baseResLoaded = false; baseNpcsLoaded = false; npcJoinsDirty = true; placeNpc.clear();
    for (auto& g : gens) {
        std::string file = fs::path(g.path).filename().string();
        g.path = dataRoot + "/" + file;
        g.basePath.clear();   // campaigns are edited on their own, never mixed with the base game
        g.replaceWhole = file == "overworld.txt";
        g.load(); g.sel = -1; g.scroll = 0; g.filter.clear(); g.filterKey.clear(); g.filterVal.clear();
    }
    mapLoaded = false; mapDirty = false; te.clearKeepView(); mapFilter.clear(); dlgFilter.clear(); refreshFiles(); worldNeedsGen = true; mapListScroll = 0; tab = 0;
    loadCampaignForm(); loadPlayState(); loadFlagDocs(); flagIndexDirty = true; mapScan.clear(); flagSelName.clear(); questPreview.clear();
    // open the campaign's own starting map (its own files are listed first)
    std::string first = c->startMap; if (std::find(mapFiles.begin(), mapFiles.end(), first) == mapFiles.end() && !mapFiles.empty()) first = mapFiles[0];
    for (size_t i = 0; i < mapFiles.size(); i++) if (mapFiles[i] == first) { mapListSel = (int)i; loadMapFile(first); }
    say("Editing campaign: " + c->name + (c->dir.empty() ? " (the base game)" : " - " + c->dir));
}
// 1.1.6 (owner's #29): a new campaign is an empty folder - campaign.txt with its name and nothing else. Each tab's Import
// button copies what it needs from the base game or another campaign; the first map made becomes the start.
void Editor::newCampaign(const std::string& name0) {
    std::string name = trim(name0); if (name.empty()) return;
    std::string id; for (char ch : toLower(name)) { if (isalnum((unsigned char)ch)) id += ch; else if (!id.empty() && id.back() != '_') id += '_'; }
    while (!id.empty() && id.back() == '_') id.pop_back(); if (id.empty() || id == "triffall") id = "campaign";
    std::string dir = assetDir + "/campaigns/" + id; std::error_code ec;
    if (fs::exists(dir)) { say("A campaign folder '" + id + "' already exists."); return; }
    fs::create_directories(dir + "/maps", ec); fs::create_directories(dir + "/dialogue", ec);
    writeFile(dir + "/campaign.txt", "# Campaign settings - edit them in the Editor's Campaign tab. This campaign is a separate game: it only uses the\n# files in this folder (sounds are shared by every campaign). It started empty: each tab's Import button copies\n# entries in from the base game or another campaign; the Tileset tab imports sprite sheets into sheets/.\n[campaign]\nname=" + name + "\ndesc=\nauthor=\nicon=0,7\ngold=0\n");
    setCampaign(id);
    say("Created the empty campaign '" + name + "'. Each tab's Import button brings in items, monsters, maps... from other campaigns; the first new map becomes the start.");
}
void Editor::open() {
    for (auto& g : gens) g.load();
    refreshFiles();
    if (!camp.dirty) loadCampaignForm();
    if (!flagDocsDirty) loadFlagDocs();
    flagIndexDirty = true;
    worldNeedsGen = true; wantsClose = false; pop = P_NONE; ui.blur();
    if (!mapLoaded && !mapFiles.empty()) { mapListSel = 0; loadMapFile(mapFiles[0]); }
}

// ---------------- popups ----------------
void Editor::openTilePicker(Tile current, std::function<void(Tile)> cb, char cat) {
    syncBooks(); pkCb = cb; pkCur = current; popTile = cb; popCur = current; popTitle = "Pick a tile";
    auto vis = visibleSheets();
    if (current.valid() && std::find(vis.begin(), vis.end(), (int)current.s) != vis.end()) pkSheet = current.s;
    else if (std::find(vis.begin(), vis.end(), pkSheet) == vis.end()) pkSheet = vis.empty() ? 0 : vis[0];
    pkCat = cat; pkGroup = -1; pkScroll = 0; pkGroupScroll = 0; pkFind.clear();
    if (cat) { int n = 0; pickerTiles(pkSheet, cat, -1, n); if (n == 0) pkCat = 0; }   // nothing sorted into it on this sheet yet: show them all
    if (current.valid()) { tileScrollX = std::max(0, current.c * 26 - 400); tileScrollY = std::max(0, current.r * 26 - 200); }
    reopenTilePicker();
}
void Editor::openListPicker(const std::string& title, const std::vector<std::string>& items, int cur, std::function<void(int)> cb) { popCancel = nullptr; popOpenedFrame = frameNo; pop = P_LIST; popItems = items; popList = cb; popSel = cur; popScroll = std::max(0, cur - 5); popTitle = title; popValue.clear(); ui.modal = true; ui.blur(); }
void Editor::openConfirm(const std::string& title, std::function<void()> yes) { popCancel = nullptr; popOpenedFrame = frameNo; pop = P_CONFIRM; popYes = yes; popTitle = title; ui.modal = true; ui.blur(); }
void Editor::openTextPrompt(const std::string& title, const std::string& initial, std::function<void(const std::string&)> cb) { popCancel = nullptr; popOpenedFrame = frameNo; pop = P_TEXT; popText = cb; popTitle = title; popValue = initial; ui.modal = true; ui.blur(); }

void Editor::drawPopup(InputState& in) {
    if (pop == P_NONE) return;
    Canvas& cv = *ui.cv;
    cv.dim(0, 0, W, H, 140);
    ui.modal = false; // widgets inside the popup may use the mouse
    if (popOpenedFrame == frameNo) { in.eatMouse(); in.eatKeys(); } // the click/key that opened us must not act inside
    bool close = false;
    ui.bigAllowed = false;
    if (pop == P_TILE) {
        drawTilePicker(in, close);
    } else if (pop == P_LIST) {
        int w = 360, h = 360, x = (W - w) / 2, y = (H - h) / 2;
        ui.panel(x, y, w, h); ui.header(x, y, w, popTitle);
        ui.label(x + 8, y + 22, "find:"); ui.idCounter = 6000; if (ui.focus < 6001) { ui.focus = 6001; ui.caret = (int)popValue.size(); } ui.searchField(x + 50, y + 20, w - 60, popValue, "type to search");
        std::vector<std::string> shown; std::vector<int> idx; std::string f = toLower(popValue);
        for (size_t i = 0; i < popItems.size(); i++) if (edMatch(popItems[i], popValue)) { shown.push_back(popItems[i]); idx.push_back((int)i); }
        int lsel = -1; for (size_t i = 0; i < idx.size(); i++) if (idx[i] == popSel) lsel = (int)i;
        if (in.pressed[K_DOWN] && !shown.empty()) { lsel = std::min((int)shown.size() - 1, lsel + 1); popSel = idx[lsel]; }
        if (in.pressed[K_UP] && !shown.empty()) { lsel = std::max(0, lsel - 1); popSel = idx[lsel]; }
        int hit = ui.listBox(x + 8, y + 42, w - 16, h - 80, shown, lsel, popScroll);
        if (hit < 0 && in.pressed[K_ENTER] && !shown.empty()) hit = lsel >= 0 ? lsel : 0;
        if (hit >= 0) { auto cb = popList; close = true; cb(idx[hit]); }
        ui.text(x + 8, y + h - 24, fmt("%d of %d - arrows + Enter or click", (int)shown.size(), (int)popItems.size()), DIM);
        if (!close && (ui.button(x + w - 80, y + h - 28, 70, 20, "Cancel") || in.pressed[K_ESCAPE])) { close = true; if (popCancel) { auto c = popCancel; popCancel = nullptr; c(); } }
    } else if (pop == P_CONFIRM) {
        int w = 360, h = 110, x = (W - w) / 2, y = (H - h) / 2;
        ui.panel(x, y, w, h); ui.header(x, y, w, "Confirm");
        font->drawWrapped(cv, *ts, x + 10, y + 26, w - 20, popTitle, TEXT);
        if (ui.button(x + w - 170, y + h - 30, 70, 22, "Yes", true, false, OK) || in.pressed[K_ENTER]) { auto cb = popYes; close = true; cb(); }
        if (!close && (ui.button(x + w - 90, y + h - 30, 70, 22, "No") || in.pressed[K_ESCAPE])) { close = true; if (popCancel) { auto c = popCancel; popCancel = nullptr; c(); } }
    } else if (pop == P_HELP) {
        drawGuide(in, close);
    } else if (pop == P_REPORT) {
        drawReport(in, close);
    } else if (pop == P_IMPORT) {
        drawImport(in, close);
    } else if (pop == P_BIGTEXT) {
        drawBigText(in, close);
    } else if (pop == P_TEXT) {
        int w = 360, h = 100, x = (W - w) / 2, y = (H - h) / 2;
        ui.panel(x, y, w, h); ui.header(x, y, w, popTitle);
        ui.idCounter = 5000; if (ui.focus < 5001) { ui.focus = 5001; ui.caret = (int)popValue.size(); }
        ui.textField(x + 10, y + 30, w - 20, popValue, 60);
        if (ui.button(x + w - 170, y + h - 30, 70, 22, "OK", !popValue.empty(), false, OK) || (in.pressed[K_ENTER] && !popValue.empty())) { auto cb = popText; std::string v = popValue; close = true; cb(v); }
        if (!close && (ui.button(x + w - 90, y + h - 30, 70, 22, "Cancel") || in.pressed[K_ESCAPE])) { close = true; if (popCancel) { auto c = popCancel; popCancel = nullptr; c(); } }
    } else if (pop == P_SHEET) {
        drawSheetImport(in, close);
    }

    else if (drawPopupBuilders(in, close)) {}   // 1.6.0: editor_builders.cpp (condition / action builders, search, Check all)

    else if (drawPopupStory(in, close)) {}      // 1.6.0: editor_story.cpp

    else if (drawPopupRules(in, close)) {}      // 1.6.0: editor_rules.cpp

    else if (drawPopupHub(in, close)) {}        // 1.6.0: editor_hub.cpp (NPC hub, bounties, Test this)

    else if (drawPopupQuests(in, close)) {}     // 1.6.0: editor_quests.cpp (quest builder, starter campaign, checklist)

    if (close && popOpenedFrame == frameNo) { ui.modal = true; in.eatMouse(); in.eatKeys(); }   // the callback opened the next popup (e.g. New campaign -> name prompt): keep that one
    else if (close) { pop = P_NONE; ui.modal = false; ui.blur(); in.eatMouse(); in.eatKeys(); if (bigRefocus) { ui.focus = bigTarget; bigRefocus = false; } }
    else ui.modal = true;
    ui.bigAllowed = true;
}

// 1.6.0 (#108): unsaved changes per tab (the * on tabs and workspaces)
bool Editor::tabDirty(int i) {
    switch (i) {
        case 0: return mapDirty;
        case 1: return kindsDirty;
        case 2: return gens[8].dirty;
        case 3: case 4: case 5: case 6: case 7: case 8: case 9: case 10: return gens[i - 3].dirty;
        case 11: return te.dirty;
        case 12: return sndDirty;
        case 14: case 15: case 16: case 17: return gens[i - 5].dirty;
        case 18: return camp.dirty;
        case 19: return flagDocsDirty;
        case 20: return gens[13].dirty;
        case 21: return gens[14].dirty;
        case 22: return gens[15].dirty;
        case 23: return gens[16].dirty;
        case 24: return gens[17].dirty;
        case 25: return ste.dirty;

        case 26: case 27: case 28: case 29: case 30: return tabDirtyStory(i);

        case 31: case 32: case 33: case 34: return tabDirtyRules(i);

        default: return false;
    }
}
bool Editor::anyDirty() { for (int i = 0; i < NTABS; i++) if (tabDirty(i)) return true; return false; }
// 1.6.0 (#108): the Editor's own preferences (saves/editor.ini, next to settings.ini)
void Editor::loadPrefs() {
    prefsLoaded = true; std::string t;
    if (prefsPath.empty() || !readFile(prefsPath, t)) return;
    for (auto& sec : parseConfig(t)) for (auto& kv : sec.kv) {
        if (kv.first == "flattabs") flatTabs = kv.second == "1";
    }
}
void Editor::savePrefs() {
    if (prefsPath.empty()) return;
    std::string s = "[editor]\nflattabs=" + std::string(flatTabs ? "1" : "0") + "\n";
    writeFile(prefsPath, s);
}

// ---------------- main ----------------
void Editor::update(InputState& in, float dt, Canvas& cv) {
    if (cv.w != W || cv.h != H) cv.init(W, H);
    frameNo++;
    ui.begin(&cv, ts, font, &in, dt);
    ui.modal = pop != P_NONE;
    cv.clear(BG);
    if (toastT > 0) toastT -= dt;
    // tabs
    // tabs flow into a second row when they do not fit beside the Exit button
    // 1.3.1 (#46): one row = Campaign | Guide | Check all | Exit side by side (needs W - 382 for the tabs); else the
    // buttons stack in two rows at the right (W - 250 for the tabs) and the bar is at least two rows high
    // 1.6.0 (#108): tabs are grouped into workspaces (row 1) with their tabs underneath (row 2); when everything fits
    // beside the header buttons it is one row (workspaces | tabs). "All tabs" (editor.ini flattabs=1) = the old flat bar.
    if (!prefsLoaded) loadPrefs();
    auto pickTab = [&](int i) { tab = i; ui.blur(); if (i != 1) kindReturnTab = -1; if (i == 19 || i == 8) flagIndexDirty = true; wsLastTab[workspaceOf(i)] = i; };
    const char* flatLbl = flatTabs ? "Workspaces" : "All tabs";
    int flatW = font->width(flatLbl) + 8;
    if (flatTabs) {
        int tabsW = 4; for (int i = 0; i < NSHOWN; i++) tabsW += font->width(TABS[i]) + 10;
        tabsW += flatW + 10;
        hdrWide = tabsW <= W - 382;
        int lim = hdrWide ? W - 382 : W - 250;
        int rows = 1, fx = 4; for (int i = 0; i <= NSHOWN; i++) { int tw = (i < NSHOWN ? font->width(TABS[i]) + 8 : flatW); if (fx + tw > lim) { rows++; fx = 4; } fx += tw + 2; }
        if (!hdrWide && rows < 2) rows = 2;
        gEdTop = 6 + 22 * rows;   // 28 / 50 / 72
        cv.fillRect(0, 0, W, TOP, PANEL);
        int tx = 4, trow = 0;
        for (int i = 0; i <= NSHOWN; i++) {
            int tw = i < NSHOWN ? font->width(TABS[i]) + 8 : flatW;
            if (tx + tw > lim) { trow++; tx = 4; }
            if (i == NSHOWN) {
                if (ui.button(tx, 4 + trow * 22, tw, 20, flatLbl, true, false, DIM)) { flatTabs = false; savePrefs(); }
                if (ui.hover(tx, 4 + trow * 22, tw, 20)) ui.tip("Group the tabs into workspaces again (World, People, Story...).");
                break;
            }
            if (ui.button(tx, 4 + trow * 22, tw, 20, std::string(TABS[i]) + (tabDirty(i) ? "*" : ""), true, tab == i)) pickTab(i);
            tx += tw + 2;
        }
    } else {
        auto& ws = workspaces(); int cw = workspaceOf(tab);
        auto wsW = [&](size_t i) { return font->width(ws[i].name) + 14; };
        auto subW = [&](int t) { return font->width(TAB_LONG[t]) + 10; };
        int row1 = 4; for (size_t i = 0; i < ws.size(); i++) row1 += wsW(i) + 2;
        int maxSub = 0; for (auto& w : ws) { int s = 0; for (int t : w.tabs) s += subW(t) + 2; maxSub = std::max(maxSub, s); }
        hdrWide = row1 + 14 + maxSub + flatW + 6 <= W - 382;   // the widest workspace decides, so the bar doesn't jump between workspaces
        gEdTop = hdrWide ? 28 : 50;
        cv.fillRect(0, 0, W, TOP, PANEL);
        int tx = 4;
        for (size_t i = 0; i < ws.size(); i++) {
            bool d = false; for (int t : ws[i].tabs) if (tabDirty(t)) d = true;
            int w = wsW(i);
            if (ui.button(tx, 4, w, 20, std::string(ws[i].name) + (d ? "*" : ""), true, (int)i == cw, (int)i == cw ? ACCENT2 : TEXT)) {
                int t = wsLastTab[i] >= 0 ? wsLastTab[i] : ws[i].tabs[0]; pickTab(t);
            }
            if (ui.hover(tx, 4, w, 20)) ui.tip(ws[i].tip);
            tx += w + 2;
        }
        cw = workspaceOf(tab);
        int sx = hdrWide ? tx + 12 : 4, sy = hdrWide ? 4 : 26;
        if (hdrWide) cv.fillRect(tx + 5, 6, 1, 16, DIM);
        else cv.fillRect(0, 24, W - 246, 1, BORDER);
        for (int t : ws[cw].tabs) {
            int w = subW(t);
            if (ui.button(sx, sy, w, 20, std::string(TAB_LONG[t]) + (tabDirty(t) ? "*" : ""), true, tab == t)) pickTab(t);
            sx += w + 2;
        }
        int fxx = hdrWide ? W - 382 - flatW : tx + 8;
        if (ui.button(fxx, 4, flatW, 20, flatLbl, true, false, DIM)) { flatTabs = true; savePrefs(); }
        if (ui.hover(fxx, 4, flatW, 20)) ui.tip("Show every tab in one bar, one per data file (the Editor's layout before 1.6.0).");
    }
    if (tab == 1 && kindReturnTab >= 0 && !ui.typing() && pop == P_NONE && in.pressed[K_ESCAPE]) { tab = kindReturnTab; kindReturnTab = -1; in.eatKeys(); }
    {
        std::string cname = "Triffall"; for (auto& c : listCampaigns(assetDir)) if (c.id == campaignId) cname = c.name;
        if (ui.button(hdrWide ? W - 374 : W - 240, 4, 164, 20, "Campaign: " + cname.substr(0, 14), true, campaignId != "triffall", ACCENT2)) {
            auto all = listCampaigns(assetDir); std::vector<std::string> names; int cur = 0;
            for (size_t i = 0; i < all.size(); i++) { names.push_back(all[i].name + (all[i].id == "triffall" ? "  (base game)" : "  [" + all[i].id + "]")); if (all[i].id == campaignId) cur = (int)i; }
            names.push_back("+ New campaign...");
            std::vector<CampaignInfo> deletable; for (auto& c : all) if (!isCoreCampaign(c.id)) deletable.push_back(c);
            if (!deletable.empty()) names.push_back("- Delete a campaign...");   // 1.7.0 (#145)
            openListPicker("Campaign to edit", names, cur, [this, all, deletable](int i) {
                if (i == (int)all.size() + 1) {   // 1.7.0 (#145): pick one (never the base game or the Tutorial), confirm, delete
                    std::vector<std::string> dn; for (auto& c : deletable) dn.push_back(c.name + "  [" + c.id + "]");
                    openListPicker("Delete which campaign? (the base game and the Tutorial can't be deleted)", dn, 0, [this, deletable](int k) {
                        if (k < 0 || k >= (int)deletable.size()) return;
                        std::string id = deletable[k].id, nm = deletable[k].name;
                        openConfirm("Delete the campaign " + nm + " (assets/campaigns/" + id + ") and its saves for good? This can't be undone." + (id == campaignId && anyDirty() ? " Its unsaved changes are lost too." : ""), [this, id, nm]() {
                            std::string err;
                            if (onDeleteCampaign && onDeleteCampaign(id, err)) say("Deleted the campaign " + nm + ".");
                            else say("Couldn't delete " + nm + ": " + (err.empty() ? std::string("not possible here.") : err));
                        });
                    });
                    return;
                }
                if (i >= (int)all.size()) { openTextPrompt("Name of the new campaign (it starts empty)", "", [this](const std::string& nm) { newCampaign(nm); }); return; }
                bool any = anyDirty();
                std::string id = all[i].id;
                if (any) openConfirm("Unsaved changes will be lost. Switch campaign anyway?", [this, id]() { setCampaign(id); }); else setCampaign(id);
            });
        }
        if (ui.hover(hdrWide ? W - 374 : W - 240, 4, 164, 20)) ui.tip("Each campaign is a separate game in assets/campaigns/<id>/ with its own maps, items, monsters... and its own desktop icon. The Campaign tab sets its name and where new games start.");
    }
    { int gx = hdrWide ? W - 206 : W - 240, gy = hdrWide ? 4 : 26, gw = hdrWide ? 60 : 164;
      int kx = hdrWide ? W - 140 : W - 70, ky = hdrWide ? 4 : 26;
      if (ui.button(gx, gy, gw, 20, hdrWide ? "? Guide" : "? Guide  (F1)", true, false, ACCENT)) openGuide(tabTopic());
      if (ui.hover(gx, gy, gw, 20)) ui.tip("How everything in the Editor works (F1). The small ? buttons open it on the matching page.");
      if (in.pressed[K_F1] && pop == P_NONE) openGuide(tabTopic());
      if (ui.button(kx, ky, 64, 20, "Check all", true, false, ACCENT2) && pop == P_NONE) { runCampaignCheck(); popOpenedFrame = frameNo; pop = P_REPORT; ui.modal = true; ui.blur(); }
      if (ui.hover(kx, ky, 64, 20)) ui.tip("Check the whole campaign: maps, dialogue, flags, quests, shops, classes, creatures, recipes, tables..."); }
    if (ui.button(W - 70, 4, 64, 20, "Exit", true, false, DANGER) || (!ui.typing() && pop == P_NONE && in.pressed[K_ESCAPE] && !mapPanning)) {
        bool any = anyDirty();
        if (any) openConfirm("There are unsaved changes. Exit anyway? (Save each tab with its Save button.)", [this]() { wantsClose = true; });
        else wantsClose = true;
    }
    if (tab != lastTab) { if (tab == 25) flagIndexDirty = true; if (tab == 3 || tab == 4 || tab == 5 || tab == 7 || tab == 8 || tab == 9 || tab == 11 || tab == 19 || tab == 20 || tab == 22 || tab == 23 || tab == 24) flagIndexDirty = true; if (tab == 22 || tab == 23 || tab == 24) poNeedScan = true; if (tab == 24) npcJoinsDirty = true; lastTab = tab; wsLastTab[workspaceOf(tab)] = tab; }
    // 1.2.0 (#36): a PNG dropped on the window opens the sprite sheet import (the import window takes it itself)
    if (pop == P_NONE) for (auto& d : in.dropped) if (toLower(fs::path(d).extension().string()) == ".png") { tab = 13; openSheetImport(d); break; }
    switch (tab) {
        case 0: drawMaps(in); break;
        case 8: { int uw = 330; drawGeneric(gens[5], 4, TOP + 4, W - 8 - uw - 6, H - TOP - BOTTOM - 8, in); drawEncounterUsage(W - 4 - uw, TOP + 4, uw, H - TOP - BOTTOM - 8, in); break; }
        case 1: drawKinds(in); break;
        case 2: drawWorld(in); break;
        case 11: drawDialogue(in); break;
        case 12: drawSounds(in); break;
        case 13: drawTileset(in); break;
        case 18: drawCampaign(in); break;
        case 19: drawFlags(in); break;
        case 21: { int uw = 330; drawGeneric(gens[14], 4, TOP + 4, W - 8 - uw - 6, H - TOP - BOTTOM - 8, in); drawFactionInfo(W - 4 - uw, TOP + 4, uw, H - TOP - BOTTOM - 8, in); break; }
        case 20: { int uw = 330; drawGeneric(gens[13], 4, TOP + 4, W - 8 - uw - 6, H - TOP - BOTTOM - 8, in); drawBoardInfo(W - 4 - uw, TOP + 4, uw, H - TOP - BOTTOM - 8, in); break; }
        case 22: drawObjectsTab(in); break;
        case 23: drawResourcesTab(in); break;
        case 24: drawGeneric(gens[17], 4, TOP + 4, W - 8, H - TOP - BOTTOM - 8, in); break;   // 1.1.9: NPCs (the info box is drawn under the fields)
        case 25: drawScripts(in); break;   // 1.1.10 (#34)
        case 14: case 15: case 16: case 17: drawGeneric(gens[tab - 5], 4, TOP + 4, W - 8, H - TOP - BOTTOM - 8, in); break;

        case 9: drawGeneric(gens[6], 4, TOP + 4, W - 8, H - TOP - BOTTOM - 8, in); break;   // Quests

        // 1.6.0 tabs 26-30 (Lore, Epilogue, Credits, Hints, Contradictions): editor_story.cpp
        case 26: case 27: case 28: case 29: case 30: drawTabStory(in); break;

        // 1.6.0 tabs 31-34 (Dungeon styles, Healers, Combat rules, Formulas): editor_rules.cpp
        case 31: case 32: case 33: case 34: drawTabRules(in); break;

        default: drawGeneric(gens[tab - 3], 4, TOP + 4, W - 8, H - TOP - BOTTOM - 8, in); break;
    }
    // status bar
    cv.fillRect(0, H - BOTTOM, W, BOTTOM, PANEL);
    ui.text(6, H - BOTTOM + 3, toastT > 0 ? toast : "Editor - Tab/Shift+Tab move between fields, Enter/Esc leave a field, Ctrl+S saves the current tab, wheel over a number nudges it (shift = x10).", toastT > 0 ? ACCENT2 : DIM);
    // 1.1.11 (#35): a single-line field asked to be edited in the big box
    if (ui.bigReqId >= 0) { if (pop == P_NONE) openBigText(ui.bigReqId, ui.bigReqValue, ui.bigReqMax); ui.bigReqId = -1; }
    drawPopup(in);
    ui.drawMenu();
    ui.drawTooltip();
    // cursor
    if (in.mouseInWindow) { int mx = in.mouseX, my = in.mouseY; static const char* cur[] = {"X........", "XX.......", "XOX......", "XOOX.....", "XOOOX....", "XOOOOX...", "XOOOOOX..", "XOOOOOOX.", "XOOOOXXXX", "XOXXOX...", "XX.XOOX..", "...XOOX..", "....XX..."}; for (int y = 0; y < 13; y++) for (int x = 0; x < 9; x++) { char ch = cur[y][x]; if (ch == 'X') cv.put(mx + x, my + y, C::Black); else if (ch == 'O') cv.put(mx + x, my + y, C::White); } }
}

// ---------------- generic key/value editor ----------------
// 1.2.0 (#37): the tile category a sprite field's picker opens on
// 1.3.1 (#48): the track a map with no music= plays (Game::musicWanted)
static std::string edKindMusic(const std::string& kind) { return kind == "overworld" ? "overworld" : kind == "town" || kind == "interior" ? "town" : kind == "dungeon" ? "dungeon" : "forest"; }
static char tileCatFor(const std::string& sec, const std::string& key) {
    if (key == "partysprite" || sec == "class" || sec == "npc") return 'c';
    if (sec == "item") return 'i';
    if (sec == "creature") return 'm';
    if (sec == "spell" || sec == "achievement") return 'e';
    if (sec == "template" || sec == "type" || sec == "resource") return 'p';
    return 0;
}
static bool isTileKey(const std::string& k) { return k == "tile" || k == "sprite" || k == "spentsprite" || k == "opensprite" || k == "onsprite" || k == "partysprite"; }
static bool isColorKey(const std::string& k) { return k == "color" || k == "tint" || k == "bg" || k == "spent" || k == "sparklecolor"; }

// 1.1.6 (#30): Filter button - pick a field the entries have, then one of its values (counted)
void Editor::openFieldFilter(GenericEditor& g) {
    std::map<std::string, std::map<std::string, int>> vals; std::map<std::string, int> keyN;
    for (auto& sc : g.secs) { std::set<std::string> seen; for (auto& kv : sc.kv) { if (kv.first == g.listKey && kv.first != "type") continue; if (seen.insert(kv.first).second) keyN[kv.first]++; for (auto& t : edFilterTokens(kv.second)) vals[kv.first][t]++; } }
    std::vector<std::string> keys, labels;
    for (auto& k : keyN) { if (vals[k.first].size() > 400) continue; keys.push_back(k.first); }   // free text (desc, text...) has nothing to pick from
    std::sort(keys.begin(), keys.end(), [&](const std::string& a, const std::string& b) { return keyN[a] != keyN[b] ? keyN[a] > keyN[b] : a < b; });
    for (auto& k : keys) labels.push_back(k + fmt("   (%d entries)", keyN[k]));
    if (keys.empty()) { say("Nothing to filter by yet."); return; }
    GenericEditor* gp = &g;
    openListPicker("Filter by which field?", labels, 0, [this, gp, keys, vals](int i) {
        std::string key = keys[i]; auto vm = vals.at(key);
        std::vector<std::pair<int, std::string>> order; for (auto& v : vm) order.push_back({-v.second, v.first}); std::sort(order.begin(), order.end());
        std::vector<std::string> ids{"\x01has", "\x01none"}, lab{"(any value - has " + key + ")", "(missing or empty)"};
        for (auto& o : order) { ids.push_back(o.second); lab.push_back(o.second + fmt("   (%d)", -o.first)); }
        openListPicker(key + " = ?", lab, 2 < (int)lab.size() ? 2 : 0, [this, gp, key, ids](int j) { gp->filterKey = key; gp->filterVal = ids[j]; gp->scroll = 0; });
    });
}
void Editor::drawGeneric(GenericEditor& g, int x, int y, int w, int h, InputState& in) {
    genUndo(g, in);
    int lw = 230;
    auto idx = g.filtered(); std::vector<std::string> labels;
    ui.panel(x, y, lw, h); { int own = 0; for (auto& sc : g.secs) if (!sc.inherited) own++;
        std::string cnt = own == (int)g.secs.size() ? fmt(" (%d)", own) : fmt(" (%d + %d base)", own, (int)g.secs.size() - own);
        if (!g.filter.empty() || !g.filterKey.empty()) cnt = fmt(" (%d of %d)", (int)idx.size(), (int)g.secs.size());
        listHeader(x, y, lw, g.title + cnt, IMP_GEN, (int)(&g - &gens[0])); }
    helpBtn(x + lw - 16, y + 1, tabTopic());
    // 1.1.6 (#30): search box (every value) + a field filter ("type = weapon"), shown as a removable line under it
    if (ui.searchField(x + 4, y + 20, lw - 58, g.filter, "search...", 60)) g.scroll = 0;
    if (ui.hover(x + 4, y + 20, lw - 58, 16) && !ui.typing()) ui.tip("Finds entries with every typed word in any field (id, name, type, loot...)");
    if (ui.button(x + lw - 52, y + 20, 48, 16, "Filter", true, !g.filterKey.empty())) openFieldFilter(g);
    if (ui.hover(x + lw - 52, y + 20, 48, 16)) ui.tip("Show only entries with a chosen value in a field (type = weapon, faction = undead...)");
    int fy0 = 0;
    if (!g.filterKey.empty()) {
        fy0 = 18; std::string v = g.filterVal == "\x01has" ? "(any value)" : g.filterVal == "\x01none" ? "(missing / empty)" : g.filterVal;
        ui.cv->fillRect(x + 4, y + 38, lw - 8, 16, PANEL2); ui.cv->setClip({x + 4, y + 38, lw - 30, 16}); ui.text(x + 8, y + 40, g.filterKey + " = " + v, ACCENT); ui.cv->resetClip();
        if (ui.button(x + lw - 22, y + 38, 18, 16, "x")) { g.filterKey.clear(); g.filterVal.clear(); g.scroll = 0; }
    }
    for (auto& sc : g.secs) if (sc.inherited && sc.kv != sc.origKv) { sc.inherited = false; sc.origKv.clear(); g.dirty = true; say("Copied '" + sc.get(g.listKey) + "' into the campaign - Save to keep it."); }
    bool objTab = &g == &gens[15];   // 1.1.1: Objects tab lists [type] entries ("every <type>") and templates
    for (int i : idx) { std::string l = g.secs[i].get(g.listKey); if (l.empty()) l = g.secs[i].get("name"); if (l.empty()) l = "(" + g.secs[i].name + ")"; if (g.secs[i].inherited) l += "  (base)";
        if (objTab) l = g.secs[i].name == "type" ? "every " + l : "template: " + l + (g.secs[i].get("type").empty() ? "" : "  (" + g.secs[i].get("type") + ")");
        else if (!g.altSection.empty()) l = (g.secs[i].name == g.altSection ? "  notice: " : "board: ") + l;
        if (&g == &gens[5] && encounterUnused(g.secs[i].get("id"))) l += "  (unused)";
        if (&g == &gens[17]) { std::string nm = g.secs[i].get("name"); if (!nm.empty() && nm != l) l = nm + "  (" + l + ")"; if (!trim(g.secs[i].get("companion")).empty()) l += "  - companion"; }   // 1.1.9: NPCs by name
        labels.push_back(l); }
    int lsel = -1; for (size_t i = 0; i < idx.size(); i++) if (idx[i] == g.sel) lsel = (int)i;
    int hit = ui.listBox(x + 4, y + 40 + fy0, lw - 8, h - (g.altSection.empty() ? 100 : 124) - fy0, labels, lsel, g.scroll);
    if (g.secs.empty()) ui.text(x + 10, y + 44 + fy0, "Empty - New, or Import.", DIM);
    else if (idx.empty()) ui.text(x + 10, y + 44 + fy0, "Nothing matches.", DIM);
    if (hit >= 0) { g.sel = idx[hit]; g.fieldScroll = 0; ui.blur(); }
    int by = y + h - 56;
    if (objTab) {   // a [type] entry: pick an object type that has none yet; it starts with every look / use key, empty = built-in
        if (ui.button(x + 4, by - 24, 222, 20, "New object type entry [type]")) {
            std::vector<std::string> opts; for (auto& t : edObjTypes()) { bool have = false; for (auto& sc : g.secs) if (sc.name == "type" && sc.get("id") == t) have = true; if (!have) opts.push_back(t); }
            if (opts.empty()) say("Every object type already has an entry.");
            else openListPicker("Object type: the entry sets what every object of it uses", opts, 0, [this, &g, opts](int i) { EdSection s; s.name = "type"; s.kv.push_back({"id", opts[i]}); for (auto& k : edObjLookKeys(opts[i])) s.kv.push_back({k, ""});
                g.secs.push_back(s); g.sel = (int)g.secs.size() - 1; g.dirty = true; g.filter.clear(); });
        }
        if (ui.hover(x + 4, by - 24, 222, 20)) ui.tip("What EVERY object of a type uses (save points, chests, signs...) unless the object or its template sets the key itself");
    }
    else if (!g.altSection.empty() && ui.button(x + 4, by - 24, 222, 20, "New hand-written notice [" + g.altSection + "]")) { EdSection s; s.name = g.altSection; for (auto& k : g.altTemplateKeys) s.kv.push_back({k, k == g.listKey ? g.altSection + "_" + std::to_string(g.secs.size()) : ""}); g.secs.push_back(s); g.sel = (int)g.secs.size() - 1; g.dirty = true; g.filter.clear(); }
    if (ui.button(x + 4, by, 52, 20, "New")) { EdSection s; s.name = g.sectionName; for (auto& k : g.templateKeys) s.kv.push_back({k, k == g.listKey ? "new_" + std::to_string(g.secs.size()) : ""}); g.secs.push_back(s); g.sel = (int)g.secs.size() - 1; g.dirty = true; g.filter.clear(); }
    if (ui.button(x + 60, by, 70, 20, "Duplicate", g.sel >= 0)) { EdSection s = g.secs[g.sel]; s.inherited = false; s.origKv.clear(); s.set(g.listKey, s.get(g.listKey) + "_copy"); g.secs.insert(g.secs.begin() + g.sel + 1, s); g.sel++; g.dirty = true; }
    bool selInherited = g.sel >= 0 && g.sel < (int)g.secs.size() && g.secs[g.sel].inherited && !g.replaceWhole;
    if (ui.button(x + 190, by, 36, 20, "Undo", !g.undo.empty())) genStep(g, true);
    if (ui.hover(x + 190, by, 36, 20)) ui.tip("Undo the last change in this tab (Ctrl+Z; Ctrl+Y redoes)");
    if (ui.button(x + 134, by, 52, 20, "Delete", g.sel >= 0 && !selInherited, false, DANGER)) { openConfirm("Delete '" + labels[std::max(0, lsel)] + "'?", [&g]() { g.secs.erase(g.secs.begin() + g.sel); g.sel = std::min(g.sel, (int)g.secs.size() - 1); g.dirty = true; }); }
    if (ui.button(x + 4, by + 26, 70, 20, "Save", g.dirty, false, OK)) { if (g.save()) { say("Saved " + g.path); changed = true; } }
    if (ui.button(x + 80, by + 26, 70, 20, "Revert", g.dirty)) { g.load(); say("Reverted."); }
    if (&g == &gens[7] && g.sel >= 0 && ui.button(x + 156, by + 26, 70, 20, "Bake map", true, false, ACCENT2)) { EdSection sec = g.secs[g.sel]; std::string id = sec.get("id"); openConfirm("Generate dungeon '" + id + "' with the current seed and save it as maps/" + id + ".txt? The map file then replaces the generated dungeon in the game and can be painted like any other map. (If the file already exists it is overwritten.)", [this, sec]() { std::string err; if (bakeDungeon(sec, err)) { tab = 0; say("Baked " + sec.get("id") + " - it is now open in the Maps tab."); } else say("Bake failed: " + err); }); }
    else if (!g.status.empty()) ui.text(x + 156, by + 30, g.status, DIM);
    if (in.ctrl() && in.pressed[K_S] && g.dirty && pop == P_NONE) { if (g.save()) { say("Saved " + g.path); changed = true; } }
    // fields
    int fx = x + lw + 6, fw = w - lw - 6;
    ui.panel(fx, y, fw, h);
    if (g.sel < 0 || g.sel >= (int)g.secs.size()) { ui.text(fx + 10, y + 10, "Select an entry on the left, or press New.", DIM); if (&g == &gens[17]) drawNpcInfo(fx + 4, y + 30, fw - 8, std::min(260, h - 40), in); return; }
    EdSection& s = g.secs[g.sel];
    ui.header(fx, y, fw, (s.name == "type" && objTab ? "[type]  every " + s.get("id") + " on every map   (empty = built-in)" : "[" + s.name + "]  " + s.get(g.listKey) + "   " + s.get("name")) + (s.inherited ? "   - from the base game; any edit makes it the campaign's own" : ""));
    helpBtn(fx + fw - 58, y + 1, s.name, tabTopic());
    { Tile pv; if (s.has("sprite")) pv = parseTile(s.get("sprite")); else if (s.has("tile")) pv = parseTile(s.get("tile")); else if (s.has("spriterow")) pv = T(s.has("spritecol") ? atoi(s.get("spritecol").c_str()) : 104, atoi(s.get("spriterow").c_str()));
      int psz = s.get("size") == "2" ? 2 : 1;
      if (pv.valid()) { ui.cv->fillRect(fx + fw - 40, y + 1, 36, 14, PANEL2); for (int b = 0; b < psz; b++) for (int a = 0; a < psz; a++) { Tile t = tileAt(pv, a, b); if (!trim(s.get("color")).empty()) ts->drawRecolor(*ui.cv, t, fx + fw - 28 + a * 12, y + 2 + b * 12, 1, parseColor(s.get("color"))); else ts->draw(*ui.cv, t, fx + fw - 28 + a * 12, y + 2 + b * 12, 1); } } }
    // items, creatures, spells, shops: a "Used by" box under the fields
    std::string useKey = &g == &gens[0] ? "item" : &g == &gens[1] ? "creature" : &g == &gens[2] ? "spell" : &g == &gens[4] ? "shop" : "";
    bool npcTab = &g == &gens[17] && s.name == "npc";   // 1.1.9 (#33): NPCs get the Look card and a box with where they stand
    int usedH = npcTab ? 196 : useKey.empty() ? 0 : 130;
    int fieldsBottom = y + h - 32 - usedH;
    bool itemLook = &g == &gens[0] && s.name == "item";   // 1.1.8 (#32): items get a Look card (sprite + colour) above their fields
    int lookH = itemLook ? drawItemLook(g, s, fx, y + 17, fw) : npcTab ? drawLookCard(g, s, fx, y + 17, fw, true) : 0;
    int rowH = 22; int fy = y + 22 + lookH - g.fieldScroll;
    if (ui.hover(fx, y + 20 + lookH, fw, fieldsBottom - y - 20 - lookH) && in.wheel) g.fieldScroll = std::max(0, g.fieldScroll - in.wheel * 22);
    ui.cv->setClip({fx + 1, y + 18 + lookH, fw - 2, fieldsBottom - y - 18 - lookH});
    int keyW = 110;
    bool quests = &g == &gens[6];
    int row = 0;
    for (size_t i = 0; i < s.kv.size(); i++) {
        auto& kv = s.kv[i];
        if (quests && (kv.first == "stage" || kv.first == "fail" || (kv.first == "objective" && atoi(trim(split(kv.second, '|')[0]).c_str()) != 0))) { ui.idCounter += 3; continue; }   // drawn as stage cards below
        if ((itemLook && (kv.first == "tile" || kv.first == "color")) || (npcTab && (kv.first == "sprite" || kv.first == "color"))) { ui.idCounter += 3; continue; }   // set in the Look card
        int ry = fy + row * rowH; row++;
        if (ry + 16 > fieldsBottom || ry < y + 18 + lookH) { ui.idCounter += 3; continue; }
        ui.label(fx + 8, ry + 2, kv.first);
        if (s.name == "template") helpBtn(fx + keyW - 17, ry + 1, "obj." + s.get("type") + "." + kv.first, kv.first == "type" || kv.first == "id" ? "tab.objects" : "obj." + s.get("type"));
        else if (s.name == "type" && objTab) helpBtn(fx + keyW - 17, ry + 1, kv.first == "id" ? "objtype" : "obj." + s.get("id") + "." + kv.first, "objtype");
        else if (npcTab) helpBtn(fx + keyW - 17, ry + 1, "obj.npc." + kv.first, "tab.npcs");
        else helpBtn(fx + keyW - 17, ry + 1, s.name + "." + kv.first, s.name);
        fieldWidgets(g, s, kv, fx + keyW, ry, fw - keyW - 30);
        if (ui.button(fx + fw - 24, ry, 18, 16, "x", true, false, DANGER)) { s.kv.erase(s.kv.begin() + i); g.dirty = true; ui.blur(); break; }
    }
    if (!quests) g.fieldScroll = std::min(g.fieldScroll, std::max(0, row * rowH - (fieldsBottom - y - 40 - lookH)));
    if (quests) { int used = drawQuestStages(g, s, fx, fy + row * rowH + 4, fw, y + 18, y + h - 32, in); if (ui.hover(fx, y + 20, fw, h - 50) && in.wheel) {} g.fieldScroll = std::min(g.fieldScroll, std::max(0, row * rowH + used - (h - 80))); }
    ui.cv->resetClip();
    if (npcTab) drawNpcInfo(fx + 4, fieldsBottom + 2, fw - 8, usedH - 6, in);
    else if (usedH) drawUsedBy(useKey + ":" + s.get("id"), fx + 4, fieldsBottom + 2, fw - 8, usedH - 6, in);
    // add field row
    int ay = y + h - 26;
    ui.cv->fillRect(fx + 1, ay - 4, fw - 2, 30, PANEL);
    ui.label(fx + 8, ay + 2, "add key:"); ui.textField(fx + 70, ay, 140, g.newKey, 40);
    if (ui.button(fx + 214, ay, 40, 16, "Add", !g.newKey.empty())) { s.kv.push_back({trim(g.newKey), ""}); g.newKey.clear(); g.dirty = true; }
    // template keys quick-add
    int qx = fx + 262;
    std::vector<std::string> quick = g.templateKeys;   // templates: the keys their object type reads
    if (s.name == "template") for (auto& k : edObjKeys(s.get("type"))) if (std::find(quick.begin(), quick.end(), k) == quick.end()) quick.push_back(k);
    if (s.name == "type" && objTab) { quick.clear(); for (auto& k : edObjKeys(s.get("id"))) quick.push_back(k); }
    if (npcTab) { quick = edNpcKeys(); if (trim(s.get("companion")).empty()) quick.erase(std::remove_if(quick.begin(), quick.end(), [](const std::string& k) { return k == "partysprite" || k == "level" || k == "stats" || k == "gear" || k == "spells"; }), quick.end()); }
    for (auto& k : quick) { if (s.has(k) || (itemLook && (k == "tile" || k == "color")) || (npcTab && (k == "sprite" || k == "color"))) continue; int bw = font->width(k) + 8; if (qx + bw > fx + fw - 4) break; if (ui.button(qx, ay, bw, 16, k)) { s.kv.push_back({k, ""}); g.dirty = true; } qx += bw + 3; }
}


// ---------------- structured field widgets ----------------
static const char* STAT_KEYS[6] = {"str", "dex", "con", "int", "wis", "cha"};
static const char* DMG_KEYS[6] = {"physical", "fire", "cold", "shock", "poison", "magic"};
static std::map<std::string, int> parsePairs(const std::string& v) { std::map<std::string, int> m; for (auto& part : split(v, ',')) { auto kv = split(trim(part), ':'); if (kv.size() == 2) m[toLower(trim(kv[0]))] = atoi(trim(kv[1]).c_str()); } return m; }
static std::string joinPairs(const char* const* keys, const std::map<std::string, int>& m, bool all) { std::string o; for (int i = 0; i < 6; i++) { auto it = m.find(keys[i]); int v = it == m.end() ? 0 : it->second; if (!all && v == 0) continue; if (!o.empty()) o += ","; o += std::string(keys[i]) + ":" + std::to_string(v); } return o; }
static const std::map<std::string, std::vector<std::string>>& choiceTable() {
    static std::map<std::string, std::vector<std::string>> t = {
        {"type", {"weapon", "armor", "helm", "boots", "accessory", "offhand", "consumable", "key", "material", "ammo"}},
        {"dmgtype", {"physical", "fire", "cold", "shock", "poison", "magic"}},
        {"stat", {"str", "dex", "con", "int", "wis", "cha"}},
        {"ai", {"melee", "ranged", "caster", "support", "coward", "boss", "rift"}},
        {"role", {"", "guard", "sniper", "support", "leader"}},             // 1.4.0 (#63)
        {"trait", {"", "reach", "cleave", "backstab", "crush", "arc", "steady", "quick", "spread"}},   // 1.4.0 (#68)
        {"shape", {"", "ball", "line", "cone"}},                            // 1.4.0 (#64)
        {"family", {"", "blade", "dagger", "mace", "axe", "spear", "staff", "wand", "bow", "crossbow", "gun"}},   // 1.5.0 (#93)
        {"weight", {"", "light", "medium", "heavy"}},
        {"wears", {"light", "medium", "heavy"}},
        {"ground", {"", "fire", "ice"}},
        {"target", {"enemy", "ally", "self", "any", "ground"}},
        {"effect", {"", "burn", "freeze", "shock", "poison", "regen", "haste", "shield", "weaken", "stun", "aim", "strength", "focus", "blind", "cure", "revive", "marked", "tagged", "override", "slow", "smoke"}},
        {"utility", {"", "light", "stealth", "detect", "unlock", "recall", "freeze", "shock", "emp", "force"}},
        {"faction", {"monster", "bandit", "machine", "wild", "undead", "rift"}},
        {"style", {"cave", "crypt", "castle", "tech", "ship", "mine", "sewer"}},
        {"loot", {"common", "tech", "rare"}},
        {"onhit", {"", "poison", "weaken", "burn", "freeze", "stun"}},
    };
    return t;
}
void Editor::fieldWidgets(GenericEditor& g, EdSection& s, std::pair<std::string, std::string>& kv, int x, int y, int w) {
    const std::string& k = kv.first; std::string& v = kv.second;
    auto& kvr = kv;
    // --- 1.1.9 (#33): NPCs (npcs.txt) ---
    if (s.name == "npc" && &g == &gens[17]) {
        auto npcSingle = [&](const std::string& title, std::vector<std::string> opts) { if (ui.textField(x, y, w - 60, v, 80)) g.dirty = true;
            if (ui.button(x + w - 56, y, 52, 16, "pick...")) { int cur = 0; for (size_t i = 0; i < opts.size(); i++) if (opts[i] == v) cur = (int)i; openListPicker(title, opts, cur, [&kvr, opts, &g](int i) { kvr.second = opts[i]; g.dirty = true; }); } };
        if (k == "id") { ui.text(x, y + 2, v, ACCENT2); ui.text(x + font->width(v) + 10, y + 2, "(Rename id... below changes it everywhere)", DIM); return; }
        if (k == "dialogue") { std::vector<std::string> o{""}; for (auto& d : dlgFiles) o.push_back(d); npcSingle("dialogue (what talking to it opens; [camp] = a companion's party talk)", o); return; }
        if (k == "script") { std::vector<std::string> o{""}; for (auto& d : scriptFiles) o.push_back(d); npcSingle("script talking runs (Scripts tab; wins over dialogue, board, travel)", o);
            if (!trim(v).empty() && ui.hover(x, y, w - 60, 16)) ui.tip("Talking runs scripts/" + trim(v) + ".txt (open it in the Scripts tab)"); return; }
        if (k == "shop") { std::vector<std::string> o{""}; for (auto& sh : gens[4].secs) o.push_back(sh.get("id")); npcSingle("shop (opened when it has no dialogue)", o); return; }
        if (k == "board") { std::vector<std::string> o{""}; for (auto& b : boardIds()) o.push_back(b); npcSingle("notice board of which town", o); return; }
        if (k == "companion") { std::vector<std::string> o{""}; for (auto& c : gd->classes) o.push_back(c.first); npcSingle("class it joins the party as (empty = not a companion)", o); return; }
        if (k == "craft") { if (ui.choice(x, y, 140, v.empty() ? "(none)" : v)) { std::vector<std::string> opts{"", "forge", "workbench", "alchemy", "any"}; int cur = 0; for (size_t i = 0; i < opts.size(); i++) if (opts[i] == v) cur = (int)i; openListPicker("crafting station it offers (only without a dialogue)", opts, cur, [&kvr, opts, &g](int i) { kvr.second = opts[i]; g.dirty = true; }); } return; }
        if (k == "travel") { bool b = v == "1"; if (ui.checkbox(x, y + 2, b ? "talking opens coach travel (built in; the coachman now uses script=coachman)" : "no coach travel", b)) { v = b ? "1" : ""; g.dirty = true; } return; }
        if (k == "wander") { bool b = v == "1"; if (ui.checkbox(x, y + 2, b ? "wanders near its spot" : "stands still", b)) { v = b ? "1" : ""; g.dirty = true; } return; }
        if (k == "hostile") { if (ui.textField(x, y, w - 60, v, 200)) g.dirty = true; if (ui.button(x + w - 56, y, 52, 16, "+ add")) { std::vector<std::string> opts; for (auto& c : gd->creatures) opts.push_back(c.first); openListPicker("creature it turns into when talked to / attacked", opts, 0, [&kvr, opts, &g](int i) { kvr.second = trim(kvr.second).empty() ? opts[i] : kvr.second + "," + opts[i]; g.dirty = true; }); } return; }
        if (k == "gear") { if (ui.textField(x, y, w - 60, v, 300)) g.dirty = true; if (ui.hover(x, y, w - 60, 16)) ui.tip("Items they arrive with: weapons and armour are worn (in place of the class's), the rest go to the pack (id:count)");
            if (ui.button(x + w - 56, y, 52, 16, "+ add")) { std::vector<std::string> opts; for (auto& it : gd->items) opts.push_back(it.first); openListPicker("item they join with", opts, 0, [&kvr, opts, &g](int i) { kvr.second = trim(kvr.second).empty() ? opts[i] : kvr.second + "," + opts[i]; g.dirty = true; }); } return; }
        if (k == "level") { int lv = atoi(v.c_str()); if (ui.intField(x, y, 70, lv, 0, 30)) { v = lv > 0 ? std::to_string(lv) : ""; g.dirty = true; } ui.text(x + 78, y + 2, lv > 0 ? "arrives at this level" : "0 / empty = the hero's level when they join", DIM); return; }
        if (k == "stats" && trim(v).empty()) { ui.text(x, y + 2, "(the class's stats)", DIM);
            if (ui.button(x + 150, y, 120, 16, "set own stats")) { const ClassDef* c = gd->cls(trim(s.get("companion"))); static const char* SK[6] = {"str", "dex", "con", "int", "wis", "cha"}; std::string o; for (int i = 0; i < 6; i++) o += std::string(i ? "," : "") + SK[i] + ":" + std::to_string(c ? c->stats[i] : 10); v = o; g.dirty = true; }
            return; }
        if (k == "notes") { if (ui.textField(x, y, w, v, 400)) g.dirty = true; if (ui.hover(x, y, w, 16)) ui.tip("For you: who they are, where they fit in the story. The game doesn't show it."); return; }
    }
    // --- six-stat rows ---
    if (k == "stats" || k == "bonus" || k == "req") {
        auto m = parsePairs(v); bool ch = false;
        for (int i = 0; i < 6; i++) { int val = m.count(STAT_KEYS[i]) ? m[STAT_KEYS[i]] : 0; ui.text(x + i * 84, y + 2, toUpper(STAT_KEYS[i]), DIM); if (ui.intField(x + 26 + i * 84, y, 54, val, -20, 99)) { m[STAT_KEYS[i]] = val; ch = true; } }
        if (ch) { v = joinPairs(STAT_KEYS, m, k == "stats"); g.dirty = true; }
        return;
    }
    if (k == "res") {
        auto m = parsePairs(v); bool ch = false;
        for (int i = 0; i < 6; i++) { int val = m.count(DMG_KEYS[i]) ? m[DMG_KEYS[i]] : 0; std::string lbl = std::string(DMG_KEYS[i]).substr(0, 3); ui.text(x + i * 86, y + 2, lbl, DIM); if (ui.intField(x + 24 + i * 86, y, 58, val, -100, 100)) { m[DMG_KEYS[i]] = val; ch = true; } }
        // 1.5.0 (#92): weapon-trait keys (crush:-100) have no box; they are kept and listed after the boxes
        std::string tr; for (auto& kv : m) if (traitIndex(kv.first) >= 0) tr += (tr.empty() ? "" : ",") + kv.first + ":" + std::to_string(kv.second);
        if (ch) { v = joinPairs(DMG_KEYS, m, false); if (!tr.empty()) v += (v.empty() ? "" : ",") + tr; g.dirty = true; }
        ui.text(x + 6 * 86, y + 2, tr.empty() ? "% 50/100/-100" : "+ " + tr, DIM);
        if (ui.hover(x, y, 6 * 86 + 120, 16)) ui.tip("50 resists (half damage), 100 immune, -100 weak (double). Weapon-trait keys like crush:-100 are typed in the file.");
        return;
    }
    // --- lo-hi ranges ---
    if (k == "dmg" || k == "gold" || (k == "count" && (s.name == "resource" || s.name == "template"))) {
        int lo = 0, hi = 0; parseRange(v, lo, hi); bool ch = false;
        if (ui.intField(x, y, 70, lo, 0, 9999)) ch = true; ui.text(x + 76, y + 2, "to", DIM); if (ui.intField(x + 94, y, 70, hi, 0, 9999)) ch = true;
        if (ch) { if (hi < lo) hi = lo; v = (k == "gold" && s.name == "template") ? std::to_string(lo) : fmt("%d-%d", lo, hi); g.dirty = true; }
        return;
    }
    // --- colours ---
    if (isColorKey(k)) {
        Color8 c = v.empty() ? C::White : parseColor(v); int r = c.r, gg = c.g, b = c.b; bool ch = false;
        int cw = clampi((w - 34) / 3 - 6, 46, 70), sw = clampi(w - 3 * (cw + 6) - 4, 20, 40);   // narrower in the side-by-side tabs
        if (ui.intField(x, y, cw, r, 0, 255)) ch = true; if (ui.intField(x + cw + 6, y, cw, gg, 0, 255)) ch = true; if (ui.intField(x + 2 * (cw + 6), y, cw, b, 0, 255)) ch = true;
        ui.cv->fillRect(x + 3 * (cw + 6) + 2, y, sw, 16, RGB((uint8_t)r, (uint8_t)gg, (uint8_t)b)); ui.cv->rect(x + 3 * (cw + 6) + 2, y, sw, 16, BORDER);
        if (ch) { v = fmt("%d,%d,%d", r, gg, b); g.dirty = true; }
        return;
    }
    // --- tiles / sprites ---
    if (isTileKey(k) || k == "spriterow" || k == "spritecol") {
        Tile t = isTileKey(k) ? parseTile(v) : T(s.has("spritecol") ? atoi(s.get("spritecol").c_str()) : 104, atoi(s.get("spriterow").c_str()));
        if (ui.textField(x, y, 90, v, 20)) g.dirty = true;
        { int psz = (k == "sprite" && s.get("size") == "2") ? 2 : 1;
          if (psz == 2) { ui.cv->fillRect(x + 98, y - 6, 52, 52, FIELD); ui.cv->rect(x + 98, y - 6, 52, 52, ACCENT2); }
          for (int b = 0; b < psz; b++) for (int a = 0; a < psz; a++) { Tile tt = tileAt(t, a, b); if (!trim(s.get("color")).empty() && k != "partysprite") ts->drawRecolor(*ui.cv, tt, x + 100 + a * 24, y - 4 + b * 24, 2, parseColor(s.get("color"))); else ts->draw(*ui.cv, tt, x + 100 + a * 24, y - 4 + b * 24, 2); }
          if (psz == 2) ui.text(x + 154, y + 26, "size=2: the sprite is the 2x2 block starting at this tile (top-left)", DIM); }
        if (ui.button(x + 130, y, 50, 16, "pick...")) { EdSection* sp = &s; std::string key = k; openTilePicker(t, [sp, key, &g](Tile nt) { if (!nt.valid()) return; if (key == "spriterow" || key == "spritecol") { for (size_t i = 0; i < sp->kv.size(); i++) if (sp->kv[i].first == "spriterow" || sp->kv[i].first == "spritecol") { sp->kv.erase(sp->kv.begin() + i); i--; } sp->set("sprite", tileText(nt)); } else sp->set(key, tileText(nt)); g.dirty = true; }, tileCatFor(s.name, k)); }
        if (k == "sprite" && s.name == "creature") { ui.text(x + 190, y + 2, "character rows: col 104 + row = race; col 105 sword, 106 axe, 109 staff, 113 shield, 116 gun, 117 bow", DIM); }
        return;
    }
    // 1.1.10: an item's effect can be script:<id> (or script:<id>:keep), so it is typed, with a list to pick from
    if (k == "effect" && s.name == "item") {
        if (ui.textField(x, y, w - 60, v, 80)) g.dirty = true;
        if (ui.hover(x, y, w - 60, 16)) ui.tip("What using it does. script:<id> runs a script (the item is used up); script:<id>:keep runs it and keeps the item. Only consumables can be used.");
        if (ui.button(x + w - 56, y, 52, 16, "pick...")) { std::vector<std::string> opts = choiceTable().at("effect"); for (const char* e : {"torch", "dig", "blast", "flare", "respec", "frost", "power", "lure", "lure:12:7"}) opts.push_back(e); for (auto& sf : scriptFiles) { opts.push_back("script:" + sf); opts.push_back("script:" + sf + ":keep"); }
            int cur = 0; for (size_t i = 0; i < opts.size(); i++) if (opts[i] == v) cur = (int)i; openListPicker("effect", opts, cur, [&kvr, opts, &g](int i) { kvr.second = opts[i]; g.dirty = true; }); }
        return;
    }
    // --- fixed choices ---
    auto& ct = choiceTable();
    auto cit = ct.find(k);
    if (cit != ct.end() && !(k == "loot" && s.name == "creature") && !(k == "type" && s.name != "item")) {
        if (ui.choice(x, y, 140, v.empty() ? "(none)" : v)) { auto opts = cit->second; int cur = 0; for (size_t i = 0; i < opts.size(); i++) if (opts[i] == v) cur = (int)i; openListPicker(k, opts, cur, [&kvr, opts, &g](int i) { kvr.second = opts[i]; g.dirty = true; }); }
        return;
    }
    if (k == "kind" && s.name == "location") {
        if (ui.choice(x, y, 140, v)) { std::vector<std::string> opts; for (auto& kk : allTileKinds()) if (startsWith(kk.name, "loc_")) opts.push_back(kk.name); int cur = 0; for (size_t i = 0; i < opts.size(); i++) if (opts[i] == v) cur = (int)i; openListPicker("marker kind", opts, cur, [&kvr, opts, &g](int i) { kvr.second = opts[i]; g.dirty = true; }); }
        return;
    }
    // --- id pickers (single value) ---
    auto singlePick = [&](const std::string& title, std::vector<std::string> opts) {
        if (ui.textField(x, y, w - 60, v, 60)) g.dirty = true;
        if (ui.button(x + w - 56, y, 52, 16, "pick...")) { int cur = 0; for (size_t i = 0; i < opts.size(); i++) if (opts[i] == v) cur = (int)i; openListPicker(title, opts, cur, [&kvr, opts, &g](int i) { kvr.second = opts[i]; g.dirty = true; }); }
    };
    auto itemIds = [&]() { std::vector<std::string> o; o.push_back(""); for (auto& it : gd->items) o.push_back(it.first); return o; };
    auto creatureIds = [&]() { std::vector<std::string> o; for (auto& it : gd->creatures) o.push_back(it.first); return o; };
    auto spellIds = [&]() { std::vector<std::string> o; o.push_back(""); for (auto& it : gd->spells) o.push_back(it.first); return o; };
    auto mapIds = [&]() { std::vector<std::string> o{"overworld"}; for (auto& m : mapFiles) o.push_back(m); for (auto& d : gens[7].secs) o.push_back(d.get("id")); return o; };
    // --- object templates (objects.txt) and resource nodes (resources.txt), 1.1.0 ---
    if (s.name == "template" || s.name == "resource" || (s.name == "type" && &g == &gens[15])) {
        if (k == "id" && s.name == "type") { ui.text(x, y + 2, v, ACCENT2); ui.text(x + font->width(v) + 10, y + 2, "(the object type; make another with New object type entry)", DIM); return; }
        if (k == "onuse") {
            if (ui.textField(x, y, w - 60, v, 300)) g.dirty = true;
            if (ui.hover(x, y, w - 60, 16)) ui.tip("Script run when it is used: actions separated by ; e.g. heal;message:Rested|You feel better. (as a trigger's script=)");
            if (ui.button(x + w - 56, y, 52, 16, "+ action")) { std::vector<std::string> acts, labels; actionChoices(acts, labels);   // 1.4.0: scripts with their args
                openListPicker("Action (added with ;)", labels, 0, [&kvr, &g, acts](int i) { kvr.second = trim(kvr.second).empty() ? acts[i] : trim(kvr.second) + ";" + acts[i]; g.dirty = true; }); }
            return;
        }
        if (k == "needs") {   // 1.4.0 (#75): an obstacle template's ways to clear it
            if (ui.textField(x, y, w - 60, v, 200)) g.dirty = true;
            if (ui.button(x + w - 56, y, 52, 16, "+ add")) { std::vector<std::string> ids = obstacleNeedChoices(), labels; for (auto& n : ids) labels.push_back(obstacleNeedLabel(n));
                openListPicker("What clears it (added with |)", labels, 0, [&kvr, &g, ids](int i) { std::string cur = trim(kvr.second); kvr.second = cur.empty() || cur == "rubble" ? ids[i] : cur + "|" + ids[i]; g.dirty = true; }); }
            return;
        }
        if (k == "sparkle") { bool b = v != "0"; if (ui.checkbox(x, y + 2, "twinkle above it", b)) { v = b ? "1" : "0"; g.dirty = true; } return; }
        if ((k == "color" || k == "sparklecolor") && trim(v).empty()) {   // empty = its own colours: say so instead of showing 255,255,255
            ui.text(x, y + 2, k == "color" ? "(its own colours)" : "(built-in)", DIM);
            if (ui.button(x + 130, y, 70, 16, "set colour")) { v = k == "color" ? "255,255,255" : "0,220,220"; g.dirty = true; }
            return;
        }
        if (k == "sound") { std::vector<std::string> o{""}; for (auto& id : gAudio.sfxChoices()) o.push_back(id); singlePick("sound event (empty = the built-in one)", o); return; }
        if (k == "type" && s.name == "template") { if (ui.choice(x, y, 140, v.empty() ? "(pick a type)" : v)) { auto opts = edObjTypes(); int cur = 0; for (size_t i = 0; i < opts.size(); i++) if (opts[i] == v) cur = (int)i; openListPicker("object type", opts, cur, [&kvr, opts, &g](int i) { kvr.second = opts[i]; g.dirty = true; }); } return; }
        if (k == "item" || k == "tool" || k == "key") { singlePick("item", itemIds()); return; }
        if (k == "dialogue") { std::vector<std::string> o{""}; for (auto& d : dlgFiles) o.push_back(d); singlePick("dialogue", o); return; }
        if (k == "shop") { std::vector<std::string> o{""}; for (auto& sh : gens[4].secs) o.push_back(sh.get("id")); singlePick("shop", o); return; }
        if (k == "node") { std::vector<std::string> o; for (auto& rs : gens[16].secs) o.push_back(rs.get("id")); singlePick("resource node type", o); return; }
        if (k == "kind" || k == "clear") { std::vector<std::string> o{""}; for (auto& kk : allTileKinds()) o.push_back(kk.name); singlePick("tile kind", o); return; }
        if (k == "styles") {
            if (ui.textField(x, y, w - 60, v, 200)) g.dirty = true;
            if (ui.hover(x, y, w - 60, 16)) ui.tip("style[:weight[:count]], e.g. mine:2:2-4,cave - weight = lots in the draw, count overrides count= there");
            if (ui.button(x + w - 56, y, 52, 16, "+ add")) { std::vector<std::string> opts{"mine", "cave", "crypt", "castle", "tech", "ship", "sewer"}; openListPicker("generated dungeon style that scatters this node", opts, 0, [&kvr, opts, &g](int i) { kvr.second = trim(kvr.second).empty() ? opts[i] : trim(kvr.second) + "," + opts[i]; g.dirty = true; }); }
            return;
        }
        if (k == "flag") { if (ui.textField(x, y, w - 60, v, 80)) g.dirty = true; flagPicker(x + w - 56, y, 52, false, [&kvr, &g](const std::string& f) { kvr.second = f; g.dirty = true; }); return; }
        if (k == "show" || k == "hide" || k == "require" || k == "openif" || k == "moveif" || k == "if") {
            if (ui.textField(x, y, w - 60, v, 200)) g.dirty = true;
            if (ui.hover(x, y, w - 60, 16) && !v.empty()) ui.tip("Reads: " + condEnglish(v));
            flagPicker(x + w - 56, y, 52, true, [&kvr, &g](const std::string& f) { kvr.second = trim(kvr.second).empty() ? f : trim(kvr.second) + "&" + f; g.dirty = true; });
            return;
        }
    }
    if (k == "weapon" || k == "armor" || k == "ammo" || (k == "item" && s.name == "shop") || k == "locked" || k == "key") { singlePick("item", itemIds()); return; }
    // 1.3.2: a creature's boss=1 is a tick box (dungeons' boss= stays a creature picker)
    if (k == "boss" && s.name == "creature") { bool b = atoi(v.c_str()) != 0; if (ui.checkbox(x, y + 2, b ? "a boss: its music plays in its fights, stun fails, fleeing is harder" : "not a boss", b)) { v = b ? "1" : "0"; g.dirty = true; } return; }
    if (k == "boss") { singlePick("creature", creatureIds()); return; }
    if (k == "summon" && s.name == "spell") { std::vector<std::string> o2{""}; for (auto& c : creatureIds()) o2.push_back(c); singlePick("creature", o2); return; }
    if (k == "faction" && s.name == "shop") { std::vector<std::string> o{""}; for (auto& f : gens[14].secs) o.push_back(f.get("id")); for (auto& b : boardIds()) if (std::find(o.begin(), o.end(), b) == o.end()) o.push_back(b); singlePick("faction", o); return; }
    if (k == "result" && s.name == "recipe") { singlePick("item", itemIds()); return; }
    if (k == "class" && s.name == "banter") { std::vector<std::string> o{"any"}; for (auto& c : gd->classes) o.push_back(c.first); singlePick("class", o); return; }
    if (k == "when" && s.name == "banter") { if (ui.choice(x, y, 140, v.empty() ? "(none)" : v)) { std::vector<std::string> opts{"enter", "idle", "victory", "night", "rain", "dungeon", "town", "overworld"}; int cur = 0; for (size_t i = 0; i < opts.size(); i++) if (opts[i] == v) cur = (int)i; openListPicker("when", opts, cur, [&kvr, opts, &g](int i) { kvr.second = opts[i]; g.dirty = true; }); } return; }
    if (k == "station") { if (ui.choice(x, y, 140, v.empty() ? "(none)" : v)) { std::vector<std::string> opts{"any", "forge", "alchemy", "workbench"}; int cur = 0; for (size_t i = 0; i < opts.size(); i++) if (opts[i] == v) cur = (int)i; openListPicker("station", opts, cur, [&kvr, opts, &g](int i) { kvr.second = opts[i]; g.dirty = true; }); } return; }
    if (k == "pos") { if (ui.choice(x, y, 140, v.empty() ? "(none)" : v)) { std::vector<std::string> opts{"prefix", "suffix"}; int cur = v == "suffix" ? 1 : 0; openListPicker("position", opts, cur, [&kvr, opts, &g](int i) { kvr.second = opts[i]; g.dirty = true; }); } return; }
    if (k == "cond" && s.name == "achievement") {
        if (ui.textField(x, y, w - 60, v, 200)) g.dirty = true;
        if (ui.button(x + w - 56, y, 52, 16, "+ term")) {
            static const std::vector<std::string> terms = {"flag:  (a flag or condition)", "kills:10", "fights:5", "fled:1", "gold:500  (earned)", "level:5", "chests:10", "maps:10  (visited)", "crafted:5", "bounties:3", "bestiary:20  (kinds seen)", "steps:1000", "day:10", "travels:5", "crits:10", "boss:CREATURE", "party:4  (members)", "quests:3  (done)", "spells:20  (cast)", "damage:1000", "knockouts:1", "notes:10", "item:ITEM_ID", "approval:CLASS>=60", "rep:FACTION>=30"};
            openListPicker("Achievement condition term (joined with &)", terms, 0, [this, &kvr, &g](int i) {
                std::string t = trim(split(terms[i], ' ')[0]);
                if (t == "flag:") { auto add = [&kvr, &g](const std::string& f) { kvr.second = trim(kvr.second).empty() ? "flag:" + f : trim(kvr.second) + "&flag:" + f; g.dirty = true; };
                    ensureIndex(); std::vector<std::string> names; for (auto& f : flagIndex) names.push_back(f.first);
                    openListPicker("Flag", names, 0, [names, add](int j) { add(names[j]); }); return; }
                kvr.second = trim(kvr.second).empty() ? t : trim(kvr.second) + "&" + t; g.dirty = true; });
        }
        return;
    }
    // --- flags and conditions: pick from the flags the campaign uses ---
    if (k == "bossflag" || (k == "flag" && s.name == "banter")) {
        if (ui.textField(x, y, w - 60, v, 80)) g.dirty = true;
        flagPicker(x + w - 56, y, 52, false, [&kvr, &g](const std::string& f) { kvr.second = f; g.dirty = true; });
        return;
    }
    if ((k == "require" && s.name == "location") || (k == "if" && s.name == "banter")) {
        if (ui.textField(x, y, w - 60, v, 200)) g.dirty = true;
        if (ui.hover(x, y, w - 60, 16) && !v.empty()) ui.tip("Reads: " + condEnglish(v));
        flagPicker(x + w - 56, y, 52, true, [&kvr, &g](const std::string& f) { kvr.second = trim(kvr.second).empty() ? f : trim(kvr.second) + "&" + f; g.dirty = true; });
        return;
    }
    if (k == "spell") { singlePick("spell", spellIds()); return; }
    if (k == "map" || k == "to") { singlePick("map", mapIds()); return; }
    if (k == "sound" || k == "attacksound") { std::vector<std::string> o{""}; for (auto& id : gAudio.sfxChoices()) o.push_back(id); singlePick("sound event (empty = automatic)", o); return; }
    if (k == "music" || k == "combatmusic") { std::vector<std::string> o{""}; for (auto& id : gAudio.order) if (gAudio.defs.count(id) && gAudio.defs[id].kind == "music") o.push_back(id); singlePick("music track", o); return; }
    if (k == "ambient") { std::vector<std::string> o{"", "none"}; for (auto& id : gAudio.order) if (gAudio.defs.count(id) && gAudio.defs[id].kind == "ambient") o.push_back(id); singlePick("background loop (empty = none)", o); return; }
    // --- list builders (comma lists) ---
    auto listPick = [&](const std::string& title, std::vector<std::string> opts, const std::string& suffix) {
        if (ui.textField(x, y, w - 60, v, 400)) g.dirty = true;
        if (ui.button(x + w - 56, y, 52, 16, "+ add")) { std::string suf = suffix; openListPicker(title, opts, 0, [&kvr, opts, suf, &g](int i) { if (opts[i].empty()) return; kvr.second = trim(kvr.second).empty() ? opts[i] + suf : kvr.second + "," + opts[i] + suf; g.dirty = true; }); }
    };
    if (k == "loot") { listPick("item (chance % is appended, edit it)", itemIds(), ":50"); return; }
    if (k == "materials") { listPick("material item (count appended)", itemIds(), ":1"); return; }
    if (k == "types" && s.name == "affix") { listPick("item type", {"weapon", "armor", "helm", "boots", "accessory", "offhand", "any"}, ""); return; }
    if (k == "items") { listPick("item (count appended)", itemIds(), ":1"); return; }
    if (k == "spells") { listPick("spell", spellIds(), ""); return; }
    if (k == "learn") {
        if (ui.textField(x, y, w - 60, v, 400)) g.dirty = true;
        if (ui.button(x + w - 56, y, 52, 16, "+ add")) { auto opts = spellIds(); openListPicker("spell learned at level 5 (edit the number)", opts, 0, [&kvr, opts, &g](int i) { if (opts[i].empty()) return; kvr.second = trim(kvr.second).empty() ? "5:" + opts[i] : kvr.second + ",5:" + opts[i]; g.dirty = true; }); }
        return;
    }
    if (k == "monsters" || k == "group") { listPick("creature", creatureIds(), ""); return; }
    if (k == "road") { std::vector<std::string> locs; for (auto& l : gens[8].secs) locs.push_back(l.get("id")); listPick("location to connect by road", locs, ""); return; }
    // --- items: quest flag and category ---
    if (s.name == "item" && k == "quest") {
        bool def = s.get("type") == "key"; bool on = v.empty() ? def : atoi(v.c_str()) != 0;
        if (ui.checkbox(x, y + 2, on ? "quest item - can't be dropped or sold" : "normal item", on)) { v = on ? "1" : "0"; g.dirty = true; }
        if (v.empty()) ui.text(x + 280, y + 2, def ? "(default for type=key)" : "(default)", DIM);
        return;
    }
    if (s.name == "dungeon" && k == "touch") {   // 1.2.3 (#40)
        bool on = atoi(v.c_str()) != 0;
        if (ui.checkbox(x, y + 2, on ? "fights start only when a monster group touches the party" : "monster groups attack when they see the party (aggro range)", on)) { v = on ? "1" : ""; g.dirty = true; }
        return;
    }
    if (s.name == "item" && k == "unique") {   // 1.1.3
        bool on = atoi(v.c_str()) != 0;
        if (ui.checkbox(x, y + 2, on ? "unique - never gets affixes (loot, crafting, enchanting)" : "can get random affixes", on)) { v = on ? "1" : ""; g.dirty = true; }
        return;
    }
    if (s.name == "item" && k == "category") {
        std::vector<std::string> opts{"", "Weapons", "Armor", "Accessories", "Consumables", "Ammo", "Tools", "Materials", "Formulas", "Trophies", "Quest"};
        for (auto& o : gens[0].secs) { std::string c = o.get("category"); if (!c.empty() && std::find(opts.begin(), opts.end(), c) == opts.end()) opts.push_back(c); }
        if (ui.textField(x, y, w - 60, v, 40)) g.dirty = true;
        if (v.empty()) { const ItemDef* d = gd->item(s.get("id")); ui.text(x + 6 + font->width(v), y + 2, d ? "(by type: " + itemCategory(*d) + ")" : "(by type)", DIM); }
        if (ui.button(x + w - 56, y, 52, 16, "pick...")) { int cur = 0; for (size_t i = 0; i < opts.size(); i++) if (opts[i] == v) cur = (int)i; openListPicker("Category (inventory and shop heading; empty = by type)", opts, cur, [&kvr, opts, &g](int i) { kvr.second = opts[i]; g.dirty = true; }); }
        return;
    }
    // --- notice boards (bounties.txt) ---
    if (s.name == "board" || s.name == "bounty") {
        auto dungeonIds = [&]() { std::vector<std::string> o; for (auto& d : gens[7].secs) o.push_back(d.get("id")); return o; };
        auto locIds = [&]() { std::vector<std::string> o; for (auto& l : gens[8].secs) o.push_back(l.get("id")); return o; };
        if (k == "kinds") { listPick("kind of notice this board posts", {"kill", "fetch", "deliver"}, ""); return; }
        if (k == "kill") { listPick("dungeon whose monsters it wants dead", dungeonIds(), ""); return; }
        if (k == "creatures") { listPick("creature", creatureIds(), ""); return; }
        if (k == "fetch") { auto o = itemIds(); o.erase(o.begin()); listPick("item it asks for", o, ""); return; }
        if (k == "deliver") { listPick("board letters may go to", boardIds(), ""); return; }
        if (k == "board") { singlePick("board", boardIds()); return; }
        if (k == "location") { singlePick("overworld location", locIds()); return; }
        if (k == "where") { auto o = dungeonIds(); for (auto& l : locIds()) o.push_back(l); singlePick("dungeon or location (hint + map pin)", o); return; }
        if (k == "kind" && s.name == "bounty") { if (ui.choice(x, y, 140, v.empty() ? "(kill)" : v)) { std::vector<std::string> opts{"kill", "fetch", "deliver"}; int cur = v == "fetch" ? 1 : v == "deliver" ? 2 : 0; openListPicker("kind", opts, cur, [&kvr, opts, &g](int i) { kvr.second = opts[i]; g.dirty = true; }); } return; }
        if (k == "target") { std::string kd = s.get("kind", "kill"); if (kd == "fetch") { auto o = itemIds(); o.erase(o.begin()); singlePick("item to bring", o); } else if (kd == "deliver") singlePick("board the letter goes to", boardIds()); else singlePick("creature to kill", creatureIds()); return; }
        if (k == "require") { if (ui.textField(x, y, w - 60, v, 200)) g.dirty = true; if (ui.hover(x, y, w - 60, 16) && !v.empty()) ui.tip("Reads: " + condEnglish(v)); flagPicker(x + w - 56, y, 52, true, [&kvr, &g](const std::string& f) { kvr.second = trim(kvr.second).empty() ? f : trim(kvr.second) + "&" + f; g.dirty = true; }); return; }
    }
    if (k == "table") { std::vector<std::string> tabs; for (auto& e : gens[5].secs) tabs.push_back(e.get("id")); singlePick("encounter table", tabs); return; }
    if (k == "exit" && s.name == "dungeon") { if (ui.textField(x, y, w - 60, v, 60)) g.dirty = true; ui.text(x + w - 56, y + 2, "map,x,y", DIM); return; }
    // --- plain text ---
    if (ui.textField(x, y, w, v, 400)) g.dirty = true;
}

// ---------------- tile kinds ----------------
void Editor::drawKinds(InputState& in) {
    auto& kinds = editableTileKinds();
    int x = 4, y = TOP + 4, h = H - TOP - BOTTOM - 8, lw = 230;
    ui.panel(x, y, lw, h); listHeader(x, y, lw, fmt("Tile kinds (%d)", (int)kinds.size()), IMP_KINDS); helpBtn(x + lw - 16, y + 1, "tab.tiles");
    ui.searchField(x + 4, y + 20, lw - 8, kindFilter, "search name, biome, water, blocked...");
    std::vector<int> idx; std::vector<std::string> labels;
    for (size_t i = 0; i < kinds.size(); i++) if (edMatch(kinds[i].name + " " + kinds[i].biome + (kinds[i].walkable ? " walk" : " blocked") + (kinds[i].water ? " water" : "") + (kinds[i].prop.valid() ? " prop" : ""), kindFilter)) { idx.push_back((int)i); labels.push_back(kinds[i].name); }
    int lsel = -1; for (size_t i = 0; i < idx.size(); i++) if (idx[i] == kindSel) lsel = (int)i;
    // draw list with previews
    int lx = x + 4, ly = y + 40, lh = h - 100, rowH = 18; int rows = lh / rowH;
    ui.cv->fillRect(lx, ly, lw - 8, lh, FIELD); ui.cv->rect(lx, ly, lw - 8, lh, BORDER);
    if (ui.hover(lx, ly, lw - 8, lh) && in.wheel) kindScroll -= in.wheel * 3;
    kindScroll = clampi(kindScroll, 0, std::max(0, (int)idx.size() - rows));
    for (int r = 0; r < rows && kindScroll + r < (int)idx.size(); r++) {
        int i = idx[kindScroll + r]; int ry = ly + 1 + r * rowH; auto& k = kinds[i];
        bool hv = ui.hover(lx, ry, lw - 8, rowH);
        if (i == kindSel) ui.cv->fillRect(lx + 1, ry, lw - 10, rowH, SEL); else if (hv) ui.cv->fillRect(lx + 1, ry, lw - 10, rowH, HOVER);
        ui.cv->fillRect(lx + 3, ry + 1, 16, 16, k.dayBg);   // 1.8.0 (#159): as the game draws it by day
        if (!k.ground.empty() && k.ground[0].valid()) { if (k.recolor) ts->drawRecolor(*ui.cv, k.ground[0], lx + 5, ry + 3, 1, k.dayTint); else if (k.opaque) ts->drawOpaque(*ui.cv, k.ground[0], lx + 5, ry + 3, 1, k.dayTint); else ts->draw(*ui.cv, k.ground[0], lx + 5, ry + 3, 1, k.dayTint); }
        if (k.prop.valid()) ts->draw(*ui.cv, k.prop, lx + 5, ry + 3, 1);
        font->draw(*ui.cv, *ts, lx + 24, ry + 3, k.name, k.walkable ? TEXT : RGB(255, 150, 150));
        if (hv && in.mousePressed[0] && !ui.mouseUsed) { ui.mouseUsed = true; kindSel = i; ui.blur(); }
    }
    int by = y + h - 56;
    if (ui.button(x + 4, by, 52, 20, "New")) { TileKind k = kinds[clampi(kindSel, 0, (int)kinds.size() - 1)]; k.name = k.name + "_new"; kinds.insert(kinds.begin() + kindSel + 1, k); kindSel++; rebuildTileKindIndex(); kindsDirty = true; }
    if (ui.button(x + 60, by, 52, 20, "Delete", kinds.size() > 1 && kinds[kindSel].name != "void", false, DANGER)) { openConfirm("Delete tile kind '" + kinds[kindSel].name + "'? Maps using it will show as void.", [this]() { auto& ks = editableTileKinds(); ks.erase(ks.begin() + kindSel); kindSel = std::min(kindSel, (int)ks.size() - 1); rebuildTileKindIndex(); kindsDirty = true; }); }
    if (ui.button(x + 4, by + 26, 70, 20, "Save", kindsDirty, false, OK)) { rebuildTileKindIndex(); if (saveTileKinds(kindsPath())) { kindsDirty = false; changed = true; say("Saved tile kinds."); } }
    if (ui.button(x + 80, by + 26, 70, 20, "Revert", kindsDirty)) { loadTileKinds(kindsPath()); kindsDirty = false; say("Reverted."); }
    if (kindReturnTab >= 0 && ui.button(x + 156, by + 26, 70, 20, "< Back", true, false, ACCENT2)) { tab = kindReturnTab; kindReturnTab = -1; }
    if (in.ctrl() && in.pressed[K_S] && kindsDirty && pop == P_NONE) { rebuildTileKindIndex(); if (saveTileKinds(kindsPath())) { kindsDirty = false; changed = true; say("Saved tile kinds."); } }
    // form
    int fx = x + lw + 6, fw = W - fx - 4;
    ui.panel(fx, y, fw, h);
    if (kindSel < 0 || kindSel >= (int)kinds.size()) return;
    TileKind& k = kinds[kindSel];
    ui.header(fx, y, fw, "Tile kind: " + k.name);
    int cy = y + 26, keyW = 120;
    auto kh = [&](const char* key, int yy) { helpBtn(fx + 5, yy + 1, std::string("tilekind.") + key, "tab.tiles"); };
    auto kcheck = [&](int cx2, const char* lbl, bool& v, const char* key) { bool r = ui.checkbox(cx2, cy, lbl, v); helpBtn(cx2 + 18 + font->width(lbl), cy - 1, std::string("tilekind.") + key, "tab.tiles"); return r; };
    kh("name", cy); kh("ground", cy + 24); kh("prop", cy + 58);
    ui.label(fx + 22, cy + 2, "name"); std::string nm = k.name; if (ui.textField(fx + keyW, cy, 200, nm, 40)) { k.name = nm; kindsDirty = true; rebuildTileKindIndex(); } cy += 24;
    ui.label(fx + 22, cy + 2, "ground tiles"); ui.text(fx + 22, cy + 16, "(random per cell)", DIM);
    for (size_t i = 0; i < k.ground.size(); i++) {
        int gx = fx + keyW + (int)i * 70;
        ui.tile(k.ground[i], gx, cy - 2, 2, k.opaque ? C::White : k.dayTint, k.dayBg);
        if (ui.button(gx + 28, cy - 2, 26, 13, "...")) { size_t ii = i; openTilePicker(k.ground[i], [&k, ii, this](Tile t) { k.ground[ii] = t; kindsDirty = true; }, 't'); }
        if (k.ground.size() > 1 && ui.button(gx + 28, cy + 12, 26, 13, "x", true, false, DANGER)) { k.ground.erase(k.ground.begin() + i); kindsDirty = true; break; }
    }
    if (k.ground.size() < 6 && ui.button(fx + keyW + (int)k.ground.size() * 70, cy, 26, 20, "+")) { k.ground.push_back(k.ground.back()); kindsDirty = true; }
    cy += 34;
    ui.label(fx + 22, cy + 2, "prop (overlay)"); ui.tile(k.prop, fx + keyW, cy - 2, 2);
    if (ui.button(fx + keyW + 28, cy - 2, 26, 13, "...")) openTilePicker(k.prop, [&k, this](Tile t) { k.prop = t; kindsDirty = true; }, 'p');
    if (k.prop.valid() && ui.button(fx + keyW + 28, cy + 12, 26, 13, "x", true, false, DANGER)) { k.prop = Tile{}; kindsDirty = true; }
    cy += 34;
    if (kcheck(fx + 10, "walkable", k.walkable, "walkable")) kindsDirty = true;
    if (kcheck(fx + 130, "blocks sight", k.blocksSight, "blockssight")) kindsDirty = true;
    if (kcheck(fx + 270, "water", k.water, "water")) kindsDirty = true;
    if (kcheck(fx + 360, "opaque ground (draw black bg)", k.opaque, "opaque")) kindsDirty = true;
    cy += 22;
    auto colorRow = [&](const char* lbl, Color8& c) {
        ui.label(fx + 22, cy + 2, lbl); kh(lbl[0] == 't' ? "tint" : "bg", cy); int r = c.r, g = c.g, b = c.b;
        if (ui.intField(fx + keyW, cy, 70, r, 0, 255)) { c.r = (uint8_t)r; kindsDirty = true; }
        if (ui.intField(fx + keyW + 76, cy, 70, g, 0, 255)) { c.g = (uint8_t)g; kindsDirty = true; }
        if (ui.intField(fx + keyW + 152, cy, 70, b, 0, 255)) { c.b = (uint8_t)b; kindsDirty = true; }
        ui.cv->fillRect(fx + keyW + 230, cy, 40, 16, c); ui.cv->rect(fx + keyW + 230, cy, 40, 16, BORDER);
        cy += 22;
    };
    colorRow("tint (ground)", k.tint); colorRow("background", k.bg);
    // 1.8.0 (#159): the day colours the game draws, worked out from the kind unless picked by hand (daybg= / daytint=)
    computeTileKindDay(k);
    auto dayRow = [&](const char* lbl, bool& has, Color8& set, Color8 now, const char* what) {
        ui.label(fx + 22, cy + 2, lbl); helpBtn(fx + 5, cy + 1, "tilekind.daybg", "tab.tiles");
        if (ui.checkbox(fx + keyW, cy, "by hand", has)) { if (has) set = now; kindsDirty = true; }
        int x0 = fx + keyW + 76;
        if (has) {
            int r = set.r, g = set.g, bb = set.b;
            if (ui.intField(x0, cy, 50, r, 0, 255)) { set.r = (uint8_t)r; kindsDirty = true; }
            if (ui.intField(x0 + 54, cy, 50, g, 0, 255)) { set.g = (uint8_t)g; kindsDirty = true; }
            if (ui.intField(x0 + 108, cy, 50, bb, 0, 255)) { set.b = (uint8_t)bb; kindsDirty = true; }
        } else ui.text(x0, cy + 2, fmt("worked out: %d,%d,%d", now.r, now.g, now.b), DIM);
        ui.cv->fillRect(x0 + 166, cy, 40, 16, now); ui.cv->rect(x0 + 166, cy, 40, 16, BORDER);
        ui.text(x0 + 214, cy + 2, what, DIM);
        cy += 22;
    };
    dayRow("day background", k.hasDayBg, k.dayBgSet, k.dayBg, k.opaque || k.under || k.name == "rock" || k.name == "void" || k.bg.r || k.bg.g || k.bg.b ? "(kept: wall, piece, water or own bg)" : "(drawn by day)");
    if (k.recolor) dayRow("day pattern", k.hasDayTint, k.dayTintSet, k.dayTint, "(the pattern's colour by day)");
    computeTileKindDay(k);
    ui.label(fx + 22, cy + 2, "encounter weight"); kh("encounter", cy); if (ui.intField(fx + keyW, cy, 80, k.encounter, 0, 20)) kindsDirty = true; ui.text(fx + keyW + 90, cy + 2, "overworld: higher = more random fights (0 = none)", DIM); cy += 22;
    ui.label(fx + 22, cy + 2, "biome"); kh("biome", cy); std::string bm = k.biome; if (ui.textField(fx + keyW, cy, 124, bm, 20)) { k.biome = bm; kindsDirty = true; }
    if (ui.button(fx + keyW + 126, cy, 14, 16, "v")) { std::vector<std::string> ids{"(none)"}; for (auto& e : gens[5].secs) { std::string id = e.get("id"); if (id.size() > 6 && id.substr(id.size() - 6) == "_night") continue; ids.push_back(id); } int cur = 0; for (size_t i = 0; i < ids.size(); i++) if (ids[i] == k.biome) cur = (int)i; TileKind* kp = &k; openListPicker("Biome = the encounter table random fights on this tile use", ids, cur, [this, ids, kp](int i) { kp->biome = i == 0 ? "" : ids[i]; kindsDirty = true; }); }
    { bool tbl = false; for (auto& e : gens[5].secs) if (e.get("id") == k.biome) tbl = true; ui.text(fx + keyW + 150, cy + 2, k.biome.empty() ? "no biome: fights here use the map's table=" : tbl ? (k.encounter > 0 ? "random fights here use the '" + k.biome + "' table (Encounters tab)" : "encounter weight is 0: no random fights here") : "no encounter table called '" + k.biome + "' - fights fall back to the map's table=", k.biome.empty() || tbl ? DIM : ACCENT2); }
    cy += 22;
    ui.label(fx + 22, cy + 2, "move cost"); kh("movecost", cy); if (ui.intField(fx + keyW, cy, 80, k.moveCost, 1, 9)) kindsDirty = true; cy += 22;
    // 1.3.3: footstep sound when the party walks onto this kind
    ui.label(fx + 22, cy + 2, "footsteps"); kh("step", cy); { std::string st = k.step; if (ui.textField(fx + keyW, cy, 124, st, 40)) { k.step = trim(st); kindsDirty = true; } }
    if (ui.button(fx + keyW + 126, cy, 14, 16, "v")) { std::vector<std::string> ids{"(plain step)"}; for (auto& id : gAudio.sfxChoices()) ids.push_back(id); int cur = 0; for (size_t i = 0; i < ids.size(); i++) if (ids[i] == k.step) cur = (int)i; TileKind* kp = &k; openListPicker("Footstep sound when the party walks onto this tile", ids, cur, [this, ids, kp](int i) { kp->step = i == 0 ? "" : ids[i]; kindsDirty = true; gAudio.play(i == 0 ? "step" : ids[i]); }); }
    if (ui.button(fx + keyW + 144, cy, 40, 16, "play")) gAudio.play(!k.step.empty() && gAudio.has(k.step) ? k.step : "step");
    ui.text(fx + keyW + 192, cy + 2, k.step.empty() ? "empty = the plain 'step' sound" : gAudio.has(k.step) ? "sound event (Sounds tab)" : "no sound '" + k.step + "' - plays 'step'", k.step.empty() || gAudio.has(k.step) ? DIM : ACCENT2);
    cy += 22;
    // 1.4.0 (#58 #61 #79): what the kind does in fights
    ui.label(fx + 22, cy + 2, "in fights"); kh("fights", cy);
    if (kcheck(fx + keyW, "carries shock", k.conduct, "conduct")) kindsDirty = true;
    if (kcheck(fx + keyW + 120, "flammable ground", k.flammable, "flammable")) kindsDirty = true;
    if (kcheck(fx + keyW + 260, "burns (cover)", k.burns, "burns")) kindsDirty = true;
    cy += 22;
    {
        auto kindField = [&](int x0, int w0, std::string& v, const char* key, const char* lbl) {
            ui.label(x0, cy + 2, lbl); int lw = font->width(lbl) + 6; std::string t = v;
            if (ui.textField(x0 + lw, cy, w0, t, 40)) { v = trim(t); kindsDirty = true; }
            if (ui.button(x0 + lw + w0 + 2, cy, 14, 16, "v")) { std::vector<std::string> ids{"(none)"}; for (auto& kk : allTileKinds()) ids.push_back(kk.name); int cur = 0; for (size_t i = 0; i < ids.size(); i++) if (ids[i] == v) cur = (int)i; std::string* vp = &v; openListPicker(std::string("Tile kind it turns into (") + key + "=)", ids, cur, [this, vp](int i) { auto& ks = allTileKinds(); *vp = i <= 0 ? "" : ks[i - 1].name; kindsDirty = true; }); }
            return x0 + lw + w0 + 22;
        };
        ui.label(fx + 22, cy + 2, "turns into"); kh("breaks", cy);
        int x1 = kindField(fx + keyW, 90, k.breaks, "breaks", "broken:");
        ui.label(x1, cy + 2, "hp"); if (ui.intField(x1 + 18, cy, 48, k.breakHp, 0, 999)) kindsDirty = true;
        int x2 = kindField(x1 + 74, 80, k.snuffTo, "snuff", "put out:");
        kindField(x2, 80, k.kindleTo, "kindle", "lit:");
    }
    cy += 30;
    // big preview
    ui.label(fx + 22, cy, "preview (as in game, 2x):"); cy += 14;
    for (int i = 0; i < 6; i++) { int px = fx + 10 + i * 26; ui.cv->fillRect(px, cy, 24, 24, k.dayBg); Tile g = k.ground[i % k.ground.size()]; if (g.valid()) { if (k.recolor) ts->drawRecolor(*ui.cv, g, px, cy, 2, k.dayTint); else if (k.opaque) ts->drawOpaque(*ui.cv, g, px, cy, 2, k.dayTint); else ts->draw(*ui.cv, g, px, cy, 2, k.dayTint); } if (k.prop.valid()) ts->draw(*ui.cv, k.prop, px, cy, 2); }
    cy += 34;
    // usage
    int used = 0; if (mapLoaded) for (auto& c : em.cells) if (split(c, '|')[0] == k.name) used++;
    ui.text(fx + 10, cy, mapLoaded ? fmt("used by %d cells of the map open in the Maps tab (%s)", used, em.id.c_str()) : "", DIM);
    ui.text(fx + 10, cy + 16, "Tip: 'door_wood', 'door_metal', 'door_arch' are the door kinds; pick the tile you prefer in 'prop'.", DIM);
}

// ---------------- maps ----------------
std::vector<std::string> Editor::paletteList() const {
    std::vector<std::string> out;
    for (auto& k : allTileKinds()) if (edMatch(k.name + " " + k.biome, paletteFilter)) out.push_back(k.name);
    return out;
}
void Editor::loadMapFile(const std::string& f) {
    if (em.load(readPath("maps/" + f + ".txt"))) { em.path = dataRoot + "/maps/" + f + ".txt"; mapLoaded = true; mapDirty = false; viewX = viewY = 0; objSel = -1; resizeW = em.w; resizeH = em.h; mapUndo.clear(); selX0 = -1; pasting = false; say("Opened " + f); }
}
void Editor::mapPushUndo() { MapSnapshot sn{em.w, em.h, em.cells, em.objs}; mapUndo.push_back(sn); if (mapUndo.size() > 40) mapUndo.erase(mapUndo.begin()); }
void Editor::mapUndoPop() { if (mapUndo.empty()) { say("Nothing to undo."); return; } auto& sn = mapUndo.back(); em.w = sn.w; em.h = sn.h; em.cells = sn.cells; em.objs = sn.objs; mapUndo.pop_back(); mapDirty = true; objSel = -1; resizeW = em.w; resizeH = em.h; say("Undo."); }
void Editor::paintCell(int x, int y, const std::string& spec) { if (x < 0 || y < 0 || x >= em.w || y >= em.h) return; if (em.at(x, y) != spec) { em.at(x, y) = spec; mapDirty = true; } }
void Editor::floodFill(int x, int y, const std::string& spec) {
    if (x < 0 || y < 0 || x >= em.w || y >= em.h) return;
    std::string from = em.at(x, y); if (from == spec) return;
    std::vector<std::pair<int, int>> st{{x, y}};
    while (!st.empty()) { auto [cx, cy] = st.back(); st.pop_back(); if (cx < 0 || cy < 0 || cx >= em.w || cy >= em.h || em.at(cx, cy) != from) continue; em.at(cx, cy) = spec; st.push_back({cx + 1, cy}); st.push_back({cx - 1, cy}); st.push_back({cx, cy + 1}); st.push_back({cx, cy - 1}); }
    mapDirty = true;
}
void Editor::drawMapCell(int sx, int sy, const std::string& spec, int scale, int mx, int my) {
    std::string first = trim(split(spec, '|')[0]);
    const TileKind* k = tileKind(first);
    int sz = 12 * scale;
    if (!k) { ui.cv->fillRect(sx, sy, sz, sz, RGB(255, 0, 255)); return; }
    Tile g = k->ground.empty() ? Tile{} : k->ground[0]; Color8 bg = k->dayBg;   // 1.8.0 (#159): the day look
    for (auto& t : k->ground) if (t.valid()) { g = t; break; }
    // 1.1.1: on the map, each cell shows one of the kind's ground pieces like the game does (which picks one at random
    // per cell; an empty entry draws nothing), so a pattern list with gaps shows its gaps here too
    if (mx >= 0 && k->ground.size() > 1) { uint32_t h = (uint32_t)mx * 73856093u ^ (uint32_t)my * 19349663u; h ^= h >> 13; h *= 0x5bd1e995u; h ^= h >> 15; g = k->ground[h % k->ground.size()]; }
    int mask = 0, corners = 0;
    if (mx >= 0 && (!k->edges.empty() || k->under))   // joined pieces (paths, shores, rivers...) as the game draws them
        tileKindLook(*k, [&](int dx, int dy) -> const TileKind* { int nx = mx + dx, ny = my + dy; if (nx < 0 || ny < 0 || nx >= em.w || ny >= em.h) return nullptr; return tileKind(trim(split(em.at(nx, ny), '|')[0])); }, g, bg, &mask, &corners);
    ui.cv->fillRect(sx, sy, sz, sz, bg);
    drawGroundPiece(*ui.cv, *ts, k, g, sx, sy, scale, k->dayTint, k->recolor, k->opaque, mask, corners);
    if (k->prop.valid()) ts->draw(*ui.cv, k->prop, sx, sy, scale);
}
static const char* OBJ_TYPES[] = {"npc", "enemy", "chest", "exit", "trigger", "sign", "door", "savepoint", "obstacle", "lever", "change", "gather", "crate", "plate", "trap"};   // 1.4.0: crate, plate; 1.5.0: trap (keep new types at the end: tests click by index)
static const int NOBJ = 15;
static std::vector<std::pair<std::string, std::string>> objTemplate(const std::string& t, int x, int y) {
    std::vector<std::pair<std::string, std::string>> kv{{"x", std::to_string(x)}, {"y", std::to_string(y)}};
    if (t == "npc") { kv.insert(kv.begin(), {{"id", "npc1"}, {"name", "Villager"}}); kv.push_back({"sprite", "104,0"}); kv.push_back({"dialogue", ""}); kv.push_back({"wander", "0"}); }
    else if (t == "enemy") { kv.push_back({"group", "goblin,goblin"}); kv.push_back({"aggro", "5"}); kv.push_back({"flag", ""}); kv.push_back({"level", "0"}); }
    else if (t == "chest") { kv.push_back({"flag", "chest_" + std::to_string(x) + "_" + std::to_string(y)}); kv.push_back({"items", "potion_red:1"}); kv.push_back({"gold", "10"}); kv.push_back({"locked", ""}); }
    else if (t == "exit") { kv.push_back({"w", "1"}); kv.push_back({"h", "1"}); kv.push_back({"to", "overworld"}); kv.push_back({"tx", "0"}); kv.push_back({"ty", "0"}); }
    else if (t == "trigger") { kv.push_back({"w", "1"}); kv.push_back({"h", "1"}); kv.push_back({"script", "message:Title|Text"}); kv.push_back({"flag", ""}); kv.push_back({"once", "1"}); }
    else if (t == "sign") kv.push_back({"text", "A sign."});
    else if (t == "door") { kv.push_back({"key", ""}); kv.push_back({"flag", ""}); kv.push_back({"openif", ""}); kv.push_back({"require", ""}); kv.push_back({"locked", ""}); }
    else if (t == "obstacle") { kv.push_back({"w", "1"}); kv.push_back({"h", "1"}); kv.push_back({"name", "Rubble"}); kv.push_back({"needs", "rubble"}); kv.push_back({"clear", ""}); kv.push_back({"flag", "rubble_" + std::to_string(x) + "_" + std::to_string(y)}); kv.push_back({"text", "Fallen rock blocks the way."}); kv.push_back({"onclear", ""}); }
    else if (t == "lever") { kv.push_back({"name", "Lever"}); kv.push_back({"flag", "lever_" + std::to_string(x) + "_" + std::to_string(y)}); kv.push_back({"toggle", "1"}); kv.push_back({"text", ""}); }
    else if (t == "change") { kv.push_back({"w", "1"}); kv.push_back({"h", "1"}); kv.push_back({"kind", "floor_stone"}); kv.push_back({"if", "some_flag"}); }
    else if (t == "gather") { kv.push_back({"item", "herb_moon"}); kv.push_back({"count", "1-2"}); kv.push_back({"tool", ""}); kv.push_back({"respawn", "3"}); }
    else if (t == "crate") { kv.insert(kv.begin(), {"id", "crate_" + std::to_string(x) + "_" + std::to_string(y)}); kv.push_back({"name", "Crate"}); kv.push_back({"heavy", "0"}); }   // 1.4.0 (#76)
    else if (t == "plate") { kv.push_back({"group", "plates1"}); kv.push_back({"onpress", "script:pressure_plates,plates1,plates1_open"}); }
    else if (t == "trap") { kv.push_back({"name", "Spike pit"}); kv.push_back({"hurt", "100"}); kv.push_back({"damage", "10"}); kv.push_back({"flag", ""}); }   // 1.5.0 (#99)
    return kv;
}
// 1.4.0 (#75): every needs= value an obstacle understands (worldstate.cpp obstacleMenu), for the object panel's + list
std::vector<std::string> Editor::obstacleNeedChoices() const { return {"rubble", "dig", "blast", "burn", "melt", "freeze", "shock", "emp", "light", "force:15", "spell:detect", "spell:unlock", "spell:", "item:", "flag:SOME_FLAG"}; }
std::string Editor::obstacleNeedLabel(const std::string& n) const {
    if (n == "rubble") return "rubble - a shovel or a blast";
    if (n == "dig") return "dig - a shovel";
    if (n == "blast") return "blast - a blasting charge or a blast spell";
    if (n == "burn") return "burn - a torch, fire oil or a fire spell";
    if (n == "melt") return "melt - fire, as burn";
    if (n == "freeze") return "freeze - water: a freeze spell or a Frost Flask (this visit only)";
    if (n == "shock") return "shock - a dead machine: a shock spell or a Charged Cell";
    if (n == "emp") return "emp - a sealed tech door: an emp spell or an EMP Grenade";
    if (n == "light") return "light - hidden until a lit party comes close";
    if (n == "force:15") return "force:N - STR N, or a force spell (edit the number)";
    if (n == "spell:detect") return "spell:detect - a detect spell nearby finds it";
    if (n == "spell:unlock") return "spell:unlock - a knock / hack spell";
    if (n == "spell:") return "spell:UTILITY - a spell with that utility...";
    if (n == "item:") return "item:ID - uses up that item...";
    return "flag:COND - clears itself once the condition holds (edit it)";
}
// 1.4.0 (#76): the "+" list for action keys: common actions, then every script with the argument names its "# args:" line gives
std::string Editor::scriptArgs(const std::string& id) const {
    std::string t; if (!readFile(readPath("scripts/" + id + ".txt"), t)) return "";
    for (auto& raw : split(t, '\n')) { std::string l = trim(raw); if (l.empty()) continue; if (l[0] != '#') break; l = trim(l.substr(1)); if (startsWith(toLower(l), "args:")) return trim(l.substr(5)); }
    return "";
}
void Editor::actionChoices(std::vector<std::string>& ids, std::vector<std::string>& labels) const {
    static const char* acts[] = {"heal", "message:Title|Text", "item:ITEM_ID:1", "gold:10", "flag:NAME", "dialogue:ID", "combat:goblin,goblin", "teleport:MAP,X,Y", "cutscene:line 1|line 2", "resetcrates:", "lorefile:NAME", "recipe:RECIPE_ID"};
    for (auto a : acts) { ids.push_back(a); labels.push_back(a); }
    for (auto& sf : scriptFiles) {
        std::string args = scriptArgs(sf), id = "script:" + sf;
        if (!args.empty()) for (auto& a : split(args, ',')) { std::string an = trim(a); if (!an.empty()) id += "," + an; }
        ids.push_back(id); labels.push_back("script:" + sf + (args.empty() ? "" : "   args: " + args));
    }
}
const std::vector<std::string>& edObjTypes() { static std::vector<std::string> v(OBJ_TYPES, OBJ_TYPES + NOBJ); return v; }
std::vector<std::string> edObjKeys(const std::string& t) {   // what each object type reads (world.cpp loadMapText); x, y and an npc's id are per placement
    if (t == "npc") return {"npc", "name", "sprite", "color", "dialogue", "script", "shop", "wander", "show", "hide", "hostile", "craft", "board", "travel", "moveif", "movex", "movey"};
    if (t == "enemy") return {"group", "aggro", "level", "flag", "ondefeat", "show", "hide", "link", "leash", "goal", "goaltext", "ongoal", "ongoalfail"};
    if (t == "chest") return {"items", "gold", "locked", "flag", "sprite", "opensprite", "color", "sound", "onuse"};
    if (t == "exit") return {"w", "h", "to", "tx", "ty", "require", "item", "locked"};
    if (t == "trigger") return {"w", "h", "script", "flag", "once", "require"};
    if (t == "sign") return {"text", "sprite", "color", "title", "sound", "onuse"};
    if (t == "door") return {"key", "flag", "openif", "require", "locked", "metal", "sprite", "opensprite", "sound", "text", "onuse"};
    if (t == "savepoint") return {"sprite", "color", "sparkle", "sparklecolor", "text", "sound", "onuse"};
    if (t == "obstacle") return {"w", "h", "name", "kind", "clear", "needs", "flag", "text", "onclear"};
    if (t == "lever") return {"name", "flag", "toggle", "text", "sprite", "onsprite", "sound", "onuse"};
    if (t == "change") return {"w", "h", "kind", "if"};
    if (t == "gather") return {"node", "item", "count", "tool", "respawn", "name", "sprite", "color", "spentsprite", "spent", "sound"};
    if (t == "crate") return {"name", "heavy", "sprite", "color"};          // 1.4.0 (#76); id is per placement
    if (t == "plate") return {"group", "onpress", "onrelease", "sprite", "color"};
    if (t == "trap") return {"name", "hurt", "damage", "flag", "text", "onspring", "sprite", "sprungsprite", "color"};   // 1.5.0 (#99)
    return {};
}

void Editor::drawMaps(InputState& in) {
    int x = 4, y = TOP + 4, h = H - TOP - BOTTOM - 8, lw = 200;
    // ---- left column: maps list + tools + palette ----
    ui.panel(x, y, lw, h);
    listHeader(x, y, lw, fmt("Maps (%d)", (int)mapFiles.size()), IMP_MAPS); helpBtn(x + lw - 16, y + 1, "tab.maps");
    ui.searchField(x + 4, y + 20, lw - 8, mapFilter, "search maps...");
    std::vector<std::string> ml; std::vector<int> mIdx;
    for (size_t i = 0; i < mapFiles.size(); i++) { std::string n = mapFiles[i]; if (!edMatch(n, mapFilter)) continue; if (dataRoot != baseRoot && !fileExists(dataRoot + "/maps/" + n + ".txt")) n += "  (base)"; ml.push_back(n); mIdx.push_back((int)i); }   // (base) = inherited: saving copies it into the campaign
    int mSel = -1; for (size_t i = 0; i < mIdx.size(); i++) if (mIdx[i] == mapListSel) mSel = (int)i;
    int hit = ui.listBox(x + 4, y + 38, lw - 8, 92, ml, mSel, mapListScroll);
    if (mapFiles.empty()) ui.text(x + 10, y + 42, "No maps yet: New or Import.", DIM);
    if (hit >= 0) { hit = mIdx[hit]; mapListSel = hit; }
    if (hit >= 0) { if (mapDirty) { std::string f = mapFiles[hit]; openConfirm("Discard unsaved changes to the current map?", [this, f]() { loadMapFile(f); }); } else loadMapFile(mapFiles[hit]); }
    int by = y + 134;
    if (ui.button(x + 4, by, 44, 18, "New")) openTextPrompt("New map id (file name)", "new_map", [this](const std::string& nm) { em = EdMap(); em.id = nm; em.path = dataRoot + "/maps/" + nm + ".txt"; em.header = {{"id", nm}, {"name", nm}, {"kind", "town"}, {"music", "town"}, {"spawn", "5,5"}, {"bg", "0,0,0"}}; em.init(30, 20, "floor_grass"); for (int i = 0; i < 30; i++) { em.at(i, 0) = "tree"; em.at(i, 19) = "tree"; } for (int j = 0; j < 20; j++) { em.at(0, j) = "tree"; em.at(29, j) = "tree"; } mapLoaded = true; mapDirty = true; resizeW = 30; resizeH = 20; objSel = -1; refreshFiles();
        if (camp.startMap.empty() && dataRoot != baseRoot) { camp.startMap = nm; camp.startX = 5; camp.startY = 5; saveCampaignForm(); say("New games of this campaign start on " + nm + " (B over a tile or the Campaign tab changes it). Save the map to keep it."); } });
    if (ui.button(x + 52, by, 44, 18, "Save", mapLoaded && mapDirty, false, OK)) { if (em.save()) { mapDirty = false; changed = true; refreshFiles(); say("Saved " + em.path); } else say("Save failed!"); }
    if (ui.button(x + 100, by, 48, 18, "Revert", mapLoaded && mapDirty)) { std::string f = em.id; openConfirm("Revert all changes?", [this, f]() { loadMapFile(f); }); }
    if (ui.button(x + 152, by, 44, 18, "Check", mapLoaded)) {
        std::vector<std::string> problems;
        auto walk = [&](int cx, int cy) { if (cx < 0 || cy < 0 || cx >= em.w || cy >= em.h) return false; const TileKind* k = tileKind(trim(split(em.at(cx, cy), '|')[0])); return k && k->walkable; };
        for (auto& oRaw : em.objs) { EdObj o = resolvedObj(oRaw); int ox = o.geti("x"), oy = o.geti("y");   // as the game sees it (template and resource keys filled in)
            if (!oRaw.get("template").empty() && !templateSec(oRaw.get("template"))) problems.push_back(fmt("object at %d,%d uses unknown template '%s'", ox, oy, oRaw.get("template").c_str()));
            if (o.type == "gather" && !o.get("node").empty() && !resourceSec(o.get("node"), "")) problems.push_back(fmt("gather node at %d,%d uses unknown resource type '%s'", ox, oy, o.get("node").c_str()));
            if (o.type == "npc" && !trim(o.get("npc")).empty() && !npcSec(o.get("npc"))) problems.push_back(fmt("npc at %d,%d shows unknown NPC '%s' (NPCs tab)", ox, oy, o.get("npc").c_str()));
            if (o.type == "npc" && trim(o.get("npc")).empty()) problems.push_back(fmt("npc at %d,%d is not linked to an NPC (select it > make NPC)", ox, oy));
            if (o.type == "gather" && !gd->item(o.get("item"))) problems.push_back(fmt("gather node at %d,%d gives unknown item '%s'", ox, oy, o.get("item").c_str()));
            if (o.type != "door" && o.type != "sign" && o.type != "lever" && o.type != "obstacle" && o.type != "change" && o.type != "gather" && !walk(ox, oy)) problems.push_back(o.type + " " + o.get("id") + o.get("name") + fmt(" at %d,%d is on a blocked tile", ox, oy)); if (o.type == "npc" && !o.get("dialogue").empty() && std::find(dlgFiles.begin(), dlgFiles.end(), o.get("dialogue")) == dlgFiles.end()) problems.push_back("npc " + o.get("id") + " references missing dialogue '" + o.get("dialogue") + "'"); if (o.type == "exit") { std::string to = o.get("to"); bool ok = to == "overworld" || std::find(mapFiles.begin(), mapFiles.end(), to) != mapFiles.end(); for (auto& g : gens[7].secs) if (g.get("id") == to) ok = true; if (!ok) problems.push_back(fmt("exit at %d,%d leads to unknown map '%s'", ox, oy, to.c_str())); } if (o.type == "enemy") for (auto& c : split(o.get("group"), ',')) if (!trim(c).empty() && !gd->creature(trim(c))) problems.push_back("enemy group uses unknown creature '" + trim(c) + "'"); if (o.type == "chest") for (auto& it : split(o.get("items"), ',')) { std::string id = trim(split(it, ':')[0]); if (!id.empty() && !gd->item(id)) problems.push_back("chest has unknown item '" + id + "'"); } }
        auto sp = split(em.hget("spawn"), ','); if (sp.size() == 2 && !walk(atoi(sp[0].c_str()), atoi(sp[1].c_str()))) problems.push_back("spawn point is on a blocked tile");
        for (auto& c : em.cells) if (!tileKind(trim(split(c, '|')[0]))) { problems.push_back("map uses unknown tile kind '" + c + "'"); break; }
        { int stx, sty; if (campaignStart(em.id, stx, sty) && !walk(stx, sty)) problems.insert(problems.begin(), fmt("the new-game START (%d,%d) is on a blocked tile - press B over a floor tile", stx, sty)); }
        {   // entering from the overworld should put the party next to the way back out
            int sx0 = sp.size() == 2 ? atoi(sp[0].c_str()) : 0, sy0 = sp.size() == 2 ? atoi(sp[1].c_str()) : 0;
            for (auto& l : gens[8].secs) if (l.get("map") == em.id) {
                int tx = atoi(l.get("tx", "0").c_str()), ty = atoi(l.get("ty", "0").c_str()); if (!tx && !ty) { tx = sx0; ty = sy0; }
                int best = 9999; for (auto& o : em.objs) if (o.type == "exit" && o.get("to") == "overworld") { int ox = o.geti("x"), oy = o.geti("y"), ow = std::max(1, o.geti("w", 1)), oh = std::max(1, o.geti("h", 1)); int dx = std::max({ox - tx, 0, tx - (ox + ow - 1)}), dy = std::max({oy - ty, 0, ty - (oy + oh - 1)}); best = std::min(best, std::max(dx, dy)); }
                if (best != 9999 && best > 3) problems.push_back(fmt("arriving from the overworld (%s) at %d,%d is %d tiles from the exit back out - press N over a tile near the exit to move it", l.get("name", l.get("id")).c_str(), tx, ty, best));
                if (!walk(tx, ty)) problems.push_back(fmt("overworld arrival (%s) at %d,%d is on a blocked tile", l.get("name", l.get("id")).c_str(), tx, ty));
            }
        }
        if (problems.empty()) say("Check: no problems found."); else { std::sort(problems.begin(), problems.end(), [](const std::string& a, const std::string& b) { return (a.find("arriving") != std::string::npos) > (b.find("arriving") != std::string::npos); }); std::string all; for (size_t i = 0; i < problems.size() && i < 3; i++) all += (i ? "  |  " : "") + problems[i]; say(fmt("%d problem(s): %s", (int)problems.size(), all.c_str())); }
    }
    // tools
    int ty = by + 24; ui.header(x, ty, lw, "Tools"); helpBtn(x + lw - 16, ty + 1, "maps.tools", "tab.maps"); ty += 18;
    const char* tools[] = {"Paint", "Fill", "Rect", "Objects", "Erase obj", "Select"};
    for (int i = 0; i < 6; i++) { int bw = 60; int bx = x + 4 + (i % 3) * 64; int byy = ty + (i / 3) * 22; if (ui.button(bx, byy, bw, 18, tools[i], true, mapTool == i)) { mapTool = i; pasting = false; } }
    ty += 46;
    if (ui.button(x + 4, ty, 40, 18, "1x", true, zoom == 1)) zoom = 1; if (ui.button(x + 48, ty, 40, 18, "2x", true, zoom == 2)) zoom = 2;
    ui.checkbox(x + 96, ty + 3, "grid", showGrid); ui.checkbox(x + 150, ty + 3, "objs", showObjs);
    ty += 22;
    if (ui.button(x + 4, ty, 44, 18, "Undo", !mapUndo.empty())) mapUndoPop();
    if (ui.button(x + 52, ty, 44, 18, "Copy", selX0 >= 0)) { in.press(K_C); in.down[K_CTRL] = true; }
    if (ui.button(x + 100, ty, 44, 18, "Paste", mapClip.w > 0)) { pasting = true; mapTool = 5; }
    if (ui.button(x + 148, ty, 48, 18, "Clear", selX0 >= 0)) { in.press(K_DELETE); }
    ty += 24;
    if (mapTool == 3) {
        ui.label(x + 4, ty, "place:"); helpBtn(x + 44, ty - 1, "obj." + std::string(OBJ_TYPES[objType]), "maps.objects"); for (int i = 0; i < NOBJ; i++) { int bw = 46, bx = x + 4 + (i % 4) * 48, byy = ty + 14 + (i / 4) * 20; if (ui.button(bx, byy, bw, 18, OBJ_TYPES[i], true, objType == i && placeTmpl.empty())) { objType = i; placeTmpl.clear(); } }
        ty += 16 + (NOBJ + 3) / 4 * 20;
        {   // 1.1.0: place copies of a template from the Objects tab
            const EdSection* pt = placeTmpl.empty() ? nullptr : templateSec(placeTmpl); if (!pt) placeTmpl.clear();
            ui.label(x + 4, ty + 2, "template:");
            if (ui.choice(x + 62, ty, lw - 66, !pt ? (gens[15].secs.empty() ? "(none - Objects tab)" : "(none)") : placeTmpl + " (" + pt->get("type") + ")")) {
                std::vector<std::string> ids{""}, labels{"(none) - place the plain type above"};
                for (auto& s : gens[15].secs) if (s.name == "template") { ids.push_back(s.get("id")); labels.push_back(s.get("id") + "  (" + s.get("type") + ")" + (s.get("name").empty() ? "" : "  " + s.get("name"))); }
                int cur = 0; for (size_t i = 0; i < ids.size(); i++) if (ids[i] == placeTmpl) cur = (int)i;
                openListPicker("Place copies of a template (make them in the Objects tab)", labels, cur, [this, ids](int i) { placeTmpl = ids[i]; });
            }
            if (ui.hover(x + 62, ty, lw - 66, 16)) ui.tip("Copies follow their template (Objects tab); a key set on one copy is that copy's own");
            ty += 20;
        }
        if (objType == 0 && placeTmpl.empty()) {   // 1.1.9 (#33): which NPC (NPCs tab) an npc placement shows
            const EdSection* pn = npcSec(placeNpc); if (!pn) placeNpc.clear();
            ui.label(x + 4, ty + 2, "NPC:");
            if (ui.choice(x + 62, ty, lw - 66, pn ? pn->get("name", placeNpc) : "(a new NPC)")) {
                std::vector<std::string> ids{""}, labels{"+ a new NPC (made in the NPCs tab when you click)"};
                for (auto& sc : gens[17].secs) if (sc.name == "npc" && !trim(sc.get("id")).empty()) { ids.push_back(trim(sc.get("id"))); labels.push_back(sc.get("name", sc.get("id")) + "  (" + sc.get("id") + ")" + (trim(sc.get("companion")).empty() ? "" : "  - companion")); }
                int cur = 0; for (size_t i = 0; i < ids.size(); i++) if (ids[i] == placeNpc) cur = (int)i;
                openListPicker("NPC to place (edit them in the NPCs tab)", labels, cur, [this, ids](int i) { placeNpc = ids[i]; });
            }
            if (ui.hover(x + 62, ty, lw - 66, 16)) ui.tip("Clicking the map places this NPC (npc=). Its look, name and dialogue come from the NPCs tab; the placement keeps its spot and show/hide.");
            ty += 20;
        }
    } else {
        ui.label(x + 4, ty, "brush:"); ui.text(x + 44, ty, brush, ACCENT2); if (ui.button(x + lw - 50, ty - 2, 44, 14, "edit")) jumpToKind(brush); ty += 14;
        ui.searchField(x + 4, ty, lw - 8, paletteFilter, "search tiles...", 30); ty += 20;
        auto pal = paletteList(); int psel = -1; for (size_t i = 0; i < pal.size(); i++) if (pal[i] == brush) psel = (int)i;
        int ph = y + h - ty - 4; int rowH = 16, rows = ph / rowH;
        ui.cv->fillRect(x + 4, ty, lw - 8, ph, FIELD); ui.cv->rect(x + 4, ty, lw - 8, ph, BORDER);
        if (ui.hover(x + 4, ty, lw - 8, ph) && in.wheel) paletteScroll -= in.wheel * 3;
        paletteScroll = clampi(paletteScroll, 0, std::max(0, (int)pal.size() - rows));
        for (int r = 0; r < rows && paletteScroll + r < (int)pal.size(); r++) {
            int i = paletteScroll + r; int ry = ty + 1 + r * rowH; const TileKind* k = tileKind(pal[i]);
            bool hv = ui.hover(x + 5, ry, lw - 10, rowH);
            if (i == psel) ui.cv->fillRect(x + 5, ry, lw - 10, rowH, SEL); else if (hv) ui.cv->fillRect(x + 5, ry, lw - 10, rowH, HOVER);
            if (k) { ui.cv->fillRect(x + 7, ry + 1, 14, 14, k->dayBg); if (k->ground[0].valid()) { if (k->recolor) ts->drawRecolor(*ui.cv, k->ground[0], x + 8, ry + 2, 1, k->dayTint); else if (k->opaque) ts->drawOpaque(*ui.cv, k->ground[0], x + 8, ry + 2, 1, k->dayTint); else ts->draw(*ui.cv, k->ground[0], x + 8, ry + 2, 1, k->dayTint); } if (k->prop.valid()) ts->draw(*ui.cv, k->prop, x + 8, ry + 2, 1); }
            font->draw(*ui.cv, *ts, x + 26, ry + 2, pal[i], k && !k->walkable ? RGB(255, 150, 150) : TEXT);
            if (hv && in.mousePressed[0] && !ui.mouseUsed) { ui.mouseUsed = true; brush = pal[i]; }
        }
    }
    if (!mapLoaded) { ui.text(x + lw + 20, y + 20, "Open a map on the left or press New.", DIM); return; }
    // ---- right: properties strip + view ----
    int vx = x + lw + 6, vy = y, vw = W - vx - 4, vh = h;
    ui.panel(vx, vy, vw, vh);
    // header fields
    int hy = vy + 4;
    // header fields: click a field's name for its guide page
    auto hfield = [&](const char* key, int& cx, int w) { int lw2 = font->width(key); bool hv = ui.hover(cx, hy, lw2 + 2, 16); ui.text(cx, hy + 2, key, hv ? ACCENT : DIM); if (hv) { ui.cv->fillRect(cx, hy + 14, lw2, 1, ACCENT); ui.tip(std::string("Guide: map ") + key + " (click)"); if (ui.clicked(cx, hy, lw2 + 2, 16)) openGuide(std::string("map.") + key); }
        std::string v = em.hget(key); cx += lw2 + 4; if (ui.textField(cx, hy, w, v, 60)) { em.hset(key, v); mapDirty = true; } cx += w + 8; };
    helpBtn(vx + 5, hy + 1, "maps.header", "tab.maps");
    int cx = vx + 22;
    bool hnarrow = vw < 820;   // 1.3.1: the 960-wide Editor - narrower fields so music v, fight and on enter fit
    hfield("name", cx, hnarrow ? 72 : 96); hfield("kind", cx, hnarrow ? 52 : 64); hfield("spawn", cx, hnarrow ? 44 : 50); hfield("dark", cx, 24); hfield("light", cx, 24); hfield("encounter", cx, 24);
    // 1.3.1 (#48): music - a list of the tracks (Sounds tab); empty plays the track for the map's kind, shown greyed
    { int mfx = cx + font->width("music") + 4; hfield("music", cx, hnarrow ? 60 : 64);
      if (em.hget("music").empty()) ui.text(mfx + 3, hy + 2, edKindMusic(em.hget("kind")), DIM);
      if (ui.button(cx - 8, hy, 14, 16, "v")) { std::string def = edKindMusic(em.hget("kind")); std::vector<std::string> ids, labels;
          for (auto& id : gAudio.order) { auto it = gAudio.defs.find(id); if (it == gAudio.defs.end() || it->second.kind != "music") continue; ids.push_back(id); labels.push_back(it->second.desc.empty() ? id : id + "  - " + it->second.desc); }
          std::string now = em.hget("music").empty() ? def : em.hget("music"); int cur = 0; for (size_t i = 0; i < ids.size(); i++) if (ids[i] == now) cur = (int)i;
          if (!ids.empty()) openListPicker("Music on this map (Sounds tab: the tracks)", labels, cur, [this, ids](int i) { em.hset("music", ids[i]); mapDirty = true; }); }
      cx += 10; }
    // 1.3.1 (#47): combatmusic= - this map's own fight music (empty = the campaign's, Campaign tab); bosses keep the boss music
    { std::string cm = em.hget("combatmusic"); int bw = hnarrow ? 40 : 46;
      if (ui.button(cx - 4, hy, bw, 16, cm.empty() ? "fight" : "fight*", true, !cm.empty())) {
          std::vector<std::string> ids{""}, labels{"(the campaign's fight music)"};
          for (auto& id : gAudio.order) { auto it = gAudio.defs.find(id); if (it == gAudio.defs.end() || it->second.kind != "music") continue; ids.push_back(id); labels.push_back(it->second.desc.empty() ? id : id + "  - " + it->second.desc); }
          int cur = 0; for (size_t i = 0; i < ids.size(); i++) if (ids[i] == cm) cur = (int)i;
          openListPicker("Fight music on this map (bosses play their own)", labels, cur, [this, ids](int i) { em.hset("combatmusic", ids[i]); if (ids[i].empty()) em.header.erase(std::remove_if(em.header.begin(), em.header.end(), [](const std::pair<std::string, std::string>& kv) { return kv.first == "combatmusic"; }), em.header.end()); mapDirty = true; }); }
      if (ui.hover(cx - 4, hy, bw, 16)) { ui.tip(cm.empty() ? "Fights here play the campaign's fight music (Campaign tab). Click to give this map its own. Right-click: guide" : "Fights here play " + cm + " (a boss, or a creature with its own music, plays that). Right-click: guide"); if (in.mousePressed[1]) openGuide("map.combatmusic"); }
      cx += bw + 4; }
    // 1.1.10 (#34): onenter= - actions run each time the party arrives (usually script:<id>)
    { std::string oe = em.hget("onenter"); int bw = std::min(76, vx + vw - 6 - cx);
      if (ui.button(cx - 4, hy, bw, 16, oe.empty() ? "on enter" : "on enter *", true, !oe.empty())) {
          std::vector<std::string> ids{"", "(type actions...)"}, labels{"(nothing)", "Type actions (key:value;key:value)..."}; for (auto& sf : scriptFiles) { ids.push_back("script:" + sf); labels.push_back("run script " + sf); }
          int cur = 0; for (size_t i = 0; i < ids.size(); i++) if (ids[i] == oe) cur = (int)i;
          openListPicker("When the party arrives on this map (not when a save is loaded)", labels, cur, [this, ids](int i) {
              if (i == 1) { openTextPrompt("Actions run on arrival, e.g. script:town_events or log:You smell smoke.", em.hget("onenter"), [this](const std::string& v) { em.hset("onenter", trim(v)); mapDirty = true; }); return; }
              em.hset("onenter", ids[i]); mapDirty = true; }); }
      if (ui.hover(cx - 4, hy, bw, 16)) ui.tip(oe.empty() ? "Nothing runs when the party arrives. Click to run a script (Scripts tab) each time they do." : "On arrival: " + oe); }
    hy += 20; cx = vx + 22;
    hfield("table", cx, 56);
    if (ui.button(cx - 8, hy, 14, 16, "v")) { std::vector<std::string> ids{"(none)"}; for (auto& e : gens[5].secs) ids.push_back(e.get("id")); int cur = 0; for (size_t i = 0; i < ids.size(); i++) if (ids[i] == em.hget("table")) cur = (int)i; openListPicker("Encounter table (used where a tile kind has no biome table)", ids, cur, [this, ids](int i) { em.hset("table", i == 0 ? "" : ids[i]); mapDirty = true; }); }
    cx += 10; hfield("bg", cx, 54); hfield("worldmap", cx, 46);
    // 1.1.5 (#28): the map's background loop - nothing plays unless a map picks one here (the list = [ambient] entries in the Sounds tab)
    hfield("ambient", cx, 64);
    if (ui.button(cx - 8, hy, 14, 16, "v")) { std::vector<std::string> ids{"(none)"}, labels{"(none)"}; for (auto& id : gAudio.order) { auto it = gAudio.defs.find(id); if (it == gAudio.defs.end() || it->second.kind != "ambient") continue; ids.push_back(id); labels.push_back(it->second.desc.empty() ? id : id + "  - " + it->second.desc); }
        int cur = 0; for (size_t i = 0; i < ids.size(); i++) if (ids[i] == em.hget("ambient")) cur = (int)i;
        openListPicker("Background loop (plays under the music on this map)", labels, cur, [this, ids](int i) { em.hset("ambient", i == 0 ? "" : ids[i]); mapDirty = true; }); }
    cx += 10;
    // 1.2.3 (#40): touch=1 - map enemy groups start a fight only by touching the party (the overworld's usual choice)
    { bool on = atoi(em.hget("touch").c_str()) != 0; int lw2 = font->width("touch");
      if (ui.checkbox(cx, hy + 2, "touch", on)) { if (on) em.hset("touch", "1"); else em.header.erase(std::remove_if(em.header.begin(), em.header.end(), [](const std::pair<std::string, std::string>& kv) { return kv.first == "touch"; }), em.header.end()); mapDirty = true; }
      if (ui.hover(cx, hy, 16 + lw2, 16)) { ui.tip(on ? "touch=1: enemy groups start a fight only by touching the party (right next to it). Right-click: guide" : "Enemy groups attack when the party comes within their aggro range. Tick: only by touching it. Right-click: guide"); if (in.mousePressed[1]) openGuide("map.touch"); }
      cx += 16 + lw2 + 6; }
    ui.label(cx, hy + 2, "size"); cx += 30; ui.intField(cx, hy, 50, resizeW, 4, 300); cx += 52; ui.intField(cx, hy, 50, resizeH, 4, 300); cx += 54;
    if (ui.button(cx, hy, 50, 16, "Resize", resizeW != em.w || resizeH != em.h)) { mapPushUndo(); em.resize(resizeW, resizeH, brush); mapDirty = true; } cx += 56;
    if (ui.button(cx, hy, 60, 16, "Play >", true, false, OK)) { auto sp = split(em.hget("spawn"), ','); requestPlay(sp.size() == 2 ? atoi(sp[0].c_str()) : 0, sp.size() == 2 ? atoi(sp[1].c_str()) : 0); } if (ui.hover(cx, hy, 60, 16)) ui.tip("Saves everything and starts the game here at the spawn point (P over a tile: play there; N over a tile: overworld arrival point)");
    // view
    int ax = vx + 2, ay = vy + 44, aw = vw - 4, ah = vh - 46;
    int cell = 12 * zoom;
    ui.cv->fillRect(ax, ay, aw, ah, RGB(14, 14, 18));
    // 1.1.0 fix: the object panel (drawn last, over the view) owns the mouse in its area - clicks there used to reach the map
    bool panelOn = objSel >= 0 && objSel < (int)em.objs.size() && showObjs;
    int pw = 300, px = vx + vw - pw - 4, py = ay + 4, ph = 0;
    if (panelOn) { std::string from; int ni = (int)inheritedKeys(em.objs[objSel], from).size(); ph = std::min(ah - 8, 22 + ((int)em.objs[objSel].kv.size() + ni) * 20 + (ni ? 16 : 0) + 44); }   // same sum as the panel below
    bool overPanel = panelOn && ui.hover(px, py, pw, ph);
    bool overView = ui.hover(ax, ay, aw, ah) && !overPanel;
    bool typing = ui.typing();
    // scrolling: middle drag, wheel, keys
    if (overView && in.mousePressed[1]) { mapPanning = true; }
    if (mapPanning) { viewX -= in.mouseDX; viewY -= in.mouseDY; if (!in.mouseDown[1]) mapPanning = false; }
    if (overView && in.wheel) { if (in.down[K_SHIFT]) viewX -= in.wheel * cell * 3; else viewY -= in.wheel * cell * 3; }
    if (!typing && pop == P_NONE) { int sp = 6; if (in.down[K_LEFT] || in.down[K_A]) viewX -= sp; if (in.down[K_RIGHT] || in.down[K_D]) viewX += sp; if (in.down[K_UP] || in.down[K_W]) viewY -= sp; if (in.down[K_DOWN] || in.down[K_S]) viewY += sp; }
    viewX = clampi(viewX, 0, std::max(0, em.w * cell - aw)); viewY = clampi(viewY, 0, std::max(0, em.h * cell - ah));
    ui.cv->setClip({ax, ay, aw, ah});
    int c0 = viewX / cell, r0 = viewY / cell;
    for (int r = r0; r <= r0 + ah / cell + 1 && r < em.h; r++) for (int c = c0; c <= c0 + aw / cell + 1 && c < em.w; c++) {
        int sx = ax + c * cell - viewX, sy = ay + r * cell - viewY;
        drawMapCell(sx, sy, em.at(c, r), zoom, c, r);
        if (showGrid && zoom == 2) { ui.cv->put(sx, sy, RGB(255, 255, 255, 40)); }
    }
    // objects
    int hoverObj = -1;
    if (showObjs) for (size_t i = 0; i < em.objs.size(); i++) {
        EdObj o = resolvedObj(em.objs[i]); int ox = o.geti("x"), oy = o.geti("y"), ow = std::max(1, o.geti("w", 1)), oh = std::max(1, o.geti("h", 1));
        int sx = ax + ox * cell - viewX, sy = ay + oy * cell - viewY;
        Color8 col = C::White;
        if (o.type == "npc") { Tile t = parseTile(o.get("sprite")); if (!t.valid()) t = T(104, 0); if (!o.get("color").empty()) ts->drawRecolor(*ui.cv, t, sx, sy, zoom, parseColor(o.get("color"))); else ts->draw(*ui.cv, t, sx, sy, zoom); col = C::Cyan; }
        else if (o.type == "enemy") { std::string first = trim(split(o.get("group"), ',')[0]); const CreatureDef* cd = gd->creature(first); int sz = cd && cd->size == 2 ? 2 : 1; ui.cv->fillRect(sx, sy, cell * sz, cell * sz, RGB(120, 0, 0, 160));
            if (cd) for (int b = 0; b < sz; b++) for (int a = 0; a < sz; a++) { Tile t = tileAt(cd->tile, a, b); if (cd->recolor) ts->drawRecolor(*ui.cv, t, sx + a * cell, sy + b * cell, zoom, cd->color); else ts->draw(*ui.cv, t, sx + a * cell, sy + b * cell, zoom); }
            if (sz == 2) { ow = oh = 2; ui.cv->rect(sx, sy, cell * 2, cell * 2, C::Orange); font->draw(*ui.cv, *ts, sx + 2, sy + cell * 2 - 11, "2x2", C::Orange); }
            col = C::Red; }
        else if (o.type == "chest" || o.type == "sign" || o.type == "savepoint" || o.type == "lever") { drawObjectLook(o, sx, sy, zoom, false); col = o.type == "sign" ? C::Cream : o.type == "savepoint" ? C::Cyan : C::Yellow; }   // 1.1.1: as the game draws it
        else if (o.type == "exit") { ui.cv->fillRect(sx, sy, ow * cell, oh * cell, RGB(0, 200, 255, 90)); col = C::Cyan; }
        else if (o.type == "trigger") { ui.cv->fillRect(sx, sy, ow * cell, oh * cell, RGB(255, 200, 0, 80)); col = C::Yellow; }
        else if (o.type == "door") { if (parseTile(o.get("sprite")).valid()) drawObjectLook(o, sx, sy, zoom, false); ui.cv->rect(sx + 1, sy + 1, cell - 2, cell - 2, C::Orange); col = C::Orange; }
        else if (o.type == "obstacle") { ui.cv->fillRect(sx, sy, ow * cell, oh * cell, RGB(150, 110, 60, 110)); ts->draw(*ui.cv, TL::ROCKS, sx, sy, zoom); col = C::Orange; }
        else if (o.type == "change") { ui.cv->fillRect(sx, sy, ow * cell, oh * cell, RGB(180, 80, 255, 70)); col = C::Purple; }
        else if (o.type == "gather") { if (ox >= 0 && oy >= 0 && ox < em.w && oy < em.h) drawGatherNode(o, sx, sy, zoom, em.at(ox, oy)); col = C::Green; }   // as the game draws it
        else if (o.type == "crate") { Tile t = parseTile(o.get("sprite")); if (!t.valid()) t = T(8, 36); Color8 cc = !trim(o.get("color")).empty() ? parseColor(o.get("color")) : atoi(o.get("heavy").c_str()) > 0 ? RGB(190, 150, 110) : RGB(225, 180, 110); ts->drawRecolor(*ui.cv, t, sx, sy, zoom, cc); col = C::Orange; }   // 1.4.0
        else if (o.type == "plate") { Tile t = parseTile(o.get("sprite")); if (t.valid()) ts->draw(*ui.cv, t, sx, sy, zoom); else { int in2 = 3 * zoom / 2; ui.cv->fillRect(sx + in2, sy + in2, cell - 2 * in2, cell - 2 * in2, RGB(75, 70, 60)); ui.cv->rect(sx + in2, sy + in2, cell - 2 * in2, cell - 2 * in2, RGB(150, 140, 120)); } col = C::Cream; }
        else if (o.type == "trap") { Tile t = parseTile(o.get("sprite")); if (t.valid()) ts->draw(*ui.cv, t, sx, sy, zoom); else { int in2 = 2 * zoom; ui.cv->fillRect(sx + in2, sy + in2, cell - 2 * in2, cell - 2 * in2, RGB(15, 10, 10)); for (int k = 0; k < 3; k++) { int bx = sx + in2 + 1 + k * (cell - 2 * in2) / 3; ui.cv->line(bx, sy + cell - in2 - 1, bx + zoom, sy + in2 + 1, RGB(200, 190, 180)); } } col = C::Red; }   // 1.5.0 (#99)
        ui.cv->rect(sx, sy, ow * cell, oh * cell, (int)i == objSel ? C::White : Color8{col.r, col.g, col.b, 160});
        if (overView && in.mouseX >= sx && in.mouseY >= sy && in.mouseX < sx + ow * cell && in.mouseY < sy + oh * cell) hoverObj = (int)i;
    }
    // arrival points: the map's spawn (S) and where each overworld location drops the party (A)
    auto arrivals = [&]() { std::vector<std::pair<std::string, std::pair<int, int>>> out; auto sp = split(em.hget("spawn"), ','); int sx0 = sp.size() == 2 ? atoi(sp[0].c_str()) : 0, sy0 = sp.size() == 2 ? atoi(sp[1].c_str()) : 0;
        for (auto& l : gens[8].secs) if (l.get("map") == em.id) { int tx = atoi(l.get("tx", "0").c_str()), ty = atoi(l.get("ty", "0").c_str()); if (!tx && !ty) { tx = sx0; ty = sy0; } out.push_back({l.get("name", l.get("id")), {tx, ty}}); }
        return out; };
    if (showObjs) {
        auto sp = split(em.hget("spawn"), ',');
        if (sp.size() == 2) { int sx = ax + atoi(sp[0].c_str()) * cell - viewX, sy = ay + atoi(sp[1].c_str()) * cell - viewY; ui.cv->rect(sx, sy, cell, cell, C::Green); font->draw(*ui.cv, *ts, sx + 2, sy + (cell - 10) / 2, "S", C::Green); }
        for (auto& a : arrivals()) { int sx = ax + a.second.first * cell - viewX, sy = ay + a.second.second * cell - viewY; ui.cv->rect(sx + 1, sy + 1, cell - 2, cell - 2, C::Purple); font->draw(*ui.cv, *ts, sx + cell - 7, sy, "A", C::Purple); }
        int stx, sty; if (campaignStart(em.id, stx, sty)) {   // where a new game begins
            int sx = ax + stx * cell - viewX, sy = ay + sty * cell - viewY; bool blink = (ui.time * 2 - (int)(ui.time * 2)) < 0.5f;
            ui.cv->rect(sx - 1, sy - 1, cell + 2, cell + 2, blink ? C::Yellow : C::White); ui.cv->rect(sx, sy, cell, cell, C::Yellow);
            int tw = font->width("START") + 4; ui.cv->fillRect(sx + cell / 2 - tw / 2, sy - 12, tw, 11, RGB(0, 0, 0, 200)); font->draw(*ui.cv, *ts, sx + cell / 2 - tw / 2 + 2, sy - 12, "START", C::Yellow);
        }
    }
    // mouse on tiles
    int mtx = (in.mouseX - ax + viewX) / cell, mty = (in.mouseY - ay + viewY) / cell;
    bool inMap = overView && mtx >= 0 && mty >= 0 && mtx < em.w && mty < em.h && !mapPanning;
    if (inMap) {
        int sx = ax + mtx * cell - viewX, sy = ay + mty * cell - viewY;
        ui.cv->rect(sx, sy, cell, cell, C::White);
        const TileKind* hk = tileKind(trim(split(em.at(mtx, mty), '|')[0]));
        std::string info = fmt("(%d,%d) %s", mtx, mty, em.at(mtx, mty).c_str()); for (auto& a : arrivals()) if (a.second.first == mtx && a.second.second == mty) info += "  arrival from " + a.first; { int stx, sty; if (campaignStart(em.id, stx, sty) && stx == mtx && sty == mty) info += "  NEW GAME START"; } if (hk && !hk->walkable) info += " [blocked]";
        if (hk && hk->encounter > 0 && atoi(em.hget("encounter").c_str()) > 0 && hk->biome != "loc") {   // which encounter table a random fight here would use
            bool tbl = false; for (auto& e : gens[5].secs) if (!hk->biome.empty() && e.get("id") == hk->biome) tbl = true;
            std::string t = tbl ? hk->biome : em.hget("table"); if (!t.empty()) info += "  fights: " + t; }
        if (hoverObj >= 0) info += "  obj: " + em.objs[hoverObj].type + " " + em.objs[hoverObj].get("id") + em.objs[hoverObj].get("name");
        // 1.1.5: drawn in a box at the bottom right of the view, over the key hints (the header row is full since the ambient field)
        { int iw = font->width(info) + 10, ix = std::max(ax, ax + aw - iw - 4), iy = ay + ah - 38; ui.cv->fillRect(ix, iy, iw, 18, RGB(24, 26, 34)); ui.cv->rect(ix, iy, iw, 18, RGB(70, 74, 90)); ui.text(ix + 5, iy + 3, info, ACCENT2); }
        if (!typing && pop == P_NONE && !ui.modal) {
            if (in.pressed[K_K]) { jumpToKind(trim(split(em.at(mtx, mty), '|')[0])); return; }
            if (in.pressed[K_P]) { requestPlay(mtx, mty); return; }
            if (in.pressed[K_B]) { setCampaignStart(em.id, mtx, mty); }   // B: new games begin on this tile
            if (in.pressed[K_N]) {   // N: the party arrives here when entering this map from the overworld
                int n = 0; for (auto& l : gens[8].secs) if (l.get("map") == em.id) { l.set("tx", std::to_string(mtx)); l.set("ty", std::to_string(mty)); n++; }
                if (n) { gens[8].save(); changed = true; worldNeedsGen = true; say(fmt("Overworld arrival for %s set to %d,%d (saved overworld.txt).", em.id.c_str(), mtx, mty)); }
                else say("No overworld location enters " + em.id + " (World tab: set a location's map to it).");
            }
        }
        if (mapTool <= 2 && !ui.modal) {
            if (in.mousePressed[2]) { brush = trim(split(em.at(mtx, mty), '|')[0]); say("Picked brush: " + brush); }
            if (mapTool == 0 && in.mousePressed[0] && !ui.mouseUsed) mapPushUndo();
            if (mapTool == 0 && in.mouseDown[0] && !ui.mouseUsed) paintCell(mtx, mty, brush);
            if (mapTool == 1 && in.mousePressed[0] && !ui.mouseUsed) { ui.mouseUsed = true; mapPushUndo(); floodFill(mtx, mty, brush); }
            if (mapTool == 2) {
                if (in.mousePressed[0] && !ui.mouseUsed) { rectX0 = mtx; rectY0 = mty; rectDragging = true; ui.mouseUsed = true; }
                if (rectDragging) { int x0 = std::min(rectX0, mtx), x1 = std::max(rectX0, mtx), y0 = std::min(rectY0, mty), y1 = std::max(rectY0, mty);
                    ui.cv->rect(ax + x0 * cell - viewX, ay + y0 * cell - viewY, (x1 - x0 + 1) * cell, (y1 - y0 + 1) * cell, C::Yellow);
                    if (!in.mouseDown[0]) { mapPushUndo(); bool outline = in.down[K_SHIFT]; for (int yy = y0; yy <= y1; yy++) for (int xx = x0; xx <= x1; xx++) if (!outline || xx == x0 || xx == x1 || yy == y0 || yy == y1) paintCell(xx, yy, brush); rectDragging = false; } }
            }
        } else if (mapTool == 3 && !ui.modal) {
            if (in.mousePressed[0] && !ui.mouseUsed) {
                ui.mouseUsed = true;
                if (hoverObj >= 0) { mapPushUndo(); objSel = hoverObj; dragObj = true; ui.blur(); }
                else { mapPushUndo(); EdObj o; const EdSection* pt = placeTmpl.empty() ? nullptr : templateSec(placeTmpl);
                    if (pt) {   // a copy of a template: only what belongs to this spot is written on the map
                        o.type = pt->get("type", "npc"); o.kv = {{"template", placeTmpl}, {"x", std::to_string(mtx)}, {"y", std::to_string(mty)}};
                        if (o.type == "npc") { int n = 1; for (auto& eo : em.objs) if (eo.get("template") == placeTmpl) n++; o.kv.insert(o.kv.begin() + 1, {"id", placeTmpl + "_" + std::to_string(n)}); }
                        if ((o.type == "chest" || o.type == "lever" || o.type == "obstacle") && !pt->has("flag")) o.set("flag", o.type + "_" + std::to_string(mtx) + "_" + std::to_string(mty));
                    } else if (objType == 0) {   // 1.1.9: an NPC placement (a new NPC is made when none is picked)
                        std::string nid = placeNpc; if (!npcSec(nid)) { nid = newNpc(nullptr); placeNpc = nid; say("New NPC '" + nid + "' made and placed - give it a name, look and dialogue in the NPCs tab."); }
                        o.type = "npc"; o.kv = {{"id", npcPlacementId(em, nid)}, {"npc", nid}, {"x", std::to_string(mtx)}, {"y", std::to_string(mty)}};
                    } else { o.type = OBJ_TYPES[objType]; o.kv = objTemplate(o.type, mtx, mty);
                        if (o.type == "gather" && !gens[16].secs.empty()) o.kv = {{"x", std::to_string(mtx)}, {"y", std::to_string(mty)}, {"node", gens[16].secs[0].get("id")}}; }   // takes the rest from its resource type
                    em.objs.push_back(o); objSel = (int)em.objs.size() - 1; mapDirty = true; dragObj = true; ui.blur(); }
            }
            if (dragObj && objSel >= 0) { if (!in.mouseDown[0]) dragObj = false; else { auto& o = em.objs[objSel]; if (o.geti("x") != mtx || o.geti("y") != mty) { o.set("x", std::to_string(mtx)); o.set("y", std::to_string(mty)); mapDirty = true; } } }
        } else if (mapTool == 4 && !ui.modal) {
            if (in.mousePressed[0] && hoverObj >= 0 && !ui.mouseUsed) { ui.mouseUsed = true; mapPushUndo(); em.objs.erase(em.objs.begin() + hoverObj); objSel = -1; mapDirty = true; }
        } else if (mapTool == 5 && !ui.modal) {
            if (pasting) {
                // ghost preview + stamp on click
                for (int yy = 0; yy < mapClip.h; yy++) for (int xx = 0; xx < mapClip.w; xx++) { int gx = mtx + xx, gy = mty + yy; if (gx >= em.w || gy >= em.h) continue; drawMapCell(ax + gx * cell - viewX, ay + gy * cell - viewY, mapClip.cells[(size_t)yy * mapClip.w + xx], zoom); }
                ui.cv->rect(ax + mtx * cell - viewX, ay + mty * cell - viewY, mapClip.w * cell, mapClip.h * cell, C::Yellow);
                if (in.mousePressed[0] && !ui.mouseUsed) {
                    ui.mouseUsed = true; mapPushUndo();
                    for (int yy = 0; yy < mapClip.h; yy++) for (int xx = 0; xx < mapClip.w; xx++) paintCell(mtx + xx, mty + yy, mapClip.cells[(size_t)yy * mapClip.w + xx]);
                    for (auto o : mapClip.objs) { o.set("x", std::to_string(o.geti("x") + mtx)); o.set("y", std::to_string(o.geti("y") + mty)); if (o.geti("x") < em.w && o.geti("y") < em.h) em.objs.push_back(o); }
                    mapDirty = true; if (!in.shift()) pasting = false;
                }
                if (in.mousePressed[2]) pasting = false;
            } else {
                if (in.mousePressed[0] && !ui.mouseUsed) {
                    ui.mouseUsed = true;
                    if (hoverObj >= 0) { objSel = hoverObj; selX0 = -1; selDragging = false; ui.blur(); }
                    else { objSel = -1; selX0 = selX1 = mtx; selY0 = selY1 = mty; selDragging = true; }
                }
                if (selDragging) { if (!in.mouseDown[0]) selDragging = false; else { selX1 = mtx; selY1 = mty; } }
                if (in.mousePressed[2]) selX0 = -1;
            }
        }
    } else if (!in.mouseDown[0]) { rectDragging = false; dragObj = false; selDragging = false; }
    // selection rectangle (shown in every tool so copy/clear stay visible)
    if (selX0 >= 0) { int x0 = std::min(selX0, selX1), x1 = std::max(selX0, selX1), y0 = std::min(selY0, selY1), y1 = std::max(selY0, selY1); ui.cv->setClip({ax, ay, aw, ah}); ui.cv->fillRect(ax + x0 * cell - viewX, ay + y0 * cell - viewY, (x1 - x0 + 1) * cell, (y1 - y0 + 1) * cell, RGB(255, 255, 255, 40)); ui.cv->rect(ax + x0 * cell - viewX, ay + y0 * cell - viewY, (x1 - x0 + 1) * cell, (y1 - y0 + 1) * cell, (ui.time * 4 - (int)(ui.time * 4)) < 0.5f ? C::White : C::Yellow); ui.cv->resetClip(); }
    // keyboard shortcuts for the map (when not typing)
    if (!typing && pop == P_NONE && !ui.modal) {
        bool ctrl = in.ctrl();
        if (ctrl && in.pressed[K_Z]) mapUndoPop();
        if (ctrl && in.pressed[K_S] && mapDirty) { if (em.save()) { mapDirty = false; changed = true; say("Saved " + em.path); } }
        if (ctrl && in.pressed[K_A]) { selX0 = 0; selY0 = 0; selX1 = em.w - 1; selY1 = em.h - 1; mapTool = 5; }
        if (ctrl && (in.pressed[K_C] || in.pressed[K_X]) && selX0 >= 0) {
            int x0 = std::min(selX0, selX1), x1 = std::max(selX0, selX1), y0 = std::min(selY0, selY1), y1 = std::max(selY0, selY1);
            mapClip = MapClip(); mapClip.w = x1 - x0 + 1; mapClip.h = y1 - y0 + 1;
            for (int yy = y0; yy <= y1; yy++) for (int xx = x0; xx <= x1; xx++) mapClip.cells.push_back(em.at(xx, yy));
            for (auto& o : em.objs) { int ox = o.geti("x"), oy = o.geti("y"); if (ox >= x0 && ox <= x1 && oy >= y0 && oy <= y1) { EdObj c = o; c.set("x", std::to_string(ox - x0)); c.set("y", std::to_string(oy - y0)); mapClip.objs.push_back(c); } }
            say(fmt("Copied %dx%d tiles and %d objects. Ctrl+V / Paste, then click to stamp (shift-click to stamp repeatedly).", mapClip.w, mapClip.h, (int)mapClip.objs.size()));
            if (in.pressed[K_X]) { in.pressed[K_DELETE] = true; }
        }
        if (ctrl && in.pressed[K_V] && mapClip.w > 0) { pasting = true; mapTool = 5; }
        if (in.pressed[K_DELETE] && selX0 >= 0 && !(objSel >= 0 && mapTool == 3)) {
            int x0 = std::min(selX0, selX1), x1 = std::max(selX0, selX1), y0 = std::min(selY0, selY1), y1 = std::max(selY0, selY1);
            mapPushUndo();
            for (int yy = y0; yy <= y1; yy++) for (int xx = x0; xx <= x1; xx++) paintCell(xx, yy, brush);
            for (size_t i = 0; i < em.objs.size(); i++) { int ox = em.objs[i].geti("x"), oy = em.objs[i].geti("y"); if (ox >= x0 && ox <= x1 && oy >= y0 && oy <= y1) { em.objs.erase(em.objs.begin() + i); i--; } }
            objSel = -1; mapDirty = true; say("Cleared selection with the current brush.");
        }
        if (in.pressed[K_ESCAPE] && (pasting || selX0 >= 0)) { pasting = false; selX0 = -1; in.eatKeys(); }
    }
    ui.cv->resetClip();
    if (mapTool <= 2) ui.text(ax + 4, ay + ah - 17, "LMB paint  RMB pick  K: edit kind  P: play here  B: new-game start  N: overworld arrival  mid-drag/wheel/WASD: scroll  Rect: shift = outline  ^Z undo  ^S save", Color8{255, 255, 255, 150});
    else if (mapTool == 5) ui.text(ax + 4, ay + ah - 17, pasting ? "click to stamp (shift-click: keep stamping)  RMB/Esc: cancel" : "click object: edit properties  drag empty space: select  ^C copy  ^X cut  ^V paste  Del: clear  ^A all", Color8{255, 255, 255, 150});
    else ui.text(ax + 4, ay + ah - 17, "click empty: place  click object: select/drag  Delete key: remove selected  ctrl+Z undo", Color8{255, 255, 255, 150});
    if (objSel >= 0 && objSel < (int)em.objs.size() && !typing && in.pressed[K_DELETE] && pop == P_NONE && mapTool == 3) { mapPushUndo(); em.objs.erase(em.objs.begin() + objSel); objSel = -1; mapDirty = true; }
    // object property panel (its area is kept free of map clicks above)
    if (objSel >= 0 && objSel < (int)em.objs.size() && showObjs) {
        EdObj& o = em.objs[objSel];
        EdObj ro = resolvedObj(o);   // type and keys as the game sees them
        std::string from; auto inh = inheritedKeys(o, from);
        std::string tmplId = o.get("template"); const EdSection* tsec = tmplId.empty() ? nullptr : templateSec(tmplId);
        int rows = (int)o.kv.size() + (int)inh.size();
        int contentH = 22 + rows * 20 + (inh.empty() ? 0 : 16) + 22 + 22;
        ph = std::min(ah - 8, contentH);
        ui.panel(px, py, pw, ph, RGB(38, 41, 50, 240)); ui.header(px, py, pw, "object: " + ro.type + (tmplId.empty() ? "" : "  - copy of " + tmplId)); helpBtn(px + pw - 16, py + 1, "obj." + ro.type, "maps.objects");
        int viewTop = py + 18, viewBot = py + ph - 1;
        if (overPanel && in.wheel) objPanelScroll -= in.wheel * 20;
        objPanelScroll = clampi(objPanelScroll, 0, std::max(0, contentH - ph));
        auto shown = [&](int yy) { return yy >= viewTop && yy + 16 <= viewBot; };
        ui.cv->setClip({px + 1, viewTop, pw - 2, viewBot - viewTop});
        int ry = py + 22 - objPanelScroll;
        bool gone = false;
        for (size_t i = 0; i < o.kv.size(); i++) {
            const std::string key = o.kv[i].first;
            if (!shown(ry)) { ui.idCounter += 1; ry += 20; continue; }
            if (key == "template") {   // the link to an Objects tab template
                ui.label(px + 22, ry + 2, "template"); ui.text(px + 88, ry + 2, o.kv[i].second, tsec ? ACCENT2 : DANGER);
                if (!tsec && ui.hover(px + 88, ry, 90, 16)) ui.tip("objects.txt has no template with this id");
                if (ui.button(px + pw - 122, ry, 46, 16, "edit", tsec != nullptr)) { for (size_t t = 0; t < gens[15].secs.size(); t++) if (&gens[15].secs[t] == tsec) gens[15].sel = (int)t; gens[15].filter.clear(); gens[15].fieldScroll = 0; tab = 22; ui.blur(); }
                if (ui.hover(px + pw - 122, ry, 46, 16)) ui.tip("Edit the template in the Objects tab: every copy on every map follows it");
                if (ui.button(px + pw - 72, ry, 66, 16, "detach", tsec != nullptr)) { mapPushUndo(); o.type = tsec->get("type", o.type); for (auto& kv : tsec->kv) if (kv.first != "id" && kv.first != "type" && !o.has(kv.first)) o.kv.push_back(kv); o.erase("template"); mapDirty = true; ui.blur(); ui.idCounter += 1; break; }
                if (ui.hover(px + pw - 72, ry, 66, 16)) ui.tip("Copy the template's values into this object and unlink it from the template");
                ui.idCounter += 1; ry += 20; continue;
            }
            if (key == "npc" && ro.type == "npc") {   // 1.1.9: the link to an NPCs tab entry
                const EdSection* ns = npcSec(o.kv[i].second);
                helpBtn(px + 5, ry + 1, "obj.npc.npc", "obj.npc");
                ui.label(px + 22, ry + 2, "npc"); ui.text(px + 88, ry + 2, o.kv[i].second + (ns ? "  " + ns->get("name") : ""), ns ? ACCENT2 : DANGER);
                if (!ns && ui.hover(px + 88, ry, 90, 16)) ui.tip("npcs.txt has no NPC with this id");
                if (ui.button(px + pw - 122, ry, 46, 16, "edit", ns != nullptr)) { std::string nid = trim(o.kv[i].second); for (size_t t = 0; t < gens[17].secs.size(); t++) if (trim(gens[17].secs[t].get("id")) == nid) gens[17].sel = (int)t; gens[17].filter.clear(); gens[17].filterKey.clear(); gens[17].fieldScroll = 0; gens[17].scroll = std::max(0, gens[17].sel - 5); tab = 24; ui.blur(); }
                if (ui.hover(px + pw - 122, ry, 46, 16)) ui.tip("Edit the NPC in the NPCs tab: every placement of it follows");
                if (ui.button(px + pw - 72, ry, 66, 16, "other...")) { std::vector<std::string> ids, labels; int cur = 0; for (auto& sc : gens[17].secs) if (sc.name == "npc") { if (trim(sc.get("id")) == trim(o.kv[i].second)) cur = (int)ids.size(); ids.push_back(trim(sc.get("id"))); labels.push_back(sc.get("name", sc.get("id")) + "  (" + sc.get("id") + ")"); }
                    auto* kvp = &o.kv[i]; openListPicker("This placement shows which NPC?", labels, cur, [this, kvp, ids](int j) { mapPushUndo(); kvp->second = ids[j]; mapDirty = true; }); }
                if (ui.hover(px + pw - 72, ry, 66, 16)) ui.tip("Show a different NPC here (the spot, id and show/hide stay)");
                ui.idCounter += 1; ry += 20; continue;
            }
            helpBtn(px + 5, ry + 1, "obj." + ro.type + "." + key, "obj." + ro.type);
            ui.label(px + 22, ry + 2, key);
            bool flagKey = key == "flag", condKey = key == "require" || key == "openif" || key == "show" || key == "hide" || key == "moveif" || key == "if";
            // 1.4.0 (#75/#76): needs= gets a list of what clears an obstacle; action lists get the actions and the scripts (with their args)
            bool needsKey = key == "needs" && ro.type == "obstacle";
            bool actKey = key == "onuse" || key == "onclear" || key == "onpress" || key == "onrelease" || key == "ondefeat" || (key == "script" && ro.type == "trigger");
            if (needsKey || actKey) { flagKey = condKey = false; }
            int vw2 = pw - 116 - (isTileKey(key) ? 30 : 0) - (flagKey || condKey || needsKey || actKey ? 30 : 0);
            if (ui.textField(px + 88, ry, vw2, o.kv[i].second, 300)) mapDirty = true;
            if (condKey && ui.hover(px + 88, ry, vw2, 16) && !o.kv[i].second.empty()) ui.tip("Reads: " + condEnglish(o.kv[i].second));
            if (flagKey && ui.hover(px + 88, ry, vw2, 16) && !o.kv[i].second.empty()) { std::string w = flagSetSummary(o.kv[i].second); ui.tip(o.kv[i].second + (w.empty() ? " - only this object sets it" : " - also set by " + w)); }
            if (isTileKey(key)) { Tile t = parseTile(o.kv[i].second); if (ui.button(px + 88 + vw2 + 4, ry, 22, 16, "...")) { auto& kvr = o.kv[i]; openTilePicker(t, [&kvr, this](Tile nt) { kvr.second = nt.valid() ? tileText(nt) : ""; mapDirty = true; }, o.type == "npc" || key == "partysprite" ? 'c' : 'p'); } }
            if (flagKey || condKey) { auto* kvp = &o.kv[i]; bool app = condKey;
                flagPicker(px + 88 + vw2 + 4, ry, 26, app, [kvp, app, this](const std::string& f) { kvp->second = app && !trim(kvp->second).empty() ? trim(kvp->second) + "&" + f : f; mapDirty = true; }, "f"); }
            if (needsKey) { auto* kvp = &o.kv[i];
                if (ui.button(px + 88 + vw2 + 4, ry, 26, 16, "+")) { std::vector<std::string> ids = obstacleNeedChoices(), labels; for (auto& n : ids) labels.push_back(obstacleNeedLabel(n));
                    openListPicker("What clears it (added with |)", labels, 0, [this, kvp, ids](int j) { std::string v = ids[j]; std::string cur = trim(kvp->second);
                        if (v == "item:" || v == "spell:") { openTextPrompt(v == "item:" ? "Item id that clears it (used up)" : "Spell utility that clears it", "", [this, kvp, v](const std::string& t) { if (trim(t).empty()) return; kvp->second = trim(kvp->second).empty() ? v + trim(t) : trim(kvp->second) + "|" + v + trim(t); mapDirty = true; }); return; }
                        kvp->second = cur.empty() || cur == "rubble" ? v : cur + "|" + v; mapDirty = true; }); }
                if (ui.hover(px + 88 + vw2 + 4, ry, 26, 16)) ui.tip("Add a way to clear it: rubble, burn, freeze, shock, emp, light, force:N, spell:, item:, flag: (right-click the name: guide)"); }
            if (actKey) { auto* kvp = &o.kv[i];
                if (ui.button(px + 88 + vw2 + 4, ry, 26, 16, "+")) { std::vector<std::string> ids, labels; actionChoices(ids, labels);
                    openListPicker("Action (added with ;) - scripts show their arguments", labels, 0, [this, kvp, ids](int j) { kvp->second = trim(kvp->second).empty() ? ids[j] : trim(kvp->second) + ";" + ids[j]; mapDirty = true; }); }
                if (ui.hover(px + 88 + vw2 + 4, ry, 26, 16)) ui.tip("Add an action or a script (with its argument names filled in for you to replace)"); }
            if (ui.button(px + pw - 22, ry, 16, 16, "x", true, false, DANGER)) { o.kv.erase(o.kv.begin() + i); mapDirty = true; ui.blur(); break; }
            if (ui.hover(px + pw - 22, ry, 16, 16) && (tsec || ro.type == "gather")) ui.tip("Remove this key (a copy then takes it from its template / resource type again)");
            ry += 20;
        }
        if (!inh.empty()) {   // keys it takes from its template / resource type
            if (shown(ry)) ui.text(px + 6, ry + 2, "from " + from + ":", DIM);
            ry += 16;
            for (auto& kv : inh) {
                if (shown(ry)) {
                    ui.label(px + 22, ry + 2, kv.first);
                    std::string v = kv.second.empty() ? "(empty)" : kv.second; while (!v.empty() && font->width(v) > pw - 150) v.pop_back();
                    ui.text(px + 88, ry + 2, v, DIM);
                    if (ui.button(px + pw - 50, ry, 44, 16, "own")) { mapPushUndo(); o.set(kv.first, kv.second); mapDirty = true; }
                    if (ui.hover(px + pw - 50, ry, 44, 16)) ui.tip("Give only this object its own " + kv.first + " (then edit it above)");
                }
                ry += 20;
            }
        }
        static std::string newk;
        if (shown(ry)) {
            ui.textField(px + 6, ry, 110, newk, 30);
            if (ui.button(px + 120, ry, 40, 16, "Add", !newk.empty())) { o.set(trim(newk), ""); newk.clear(); mapDirty = true; }
            if (ui.button(px + 166, ry, 60, 16, "Delete", true, false, DANGER)) { mapPushUndo(); em.objs.erase(em.objs.begin() + objSel); objSel = -1; mapDirty = true; gone = true; }
        } else ui.idCounter += 1;
        ry += 22;
        if (!gone && shown(ry)) {
            if (ro.type == "npc") { if (ui.button(px + 6, ry, 110, 16, "own dialogue")) { openListPicker("Dialogue for this placement only (the NPC's own is set in the NPCs tab)", dlgFiles, 0, [&o, this](int i) { o.set("dialogue", dlgFiles[i]); mapDirty = true; }); }
                if (ui.hover(px + 6, ry, 110, 16)) ui.tip("Give only this placement a dialogue of its own (the NPC keeps its dialogue everywhere else)");
                if (trim(o.get("npc")).empty() && ui.button(px + pw - 110, ry, 104, 16, "make NPC", true, false, ACCENT2)) {   // 1.1.9: turn a hand-made npc object into an NPC
                    mapPushUndo(); EdObj src = ro; std::string nid = newNpc(&src);
                    for (size_t k2 = 0; k2 < o.kv.size(); k2++) if (o.kv[k2].first != "id" && o.kv[k2].first != "template" && o.kv[k2].first != "x" && o.kv[k2].first != "y" && o.kv[k2].first != "show" && o.kv[k2].first != "hide" && o.kv[k2].first != "moveif" && o.kv[k2].first != "movex" && o.kv[k2].first != "movey") { o.kv.erase(o.kv.begin() + k2); k2--; }
                    o.kv.insert(o.kv.begin() + (o.has("id") ? 1 : 0), {"npc", nid}); mapDirty = true; say("NPC '" + nid + "' made from this object (NPCs tab) and linked to it. Save the map."); }
                if (trim(o.get("npc")).empty() && ui.hover(px + pw - 110, ry, 104, 16)) ui.tip("Move this object's name, look and dialogue into a new NPC (NPCs tab) and link it"); }
            if (ro.type == "exit") { if (ui.button(px + 6, ry, 110, 16, "pick map")) { std::vector<std::string> ms = mapFiles; ms.insert(ms.begin(), "overworld"); for (auto& g : gens[7].secs) ms.push_back(g.get("id")); openListPicker("Target map", ms, 0, [&o, ms, this](int i) { o.set("to", ms[i]); mapDirty = true; }); } }
            if (ro.type == "enemy") { if (ui.button(px + 6, ry, 110, 16, "add creature")) { std::vector<std::string> cs; for (auto& c : gd->creatures) cs.push_back(c.first); openListPicker("Creature", cs, 0, [&o, cs, this](int i) { std::string g = o.get("group"); o.set("group", g.empty() ? cs[i] : g + "," + cs[i]); mapDirty = true; }); } }
            if (ro.type == "chest") { if (ui.button(px + 6, ry, 110, 16, "add item")) { std::vector<std::string> is; for (auto& c : gd->items) is.push_back(c.first); openListPicker("Item", is, 0, [&o, is, this](int i) { std::string g = o.get("items"); o.set("items", g.empty() ? is[i] + ":1" : g + "," + is[i] + ":1"); mapDirty = true; }); } }
            if (ro.type == "gather") { if (ui.button(px + 6, ry, 110, 16, "pick node type")) { std::vector<std::string> ids; int cur = 0; for (auto& s : gens[16].secs) { if (s.get("id") == ro.get("node")) cur = (int)ids.size(); ids.push_back(s.get("id")); }
                if (ids.empty()) say("No resource node types yet - make them in the Resources tab.");
                else openListPicker("Resource node type (Resources tab)", ids, cur, [&o, ids, this](int i) { mapPushUndo(); o.set("node", ids[i]); o.erase("item"); mapDirty = true; }); }
                if (ui.hover(px + 6, ry, 110, 16)) ui.tip("What kind of node this is: it takes item, amount, tool, look... from the Resources tab"); }
            if (tmplId.empty() && ro.type != "npc") { if (ui.button(px + pw - 110, ry, 104, 16, "make template")) templateFromObject(o, em.id, objSel);
                if (ui.hover(px + pw - 110, ry, 104, 16)) ui.tip("Turn this object into a template (Objects tab) and link it, so copies can share its values"); }
        }
        ui.cv->resetClip();
    }
}

// ---------------- world ----------------
void Editor::drawWorld(InputState& in) {
    GenericEditor& g = gens[8];
    int x = 4, y = TOP + 4, h = H - TOP - BOTTOM - 8;
    const int S = 2; int mw = 160 * S, mh = 140 * S;
    // live preview: rebuilt whenever the locations (saved or not) or the overworld map change - including unsaved
    // painting of maps/overworld.txt in the Maps tab - so nothing needs a save / close / reopen to show up here
    bool openOW = mapLoaded && em.id == "overworld";
    std::string owFile = readPath("maps/overworld.txt");
    size_t sig = std::hash<std::string>()(g.toText()) * 31 + (size_t)worldSeed;
    if (openOW) { std::hash<std::string> hs; size_t hm = (size_t)em.w * 7919 + em.h; for (auto& c : em.cells) hm = hm * 1000003u ^ hs(c); for (auto& kv : em.header) hm = hm * 31 + hs(kv.first + "=" + kv.second); sig ^= hm + 0x9e3779b9; }
    else if (fileExists(owFile)) { std::error_code ec; auto t = fs::last_write_time(owFile, ec); sig ^= (size_t)t.time_since_epoch().count() * 17 + 1; }
    if (worldNeedsGen || sig != worldSig) {
        std::string mapText; if (openOW) mapText = em.toText(); else readFile(owFile, mapText);
        delete worldPreview; worldPreview = new Map(); buildOverworldText(*worldPreview, worldSeed, mapText, g.toText()); worldNeedsGen = false; worldSig = sig;
    }
    bool painted = openOW || fileExists(owFile);
    ui.panel(x, y, mw + 8, h); ui.header(x, y, mw + 8, "Overworld preview - click to move the selected location"); helpBtn(x + mw - 8, y + 1, "tab.world");
    int ax = x + 4, ay = y + 20;
    auto& kinds = allTileKinds();
    for (int yy = 0; yy < worldPreview->h; yy++) for (int xx = 0; xx < worldPreview->w; xx++) {
        const Cell& c = worldPreview->at(xx, yy); const TileKind& k = kinds[c.kind];
        Color8 col = k.bg; if (k.name == "road" || k.name == "bridge") col = RGB(210, 190, 150); else if (!k.walkable && !k.water) col = RGB(col.r / 2 + 60, col.g / 2 + 60, col.b / 2 + 60);
        ui.cv->fillRect(ax + xx * S, ay + yy * S, S, S, col);
    }
    for (size_t i = 0; i < g.secs.size(); i++) {
        int lx = atoi(g.secs[i].get("x").c_str()), ly = atoi(g.secs[i].get("y").c_str());
        bool sel = (int)i == g.sel; int px = ax + lx * S, py = ay + ly * S;
        ui.cv->fillRect(px - 2, py - 2, 6, 6, sel ? C::Yellow : C::Red); ui.cv->rect(px - 3, py - 3, 8, 8, C::Black);
        if (sel || ui.hover(px - 4, py - 4, 10, 10)) { std::string nm = g.secs[i].get("name"); font->drawShadow(*ui.cv, *ts, px + 7, py - 6, nm, C::White, C::Black); if (ui.clicked(px - 4, py - 4, 10, 10)) g.sel = (int)i; }
    }
    if (ui.hover(ax, ay, mw, mh) && in.mousePressed[0] && !ui.mouseUsed && g.sel >= 0) {
        int tx = (in.mouseX - ax) / S, ty = (in.mouseY - ay) / S;
        g.secs[g.sel].set("x", std::to_string(tx)); g.secs[g.sel].set("y", std::to_string(ty)); g.dirty = true; ui.mouseUsed = true;
    }
    int by = ay + mh + 8;
    helpBtn(ax + 306, by + 24, "world.bake", "tab.world");
    if (ui.button(ax, by, 150, 20, "Save locations", true, false, OK)) { if (g.save()) { changed = true; say("Saved overworld.txt"); } worldNeedsGen = true; }
    if (painted) { if (ui.button(ax + 156, by, 160, 20, "Paint in Maps tab")) { refreshFiles(); for (size_t i = 0; i < mapFiles.size(); i++) if (mapFiles[i] == "overworld") { mapListSel = (int)i; loadMapFile("overworld"); } tab = 0; } }
    else if (ui.button(ax + 156, by, 160, 20, "Bake to paintable map", true, false, ACCENT2)) openConfirm("Generate the overworld terrain from the seed and the locations, and save it as maps/overworld.txt so you can paint it by hand? (Locations stay here and are placed on top when the map loads.)", [this]() { bakeOverworld(); });
    font->drawWrapped(*ui.cv, *ts, ax, by + 28, mw, painted ? "The overworld is a hand-made map (maps/overworld.txt): paint terrain, monsters and chests in the Maps tab. Locations here are stamped on top when it loads: 'map' is the map entered, 'tx,ty' the arrival tile (0,0 = its spawn), 'kind' the marker tile (loc_*). Roads and clearings must be painted." : "Terrain is generated from the world seed; roads, clearings and entrances come from this location list. Bake it to a map to paint it by hand. 'map' = map entered, 'tx,ty' = arrival tile (0,0 = spawn), 'road' = location ids to connect, 'kind' = marker tile (loc_*).", DIM);
    drawGeneric(g, x + mw + 14, y, W - (x + mw + 14) - 4, h, in);
}

// ---------------- dialogue ----------------
void Editor::drawDialogue(InputState& in) {
    int x = 4, y = TOP + 4, h = H - TOP - BOTTOM - 8, lw = 200;
    liveCheck(te, 0);   // 1.1.11: gutter dots for what Check finds, a moment after each change
    if (te.wide && !te.path.empty()) {   // 1.1.11 (#35): Wide - the open file across the whole window
        int ex = 4, ew = W - 8;
        ui.panel(ex, y, ew, h); ui.header(ex, y, ew, fs::path(te.path).filename().string() + (te.dirty ? " *" : "") + "   (Wide: Ctrl+E or Narrow goes back)");
        int bx = ex + ew - 4;
        bx -= 58; if (ui.button(bx, y + 1, 56, 14, "Narrow", true, true)) { te.wide = false; ui.blur(); }
        bx -= 48; if (ui.button(bx, y + 1, 44, 14, "Graph", true, dlgGraphMode)) { dlgGraphMode = true; ui.blur(); }
        bx -= 40; if (ui.button(bx, y + 1, 36, 14, "Text", true, !dlgGraphMode)) { dlgGraphMode = false; ui.blur(); }
        bx -= 54; if (ui.button(bx, y + 1, 50, 14, "Check", true, false, ACCENT2)) { lint = lintDialogue(te.text(), fs::path(te.path).stem().string()); lintShown = true; lintScroll = 0; lintTitle = fs::path(te.path).stem().string(); te.checkedHash = 0; te.editT = -10; liveCheck(te, 0); say(lint.empty() ? "Check: no problems found." : fmt("Check: %d finding(s) - dots in the margin, Narrow for the list.", (int)lint.size())); }
        bx -= 50; if (ui.button(bx, y + 1, 46, 14, "Save", te.dirty, false, OK)) { if (te.save()) { changed = true; say("Saved " + te.path); } }
        if (dlgGraphMode) drawDialogueGraph(ex + 2, y + 18, ew - 4, h - 20, in);
        else { if (dlgGFocusText) { ui.focus = ui.idCounter + 1; dlgGFocusText = false; } textEditWidget(te, ex + 2, y + 18, ew - 4, h - 20, in); }
        return;
    }
    ui.panel(x, y, lw, h); listHeader(x, y, lw, fmt("Dialogue (%d)", (int)dlgFiles.size()), IMP_DLG); helpBtn(x + lw - 16, y + 1, "tab.dialogue");
    ui.searchField(x + 4, y + 20, lw - 8, dlgFilter, "search names and text...");
    if (dlgFilter != dlgFilterLast || dlgFilterN != dlgFiles.size()) { dlgFilterLast = dlgFilter; dlgFilterN = dlgFiles.size(); dlgFilterHits.clear(); std::string t; if (!dlgFilter.empty()) for (auto& f : dlgFiles) if (readFile(readPath("dialogue/" + f + ".txt"), t) && edMatch(f + " " + t, dlgFilter)) dlgFilterHits.insert(f); }   // searches the file names and what is said in them
    std::vector<std::string> dl; std::vector<int> dIdx;
    for (size_t i = 0; i < dlgFiles.size(); i++) { std::string n = dlgFiles[i]; if (!dlgFilter.empty() && !dlgFilterHits.count(n)) continue; if (dataRoot != baseRoot && !fileExists(dataRoot + "/dialogue/" + n + ".txt")) n += "  (base)"; dl.push_back(n); dIdx.push_back((int)i); }
    int dSel = -1; for (size_t i = 0; i < dIdx.size(); i++) if (dIdx[i] == dlgSel) dSel = (int)i;
    int hit = ui.listBox(x + 4, y + 38, lw - 8, h - 98, dl, dSel, dlgScroll);
    if (dlgFiles.empty()) ui.text(x + 10, y + 42, "No dialogue yet: New or Import.", DIM);
    if (hit >= 0) { hit = dIdx[hit]; dlgSel = hit; }
    if (hit >= 0) { if (te.dirty) { int hh = hit; openConfirm("Discard unsaved changes?", [this, hh]() { te.load(readPath("dialogue/" + dlgFiles[hh] + ".txt")); te.path = dataRoot + "/dialogue/" + dlgFiles[hh] + ".txt"; }); } else { te.load(readPath("dialogue/" + dlgFiles[hit] + ".txt")); te.path = dataRoot + "/dialogue/" + dlgFiles[hit] + ".txt"; } }
    int by = y + h - 54;
    if (ui.button(x + 4, by, 50, 20, "New")) openTextPrompt("New dialogue id", "npc_name", [this](const std::string& nm) { std::string p = dataRoot + "/dialogue/" + nm + ".txt"; if (!fileExists(p)) writeFile(p, "[start]\nspeaker=" + nm + "\ntext=Hello, $hero.\noption=Goodbye.|end\n"); refreshFiles(); te.load(p); for (size_t i = 0; i < dlgFiles.size(); i++) if (dlgFiles[i] == nm) dlgSel = (int)i; });
    if (ui.button(x + 60, by, 50, 20, "Save", te.dirty && !te.path.empty(), false, OK)) { if (te.save()) { changed = true; say("Saved " + te.path); } }
    if (ui.button(x + 116, by, 60, 20, "Delete", !te.path.empty(), false, DANGER)) openConfirm("Delete this dialogue file?", [this]() { remove(te.path.c_str()); te.clearKeepView(); refreshFiles(); dlgSel = -1; });
    if (ui.button(x + 4, by + 24, 60, 20, "Check", !te.path.empty(), false, ACCENT2)) { lint = lintDialogue(te.text(), fs::path(te.path).stem().string()); lintShown = true; lintScroll = 0; lintTitle = fs::path(te.path).stem().string(); say(lint.empty() ? "Check: no problems found." : fmt("Check: %d finding(s).", (int)lint.size())); }
    if (ui.button(x + 68, by + 24, 76, 20, "Check all", true, false, ACCENT2)) {
        lint.clear(); int files = 0; std::string txt;
        for (auto& f : dlgFiles) if (readFile(readPath("dialogue/" + f + ".txt"), txt)) { files++; for (auto& it : lintDialogue(txt, f)) { it.msg = f + ": " + it.msg; it.line = -1; lint.push_back(it); } }
        std::stable_sort(lint.begin(), lint.end(), [](const LintItem& a, const LintItem& b) { return a.warn < b.warn; });
        lintShown = true; lintScroll = 0; lintTitle = fmt("all %d files", files); say(fmt("Checked %d files: %d finding(s).", files, (int)lint.size()));
    }
    if (lintShown && ui.button(x + 148, by + 24, 48, 20, "Hide")) lintShown = false;
    if (!lintShown) helpBtn(x + 152, by + 28, "dlg.check", "tab.dialogue");
    int ex = x + lw + 6, ew = W - ex - 4 - 290;
    ui.panel(ex, y, ew, h); ui.header(ex, y, ew, te.path.empty() ? "no file open" : fs::path(te.path).filename().string() + (te.dirty ? " *" : ""));
    // Text | Graph toggle (the graph is a view of te.lines; editor_graph.cpp)
    helpBtn(ex + ew - 100, y + 1, "dlg.graph");
    if (!te.path.empty()) { if (ui.button(ex + ew - 148, y + 1, 44, 14, "Wide")) { te.wide = true; ui.blur(); } if (ui.hover(ex + ew - 148, y + 1, 44, 14)) ui.tip("Make the editor as wide as the window (Ctrl+E)"); }
    if (ui.button(ex + ew - 84, y + 1, 36, 14, "Text", true, !dlgGraphMode)) { dlgGraphMode = false; ui.blur(); }
    if (ui.button(ex + ew - 46, y + 1, 44, 14, "Graph", true, dlgGraphMode)) { dlgGraphMode = true; ui.blur(); }
    if (ui.hover(ex + ew - 84, y + 1, 82, 14)) ui.tip("Edit as text, or see the file as a node graph");
    if (dlgGraphMode) drawDialogueGraph(ex + 2, y + 18, ew - 4, h - 20, in);
    else if (!te.path.empty()) { if (dlgGFocusText) { ui.focus = ui.idCounter + 1; dlgGFocusText = false; } textEditWidget(te, ex + 2, y + 18, ew - 4, h - 20, in); }
    // reference
    int rx = ex + ew + 4, rw = W - rx - 4;
    ui.panel(rx, y, rw, h);
    if (lintShown) {
        int errs = 0; for (auto& l : lint) if (!l.warn) errs++;
        ui.header(rx, y, rw, fmt("Check: %s - %d error(s), %d warning(s)", lintTitle.c_str(), errs, (int)lint.size() - errs));
        int rowH = 26, rows = (h - 40) / rowH;
        if (ui.hover(rx, y + 18, rw, h - 18) && in.wheel) lintScroll -= in.wheel * 2;
        lintScroll = clampi(lintScroll, 0, std::max(0, (int)lint.size() - rows));
        if (lint.empty()) ui.text(rx + 8, y + 24, "No problems found.", OK);
        for (int r = 0; r < rows && lintScroll + r < (int)lint.size(); r++) {
            auto& it = lint[lintScroll + r]; int ry = y + 20 + r * rowH;
            bool hv = ui.hover(rx + 2, ry, rw - 4, rowH) && it.line >= 0;
            if (hv) ui.cv->fillRect(rx + 2, ry, rw - 4, rowH, HOVER);
            std::string head = (it.line >= 0 ? fmt("line %d: ", it.line + 1) : std::string()) + (it.warn ? "warning" : "ERROR");
            font->draw(*ui.cv, *ts, rx + 6, ry + 1, head, it.warn ? ACCENT2 : DANGER);
            auto wl = font->wrap(it.msg, rw - 12); font->draw(*ui.cv, *ts, rx + 6, ry + 13, wl.empty() ? "" : wl[0], TEXT);
            if (hv && in.mousePressed[0] && !ui.mouseUsed) { ui.mouseUsed = true; dlgGraphMode = false; te.cy = clampi(it.line, 0, (int)te.lines.size() - 1); te.cx = 0; te.ax = te.ay = -1; te.scroll = std::max(0, te.cy - 6); }
        }
        ui.text(rx + 6, y + h - 16, "click a finding to jump to its line", DIM);
        return;
    }
    if (!te.path.empty()) drawUsedBy("dialogue:" + fs::path(te.path).stem().string(), rx + 2, y + h - 150, rw - 4, 146, in);
    ui.header(rx, y, rw, "Syntax"); helpBtn(rx + rw - 64, y + 1, "dlg.nodes", "tab.dialogue"); helpBtn(rx + rw - 48, y + 1, "dlg.options", "tab.dialogue"); helpBtn(rx + rw - 32, y + 1, "concepts.conditions", "tab.dialogue"); helpBtn(rx + rw - 16, y + 1, "dlg.actions", "tab.dialogue");
    static const char* ref[] = {"script=id  (runs a script, Scripts tab)", "[node]  (first node is 'start')", "speaker=Name   ($hero = hero's name)", "text=Line  (repeat for more lines)", "option=Label|target|condition", "  target 'end' closes the dialogue", "[camp] = companion talk (Party menu, T)", "  conditions (join with &): flag, !flag,", "  item:id, gold:n, level:n, party:class,", "  flag>=n, kills:creature:n, itemn:id:n,", "  approval:class:n  stat:cha>=12  stealthy", "next=node  (auto-advance, no options)", "", "Actions (run when the node shows):", "set=flag=1,other    flag=name    inc=name", "clear=flag   give=item:n   take=item:n", "gold=n (negative = pay)   xp=n", "heal=1 (free full heal)   rest=1 (inn)", "healer=1 (paid healer services)", "respec=1   shop=shop_id", "join=npc_id  leave=class  teach=bram:sp|sp2", "fell=class|text  (gone for good)", "combat=creature,creature  fight=group_flag", "teleport=map,x,y   message=Title|Text", "cutscene=line|line   dialogue=other_id", "spawn=creature,x,y   hidenpc=id  shownpc=id", "notify=text   quest=Name   win=line|line", "revealmap=map  or  map,x,y,radius", "approve=class:n   craft=forge|alchemy|workbench", "rep=faction:n  (town reputation, + or -)", "  condition rep:faction:n = at least n", "board=town_id (notice board)", "", "Hook a file to an NPC with dialogue=<file>", "in the Maps tab (object panel)."};
    for (size_t i = 0; i < sizeof(ref) / sizeof(ref[0]); i++) font->draw(*ui.cv, *ts, rx + 6, y + 22 + (int)i * 13, ref[i], i == 11 ? ACCENT2 : (ref[i][0] == ' ' ? DIM : TEXT));
}

