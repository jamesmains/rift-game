#include "world.h"
#include "tiles.h"
#include <queue>
#include <cmath>
#include <cstring>

using namespace TL;

std::string gAssetDir = "assets";
static std::vector<TileKind> gKinds;
static std::map<std::string, int> gKindIndex;

static Color8 gBgOverride = C::Trans;
static void builtinLights();
static int defaultLight(const std::string& n);
static void K(const std::string& name, std::vector<Tile> ground, Tile prop = Tile{}, bool walk = true, bool sight = false,
              bool water = false, Color8 tint = C::White, int encounter = 0, const std::string& biome = "", int cost = 1) {
    TileKind k; k.name = name; k.ground = ground; k.prop = prop; k.walkable = walk; k.blocksSight = sight; k.water = water;
    k.tint = tint; k.encounter = encounter; k.biome = biome; k.moveCost = cost;
    // walls (non-walkable, sight-blocking, no prop) are drawn opaque; everything else gets a dark background color
    k.opaque = (!walk && sight && !prop.valid());
    k.bg = gBgOverride.a ? gBgOverride : RGB((uint8_t)(tint.r * 0.30f), (uint8_t)(tint.g * 0.30f), (uint8_t)(tint.b * 0.30f));
    gBgOverride = C::Trans;
    gKindIndex[name] = (int)gKinds.size(); gKinds.push_back(k);
}

static void buildKinds() {
    if (!gKinds.empty()) return;
    const Color8 grassTint = RGB(110, 170, 90), forestTint = RGB(60, 120, 60), sandTint = RGB(210, 190, 120);
    const Color8 stoneTint = RGB(130, 130, 140), dirtTint = RGB(150, 110, 70), waterTint = RGB(40, 90, 200), metalTint = RGB(130, 150, 170);
    const Color8 caveTint = RGB(110, 95, 85), woodTint = RGB(170, 120, 70), snowTint = RGB(220, 230, 240), ashTint = RGB(90, 80, 80);
    // ---- overworld ----
    gBgOverride = RGB(58, 112, 52);
    K("grass", {GRASS, GRASS, GRASS, GRASS2, GRASS_TUFT}, Tile{}, true, false, false, grassTint, 2, "plains");
    gBgOverride = RGB(62, 118, 54);
    K("meadow", {GRASS, GRASS2}, FLOWER, true, false, false, grassTint, 1, "plains");
    K("forest", {GRASS, GRASS2}, TREE, true, true, false, forestTint, 4, "forest", 2);
    K("forest2", {GRASS, GRASS2}, TREE2, true, true, false, forestTint, 4, "forest", 2);
    K("pine", {GRASS, GRASS2}, PINE, true, true, false, forestTint, 4, "forest", 2);
    K("deepforest", {GRASS2}, PINE2, false, true, false, forestTint, 0, "forest");
    K("bush", {GRASS, GRASS2}, BUSH, true, false, false, grassTint, 3, "plains");
    K("tallgrass", {GRASS, GRASS2}, TALLGRASS, true, false, false, grassTint, 3, "plains");
    gBgOverride = RGB(150, 130, 80);
    K("sand", {SAND, SAND, DESERT}, Tile{}, true, false, false, sandTint, 2, "desert");
    gBgOverride = RGB(150, 130, 80);
    K("dune", {SAND}, DUNE, true, false, false, sandTint, 2, "desert", 2);
    gBgOverride = RGB(150, 130, 80);
    K("cactus", {SAND}, CACTUS, false, false, false, sandTint, 0, "desert");
    gBgOverride = RGB(28, 60, 150);
    K("water", {WATER, WATER, WATER2}, Tile{}, false, false, true, waterTint, 0, "water");
    K("deepwater", {LAKE}, Tile{}, false, false, true, RGB(20, 50, 160), 0, "water");
    gBgOverride = RGB(50, 100, 190);
    K("shallows", {WATER2}, Tile{}, true, false, true, RGB(90, 150, 230), 1, "water", 2);
    K("hill", {GRASS2}, HILL, true, false, false, grassTint, 3, "hills", 2);
    K("hill2", {GRASS2}, HILL2, true, false, false, grassTint, 3, "hills", 2);
    K("mountain", {SCORCHED}, MOUNTAIN, false, true, false, stoneTint, 0, "mountain");
    K("snowpeak", {SCORCHED}, SNOWPEAK, false, true, false, snowTint, 0, "mountain");
    K("volcano", {SCORCHED}, VOLCANO, false, true, false, RGB(200, 80, 60), 0, "mountain");
    gBgOverride = RGB(40, 62, 46);
    K("swamp", {SWAMP, SWAMP, WATER2}, Tile{}, true, false, false, RGB(70, 110, 80), 5, "swamp", 2);
    K("deadtree", {SCORCHED}, DEADTREE, true, true, false, ashTint, 5, "wasteland", 2);
    K("deadtree2", {SCORCHED}, DEADTREE2, true, true, false, ashTint, 5, "wasteland", 2);
    K("wasteland", {SCORCHED, SCORCHED, CRATER, RUBBLE}, Tile{}, true, false, false, ashTint, 4, "wasteland");
    K("rubble", {RUBBLE}, Tile{}, true, false, false, stoneTint, 3, "wasteland");
    gBgOverride = RGB(105, 92, 70);
    K("road", {ROAD}, Tile{}, true, false, false, RGB(190, 170, 130), 1, "plains");
    K("dirt", {DIRT, DIRT2}, Tile{}, true, false, false, dirtTint, 1, "plains");
    gBgOverride = RGB(28, 60, 150);
    K("river", {RIVER}, Tile{}, false, false, true, waterTint, 0, "water");
    gBgOverride = RGB(105, 72, 40);
    K("bridge", {FLOOR_WOOD}, Tile{}, true, false, false, woodTint, 0, "plains");
    gBgOverride = RGB(150, 165, 180);
    K("snow", {SAND}, Tile{}, true, false, false, snowTint, 2, "snow");
    gBgOverride = RGB(150, 165, 180);
    K("snowpine", {SAND}, PINE, true, true, false, RGB(180, 210, 200), 3, "snow", 2);
    gBgOverride = RGB(70, 30, 90);
    K("riftscar", {CRATER}, Tile{}, true, false, false, RGB(170, 80, 220), 6, "rift");
    gBgOverride = RGB(70, 30, 90);
    K("riftcrystal", {CRATER}, IT_CRYSTAL, false, true, false, RGB(170, 80, 220), 0, "rift");
    // overworld locations (props drawn full color; walkable so you can step on them)
    K("loc_village", {GRASS}, LOC_VILLAGE, true, false, false, grassTint, 0, "loc");
    K("loc_town", {GRASS}, LOC_TOWN, true, false, false, grassTint, 0, "loc");
    K("loc_city", {GRASS}, LOC_CITY, true, false, false, grassTint, 0, "loc");
    K("loc_house", {GRASS}, LOC_HOUSE, true, false, false, grassTint, 0, "loc");
    K("loc_farm", {GRASS}, LOC_FARM, true, false, false, grassTint, 0, "loc");
    K("loc_tent", {GRASS}, LOC_TENT, true, false, false, grassTint, 0, "loc");
    K("loc_cave", {SCORCHED}, LOC_CAVE, true, false, false, stoneTint, 0, "loc");
    K("loc_tower", {GRASS}, LOC_TOWER, true, false, false, grassTint, 0, "loc");
    K("loc_castle", {GRASS}, LOC_CASTLE, true, false, false, grassTint, 0, "loc");
    K("loc_ruin", {SCORCHED}, LOC_RUIN, true, false, false, stoneTint, 0, "loc");
    K("loc_temple", {GRASS}, LOC_TEMPLE, true, false, false, grassTint, 0, "loc");
    K("loc_crypt", {SCORCHED}, LOC_CRYPT, true, false, false, stoneTint, 0, "loc");
    K("loc_ship", {SCORCHED}, LOC_SHIP, true, false, false, ashTint, 0, "loc");
    K("loc_citymod", {RUBBLE}, LOC_CITYMOD, true, false, false, stoneTint, 0, "loc");
    K("loc_lab", {RUBBLE}, LOC_LAB, true, false, false, stoneTint, 0, "loc");
    K("loc_shrine", {GRASS}, LOC_SHRINE, true, false, false, grassTint, 0, "loc");
    K("loc_mine", {SCORCHED}, LOC_CAVE, true, false, false, dirtTint, 0, "loc");
    // ---- interiors / dungeons ----
    K("wall_stone", {WALL_STONE, WALL_STONE, WALL_STONE2}, Tile{}, false, true, false, stoneTint);
    K("wall_gray", {WALL_GRAY, WALL_GRAY, WALL_GRAY2}, Tile{}, false, true, false, stoneTint);
    K("wall_brick", {WALL_BRICK, WALL_BRICK, WALL_BRICK2}, Tile{}, false, true, false, RGB(200, 90, 70));
    K("wall_wood", {WALL_WOOD, WALL_WOOD, WALL_WOOD2}, Tile{}, false, true, false, woodTint);
    K("wall_cave", {WALL_CAVE, WALL_CAVE2}, Tile{}, false, true, false, caveTint);
    K("wall_metal", {WALL_METAL, WALL_METAL, WALL_METAL2}, Tile{}, false, true, false, metalTint);
    K("wall_tech", {WALL_TECH}, Tile{}, false, true, false, metalTint);
    K("wall_concrete", {WALL_CONCRETE, WALL_CONCRETE2}, Tile{}, false, true, false, stoneTint);
    K("wall_hull", {SHIP_HULL, SHIP_HULL2}, Tile{}, false, true, false, metalTint);
    K("window", {WALL_WOOD}, WINDOW, false, false, false, woodTint);
    gBgOverride = RGB(58, 58, 66);
    K("floor_stone", {FLOOR_STONE, FLOOR_STONE, FLOOR_STONE2}, Tile{}, true, false, false, stoneTint);
    gBgOverride = RGB(70, 48, 26);
    K("floor_wood", {FLOOR_WOOD, FLOOR_WOOD2}, Tile{}, true, false, false, woodTint);
    K("floor_tile", {FLOOR_TILE, FLOOR_TILE2}, Tile{}, true, false, false, RGB(160, 150, 170));
    gBgOverride = RGB(62, 52, 46);
    K("floor_cave", {FLOOR_CAVE, FLOOR_CAVE2, FLOOR_CAVE}, Tile{}, true, false, false, caveTint);
    gBgOverride = RGB(50, 58, 68);
    K("floor_metal", {FLOOR_METAL, FLOOR_METAL2}, Tile{}, true, false, false, metalTint);
    K("floor_grid", {FLOOR_GRID}, Tile{}, true, false, false, metalTint);
    K("floor_dirt", {DIRT, DIRT2}, Tile{}, true, false, false, dirtTint);
    K("floor_grass", {GRASS, GRASS2, GRASS_TUFT}, Tile{}, true, false, false, grassTint);
    K("floor_sand", {SAND}, Tile{}, true, false, false, sandTint);
    gBgOverride = RGB(45, 45, 52);
    K("floor_road", {FLOOR_ASPHALT}, Tile{}, true, false, false, RGB(90, 90, 100));
    gBgOverride = RGB(70, 70, 70);
    K("floor_concrete", {FLOOR_CONCRETE}, Tile{}, true, false, false, RGB(150, 150, 150));
    K("floor_carpet", {FLOOR_TILE}, Tile{}, true, false, false, RGB(180, 60, 60));
    gBgOverride = RGB(120, 120, 130);
    K("floor_marble", {FLOOR_TILE2}, Tile{}, true, false, false, RGB(220, 220, 230));
    gBgOverride = RGB(28, 60, 150);
    K("floor_water", {WATER, WATER2}, Tile{}, false, false, true, waterTint);
    K("mire", {WATER2, WATER2, WATER}, Tile{}, true, false, true, RGB(60, 110, 90), 0, "", 2);   // knee-deep dungeon water: slow but passable
    gBgOverride = RGB(120, 30, 10);
    K("floor_lava", {WATER, WATER2}, Tile{}, false, false, true, RGB(240, 100, 30));
    K("door_wood", {FLOOR_WOOD}, DOOR_WOOD, true, true, false, woodTint);
    K("door_metal", {FLOOR_METAL}, DOOR_METAL, true, true, false, metalTint);
    K("door_arch", {FLOOR_STONE}, DOOR_ARCH, true, false, false, stoneTint);
    K("stairs_down", {FLOOR_STONE}, STAIRS_DOWN, true, false, false, stoneTint);
    K("stairs_up", {FLOOR_STONE}, STAIRS_UP, true, false, false, stoneTint);
    K("pillar", {FLOOR_STONE}, PILLAR, false, false, false, stoneTint);
    K("torch", {FLOOR_STONE}, TORCH, false, false, false, stoneTint);
    K("torch_wall", {WALL_STONE}, TORCH, false, true, false, stoneTint);
    K("campfire", {DIRT}, CAMPFIRE, false, false, false, dirtTint);
    K("fire", {FLOOR_STONE}, FIRE, false, false, false, stoneTint);
    K("brazier", {FLOOR_STONE}, BRAZIER, false, false, false, stoneTint);
    K("bones", {FLOOR_CAVE}, BONES, true, false, false, caveTint);
    K("skull", {FLOOR_CAVE}, SKULL, true, false, false, caveTint);
    K("grave", {GRASS2}, GRAVE, false, false, false, forestTint);
    K("grave2", {GRASS2}, GRAVE2, false, false, false, forestTint);
    K("tomb", {FLOOR_STONE}, TOMB, false, false, false, stoneTint);
    K("coffin", {FLOOR_STONE}, COFFIN, false, false, false, stoneTint);
    K("bed", {FLOOR_WOOD}, BED, false, false, false, woodTint);
    K("table", {FLOOR_WOOD}, TABLE, false, false, false, woodTint);
    K("table2", {FLOOR_WOOD}, TABLE2, false, false, false, woodTint);
    K("chair", {FLOOR_WOOD}, CHAIR, true, false, false, woodTint);
    K("shelf", {FLOOR_WOOD}, SHELF, false, true, false, woodTint);
    K("bookshelf", {FLOOR_WOOD}, BOOKSHELF, false, true, false, woodTint);
    K("barrel", {FLOOR_WOOD}, BARREL, false, false, false, woodTint);
    K("crate", {FLOOR_WOOD}, CRATE, false, false, false, woodTint);
    K("anvil", {FLOOR_STONE}, ANVIL, false, false, false, stoneTint);
    K("forge", {FLOOR_STONE}, FORGE, false, false, false, stoneTint);
    K("cauldron", {FLOOR_WOOD}, CAULDRON, false, false, false, woodTint);
    K("altar", {FLOOR_TILE}, ALTAR, false, false, false, RGB(160, 150, 170));
    K("statue", {FLOOR_TILE}, STATUE, false, true, false, RGB(160, 150, 170));
    K("fence", {GRASS}, FENCE, false, false, false, grassTint);
    K("fence_wood", {GRASS}, FENCE_WOOD, false, false, false, grassTint);
    K("well", {ROAD}, WELL, false, false, false, RGB(190, 170, 130));
    K("fountain", {ROAD}, FOUNTAIN, false, false, false, RGB(190, 170, 130));
    K("sign", {ROAD}, SIGN, false, false, false, RGB(190, 170, 130));
    K("shop_sign", {WALL_WOOD}, SHOP_SIGN, false, true, false, woodTint);
    K("inn_sign", {WALL_WOOD}, INN_SIGN, false, true, false, woodTint);
    K("crop", {DIRT}, CROP, true, false, false, dirtTint);
    K("crop2", {DIRT}, CROP2, true, false, false, dirtTint);
    K("hay", {DIRT}, HAYSTACK, false, false, false, dirtTint);
    K("flower", {GRASS}, FLOWER, true, false, false, grassTint);
    K("flower2", {GRASS}, FLOWER2, true, false, false, grassTint);
    K("mushroom", {FLOOR_CAVE}, MUSHROOM, true, false, false, caveTint);
    K("mushroom2", {FLOOR_CAVE}, MUSHROOM2, true, false, false, caveTint);
    K("web", {FLOOR_CAVE}, WEB, true, false, false, caveTint);
    K("rocks", {FLOOR_CAVE}, ROCKS, false, false, false, caveTint);
    K("tree", {GRASS}, TREE, false, true, false, grassTint);
    K("tree2", {GRASS}, TREE2, false, true, false, grassTint);
    K("pine_t", {GRASS}, PINE, false, true, false, grassTint);
    K("deadtree_t", {SCORCHED}, DEADTREE, false, true, false, ashTint);
    K("banner_red", {WALL_STONE}, BANNER_RED, false, true, false, stoneTint);
    K("banner_blue", {WALL_STONE}, BANNER_BLUE, false, true, false, stoneTint);
    K("banner_gold", {WALL_STONE}, BANNER_GOLD, false, true, false, stoneTint);
    K("portal_tl", {FLOOR_STONE}, PORTAL_TL, false, false, false, stoneTint);
    K("portal_tr", {FLOOR_STONE}, PORTAL_TR, false, false, false, stoneTint);
    K("portal_bl", {FLOOR_STONE}, PORTAL_BL, true, false, false, stoneTint);
    K("portal_br", {FLOOR_STONE}, PORTAL_BR, true, false, false, stoneTint);
    K("sparkle", {FLOOR_STONE}, SPARKLE, true, false, false, stoneTint);
    K("lever", {FLOOR_STONE}, LEVER, false, false, false, stoneTint);
    K("crystal", {FLOOR_CAVE}, IT_CRYSTAL, false, false, false, RGB(170, 120, 230));
    K("gem_blue", {FLOOR_CAVE}, IT_GEM_BLUE, true, false, false, caveTint);
    // modern / sci-fi props
    K("computer", {FLOOR_METAL}, COMPUTER, false, false, false, metalTint);
    K("terminal", {FLOOR_METAL}, TERMINAL, false, false, false, metalTint);
    K("terminal2", {FLOOR_METAL}, TERMINAL2, false, false, false, metalTint);
    K("server", {FLOOR_METAL}, SERVER, false, true, false, metalTint);
    K("car", {FLOOR_ASPHALT}, CAR, false, false, false, RGB(90, 90, 100));
    K("car2", {FLOOR_ASPHALT}, CAR2, false, false, false, RGB(90, 90, 100));
    K("car_red", {FLOOR_ASPHALT}, CAR_RED, false, false, false, RGB(90, 90, 100));
    K("truck", {FLOOR_ASPHALT}, TRUCK, false, false, false, RGB(90, 90, 100));
    K("tank", {FLOOR_ASPHALT}, TANK, false, true, false, RGB(90, 90, 100));
    K("building", {FLOOR_CONCRETE}, BUILDING, false, true, false, RGB(150, 150, 150));
    K("building2", {FLOOR_CONCRETE}, BUILDING2, false, true, false, RGB(150, 150, 150));
    K("building3", {FLOOR_CONCRETE}, BUILDING3, false, true, false, RGB(150, 150, 150));
    K("building4", {FLOOR_CONCRETE}, BUILDING4, false, true, false, RGB(150, 150, 150));
    K("building_red", {FLOOR_CONCRETE}, BUILDING_RED, false, true, false, RGB(150, 150, 150));
    K("streetlamp", {FLOOR_CONCRETE}, STREETLAMP, false, false, false, RGB(150, 150, 150));
    K("trafficlight", {FLOOR_CONCRETE}, TRAFFICLIGHT, false, false, false, RGB(150, 150, 150));
    K("sign_stop", {FLOOR_CONCRETE}, SIGN_STOP, false, false, false, RGB(150, 150, 150));
    K("sign_warn", {FLOOR_CONCRETE}, SIGN_WARN, false, false, false, RGB(150, 150, 150));
    K("pipe_h", {FLOOR_METAL}, PIPE_H, false, false, false, metalTint);
    K("pipe_v", {FLOOR_METAL}, PIPE_V, false, false, false, metalTint);
    K("vending", {FLOOR_CONCRETE}, VENDING, false, false, false, RGB(150, 150, 150));
    K("fridge", {FLOOR_TILE}, FRIDGE, false, false, false, RGB(160, 150, 170));
    K("tv", {FLOOR_WOOD}, TV, false, false, false, woodTint);
    K("cryopod", {FLOOR_METAL}, CRYOPOD, false, false, false, metalTint);
    K("reactor", {FLOOR_METAL}, REACTOR, false, true, false, metalTint);
    K("ship_window", {SHIP_HULL}, SHIP_WINDOW, false, false, false, metalTint);
    K("trash", {FLOOR_CONCRETE}, TRASH, false, false, false, RGB(150, 150, 150));
    K("box", {FLOOR_CONCRETE}, BOX, false, false, false, RGB(150, 150, 150));
    K("road_line", {FLOOR_ASPHALT}, ROAD_LINE, true, false, false, RGB(90, 90, 100));
    K("satellite", {RUBBLE}, SATELLITE, false, true, false, stoneTint);
    K("rocket_wreck", {RUBBLE}, ROCKET, false, true, false, stoneTint);
    K("ufo", {RUBBLE}, UFO, false, true, false, stoneTint);
    gBgOverride = C::Black;
    K("void", {Tile{}}, Tile{}, false, true, false, C::Black);
    // obstacles that world triggers can clear (see Map::obstacles): rubble needs a blast or a shovel, vines burn, ice melts
    K("rubble_pile", {FLOOR_CAVE}, ROCKS, false, true, false, RGB(150, 140, 120));
    K("vines", {FLOOR_CAVE}, BUSH, false, true, false, RGB(80, 150, 70));
    K("ice_wall", {FLOOR_STONE}, IT_CRYSTAL, false, true, false, RGB(170, 220, 255));
    K("lever_off", {FLOOR_STONE}, LEVER, false, false, false, stoneTint);
    builtinLights();
}
// default light radii for the built-in kinds (tilekinds.txt can override with light=N)
static int defaultLight(const std::string& n) {
    static const std::map<std::string, int> L = {{"torch", 4}, {"torch_wall", 4}, {"brazier", 4}, {"fire", 4}, {"campfire", 5}, {"crystal", 2}, {"riftcrystal", 2},
        {"floor_lava", 3}, {"reactor", 3}, {"terminal", 2}, {"terminal2", 2}, {"computer", 2}, {"streetlamp", 5}, {"sparkle", 2}, {"portal_tl", 3}, {"portal_tr", 3},
        {"portal_bl", 3}, {"portal_br", 3}, {"altar", 2}, {"gem_blue", 1}, {"ice_wall", 1}, {"cryopod", 1}, {"vending", 2}, {"tv", 1}};
    auto it = L.find(n); return it == L.end() ? 0 : it->second;
}
static void builtinLights() { for (auto& k : gKinds) k.light = defaultLight(k.name); }

const TileKind* tileKind(const std::string& name) { buildKinds(); auto it = gKindIndex.find(name); return it == gKindIndex.end() ? nullptr : &gKinds[it->second]; }
const std::vector<TileKind>& allTileKinds() { buildKinds(); return gKinds; }
std::vector<TileKind>& editableTileKinds() { buildKinds(); return gKinds; }
int tileKindIndex(const std::string& name) { buildKinds(); auto it = gKindIndex.find(name); return it == gKindIndex.end() ? -1 : it->second; }
void rebuildTileKindIndex() { gKindIndex.clear(); for (size_t i = 0; i < gKinds.size(); i++) gKindIndex[gKinds[i].name] = (int)i; }

static std::string tileStr(Tile t) { return t.valid() ? fmt("%d,%d", t.c, t.r) : ""; }
static std::string colStr(Color8 c) { return fmt("%d,%d,%d", c.r, c.g, c.b); }
std::string tileKindsText(const std::vector<TileKind>& kinds) {
    std::string o = "# Tile kinds: what each map cell type looks like and how it behaves.\n# ground = one or more tiles (col,row) separated by | - one is picked at random per cell.\n# opaque=1 draws the ground tile with its black background (walls); otherwise the bg color shows through.\n\n";
    for (auto& k : kinds) {
        o += "[kind]\nname=" + k.name + "\nground=";
        for (size_t i = 0; i < k.ground.size(); i++) o += (i ? "|" : "") + tileStr(k.ground[i]);
        o += "\nprop=" + tileStr(k.prop) + fmt("\nwalk=%d\nsight=%d\nwater=%d\nopaque=%d\n", k.walkable, k.blocksSight, k.water, k.opaque);
        o += "tint=" + colStr(k.tint) + "\nbg=" + colStr(k.bg) + fmt("\nencounter=%d\nbiome=%s\ncost=%d\n", k.encounter, k.biome.c_str(), k.moveCost);
        o += k.light ? fmt("light=%d\n\n", k.light) : "\n";
    }
    return o;
}
bool saveTileKinds(const std::string& path) { buildKinds(); return writeFile(path, tileKindsText(gKinds)); }
bool loadTileKinds(const std::string& path) {
    std::string text; if (!readFile(path, text)) return false;
    std::vector<TileKind> kinds;
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
        kinds.push_back(k);
    }
    if (kinds.empty()) return false;
    buildKinds();
    // built-in kinds that an older tilekinds.txt doesn't know about yet are appended, so new content keeps working
    for (auto& b : gKinds) { bool have = false; for (auto& k : kinds) if (k.name == b.name) { have = true; break; } if (!have) kinds.push_back(b); }
    gKinds = kinds; rebuildTileKindIndex();
    if (gKindIndex.find("void") == gKindIndex.end()) { TileKind v; v.name = "void"; v.ground.push_back(Tile{}); v.walkable = false; v.blocksSight = true; v.opaque = true; gKinds.push_back(v); rebuildTileKindIndex(); }
    return true;
}
bool loadTileKindsOverlay(const std::string& path) {
    std::string text; if (!readFile(path, text)) return false;
    buildKinds();
    for (auto& sec : parseConfig(text)) {
        if (sec.name != "kind") continue;
        TileKind k; k.name = sec.get("name"); if (k.name.empty()) continue;
        for (auto& g : split(sec.get("ground"), '|')) { std::string t = trim(g); k.ground.push_back(t.empty() ? Tile{} : parseTile(t)); }
        if (k.ground.empty()) k.ground.push_back(Tile{});
        std::string pr = trim(sec.get("prop")); k.prop = pr.empty() ? Tile{} : parseTile(pr);
        k.walkable = sec.geti("walk", 1) != 0; k.blocksSight = sec.geti("sight", 0) != 0; k.water = sec.geti("water", 0) != 0; k.opaque = sec.geti("opaque", 0) != 0;
        k.tint = sec.has("tint") ? parseColor(sec.get("tint")) : C::White; k.bg = sec.has("bg") ? parseColor(sec.get("bg")) : C::Black;
        k.encounter = sec.geti("encounter", 0); k.biome = sec.get("biome"); k.moveCost = sec.geti("cost", 1); k.light = sec.geti("light", defaultLight(k.name));
        auto it = gKindIndex.find(k.name);
        if (it != gKindIndex.end()) gKinds[it->second] = k; else { gKindIndex[k.name] = (int)gKinds.size(); gKinds.push_back(k); }
    }
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
    c.prop = k->prop; c.walkable = k->walkable; c.blocksSight = k->blocksSight; c.water = k->water; c.tint = k->tint; c.bg = k->bg; c.opaque = k->opaque;
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
            else if (k == "ambient") m.ambient = v;
            else if (k == "worldmap") m.worldMap = v;
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
        const std::string& type = ob.first;
        if (type == "npc") {
            NPC n; n.id = gets(kv, "id"); n.name = gets(kv, "name", n.id); n.dialogue = gets(kv, "dialogue"); n.shop = gets(kv, "shop");
            n.x = n.homeX = geti(kv, "x", 0); n.y = n.homeY = geti(kv, "y", 0);
            n.tile = parseTile(gets(kv, "sprite", "104,0")); n.wander = geti(kv, "wander", 0) != 0;
            if (!gets(kv, "color").empty()) { n.recolor = true; n.color = parseColor(gets(kv, "color")); }
            n.showFlag = gets(kv, "show"); n.hideFlag = gets(kv, "hide"); n.hostileCreature = gets(kv, "hostile"); n.craft = gets(kv, "craft"); n.board = gets(kv, "board"); n.travel = geti(kv, "travel", 0) != 0;
            n.moveIf = gets(kv, "moveif"); n.moveX = geti(kv, "movex", -1); n.moveY = geti(kv, "movey", -1);
            m.npcs.push_back(n);
        } else if (type == "enemy") {
            EnemyGroup e; e.flag = gets(kv, "flag"); e.x = e.homeX = geti(kv, "x", 0); e.y = e.homeY = geti(kv, "y", 0); e.aggro = geti(kv, "aggro", 5);
            for (auto& c : split(gets(kv, "group"), ',')) if (!trim(c).empty()) e.creatures.push_back(trim(c));
            e.levelBonus = geti(kv, "level", 0); e.onDefeatScript = gets(kv, "ondefeat"); e.showFlag = gets(kv, "show"); e.hideFlag = gets(kv, "hide"); e.link = gets(kv, "link");
            m.enemies.push_back(e);
        } else if (type == "chest") {
            Chest c; c.x = geti(kv, "x", 0); c.y = geti(kv, "y", 0); c.flag = gets(kv, "flag"); c.gold = geti(kv, "gold", 0); c.locked = gets(kv, "locked");
            for (auto& s : split(gets(kv, "items"), ',')) { auto p = split(trim(s), ':'); if (p.empty() || trim(p[0]).empty()) continue; c.items.push_back({trim(p[0]), p.size() > 1 ? atoi(p[1].c_str()) : 1}); }
            m.chests.push_back(c);
        } else if (type == "exit") {
            Exit e; e.x = geti(kv, "x", 0); e.y = geti(kv, "y", 0); e.w = geti(kv, "w", 1); e.h = geti(kv, "h", 1); e.to = gets(kv, "to"); e.tx = geti(kv, "tx", 0); e.ty = geti(kv, "ty", 0);
            e.requireFlag = gets(kv, "require"); e.requireItem = gets(kv, "item"); e.lockedMsg = gets(kv, "locked");
            m.exits.push_back(e);
        } else if (type == "trigger") {
            Trigger t; t.x = geti(kv, "x", 0); t.y = geti(kv, "y", 0); t.w = geti(kv, "w", 1); t.h = geti(kv, "h", 1); t.script = gets(kv, "script"); t.flag = gets(kv, "flag"); t.once = geti(kv, "once", 1) != 0; t.requireFlag = gets(kv, "require");
            m.triggers.push_back(t);
        } else if (type == "sign") {
            SignPost s; s.x = geti(kv, "x", 0); s.y = geti(kv, "y", 0); s.text = gets(kv, "text"); m.signs.push_back(s);
            if (m.inBounds(s.x, s.y)) m.at(s.x, s.y).walkable = false;
        } else if (type == "door") {
            Door d; d.x = geti(kv, "x", 0); d.y = geti(kv, "y", 0); d.key = gets(kv, "key"); d.flag = gets(kv, "flag"); d.openIf = gets(kv, "openif"); d.require = gets(kv, "require"); d.lockedMsg = gets(kv, "locked");
            d.closedTile = m.inBounds(d.x, d.y) ? m.at(d.x, d.y).prop : DOOR_WOOD; d.openTile = gets(kv, "metal") == "1" ? DOOR_METAL : DOOR_OPEN;
            if (m.inBounds(d.x, d.y)) { m.at(d.x, d.y).walkable = false; m.at(d.x, d.y).blocksSight = true; }
            m.doors.push_back(d);
        } else if (type == "savepoint") {
            SavePoint s; s.x = geti(kv, "x", 0); s.y = geti(kv, "y", 0); m.savepoints.push_back(s);
        } else if (type == "obstacle") {
            Obstacle o; o.x = geti(kv, "x", 0); o.y = geti(kv, "y", 0); o.w = geti(kv, "w", 1); o.h = geti(kv, "h", 1); o.name = gets(kv, "name", "Rubble");
            o.kind = gets(kv, "kind"); o.clearKind = gets(kv, "clear"); o.needs = gets(kv, "needs", "rubble"); o.flag = gets(kv, "flag"); o.msg = gets(kv, "text"); o.onClear = gets(kv, "onclear");
            if (o.kind.empty()) o.kind = o.needs.find("burn") != std::string::npos ? "vines" : o.needs.find("melt") != std::string::npos ? "ice_wall" : "rubble_pile";
            if (o.clearKind.empty() && m.inBounds(o.x, o.y)) { const auto& ks = allTileKinds(); o.clearKind = ks[m.at(o.x, o.y).kind].name; if (!ks[m.at(o.x, o.y).kind].walkable) o.clearKind = "floor_cave"; }
            for (int yy = o.y; yy < o.y + o.h; yy++) for (int xx = o.x; xx < o.x + o.w; xx++) m.set(xx, yy, o.kind, rng);
            m.obstacles.push_back(o);
        } else if (type == "lever") {
            Lever l; l.x = geti(kv, "x", 0); l.y = geti(kv, "y", 0); l.flag = gets(kv, "flag"); l.msg = gets(kv, "text"); l.name = gets(kv, "name", "Lever"); l.toggle = geti(kv, "toggle", 1) != 0;
            if (m.inBounds(l.x, l.y)) { m.set(l.x, l.y, "lever_off", rng); }
            m.levers.push_back(l);
        } else if (type == "change") {
            WorldChange c; c.x = geti(kv, "x", 0); c.y = geti(kv, "y", 0); c.w = geti(kv, "w", 1); c.h = geti(kv, "h", 1); c.kind = gets(kv, "kind"); c.cond = gets(kv, "if");
            for (int yy = c.y; yy < c.y + c.h; yy++) for (int xx = c.x; xx < c.x + c.w; xx++) c.orig.push_back(m.inBounds(xx, yy) ? m.at(xx, yy).kind : 0);
            m.changes.push_back(c);
        } else if (type == "gather") {
            GatherNode g; g.x = geti(kv, "x", 0); g.y = geti(kv, "y", 0); g.item = gets(kv, "item"); g.tool = gets(kv, "tool"); g.name = gets(kv, "name");
            auto cnt = split(gets(kv, "count", "1"), '-'); g.lo = atoi(cnt[0].c_str()); g.hi = cnt.size() > 1 ? atoi(cnt[1].c_str()) : g.lo; g.respawn = geti(kv, "respawn", 3);
            if (!gets(kv, "color").empty()) { g.hasColor = true; g.color = parseColor(gets(kv, "color")); }
            m.gathers.push_back(g);
        }
    }
    return true;
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
