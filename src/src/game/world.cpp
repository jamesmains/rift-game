#include "world.h"
#include "tiles.h"
#include "tilekinds_builtin.h"
#include <queue>
#include <cmath>
#include <cstring>

using namespace TL;

std::string gAssetDir = "assets";
static std::vector<TileKind> gKinds;
static std::map<std::string, int> gKindIndex;

static void parseKindsText(const std::string& text, std::vector<TileKind>& kinds);
static int defaultLight(const std::string& n);

// The built-in kinds are assets/data/tilekinds.txt as shipped, compiled in (src/tools/mktilekinds.py writes
// tilekinds_builtin.h from it). The data file replaces them; kinds it doesn't name yet are kept from here.
static void buildKinds() {
    if (!gKinds.empty()) return;
    std::string text; for (const char* const* p = kBuiltinTileKinds; *p; ++p) text += *p;
    parseKindsText(text, gKinds);
    bool haveVoid = false; for (auto& k : gKinds) if (k.name == "void") haveVoid = true;
    if (!haveVoid) { TileKind v; v.name = "void"; v.ground.push_back(Tile{}); v.walkable = false; v.blocksSight = true; v.opaque = true; v.bg = C::Black; gKinds.push_back(v); }
    rebuildTileKindIndex();
}
// default light radii for the built-in kinds (tilekinds.txt can override with light=N)
static int defaultLight(const std::string& n) {
    static const std::map<std::string, int> L = {{"torch", 4}, {"torch_wall", 4}, {"brazier", 4}, {"fire", 4}, {"campfire", 5}, {"crystal", 2}, {"riftcrystal", 2},
        {"floor_lava", 3}, {"reactor", 3}, {"terminal", 2}, {"terminal2", 2}, {"computer", 2}, {"streetlamp", 5}, {"sparkle", 2}, {"portal_tl", 3}, {"portal_tr", 3},
        {"portal_bl", 3}, {"portal_br", 3}, {"altar", 2}, {"gem_blue", 1}, {"ice_wall", 1}, {"cryopod", 1}, {"vending", 2}, {"tv", 1}};
    auto it = L.find(n); return it == L.end() ? 0 : it->second;
}

const TileKind* tileKind(const std::string& name) { buildKinds(); auto it = gKindIndex.find(name); return it == gKindIndex.end() ? nullptr : &gKinds[it->second]; }
const std::vector<TileKind>& allTileKinds() { buildKinds(); return gKinds; }
std::vector<TileKind>& editableTileKinds() { buildKinds(); return gKinds; }
int tileKindIndex(const std::string& name) { buildKinds(); auto it = gKindIndex.find(name); return it == gKindIndex.end() ? -1 : it->second; }
void rebuildTileKindIndex() { gKindIndex.clear(); for (size_t i = 0; i < gKinds.size(); i++) gKindIndex[gKinds[i].name] = (int)i; resolveTileKindGroups(); }

static std::string tileStr(Tile t) { return tileText(t); }
static std::string joinList(const std::vector<std::string>& v) { std::string o; for (size_t i = 0; i < v.size(); i++) o += (i ? "," : "") + v[i]; return o; }
static void readJoinFields(const ConfigSection& sec, TileKind& k) {
    k.recolor = sec.geti("recolor", 0) != 0; k.under = sec.geti("under", 0) != 0; k.shift = sec.geti("shift", 0);
    k.join.clear(); k.link.clear(); k.edges.clear();
    for (auto& g : split(sec.get("join"), ',')) if (!trim(g).empty()) k.join.push_back(trim(g));
    for (auto& g : split(sec.get("link"), ',')) if (!trim(g).empty()) k.link.push_back(trim(g));
    if (sec.has("edges")) { for (auto& g : split(sec.get("edges"), '|')) { std::string t = trim(g); k.edges.push_back(t.empty() ? Tile{} : parseTile(t)); } k.edges.resize(16); }
}
void resolveTileKindGroups() {
    std::map<std::string, int> ids;
    auto bit = [&](const std::string& g) -> uint32_t { auto it = ids.find(g); if (it == ids.end()) { if (ids.size() >= 32) return 0; it = ids.emplace(g, (int)ids.size()).first; } return 1u << it->second; };
    for (auto& k : gKinds) { k.joinBits = 0; for (auto& g : k.join) k.joinBits |= bit(g); }
    for (auto& k : gKinds) { k.linkBits = 0; for (auto& g : (k.link.empty() ? k.join : k.link)) k.linkBits |= bit(g); }
}
void Map::look(int x, int y, Tile& ground, Color8& bg, int* mask, int* corners) const {
    const Cell& c = at(x, y); ground = c.ground; bg = c.bg;
    if (mask) *mask = 0;
    if (corners) *corners = 0;
    if (c.kind >= gKinds.size()) return;
    const TileKind& k = gKinds[c.kind];
    if (k.edges.size() < 16 && !k.under) return;
    tileKindLook(k, [&](int dx, int dy) -> const TileKind* { int nx = x + dx, ny = y + dy; if (!inBounds(nx, ny)) return nullptr; uint16_t kk = at(nx, ny).kind; return kk < gKinds.size() ? &gKinds[kk] : nullptr; }, ground, bg, mask, corners);
}
void drawGroundPiece(Canvas& cv, const Tileset& ts, const TileKind* k, Tile g, int sx, int sy, int scale, Color8 tint, bool recolor, bool opaque, int mask, int corners, bool spill) {
    auto piece = [&](int y) {
        if (!g.valid()) return;
        if (recolor) ts.drawRecolor(cv, g, sx, y, scale, tint);
        else if (opaque) ts.drawOpaque(cv, g, sx, y, scale, tint); else ts.draw(cv, g, sx, y, scale, tint);
    };
    int shift = k ? k->shift * scale : 0, S = Tileset::TS * scale, H = S / 2;
    if (shift <= 0 || shift >= S) piece(sy);
    else {
        Rect keep = cv.clip;
        auto clipTo = [&](int x, int y, int w, int h) {
            int x0 = std::max(x, keep.x), y0 = std::max(y, keep.y), x1 = std::min(x + w, keep.x + keep.w), y1 = std::min(y + h, keep.y + keep.h);
            cv.clip = {x0, y0, std::max(0, x1 - x0), std::max(0, y1 - y0)};
        };
        clipTo(sx, sy, S, spill ? S + shift : S); piece(sy + shift);
        if (mask & MASK_RUNS_NORTH) { clipTo(sx, sy, S, shift); piece(sy); }   // the path comes in from the north: no gap at the top
        cv.clip = keep;
    }
    if (recolor && corners) {
        if (corners & 1) cv.fillRect(sx + H, sy, H, H, tint);
        if (corners & 2) cv.fillRect(sx + H, sy + H, H, H, tint);
        if (corners & 4) cv.fillRect(sx, sy + H, H, H, tint);
        if (corners & 8) cv.fillRect(sx, sy, H, H, tint);
    }
}
static std::string colStr(Color8 c) { return fmt("%d,%d,%d", c.r, c.g, c.b); }
std::string tileKindsText(const std::vector<TileKind>& kinds) {
    std::string o = "# Tile kinds: what each map cell type looks like and how it behaves.\n# ground = one or more tiles (col,row) separated by | - one is picked at random per cell.\n# opaque=1 draws the ground tile with its black background (walls); otherwise the bg color shows through.\n\n";
    for (auto& k : kinds) {
        o += "[kind]\nname=" + k.name + "\nground=";
        for (size_t i = 0; i < k.ground.size(); i++) o += (i ? "|" : "") + tileStr(k.ground[i]);
        o += "\nprop=" + tileStr(k.prop) + fmt("\nwalk=%d\nsight=%d\nwater=%d\nopaque=%d\n", k.walkable, k.blocksSight, k.water, k.opaque);
        o += "tint=" + colStr(k.tint) + "\nbg=" + colStr(k.bg) + fmt("\nencounter=%d\nbiome=%s\ncost=%d\n", k.encounter, k.biome.c_str(), k.moveCost);
        if (k.light) o += fmt("light=%d\n", k.light);
        if (!k.step.empty()) o += "step=" + k.step + "\n";   // 1.3.3
        if (k.recolor) o += "recolor=1\n";
        if (!k.join.empty()) o += "join=" + joinList(k.join) + "\n";
        if (!k.link.empty()) o += "link=" + joinList(k.link) + "\n";
        if (!k.edges.empty()) { o += "edges="; for (size_t i = 0; i < k.edges.size(); i++) o += (i ? "|" : "") + tileStr(k.edges[i]); o += "\n"; }
        if (k.under) o += "under=1\n";
        if (k.shift) o += fmt("shift=%d\n", k.shift);   // 1.0.16: the Editor's save used to drop shift=
        o += "\n";
    }
    return o;
}
bool saveTileKinds(const std::string& path) { buildKinds(); return writeFile(path, tileKindsText(gKinds)); }
static void parseKindsText(const std::string& text, std::vector<TileKind>& kinds) {
    for (auto& sec : parseConfig(text)) {
        if (sec.name != "kind") continue;
        TileKind k; k.name = sec.get("name"); if (k.name.empty()) continue;
        for (auto& g : split(sec.get("ground"), '|')) { std::string t = trim(g); k.ground.push_back(t.empty() ? Tile{} : parseTile(t)); }
        if (k.ground.empty()) k.ground.push_back(Tile{});
        std::string pr = trim(sec.get("prop")); k.prop = pr.empty() ? Tile{} : parseTile(pr);
        k.walkable = sec.geti("walk", 1) != 0; k.blocksSight = sec.geti("sight", 0) != 0; k.water = sec.geti("water", 0) != 0; k.opaque = sec.geti("opaque", 0) != 0;
        k.tint = sec.has("tint") ? parseColor(sec.get("tint")) : C::White; k.bg = sec.has("bg") ? parseColor(sec.get("bg")) : C::Black;
        k.encounter = sec.geti("encounter", 0); k.biome = sec.get("biome"); k.moveCost = sec.geti("cost", 1);
        k.light = sec.has("light") ? sec.geti("light", 0) : defaultLight(k.name);
        k.step = trim(sec.get("step"));
        readJoinFields(sec, k);
        kinds.push_back(k);
    }
}
bool loadTileKinds(const std::string& path) {
    std::string text; if (!readFile(path, text)) return false;
    std::vector<TileKind> kinds;
    parseKindsText(text, kinds);
    if (kinds.empty()) return false;
    buildKinds();
    // built-in kinds that an older tilekinds.txt doesn't know about yet are appended, so new content keeps working
    for (auto& b : gKinds) { bool have = false; for (auto& k : kinds) if (k.name == b.name) { have = true; break; } if (!have) kinds.push_back(b); }
    gKinds = kinds; rebuildTileKindIndex();
    if (gKindIndex.find("void") == gKindIndex.end()) { TileKind v; v.name = "void"; v.ground.push_back(Tile{}); v.walkable = false; v.blocksSight = true; v.opaque = true; gKinds.push_back(v); rebuildTileKindIndex(); }
    resolveTileKindGroups();
    return true;
}
bool loadTileKindsOverlay(const std::string& path) {
    std::string text; if (!readFile(path, text)) return false;
    buildKinds();
    std::vector<TileKind> kinds; parseKindsText(text, kinds);
    for (auto& k : kinds) {
        auto it = gKindIndex.find(k.name);
        if (it != gKindIndex.end()) gKinds[it->second] = k; else { gKindIndex[k.name] = (int)gKinds.size(); gKinds.push_back(k); }
    }
    resolveTileKindGroups();
    return true;
}
void ensureTileKindsFile(const std::string& path) { if (!fileExists(path)) saveTileKinds(path); }

// ---------------- Map ----------------
bool Map::walkable(int x, int y) const { if (!inBounds(x, y)) return false; return at(x, y).walkable; }
bool Map::blocksSight(int x, int y) const { if (!inBounds(x, y)) return true; return at(x, y).blocksSight; }

void Map::set(int x, int y, const std::string& kindName, Rng& rng) {
    if (!inBounds(x, y)) return;
    const TileKind* k = tileKind(kindName);
    if (!k) k = tileKind("void");
    Cell& c = at(x, y);
    c.kind = (uint16_t)(k - &gKinds[0]);
    c.ground = k->ground.empty() ? Tile{} : rng.pick(k->ground);
    c.prop = k->prop; c.walkable = k->walkable; c.blocksSight = k->blocksSight; c.water = k->water; c.tint = k->tint; c.bg = k->bg; c.opaque = k->opaque; c.recolor = k->recolor;
}
void Map::fill(int x, int y, int fw, int fh, const std::string& kindName, Rng& rng) { for (int yy = y; yy < y + fh; yy++) for (int xx = x; xx < x + fw; xx++) set(xx, yy, kindName, rng); }
void Map::rect(int x, int y, int fw, int fh, const std::string& kindName, Rng& rng) {
    for (int xx = x; xx < x + fw; xx++) { set(xx, y, kindName, rng); set(xx, y + fh - 1, kindName, rng); }
    for (int yy = y; yy < y + fh; yy++) { set(x, yy, kindName, rng); set(x + fw - 1, yy, kindName, rng); }
}
void Map::init(int W, int H, const std::string& kindName, Rng& rng) { w = W; h = H; cells.assign((size_t)W * H, Cell{}); fill(0, 0, W, H, kindName, rng); }

NPC* Map::npcAt(int x, int y) { for (auto& n : npcs) if (!n.hidden && n.x == x && n.y == y) return &n; return nullptr; }
EnemyGroup* Map::enemyAt(int x, int y) { for (auto& e : enemies) if (!e.defeated && e.x == x && e.y == y) return &e; return nullptr; }
Chest* Map::chestAt(int x, int y) { for (auto& c : chests) if (c.x == x && c.y == y) return &c; return nullptr; }
Door* Map::doorAt(int x, int y) { for (auto& d : doors) if (d.x == x && d.y == y) return &d; return nullptr; }
Exit* Map::exitAt(int x, int y) { for (auto& e : exits) if (x >= e.x && y >= e.y && x < e.x + e.w && y < e.y + e.h) return &e; return nullptr; }
SignPost* Map::signAt(int x, int y) { for (auto& s : signs) if (s.x == x && s.y == y) return &s; return nullptr; }
SavePoint* Map::saveAt(int x, int y) { for (auto& s : savepoints) if (s.x == x && s.y == y) return &s; return nullptr; }
bool Map::occupied(int x, int y) const {
    for (auto& n : npcs) if (!n.hidden && n.x == x && n.y == y) return true;
    for (auto& e : enemies) if (!e.defeated && e.x == x && e.y == y) return true;
    for (auto& c : chests) if (c.x == x && c.y == y) return true;
    return false;
}
bool Map::findFree(int cx, int cy, int radius, int& ox, int& oy, Rng& rng) const {
    for (int r = 0; r <= radius; r++) {
        std::vector<std::pair<int, int>> cand;
        for (int dy = -r; dy <= r; dy++) for (int dx = -r; dx <= r; dx++) {
            if (std::max(abs(dx), abs(dy)) != r) continue;
            int x = cx + dx, y = cy + dy;
            if (walkable(x, y) && !occupied(x, y)) cand.push_back({x, y});
        }
        if (!cand.empty()) { auto p = rng.pick(cand); ox = p.first; oy = p.second; return true; }
    }
    return false;
}

// ---------------- object looks (1.1.1) ----------------
using ObjKv = std::vector<std::pair<std::string, std::string>>;
static const std::string* kvFind(const ObjKv& kv, const char* k) { for (auto& p : kv) if (p.first == k) return &p.second; return nullptr; }
static std::string kvStr(const ObjKv& kv, const char* k) { const std::string* v = kvFind(kv, k); return v ? *v : std::string(); }
static void kvTile(const ObjKv& kv, const char* k, Tile& t) { std::string v = trim(kvStr(kv, k)); if (!v.empty()) { Tile p = parseTile(v); if (p.valid()) t = p; } }
static void kvColor(const ObjKv& kv, const char* k, bool& has, Color8& c) { std::string v = trim(kvStr(kv, k)); if (!v.empty()) { has = true; c = parseColor(v); } }
static void kvText(const ObjKv& kv, const char* k, std::string& s) { std::string v = kvStr(kv, k); if (!trim(v).empty()) s = v; }
static void readSavePoint(SavePoint& s, const ObjKv& kv) {
    kvTile(kv, "sprite", s.sprite); kvColor(kv, "color", s.hasColor, s.color);
    std::string sp = trim(kvStr(kv, "sparkle")); if (!sp.empty()) s.sparkle = sp != "0";
    bool hs = false; kvColor(kv, "sparklecolor", hs, s.sparkleColor);
    kvText(kv, "text", s.text); kvText(kv, "sound", s.sound); kvText(kv, "onuse", s.onUse);
}
static void readChestLook(Chest& c, const ObjKv& kv) {
    kvTile(kv, "sprite", c.sprite); kvTile(kv, "opensprite", c.openSprite); kvColor(kv, "color", c.hasColor, c.color);
    kvText(kv, "sound", c.sound); kvText(kv, "onuse", c.onUse);
}
void applyTypeLook(SavePoint& s) { if (const ObjTemplate* t = objectType("savepoint")) readSavePoint(s, t->kv); }
void applyTypeLook(Chest& c) { if (const ObjTemplate* t = objectType("chest")) readChestLook(c, t->kv); }

// ---------------- ASCII map loading ----------------
bool loadMapFile(const std::string& path, Map& m, Rng& rng, std::string& err) {
    std::string text;
    if (!readFile(path, text)) { err = "cannot read " + path; return false; }
    if (!loadMapText(text, m, rng, err)) { if (err == "no rows") err = "no rows in " + path; return false; }
    return true;
}
bool loadMapText(const std::string& text, Map& m, Rng& rng, std::string& err) {
    std::map<char, std::string> legend;
    std::vector<std::string> rows;
    std::string section;
    std::vector<std::pair<std::string, std::vector<std::pair<std::string, std::string>>>> objects; // [type], kv
    for (auto& raw : split(text, '\n')) {
        std::string line = raw; while (!line.empty() && (line.back() == '\r')) line.pop_back();
        std::string t = trim(line);
        bool legendEntry = (section == "legend" && t.size() >= 3 && (t[0] == '#' || t[0] == '=') && t[1] == ' ' && t[2] == '=');
        if (section != "rows" && !legendEntry && (t.empty() || t[0] == '#')) continue;
        if (!t.empty() && t[0] == '[' && t.back() == ']') {
            section = t.substr(1, t.size() - 2);
            if (section != "map" && section != "legend" && section != "rows") objects.push_back({section, {}});
            continue;
        }
        if (section == "rows") { if (t.empty() && rows.empty()) continue; if (t.empty() && t != line) {} rows.push_back(line); continue; }
        size_t eq = legendEntry ? 2 : t.find('=');
        if (eq == std::string::npos) continue;
        std::string k = legendEntry ? std::string(1, t[0]) : trim(t.substr(0, eq)), v = trim(t.substr(eq + 1));
        if (section == "map") {
            if (k == "id") m.id = v; else if (k == "name") m.name = v; else if (k == "kind") m.kind = v;
            else if (k == "dark") m.dark = atoi(v.c_str()) != 0; else if (k == "light") m.lightRadius = atoi(v.c_str());
            else if (k == "encounter") m.encounterRate = atoi(v.c_str()); else if (k == "table") m.encounterTable = v;
            else if (k == "spawn") { auto p = split(v, ','); if (p.size() == 2) { m.spawnX = atoi(p[0].c_str()); m.spawnY = atoi(p[1].c_str()); } }
            else if (k == "music") m.music = v;
            else if (k == "combatmusic") m.combatMusic = v;
            else if (k == "ambient") m.ambient = v;
            else if (k == "worldmap") m.worldMap = v;
            else if (k == "onenter") m.onEnter = v;
            else if (k == "touch") m.touchCombat = atoi(v.c_str()) != 0;
            else if (k == "bg") m.bg = parseColor(v);
        } else if (section == "legend") {
            if (!k.empty()) legend[k[0]] = v;
        } else if (!objects.empty()) objects.back().second.push_back({k, v});
    }
    while (!rows.empty() && trim(rows.back()).empty()) rows.pop_back();
    if (rows.empty()) { err = "no rows"; return false; }
    int W = 0; for (auto& r : rows) W = std::max(W, (int)r.size());
    int H = (int)rows.size();
    m.init(W, H, "void", rng);
    for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) {
        char ch = x < (int)rows[y].size() ? rows[y][x] : ' ';
        auto it = legend.find(ch);
        if (it == legend.end()) { if (ch == ' ') m.set(x, y, "void", rng); continue; }
        // "a|b|c" random choice
        auto choices = split(it->second, '|');
        m.set(x, y, trim(rng.pick(choices)), rng);
    }
    auto geti = [](const std::vector<std::pair<std::string, std::string>>& kv, const std::string& k, int def) { for (auto& p : kv) if (p.first == k) return atoi(p.second.c_str()); return def; };
    auto gets = [](const std::vector<std::pair<std::string, std::string>>& kv, const std::string& k, const std::string& def = "") -> std::string { for (auto& p : kv) if (p.first == k) return p.second; return def; };
    for (auto& ob : objects) {
        auto& kv = ob.second;
        std::string type = ob.first;
        applyObjectTemplate(type, kv);
        if (type == "npc") {
            NPC n; n.def = trim(gets(kv, "npc")); n.id = gets(kv, "id"); if (n.id.empty()) n.id = n.def; n.name = gets(kv, "name", n.id); n.dialogue = gets(kv, "dialogue"); n.shop = gets(kv, "shop");
            n.x = n.homeX = geti(kv, "x", 0); n.y = n.homeY = geti(kv, "y", 0);
            n.tile = parseTile(gets(kv, "sprite", "104,0")); n.wander = geti(kv, "wander", 0) != 0;
            if (!gets(kv, "color").empty()) { n.recolor = true; n.color = parseColor(gets(kv, "color")); }
            n.showFlag = gets(kv, "show"); n.hideFlag = gets(kv, "hide"); n.hostileCreature = gets(kv, "hostile"); n.craft = gets(kv, "craft"); n.board = gets(kv, "board"); n.travel = geti(kv, "travel", 0) != 0;
            n.moveIf = gets(kv, "moveif"); n.moveX = geti(kv, "movex", -1); n.moveY = geti(kv, "movey", -1);
            n.script = trim(gets(kv, "script"));
            m.npcs.push_back(n);
        } else if (type == "enemy") {
            EnemyGroup e; e.flag = gets(kv, "flag"); e.x = e.homeX = geti(kv, "x", 0); e.y = e.homeY = geti(kv, "y", 0); e.aggro = geti(kv, "aggro", 5);
            for (auto& c : split(gets(kv, "group"), ',')) if (!trim(c).empty()) e.creatures.push_back(trim(c));
            e.levelBonus = geti(kv, "level", 0); e.onDefeatScript = gets(kv, "ondefeat"); e.showFlag = gets(kv, "show"); e.hideFlag = gets(kv, "hide"); e.link = gets(kv, "link");
            e.leash = geti(kv, "leash", 0);   // 1.2.4 (#41)
            m.enemies.push_back(e);
        } else if (type == "chest") {
            Chest c; c.x = geti(kv, "x", 0); c.y = geti(kv, "y", 0); c.flag = gets(kv, "flag"); c.gold = geti(kv, "gold", 0); c.locked = gets(kv, "locked");
            for (auto& s : split(gets(kv, "items"), ',')) { auto p = split(trim(s), ':'); if (p.empty() || trim(p[0]).empty()) continue; c.items.push_back({trim(p[0]), p.size() > 1 ? atoi(p[1].c_str()) : 1}); }
            readChestLook(c, kv);
            m.chests.push_back(c);
        } else if (type == "exit") {
            Exit e; e.x = geti(kv, "x", 0); e.y = geti(kv, "y", 0); e.w = geti(kv, "w", 1); e.h = geti(kv, "h", 1); e.to = gets(kv, "to"); e.tx = geti(kv, "tx", 0); e.ty = geti(kv, "ty", 0);
            e.requireFlag = gets(kv, "require"); e.requireItem = gets(kv, "item"); e.lockedMsg = gets(kv, "locked");
            m.exits.push_back(e);
        } else if (type == "trigger") {
            Trigger t; t.x = geti(kv, "x", 0); t.y = geti(kv, "y", 0); t.w = geti(kv, "w", 1); t.h = geti(kv, "h", 1); t.script = gets(kv, "script"); t.flag = gets(kv, "flag"); t.once = geti(kv, "once", 1) != 0; t.requireFlag = gets(kv, "require");
            m.triggers.push_back(t);
        } else if (type == "sign") {
            SignPost s; s.x = geti(kv, "x", 0); s.y = geti(kv, "y", 0); s.text = gets(kv, "text");
            kvTile(kv, "sprite", s.sprite); kvColor(kv, "color", s.hasColor, s.color); kvText(kv, "title", s.title); kvText(kv, "sound", s.sound); kvText(kv, "onuse", s.onUse);
            m.signs.push_back(s);
            if (m.inBounds(s.x, s.y)) m.at(s.x, s.y).walkable = false;
        } else if (type == "door") {
            Door d; d.x = geti(kv, "x", 0); d.y = geti(kv, "y", 0); d.key = gets(kv, "key"); d.flag = gets(kv, "flag"); d.openIf = gets(kv, "openif"); d.require = gets(kv, "require"); d.lockedMsg = gets(kv, "locked");
            d.closedTile = m.inBounds(d.x, d.y) ? m.at(d.x, d.y).prop : DOOR_WOOD; d.openTile = gets(kv, "metal") == "1" ? DOOR_METAL : DOOR_OPEN;
            { Tile sp; kvTile(kv, "sprite", sp); if (sp.valid()) { d.closedTile = sp; if (m.inBounds(d.x, d.y)) m.at(d.x, d.y).prop = sp; } }   // 1.1.1: sprite = the closed door's picture
            kvTile(kv, "opensprite", d.openTile); kvText(kv, "sound", d.sound); kvText(kv, "text", d.msg); kvText(kv, "onuse", d.onUse);
            if (m.inBounds(d.x, d.y)) { m.at(d.x, d.y).walkable = false; m.at(d.x, d.y).blocksSight = true; }
            m.doors.push_back(d);
        } else if (type == "savepoint") {
            SavePoint s; s.x = geti(kv, "x", 0); s.y = geti(kv, "y", 0); readSavePoint(s, kv); m.savepoints.push_back(s);
        } else if (type == "obstacle") {
            Obstacle o; o.x = geti(kv, "x", 0); o.y = geti(kv, "y", 0); o.w = geti(kv, "w", 1); o.h = geti(kv, "h", 1); o.name = gets(kv, "name", "Rubble");
            o.kind = gets(kv, "kind"); o.clearKind = gets(kv, "clear"); o.needs = gets(kv, "needs", "rubble"); o.flag = gets(kv, "flag"); o.msg = gets(kv, "text"); o.onClear = gets(kv, "onclear");
            // 1.4.0 (#75): freeze (water -> ice), shock / emp (a dead or sealed machine door), light (shows only when lit:
            // looks like whatever the map has there)
            auto needsHas = [&](const char* w) { for (auto& n : split(o.needs, '|')) if (trim(n) == w) return true; return false; };
            const auto& ks = allTileKinds(); bool inb = m.inBounds(o.x, o.y);
            const TileKind* here = inb && m.at(o.x, o.y).kind < ks.size() ? &ks[m.at(o.x, o.y).kind] : nullptr;
            if (o.kind.empty()) {
                if (needsHas("freeze")) o.kind = here && here->water && !here->walkable ? here->name : "floor_water";
                else if (needsHas("light") && here) o.kind = here->name;
                else if (needsHas("shock") || needsHas("emp")) o.kind = "door_metal";
                else o.kind = o.needs.find("burn") != std::string::npos ? "vines" : o.needs.find("melt") != std::string::npos ? "ice_wall" : "rubble_pile";
            }
            if (o.clearKind.empty() && needsHas("freeze")) o.clearKind = "floor_ice";
            if (o.clearKind.empty() && inb) { o.clearKind = ks[m.at(o.x, o.y).kind].name; if (!ks[m.at(o.x, o.y).kind].walkable) o.clearKind = "floor_cave"; }
            for (int yy = o.y; yy < o.y + o.h; yy++) for (int xx = o.x; xx < o.x + o.w; xx++) { m.set(xx, yy, o.kind, rng); if (m.inBounds(xx, yy)) m.at(xx, yy).walkable = false; }   // 1.4.0: blocks even if its kind is walkable (a metal door)
            m.obstacles.push_back(o);
        } else if (type == "crate") {   // 1.4.0 (#76)
            Crate c; c.x = c.homeX = geti(kv, "x", 0); c.y = c.homeY = geti(kv, "y", 0); c.id = gets(kv, "id"); if (c.id.empty()) c.id = fmt("crate%d", (int)m.crates.size() + 1);
            c.heavy = geti(kv, "heavy", 0); c.name = gets(kv, "name", c.heavy > 0 ? "Heavy crate" : "Crate");
            kvTile(kv, "sprite", c.sprite); kvColor(kv, "color", c.hasColor, c.color);
            if (m.inBounds(c.x, c.y)) m.at(c.x, c.y).walkable = false;   // a crate blocks its cell (fights too); pushing it moves the block
            m.crates.push_back(c);
        } else if (type == "plate") {   // 1.4.0 (#76)
            Plate p; p.x = geti(kv, "x", 0); p.y = geti(kv, "y", 0); p.group = trim(gets(kv, "group")); p.onPress = gets(kv, "onpress"); p.onRelease = gets(kv, "onrelease");
            kvTile(kv, "sprite", p.sprite); kvColor(kv, "color", p.hasColor, p.color);
            m.plates.push_back(p);
        } else if (type == "lever") {
            Lever l; l.x = geti(kv, "x", 0); l.y = geti(kv, "y", 0); l.flag = gets(kv, "flag"); l.msg = gets(kv, "text"); l.name = gets(kv, "name", "Lever"); l.toggle = geti(kv, "toggle", 1) != 0;
            kvTile(kv, "sprite", l.sprite); kvTile(kv, "onsprite", l.onSprite); kvText(kv, "sound", l.sound); kvText(kv, "onuse", l.onUse);
            if (m.inBounds(l.x, l.y)) { m.set(l.x, l.y, "lever_off", rng); }
            m.levers.push_back(l);
        } else if (type == "change") {
            WorldChange c; c.x = geti(kv, "x", 0); c.y = geti(kv, "y", 0); c.w = geti(kv, "w", 1); c.h = geti(kv, "h", 1); c.kind = gets(kv, "kind"); c.cond = gets(kv, "if");
            for (int yy = c.y; yy < c.y + c.h; yy++) for (int xx = c.x; xx < c.x + c.w; xx++) c.orig.push_back(m.inBounds(xx, yy) ? m.at(xx, yy).kind : 0);
            m.changes.push_back(c);
        } else if (type == "gather") {
            // keys the node doesn't set come from its resource type (resources.txt): node=, else the type giving its item
            auto has = [&](const char* k) { for (auto& p : kv) if (p.first == k) return true; return false; };
            const ResourceDef* r = resourceFor(gets(kv, "node"), gets(kv, "item"));
            GatherNode g; g.x = geti(kv, "x", 0); g.y = geti(kv, "y", 0); g.node = r ? r->id : gets(kv, "node");
            g.item = has("item") ? gets(kv, "item") : r ? r->item : ""; g.tool = has("tool") ? gets(kv, "tool") : r ? r->tool : ""; g.name = has("name") ? gets(kv, "name") : r ? r->name : "";
            g.sound = has("sound") ? trim(gets(kv, "sound")) : r ? r->sound : "";
            if (r) { g.lo = r->lo; g.hi = r->hi; g.respawn = r->respawn; applyResource(g, *r); }
            if (has("count")) { auto cnt = split(gets(kv, "count", "1"), '-'); g.lo = atoi(cnt[0].c_str()); g.hi = cnt.size() > 1 ? atoi(cnt[1].c_str()) : g.lo; }
            if (has("respawn")) g.respawn = geti(kv, "respawn", 3);
            if (!gets(kv, "color").empty()) { g.hasColor = true; g.color = parseColor(gets(kv, "color")); }
            if (!gets(kv, "spent").empty()) { g.hasSpent = true; g.spent = parseColor(gets(kv, "spent")); }
            if (has("sprite")) g.sprite = parseTile(gets(kv, "sprite"));
            if (has("spentsprite")) g.spentSprite = parseTile(gets(kv, "spentsprite"));
            m.gathers.push_back(g);
        }
    }
    return true;
}

// ---------------- resource node types and object templates (1.1.0) ----------------
static std::vector<ResourceDef> gResources;
static std::vector<ObjTemplate> gObjTemplates;
void setResourceDefs(const std::vector<ConfigSection>& secs) {
    gResources.clear();
    for (auto& sec : secs) {
        if (sec.name != "resource" || sec.get("id").empty()) continue;
        ResourceDef r; r.id = sec.get("id"); r.item = sec.get("item"); r.name = sec.get("name"); r.tool = sec.get("tool"); r.kind = sec.get("kind"); r.sound = trim(sec.get("sound"));
        parseRange(sec.get("count", "1"), r.lo, r.hi); if (r.hi < r.lo) r.hi = r.lo; r.respawn = sec.geti("respawn", 3);
        r.sprite = parseTile(sec.get("sprite")); r.spentSprite = parseTile(sec.get("spentsprite"));
        if (!sec.get("color").empty()) { r.hasColor = true; r.color = parseColor(sec.get("color")); }
        if (!sec.get("spent").empty()) { r.hasSpent = true; r.spent = parseColor(sec.get("spent")); }
        for (auto& part : split(sec.get("styles"), ',')) {
            auto p = split(trim(part), ':'); if (p.empty() || trim(p[0]).empty()) continue;
            ResourceStyle st; st.style = trim(p[0]); if (p.size() > 1) st.weight = std::max(1, atoi(p[1].c_str()));
            if (p.size() > 2 && !trim(p[2]).empty()) { parseRange(trim(p[2]), st.lo, st.hi); if (st.hi < st.lo) st.hi = st.lo; }
            r.styles.push_back(st);
        }
        bool dup = false; for (auto& o : gResources) if (o.id == r.id) { o = r; dup = true; } if (!dup) gResources.push_back(r);
    }
}
const std::vector<ResourceDef>& resourceDefs() { return gResources; }
const ResourceDef* resourceFor(const std::string& node, const std::string& item) {
    if (!node.empty()) { for (auto& r : gResources) if (r.id == node) return &r; return nullptr; }
    if (!item.empty()) for (auto& r : gResources) if (r.item == item) return &r;
    return nullptr;
}
void applyResource(GatherNode& g, const ResourceDef& r) {
    g.node = r.id; g.sprite = r.sprite; g.spentSprite = r.spentSprite;
    if (r.hasColor) { g.hasColor = true; g.color = r.color; }
    if (r.hasSpent) { g.hasSpent = true; g.spent = r.spent; }
}
Color8 gatherGuessColor(const std::string& item) {
    std::string id = toLower(item);
    if (id.find("ore") != std::string::npos || id.find("iron") != std::string::npos || id.find("ingot") != std::string::npos) return RGB(235, 140, 60);
    if (id.find("herb") != std::string::npos || id.find("leaf") != std::string::npos || id.find("moon") != std::string::npos || id.find("lichen") != std::string::npos || id.find("cap") != std::string::npos) return RGB(120, 235, 120);
    if (id.find("crystal") != std::string::npos || id.find("gem") != std::string::npos || id.find("circuit") != std::string::npos || id.find("core") != std::string::npos || id.find("rift") != std::string::npos) return RGB(90, 220, 255);
    return RGB(255, 215, 90);
}
static std::vector<ObjTemplate> gObjTypes;   // 1.1.1: [type] sections, id = the object type
void setObjectTemplates(const std::vector<ConfigSection>& secs) {
    gObjTemplates.clear(); gObjTypes.clear();
    for (auto& sec : secs) {
        if ((sec.name != "template" && sec.name != "type") || sec.get("id").empty()) continue;
        ObjTemplate t; t.id = sec.get("id"); t.type = sec.name == "type" ? t.id : sec.get("type");
        for (auto& kv : sec.kv) if (kv.first != "id" && kv.first != "type" && kv.first != "x" && kv.first != "y" && kv.first != "template") {
            if (sec.name == "type" && trim(kv.second).empty()) continue;   // an empty value in a type = the built-in default
            t.kv.push_back(kv);
        }
        auto& list = sec.name == "type" ? gObjTypes : gObjTemplates;
        bool dup = false; for (auto& o : list) if (o.id == t.id) { o = t; dup = true; } if (!dup) list.push_back(t);
    }
}
static std::vector<NpcDef> gNpcDefs;   // 1.1.9: npcs.txt
void setNpcDefs(const std::vector<ConfigSection>& secs) {
    gNpcDefs.clear();
    for (auto& sec : secs) {
        if (sec.name != "npc" || trim(sec.get("id")).empty()) continue;
        NpcDef d; d.id = trim(sec.get("id")); for (auto& kv : sec.kv) if (kv.first != "x" && kv.first != "y" && kv.first != "npc" && kv.first != "template") d.kv.push_back(kv);
        bool dup = false; for (auto& o : gNpcDefs) if (o.id == d.id) { o = d; dup = true; } if (!dup) gNpcDefs.push_back(d);
    }
}
const std::vector<NpcDef>& npcDefs() { return gNpcDefs; }
const NpcDef* npcDef(const std::string& id) { if (id.empty()) return nullptr; for (auto& d : gNpcDefs) if (d.id == id) return &d; return nullptr; }
const NpcDef* companionDef(const std::string& cls) { if (cls.empty()) return nullptr; for (auto& d : gNpcDefs) if (trim(d.get("companion")) == cls) return &d; return nullptr; }
JoinSpec parseJoin(const std::string& v0) {
    JoinSpec j; std::string v = trim(v0);
    if (v.find(':') == std::string::npos) if (const NpcDef* d = npcDef(v)) if (!trim(d->get("companion")).empty()) {
        j.npc = d; j.cls = trim(d->get("companion")); j.name = trim(d->get("name", d->id));
        j.tile = parseTile(trim(d->get("partysprite")).empty() ? d->get("sprite") : d->get("partysprite")); return j; }
    auto jp = split(v, ':'); j.cls = jp.empty() ? "" : trim(jp[0]); j.name = jp.size() > 1 ? trim(jp[1]) : ""; if (jp.size() > 2) j.tile = parseTile(jp[2]);
    return j;
}
const ObjTemplate* objectTemplate(const std::string& id) { for (auto& t : gObjTemplates) if (t.id == id) return &t; return nullptr; }
const ObjTemplate* objectType(const std::string& type) { for (auto& t : gObjTypes) if (t.id == type) return &t; return nullptr; }
void applyObjectTemplate(std::string& type, std::vector<std::pair<std::string, std::string>>& kv) {
    auto fill = [&](const ObjTemplate& t) { for (auto& tk : t.kv) { bool have = false; for (auto& p : kv) if (p.first == tk.first) { have = true; break; } if (!have) kv.push_back(tk); } };
    std::string id; for (auto& p : kv) if (p.first == "template") id = p.second;
    if (!id.empty()) if (const ObjTemplate* t = objectTemplate(id)) { if (!t->type.empty()) type = t->type; fill(*t); }
    // 1.1.9: an NPC placement takes what it doesn't set (or leaves empty) from its NPC
    if (type == "npc") { std::string nid; for (auto& p : kv) if (p.first == "npc") nid = trim(p.second);
        if (const NpcDef* d = npcDef(nid)) for (auto& dk : d->kv) { if (dk.first == "id") continue; bool have = false; for (auto& p : kv) if (p.first == dk.first) { if (trim(p.second).empty()) p.second = dk.second; have = true; } if (!have) kv.push_back(dk); } }
    // then what every object of its type uses (an empty value on the object or its template counts as not set)
    if (const ObjTemplate* ty = objectType(type)) for (auto& tk : ty->kv) { bool have = false; for (auto& p : kv) if (p.first == tk.first) { if (trim(p.second).empty()) p.second = tk.second; have = true; } if (!have) kv.push_back(tk); }
}

// ---------------- LOS & pathfinding ----------------
int manhattan(int x0, int y0, int x1, int y1) { return abs(x1 - x0) + abs(y1 - y0); }
int chebyshev(int x0, int y0, int x1, int y1) { return std::max(abs(x1 - x0), abs(y1 - y0)); }

bool lineOfSight(const Map& m, int x0, int y0, int x1, int y1) {
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    int x = x0, y = y0;
    while (true) {
        if (x == x1 && y == y1) return true;
        if (!(x == x0 && y == y0) && m.blocksSight(x, y)) return false;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x += sx; }
        if (e2 <= dx) { err += dx; y += sy; }
    }
}

std::vector<std::pair<int, int>> findPath(const Map& m, int sx, int sy, int gx, int gy, const std::vector<std::pair<int, int>>& blocked, int maxLen, bool allowGoalBlocked) {
    std::vector<std::pair<int, int>> result;
    if (!m.inBounds(gx, gy)) return result;
    int W = m.w, H = m.h;
    std::vector<int> came((size_t)W * H, -1), gcost((size_t)W * H, 1 << 29);
    std::vector<uint8_t> blk((size_t)W * H, 0);
    for (auto& b : blocked) if (m.inBounds(b.first, b.second)) blk[(size_t)b.second * W + b.first] = 1;
    auto idx = [W](int x, int y) { return y * W + x; };
    typedef std::pair<int, int> PQ; // f, idx
    std::priority_queue<PQ, std::vector<PQ>, std::greater<PQ>> pq;
    gcost[idx(sx, sy)] = 0; pq.push({manhattan(sx, sy, gx, gy), idx(sx, sy)});
    const int DX[4] = {1, -1, 0, 0}, DY[4] = {0, 0, 1, -1};
    while (!pq.empty()) {
        auto [f, cur] = pq.top(); pq.pop();
        int cx = cur % W, cy = cur / W;
        if (cx == gx && cy == gy) break;
        if (gcost[cur] > maxLen) continue;
        for (int d = 0; d < 4; d++) {
            int nx = cx + DX[d], ny = cy + DY[d];
            if (!m.inBounds(nx, ny)) continue;
            bool isGoal = (nx == gx && ny == gy);
            if (!m.walkable(nx, ny) && !(isGoal && allowGoalBlocked)) continue;
            if (blk[idx(nx, ny)] && !(isGoal && allowGoalBlocked)) continue;
            int ng = gcost[cur] + 1;
            if (ng < gcost[idx(nx, ny)]) { gcost[idx(nx, ny)] = ng; came[idx(nx, ny)] = cur; pq.push({ng + manhattan(nx, ny, gx, gy), idx(nx, ny)}); }
        }
    }
    if (came[idx(gx, gy)] < 0 && !(sx == gx && sy == gy)) return result;
    int cur = idx(gx, gy);
    while (cur != idx(sx, sy)) { result.push_back({cur % W, cur / W}); cur = came[cur]; if (cur < 0) { result.clear(); return result; } }
    std::reverse(result.begin(), result.end());
    return result;
}
