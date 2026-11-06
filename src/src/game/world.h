// Maps: tiles, props and the objects living on them (NPCs, enemies, chests, exits...)
#pragma once
#include <string>
#include <vector>
#include <map>
#include "../core/gfx.h"
#include "../core/util.h"
#include "data.h"

struct TileKind {
    std::string name;
    std::vector<Tile> ground;   // one is chosen at random per cell
    Tile prop;                   // optional overlay drawn on top of ground
    bool walkable = true, blocksSight = false, water = false, opaque = true;
    Color8 tint = C::White;      // tint applied to ground
    Color8 bg = C::Black;        // background fill under transparent ground tiles
    int encounter = 0;           // overworld: encounter chance weight
    std::string biome;           // overworld biome name
    int moveCost = 1;
    int light = 0;               // light radius this tile gives off in dark maps (torches, braziers, crystals...)
};
const TileKind* tileKind(const std::string& name);
const std::vector<TileKind>& allTileKinds();
std::vector<TileKind>& editableTileKinds();      // editor access
int tileKindIndex(const std::string& name);       // -1 if missing
void rebuildTileKindIndex();                      // after editing names/order
bool loadTileKinds(const std::string& path);      // replaces the table (built-ins are the fallback)
bool saveTileKinds(const std::string& path);
bool loadTileKindsOverlay(const std::string& path);   // a campaign's tilekinds.txt: replaces kinds by name, adds new ones
void ensureTileKindsFile(const std::string& path); // writes the built-ins if the file is missing
std::string tileKindsText(const std::vector<TileKind>& kinds);

struct Cell {
    uint16_t kind = 0;      // index into kinds table
    Tile ground;            // resolved ground tile
    Tile prop;              // resolved prop (may be invalid)
    bool explored = false;
    bool walkable = true, blocksSight = false, water = false;
    Color8 tint = C::White;
    Color8 bg = C::Black;
    bool opaque = false;
};

struct NPC {
    std::string id, name, dialogue, shop;
    Tile tile; bool recolor = false; Color8 color = C::White;
    int x = 0, y = 0, homeX = 0, homeY = 0;
    bool wander = false, hidden = false; std::string showFlag, hideFlag; // visible only when flags match
    int wanderTimer = 0;
    std::string hostileCreature; // if set, talking/attacking turns into combat with this creature (for 'enemy npcs')
    std::string craft;           // crafting station this NPC offers (forge | workbench | alchemy | any)
    std::string board;           // notice board: town id whose bounties it posts
    bool travel = false;         // coachman / ferryman: opens the world map in travel mode (paid fast travel)
    std::string moveIf; int moveX = -1, moveY = -1;   // world state: stands at moveX,moveY once moveIf is true (a guard steps aside...)
};

struct EnemyGroup {
    std::string flag;            // set when defeated (empty = respawn each visit)
    std::vector<std::string> creatures;
    int x = 0, y = 0, aggro = 5;
    bool defeated = false;
    int levelBonus = 0;
    std::string onDefeatScript;   // dialogue/script id run after victory
    std::string showFlag, hideFlag;
    int wanderTimer = 0; int homeX = 0, homeY = 0;
    std::string link;             // groups sharing a link id always fight together
};

struct Chest {
    int x = 0, y = 0; std::string flag; bool opened = false;
    std::vector<std::pair<std::string, int>> items; int gold = 0;
    std::string locked; // key item id
};

struct Exit { int x = 0, y = 0, w = 1, h = 1; std::string to; int tx = 0, ty = 0; std::string requireFlag, requireItem, lockedMsg; };
struct Trigger { int x = 0, y = 0, w = 1, h = 1; std::string script; std::string flag; bool once = true; std::string requireFlag; };
struct SignPost { int x = 0, y = 0; std::string text; };
struct Door { int x = 0, y = 0; bool open = false; std::string key; std::string flag; Tile closedTile, openTile; std::string openIf, require, lockedMsg; };
// Something in the way that the right tool, spell or deed removes: rubble (shovel / blast), vines (fire), ice (fire), a boulder (STR)...
struct Obstacle { int x = 0, y = 0, w = 1, h = 1; std::string name, kind, clearKind, needs, flag, msg, onClear; bool cleared = false; };
// A lever or switch: toggles a flag; doors (openif=), changes and obstacles react to flags.
struct Lever { int x = 0, y = 0; std::string flag, msg, name; bool toggle = true; };
// A part of the map that looks different while a condition holds (a bridge appears, a wall crumbles, water drains).
struct WorldChange { int x = 0, y = 0, w = 1, h = 1; std::string kind, cond; std::vector<uint16_t> orig; bool applied = false; };
// A resource you can harvest (ore vein, herb patch); it grows back after some days.
struct GatherNode { int x = 0, y = 0, lo = 1, hi = 1, respawn = 3; std::string item, tool, name; bool hasColor = false; Color8 color = C::White; };   // color= tints the node (default by item)
struct SavePoint { int x = 0, y = 0; };
struct ItemDrop { int x = 0, y = 0; std::string item; int count = 1; int gold = 0; };

struct Map {
    std::string id, name, kind;  // kind: overworld | town | dungeon | interior | ruins
    int w = 0, h = 0;
    std::vector<Cell> cells;
    std::vector<NPC> npcs;
    std::vector<EnemyGroup> enemies;
    std::vector<Chest> chests;
    std::vector<Exit> exits;
    std::vector<Trigger> triggers;
    std::vector<SignPost> signs;
    std::vector<Door> doors;
    std::vector<SavePoint> savepoints;
    std::map<int, std::pair<std::string, std::vector<int>>> doorShops;   // runtime cache: door cell -> (shop with hours, interior cells)
    std::vector<ItemDrop> drops;
    std::vector<Obstacle> obstacles;
    std::vector<Lever> levers;
    std::vector<WorldChange> changes;
    std::vector<GatherNode> gathers;
    Obstacle* obstacleAt(int x, int y) { for (auto& o : obstacles) if (!o.cleared && x >= o.x && y >= o.y && x < o.x + o.w && y < o.y + o.h) return &o; return nullptr; }
    Lever* leverAt(int x, int y) { for (auto& l : levers) if (l.x == x && l.y == y) return &l; return nullptr; }
    GatherNode* gatherAt(int x, int y) { for (auto& g : gathers) if (g.x == x && g.y == y) return &g; return nullptr; }
    bool dark = false;           // fog of war / light radius
    int lightRadius = 7;
    int encounterRate = 0;       // overworld: percent per step
    std::string encounterTable;  // name of encounter table
    int spawnX = 1, spawnY = 1;
    int seed = 0;
    std::string music;
    std::string ambient;         // header ambient=: the ambient loop (sounds.txt [ambient]); "none" = silence; empty = chosen by place
    std::string worldMap;        // header worldmap=: what M shows while on this map (overrides the campaign): off | self | overworld | <map id>
    Color8 bg = C::Black;
    bool generated = false;      // procedurally generated (not saved in full)

    Cell& at(int x, int y) { return cells[(size_t)y * w + x]; }
    const Cell& at(int x, int y) const { return cells[(size_t)y * w + x]; }
    bool inBounds(int x, int y) const { return x >= 0 && y >= 0 && x < w && y < h; }
    bool walkable(int x, int y) const;
    bool blocksSight(int x, int y) const;
    void set(int x, int y, const std::string& kindName, Rng& rng);
    void fill(int x, int y, int w, int h, const std::string& kindName, Rng& rng);
    void rect(int x, int y, int w, int h, const std::string& kindName, Rng& rng);
    void init(int W, int H, const std::string& kindName, Rng& rng);
    NPC* npcAt(int x, int y);
    EnemyGroup* enemyAt(int x, int y);
    Chest* chestAt(int x, int y);
    Door* doorAt(int x, int y);
    Exit* exitAt(int x, int y);
    SignPost* signAt(int x, int y);
    SavePoint* saveAt(int x, int y);
    bool occupied(int x, int y) const; // NPC/enemy present
    bool findFree(int cx, int cy, int radius, int& ox, int& oy, Rng& rng) const;
};

extern std::string gAssetDir; // set by the app; used by generators to find data files

// Load an ASCII map file. Returns false on error (error message in err).
bool loadMapFile(const std::string& path, Map& out, Rng& rng, std::string& err);
bool loadMapText(const std::string& text, Map& out, Rng& rng, std::string& err);   // same, from the file's text (editor previews of unsaved maps)
// Generated maps
// The overworld: maps/overworld.txt (hand-painted, edit it in the Editor's Maps tab) if it exists, otherwise generated
// from the seed. Locations (entrances) always come from overworld.txt, so they can be moved without repainting.
void generateOverworld(Map& m, int seed, const std::string& locationsFile = "");
void addOverworldLocations(Map& m, const std::string& locationsFile);
void buildOverworld(Map& m, int seed, const std::string& mapFile, const std::string& locationsFile);
// The same from in-memory text (the editor's live World preview): mapText empty = generate from the seed.
void buildOverworldText(Map& m, int seed, const std::string& mapText, const std::string& locationsText);
void generateDungeon(Map& m, const std::string& id, const std::string& style, int w, int h, int seed, int level, const std::string& exitTo, int exitX, int exitY, const std::vector<std::string>& monsterPool, const std::string& bossId, const std::string& bossFlag, const std::string& lootTable);
// Symmetric line-of-sight (Bresenham)
bool lineOfSight(const Map& m, int x0, int y0, int x1, int y1);
// A* path on the map (4-dir). Returns path excluding start, including goal. blockedExtra = positions to treat as blocked.
std::vector<std::pair<int, int>> findPath(const Map& m, int sx, int sy, int gx, int gy, const std::vector<std::pair<int, int>>& blocked, int maxLen = 200, bool allowGoalBlocked = false);
int manhattan(int x0, int y0, int x1, int y1);
int chebyshev(int x0, int y0, int x1, int y1);
