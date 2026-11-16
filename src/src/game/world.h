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
    std::string step;            // 1.3.3: footstep sound event when the party walks onto it (empty = "step")
    // 1.4.0 fights: breaks=KIND (cover that turns into KIND after breakhp= area damage, default 10), burns=1 (fire breaks
    // it at once and sets it alight), conduct=1 (water: shock spreads to everyone standing in the same water), flammable=1
    // (fire spells leave burning ground on it, and the fire spreads), snuff=KIND / kindle=KIND (a light source put out /
    // lit in a fight turns into that kind)
    std::string breaks, snuffTo, kindleTo; int breakHp = 0; bool burns = false, conduct = false, flammable = false;
    // 1.0.13 joined pieces (paths, rivers, shores, bridges, streets): the sheet draws these as sets of pieces that meet
    // their neighbours. edges= lists 16 ground tiles picked by which neighbours connect (mask N=1 E=2 S=4 W=8; an empty
    // entry keeps the random ground). join= names the groups this kind belongs to, link= the groups its own pieces
    // connect to (default: its join groups). under=1 fills the cell with the neighbouring ground's background, so a
    // shore or a trail sits on the grass or sand next to it. recolor=1 draws the ground flat in the tint colour.
    bool recolor = false, under = false;
    // 1.0.14 shift=N: the joined piece is drawn N sheet pixels lower (trails: the sheet draws them across the middle of
    // the cell, but people stand at the bottom of it, so without this they look like they hover over the path).
    // 1.0.16: bridges use shift=3 too (the E-W bridge then ends where the trail does, under the feet; 3 keeps the N-S
    // bridge's 3-pixel plank pattern seamless).
    int shift = 0;
    std::vector<std::string> join, link;
    std::vector<Tile> edges;
    uint32_t joinBits = 0, linkBits = 0;   // resolved from join/link (see resolveTileKindGroups)
};
// Picks a cell's ground piece and background from its neighbours (edges=/join=/under=). nb(dx, dy) returns the
// neighbouring kind, or nullptr off the map (counts as joined, so water and roads run off the edge).
// mask (optional) gets the joined sides (N1 E2 S4 W8); corners (optional) the corners where the piece is part of a
// 2x2 block of its own kind (NE1 SE2 SW4 NW8, off the map doesn't count), so 2-wide roads draw solid, not as a lattice.
constexpr int MASK_RUNS_NORTH = 16;   // extra bit tileKindLook adds to mask (see below)
template <class F> void tileKindLook(const TileKind& k, F nb, Tile& ground, Color8& bg, int* mask = nullptr, int* corners = nullptr) {
    if (mask) *mask = 0;
    if (corners) *corners = 0;
    if (k.edges.size() >= 16 && k.linkBits) {
        auto joins = [&](int dx, int dy) { const TileKind* o = nb(dx, dy); return !o || (o->joinBits & k.linkBits) != 0; };
        int m = (joins(0, -1) ? 1 : 0) | (joins(1, 0) ? 2 : 0) | (joins(0, 1) ? 4 : 0) | (joins(-1, 0) ? 8 : 0);
        if (k.edges[m].valid()) ground = k.edges[m];
        if (mask) {
            *mask = m;
            // 1.0.16: MASK_RUNS_NORTH - the piece carries on into the cell above (a trail or bridge of its own groups, not
            // the water a bridge links to), so a shifted piece fills the top of its cell instead of leaving a gap
            const TileKind* o = nb(0, -1);
            if (!o || (!o->water && (o->joinBits & (k.joinBits | k.linkBits)))) *mask |= MASK_RUNS_NORTH;
        }
        if (corners && k.joinBits) {
            auto same = [&](int dx, int dy) { const TileKind* o = nb(dx, dy); return o && (o->joinBits & k.joinBits) != 0; };
            bool n = same(0, -1), e = same(1, 0), s = same(0, 1), w = same(-1, 0);
            *corners = (n && e && same(1, -1) ? 1 : 0) | (s && e && same(1, 1) ? 2 : 0) | (s && w && same(-1, 1) ? 4 : 0) | (n && w && same(-1, -1) ? 8 : 0);
        }
    }
    if (!k.under) return;   // props with under=1 and no edges just stand on the floor around them
    // the ground most of the open neighbours stand on (sides count double; walls, doors and void don't count)
    static const int D[8][3] = {{0, -1, 2}, {1, 0, 2}, {0, 1, 2}, {-1, 0, 2}, {-1, -1, 1}, {1, -1, 1}, {1, 1, 1}, {-1, 1, 1}};
    Color8 cand[8]; int score[8], n = 0;
    for (auto& d : D) {
        const TileKind* o = nb(d[0], d[1]);
        if (!o || (o->joinBits & k.linkBits) || o->opaque || o->blocksSight || o->name == "void") continue;
        if (o->under && o->edges.empty()) continue;   // another prop: only real ground counts
        if (o->water && !k.water && k.edges.empty()) continue;   // a prop by the water stands on the land side
        int i = 0; while (i < n && !(cand[i].r == o->bg.r && cand[i].g == o->bg.g && cand[i].b == o->bg.b)) i++;
        if (i == n) { cand[n] = o->bg; score[n++] = 0; }
        score[i] += d[2];
    }
    int best = -1; for (int i = 0; i < n; i++) if (best < 0 || score[i] > score[best]) best = i;
    if (best >= 0) bg = cand[best];
}
// Draws a cell's ground piece (after its bg): plain, recoloured or opaque, and for joined pieces the 1.0.14 extras -
// shift= (moved down, with the top filled from the unshifted piece when the cell joins northwards) and solid corners
// where a recoloured trail is 2+ wide. mask/corners as tileKindLook gives them. spill=false keeps a shifted piece
// inside its cell (the Editor); spill=true (1.0.15, the game) lets it run shift pixels into the cell below, so the
// game draws shifted pieces after all the ground and before the props.
void drawGroundPiece(Canvas& cv, const Tileset& ts, const TileKind* k, Tile g, int sx, int sy, int scale, Color8 tint, bool recolor, bool opaque, int mask, int corners, bool spill = false);
void resolveTileKindGroups();                     // after loading or editing kinds: join=/link= names to bits
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
    bool recolor = false;   // ground drawn flat in the tint colour (TileKind::recolor)
};

struct NPC {
    std::string id, name, dialogue, shop;
    std::string def;             // 1.1.9: npc= - the NPC (npcs.txt) this placement shows
    Tile tile; bool recolor = false; Color8 color = C::White;
    int x = 0, y = 0, homeX = 0, homeY = 0;
    bool wander = false, hidden = false; std::string showFlag, hideFlag; // visible only when flags match
    int wanderTimer = 0;
    std::string hostileCreature; // if set, talking/attacking turns into combat with this creature (for 'enemy npcs')
    std::string craft;           // crafting station this NPC offers (forge | workbench | alchemy | any)
    std::string board;           // notice board: town id whose bounties it posts
    bool travel = false;         // coachman / ferryman: opens the world map in travel mode (paid fast travel)
    std::string script;          // 1.1.10 (#34): script=<id> - talking runs data/scripts/<id>.txt (before board / travel / dialogue)
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
    bool hidden = false;          // 1.5.0 (#84): show=/hide= says it isn't there now (Game::refreshGroupsShown keeps it up to date); enemyAt / occupied skip it
    int wanderTimer = 0; int homeX = 0, homeY = 0;
    std::string link;             // groups sharing a link id always fight together
    // 1.2.4 (#41) touch=1 maps: the group chases the party one step per party step while the party is within leash
    // tiles of its leash point (homeX/homeY); leash=0 = aggro + 5. temp = a random ambush made on the spot (it goes away
    // once it has lost the party). Not saved: a loaded game starts with nobody chasing.
    int leash = 0; bool chasing = false, temp = false;
    // 1.4.0 (#78) goal= protect:CREATURE | reach:X,Y[,W,H] | hold:ROUNDS | close:CREATURE, goaltext=, ongoal= / ongoalfail= actions
    std::string goal, goalText, onGoal, onGoalFail;
    // 1.5.0 (#99): a thrown noisemaker pulls the group to (distractX, distractY) for distractSteps party steps (it barely
    // notices the party meanwhile), then it walks home (returning). hurt = % of its HP lost to a trap before the fight.
    // None of it is saved.
    int distractX = -1, distractY = -1, distractSteps = 0, hurt = 0; bool returning = false;
};

// 1.1.1: look and use keys every object type can take from its [type] entry in objects.txt (or its template, or set
// itself): sprite / opensprite / onsprite pictures, color (drawn in one colour), sound played on use, onuse = a script
// run when it is used (the same actions as a trigger's script=: heal, item:ID:N, flag:NAME, message:Title|Text...).
struct Chest {
    int x = 0, y = 0; std::string flag; bool opened = false;
    std::vector<std::pair<std::string, int>> items; int gold = 0;
    std::string locked; // key item id
    Tile sprite, openSprite; bool hasColor = false; Color8 color = C::White; std::string sound, onUse;
};

struct Exit { int x = 0, y = 0, w = 1, h = 1; std::string to; int tx = 0, ty = 0; std::string requireFlag, requireItem, lockedMsg; };
struct Trigger { int x = 0, y = 0, w = 1, h = 1; std::string script; std::string flag; bool once = true; std::string requireFlag; };
struct SignPost { int x = 0, y = 0; std::string text; Tile sprite; bool hasColor = false; Color8 color = C::White; std::string title, sound, onUse; };
struct Door { int x = 0, y = 0; bool open = false; std::string key; std::string flag; Tile closedTile, openTile; std::string openIf, require, lockedMsg; std::string sound, msg, onUse; };
// Something in the way that the right tool, spell or deed removes: rubble (shovel / blast), vines (fire), ice (fire), a boulder (STR)...
struct Obstacle { int x = 0, y = 0, w = 1, h = 1; std::string name, kind, clearKind, needs, flag, msg, onClear; bool cleared = false;
    bool visit = false; };   // 1.4.0 (#75): visit = cleared only for this visit (frozen water): it comes back when the party leaves the map
// 1.4.0 (#76): a crate the party pushes one tile by walking into it (heavy=N: needs STR N or a utility=force spell).
// Crates go back to their placed spots whenever the party arrives on the map (a stuck puzzle is never lost for good).
struct Crate { int x = 0, y = 0, homeX = 0, homeY = 0, heavy = 0; std::string id, name; Tile sprite; bool hasColor = false; Color8 color = C::White; };
// 1.4.0 (#76): a pressure plate: down while a crate or the party's leader stands on it. group= ties plates into one
// puzzle (script functions plates/platesdown); onpress= / onrelease= are action lists run when it goes down / comes up.
struct Plate { int x = 0, y = 0; std::string group, onPress, onRelease; Tile sprite; bool hasColor = false; Color8 color = C::White; bool down = false; };
// 1.5.0 (#99): a trap (spike pit, deadfall...). An enemy group that walks onto it while chasing, walking to a noise or
// walking home loses hurt% of its HP (100 = gone for good, its flag set); the party stepping on it takes damage= each.
// Sprung until the next visit, or for good with flag=. onspring= runs actions when it goes off.
struct Trap { int x = 0, y = 0; std::string name, flag, text, onSpring; int hurt = 100, damage = 10; Tile sprite, sprungSprite; bool hasColor = false; Color8 color = C::White; bool sprung = false; };
// A lever or switch: toggles a flag; doors (openif=), changes and obstacles react to flags.
struct Lever { int x = 0, y = 0; std::string flag, msg, name; bool toggle = true; Tile sprite, onSprite; std::string sound, onUse; };
// A part of the map that looks different while a condition holds (a bridge appears, a wall crumbles, water drains).
struct WorldChange { int x = 0, y = 0, w = 1, h = 1; std::string kind, cond; std::vector<uint16_t> orig; bool applied = false; };
// A resource you can harvest (ore vein, herb patch); it grows back after some days.
struct GatherNode { int x = 0, y = 0, lo = 1, hi = 1, respawn = 3; std::string item, tool, name, sound; bool hasColor = false; Color8 color = C::White;   // sound= (1.3.3) played on harvest; color= tints the node (default by item)
    std::string node; Tile sprite, spentSprite; bool hasSpent = false; Color8 spent = RGB(85, 85, 85); };   // 1.1.0: node= its resource type; sprite = its own picture (empty = the map tile)
// 1.1.0: resource node types (resources.txt). A [gather] object names one with node= (or matches one by item=) and
// takes every key it doesn't set itself from it; generated dungeons scatter the ones whose styles= name their style.
struct ResourceStyle { std::string style; int weight = 1, lo = -1, hi = -1; };   // styles=mine:2:2-4 -> style, weight, count
struct ResourceDef { std::string id, name, item, tool, kind, sound; int lo = 1, hi = 1, respawn = 3; Tile sprite, spentSprite;
    bool hasColor = false, hasSpent = false; Color8 color = C::White, spent = RGB(85, 85, 85); std::vector<ResourceStyle> styles; };
void setResourceDefs(const std::vector<ConfigSection>& secs);     // later sections replace earlier ones with the same id
const std::vector<ResourceDef>& resourceDefs();
const ResourceDef* resourceFor(const std::string& node, const std::string& item);   // by node= id, else the first one giving that item
void applyResource(GatherNode& g, const ResourceDef& r);         // the look (picture and colours) of a node of this type
Color8 gatherGuessColor(const std::string& item);                // the colour a node without color= gets (ore copper, herbs green...)
// 1.1.0: object templates (objects.txt): a placed object with template=<id> gets the template's type and every key it
// doesn't set itself, so editing the template changes every copy on every map.
struct ObjTemplate { std::string id, type; std::vector<std::pair<std::string, std::string>> kv; };
// 1.1.1: [type] entries in objects.txt (id = an object type: savepoint, chest, sign...) hold what every object of that
// type uses unless it (or its template) sets the key itself. An empty value there means the built-in default.
void setObjectTemplates(const std::vector<ConfigSection>& secs);   // reads both [template] and [type] sections
const ObjTemplate* objectTemplate(const std::string& id);
const ObjTemplate* objectType(const std::string& type);
void applyObjectTemplate(std::string& type, std::vector<std::pair<std::string, std::string>>& kv);   // template, then NPC, then type
// 1.1.9 (owner's #33): NPCs (npcs.txt [npc]) are characters of their own: look, name, what talking to them does, and for
// companions the class they join as and how they arrive. A map [npc] object is a placement with npc=<id> (plus its
// spot, its own id and show/hide/moveif); every key it doesn't set (or leaves empty) comes from the NPC.
// Order: placed object > its template > its NPC > the npc [type] entry > built-in.
struct NpcDef { std::string id; std::vector<std::pair<std::string, std::string>> kv;
    std::string get(const std::string& k, const std::string& d = "") const { for (auto& p : kv) if (p.first == k) return p.second; return d; } };
void setNpcDefs(const std::vector<ConfigSection>& secs);         // later sections replace earlier ones with the same id
const std::vector<NpcDef>& npcDefs();
const NpcDef* npcDef(const std::string& id);
const NpcDef* companionDef(const std::string& cls);              // the NPC with companion=<class id>, if any
// a join= action: "<npc id>" (an NPC with companion=, 1.1.9) or the older "class:Name:col,row"
struct JoinSpec { std::string cls, name; Tile tile; const NpcDef* npc = nullptr; };
JoinSpec parseJoin(const std::string& v);
struct SavePoint { int x = 0, y = 0; Tile sprite; bool hasColor = false; Color8 color = C::White; bool sparkle = true; Color8 sparkleColor = C::Cyan; std::string text, sound, onUse; };
// objects made by code (generated dungeons, the overworld, the console) take their type's look the same way
void applyTypeLook(SavePoint& s);
void applyTypeLook(Chest& c);
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
    std::vector<Crate> crates;     // 1.4.0 (#76)
    std::vector<Plate> plates;
    Crate* crateAt(int x, int y) { for (auto& c : crates) if (c.x == x && c.y == y) return &c; return nullptr; }
    Plate* plateAt(int x, int y) { for (auto& p : plates) if (p.x == x && p.y == y) return &p; return nullptr; }
    std::vector<Trap> traps;       // 1.5.0 (#99)
    Trap* trapAt(int x, int y, bool armedOnly = true) { for (auto& t : traps) if (t.x == x && t.y == y && (!armedOnly || !t.sprung)) return &t; return nullptr; }
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
    std::string combatMusic;     // 1.3.1 (#47) header combatmusic=: ordinary fights on this map (empty = the campaign's)
    std::string ambient;         // header ambient=: the ambient loop (sounds.txt [ambient]); "none" = silence; empty = chosen by place
    std::string worldMap;        // header worldmap=: what M shows while on this map (overrides the campaign): off | self | overworld | <map id>
    std::string onEnter;         // 1.1.10 header onenter=: actions (script:<id>...) run each time the party arrives (not on loading a save)
    bool touchCombat = false;    // 1.2.3 (#40) header touch=1: map enemy groups only start a fight by touching the party (next to it), not by seeing it;
                                 // 1.2.4 (#41) they chase it step for step inside their leash, random ambushes appear a few tiles away (coach ambushes still fight at once)
    Color8 bg = C::Black;
    bool generated = false;      // procedurally generated (not saved in full)

    Cell& at(int x, int y) { return cells[(size_t)y * w + x]; }
    const Cell& at(int x, int y) const { return cells[(size_t)y * w + x]; }
    bool inBounds(int x, int y) const { return x >= 0 && y >= 0 && x < w && y < h; }
    void look(int x, int y, Tile& ground, Color8& bg, int* mask = nullptr, int* corners = nullptr) const;   // the ground piece and background a cell draws with (joined pieces)
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
