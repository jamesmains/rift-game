// 1.2.0 sprite sheets in the Editor.
//  #36 (owner: "import sprite sheet(s); new campaigns default to no spritesheet with the option to copy from the base
//      game"): a campaign's sheets are <campaign>/sheets/<name>.png, each with <name>.txt (the grid it is cut by, its
//      categories and groups). A standalone campaign starts with none - the Tileset tab imports a PNG (also a file
//      dropped on the window) or copies the base game's sheet. A tile on a sheet other than "tiles" is written
//      col,row@name. Campaigns with inherit=1 also see the base game's sheets.
//  #37 (owner: "tiles need categorization, this will dictate what is shown in what tab; also allow grouping"): every
//      tile has a category letter ([cells] in <name>.txt); each sprite field's picker opens on its category (Items ->
//      items, NPCs / Classes -> characters...). Named groups ([group]) find tiles quickly. Sorted in the Tileset tab
//      (select, keys 1-9, right-click menu) or from the picker's right-click menu. Saved at once.
#include "editor.h"
#include <filesystem>
#include <algorithm>
#include <set>

namespace fs = std::filesystem;
using namespace ED;
extern int gEdTop;
#define TOP gEdTop
static const int BOTTOM = 18;

namespace {
struct TileCat { char k; const char* name; const char* about; Color8 col; };
const TileCat CATS[] = {
    {'t', "Terrain", "Ground, floors, walls, water, roads. The Tiles tab's ground pieces open here.", {96, 176, 96, 255}},
    {'p', "Props", "Furniture, plants, doors, signs, machines. Tiles tab props, Objects, Resources and map objects open here.", {206, 150, 72, 255}},
    {'c', "Characters", "People: party members and NPCs. Classes and NPCs open here.", {96, 160, 255, 255}},
    {'m', "Creatures", "Monsters and animals. The Creatures tab opens here.", {226, 84, 84, 255}},
    {'i', "Items", "Weapons, armour, potions, loot. The Items tab opens here.", {240, 212, 84, 255}},
    {'e', "Effects", "Spells, icons, symbols. Spells and Achievements open here.", {190, 112, 240, 255}},
    {'l', "Letters", "Text and digits.", {176, 176, 176, 255}},
    {'x', "Unused", "Never offered in a category (separators, scraps). Still listed under All.", {120, 120, 120, 255}},
    {'.', "Not sorted", "No category yet. Shown after the sorted tiles in every category, so nothing is out of reach.", {150, 150, 165, 255}},
};
const int NCATS = (int)(sizeof(CATS) / sizeof(CATS[0]));
int catIndex(char k) { for (int i = 0; i < NCATS; i++) if (CATS[i].k == k) return i; return NCATS - 1; }
bool validCat(char k) { for (auto& c : CATS) if (c.k == k) return true; return false; }
const int CELL = 26;
inline int selKey(int c, int r) { return r * 4096 + c; }
std::string slug(const std::string& s) {
    std::string o; for (char ch : toLower(trim(s))) { if (isalnum((unsigned char)ch) || ch == '_' || ch == '-') o += ch; else if (!o.empty() && o.back() != '_') o += '_'; }
    while (!o.empty() && o.back() == '_') o.pop_back(); return o;
}
std::string unquote(std::string p) { p = trim(p); if (p.size() >= 2 && (p[0] == '"' || p[0] == '\'') && p.back() == p[0]) p = p.substr(1, p.size() - 2); return p; }
bool spriteKey(const std::string& k) { return k == "tile" || k == "sprite" || k == "spentsprite" || k == "opensprite" || k == "onsprite" || k == "partysprite"; }
enum { MC_CAT = 200, MC_ADDGROUP = 300, MC_NEWGROUP = 301, MC_REMOVE = 400, MENU_PICKER = 9100, MENU_TILESET = 9101 };
const char* BOOK_HEAD =
    "# [sheet] = how the picture is cut into tiles. [cells] = each tile's category, one line per row (r<row>=), one letter\n"
    "# per column: t terrain, p props, c characters, m creatures, i items, e effects, l letters, x unused, . not sorted.\n"
    "# [group] = a named set of tiles (col,row ...) for finding them quickly in the tile picker.\n";
std::string sheetSig(const Tileset* ts) {
    std::string sig = ts->campaignDir + "|";
    for (auto& sp : ts->sheets) sig += sp.name + ":" + sp.info + ":" + std::to_string(sp.cols) + "x" + std::to_string(sp.rows) + ";";
    return sig;
}
}

// ---------------- the books: categories + groups per sheet ----------------
std::vector<int> Editor::visibleSheets() const {
    std::vector<int> v; if (!ts) return v;
    for (int i = 0; i < (int)ts->sheets.size(); i++) if (ts->visible(i) && ts->sheetCols(i) > 0) v.push_back(i);
    return v;
}
void Editor::syncBooks(bool force) {
    if (!ts) return;
    std::string sig = sheetSig(ts);
    if (!force && sig == bookSig && books.size() == ts->sheets.size()) return;
    bookSig = sig; books.assign(ts->sheets.size(), SheetBook());
    for (size_t s = 0; s < ts->sheets.size(); s++) {
        SheetBook& b = books[s]; b.name = ts->sheets[s].name; b.cols = ts->sheetCols((int)s); b.rows = ts->sheetRows((int)s);
        b.cats.assign((size_t)b.cols * b.rows, '.');
        const std::string& path = ts->sheets[s].info;
        b.grid = readSheetGrid(path); b.loaded = true;
        std::string text; if (!readFile(path, text)) continue;
        for (auto& sec : parseConfig(text)) {
            if (sec.name == "cells") {
                for (auto& kv : sec.kv) {
                    if (kv.first.size() < 2 || kv.first[0] != 'r') continue;
                    int r = atoi(kv.first.c_str() + 1); if (r < 0 || r >= b.rows) continue;
                    for (int c = 0; c < (int)kv.second.size() && c < b.cols; c++) if (validCat(kv.second[c])) b.cats[(size_t)r * b.cols + c] = kv.second[c];
                }
            } else if (sec.name == "group") {
                SheetGroup g; g.name = trim(sec.get("name"));
                for (auto& t : split(sec.get("tiles"), ' ')) { auto p = split(trim(t), ','); if (p.size() >= 2 && !trim(p[0]).empty()) g.cells.push_back({atoi(p[0].c_str()), atoi(p[1].c_str())}); }
                if (!g.name.empty()) b.groups.push_back(g);
            }
        }
    }
}
char Editor::cellCat(Tile t) {
    if (!t.valid() || t.s >= books.size()) return '.';
    const SheetBook& b = books[t.s]; if (t.c >= b.cols || t.r < 0 || t.r >= b.rows) return '.';
    return b.cats[(size_t)t.r * b.cols + t.c];
}
bool Editor::saveBook(int s) {
    if (s < 0 || s >= (int)books.size() || !ts) return false;
    SheetBook& b = books[s];
    std::string o = "# Sprite sheet \"" + b.name + "\" - written by the Editor's Tileset tab.\n" + BOOK_HEAD + sheetGridText(b.grid) + "\n[cells]\n";
    for (int r = 0; r < b.rows; r++) {
        std::string row = b.cats.substr((size_t)r * b.cols, (size_t)b.cols);
        while (!row.empty() && row.back() == '.') row.pop_back();
        if (!row.empty()) o += "r" + std::to_string(r) + "=" + row + "\n";
    }
    for (auto& g : b.groups) {
        o += "\n[group]\nname=" + g.name + "\ntiles=";
        for (size_t i = 0; i < g.cells.size(); i++) o += (i ? " " : "") + fmt("%d,%d", g.cells[i].first, g.cells[i].second);
        o += "\n";
    }
    std::error_code ec; fs::create_directories(sheetsDir(), ec);
    std::string path = sheetsDir() + "/" + b.name + ".txt";
    if (!writeFile(path, o)) { say("Could not save " + path); return false; }
    ts->sheets[s].info = path; bookSig = sheetSig(ts);   // the campaign's own copy from now on (copy on write)
    return true;
}
void Editor::reloadSheets() {
    if (!ts) return;
    ts->useCampaign(baseRoot, dataRoot == baseRoot ? "" : dataRoot, campaignInherit, true);
    ownColCache.clear(); syncBooks(true);
    computeTileKindDayColors();   // 1.8.0 (#159): day colours come from the sheets
}
int Editor::sheetImport(const std::string& png0, const std::string& name0, const SheetGrid& g, std::string& err) {
    std::string png = unquote(png0), name = slug(name0);
    if (name.empty()) { err = "Give the sheet a name (letters, digits, _ or -)."; return -1; }
    Image8 im; if (!im.load(png)) { err = "Could not read a PNG picture at " + png; return -1; }
    if (g.cols(im.w) <= 0 || g.rows(im.h) <= 0) { err = "With that grid the picture holds no tiles."; return -1; }
    std::error_code ec; fs::create_directories(sheetsDir(), ec);
    std::string dst = sheetsDir() + "/" + name + ".png";
    bool same = fs::exists(dst, ec) && fs::equivalent(png, dst, ec);
    if (!same) { fs::copy_file(png, dst, fs::copy_options::overwrite_existing, ec); if (ec) { err = "Could not copy the picture: " + ec.message(); return -1; } }
    // a sheet imported again under the same name keeps its categories and groups
    std::string info = sheetsDir() + "/" + name + ".txt", old, rest;
    if (readFile(info, old)) { size_t a = old.find("[cells]"), b = old.find("[group]"); size_t k = std::min(a, b); if (k != std::string::npos) rest = old.substr(k); }
    SheetGrid gg = g; if (gg.title.empty()) gg.title = fs::path(png).filename().string();
    if (!writeFile(info, "# Sprite sheet \"" + name + "\" - written by the Editor's Tileset tab.\n" + std::string(BOOK_HEAD) + sheetGridText(gg) + (rest.empty() ? "" : "\n" + rest))) { err = "Could not write " + info; return -1; }
    reloadSheets();
    for (int i = 0; i < (int)ts->sheets.size(); i++) if (ts->sheets[i].name == name && ts->sheets[i].own) return i;
    err = "The picture was copied but did not load."; return -1;
}
bool Editor::copyBaseSheets(std::string& err) {
    if (dataRoot == baseRoot) { err = "This is the base game: its sheets are already here."; return false; }
    std::error_code ec; fs::create_directories(sheetsDir(), ec);
    int n = 0;
    auto copyOne = [&](const std::string& png, const std::string& txt, const std::string& name) {
        std::string dp = sheetsDir() + "/" + name + ".png", dt = sheetsDir() + "/" + name + ".txt";
        if (fileExists(dp) || !fileExists(png)) return;
        fs::copy_file(png, dp, ec); if (ec) { err = "Could not copy " + png + ": " + ec.message(); return; }
        if (!fileExists(dt) && fileExists(txt)) fs::copy_file(txt, dt, ec);
        n++;
    };
    copyOne(baseRoot + "/../tiles.png", baseRoot + "/sheets/tiles.txt", "tiles");
    for (auto& e : fs::directory_iterator(baseRoot + "/sheets", ec)) {
        if (toLower(e.path().extension().string()) != ".png") continue;
        std::string nm = e.path().stem().string(); if (toLower(nm) == "tiles") continue;
        copyOne(e.path().string(), baseRoot + "/sheets/" + nm + ".txt", nm);
    }
    if (!err.empty()) return false;
    if (n == 0) { err = "This campaign already has copies of the base game's sheets."; return false; }
    reloadSheets();
    return true;
}
bool Editor::removeSheet(int s, std::string& err) {
    const SheetPics* sp = ts ? ts->sheet(s) : nullptr;
    if (!sp || !sp->own || !sp->loaded) { err = "Only a sheet in this campaign's own sheets folder can be removed."; return false; }
    std::error_code ec; std::string nm = sp->name;
    fs::remove(sheetsDir() + "/" + nm + ".png", ec); fs::remove(sheetsDir() + "/" + nm + ".txt", ec);
    reloadSheets(); return true;
}
std::vector<Tile> Editor::pickerTiles(int s, char cat, int group, int& sortedCount) {
    std::vector<Tile> out; sortedCount = 0; syncBooks();
    if (s < 0 || s >= (int)books.size()) return out;
    const SheetBook& b = books[s];
    auto mk = [&](int c, int r) { Tile t = T(c, r); t.s = (uint8_t)s; return t; };
    if (group >= 0 && group < (int)b.groups.size()) {
        for (auto& p : b.groups[group].cells) { Tile t = mk(p.first, p.second); if (!ts->cellEmpty(t)) out.push_back(t); }
        sortedCount = (int)out.size(); return out;
    }
    for (int r = 0; r < b.rows; r++) for (int c = 0; c < b.cols; c++) {
        char k = b.cats[(size_t)r * b.cols + c];
        if (cat == 0 || k == cat) { Tile t = mk(c, r); if (!ts->cellEmpty(t)) out.push_back(t); }
    }
    sortedCount = (int)out.size();
    if (cat && cat != '.' && cat != 'x')   // the tiles nobody sorted yet come after, so a fresh sheet is usable at once
        for (int r = 0; r < b.rows; r++) for (int c = 0; c < b.cols; c++) if (b.cats[(size_t)r * b.cols + c] == '.') { Tile t = mk(c, r); if (!ts->cellEmpty(t)) out.push_back(t); }
    return out;
}
std::vector<int> Editor::shownGroups(int s, char cat, const std::string& find) {
    std::vector<int> all, inCat; if (s < 0 || s >= (int)books.size()) return all;
    const SheetBook& b = books[s];
    for (int i = 0; i < (int)b.groups.size(); i++) {
        if (!edMatch(b.groups[i].name, find)) continue;
        all.push_back(i);
        if (cat) for (auto& p : b.groups[i].cells) { Tile t = T(p.first, p.second); t.s = (uint8_t)s; if (cellCat(t) == cat) { inCat.push_back(i); break; } }
    }
    return cat && !inCat.empty() ? inCat : all;
}
void Editor::setCats(const std::vector<Tile>& tiles, char k) {
    std::set<int> touched; int n = 0;
    for (auto& t : tiles) {
        if (t.s >= books.size()) continue; SheetBook& b = books[t.s];
        if (t.c < 0 || t.c >= b.cols || t.r < 0 || t.r >= b.rows) continue;
        b.cats[(size_t)t.r * b.cols + t.c] = k; touched.insert(t.s); n++;
    }
    for (int s : touched) saveBook(s);
    if (n) say(fmt("%d tile%s -> %s (saved).", n, n == 1 ? "" : "s", CATS[catIndex(k)].name));
}
std::string Editor::tileUses(Tile t) {
    static std::string lastKey, lastVal;
    std::string key = tileText(t) + "|" + std::to_string(frameNo / 120); if (key == lastKey) return lastVal;
    std::string o; int n = 0;
    auto add = [&](const std::string& s) { if (n < 6) o += (o.empty() ? "" : ", ") + s; n++; };
    for (auto& k : allTileKinds()) {
        for (auto& g : k.ground) if (sameTile(g, t)) { add("kind " + k.name); break; }
        if (sameTile(k.prop, t)) add("kind " + k.name + " (prop)");
    }
    static const char* what[] = {"item", "creature", "spell", "class", "shop", "encounter", "quest", "dungeon", "world", "recipe", "affix", "banter", "achievement", "bounty", "faction", "object", "resource", "NPC"};
    for (size_t gi = 0; gi < gens.size(); gi++) for (auto& sc : gens[gi].secs) for (auto& kv : sc.kv) {
        if (!spriteKey(kv.first) || kv.second.empty()) continue;
        if (sameTile(parseTile(kv.second), t)) { add(std::string(gi < 18 ? what[gi] : "entry") + " " + sc.get(gens[gi].listKey, sc.get("id", sc.get("name")))); break; }
    }
    if (n > 6) o += fmt(" and %d more", n - 6);
    lastKey = key; lastVal = o; return o;
}

// ---------------- right-click menu (picker and Tileset tab) ----------------
void Editor::openTileMenu(const std::vector<Tile>& tiles, int menuId) {
    pkMenuTiles = tiles; if (tiles.empty()) return;
    char cur = cellCat(tiles[0]); bool same = true; for (auto& t : tiles) if (cellCat(t) != cur) same = false;
    std::vector<EdUI::MenuItem> items;
    for (int i = 0; i < NCATS; i++) items.push_back({MC_CAT + i, std::string(same && CATS[i].k == cur ? "> " : "   ") + CATS[i].name, std::to_string(i + 1), !(same && CATS[i].k == cur)});
    items.push_back(EdUI::sep());
    int n = (int)tiles.size();
    items.push_back({MC_ADDGROUP, n > 1 ? fmt("Add these %d to a group...", n) : std::string("Add to a group..."), "", true});
    items.push_back({MC_NEWGROUP, n > 1 ? std::string("New group of these...") : std::string("New group with this one..."), "", true});
    int s = tiles[0].s, shown = 0;
    if (s < (int)books.size()) for (int gi = 0; gi < (int)books[s].groups.size() && shown < 6; gi++) {
        bool in = false; for (auto& p : books[s].groups[gi].cells) { for (auto& t : tiles) if (p.first == t.c && p.second == t.r) in = true; if (in) break; }
        if (in) { items.push_back({MC_REMOVE + gi, "Remove from " + books[s].groups[gi].name, "", true}); shown++; }
    }
    ui.openMenu(menuId, items);
}
void Editor::applyTileMenu(int cmd, bool fromPicker) {
    std::vector<Tile> tiles = pkMenuTiles; if (tiles.empty()) return;
    int s = tiles[0].s; if (s >= (int)books.size()) return;
    if (cmd >= MC_CAT && cmd < MC_CAT + NCATS) { setCats(tiles, CATS[cmd - MC_CAT].k); return; }
    if (cmd >= MC_REMOVE && cmd < MC_REMOVE + 10000) {
        int gi = cmd - MC_REMOVE; if (gi >= (int)books[s].groups.size()) return;
        auto& cells = books[s].groups[gi].cells; size_t before = cells.size();
        cells.erase(std::remove_if(cells.begin(), cells.end(), [&](const std::pair<int, int>& p) { for (auto& t : tiles) if (p.first == t.c && p.second == t.r) return true; return false; }), cells.end());
        if (saveBook(s)) say(fmt("Removed %d from the group %s (saved).", (int)(before - cells.size()), books[s].groups[gi].name.c_str()));
        return;
    }
    auto back = [this, fromPicker]() { if (fromPicker) reopenTilePicker(); };
    auto addTo = [this, s, tiles](int gi) {
        if (gi < 0 || gi >= (int)books[s].groups.size()) return;
        auto& cells = books[s].groups[gi].cells; int n = 0;
        for (auto& t : tiles) { bool have = false; for (auto& p : cells) if (p.first == t.c && p.second == t.r) have = true; if (!have) { cells.push_back({t.c, t.r}); n++; } }
        if (saveBook(s)) say(fmt("Added %d to the group %s (saved).", n, books[s].groups[gi].name.c_str()));
    };
    auto newGroup = [this, s, addTo, back]() {
        openTextPrompt("Name of the new group", "", [this, s, addTo, back](const std::string& nm0) {
            std::string nm = trim(nm0); if (nm.empty()) { back(); return; }
            int gi = -1; for (int i = 0; i < (int)books[s].groups.size(); i++) if (toLower(books[s].groups[i].name) == toLower(nm)) gi = i;
            if (gi < 0) { SheetGroup g; g.name = nm; books[s].groups.push_back(g); gi = (int)books[s].groups.size() - 1; }
            addTo(gi); back();
        });
        popCancel = back;
    };
    if (cmd == MC_NEWGROUP) { newGroup(); return; }
    if (cmd == MC_ADDGROUP) {
        std::vector<std::string> names; for (auto& g : books[s].groups) names.push_back(g.name + fmt("   (%d)", (int)g.cells.size()));
        names.push_back("+ New group...");
        openListPicker("Add to which group?", names, 0, [this, s, addTo, newGroup, back](int i) { if (i >= (int)books[s].groups.size()) { newGroup(); return; } addTo(i); back(); });
        popCancel = back;
    }
}

// ---------------- the tile picker (P_TILE) ----------------
void Editor::drawTilePicker(InputState& in, bool& close) {
    Canvas& cv = *ui.cv; syncBooks();
    int x = 20, y = 30, w = W - 40, h = H - 60;
    ui.panel(x, y, w, h); ui.header(x, y, w, popTitle + (pkCur.valid() ? "   (now " + tileText(pkCur) + ")" : "") + "  - click a tile; right-click to sort it");
    helpBtn(x + w - 16, y + 1, "tile.picker", "tab.tileset");
    ui.idCounter = 6100;
    auto vis = visibleSheets();
    auto cancelRow = [&]() {
        if (ui.button(x + w - 150, y + h - 28, 60, 20, "None")) { auto cb = pkCb; close = true; if (cb) cb(Tile{}); }
        if (!close && (ui.button(x + w - 80, y + h - 28, 70, 20, "Cancel") || in.pressed[K_ESCAPE])) close = true;
    };
    if (vis.empty()) {   // 1.2.0 (#36): a new campaign has no sheets until one is imported or copied
        font->drawWrapped(cv, *ts, x + 24, y + 40, w - 48, "This campaign has no sprite sheets yet. Import a picture in the Tileset tab (or drop a PNG file on the Editor window), or copy the base game's sheet into the campaign to start from it.", TEXT);
        if (ui.button(x + 24, y + 96, 190, 22, "Open the Tileset tab")) { close = true; tab = 13; }
        if (ui.button(x + 222, y + 96, 230, 22, "Copy the base game's sheet", dataRoot != baseRoot)) {
            std::string err; if (copyBaseSheets(err)) say("Copied the base game's sheet into " + sheetsDir() + "."); else say(err);
        }
        cancelRow(); return;
    }
    if (std::find(vis.begin(), vis.end(), pkSheet) == vis.end()) { pkSheet = vis[0]; pkGroup = -1; }
    int s = pkSheet; SheetBook& b = books[s];
    // row 1: the sheets
    int ry = y + 20, rx = x + 6;
    ui.label(rx, ry + 3, "Sheet"); rx += 40;
    for (int v : vis) {
        const std::string& nm = ts->sheets[v].name; int bw = font->width(nm) + 14;
        if (rx + bw > x + w - 140) break;
        if (ui.button(rx, ry, bw, 18, nm, true, v == s)) { pkSheet = v; pkGroup = -1; pkScroll = 0; tileScrollX = tileScrollY = 0; if (pkCat) { int n = 0; pickerTiles(v, pkCat, -1, n); if (n == 0) pkCat = 0; } }
        rx += bw + 3;
    }
    if (ui.checkbox(x + w - 120, ry + 3, "Whole sheet", pkLayout)) pkScroll = 0;
    if (ui.hover(x + w - 120, ry + 2, 110, 14)) ui.tip("Show the sheet as it is laid out (tiles outside the category or group dimmed) instead of only the matching tiles");
    // row 2: categories with counts
    int cnt[NCATS] = {}; int all = 0;
    for (int r = 0; r < b.rows; r++) for (int c = 0; c < b.cols; c++) { Tile t = T(c, r); t.s = (uint8_t)s; if (ts->cellEmpty(t)) continue; cnt[catIndex(b.cats[(size_t)r * b.cols + c])]++; all++; }
    ry += 22; rx = x + 6;
    auto catBtn = [&](char k, const std::string& label, Color8 col, int n, const char* about) {
        std::string l = label + " " + std::to_string(n); int bw = font->width(l) + 12;
        bool on = pkCat == k && pkGroup < 0;
        if (ui.button(rx, ry, bw, 18, l, true, on, col)) { pkCat = k; pkGroup = -1; pkScroll = 0; }
        if (about && ui.hover(rx, ry, bw, 18)) ui.tip(about);
        rx += bw + 3;
    };
    catBtn(0, "All", TEXT, all, "Every tile on the sheet");
    for (int i = 0; i < NCATS; i++) if (CATS[i].k != 'x' && (CATS[i].k != '.' || cnt[i])) catBtn(CATS[i].k, CATS[i].name, CATS[i].col, cnt[i], CATS[i].about);
    if (cnt[catIndex('x')]) catBtn('x', "Unused", DIM, cnt[catIndex('x')], CATS[catIndex('x')].about);
    // left: groups
    int gx = x + 6, gy = ry + 24, gw = 186, gh = y + h - 34 - gy;
    ui.searchField(gx, gy, gw, pkFind, "find a group");
    auto gl = shownGroups(s, pkCat, pkFind);
    std::vector<std::string> labels{pkCat ? std::string("(the whole category)") : std::string("(every tile)")}; int gsel = 0;
    for (size_t i = 0; i < gl.size(); i++) { labels.push_back(b.groups[gl[i]].name + fmt("  (%d)", (int)b.groups[gl[i]].cells.size())); if (gl[i] == pkGroup) gsel = (int)i + 1; }
    int ghit = ui.listBox(gx, gy + 20, gw, gh - 20, labels, gsel, pkGroupScroll);
    if (ghit == 0) { pkGroup = -1; pkScroll = 0; } else if (ghit > 0) { pkGroup = gl[ghit - 1]; pkScroll = 0; }
    if (b.groups.empty()) font->drawWrapped(cv, *ts, gx + 6, gy + 42, gw - 12, "No groups on this sheet yet. Right-click a tile > New group, or make them in the Tileset tab.", DIM);
    // main area
    int ax = gx + gw + 6, ay = gy, aw = x + w - 6 - ax, ah = gh;
    cv.fillRect(ax, ay, aw, ah, RGB(18, 18, 22));
    bool over = ui.hover(ax, ay, aw, ah); Tile hov; int hx = 0, hy = 0;
    if (!pkLayout) {
        int sorted = 0; auto tiles = pickerTiles(s, pkCat, pkGroup, sorted);
        int per = std::max(1, (aw - 12) / CELL);
        int sortedRows = (sorted + per - 1) / per, rest = (int)tiles.size() - sorted, headH = rest > 0 ? 20 : 0, restRows = (rest + per - 1) / per;
        int total = (sortedRows + restRows) * CELL + headH;
        if (over) pkScroll -= in.wheel * 52;
        if (!ui.typing()) { if (in.pressed[K_PAGEDOWN]) pkScroll += ah - CELL; if (in.pressed[K_PAGEUP]) pkScroll -= ah - CELL; if (in.down[K_DOWN]) pkScroll += 8; if (in.down[K_UP]) pkScroll -= 8; }
        pkScroll = clampi(pkScroll, 0, std::max(0, total - ah));
        cv.setClip({ax, ay, aw, ah});
        for (int i = 0; i < (int)tiles.size(); i++) {
            int px, py;
            if (i < sorted) { px = ax + 2 + (i % per) * CELL; py = ay + 2 + (i / per) * CELL - pkScroll; }
            else { int j = i - sorted; px = ax + 2 + (j % per) * CELL; py = ay + 2 + sortedRows * CELL + headH + (j / per) * CELL - pkScroll; }
            if (py + CELL < ay || py > ay + ah) continue;
            ts->drawOpaque(cv, tiles[i], px + 1, py + 1, 2);
            if (sameTile(tiles[i], pkCur)) { cv.rect(px, py, CELL, CELL, ACCENT2); cv.rect(px + 1, py + 1, CELL - 2, CELL - 2, ACCENT2); }
            if (over && in.mouseX >= px && in.mouseX < px + CELL && in.mouseY >= py && in.mouseY < py + CELL) { hov = tiles[i]; hx = px; hy = py; }
        }
        if (headH) { int ly = ay + 2 + sortedRows * CELL - pkScroll; cv.fillRect(ax, ly + 3, aw, 14, PANEL2); font->draw(cv, *ts, ax + 6, ly + 4, fmt("Not sorted yet (%d) - right-click one to give it a category", rest), DIM); }
        cv.resetClip();
        if (tiles.empty()) font->drawWrapped(cv, *ts, ax + 20, ay + 20, aw - 40, pkGroup >= 0 ? "This group has no tiles yet." : "No tiles in this category on this sheet. Click All to see every tile; right-click tiles to sort them.", DIM);
        if (total > ah) { int th = std::max(16, ah * ah / total), ty = ay + (ah - th) * pkScroll / std::max(1, total - ah); cv.fillRect(ax + aw - 6, ay, 6, ah, PANEL); cv.fillRect(ax + aw - 6, ty, 6, th, BORDER); }
    } else {
        int cols = b.cols, rows = b.rows;
        std::set<int> inGroup; if (pkGroup >= 0 && pkGroup < (int)b.groups.size()) for (auto& p : b.groups[pkGroup].cells) inGroup.insert(selKey(p.first, p.second));
        if (over) { if (in.down[K_SHIFT]) tileScrollX -= in.wheel * 60; else tileScrollY -= in.wheel * 60; }
        if (in.mouseDown[1]) { tileScrollX -= in.mouseDX; tileScrollY -= in.mouseDY; }
        if (!ui.typing()) { if (in.down[K_LEFT]) tileScrollX -= 8; if (in.down[K_RIGHT]) tileScrollX += 8; if (in.down[K_UP]) tileScrollY -= 8; if (in.down[K_DOWN]) tileScrollY += 8; }
        tileScrollX = clampi(tileScrollX, 0, std::max(0, cols * CELL - aw)); tileScrollY = clampi(tileScrollY, 0, std::max(0, rows * CELL - ah));
        cv.setClip({ax, ay, aw, ah});
        int c0 = tileScrollX / CELL, r0 = tileScrollY / CELL;
        for (int r = r0; r <= r0 + ah / CELL + 1 && r < rows; r++) for (int c = c0; c <= c0 + aw / CELL + 1 && c < cols; c++) {
            int sx = ax + c * CELL - tileScrollX, sy = ay + r * CELL - tileScrollY; Tile t = T(c, r); t.s = (uint8_t)s;
            ts->drawOpaque(cv, t, sx + 1, sy + 1, 2);
            bool match = pkGroup >= 0 ? inGroup.count(selKey(c, r)) > 0 : (pkCat == 0 || b.cats[(size_t)r * cols + c] == pkCat);
            if (!match) cv.dim(sx, sy, CELL, CELL, 175);
            if (sameTile(t, pkCur)) cv.rect(sx, sy, CELL, CELL, ACCENT2);
            if (over && in.mouseX >= sx && in.mouseX < sx + CELL && in.mouseY >= sy && in.mouseY < sy + CELL && !ts->cellEmpty(t)) { hov = t; hx = sx; hy = sy; }
        }
        cv.resetClip();
    }
    pkHover = hov;
    if (hov.valid()) {
        cv.rect(hx, hy, CELL, CELL, C::White);
        std::string gs; for (auto& g : b.groups) for (auto& p : g.cells) if (p.first == hov.c && p.second == hov.r) { gs += (gs.empty() ? "" : ", ") + g.name; break; }
        ts->drawOpaque(cv, hov, x + 8, y + h - 30, 2);
        ui.text(x + 36, y + h - 24, tileText(hov) + "   " + CATS[catIndex(cellCat(hov))].name + (gs.empty() ? "" : "   groups: " + gs), ACCENT2);
        if (in.mousePressed[0] && !ui.mouseUsed) { ui.mouseUsed = true; auto cb = pkCb; close = true; if (cb) cb(hov); }
        else if (in.mousePressed[2]) openTileMenu({hov}, MENU_PICKER);
    } else ui.text(x + 8, y + h - 24, pkLayout ? "wheel: scroll  shift+wheel: sideways  middle-drag / arrows: pan" : "wheel / PgUp / PgDn: scroll    right-click a tile: category and groups", DIM);
    if (int cmd = ui.takeCmd(MENU_PICKER)) applyTileMenu(cmd, true);
    if (!close) cancelRow();
}

// ---------------- the Tileset tab ----------------
std::vector<Tile> Editor::tsSelTiles() {
    std::vector<Tile> v; for (int k : tsSel) { Tile t = T(k % 4096, k / 4096); t.s = (uint8_t)tsSheet; v.push_back(t); } return v;
}
void Editor::drawTileset(InputState& in) {
    syncBooks();
    Canvas& cv = *ui.cv;
    int x = 4, y = TOP + 4, w = W - 8, h = H - TOP - BOTTOM - 8, lw = 226;
    auto vis = visibleSheets();
    if (!vis.empty() && std::find(vis.begin(), vis.end(), tsSheet) == vis.end()) { tsSheet = vis[0]; tsSel.clear(); tsGroupSel = -1; }
    bool haveSheet = !vis.empty() && tsSheet < (int)books.size();
    bool base = dataRoot == baseRoot;
    // ---- left: sheets ----
    ui.panel(x, y, lw, h);
    ui.header(x, y, lw, fmt("Sprite sheets (%d)", (int)vis.size())); helpBtn(x + lw - 16, y + 1, "tab.tileset");
    std::vector<std::string> names; int sel = -1;
    for (size_t i = 0; i < vis.size(); i++) { auto& sp = ts->sheets[vis[i]]; names.push_back(sp.name + fmt("  %dx%d", ts->sheetCols(vis[i]), ts->sheetRows(vis[i])) + (sp.own ? "" : "  (base game)")); if (vis[i] == tsSheet) sel = (int)i; }
    int ly = y + 18;
    int hit = ui.listBox(x + 2, ly, lw - 4, 62, names, sel, tsSheetScroll);
    if (hit >= 0) { tsSheet = vis[hit]; tsSel.clear(); tsGroupSel = -1; tileScrollX = tileScrollY = 0; }
    if (vis.empty()) ui.text(x + 8, ly + 4, "none yet", DIM);
    ly += 66;
    const SheetPics* cur = haveSheet ? ts->sheet(tsSheet) : nullptr;
    bool ownCur = cur && cur->own && cur->loaded;
    if (ui.button(x + 4, ly, 108, 18, "Import picture...")) openSheetImport("");
    if (ui.hover(x + 4, ly, 108, 18)) ui.tip("Add a PNG sprite sheet to this campaign (you can also drop a PNG file on the Editor window). You set how it is cut into tiles.");
    bool ownTiles = !base && fileExists(sheetsDir() + "/tiles.png");
    if (ui.button(x + 116, ly, 106, 18, "Copy base game's", !base && !ownTiles))
        openConfirm("Copy the base game's sprite sheet (tiles.png, with its categories and groups) into this campaign's sheets folder? Sprites without @sheet then come from the campaign's copy, which you can repaint.", [this]() { std::string err; if (copyBaseSheets(err)) say("Copied the base game's sheet into " + sheetsDir() + "."); else say(err); });
    if (ui.hover(x + 116, ly, 106, 18)) ui.tip(base ? "This is the base game." : ownTiles ? "This campaign already has its own tiles.png." : "Start this campaign's art from the base game's sheet (a copy in the campaign's sheets folder).");
    ly += 22;
    if (ui.button(x + 4, ly, 70, 18, "Grid...", ownCur)) openSheetImport(cur->path, tsSheet);
    if (ui.hover(x + 4, ly, 70, 18)) ui.tip(ownCur ? "How this picture is cut into tiles, and its see-through colour" : "Only a sheet in this campaign's own folder (not the base game's built-in tiles.png) has a grid to change");
    if (ui.button(x + 78, ly, 66, 18, "Reload", haveSheet)) { reloadSheets(); say("Sheets reloaded from disk (pictures repainted outside the game show now)."); }
    if (ui.button(x + 148, ly, 74, 18, "Remove", ownCur, false, DANGER)) {
        std::string nm = cur->name; int s = tsSheet;
        openConfirm("Remove the sheet '" + nm + "' from this campaign? This deletes sheets/" + nm + ".png and .txt. Sprites on it then draw nothing (Check all lists them).", [this, s]() { std::string err; if (removeSheet(s, err)) { say("Sheet removed."); tsSel.clear(); } else say(err); });
    }
    ly += 26;
    // ---- left: categories ----
    int cnt[NCATS] = {};
    if (haveSheet) { SheetBook& b = books[tsSheet]; for (int r = 0; r < b.rows; r++) for (int c = 0; c < b.cols; c++) { Tile t = T(c, r); t.s = (uint8_t)tsSheet; if (!ts->cellEmpty(t)) cnt[catIndex(b.cats[(size_t)r * b.cols + c])]++; } }
    ui.header(x, ly, lw, "Categories (click: show only)"); ly += 18;
    for (int i = 0; i < NCATS; i++) {
        int ry = ly + i * 16; bool on = tsCatFilter == CATS[i].k, hv = ui.hover(x + 2, ry, lw - 4, 16);
        if (on) cv.fillRect(x + 2, ry, lw - 4, 16, SEL); else if (hv) cv.fillRect(x + 2, ry, lw - 4, 16, HOVER);
        cv.fillRect(x + 8, ry + 3, 10, 10, CATS[i].col);
        ui.text(x + 24, ry + 2, fmt("%d  %s", i + 1, CATS[i].name), TEXT); ui.textRight(x + lw - 8, ry + 2, std::to_string(cnt[i]), DIM);
        if (hv) ui.tip(CATS[i].about + std::string(" Key ") + std::to_string(i + 1) + " gives the selected tiles this category.");
        if (ui.clicked(x + 2, ry, lw - 4, 16)) { tsCatFilter = on ? 0 : CATS[i].k; tsGroupSel = -1; }
    }
    ly += NCATS * 16 + 4;
    // ---- left: groups ----
    std::vector<int> gl; if (haveSheet) gl = shownGroups(tsSheet, 0, tsGroupFind);
    ui.header(x, ly, lw, haveSheet ? fmt("Groups (%d)", (int)books[tsSheet].groups.size()) : std::string("Groups")); ly += 18;
    ui.searchField(x + 4, ly, lw - 8, tsGroupFind, "find a group"); ly += 20;
    std::vector<std::string> gnames; int gsel = -1;
    for (size_t i = 0; i < gl.size(); i++) { auto& g = books[tsSheet].groups[gl[i]]; gnames.push_back(g.name + fmt("  (%d)", (int)g.cells.size())); if (gl[i] == tsGroupSel) gsel = (int)i; }
    int listH = y + h - ly - 48;
    int ghit = ui.listBox(x + 2, ly, lw - 4, listH, gnames, gsel, tsGroupScroll);
    if (ghit >= 0) { tsGroupSel = gl[ghit] == tsGroupSel ? -1 : gl[ghit]; tsCatFilter = 0; }
    ly += listH + 4;
    int nsel = (int)tsSel.size();
    bool gOk = haveSheet && tsGroupSel >= 0 && tsGroupSel < (int)books[tsSheet].groups.size();
    if (ui.button(x + 4, ly, 72, 18, "New...", haveSheet)) {
        int s = tsSheet; auto tiles = tsSelTiles();
        openTextPrompt(nsel ? fmt("Name of the new group (with the %d selected tiles)", nsel) : std::string("Name of the new group"), "", [this, s, tiles](const std::string& nm0) {
            std::string nm = trim(nm0); if (nm.empty()) return;
            SheetGroup g; g.name = nm; for (auto& t : tiles) g.cells.push_back({t.c, t.r});
            books[s].groups.push_back(g); tsGroupSel = (int)books[s].groups.size() - 1;
            if (saveBook(s)) say("Group " + nm + fmt(" made with %d tiles (saved).", (int)tiles.size()));
        });
    }
    if (ui.button(x + 80, ly, 70, 18, "Rename...", gOk)) {
        int s = tsSheet, gi = tsGroupSel;
        openTextPrompt("New name of the group", books[s].groups[gi].name, [this, s, gi](const std::string& nm0) { std::string nm = trim(nm0); if (nm.empty() || gi >= (int)books[s].groups.size()) return; books[s].groups[gi].name = nm; if (saveBook(s)) say("Group renamed (saved)."); });
    }
    if (ui.button(x + 154, ly, 68, 18, "Delete", gOk, false, DANGER)) {
        int s = tsSheet, gi = tsGroupSel;
        openConfirm("Delete the group '" + books[s].groups[gi].name + "'? (The tiles stay on the sheet.)", [this, s, gi]() { if (gi < (int)books[s].groups.size()) { books[s].groups.erase(books[s].groups.begin() + gi); tsGroupSel = -1; if (saveBook(s)) say("Group deleted (saved)."); } });
    }
    ly += 22;
    if (ui.button(x + 4, ly, 108, 18, nsel ? fmt("+ Add %d", nsel) : std::string("+ Add selected"), gOk && nsel > 0)) { pkMenuTiles = tsSelTiles(); auto& cells = books[tsSheet].groups[tsGroupSel].cells; int n = 0;
        for (auto& t : pkMenuTiles) { bool have = false; for (auto& p : cells) if (p.first == t.c && p.second == t.r) have = true; if (!have) { cells.push_back({t.c, t.r}); n++; } }
        if (saveBook(tsSheet)) say(fmt("Added %d to the group (saved).", n)); }
    if (ui.hover(x + 4, ly, 108, 18)) ui.tip("Add the selected tiles to the group picked in the list");
    if (ui.button(x + 116, ly, 106, 18, "- Remove selected", gOk && nsel > 0)) { pkMenuTiles = tsSelTiles(); applyTileMenu(MC_REMOVE + tsGroupSel, false); }
    // ---- right: the sheet ----
    int mx = x + lw + 6, mw = x + w - mx;
    ui.panel(mx, y, mw, h);
    if (!haveSheet) {
        ui.header(mx, y, mw, "No sprite sheets");
        font->drawWrapped(cv, *ts, mx + 24, y + 40, mw - 48, base ? "The base game's sheet (assets/tiles.png) did not load." :
            "This campaign has no sprite sheets yet (a new campaign starts without any). Import a picture - a PNG of tiles in a grid, any tile size (the game draws them 12x12) - or drop a PNG file on this window. Or copy the base game's sheet into the campaign and start from that. Until then the pictures the game draws itself (chests, doors...) and anything without @sheet come from the base game's sheet.", TEXT);
        if (ui.button(mx + 24, y + 130, 180, 24, "Import picture...")) openSheetImport("");
        if (ui.button(mx + 212, y + 130, 220, 24, "Copy the base game's sheet", !base && !ownTiles)) { std::string err; if (copyBaseSheets(err)) say("Copied the base game's sheet into " + sheetsDir() + "."); else say(err); }
        return;
    }
    SheetBook& b = books[tsSheet];
    ui.header(mx, y, mw, b.name + (b.grid.title.empty() ? "" : " (" + b.grid.title + ")") + " - click / drag to select, Ctrl adds, Shift extends, 1-9 category, right-click: more");
    int ax = mx + 4, ay = y + 20, aw = mw - 8, ah = h - 20 - 48, cols = b.cols, rows = b.rows;
    bool over = ui.hover(ax, ay, aw, ah);
    if (over) { if (in.down[K_SHIFT] && !in.mouseDown[0]) tileScrollX -= in.wheel * 60; else tileScrollY -= in.wheel * 60; }
    if (in.mouseDown[1] || in.mouseDown[2]) { tileScrollX -= in.mouseDX; tileScrollY -= in.mouseDY; }
    if (in.mousePressed[2]) { tsRDownX = in.mouseX; tsRDownY = in.mouseY; }
    if (!ui.typing() && pop == P_NONE) { if (in.down[K_LEFT]) tileScrollX -= 8; if (in.down[K_RIGHT]) tileScrollX += 8; if (in.down[K_UP]) tileScrollY -= 8; if (in.down[K_DOWN]) tileScrollY += 8; }
    tileScrollX = clampi(tileScrollX, 0, std::max(0, cols * CELL - aw)); tileScrollY = clampi(tileScrollY, 0, std::max(0, rows * CELL - ah));
    std::set<int> inGroup; if (tsGroupSel >= 0 && tsGroupSel < (int)b.groups.size()) for (auto& p : b.groups[tsGroupSel].cells) inGroup.insert(selKey(p.first, p.second));
    cv.fillRect(ax, ay, aw, ah, RGB(18, 18, 22)); cv.setClip({ax, ay, aw, ah});
    int c0 = tileScrollX / CELL, r0 = tileScrollY / CELL;
    for (int r = r0; r <= r0 + ah / CELL + 1 && r < rows; r++) for (int c = c0; c <= c0 + aw / CELL + 1 && c < cols; c++) {
        int sx = ax + c * CELL - tileScrollX, sy = ay + r * CELL - tileScrollY; Tile t = T(c, r); t.s = (uint8_t)tsSheet;
        ts->drawOpaque(cv, t, sx + 1, sy + 1, 2);
        if (ts->cellEmpty(t)) continue;
        char k = b.cats[(size_t)r * cols + c];
        bool match = tsGroupSel >= 0 ? inGroup.count(selKey(c, r)) > 0 : (tsCatFilter == 0 || k == tsCatFilter);
        if (!match) cv.dim(sx, sy, CELL, CELL, 175);
        if (k != '.') cv.fillRect(sx + 2, sy + CELL - 2, CELL - 4, 2, CATS[catIndex(k)].col);   // no mark = not sorted
        if (tsSel.count(selKey(c, r))) { cv.rect(sx, sy, CELL, CELL, ACCENT2); cv.rect(sx + 1, sy + 1, CELL - 2, CELL - 2, ACCENT2); }
    }
    for (int c = c0; c <= c0 + aw / CELL + 1 && c < cols; c++) if (c % 5 == 0) font->draw(cv, *ts, ax + c * CELL - tileScrollX + 2, ay + 1, std::to_string(c), Color8{255, 255, 0, 150});
    for (int r = r0; r <= r0 + ah / CELL + 1 && r < rows; r++) if (r % 5 == 0) font->draw(cv, *ts, ax + 2, ay + r * CELL - tileScrollY + 2, std::to_string(r), Color8{255, 255, 0, 150});
    cv.resetClip();
    int hc = (in.mouseX - ax + tileScrollX) / CELL, hr = (in.mouseY - ay + tileScrollY) / CELL;
    bool onCell = over && in.mouseX >= ax && in.mouseY >= ay && hc >= 0 && hr >= 0 && hc < cols && hr < rows;
    Tile hov; if (onCell) { hov = T(hc, hr); hov.s = (uint8_t)tsSheet; tsHover = hov; int sx = ax + hc * CELL - tileScrollX, sy = ay + hr * CELL - tileScrollY; cv.rect(sx, sy, CELL, CELL, C::White); }
    auto rectSel = [&](int ca, int ra, int cb, int rb) {
        for (int r = std::min(ra, rb); r <= std::max(ra, rb); r++) for (int c = std::min(ca, cb); c <= std::max(ca, cb); c++) { Tile t = T(c, r); t.s = (uint8_t)tsSheet; if (!ts->cellEmpty(t)) tsSel.insert(selKey(c, r)); }
    };
    if (onCell && in.mousePressed[0] && !ui.mouseUsed) {
        ui.mouseUsed = true;
        if (in.ctrl()) { int k = selKey(hc, hr); if (tsSel.count(k)) tsSel.erase(k); else if (!ts->cellEmpty(hov)) tsSel.insert(k); tsAnchor = k; }
        else if (in.shift() && tsAnchor >= 0) rectSel(tsAnchor % 4096, tsAnchor / 4096, hc, hr);
        else { tsSel.clear(); tsDragging = true; tsDragC = hc; tsDragR = hr; tsAnchor = selKey(hc, hr); if (!ts->cellEmpty(hov)) tsSel.insert(tsAnchor); }
        tsPick = hov;
    }
    if (tsDragging) {
        if (!in.mouseDown[0]) tsDragging = false;
        else if (onCell) { tsSel.clear(); rectSel(tsDragC, tsDragR, hc, hr); }
    }
    if (onCell && in.mouseReleased[2] && abs(in.mouseX - tsRDownX) + abs(in.mouseY - tsRDownY) < 5) {   // a right click (not a drag)
        if (!tsSel.count(selKey(hc, hr))) { tsSel.clear(); if (!ts->cellEmpty(hov)) tsSel.insert(selKey(hc, hr)); }
        if (!tsSel.empty()) openTileMenu(tsSelTiles(), MENU_TILESET);
    }
    if (!ui.typing() && pop == P_NONE && !ui.menuOpen) {
        for (int i = 0; i < NCATS && i < 9; i++) if (in.pressed[K_1 + i] && !tsSel.empty()) setCats(tsSelTiles(), CATS[i].k);
        if (in.ctrl() && in.pressed[K_A]) { tsSel.clear(); for (int r = 0; r < rows; r++) for (int c = 0; c < cols; c++) { Tile t = T(c, r); t.s = (uint8_t)tsSheet; if (ts->cellEmpty(t)) continue; bool m = tsGroupSel >= 0 ? inGroup.count(selKey(c, r)) > 0 : (tsCatFilter == 0 || b.cats[(size_t)r * cols + c] == tsCatFilter); if (m) tsSel.insert(selKey(c, r)); } }
    }
    if (int cmd = ui.takeCmd(MENU_TILESET)) applyTileMenu(cmd, false);
    // ---- bottom: selection actions + hover info ----
    int by = y + h - 46;
    ui.text(ax, by + 3, nsel ? fmt("%d selected:", (int)tsSel.size()) : std::string("Select tiles, then:"), nsel ? ACCENT2 : DIM);
    int bx = ax + 96;
    for (int i = 0; i < NCATS; i++) {
        int bw = font->width(CATS[i].name) + 10;
        if (bx + bw > ax + aw) break;
        if (ui.button(bx, by, bw, 18, CATS[i].name, !tsSel.empty(), false, CATS[i].col)) setCats(tsSelTiles(), CATS[i].k);
        bx += bw + 2;
    }
    std::string info;
    if (hov.valid() && !ts->cellEmpty(hov)) {
        info = tileText(hov) + "  " + CATS[catIndex(cellCat(hov))].name;
        std::string gs; for (auto& g : b.groups) for (auto& p : g.cells) if (p.first == hov.c && p.second == hov.r) { gs += (gs.empty() ? "" : ", ") + g.name; break; }
        if (!gs.empty()) info += "  groups: " + gs;
        std::string uses = tileUses(hov); if (!uses.empty()) info += "  used by: " + uses;
    } else if (tsPick.valid() && tsPick.s == tsSheet) info = "picked " + tileText(tsPick) + "  (type it in a sprite field, or use the field's pick... button)";
    cv.setClip({ax, by + 22, aw - 34, 16}); ui.text(ax, by + 25, info, TEXT); cv.resetClip();
    if (hov.valid()) ts->drawOpaque(cv, hov, ax + aw - 26, by + 20, 2);
}

// ---------------- import / grid window (P_SHEET) ----------------
void Editor::openSheetImport(const std::string& path, int editSheet) {
    siEdit = editSheet; siErr.clear(); siPickKey = false; siGame = false; siScrollX = siScrollY = 0; siImg = Image8(); siConvSig.clear();
    const SheetPics* sp = editSheet >= 0 && ts ? ts->sheet(editSheet) : nullptr;
    if (sp) { siName = sp->name; siGrid = sp->grid; siTitle = siGrid.title; siPath = sp->path; siImg.load(siPath); siZoom = siImg.w > 1000 ? 1 : 2; }
    else { siEdit = -1; siName.clear(); siTitle.clear(); siGrid = SheetGrid(); siPath.clear(); if (!path.empty()) siLoad(path); }
    popCancel = nullptr; popOpenedFrame = frameNo; pop = P_SHEET; ui.modal = true; ui.blur();
}
void Editor::siLoad(const std::string& p0) {
    std::string p = unquote(p0); siPath = p; siErr.clear(); siConvSig.clear();
    Image8 im; if (!im.load(p)) { siErr = "Could not read a PNG picture at that path."; siImg = Image8(); return; }
    siImg = im; SheetGrid g = guessSheetGrid(im.w, im.h);
    bool alpha = false; for (auto& px : im.px) if (px.a < 128) { alpha = true; break; }
    g.clear = alpha ? 1 : 0; siGrid = g;
    if (siEdit < 0) { siName = slug(fs::path(p).stem().string()); if (siName.empty()) siName = "sheet"; siTitle = fs::path(p).filename().string(); }
    siScrollX = siScrollY = 0; siZoom = im.w > 1000 ? 1 : 2;
}
void Editor::drawSheetImport(InputState& in, bool& close) {
    Canvas& cv = *ui.cv;
    int x = 20, y = 30, w = W - 40, h = H - 60;
    ui.panel(x, y, w, h); ui.header(x, y, w, siEdit >= 0 ? "Sprite sheet '" + siName + "' - how it is cut into tiles" : "Import a sprite sheet into " + (dataRoot == baseRoot ? std::string("the base game") : "this campaign (" + campaignId + ")"));
    helpBtn(x + w - 16, y + 1, "sheet.import", "tab.tileset");
    for (auto& d : in.dropped) if (toLower(fs::path(d).extension().string()) == ".png" && siEdit < 0) { siLoad(d); break; }
    ui.idCounter = 6200;
    int lx = x + 10, fx = x + 104, fw = 210, ry = y + 26;
    ui.label(lx, ry + 3, "Picture file");
    if (siEdit < 0) {
        ui.textField(fx, ry, fw - 46, siPath, 400);
        if (ui.button(fx + fw - 42, ry, 42, 16, "Load") || (ui.focus == 6201 && in.pressed[K_ENTER])) siLoad(siPath);
        ry += 18; ui.text(fx, ry, "or drop a PNG file on this window", DIM);
    } else { cv.setClip({fx, ry, fw, 16}); ui.text(fx, ry + 3, fs::path(siPath).filename().string(), TEXT); cv.resetClip(); }
    ry += 18;
    if (siImg.w) ui.text(fx, ry, fmt("%d x %d pixels", siImg.w, siImg.h), DIM);
    ry += 22;
    ui.label(lx, ry + 3, "Name");
    if (siEdit < 0) { ui.textField(fx, ry, fw, siName, 40); if (ui.hover(fx, ry, fw, 16)) ui.tip("The sheet's id: sprites on it are written col,row@" + (slug(siName).empty() ? std::string("name") : slug(siName)) + ". Calling it 'tiles' makes it this campaign's main sheet."); }
    else ui.text(fx, ry + 3, siName, TEXT);
    ry += 20;
    ui.label(lx, ry + 3, "Title"); ui.textField(fx, ry, fw, siTitle, 80); if (ui.hover(fx, ry, fw, 16)) ui.tip("A note for yourself: where the art came from, its author and licence...");
    ry += 26;
    ui.header(lx - 6, ry, 318, "Cut into tiles"); ry += 20;
    bool ch = false;
    ui.label(lx, ry + 3, "Tile size"); ch |= ui.intField(fx, ry, 50, siGrid.tw, 1, 256); ui.label(fx + 54, ry + 3, "x"); ch |= ui.intField(fx + 64, ry, 50, siGrid.th, 1, 256); ry += 20;
    ui.label(lx, ry + 3, "First tile at"); ch |= ui.intField(fx, ry, 50, siGrid.ox, 0, 4096); ui.label(fx + 54, ry + 3, ","); ch |= ui.intField(fx + 64, ry, 50, siGrid.oy, 0, 4096); ry += 20;
    ui.label(lx, ry + 3, "Gap between"); ch |= ui.intField(fx, ry, 50, siGrid.gx, 0, 256); ui.label(fx + 54, ry + 3, ","); ch |= ui.intField(fx + 64, ry, 50, siGrid.gy, 0, 256);
    if (ui.button(fx + 124, ry - 40, 86, 16, "Guess", siImg.w > 0)) { int c = siGrid.clear; Color8 k = siGrid.key; siGrid = guessSheetGrid(siImg.w, siImg.h); siGrid.clear = c; siGrid.key = k; ch = true; }
    if (ui.hover(fx + 124, ry - 40, 86, 16)) ui.tip("Guess the grid from the picture's size (12, 16, 8, 24, 32 px tiles, with or without 1 px lines between them)");
    ry += 22;
    int cols = siImg.w ? siGrid.cols(siImg.w) : 0, rows = siImg.h ? siGrid.rows(siImg.h) : 0;
    if (siImg.w) ui.text(lx, ry, fmt("= %d x %d tiles", std::max(0, cols), std::max(0, rows)), cols > 0 && rows > 0 ? OK : DANGER);
    ry += 16;
    if (siGrid.tw != 12 || siGrid.th != 12) { font->drawWrapped(cv, *ts, lx, ry, 310, "The game draws every tile 12x12: these are scaled to fit (pixel art looks best at 12x12).", DIM); ry += 26; }
    ry += 6;
    ui.header(lx - 6, ry, 318, "See-through pixels"); ry += 20;
    if (ui.button(lx, ry, 62, 18, "Black", true, siGrid.clear == 0)) { siGrid.clear = 0; ch = true; }
    if (ui.hover(lx, ry, 62, 18)) ui.tip("Black and transparent pixels are left out (the base sheet's way: art drawn on black)");
    if (ui.button(lx + 66, ry, 118, 18, "Transparent only", true, siGrid.clear == 1)) { siGrid.clear = 1; ch = true; }
    if (ui.hover(lx + 66, ry, 118, 18)) ui.tip("Only the picture's transparent pixels are left out; black stays black");
    if (ui.button(lx + 188, ry, 118, 18, "One colour", true, siGrid.clear == 2)) { siGrid.clear = 2; ch = true; }
    if (ui.hover(lx + 188, ry, 118, 18)) ui.tip("Pixels of one colour (and transparent ones) are left out, e.g. a magenta background");
    ry += 22;
    if (siGrid.clear == 2) {
        int r = siGrid.key.r, g = siGrid.key.g, bb = siGrid.key.b;
        if (ui.intField(lx, ry, 46, r, 0, 255) | ui.intField(lx + 50, ry, 46, g, 0, 255) | ui.intField(lx + 100, ry, 46, bb, 0, 255)) { siGrid.key = Color8{(uint8_t)r, (uint8_t)g, (uint8_t)bb, 255}; ch = true; }
        cv.fillRect(lx + 150, ry, 18, 16, siGrid.key); cv.rect(lx + 150, ry, 18, 16, BORDER);
        if (ui.button(lx + 174, ry, 132, 16, siPickKey ? "click the picture..." : "Pick in the picture", siImg.w > 0, siPickKey)) siPickKey = !siPickKey;
        ry += 22;
    }
    if (ch) siConvSig.clear();
    // buttons + messages
    std::string nm = slug(siName);
    if (!siErr.empty()) font->drawWrapped(cv, *ts, lx, y + h - 76, 310, siErr, DANGER);
    else if (siEdit < 0 && nm == "tiles") font->drawWrapped(cv, *ts, lx, y + h - 88, 310, "'tiles' replaces the base sheet in this campaign: everything without @sheet - and the pictures the game draws itself, like chests and doors - then comes from this picture.", ACCENT2);
    else if (siEdit < 0 && !nm.empty() && fileExists(sheetsDir() + "/" + nm + ".png")) font->drawWrapped(cv, *ts, lx, y + h - 76, 310, "A sheet called '" + nm + "' is already here: importing replaces its picture and keeps its categories and groups.", ACCENT2);
    bool ok = siImg.w > 0 && cols > 0 && rows > 0 && !nm.empty();
    if (ui.button(lx, y + h - 30, 120, 22, siEdit >= 0 ? "Save the grid" : "Import", ok, false, OK)) {
        siGrid.title = siTitle;
        if (siEdit >= 0) {
            syncBooks(); int s = siEdit;
            if (s < (int)books.size()) { books[s].grid = siGrid; if (saveBook(s)) { reloadSheets(); say("Grid saved: sheet '" + siName + fmt("' is %d x %d tiles.", cols, rows)); } }
            close = true;
        } else {
            std::string err; int s = sheetImport(siPath, nm, siGrid, err);
            if (s < 0) siErr = err;
            else { close = true; tsSheet = s; tsSel.clear(); tsGroupSel = -1; tsCatFilter = 0; tab = 13; tileScrollX = tileScrollY = 0;
                say("Imported '" + nm + fmt("' (%d x %d tiles). Sort its tiles into categories here (select, then keys 1-9); pickers offer it now.", cols, rows)); }
        }
    }
    if (!close && (ui.button(lx + 128, y + h - 30, 80, 22, "Cancel") || (in.pressed[K_ESCAPE] && !ui.typing()))) close = true;
    // ---- preview ----
    int px = x + 340, py = y + 46, pw = x + w - 8 - px, ph = h - 54;
    ui.text(px, y + 26, "Zoom", DIM);
    static const char* zl[] = {"1/2", "1x", "2x", "3x", "4x"};
    for (int i = 0; i < 5; i++) if (ui.button(px + 40 + i * 38, y + 22, 34, 18, zl[i], true, siZoom == i)) { siZoom = i; siScrollX = siScrollY = 0; }
    ui.checkbox(px + 240, y + 25, "As the game cuts it", siGame);
    if (ui.hover(px + 240, y + 24, 150, 14)) ui.tip("Show the tiles as the game will have them: cut by the grid, scaled to 12x12, see-through pixels black");
    cv.fillRect(px, py, pw, ph, RGB(12, 12, 16)); cv.rect(px - 1, py - 1, pw + 2, ph + 2, BORDER);
    if (!siImg.w) { font->drawWrapped(cv, *ts, px + 20, py + 20, pw - 40, siEdit < 0 ? "Type the path of a PNG file and press Load, or drop the file on this window." : "The picture could not be read.", DIM); return; }
    const Image8* src = &siImg; int sw = siImg.w, sh = siImg.h;
    if (siGame) {
        std::string sig = siPath + sheetGridText(siGrid);
        if (sig != siConvSig) { siConvSig = sig; Image8 tmp; if (convertSheet(siImg, siGrid, tmp, siConvCols, siConvRows)) siConv = tmp; else siConv = Image8(); }
        if (siConv.w) { src = &siConv; sw = siConv.w; sh = siConv.h; }
    }
    int num = siZoom == 0 ? 1 : siZoom, den = siZoom == 0 ? 2 : 1;
    int vw = sw * num / den, vh = sh * num / den;
    bool overP = ui.hover(px, py, pw, ph);
    if (overP) { if (in.down[K_SHIFT]) siScrollX -= in.wheel * 60; else siScrollY -= in.wheel * 60; }
    if (overP && (in.mouseDown[1] || in.mouseDown[2] || (in.mouseDown[0] && !siPickKey))) { siScrollX -= in.mouseDX; siScrollY -= in.mouseDY; }
    siScrollX = clampi(siScrollX, 0, std::max(0, vw - pw)); siScrollY = clampi(siScrollY, 0, std::max(0, vh - ph));
    cv.setClip({px, py, pw, ph});
    if (den == 1) cv.blit(*src, siScrollX / num, siScrollY / num, pw / num + 2, ph / num + 2, px - siScrollX % num, py - siScrollY % num, num);
    else for (int yy = 0; yy < ph && yy < vh - siScrollY; yy++) for (int xx = 0; xx < pw && xx < vw - siScrollX; xx++) {
        int sx = (xx + siScrollX) * den, sy = (yy + siScrollY) * den; if (sx >= sw || sy >= sh) continue;
        Color8 c = src->px[(size_t)sy * sw + sx]; if (c.a) cv.put(px + xx, py + yy, c);
    }
    // the grid over the picture
    if (!siGame && cols > 0 && rows > 0) {
        Color8 gc{90, 160, 255, 110};
        int pitchX = siGrid.tw + siGrid.gx, pitchY = siGrid.th + siGrid.gy;
        int c0 = std::max(0, (siScrollX * den / num - siGrid.ox) / std::max(1, pitchX) - 1), r0 = std::max(0, (siScrollY * den / num - siGrid.oy) / std::max(1, pitchY) - 1);
        for (int r = r0; r < rows; r++) {
            int ty = py + (siGrid.oy + r * pitchY) * num / den - siScrollY; if (ty > py + ph) break;
            for (int c = c0; c < cols; c++) {
                int tx = px + (siGrid.ox + c * pitchX) * num / den - siScrollX; if (tx > px + pw) break;
                cv.rect(tx, ty, std::max(1, siGrid.tw * num / den), std::max(1, siGrid.th * num / den), gc);
            }
        }
    } else if (siGame) {
        Color8 gc{90, 160, 255, 60};
        for (int c = 0; c <= siConvCols; c++) { int tx = px + c * Tileset::PITCH * num / den - siScrollX; if (tx >= px && tx < px + pw) cv.vline(tx, py, ph, gc); }
        for (int r = 0; r <= siConvRows; r++) { int ty = py + r * Tileset::PITCH * num / den - siScrollY; if (ty >= py && ty < py + ph) cv.hline(px, ty, pw, gc); }
    }
    cv.resetClip();
    if (overP) {
        int ix = (in.mouseX - px + siScrollX) * den / num, iy = (in.mouseY - py + siScrollY) * den / num;
        if (ix >= 0 && iy >= 0 && ix < sw && iy < sh) {
            std::string hint = fmt("pixel %d,%d", ix, iy);
            if (!siGame && cols > 0) { int pitchX = siGrid.tw + siGrid.gx, pitchY = siGrid.th + siGrid.gy; int tc = (ix - siGrid.ox) / std::max(1, pitchX), tr = (iy - siGrid.oy) / std::max(1, pitchY); if (ix >= siGrid.ox && iy >= siGrid.oy && tc < cols && tr < rows) hint += fmt("   tile %d,%d", tc, tr); }
            else if (siGame) hint += fmt("   tile %d,%d", ix / Tileset::PITCH, iy / Tileset::PITCH);
            cv.fillRect(px + 2, py + ph - 16, font->width(hint) + 8, 14, RGB(0, 0, 0, 200)); ui.text(px + 6, py + ph - 15, hint, ACCENT2);
            if (siPickKey && !siGame && in.mousePressed[0]) { Color8 c = siImg.px[(size_t)iy * siImg.w + ix]; siGrid.key = Color8{c.r, c.g, c.b, 255}; siGrid.clear = 2; siPickKey = false; siConvSig.clear(); in.eatMouse(); }
        }
    }
}
