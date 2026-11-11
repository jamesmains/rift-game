#include "editor.h"
#include "../game/tiles.h"
#include <filesystem>
#include <algorithm>
#include <set>
#include <cstring>

namespace fs = std::filesystem;
using namespace ED;

static const char* TABS[] = {"Maps", "Tiles", "World", "Items", "Creatures", "Spells", "Classes", "Shops", "Encounters", "Quests", "Dungeons", "Dialogue", "Sounds", "Tileset", "Recipes", "Affixes", "Banter", "Achiev.", "Campaign", "Flags", "Bounties", "Factions"};
static const int NTABS = 22;
int gEdTop = 28;                      // tab bar height: 28 for one row of tabs, 50 when they need two rows
#define TOP gEdTop
static const int BOTTOM = 18;

// ============================================================ GenericEditor
bool GenericEditor::load() {
    undo.clear(); redo.clear(); shadowInit = false;
    secs.clear(); std::string text; dirty = false; status.clear();
    bool own = readFile(path, text);
    if (own) for (auto& s : parseConfig(text)) { if (s.name.empty()) continue; EdSection e; e.name = s.name; e.kv = s.kv; secs.push_back(e); }
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
    std::string o = "# " + title + " - edited with the Editor\n\n";
    for (auto& s : secs) { if (s.inherited && !replaceWhole) continue; o += "[" + s.name + "]\n"; for (auto& kv : s.kv) o += kv.first + "=" + kv.second + "\n"; o += "\n"; }
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
std::vector<int> GenericEditor::filtered() const {
    std::vector<int> out; std::string f = toLower(filter);
    for (size_t i = 0; i < secs.size(); i++) { std::string lbl = toLower(secs[i].get(listKey) + " " + secs[i].get("name")); if (f.empty() || lbl.find(f) != std::string::npos) out.push_back((int)i); }
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
void TextEdit::load(const std::string& p) { path = p; lines.clear(); std::string t; readFile(p, t); for (auto& l : split(t, '\n')) { std::string s = l; while (!s.empty() && s.back() == '\r') s.pop_back(); lines.push_back(s); } if (lines.empty()) lines.push_back(""); cx = cy = scroll = 0; dirty = false; }
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
    auto mk = [&](const std::string& file, const std::string& title, const std::string& section, const std::string& listKey, std::vector<std::string> tmpl) { GenericEditor ge; ge.path = dataRoot + "/" + file; ge.title = title; ge.sectionName = section; ge.listKey = listKey; ge.templateKeys = tmpl; gens.push_back(ge); };
    mk("items.txt", "Items", "item", "id", {"id", "name", "type", "tile", "value", "desc", "dmg", "dmgtype", "range", "ap", "stat", "def", "evasion", "res", "bonus", "heal", "mana", "effect", "light", "salvage", "tech", "sound", "category", "quest"});
    mk("creatures.txt", "Creatures", "creature", "id", {"id", "name", "sprite", "hp", "stats", "dmg", "dmgtype", "range", "apmax", "move", "def", "evasion", "res", "ai", "spells", "xp", "level", "gold", "loot", "faction", "size", "darkvision", "undead", "mechanical", "sound", "attacksound"});
    mk("spells.txt", "Spells", "spell", "id", {"id", "name", "desc", "tile", "cost", "ap", "range", "area", "dmg", "dmgtype", "stat", "heal", "effect", "duration", "target", "utility", "ff", "level", "sound"});
    mk("classes.txt", "Classes", "class", "id", {"id", "name", "desc", "role", "stats", "hd", "md", "weapon", "armor", "items", "spells", "spriterow", "spritecol", "learn"});
    mk("shops.txt", "Shops", "shop", "id", {"id", "hours", "faction", "item"});
    mk("encounters.txt", "Encounter tables", "table", "id", {"id", "level", "group"});
    mk("quests.txt", "Quests", "quest", "id", {"id", "name", "stage"});
    mk("dungeons.txt", "Dungeons", "dungeon", "id", {"id", "name", "style", "w", "h", "seed", "level", "exit", "monsters", "boss", "bossflag", "bossdefeat", "loot", "music", "table"});
    mk("overworld.txt", "Overworld locations", "location", "id", {"id", "name", "x", "y", "kind", "map", "tx", "ty", "clear", "road", "require", "locked", "reveal"});
    mk("recipes.txt", "Recipes", "recipe", "id", {"id", "name", "result", "count", "station", "materials", "gold", "level", "difficulty", "known", "reveal"});          // gens[9]
    mk("affixes.txt", "Affixes", "affix", "id", {"id", "name", "pos", "tier", "types", "mods", "dmgtype", "value"});                   // gens[10]
    mk("banter.txt", "Banter", "banter", "class", {"class", "when", "map", "if", "flag", "text"});                                    // gens[11]
    mk("achievements.txt", "Achievements", "achievement", "id", {"id", "name", "desc", "cond", "tile", "hidden"});                   // gens[12]
    mk("bounties.txt", "Notice boards & bounties", "board", "id", {"id", "name", "map", "location", "offers", "kinds", "kill", "creatures", "fetch", "deliver", "reward"});   // gens[13]
    mk("factions.txt", "Factions (reputation)", "faction", "id", {"id", "name", "text"});                                          // gens[14]
    gens[13].altSection = "bounty"; gens[13].altTemplateKeys = {"id", "board", "kind", "target", "need", "where", "reward", "xp", "title", "text", "require", "repeat"};
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
    // the campaign's own files first, then the ones it inherits from the base game
    auto ownFirst = [&](std::vector<std::string>& v, const std::string& sub) { std::sort(v.begin(), v.end()); if (dataRoot == baseRoot) return; std::stable_partition(v.begin(), v.end(), [&](const std::string& n) { return fileExists(dataRoot + "/" + sub + "/" + n + ".txt"); }); };
    ownFirst(mapFiles, "maps"); ownFirst(dlgFiles, "dialogue");
}
void Editor::setCampaign(const std::string& id) {
    if (onSwitchCampaign && !onSwitchCampaign(id)) { say("Could not load campaign " + id); return; }
    auto all = listCampaigns(assetDir); const CampaignInfo* c = nullptr; for (auto& x : all) if (x.id == id) c = &x;
    if (!c) return;
    campaignId = c->id; campaignInherit = c->inherit; dataRoot = c->dir.empty() ? baseRoot : c->dir;
    for (auto& g : gens) {
        std::string file = fs::path(g.path).filename().string();
        g.path = dataRoot + "/" + file;
        g.basePath.clear();   // campaigns are edited on their own, never mixed with the base game
        g.replaceWhole = file == "overworld.txt";
        g.load(); g.sel = -1; g.scroll = 0; g.filter.clear();
    }
    mapLoaded = false; mapDirty = false; te = TextEdit(); refreshFiles(); worldNeedsGen = true; mapListScroll = 0; tab = 0;
    loadCampaignForm(); loadPlayState(); loadFlagDocs(); flagIndexDirty = true; mapScan.clear(); flagSelName.clear(); questPreview.clear();
    // open the campaign's own starting map (its own files are listed first)
    std::string first = c->startMap; if (std::find(mapFiles.begin(), mapFiles.end(), first) == mapFiles.end() && !mapFiles.empty()) first = mapFiles[0];
    for (size_t i = 0; i < mapFiles.size(); i++) if (mapFiles[i] == first) { mapListSel = (int)i; loadMapFile(first); }
    say("Editing campaign: " + c->name + (c->dir.empty() ? " (the base game)" : " - " + c->dir));
}
// A new campaign folder with a starter map, an intro dialogue and a quest, ready to play with Play > or from its desktop icon.
void Editor::newCampaign(const std::string& name0) {
    std::string name = trim(name0); if (name.empty()) return;
    std::string id; for (char ch : toLower(name)) { if (isalnum((unsigned char)ch)) id += ch; else if (!id.empty() && id.back() != '_') id += '_'; }
    while (!id.empty() && id.back() == '_') id.pop_back(); if (id.empty() || id == "triffall") id = "campaign";
    std::string dir = assetDir + "/campaigns/" + id; std::error_code ec;
    if (fs::exists(dir)) { say("A campaign folder '" + id + "' already exists."); return; }
    fs::create_directories(dir + "/maps", ec); fs::create_directories(dir + "/dialogue", ec);
    writeFile(dir + "/campaign.txt", "# Campaign settings - edit them in the Editor's Campaign tab. This campaign is a separate game: it only uses the\n# files in this folder (tiles, sprites and sounds are shared by every campaign).\n[campaign]\nname=" + name + "\ndesc=A new adventure.\nauthor=\nicon=0,7\nstart=" + id + "_start,6,9\ngold=50\nintro=A new story begins.\nintro=$hero sets out at first light.\ndialogue=" + id + "_intro\n");
    // a starter library so the campaign is playable from the first minute: the base game's classes, spells, items,
    // monsters, recipes and affixes are copied in (they are the campaign's own from now on - edit or delete freely)
    for (const char* f : {"classes.txt", "spells.txt", "items.txt", "creatures.txt", "recipes.txt", "affixes.txt"}) {
        std::string t; if (readFile(baseRoot + "/" + f, t)) writeFile(dir + "/" + f, "# Copied from the base game when the campaign was created - this copy belongs to the campaign.\n" + t);
    }
    writeFile(dir + "/shops.txt", "[shop]\nid=" + id + "_store\nitem=bread\nitem=potion_red\nitem=antidote\nitem=torch_item\nitem=arrows\nitem=iron_dagger\nitem=leather_armor\n");
    std::string m = "[map]\nid=" + id + "_start\nname=" + name + "\nkind=town\nspawn=6,9\nbg=20,40,20\n\n[legend]\n. = floor_grass\nT = tree|tree2\nr = road\n, = flower|floor_grass\n\n[rows]\n";
    for (int y = 0; y < 18; y++) { std::string row; for (int x = 0; x < 28; x++) row += (x == 0 || y == 0 || x == 27 || y == 17) ? (y == 9 && x == 27 ? 'r' : 'T') : (y == 9 ? 'r' : ((x * 7 + y * 3) % 11 == 0 ? ',' : '.')); m += row + "\n"; }
    m += "\n[sign]\nx=10\ny=8\ntext=Welcome to " + name + ". Paint this map in the Editor (Maps tab), add NPCs and exits, then press Play >.\n\n[npc]\nid=guide\nname=Guide\nsprite=104,0\nx=13\ny=10\ndialogue=" + id + "_intro\n\n[savepoint]\nx=7\ny=11\n";
    writeFile(dir + "/maps/" + id + "_start.txt", m);
    writeFile(dir + "/dialogue/" + id + "_intro.txt", "[start]\nspeaker=Guide\ntext=Welcome, $hero. This is the first page of a new story - every word of it can be rewritten in the Editor's Dialogue tab.\noption=Where do I start?|quest|!" + id + "_started\noption=Goodbye.|end\n\n[quest]\nspeaker=Guide\ntext=Anywhere you like. Here is a quest flag to get you going.\nset=" + id + "_started\nquest=First steps\noption=Thanks.|end\n");
    writeFile(dir + "/quests.txt", "[quest]\nid=" + id + "_main\nname=" + name + "\nstage=" + id + "_started | The story has begun. (Edit quests.txt to write the rest.)\n");
    setCampaign(id);
    say("Created campaign '" + name + "' in " + dir + ". New games start at the yellow START tile (B over a tile moves it; the Campaign tab has the rest).");
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
void Editor::openTilePicker(Tile current, std::function<void(Tile)> cb) { popOpenedFrame = frameNo; pop = P_TILE; popTile = cb; popCur = current; popTitle = "Pick a tile (click)"; if (current.valid()) { tileScrollX = std::max(0, current.c * 26 - 400); tileScrollY = std::max(0, current.r * 26 - 200); } ui.modal = true; ui.blur(); }
void Editor::openListPicker(const std::string& title, const std::vector<std::string>& items, int cur, std::function<void(int)> cb) { popOpenedFrame = frameNo; pop = P_LIST; popItems = items; popList = cb; popSel = cur; popScroll = std::max(0, cur - 5); popTitle = title; popValue.clear(); ui.modal = true; ui.blur(); }
void Editor::openConfirm(const std::string& title, std::function<void()> yes) { popOpenedFrame = frameNo; pop = P_CONFIRM; popYes = yes; popTitle = title; ui.modal = true; ui.blur(); }
void Editor::openTextPrompt(const std::string& title, const std::string& initial, std::function<void(const std::string&)> cb) { popOpenedFrame = frameNo; pop = P_TEXT; popText = cb; popTitle = title; popValue = initial; ui.modal = true; ui.blur(); }

void Editor::drawPopup(InputState& in) {
    if (pop == P_NONE) return;
    Canvas& cv = *ui.cv;
    cv.dim(0, 0, W, H, 140);
    ui.modal = false; // widgets inside the popup may use the mouse
    if (popOpenedFrame == frameNo) { in.eatMouse(); in.eatKeys(); } // the click/key that opened us must not act inside
    bool close = false;
    if (pop == P_TILE) {
        int x = 20, y = 30, w = W - 40, h = H - 60;
        ui.panel(x, y, w, h); ui.header(x, y, w, popTitle);
        int ax = x + 4, ay = y + 20, aw = w - 8, ah = h - 50;
        int cols = 206, rows = 50, cell = 26;
        if (ui.hover(ax, ay, aw, ah)) { if (in.down[K_SHIFT]) tileScrollX -= in.wheel * 60; else tileScrollY -= in.wheel * 60; }
        if (in.mouseDown[1] || in.mouseDown[2]) { tileScrollX -= in.mouseDX; tileScrollY -= in.mouseDY; }
        if (!ui.typing()) { if (in.down[K_LEFT]) tileScrollX -= 8; if (in.down[K_RIGHT]) tileScrollX += 8; if (in.down[K_UP]) tileScrollY -= 8; if (in.down[K_DOWN]) tileScrollY += 8; }
        tileScrollX = clampi(tileScrollX, 0, cols * cell - aw); tileScrollY = clampi(tileScrollY, 0, rows * cell - ah);
        cv.fillRect(ax, ay, aw, ah, RGB(18, 18, 22)); cv.setClip({ax, ay, aw, ah});
        int c0 = tileScrollX / cell, r0 = tileScrollY / cell;
        for (int r = r0; r <= r0 + ah / cell + 1 && r < rows; r++) for (int c = c0; c <= c0 + aw / cell + 1 && c < cols; c++) {
            int sx = ax + c * cell - tileScrollX, sy = ay + r * cell - tileScrollY;
            ts->drawOpaque(cv, T(c, r), sx + 1, sy + 1, 2);
            if (popCur.valid() && popCur.c == c && popCur.r == r) cv.rect(sx, sy, cell, cell, ACCENT2);
        }
        cv.resetClip();
        int hc = (in.mouseX - ax + tileScrollX) / cell, hr = (in.mouseY - ay + tileScrollY) / cell;
        bool over = ui.hover(ax, ay, aw, ah) && hc >= 0 && hc < cols && hr >= 0 && hr < rows;
        if (over) { int sx = ax + hc * cell - tileScrollX, sy = ay + hr * cell - tileScrollY; cv.rect(sx, sy, cell, cell, C::White); ui.text(x + 8, y + h - 24, fmt("col %d, row %d", hc, hr), ACCENT2); ts->drawOpaque(cv, T(hc, hr), x + 110, y + h - 28, 2); if (in.mousePressed[0]) { Tile t = T(hc, hr); auto cb = popTile; close = true; cb(t); } }
        ui.text(x + 160, y + h - 24, "wheel: scroll  shift+wheel / drag: horizontal  arrows: pan", DIM);
        if (ui.button(x + w - 150, y + h - 28, 60, 20, "None")) { auto cb = popTile; close = true; cb(Tile{}); }
        if (ui.button(x + w - 80, y + h - 28, 70, 20, "Cancel") || in.pressed[K_ESCAPE]) close = true;
    } else if (pop == P_LIST) {
        int w = 360, h = 360, x = (W - w) / 2, y = (H - h) / 2;
        ui.panel(x, y, w, h); ui.header(x, y, w, popTitle);
        ui.label(x + 8, y + 22, "filter:"); ui.idCounter = 6000; if (ui.focus < 6001) { ui.focus = 6001; ui.caret = (int)popValue.size(); } ui.textField(x + 50, y + 20, w - 60, popValue);
        std::vector<std::string> shown; std::vector<int> idx; std::string f = toLower(popValue);
        for (size_t i = 0; i < popItems.size(); i++) if (f.empty() || toLower(popItems[i]).find(f) != std::string::npos) { shown.push_back(popItems[i]); idx.push_back((int)i); }
        int lsel = -1; for (size_t i = 0; i < idx.size(); i++) if (idx[i] == popSel) lsel = (int)i;
        if (in.pressed[K_DOWN] && !shown.empty()) { lsel = std::min((int)shown.size() - 1, lsel + 1); popSel = idx[lsel]; }
        if (in.pressed[K_UP] && !shown.empty()) { lsel = std::max(0, lsel - 1); popSel = idx[lsel]; }
        int hit = ui.listBox(x + 8, y + 42, w - 16, h - 80, shown, lsel, popScroll);
        if (hit < 0 && in.pressed[K_ENTER] && !shown.empty()) hit = lsel >= 0 ? lsel : 0;
        if (hit >= 0) { auto cb = popList; close = true; cb(idx[hit]); }
        ui.text(x + 8, y + h - 24, "type to filter, arrows + Enter or click", DIM);
        if (ui.button(x + w - 80, y + h - 28, 70, 20, "Cancel") || in.pressed[K_ESCAPE]) close = true;
    } else if (pop == P_CONFIRM) {
        int w = 360, h = 110, x = (W - w) / 2, y = (H - h) / 2;
        ui.panel(x, y, w, h); ui.header(x, y, w, "Confirm");
        font->drawWrapped(cv, *ts, x + 10, y + 26, w - 20, popTitle, TEXT);
        if (ui.button(x + w - 170, y + h - 30, 70, 22, "Yes", true, false, OK) || in.pressed[K_ENTER]) { auto cb = popYes; close = true; cb(); }
        if (ui.button(x + w - 90, y + h - 30, 70, 22, "No") || in.pressed[K_ESCAPE]) close = true;
    } else if (pop == P_HELP) {
        drawGuide(in, close);
    } else if (pop == P_REPORT) {
        drawReport(in, close);
    } else if (pop == P_TEXT) {
        int w = 360, h = 100, x = (W - w) / 2, y = (H - h) / 2;
        ui.panel(x, y, w, h); ui.header(x, y, w, popTitle);
        ui.idCounter = 5000; if (ui.focus < 5001) { ui.focus = 5001; ui.caret = (int)popValue.size(); }
        ui.textField(x + 10, y + 30, w - 20, popValue, 60);
        if (ui.button(x + w - 170, y + h - 30, 70, 22, "OK", !popValue.empty(), false, OK) || (in.pressed[K_ENTER] && !popValue.empty())) { auto cb = popText; std::string v = popValue; close = true; cb(v); }
        if (ui.button(x + w - 90, y + h - 30, 70, 22, "Cancel") || in.pressed[K_ESCAPE]) close = true;
    }
    if (close && popOpenedFrame == frameNo) { ui.modal = true; in.eatMouse(); in.eatKeys(); }   // the callback opened the next popup (e.g. New campaign -> name prompt): keep that one
    else if (close) { pop = P_NONE; ui.modal = false; ui.blur(); in.eatMouse(); in.eatKeys(); }
    else ui.modal = true;
}

// ---------------- main ----------------
void Editor::update(InputState& in, float dt, Canvas& cv) {
    if (cv.w != W) cv.init(W, H);
    frameNo++;
    ui.begin(&cv, ts, font, &in, dt);
    ui.modal = pop != P_NONE;
    cv.clear(BG);
    if (toastT > 0) toastT -= dt;
    // tabs
    // tabs flow into a second row when they do not fit beside the Exit button
    { int total = 4; for (int i = 0; i < NTABS; i++) total += font->width(TABS[i]) + 16; gEdTop = total > W - 250 ? 50 : 28; }
    cv.fillRect(0, 0, W, TOP, PANEL);
    int tx = 4, trow = 0;
    for (int i = 0; i < NTABS; i++) {
        int tw = font->width(TABS[i]) + 14;
        if (tx + tw > W - 250 && trow == 0) { trow = 1; tx = 4; }
        bool dirty = (i == 0 && mapDirty) || (i == 1 && kindsDirty) || (i >= 2 && i <= 10 && gens[i == 2 ? 8 : i - 3].dirty) || (i == 11 && te.dirty) || (i == 12 && sndDirty) || (i >= 14 && i <= 17 && gens[i - 5].dirty) || (i == 18 && camp.dirty) || (i == 19 && flagDocsDirty) || (i == 20 && gens[13].dirty) || (i == 21 && gens[14].dirty);
        if (ui.button(tx, 4 + trow * 22, tw, 20, std::string(TABS[i]) + (dirty ? "*" : ""), true, tab == i)) { tab = i; ui.blur(); if (i != 1) kindReturnTab = -1; if (i == 19 || i == 8) flagIndexDirty = true; }
        tx += tw + 2;
    }
    if (tab == 1 && kindReturnTab >= 0 && !ui.typing() && pop == P_NONE && in.pressed[K_ESCAPE]) { tab = kindReturnTab; kindReturnTab = -1; in.eatKeys(); }
    {
        std::string cname = "Triffall"; for (auto& c : listCampaigns(assetDir)) if (c.id == campaignId) cname = c.name;
        if (ui.button(W - 240, 4, 164, 20, "Campaign: " + cname.substr(0, 14), true, campaignId != "triffall", ACCENT2)) {
            auto all = listCampaigns(assetDir); std::vector<std::string> names; int cur = 0;
            for (size_t i = 0; i < all.size(); i++) { names.push_back(all[i].name + (all[i].id == "triffall" ? "  (base game)" : "  [" + all[i].id + "]")); if (all[i].id == campaignId) cur = (int)i; }
            names.push_back("+ New campaign...");
            openListPicker("Campaign to edit", names, cur, [this, all](int i) {
                if (i >= (int)all.size()) { openTextPrompt("Name of the new campaign", "The Lost Lighthouse", [this](const std::string& nm) { newCampaign(nm); }); return; }
                bool any = mapDirty || kindsDirty || te.dirty || camp.dirty || flagDocsDirty; for (auto& g : gens) if (g.dirty) any = true;
                std::string id = all[i].id;
                if (any) openConfirm("Unsaved changes will be lost. Switch campaign anyway?", [this, id]() { setCampaign(id); }); else setCampaign(id);
            });
        }
        if (ui.hover(W - 240, 4, 164, 20)) ui.tip("Each campaign is a separate game in assets/campaigns/<id>/ with its own maps, items, monsters... and its own desktop icon. The Campaign tab sets its name and where new games start.");
    }
    if (ui.button(W - 240, TOP == 50 ? 26 : 4, TOP == 50 ? 164 : 60, 20, TOP == 50 ? "? Guide  (F1)" : "? Guide", true, false, ACCENT)) openGuide(tabTopic());
    if (ui.hover(W - 240, TOP == 50 ? 26 : 4, 164, 20)) ui.tip("How everything in the Editor works. The small ? buttons open it on the matching page.");
    if (in.pressed[K_F1] && pop == P_NONE) openGuide(tabTopic());
    if (ui.button(W - 70, TOP == 50 ? 26 : 4, 64, 20, "Check all", true, false, ACCENT2) && pop == P_NONE) { runCampaignCheck(); popOpenedFrame = frameNo; pop = P_REPORT; ui.modal = true; ui.blur(); }
    if (ui.hover(W - 70, TOP == 50 ? 26 : 4, 64, 20)) ui.tip("Check the whole campaign: maps, dialogue, flags, quests, shops, classes, creatures, recipes, tables...");
    if (ui.button(W - 70, 4, 64, 20, "Exit", true, false, DANGER) || (!ui.typing() && pop == P_NONE && in.pressed[K_ESCAPE] && !mapPanning)) {
        bool any = mapDirty || kindsDirty || te.dirty || sndDirty || camp.dirty || flagDocsDirty; for (auto& g : gens) if (g.dirty) any = true;
        if (any) openConfirm("There are unsaved changes. Exit anyway? (Save each tab with its Save button.)", [this]() { wantsClose = true; });
        else wantsClose = true;
    }
    if (tab != lastTab) { if (tab == 3 || tab == 4 || tab == 5 || tab == 7 || tab == 8 || tab == 9 || tab == 11 || tab == 19 || tab == 20) flagIndexDirty = true; lastTab = tab; }
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
        case 14: case 15: case 16: case 17: drawGeneric(gens[tab - 5], 4, TOP + 4, W - 8, H - TOP - BOTTOM - 8, in); break;
        default: drawGeneric(gens[tab - 3], 4, TOP + 4, W - 8, H - TOP - BOTTOM - 8, in); break;
    }
    // status bar
    cv.fillRect(0, H - BOTTOM, W, BOTTOM, PANEL);
    ui.text(6, H - BOTTOM + 3, toastT > 0 ? toast : "Editor - Tab/Shift+Tab move between fields, Enter/Esc leave a field, Ctrl+S saves the current tab, wheel over a number nudges it (shift = x10).", toastT > 0 ? ACCENT2 : DIM);
    drawPopup(in);
    ui.drawTooltip();
    // cursor
    if (in.mouseInWindow) { int mx = in.mouseX, my = in.mouseY; static const char* cur[] = {"X........", "XX.......", "XOX......", "XOOX.....", "XOOOX....", "XOOOOX...", "XOOOOOX..", "XOOOOOOX.", "XOOOOXXXX", "XOXXOX...", "XX.XOOX..", "...XOOX..", "....XX..."}; for (int y = 0; y < 13; y++) for (int x = 0; x < 9; x++) { char ch = cur[y][x]; if (ch == 'X') cv.put(mx + x, my + y, C::Black); else if (ch == 'O') cv.put(mx + x, my + y, C::White); } }
}

// ---------------- generic key/value editor ----------------
static bool isTileKey(const std::string& k) { return k == "tile" || k == "sprite"; }
static bool isColorKey(const std::string& k) { return k == "color" || k == "tint" || k == "bg"; }

void Editor::drawGeneric(GenericEditor& g, int x, int y, int w, int h, InputState& in) {
    genUndo(g, in);
    int lw = 230;
    ui.panel(x, y, lw, h); { int own = 0; for (auto& sc : g.secs) if (!sc.inherited) own++; ui.header(x, y, lw, g.title + (own == (int)g.secs.size() ? fmt(" (%d)", own) : fmt(" (%d + %d base)", own, (int)g.secs.size() - own))); }
    helpBtn(x + lw - 16, y + 1, tabTopic());
    ui.textField(x + 4, y + 20, lw - 8, g.filter, 40);
    auto idx = g.filtered(); std::vector<std::string> labels;
    for (auto& sc : g.secs) if (sc.inherited && sc.kv != sc.origKv) { sc.inherited = false; sc.origKv.clear(); g.dirty = true; say("Copied '" + sc.get(g.listKey) + "' into the campaign - Save to keep it."); }
    for (int i : idx) { std::string l = g.secs[i].get(g.listKey); if (l.empty()) l = g.secs[i].get("name"); if (l.empty()) l = "(" + g.secs[i].name + ")"; if (g.secs[i].inherited) l += "  (base)"; if (!g.altSection.empty()) l = (g.secs[i].name == g.altSection ? "  notice: " : "board: ") + l; if (&g == &gens[5] && encounterUnused(g.secs[i].get("id"))) l += "  (unused)"; labels.push_back(l); }
    int lsel = -1; for (size_t i = 0; i < idx.size(); i++) if (idx[i] == g.sel) lsel = (int)i;
    int hit = ui.listBox(x + 4, y + 40, lw - 8, h - (g.altSection.empty() ? 100 : 124), labels, lsel, g.scroll);
    if (hit >= 0) { g.sel = idx[hit]; g.fieldScroll = 0; ui.blur(); }
    int by = y + h - 56;
    if (!g.altSection.empty() && ui.button(x + 4, by - 24, 222, 20, "New hand-written notice [" + g.altSection + "]")) { EdSection s; s.name = g.altSection; for (auto& k : g.altTemplateKeys) s.kv.push_back({k, k == g.listKey ? g.altSection + "_" + std::to_string(g.secs.size()) : ""}); g.secs.push_back(s); g.sel = (int)g.secs.size() - 1; g.dirty = true; g.filter.clear(); }
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
    if (g.sel < 0 || g.sel >= (int)g.secs.size()) { ui.text(fx + 10, y + 10, "Select an entry on the left, or press New.", DIM); return; }
    EdSection& s = g.secs[g.sel];
    ui.header(fx, y, fw, "[" + s.name + "]  " + s.get(g.listKey) + "   " + s.get("name") + (s.inherited ? "   - from the base game; any edit makes it the campaign's own" : ""));
    helpBtn(fx + fw - 58, y + 1, s.name, tabTopic());
    { Tile pv; if (s.has("sprite")) pv = parseTile(s.get("sprite")); else if (s.has("tile")) pv = parseTile(s.get("tile")); else if (s.has("spriterow")) pv = T(s.has("spritecol") ? atoi(s.get("spritecol").c_str()) : 104, atoi(s.get("spriterow").c_str()));
      int psz = s.get("size") == "2" ? 2 : 1;
      if (pv.valid()) { ui.cv->fillRect(fx + fw - 40, y + 1, 36, 14, PANEL2); for (int b = 0; b < psz; b++) for (int a = 0; a < psz; a++) { Tile t = T(pv.c + a, pv.r + b); if (s.has("color")) ts->drawRecolor(*ui.cv, t, fx + fw - 28 + a * 12, y + 2 + b * 12, 1, parseColor(s.get("color"))); else ts->draw(*ui.cv, t, fx + fw - 28 + a * 12, y + 2 + b * 12, 1); } } }
    // items, creatures, spells, shops: a "Used by" box under the fields
    std::string useKey = &g == &gens[0] ? "item" : &g == &gens[1] ? "creature" : &g == &gens[2] ? "spell" : &g == &gens[4] ? "shop" : "";
    int usedH = useKey.empty() ? 0 : 130;
    int fieldsBottom = y + h - 32 - usedH;
    int rowH = 22; int fy = y + 22 - g.fieldScroll;
    if (ui.hover(fx, y + 20, fw, fieldsBottom - y - 20) && in.wheel) g.fieldScroll = std::max(0, g.fieldScroll - in.wheel * 22);
    ui.cv->setClip({fx + 1, y + 18, fw - 2, fieldsBottom - y - 18});
    int keyW = 110;
    bool quests = &g == &gens[6];
    int row = 0;
    for (size_t i = 0; i < s.kv.size(); i++) {
        auto& kv = s.kv[i];
        if (quests && (kv.first == "stage" || kv.first == "fail" || (kv.first == "objective" && atoi(trim(split(kv.second, '|')[0]).c_str()) != 0))) { ui.idCounter += 3; continue; }   // drawn as stage cards below
        int ry = fy + row * rowH; row++;
        if (ry + 16 > fieldsBottom || ry < y + 18) { ui.idCounter += 3; continue; }
        ui.label(fx + 8, ry + 2, kv.first);
        helpBtn(fx + keyW - 17, ry + 1, s.name + "." + kv.first, s.name);
        fieldWidgets(g, s, kv, fx + keyW, ry, fw - keyW - 30);
        if (ui.button(fx + fw - 24, ry, 18, 16, "x", true, false, DANGER)) { s.kv.erase(s.kv.begin() + i); g.dirty = true; ui.blur(); break; }
    }
    if (!quests) g.fieldScroll = std::min(g.fieldScroll, std::max(0, row * rowH - (fieldsBottom - y - 40)));
    if (quests) { int used = drawQuestStages(g, s, fx, fy + row * rowH + 4, fw, y + 18, y + h - 32, in); if (ui.hover(fx, y + 20, fw, h - 50) && in.wheel) {} g.fieldScroll = std::min(g.fieldScroll, std::max(0, row * rowH + used - (h - 80))); }
    ui.cv->resetClip();
    if (usedH) drawUsedBy(useKey + ":" + s.get("id"), fx + 4, fieldsBottom + 2, fw - 8, usedH - 6, in);
    // add field row
    int ay = y + h - 26;
    ui.cv->fillRect(fx + 1, ay - 4, fw - 2, 30, PANEL);
    ui.label(fx + 8, ay + 2, "add key:"); ui.textField(fx + 70, ay, 140, g.newKey, 40);
    if (ui.button(fx + 214, ay, 40, 16, "Add", !g.newKey.empty())) { s.kv.push_back({trim(g.newKey), ""}); g.newKey.clear(); g.dirty = true; }
    // template keys quick-add
    int qx = fx + 262;
    for (auto& k : g.templateKeys) { if (s.has(k)) continue; int bw = font->width(k) + 8; if (qx + bw > fx + fw - 4) break; if (ui.button(qx, ay, bw, 16, k)) { s.kv.push_back({k, ""}); g.dirty = true; } qx += bw + 3; }
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
        {"ai", {"melee", "ranged", "caster", "support", "coward", "boss"}},
        {"target", {"enemy", "ally", "self", "any", "ground"}},
        {"effect", {"", "burn", "freeze", "shock", "poison", "regen", "haste", "shield", "weaken", "stun", "aim", "strength", "focus", "blind", "cure", "revive"}},
        {"utility", {"", "light", "stealth", "detect", "unlock", "recall"}},
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
    // --- six-stat rows ---
    if (k == "stats" || k == "bonus") {
        auto m = parsePairs(v); bool ch = false;
        for (int i = 0; i < 6; i++) { int val = m.count(STAT_KEYS[i]) ? m[STAT_KEYS[i]] : 0; ui.text(x + i * 84, y + 2, toUpper(STAT_KEYS[i]), DIM); if (ui.intField(x + 26 + i * 84, y, 54, val, -20, 99)) { m[STAT_KEYS[i]] = val; ch = true; } }
        if (ch) { v = joinPairs(STAT_KEYS, m, k == "stats"); g.dirty = true; }
        return;
    }
    if (k == "res") {
        auto m = parsePairs(v); bool ch = false;
        for (int i = 0; i < 6; i++) { int val = m.count(DMG_KEYS[i]) ? m[DMG_KEYS[i]] : 0; std::string lbl = std::string(DMG_KEYS[i]).substr(0, 3); ui.text(x + i * 86, y + 2, lbl, DIM); if (ui.intField(x + 24 + i * 86, y, 58, val, -100, 100)) { m[DMG_KEYS[i]] = val; ch = true; } }
        if (ch) { v = joinPairs(DMG_KEYS, m, false); g.dirty = true; }
        ui.text(x + 6 * 86, y + 2, "% (neg = weak)", DIM);
        return;
    }
    // --- lo-hi ranges ---
    if (k == "dmg" || k == "gold") {
        int lo = 0, hi = 0; parseRange(v, lo, hi); bool ch = false;
        if (ui.intField(x, y, 70, lo, 0, 9999)) ch = true; ui.text(x + 76, y + 2, "to", DIM); if (ui.intField(x + 94, y, 70, hi, 0, 9999)) ch = true;
        if (ch) { if (hi < lo) hi = lo; v = fmt("%d-%d", lo, hi); g.dirty = true; }
        return;
    }
    // --- colours ---
    if (isColorKey(k)) {
        Color8 c = v.empty() ? C::White : parseColor(v); int r = c.r, gg = c.g, b = c.b; bool ch = false;
        if (ui.intField(x, y, 70, r, 0, 255)) ch = true; if (ui.intField(x + 76, y, 70, gg, 0, 255)) ch = true; if (ui.intField(x + 152, y, 70, b, 0, 255)) ch = true;
        ui.cv->fillRect(x + 230, y, 40, 16, RGB((uint8_t)r, (uint8_t)gg, (uint8_t)b)); ui.cv->rect(x + 230, y, 40, 16, BORDER);
        if (ch) { v = fmt("%d,%d,%d", r, gg, b); g.dirty = true; }
        return;
    }
    // --- tiles / sprites ---
    if (isTileKey(k) || k == "spriterow" || k == "spritecol") {
        Tile t = isTileKey(k) ? parseTile(v) : T(s.has("spritecol") ? atoi(s.get("spritecol").c_str()) : 104, atoi(s.get("spriterow").c_str()));
        if (ui.textField(x, y, 90, v, 20)) g.dirty = true;
        { int psz = (k == "sprite" && s.get("size") == "2") ? 2 : 1;
          if (psz == 2) { ui.cv->fillRect(x + 98, y - 6, 52, 52, FIELD); ui.cv->rect(x + 98, y - 6, 52, 52, ACCENT2); }
          for (int b = 0; b < psz; b++) for (int a = 0; a < psz; a++) { Tile tt = T(t.c + a, t.r + b); if (s.has("color")) ts->drawRecolor(*ui.cv, tt, x + 100 + a * 24, y - 4 + b * 24, 2, parseColor(s.get("color"))); else ts->draw(*ui.cv, tt, x + 100 + a * 24, y - 4 + b * 24, 2); }
          if (psz == 2) ui.text(x + 154, y + 26, "size=2: the sprite is the 2x2 block starting at this tile (top-left)", DIM); }
        if (ui.button(x + 130, y, 50, 16, "pick...")) { EdSection* sp = &s; std::string key = k; openTilePicker(t, [sp, key, &g](Tile nt) { if (!nt.valid()) return; if (key == "spriterow" || key == "spritecol") { for (size_t i = 0; i < sp->kv.size(); i++) if (sp->kv[i].first == "spriterow" || sp->kv[i].first == "spritecol") { sp->kv.erase(sp->kv.begin() + i); i--; } sp->set("sprite", fmt("%d,%d", nt.c, nt.r)); } else sp->set(key, fmt("%d,%d", nt.c, nt.r)); g.dirty = true; }); }
        if (k == "sprite" && s.name == "creature") { ui.text(x + 190, y + 2, "character rows: col 104 + row = race; col 105 sword, 106 axe, 109 staff, 113 shield, 116 gun, 117 bow", DIM); }
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
    if (k == "weapon" || k == "armor" || k == "ammo" || (k == "item" && s.name == "shop") || k == "locked" || k == "key") { singlePick("item", itemIds()); return; }
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
    if (k == "sound" || k == "attacksound") { std::vector<std::string> o{""}; for (auto& id : gAudio.order) if (gAudio.defs.count(id) && gAudio.defs[id].kind == "sfx") o.push_back(id); singlePick("sound event (empty = automatic)", o); return; }
    if (k == "music") { std::vector<std::string> o{""}; for (auto& id : gAudio.order) if (gAudio.defs.count(id) && gAudio.defs[id].kind == "music") o.push_back(id); singlePick("music track", o); return; }
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
    ui.panel(x, y, lw, h); ui.header(x, y, lw, fmt("Tile kinds (%d)", (int)kinds.size())); helpBtn(x + lw - 16, y + 1, "tab.tiles");
    ui.textField(x + 4, y + 20, lw - 8, kindFilter, 40);
    std::vector<int> idx; std::vector<std::string> labels; std::string f = toLower(kindFilter);
    for (size_t i = 0; i < kinds.size(); i++) if (f.empty() || toLower(kinds[i].name).find(f) != std::string::npos) { idx.push_back((int)i); labels.push_back(kinds[i].name); }
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
        ui.cv->fillRect(lx + 3, ry + 1, 16, 16, k.bg);
        if (!k.ground.empty() && k.ground[0].valid()) { if (k.recolor) ts->drawRecolor(*ui.cv, k.ground[0], lx + 5, ry + 3, 1, k.tint); else if (k.opaque) ts->drawOpaque(*ui.cv, k.ground[0], lx + 5, ry + 3, 1, k.tint); else ts->draw(*ui.cv, k.ground[0], lx + 5, ry + 3, 1, k.tint); }
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
        ui.tile(k.ground[i], gx, cy - 2, 2, k.opaque ? C::White : k.tint, k.bg);
        if (ui.button(gx + 28, cy - 2, 26, 13, "...")) { size_t ii = i; openTilePicker(k.ground[i], [&k, ii, this](Tile t) { k.ground[ii] = t; kindsDirty = true; }); }
        if (k.ground.size() > 1 && ui.button(gx + 28, cy + 12, 26, 13, "x", true, false, DANGER)) { k.ground.erase(k.ground.begin() + i); kindsDirty = true; break; }
    }
    if (k.ground.size() < 6 && ui.button(fx + keyW + (int)k.ground.size() * 70, cy, 26, 20, "+")) { k.ground.push_back(k.ground.back()); kindsDirty = true; }
    cy += 34;
    ui.label(fx + 22, cy + 2, "prop (overlay)"); ui.tile(k.prop, fx + keyW, cy - 2, 2);
    if (ui.button(fx + keyW + 28, cy - 2, 26, 13, "...")) openTilePicker(k.prop, [&k, this](Tile t) { k.prop = t; kindsDirty = true; });
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
    ui.label(fx + 22, cy + 2, "encounter weight"); kh("encounter", cy); if (ui.intField(fx + keyW, cy, 80, k.encounter, 0, 20)) kindsDirty = true; ui.text(fx + keyW + 90, cy + 2, "overworld: higher = more random fights (0 = none)", DIM); cy += 22;
    ui.label(fx + 22, cy + 2, "biome"); kh("biome", cy); std::string bm = k.biome; if (ui.textField(fx + keyW, cy, 124, bm, 20)) { k.biome = bm; kindsDirty = true; }
    if (ui.button(fx + keyW + 126, cy, 14, 16, "v")) { std::vector<std::string> ids{"(none)"}; for (auto& e : gens[5].secs) { std::string id = e.get("id"); if (id.size() > 6 && id.substr(id.size() - 6) == "_night") continue; ids.push_back(id); } int cur = 0; for (size_t i = 0; i < ids.size(); i++) if (ids[i] == k.biome) cur = (int)i; TileKind* kp = &k; openListPicker("Biome = the encounter table random fights on this tile use", ids, cur, [this, ids, kp](int i) { kp->biome = i == 0 ? "" : ids[i]; kindsDirty = true; }); }
    { bool tbl = false; for (auto& e : gens[5].secs) if (e.get("id") == k.biome) tbl = true; ui.text(fx + keyW + 150, cy + 2, k.biome.empty() ? "no biome: fights here use the map's table=" : tbl ? (k.encounter > 0 ? "random fights here use the '" + k.biome + "' table (Encounters tab)" : "encounter weight is 0: no random fights here") : "no encounter table called '" + k.biome + "' - fights fall back to the map's table=", k.biome.empty() || tbl ? DIM : ACCENT2); }
    cy += 22;
    ui.label(fx + 22, cy + 2, "move cost"); kh("movecost", cy); if (ui.intField(fx + keyW, cy, 80, k.moveCost, 1, 9)) kindsDirty = true; cy += 30;
    // big preview
    ui.label(fx + 22, cy, "preview (as in game, 2x):"); cy += 14;
    for (int i = 0; i < 6; i++) { int px = fx + 10 + i * 26; ui.cv->fillRect(px, cy, 24, 24, k.bg); Tile g = k.ground[i % k.ground.size()]; if (g.valid()) { if (k.recolor) ts->drawRecolor(*ui.cv, g, px, cy, 2, k.tint); else if (k.opaque) ts->drawOpaque(*ui.cv, g, px, cy, 2, k.tint); else ts->draw(*ui.cv, g, px, cy, 2, k.tint); } if (k.prop.valid()) ts->draw(*ui.cv, k.prop, px, cy, 2); }
    cy += 34;
    // usage
    int used = 0; if (mapLoaded) for (auto& c : em.cells) if (split(c, '|')[0] == k.name) used++;
    ui.text(fx + 10, cy, mapLoaded ? fmt("used by %d cells of the map open in the Maps tab (%s)", used, em.id.c_str()) : "", DIM);
    ui.text(fx + 10, cy + 16, "Tip: 'door_wood', 'door_metal', 'door_arch' are the door kinds; pick the tile you prefer in 'prop'.", DIM);
}

// ---------------- maps ----------------
std::vector<std::string> Editor::paletteList() const {
    std::vector<std::string> out; std::string f = toLower(paletteFilter);
    for (auto& k : allTileKinds()) if (f.empty() || toLower(k.name).find(f) != std::string::npos) out.push_back(k.name);
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
    Tile g = k->ground.empty() ? Tile{} : k->ground[0]; Color8 bg = k->bg;
    for (auto& t : k->ground) if (t.valid()) { g = t; break; }
    int mask = 0, corners = 0;
    if (mx >= 0 && (!k->edges.empty() || k->under))   // joined pieces (paths, shores, rivers...) as the game draws them
        tileKindLook(*k, [&](int dx, int dy) -> const TileKind* { int nx = mx + dx, ny = my + dy; if (nx < 0 || ny < 0 || nx >= em.w || ny >= em.h) return nullptr; return tileKind(trim(split(em.at(nx, ny), '|')[0])); }, g, bg, &mask, &corners);
    ui.cv->fillRect(sx, sy, sz, sz, bg);
    drawGroundPiece(*ui.cv, *ts, k, g, sx, sy, scale, k->tint, k->recolor, k->opaque, mask, corners);
    if (k->prop.valid()) ts->draw(*ui.cv, k->prop, sx, sy, scale);
}
static const char* OBJ_TYPES[] = {"npc", "enemy", "chest", "exit", "trigger", "sign", "door", "savepoint", "obstacle", "lever", "change", "gather"};
static const int NOBJ = 12;
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
    return kv;
}

void Editor::drawMaps(InputState& in) {
    int x = 4, y = TOP + 4, h = H - TOP - BOTTOM - 8, lw = 200;
    // ---- left column: maps list + tools + palette ----
    ui.panel(x, y, lw, h);
    ui.header(x, y, lw, "Maps"); helpBtn(x + lw - 16, y + 1, "tab.maps");
    std::vector<std::string> ml = mapFiles;
    if (dataRoot != baseRoot) for (auto& n : ml) if (!fileExists(dataRoot + "/maps/" + n + ".txt")) n += "  (base)";   // inherited: saving copies it into the campaign
    int hit = ui.listBox(x + 4, y + 20, lw - 8, 110, ml, mapListSel, mapListScroll);
    if (hit >= 0) { if (mapDirty) { std::string f = mapFiles[hit]; openConfirm("Discard unsaved changes to the current map?", [this, f]() { loadMapFile(f); }); } else loadMapFile(mapFiles[hit]); }
    int by = y + 134;
    if (ui.button(x + 4, by, 44, 18, "New")) openTextPrompt("New map id (file name)", "new_map", [this](const std::string& nm) { em = EdMap(); em.id = nm; em.path = dataRoot + "/maps/" + nm + ".txt"; em.header = {{"id", nm}, {"name", nm}, {"kind", "town"}, {"spawn", "5,5"}, {"bg", "0,0,0"}}; em.init(30, 20, "floor_grass"); for (int i = 0; i < 30; i++) { em.at(i, 0) = "tree"; em.at(i, 19) = "tree"; } for (int j = 0; j < 20; j++) { em.at(0, j) = "tree"; em.at(29, j) = "tree"; } mapLoaded = true; mapDirty = true; resizeW = 30; resizeH = 20; objSel = -1; refreshFiles(); });
    if (ui.button(x + 52, by, 44, 18, "Save", mapLoaded && mapDirty, false, OK)) { if (em.save()) { mapDirty = false; changed = true; refreshFiles(); say("Saved " + em.path); } else say("Save failed!"); }
    if (ui.button(x + 100, by, 48, 18, "Revert", mapLoaded && mapDirty)) { std::string f = em.id; openConfirm("Revert all changes?", [this, f]() { loadMapFile(f); }); }
    if (ui.button(x + 152, by, 44, 18, "Check", mapLoaded)) {
        std::vector<std::string> problems;
        auto walk = [&](int cx, int cy) { if (cx < 0 || cy < 0 || cx >= em.w || cy >= em.h) return false; const TileKind* k = tileKind(trim(split(em.at(cx, cy), '|')[0])); return k && k->walkable; };
        for (auto& o : em.objs) { int ox = o.geti("x"), oy = o.geti("y"); if (o.type != "door" && !walk(ox, oy)) problems.push_back(o.type + " " + o.get("id") + o.get("name") + fmt(" at %d,%d is on a blocked tile", ox, oy)); if (o.type == "npc" && !o.get("dialogue").empty() && std::find(dlgFiles.begin(), dlgFiles.end(), o.get("dialogue")) == dlgFiles.end()) problems.push_back("npc " + o.get("id") + " references missing dialogue '" + o.get("dialogue") + "'"); if (o.type == "exit") { std::string to = o.get("to"); bool ok = to == "overworld" || std::find(mapFiles.begin(), mapFiles.end(), to) != mapFiles.end(); for (auto& g : gens[7].secs) if (g.get("id") == to) ok = true; if (!ok) problems.push_back(fmt("exit at %d,%d leads to unknown map '%s'", ox, oy, to.c_str())); } if (o.type == "enemy") for (auto& c : split(o.get("group"), ',')) if (!trim(c).empty() && !gd->creature(trim(c))) problems.push_back("enemy group uses unknown creature '" + trim(c) + "'"); if (o.type == "chest") for (auto& it : split(o.get("items"), ',')) { std::string id = trim(split(it, ':')[0]); if (!id.empty() && !gd->item(id)) problems.push_back("chest has unknown item '" + id + "'"); } }
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
        ui.label(x + 4, ty, "place:"); helpBtn(x + 44, ty - 1, "obj." + std::string(OBJ_TYPES[objType]), "maps.objects"); for (int i = 0; i < NOBJ; i++) { int bw = 46, bx = x + 4 + (i % 4) * 48, byy = ty + 14 + (i / 4) * 20; if (ui.button(bx, byy, bw, 18, OBJ_TYPES[i], true, objType == i)) objType = i; }
        ty += 76;
    } else {
        ui.label(x + 4, ty, "brush:"); ui.text(x + 44, ty, brush, ACCENT2); if (ui.button(x + lw - 50, ty - 2, 44, 14, "edit")) jumpToKind(brush); ty += 14;
        ui.textField(x + 4, ty, lw - 8, paletteFilter, 30); ty += 20;
        auto pal = paletteList(); int psel = -1; for (size_t i = 0; i < pal.size(); i++) if (pal[i] == brush) psel = (int)i;
        int ph = y + h - ty - 4; int rowH = 16, rows = ph / rowH;
        ui.cv->fillRect(x + 4, ty, lw - 8, ph, FIELD); ui.cv->rect(x + 4, ty, lw - 8, ph, BORDER);
        if (ui.hover(x + 4, ty, lw - 8, ph) && in.wheel) paletteScroll -= in.wheel * 3;
        paletteScroll = clampi(paletteScroll, 0, std::max(0, (int)pal.size() - rows));
        for (int r = 0; r < rows && paletteScroll + r < (int)pal.size(); r++) {
            int i = paletteScroll + r; int ry = ty + 1 + r * rowH; const TileKind* k = tileKind(pal[i]);
            bool hv = ui.hover(x + 5, ry, lw - 10, rowH);
            if (i == psel) ui.cv->fillRect(x + 5, ry, lw - 10, rowH, SEL); else if (hv) ui.cv->fillRect(x + 5, ry, lw - 10, rowH, HOVER);
            if (k) { ui.cv->fillRect(x + 7, ry + 1, 14, 14, k->bg); if (k->ground[0].valid()) { if (k->recolor) ts->drawRecolor(*ui.cv, k->ground[0], x + 8, ry + 2, 1, k->tint); else if (k->opaque) ts->drawOpaque(*ui.cv, k->ground[0], x + 8, ry + 2, 1, k->tint); else ts->draw(*ui.cv, k->ground[0], x + 8, ry + 2, 1, k->tint); } if (k->prop.valid()) ts->draw(*ui.cv, k->prop, x + 8, ry + 2, 1); }
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
    hfield("name", cx, 120); hfield("kind", cx, 70); hfield("spawn", cx, 50); hfield("dark", cx, 24); hfield("light", cx, 24); hfield("encounter", cx, 24); hfield("music", cx, 70);
    hy += 20; cx = vx + 22;
    hfield("table", cx, 70);
    if (ui.button(cx - 8, hy, 14, 16, "v")) { std::vector<std::string> ids{"(none)"}; for (auto& e : gens[5].secs) ids.push_back(e.get("id")); int cur = 0; for (size_t i = 0; i < ids.size(); i++) if (ids[i] == em.hget("table")) cur = (int)i; openListPicker("Encounter table (used where a tile kind has no biome table)", ids, cur, [this, ids](int i) { em.hset("table", i == 0 ? "" : ids[i]); mapDirty = true; }); }
    cx += 10; hfield("bg", cx, 70); hfield("worldmap", cx, 60);
    ui.label(cx, hy + 2, "size"); cx += 30; ui.intField(cx, hy, 56, resizeW, 4, 300); cx += 58; ui.intField(cx, hy, 56, resizeH, 4, 300); cx += 60;
    if (ui.button(cx, hy, 50, 16, "Resize", resizeW != em.w || resizeH != em.h)) { mapPushUndo(); em.resize(resizeW, resizeH, brush); mapDirty = true; } cx += 56;
    if (ui.button(cx, hy, 60, 16, "Play >", true, false, OK)) { auto sp = split(em.hget("spawn"), ','); requestPlay(sp.size() == 2 ? atoi(sp[0].c_str()) : 0, sp.size() == 2 ? atoi(sp[1].c_str()) : 0); } if (ui.hover(cx, hy, 60, 16)) ui.tip("Saves everything and starts the game here at the spawn point (P over a tile: play there; N over a tile: overworld arrival point)");
    // view
    int ax = vx + 2, ay = vy + 44, aw = vw - 4, ah = vh - 46;
    int cell = 12 * zoom;
    ui.cv->fillRect(ax, ay, aw, ah, RGB(14, 14, 18));
    bool overView = ui.hover(ax, ay, aw, ah);
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
        auto& o = em.objs[i]; int ox = o.geti("x"), oy = o.geti("y"), ow = std::max(1, o.geti("w", 1)), oh = std::max(1, o.geti("h", 1));
        int sx = ax + ox * cell - viewX, sy = ay + oy * cell - viewY;
        Color8 col = C::White;
        if (o.type == "npc") { Tile t = parseTile(o.get("sprite")); if (!t.valid()) t = T(104, 0); if (!o.get("color").empty()) ts->drawRecolor(*ui.cv, t, sx, sy, zoom, parseColor(o.get("color"))); else ts->draw(*ui.cv, t, sx, sy, zoom); col = C::Cyan; }
        else if (o.type == "enemy") { std::string first = trim(split(o.get("group"), ',')[0]); const CreatureDef* cd = gd->creature(first); int sz = cd && cd->size == 2 ? 2 : 1; ui.cv->fillRect(sx, sy, cell * sz, cell * sz, RGB(120, 0, 0, 160));
            if (cd) for (int b = 0; b < sz; b++) for (int a = 0; a < sz; a++) { Tile t = T(cd->tile.c + a, cd->tile.r + b); if (cd->recolor) ts->drawRecolor(*ui.cv, t, sx + a * cell, sy + b * cell, zoom, cd->color); else ts->draw(*ui.cv, t, sx + a * cell, sy + b * cell, zoom); }
            if (sz == 2) { ow = oh = 2; ui.cv->rect(sx, sy, cell * 2, cell * 2, C::Orange); font->draw(*ui.cv, *ts, sx + 2, sy + cell * 2 - 11, "2x2", C::Orange); }
            col = C::Red; }
        else if (o.type == "chest") { ts->draw(*ui.cv, TL::CHEST, sx, sy, zoom); col = C::Yellow; }
        else if (o.type == "exit") { ui.cv->fillRect(sx, sy, ow * cell, oh * cell, RGB(0, 200, 255, 90)); col = C::Cyan; }
        else if (o.type == "trigger") { ui.cv->fillRect(sx, sy, ow * cell, oh * cell, RGB(255, 200, 0, 80)); col = C::Yellow; }
        else if (o.type == "sign") { ts->draw(*ui.cv, TL::SIGN, sx, sy, zoom); col = C::Cream; }
        else if (o.type == "door") { ui.cv->rect(sx + 1, sy + 1, cell - 2, cell - 2, C::Orange); col = C::Orange; }
        else if (o.type == "savepoint") { ts->draw(*ui.cv, TL::ALTAR, sx, sy, zoom); col = C::Cyan; }
        else if (o.type == "obstacle") { ui.cv->fillRect(sx, sy, ow * cell, oh * cell, RGB(150, 110, 60, 110)); ts->draw(*ui.cv, TL::ROCKS, sx, sy, zoom); col = C::Orange; }
        else if (o.type == "lever") { ts->draw(*ui.cv, TL::LEVER, sx, sy, zoom); col = C::Yellow; }
        else if (o.type == "change") { ui.cv->fillRect(sx, sy, ow * cell, oh * cell, RGB(180, 80, 255, 70)); col = C::Purple; }
        else if (o.type == "gather") { if (auto* it = gd->item(o.get("item"))) ts->draw(*ui.cv, it->tile, sx, sy, zoom); col = C::Green; }
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
        ui.cv->resetClip(); ui.textRight(vx + vw - 6, hy + 2, info, ACCENT2); ui.cv->setClip({ax, ay, aw, ah});
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
                else { mapPushUndo(); EdObj o; o.type = OBJ_TYPES[objType]; o.kv = objTemplate(o.type, mtx, mty); em.objs.push_back(o); objSel = (int)em.objs.size() - 1; mapDirty = true; dragObj = true; ui.blur(); }
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
    if (mapTool <= 2) ui.text(ax + 4, ay + ah - 14, "LMB paint  RMB pick  K: edit kind  P: play here  B: new-game start  N: overworld arrival  mid-drag/wheel/WASD: scroll  Rect: shift = outline  ^Z undo  ^S save", Color8{255, 255, 255, 150});
    else if (mapTool == 5) ui.text(ax + 4, ay + ah - 14, pasting ? "click to stamp (shift-click: keep stamping)  RMB/Esc: cancel" : "click object: edit properties  drag empty space: select  ^C copy  ^X cut  ^V paste  Del: clear  ^A all", Color8{255, 255, 255, 150});
    else ui.text(ax + 4, ay + ah - 14, "click empty: place  click object: select/drag  Delete key: remove selected  ctrl+Z undo", Color8{255, 255, 255, 150});
    if (objSel >= 0 && objSel < (int)em.objs.size() && !typing && in.pressed[K_DELETE] && pop == P_NONE && mapTool == 3) { mapPushUndo(); em.objs.erase(em.objs.begin() + objSel); objSel = -1; mapDirty = true; }
    // object property panel
    if (objSel >= 0 && objSel < (int)em.objs.size() && showObjs) {
        EdObj& o = em.objs[objSel];
        int pw = 300, px = vx + vw - pw - 4, py = ay + 4, ph = std::min(ah - 8, 40 + (int)o.kv.size() * 20 + 60);
        ui.panel(px, py, pw, ph, RGB(38, 41, 50, 240)); ui.header(px, py, pw, "object: " + o.type); helpBtn(px + pw - 16, py + 1, "obj." + o.type, "maps.objects");
        int ry = py + 22;
        for (size_t i = 0; i < o.kv.size(); i++) {
            const std::string& key = o.kv[i].first;
            helpBtn(px + 5, ry + 1, "obj." + o.type + "." + key, "obj." + o.type);
            ui.label(px + 22, ry + 2, key);
            bool flagKey = key == "flag", condKey = key == "require" || key == "openif" || key == "show" || key == "hide" || key == "moveif" || key == "if";
            if (key == "require" && o.type == "exit") condKey = true;
            int vw2 = pw - 116 - (isTileKey(key) ? 30 : 0) - (flagKey || condKey ? 30 : 0);
            if (ui.textField(px + 88, ry, vw2, o.kv[i].second, 300)) mapDirty = true;
            if (condKey && ui.hover(px + 88, ry, vw2, 16) && !o.kv[i].second.empty()) ui.tip("Reads: " + condEnglish(o.kv[i].second));
            if (flagKey && ui.hover(px + 88, ry, vw2, 16) && !o.kv[i].second.empty()) { std::string w = flagSetSummary(o.kv[i].second); ui.tip(o.kv[i].second + (w.empty() ? " - only this object sets it" : " - also set by " + w)); }
            if (isTileKey(key)) { Tile t = parseTile(o.kv[i].second); if (ui.button(px + 88 + vw2 + 4, ry, 22, 16, "...")) { auto& kvr = o.kv[i]; openTilePicker(t, [&kvr, this](Tile nt) { kvr.second = nt.valid() ? fmt("%d,%d", nt.c, nt.r) : ""; mapDirty = true; }); } }
            if (flagKey || condKey) { auto* kvp = &o.kv[i]; bool app = condKey;
                flagPicker(px + 88 + vw2 + 4, ry, 26, app, [kvp, app, this](const std::string& f) { kvp->second = app && !trim(kvp->second).empty() ? trim(kvp->second) + "&" + f : f; mapDirty = true; }, "f"); }
            if (ui.button(px + pw - 22, ry, 16, 16, "x", true, false, DANGER)) { o.kv.erase(o.kv.begin() + i); mapDirty = true; ui.blur(); break; }
            ry += 20;
        }
        static std::string newk; ui.textField(px + 6, ry, 110, newk, 30);
        if (ui.button(px + 120, ry, 40, 16, "Add", !newk.empty())) { o.set(trim(newk), ""); newk.clear(); mapDirty = true; }
        if (ui.button(px + 166, ry, 60, 16, "Delete", true, false, DANGER)) { mapPushUndo(); em.objs.erase(em.objs.begin() + objSel); objSel = -1; mapDirty = true; }
        ry += 22;
        if (o.type == "npc") { if (ui.button(px + 6, ry, 110, 16, "pick dialogue")) { openListPicker("Dialogue", dlgFiles, 0, [&o, this](int i) { o.set("dialogue", dlgFiles[i]); mapDirty = true; }); } }
        if (o.type == "exit") { if (ui.button(px + 6, ry, 110, 16, "pick map")) { std::vector<std::string> ms = mapFiles; ms.insert(ms.begin(), "overworld"); for (auto& g : gens[7].secs) ms.push_back(g.get("id")); openListPicker("Target map", ms, 0, [&o, ms, this](int i) { o.set("to", ms[i]); mapDirty = true; }); } }
        if (o.type == "enemy") { if (ui.button(px + 6, ry, 110, 16, "add creature")) { std::vector<std::string> cs; for (auto& c : gd->creatures) cs.push_back(c.first); openListPicker("Creature", cs, 0, [&o, cs, this](int i) { std::string g = o.get("group"); o.set("group", g.empty() ? cs[i] : g + "," + cs[i]); mapDirty = true; }); } }
        if (o.type == "chest") { if (ui.button(px + 6, ry, 110, 16, "add item")) { std::vector<std::string> is; for (auto& c : gd->items) is.push_back(c.first); openListPicker("Item", is, 0, [&o, is, this](int i) { std::string g = o.get("items"); o.set("items", g.empty() ? is[i] + ":1" : g + "," + is[i] + ":1"); mapDirty = true; }); } }
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
void Editor::textEditWidget(TextEdit& t, int x, int y, int w, int h, InputState& in) {
    int id = ui.nextId();
    bool f = ui.focus == id;
    ui.cv->fillRect(x, y, w, h, FIELD); ui.cv->rect(x, y, w, h, f ? ACCENT : BORDER);
    int rowH = 13, rows = (h - 4) / rowH, gutter = 34;
    static bool dragging = false;
    int hoff = 0; if (f) { int cpx = font->width(t.lines[t.cy].substr(0, t.cx)); if (cpx > w - gutter - 20) hoff = cpx - (w - gutter - 20); }
    auto posFromMouse = [&](int& ox, int& oy) { int ly = (in.mouseY - y - 2) / rowH + t.scroll; oy = clampi(ly, 0, (int)t.lines.size() - 1); int best = 0; for (size_t i = 0; i <= t.lines[oy].size(); i++) if (x + gutter - hoff + font->width(t.lines[oy].substr(0, i)) - 2 <= in.mouseX) best = (int)i; ox = best; };
    if (ui.hover(x, y, w, h)) {
        if (in.wheel) t.scroll -= in.wheel * 3;
        if (in.mousePressed[0] && !ui.mouseUsed) { ui.mouseUsed = true; ui.focus = id; f = true; int ox, oy; posFromMouse(ox, oy); if (in.shift()) { if (t.ay < 0) { t.ax = t.cx; t.ay = t.cy; } } else { t.ax = ox; t.ay = oy; } t.cx = ox; t.cy = oy; dragging = true; }
    } else if (in.mousePressed[0] && f && !ui.modal) { ui.focus = -1; f = false; }
    if (dragging) { if (!in.mouseDown[0]) dragging = false; else if (in.mouseDX || in.mouseDY) { int ox, oy; posFromMouse(ox, oy); t.cx = ox; t.cy = oy; } }
    if (f && !ui.modal) {
        auto edit = [&]() { t.snapshot(); };
        if (in.ctrl()) {
            if (in.pressed[K_A]) { t.ax = 0; t.ay = 0; t.cy = (int)t.lines.size() - 1; t.cx = (int)t.lines[t.cy].size(); }
            if (in.pressed[K_C] && t.hasSel()) ui.copyText(t.selText());
            if (in.pressed[K_X] && t.hasSel()) { ui.copyText(t.selText()); edit(); t.deleteSel(); }
            if (in.pressed[K_V]) { std::string p = ui.pasteText(); if (!p.empty()) { edit(); t.deleteSel(); t.insert(p); } }
            if (in.pressed[K_Z] && !t.undo.empty()) { t.lines = t.undo.back(); t.undo.pop_back(); t.cy = clampi(t.cy, 0, (int)t.lines.size() - 1); t.cx = std::min(t.cx, (int)t.lines[t.cy].size()); t.ay = -1; t.dirty = true; }
            if (in.pressed[K_S]) { if (t.save()) { changed = true; say("Saved " + t.path); } }
            if (in.pressed[K_D]) { edit(); t.lines.insert(t.lines.begin() + t.cy + 1, t.lines[t.cy]); t.cy++; t.dirty = true; }
        } else {
            bool typedAny = false; for (char c : in.typed) if (c >= 32 && c < 127) typedAny = true;
            if (typedAny) { edit(); t.deleteSel(); for (char c : in.typed) if (c >= 32 && c < 127) { t.lines[t.cy].insert(t.lines[t.cy].begin() + t.cx, c); t.cx++; } t.dirty = true; }
            if (in.pressed[K_TAB]) { edit(); t.deleteSel(); t.lines[t.cy].insert(t.cx, "  "); t.cx += 2; t.dirty = true; }
            if (in.pressed[K_ENTER]) { edit(); t.deleteSel(); std::string& L = t.lines[t.cy]; std::string rest = L.substr(t.cx); L = L.substr(0, t.cx); t.lines.insert(t.lines.begin() + t.cy + 1, rest); t.cy++; t.cx = 0; t.dirty = true; }
            if (in.pressed[K_BACKSPACE]) { edit(); if (t.hasSel()) t.deleteSel(); else { std::string& L = t.lines[t.cy]; if (t.cx > 0) { L.erase(t.cx - 1, 1); t.cx--; t.dirty = true; } else if (t.cy > 0) { int nl = (int)t.lines[t.cy - 1].size(); t.lines[t.cy - 1] += L; t.lines.erase(t.lines.begin() + t.cy); t.cy--; t.cx = nl; t.dirty = true; } } }
            if (in.pressed[K_DELETE]) { edit(); if (t.hasSel()) t.deleteSel(); else { std::string& L2 = t.lines[t.cy]; if (t.cx < (int)L2.size()) { L2.erase(t.cx, 1); t.dirty = true; } else if (t.cy + 1 < (int)t.lines.size()) { L2 += t.lines[t.cy + 1]; t.lines.erase(t.lines.begin() + t.cy + 1); t.dirty = true; } } }
            auto startMove = [&]() { if (in.shift()) { if (t.ay < 0) { t.ax = t.cx; t.ay = t.cy; } } else t.ay = -1; };
            bool moved = in.pressed[K_LEFT] || in.pressed[K_RIGHT] || in.pressed[K_UP] || in.pressed[K_DOWN] || in.pressed[K_HOME] || in.pressed[K_END] || in.pressed[K_PAGEUP] || in.pressed[K_PAGEDOWN];
            if (moved) startMove();
            if (in.pressed[K_LEFT]) { if (t.cx > 0) t.cx--; else if (t.cy > 0) { t.cy--; t.cx = (int)t.lines[t.cy].size(); } }
            if (in.pressed[K_RIGHT]) { if (t.cx < (int)t.lines[t.cy].size()) t.cx++; else if (t.cy + 1 < (int)t.lines.size()) { t.cy++; t.cx = 0; } }
            if (in.pressed[K_UP] && t.cy > 0) t.cy--; if (in.pressed[K_DOWN] && t.cy + 1 < (int)t.lines.size()) t.cy++;
            if (in.pressed[K_PAGEUP]) t.cy = std::max(0, t.cy - rows); if (in.pressed[K_PAGEDOWN]) t.cy = std::min((int)t.lines.size() - 1, t.cy + rows);
            if (in.pressed[K_HOME]) t.cx = 0; if (in.pressed[K_END]) t.cx = (int)t.lines[t.cy].size();
            if (in.pressed[K_ESCAPE]) { if (t.hasSel()) t.ay = -1; else ui.focus = -1; }
        }
        t.cy = clampi(t.cy, 0, (int)t.lines.size() - 1); t.cx = std::min(t.cx, (int)t.lines[t.cy].size());
        if (t.cy < t.scroll) t.scroll = t.cy; if (t.cy >= t.scroll + rows) t.scroll = t.cy - rows + 1;
    }
    t.scroll = clampi(t.scroll, 0, std::max(0, (int)t.lines.size() - rows));
    hoff = 0; if (f) { int cpx = font->width(t.lines[t.cy].substr(0, t.cx)); if (cpx > w - gutter - 20) hoff = cpx - (w - gutter - 20); }
    int sx0 = 0, sy0 = -1, sx1 = 0, sy1 = -1; if (t.hasSel()) t.selRange(sx0, sy0, sx1, sy1);
    ui.cv->setClip({x + 1, y + 1, w - 2, h - 2});
    for (int r = 0; r < rows && t.scroll + r < (int)t.lines.size(); r++) {
        int i = t.scroll + r; int ry = y + 2 + r * rowH; const std::string& L = t.lines[i];
        if (i == t.cy && f) ui.cv->fillRect(x + 1, ry - 1, w - 2, rowH, RGB(40, 44, 56));
        Color8 col = TEXT; std::string tl = trim(L);
        if (!tl.empty() && tl[0] == '[') col = ACCENT2; else if (!tl.empty() && tl[0] == '#') col = DIM; else if (startsWith(tl, "option=")) col = RGB(150, 220, 150); else if (startsWith(tl, "text=")) col = C::White; else if (startsWith(tl, "speaker=")) col = ACCENT;
        ui.cv->setClip({x + gutter, y + 1, w - gutter - 1, h - 2});
        if (sy0 >= 0 && i >= sy0 && i <= sy1) { int a = i == sy0 ? sx0 : 0, b = i == sy1 ? sx1 : (int)L.size(); int xa = x + gutter - hoff + font->width(L.substr(0, a)), xb = x + gutter - hoff + font->width(L.substr(0, b)) + (i != sy1 ? 6 : 0); ui.cv->fillRect(xa, ry - 1, std::max(2, xb - xa), rowH, SEL); }
        font->draw(*ui.cv, *ts, x + gutter - hoff, ry, L, col);
        if (i == t.cy && f && ((int)(ui.time * 2) % 2 == 0)) ui.cv->fillRect(x + gutter - hoff + font->width(L.substr(0, t.cx)), ry, 1, 12, C::White);
        ui.cv->setClip({x + 1, y + 1, w - 2, h - 2});
        ui.cv->fillRect(x + 1, ry - 1, gutter - 3, rowH, i == t.cy && f ? RGB(40, 44, 56) : FIELD);
        font->draw(*ui.cv, *ts, x + 4, ry, std::to_string(i + 1), DIM);
    }
    ui.cv->resetClip();
    ui.text(x + w - 330, y + h - 13, "drag/shift: select  ctrl+A/C/X/V  ctrl+Z undo  ctrl+S save  ctrl+D dup line", Color8{255, 255, 255, 110});
}
void Editor::drawDialogue(InputState& in) {
    int x = 4, y = TOP + 4, h = H - TOP - BOTTOM - 8, lw = 200;
    ui.panel(x, y, lw, h); ui.header(x, y, lw, "Dialogue files"); helpBtn(x + lw - 16, y + 1, "tab.dialogue");
    std::vector<std::string> dl = dlgFiles;
    if (dataRoot != baseRoot) for (auto& n : dl) if (!fileExists(dataRoot + "/dialogue/" + n + ".txt")) n += "  (base)";
    int hit = ui.listBox(x + 4, y + 20, lw - 8, h - 80, dl, dlgSel, dlgScroll);
    if (hit >= 0) { if (te.dirty) { int hh = hit; openConfirm("Discard unsaved changes?", [this, hh]() { te.load(readPath("dialogue/" + dlgFiles[hh] + ".txt")); te.path = dataRoot + "/dialogue/" + dlgFiles[hh] + ".txt"; }); } else { te.load(readPath("dialogue/" + dlgFiles[hit] + ".txt")); te.path = dataRoot + "/dialogue/" + dlgFiles[hit] + ".txt"; } }
    int by = y + h - 54;
    if (ui.button(x + 4, by, 50, 20, "New")) openTextPrompt("New dialogue id", "npc_name", [this](const std::string& nm) { std::string p = dataRoot + "/dialogue/" + nm + ".txt"; if (!fileExists(p)) writeFile(p, "[start]\nspeaker=" + nm + "\ntext=Hello, $hero.\noption=Goodbye.|end\n"); refreshFiles(); te.load(p); for (size_t i = 0; i < dlgFiles.size(); i++) if (dlgFiles[i] == nm) dlgSel = (int)i; });
    if (ui.button(x + 60, by, 50, 20, "Save", te.dirty && !te.path.empty(), false, OK)) { if (te.save()) { changed = true; say("Saved " + te.path); } }
    if (ui.button(x + 116, by, 60, 20, "Delete", !te.path.empty(), false, DANGER)) openConfirm("Delete this dialogue file?", [this]() { remove(te.path.c_str()); te = TextEdit(); refreshFiles(); dlgSel = -1; });
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
    static const char* ref[] = {"[node]  (first node is 'start')", "speaker=Name   ($hero = hero's name)", "text=Line  (repeat for more lines)", "option=Label|target|condition", "  target 'end' closes the dialogue", "[camp] = companion talk (Party menu, T)", "  conditions (join with &): flag, !flag,", "  item:id, gold:n, level:n, party:class,", "  flag>=n, kills:creature:n, itemn:id:n,", "  approval:class:n", "next=node  (auto-advance, no options)", "", "Actions (run when the node shows):", "set=flag=1,other    flag=name    inc=name", "clear=flag   give=item:n   take=item:n", "gold=n (negative = pay)   xp=n", "heal=1 (free full heal)   rest=1 (inn)", "healer=1 (paid healer services)", "respec=1   shop=shop_id", "join=class:Name:col,row   leave=class", "combat=creature,creature", "teleport=map,x,y   message=Title|Text", "cutscene=line|line   dialogue=other_id", "spawn=creature,x,y   hidenpc=id  shownpc=id", "notify=text   quest=Name   win=line|line", "revealmap=map  or  map,x,y,radius", "approve=class:n   craft=forge|alchemy|workbench", "rep=faction:n  (town reputation, + or -)", "  condition rep:faction:n = at least n", "board=town_id (notice board)", "", "Hook a file to an NPC with dialogue=<file>", "in the Maps tab (object panel)."};
    for (size_t i = 0; i < sizeof(ref) / sizeof(ref[0]); i++) font->draw(*ui.cv, *ts, rx + 6, y + 22 + (int)i * 13, ref[i], i == 10 ? ACCENT2 : (ref[i][0] == ' ' ? DIM : TEXT));
}

// ---------------- tileset browser ----------------
void Editor::drawTileset(InputState& in) {
    int x = 4, y = TOP + 4, w = W - 8, h = H - TOP - BOTTOM - 8;
    ui.panel(x, y, w, h); ui.header(x, y, w, "Tileset browser - hover for coordinates, click to pick a tile"); helpBtn(x + w - 16, y + 1, "tab.tileset");
    int ax = x + 4, ay = y + 20, aw = w - 8, ah = h - 50, cell = 26, cols = 206, rows = 50;
    if (ui.hover(ax, ay, aw, ah)) { if (in.down[K_SHIFT]) tileScrollX -= in.wheel * 60; else tileScrollY -= in.wheel * 60; }
    if (in.mouseDown[1] || in.mouseDown[2]) { tileScrollX -= in.mouseDX; tileScrollY -= in.mouseDY; }
    if (!ui.typing()) { if (in.down[K_LEFT]) tileScrollX -= 8; if (in.down[K_RIGHT]) tileScrollX += 8; if (in.down[K_UP]) tileScrollY -= 8; if (in.down[K_DOWN]) tileScrollY += 8; }
    tileScrollX = clampi(tileScrollX, 0, cols * cell - aw); tileScrollY = clampi(tileScrollY, 0, rows * cell - ah);
    ui.cv->fillRect(ax, ay, aw, ah, RGB(18, 18, 22)); ui.cv->setClip({ax, ay, aw, ah});
    int c0 = tileScrollX / cell, r0 = tileScrollY / cell;
    for (int r = r0; r <= r0 + ah / cell + 1 && r < rows; r++) for (int c = c0; c <= c0 + aw / cell + 1 && c < cols; c++) { int sx = ax + c * cell - tileScrollX, sy = ay + r * cell - tileScrollY; ts->drawOpaque(*ui.cv, T(c, r), sx + 1, sy + 1, 2); if (tsPick.valid() && tsPick.c == c && tsPick.r == r) ui.cv->rect(sx, sy, cell, cell, ACCENT2); }
    // column/row rulers
    for (int c = c0; c <= c0 + aw / cell + 1 && c < cols; c++) if (c % 5 == 0) font->draw(*ui.cv, *ts, ax + c * cell - tileScrollX + 2, ay + 2, std::to_string(c), Color8{255, 255, 0, 160});
    for (int r = r0; r <= r0 + ah / cell + 1 && r < rows; r++) if (r % 5 == 0) font->draw(*ui.cv, *ts, ax + 2, ay + r * cell - tileScrollY + 2, std::to_string(r), Color8{255, 255, 0, 160});
    ui.cv->resetClip();
    int hc = (in.mouseX - ax + tileScrollX) / cell, hr = (in.mouseY - ay + tileScrollY) / cell;
    if (ui.hover(ax, ay, aw, ah) && hc < cols && hr < rows) { int sx = ax + hc * cell - tileScrollX, sy = ay + hr * cell - tileScrollY; ui.cv->rect(sx, sy, cell, cell, C::White); tsHover = T(hc, hr); if (in.mousePressed[0] && !ui.mouseUsed) { ui.mouseUsed = true; tsPick = tsHover; } }
    std::string info = tsHover.valid() ? fmt("hover: col %d, row %d", tsHover.c, tsHover.r) : "";
    if (tsPick.valid()) { info += fmt("    picked: %d,%d", tsPick.c, tsPick.r); std::string uses; for (auto& k : allTileKinds()) { for (auto& g : k.ground) if (g.valid() && g.c == tsPick.c && g.r == tsPick.r) uses += k.name + " "; if (k.prop.valid() && k.prop.c == tsPick.c && k.prop.r == tsPick.r) uses += k.name + "(prop) "; } if (!uses.empty()) info += "   used by: " + uses; }
    ui.text(x + 8, y + h - 24, info, ACCENT2);
    if (tsPick.valid()) ts->drawOpaque(*ui.cv, tsPick, x + w - 60, y + h - 30, 2);
    ui.text(x + w - 420, y + h - 24, "character rows: col 104 = base pose, rows = creature type", DIM);
}
