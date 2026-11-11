// Procedural generation: the overworld and dungeons.
#include "world.h"
#include "tiles.h"
#include <cmath>
#include <algorithm>

// ---- value noise ----
static float hashf(int x, int y, int seed) {
    uint32_t h = (uint32_t)x * 374761393u + (uint32_t)y * 668265263u + (uint32_t)seed * 1442695041u;
    h = (h ^ (h >> 13)) * 1274126177u; h ^= h >> 16;
    return (h & 0xFFFF) / 65535.0f;
}
static float smoothNoise(float x, float y, int seed) {
    int xi = (int)floorf(x), yi = (int)floorf(y);
    float fx = x - xi, fy = y - yi;
    fx = fx * fx * (3 - 2 * fx); fy = fy * fy * (3 - 2 * fy);
    float a = hashf(xi, yi, seed), b = hashf(xi + 1, yi, seed), c = hashf(xi, yi + 1, seed), d = hashf(xi + 1, yi + 1, seed);
    return (a + (b - a) * fx) + ((c + (d - c) * fx) - (a + (b - a) * fx)) * fy;
}
static float fbm(float x, float y, int seed, int oct = 4) {
    float v = 0, amp = 0.5f, tot = 0;
    for (int i = 0; i < oct; i++) { v += smoothNoise(x, y, seed + i * 17) * amp; tot += amp; x *= 2; y *= 2; amp *= 0.5f; }
    return v / tot;
}

struct Location { std::string id, name, kind, mapId, road; int x = 0, y = 0, tx = 0, ty = 0, clear = 2; std::string requireFlag, lockedMsg; };

static std::vector<Location> parseLocations(const std::string& text);
static std::vector<Location> readLocations(const std::string& file) {
    std::string text;
    if (!readFile(file.empty() ? gAssetDir + "/data/overworld.txt" : file, text)) return {};
    return parseLocations(text);
}
static std::vector<Location> parseLocations(const std::string& text) {
    std::vector<Location> locs;
    {
        for (auto& sec : parseConfig(text)) {
            if (sec.name != "location") continue;
            Location l; l.id = sec.get("id"); l.name = sec.get("name", l.id); l.kind = sec.get("kind", "loc_village"); l.mapId = sec.get("map", l.id);
            l.x = sec.geti("x", 10); l.y = sec.geti("y", 10); l.tx = sec.geti("tx", 0); l.ty = sec.geti("ty", 0); l.clear = sec.geti("clear", 2); l.road = sec.get("road");
            l.requireFlag = sec.get("require"); l.lockedMsg = sec.get("locked");
            locs.push_back(l);
        }
    }
    return locs;
}
static void addLocs(Map& m, const std::vector<Location>& locs);
void addOverworldLocations(Map& m, const std::string& locationsFile) { addLocs(m, readLocations(locationsFile)); }
static void addLocs(Map& m, const std::vector<Location>& locs) {
    Rng rng(7);
    for (auto& l : locs) {
        if (!m.inBounds(l.x, l.y)) continue;
        m.set(l.x, l.y, l.kind, rng);
        bool have = false; for (auto& e : m.exits) if (e.x == l.x && e.y == l.y) have = true;
        if (have) continue;
        Exit e; e.x = l.x; e.y = l.y; e.to = l.mapId; e.tx = l.tx; e.ty = l.ty; e.requireFlag = l.requireFlag; e.lockedMsg = l.lockedMsg; m.exits.push_back(e);
    }
}
void buildOverworld(Map& m, int seed, const std::string& mapFile, const std::string& locationsFile) {
    if (!mapFile.empty() && fileExists(mapFile)) {
        Rng rng((uint64_t)seed); std::string err;
        if (loadMapFile(mapFile, m, rng, err)) {
            m.id = "overworld"; if (m.kind.empty()) m.kind = "overworld"; if (m.name.empty()) m.name = "Aldmoor"; m.dark = false;
            addOverworldLocations(m, locationsFile);
            return;
        }
    }
    generateOverworld(m, seed, locationsFile);
}

static void generateOverworldLocs(Map& m, int seed, const std::vector<Location>& locs);
void generateOverworld(Map& m, int seed, const std::string& locationsFile) { generateOverworldLocs(m, seed, readLocations(locationsFile)); }
void buildOverworldText(Map& m, int seed, const std::string& mapText, const std::string& locationsText) {
    auto locs = parseLocations(locationsText);
    if (!mapText.empty()) {
        Rng rng((uint64_t)seed); std::string err;
        if (loadMapText(mapText, m, rng, err)) {
            m.id = "overworld"; if (m.kind.empty()) m.kind = "overworld"; if (m.name.empty()) m.name = "Aldmoor"; m.dark = false;
            addLocs(m, locs);
            return;
        }
    }
    generateOverworldLocs(m, seed, locs);
}
static void generateOverworldLocs(Map& m, int seed, const std::vector<Location>& locs) {
    Rng rng((uint64_t)seed);
    const int W = 160, H = 140;
    m.init(W, H, "grass", rng);
    m.id = "overworld"; m.name = "Aldmoor"; m.kind = "overworld"; m.encounterRate = 4; m.encounterTable = "plains"; m.generated = true; m.dark = false;
    for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) {
        float nx = x / (float)W, ny = y / (float)H;
        float e = fbm(x * 0.045f, y * 0.045f, seed);
        float moist = fbm(x * 0.03f + 100, y * 0.03f + 100, seed + 7);
        // island falloff
        float dx = nx - 0.5f, dy = ny - 0.5f; float d = sqrtf(dx * dx + dy * dy) * 2.0f;
        // regions: NE = rift wasteland, SW = desert, SE = swamp, N = snow
        float riftF = std::max(0.0f, 1.0f - (fabsf(nx - 0.82f) + fabsf(ny - 0.16f)) * 2.6f);
        float desertF = std::max(0.0f, 1.0f - (fabsf(nx - 0.15f) + fabsf(ny - 0.85f)) * 3.0f);
        float swampF = std::max(0.0f, 1.0f - (fabsf(nx - 0.78f) + fabsf(ny - 0.82f)) * 2.8f);
        float snowF = std::max(0.0f, 1.0f - (fabsf(nx - 0.27f) + fabsf(ny - 0.15f)) * 2.6f);
        float wob = (fbm(x * 0.08f + 300, y * 0.08f + 300, seed + 31) - 0.5f) * 0.5f;
        riftF += wob; desertF += wob; swampF += wob; snowF += wob;
        float regionBoost = std::max(0.0f, std::max(std::max(riftF, snowF), std::max(swampF, desertF)));
        e = e - d * d * 0.55f + 0.12f + regionBoost * 0.22f;
        std::string k;
        if (e < 0.22f) k = "deepwater";
        else if (e < 0.30f) k = "water";
        else if (e < 0.33f) k = "shallows";
        else if (e < 0.36f) k = "sand";
        else if (e > 0.78f) k = snowF > 0.3f ? "snowpeak" : "mountain";
        else if (e > 0.70f) k = (hashf(x, y, seed + 3) < 0.5f) ? "hill" : "hill2";
        else {
            if (riftF > 0.35f) { float r = hashf(x, y, seed + 9); k = r < 0.08f ? "riftcrystal" : r < 0.2f ? "deadtree" : r < 0.25f ? "deadtree2" : r < 0.4f ? "riftscar" : "wasteland"; }
            else if (desertF > 0.35f) { float r = hashf(x, y, seed + 11); k = r < 0.06f ? "cactus" : r < 0.25f ? "dune" : "sand"; }
            else if (swampF > 0.35f) { float r = hashf(x, y, seed + 13); k = r < 0.15f ? "deadtree" : r < 0.3f ? "water" : "swamp"; }
            else if (snowF > 0.4f) { float r = hashf(x, y, seed + 15); k = r < 0.3f ? "snowpine" : "snow"; }
            else if (moist > 0.62f) { float r = hashf(x, y, seed + 5); k = r < 0.15f ? "deepforest" : r < 0.55f ? "forest" : r < 0.8f ? "forest2" : "pine"; }
            else if (moist > 0.52f) { float r = hashf(x, y, seed + 5); k = r < 0.3f ? "forest" : r < 0.45f ? "pine" : r < 0.6f ? "bush" : "grass"; }
            else if (moist < 0.36f) { float r = hashf(x, y, seed + 21); k = r < 0.2f ? "tallgrass" : r < 0.3f ? "bush" : "grass"; }
            else { float r = hashf(x, y, seed + 23); k = r < 0.1f ? "meadow" : r < 0.18f ? "bush" : r < 0.24f ? "tallgrass" : "grass"; }
        }
        m.set(x, y, k, rng);
    }
    // rivers: a couple of meandering lines from mountains to sea
    for (int r = 0; r < 3; r++) {
        int x = rng.range(W / 4, W * 3 / 4), y = rng.range(H / 4, H / 2);
        for (int s = 0; s < 220; s++) {
            if (!m.inBounds(x, y)) break;
            const Cell& c = m.at(x, y); if (c.water) break;
            m.set(x, y, "river", rng);
            float a = fbm(x * 0.1f, y * 0.1f, seed + 40 + r);
            if (a < 0.4f) x--; else if (a > 0.6f) x++; else y++;
        }
    }
    // locations
    auto clearAround = [&](int cx, int cy, int r, const std::string& fillKind) {
        for (int y = cy - r; y <= cy + r; y++) for (int x = cx - r; x <= cx + r; x++) if (m.inBounds(x, y)) {
            const TileKind& k = allTileKinds()[m.at(x, y).kind];
            if (!k.walkable || k.water || k.moveCost > 1) m.set(x, y, fillKind, rng);
        }
    };
    for (auto& l : locs) {
        std::string base = (l.kind == "loc_cave" || l.kind == "loc_ruin" || l.kind == "loc_crypt" || l.kind == "loc_mine") ? "dirt" : (l.kind == "loc_ship" || l.kind == "loc_citymod" || l.kind == "loc_lab") ? "rubble" : "grass";
        clearAround(l.x, l.y, l.clear, base);
        m.set(l.x, l.y, l.kind, rng);
        Exit e; e.x = l.x; e.y = l.y; e.to = l.mapId; e.tx = l.tx; e.ty = l.ty; e.requireFlag = l.requireFlag; e.lockedMsg = l.lockedMsg; m.exits.push_back(e);
        SignPost s; s.x = l.x; s.y = l.y; s.text = l.name; (void)s; // name shown via notify on enter instead
    }
    // roads
    for (auto& l : locs) {
        if (l.road.empty()) continue;
        for (auto& tgtId : split(l.road, ',')) {
            const Location* t = nullptr; for (auto& o : locs) if (o.id == trim(tgtId)) t = &o;
            if (!t) continue;
            int x = l.x, y = l.y;
            int guard = 0;
            while ((x != t->x || y != t->y) && guard++ < 600) {
                int dx = t->x - x, dy = t->y - y;
                bool horiz = abs(dx) > abs(dy) ? (rng.chance(75)) : (rng.chance(25));
                if (dx == 0) horiz = false; if (dy == 0) horiz = true;
                if (horiz) x += dx > 0 ? 1 : -1; else y += dy > 0 ? 1 : -1;
                if (!m.inBounds(x, y)) break;
                const Cell& c = m.at(x, y); const TileKind& k = allTileKinds()[c.kind];
                if (k.biome == "loc") continue;
                if (c.water) m.set(x, y, "bridge", rng);
                else if (!k.walkable || k.moveCost > 1 || k.name == "grass" || k.name == "meadow" || k.name == "bush" || k.name == "tallgrass" || k.name == "sand" || k.name == "wasteland" || k.name == "snow" || k.name == "swamp") m.set(x, y, "road", rng);
            }
        }
    }
    // guarantee no mountains directly adjacent block a location: handled by clearAround.
    // roaming enemy groups near dangerous regions
    std::vector<std::pair<std::string, std::string>> roam = {{"wasteland", "rift_wisp,rift_wisp"}, {"wasteland", "scrap_drone,scrap_drone,scrap_drone"}, {"forest", "wolf,wolf,wolf"}, {"forest", "bandit,bandit_archer"},
                                                             {"desert", "scorpion,scorpion,sand_raider"}, {"swamp", "bog_zombie,bog_zombie,giant_toad"}, {"hills", "ogre"}, {"snow", "frost_wolf,frost_wolf"}, {"plains", "goblin,goblin,goblin"}, {"plains", "slime,slime"}};
    for (int i = 0; i < 70; i++) {
        auto& r = rng.pick(roam);
        for (int attempt = 0; attempt < 40; attempt++) {
            int x = rng.range(2, W - 3), y = rng.range(2, H - 3);
            const TileKind& k = allTileKinds()[m.at(x, y).kind];
            if (k.biome != r.first || !k.walkable) continue;
            bool nearLoc = false; for (auto& l : locs) if (chebyshev(l.x, l.y, x, y) < 7) nearLoc = true;
            if (nearLoc) continue;
            EnemyGroup e; e.x = e.homeX = x; e.y = e.homeY = y; e.aggro = 4; e.levelBonus = (r.first == "wasteland" || r.first == "snow") ? 3 : 0;
            for (auto& c : split(r.second, ',')) e.creatures.push_back(c);
            m.enemies.push_back(e); break;
        }
    }
    // hidden chests in the wild
    for (int i = 0; i < 25; i++) {
        int x = rng.range(2, W - 3), y = rng.range(2, H - 3);
        const TileKind& k = allTileKinds()[m.at(x, y).kind];
        if (!k.walkable || k.biome == "loc" || k.water || m.occupied(x, y)) continue;
        Chest c; c.x = x; c.y = y; c.flag = fmt("ow_chest_%d", i); c.gold = rng.range(4, 25);
        std::vector<std::string> pool = {"potion_red", "potion_red", "potion_blue", "antidote", "bread", "scroll_fire", "iron_dagger", "leather_cap", "ammo_pistol", "grenade", "ring_dex"};
        c.items.push_back({rng.pick(pool), 1}); if (rng.chance(40)) c.items.push_back({rng.pick(pool), 1});
        m.chests.push_back(c);
    }
    // spawn = first location
    if (!locs.empty()) { m.spawnX = locs[0].x; m.spawnY = locs[0].y + 1; }
}

// ---------------- dungeons ----------------
struct Room { int x, y, w, h; int cx() const { return x + w / 2; } int cy() const { return y + h / 2; } };

void generateDungeon(Map& m, const std::string& id, const std::string& style, int W, int H, int seed, int level, const std::string& exitTo, int exitX, int exitY,
                     const std::vector<std::string>& pool, const std::string& bossId, const std::string& bossFlag, const std::string& lootTable) {
    Rng rng((uint64_t)seed * 31 + 7);
    std::string wall = "wall_cave", floor = "floor_cave", door = "door_arch";
    if (style == "crypt") { wall = "wall_gray"; floor = "floor_stone"; }
    else if (style == "castle") { wall = "wall_stone"; floor = "floor_tile"; door = "door_wood"; }
    else if (style == "tech") { wall = "wall_metal"; floor = "floor_metal"; door = "door_metal"; }
    else if (style == "ship") { wall = "wall_hull"; floor = "floor_grid"; door = "door_metal"; }
    else if (style == "mine") { wall = "wall_cave"; floor = "floor_dirt"; }
    else if (style == "sewer") { wall = "wall_brick"; floor = "floor_stone"; }
    // light = the bare lantern glow around each party member; torches, the Light spell and lit props (braziers, wall torches) add to it
    m.init(W, H, wall, rng); m.id = id; m.kind = "dungeon"; m.dark = true; m.lightRadius = style == "tech" || style == "ship" ? 5 : style == "castle" ? 4 : 3; m.generated = true; m.bg = C::Black; m.encounterRate = 0;
    std::vector<Room> rooms;
    if (style == "cave" || style == "mine") {
        // cellular automata
        std::vector<uint8_t> g((size_t)W * H, 0);
        for (int y = 1; y < H - 1; y++) for (int x = 1; x < W - 1; x++) g[(size_t)y * W + x] = rng.chance(52);
        for (int it = 0; it < 5; it++) {
            std::vector<uint8_t> n = g;
            for (int y = 1; y < H - 1; y++) for (int x = 1; x < W - 1; x++) {
                int c = 0; for (int dy = -1; dy <= 1; dy++) for (int dx = -1; dx <= 1; dx++) c += g[(size_t)(y + dy) * W + x + dx];
                n[(size_t)y * W + x] = c >= 5 ? 1 : 0;
            }
            g = n;
        }
        // flood fill largest region, convert to floor
        std::vector<int> comp((size_t)W * H, -1); int best = -1, bestSize = 0, nc = 0;
        for (int y = 1; y < H - 1; y++) for (int x = 1; x < W - 1; x++) {
            if (!g[(size_t)y * W + x] || comp[(size_t)y * W + x] >= 0) continue;
            std::vector<std::pair<int, int>> st{{x, y}}; comp[(size_t)y * W + x] = nc; int sz = 0;
            while (!st.empty()) { auto [cx, cy] = st.back(); st.pop_back(); sz++; const int DX[4] = {1, -1, 0, 0}, DY[4] = {0, 0, 1, -1}; for (int k = 0; k < 4; k++) { int nx = cx + DX[k], ny = cy + DY[k]; if (nx < 1 || ny < 1 || nx >= W - 1 || ny >= H - 1) continue; if (g[(size_t)ny * W + nx] && comp[(size_t)ny * W + nx] < 0) { comp[(size_t)ny * W + nx] = nc; st.push_back({nx, ny}); } } }
            if (sz > bestSize) { bestSize = sz; best = nc; }
            nc++;
        }
        for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) if (comp[(size_t)y * W + x] == best) m.set(x, y, floor, rng);
        // treat open areas as "rooms" by sampling points
        for (int i = 0; i < 14; i++) { for (int a = 0; a < 60; a++) { int x = rng.range(2, W - 3), y = rng.range(2, H - 3); if (m.walkable(x, y)) { rooms.push_back({x - 1, y - 1, 3, 3}); break; } } }
        // decorations
        for (int y = 1; y < H - 1; y++) for (int x = 1; x < W - 1; x++) if (m.walkable(x, y)) { int r = rng.range(0, 100); if (r < 3) m.set(x, y, "mushroom", rng); else if (r < 5) m.set(x, y, "bones", rng); else if (r < 6) m.set(x, y, "rocks", rng); else if (r < 7 && style == "cave") m.set(x, y, "crystal", rng); else if (r < 8) m.set(x, y, "web", rng); }
    } else {
        // rooms and corridors
        for (int i = 0; i < 60 && rooms.size() < 12; i++) {
            Room r{rng.range(2, W - 12), rng.range(2, H - 10), rng.range(5, 10), rng.range(4, 8)};
            if (r.x + r.w >= W - 1 || r.y + r.h >= H - 1) continue;
            bool ov = false; for (auto& o : rooms) if (r.x < o.x + o.w + 2 && r.x + r.w + 2 > o.x && r.y < o.y + o.h + 2 && r.y + r.h + 2 > o.y) ov = true;
            if (ov) continue;
            rooms.push_back(r);
        }
        for (auto& r : rooms) m.fill(r.x, r.y, r.w, r.h, floor, rng);
        for (size_t i = 1; i < rooms.size(); i++) {
            int x0 = rooms[i - 1].cx(), y0 = rooms[i - 1].cy(), x1 = rooms[i].cx(), y1 = rooms[i].cy();
            if (rng.chance(50)) { for (int x = std::min(x0, x1); x <= std::max(x0, x1); x++) m.set(x, y0, floor, rng); for (int y = std::min(y0, y1); y <= std::max(y0, y1); y++) m.set(x1, y, floor, rng); }
            else { for (int y = std::min(y0, y1); y <= std::max(y0, y1); y++) m.set(x0, y, floor, rng); for (int x = std::min(x0, x1); x <= std::max(x0, x1); x++) m.set(x, y1, floor, rng); }
        }
        // decorations per style
        for (auto& r : rooms) {
            for (int k = 0; k < 3; k++) {
                int x = rng.range(r.x, r.x + r.w - 1), y = rng.range(r.y, r.y + r.h - 1);
                if (x == r.cx() && y == r.cy()) continue;
                int roll = rng.range(0, 100);
                if (style == "crypt") { if (roll < 25) m.set(x, y, "coffin", rng); else if (roll < 40) m.set(x, y, "bones", rng); else if (roll < 50) m.set(x, y, "skull", rng); else if (roll < 60) m.set(x, y, "pillar", rng); else if (roll < 68) m.set(x, y, "candle", rng); }
                else if (style == "castle") { if (roll < 20) m.set(x, y, "pillar", rng); else if (roll < 35) m.set(x, y, "table", rng); else if (roll < 50) m.set(x, y, "barrel", rng); else if (roll < 60) m.set(x, y, "brazier", rng); else if (roll < 70) m.set(x, y, "bookshelf", rng); }
                else if (style == "tech" || style == "ship") { if (roll < 20) m.set(x, y, "terminal", rng); else if (roll < 35) m.set(x, y, "server", rng); else if (roll < 50) m.set(x, y, "crate", rng); else if (roll < 60) m.set(x, y, "cryopod", rng); else if (roll < 68) m.set(x, y, "pipe_v", rng); else if (roll < 75) m.set(x, y, "box", rng); }
                else if (style == "sewer") { if (roll < 30) m.set(x, y, "mire", rng); else if (roll < 45) m.set(x, y, "barrel", rng); else if (roll < 55) m.set(x, y, "bones", rng); }
            }
            // torches on walls
            if (style != "tech" && style != "ship") { int tx = r.x + rng.range(0, r.w - 1); if (m.inBounds(tx, r.y - 1) && !m.walkable(tx, r.y - 1)) m.set(tx, r.y - 1, "torch_wall", rng); }
        }
    }
    if (rooms.empty()) { rooms.push_back({2, 2, 6, 6}); m.fill(2, 2, 6, 6, floor, rng); }
    // entrance: room 0 ; boss: farthest room
    Room& ent = rooms[0];
    int ex = ent.cx(), ey = ent.cy();
    m.set(ex, ey, "stairs_up", rng);
    Exit e; e.x = ex; e.y = ey; e.to = exitTo; e.tx = exitX; e.ty = exitY; m.exits.push_back(e);
    m.spawnX = ex + 1; m.spawnY = ey; if (!m.walkable(m.spawnX, m.spawnY)) { int ox, oy; if (m.findFree(ex, ey, 3, ox, oy, rng)) { m.spawnX = ox; m.spawnY = oy; } }
    // save shrine near entrance
    for (int attempt = 0; attempt < 12; attempt++) { int ox, oy; if (!m.findFree(ex + rng.range(-2, 2), ey + rng.range(-2, 2), 2, ox, oy, rng)) continue; if ((ox == ex && oy == ey) || (ox == m.spawnX && oy == m.spawnY)) continue; SavePoint s; s.x = ox; s.y = oy; m.savepoints.push_back(s); break; }
    int far = 0, farD = -1;
    for (size_t i = 1; i < rooms.size(); i++) { int d = manhattan(ex, ey, rooms[i].cx(), rooms[i].cy()); if (d > farD) { farD = d; far = (int)i; } }
    // monsters
    for (size_t i = 1; i < rooms.size(); i++) {
        if ((int)i == far) continue;
        if (pool.empty() || rng.chance(25)) continue;
        EnemyGroup g; int cnt = rng.range(1, 3);
        for (int k = 0; k < cnt; k++) g.creatures.push_back(rng.pick(pool));
        int ox, oy; if (!m.findFree(rooms[i].cx(), rooms[i].cy(), 3, ox, oy, rng)) continue;
        g.x = g.homeX = ox; g.y = g.homeY = oy; g.aggro = 5; g.levelBonus = std::max(0, level - 1); g.flag = fmt("%s_grp_%d", id.c_str(), (int)i);
        m.enemies.push_back(g);
    }
    // boss
    if (!bossId.empty()) {
        Room& br = rooms[far];
        EnemyGroup g; g.creatures.push_back(bossId); if (!pool.empty()) { g.creatures.push_back(rng.pick(pool)); g.creatures.push_back(rng.pick(pool)); }
        int ox, oy; if (m.findFree(br.cx(), br.cy(), 3, ox, oy, rng)) { g.x = g.homeX = ox; g.y = g.homeY = oy; }
        g.aggro = 6; g.flag = bossFlag; g.levelBonus = std::max(0, level - 1);
        m.enemies.push_back(g);
        // boss treasure
        Chest c; if (m.findFree(br.cx(), br.cy(), 3, c.x, c.y, rng)) { c.flag = id + "_boss_chest"; c.gold = 30 + level * 20; c.items.push_back({"potion_red2", 2}); c.items.push_back({lootTable == "tech" ? "cell_pack" : lootTable == "rare" ? "elixir" : "potion_blue", 1}); m.chests.push_back(c); }
    }
    // chests
    std::vector<std::string> common = {"potion_red", "potion_blue", "antidote", "bread", "bandage", "scroll_fire", "ammo_pistol", "torch_item", "torch_item", "flare", "gem_rough", "iron_ore", "iron_ore", "ember_cap", "slime_gel", "oil_fire"};
    std::vector<std::string> tech = {"medkit", "stimpak", "ammo_pistol", "ammo_rifle", "cell_pack", "scrap", "scrap", "circuit", "circuit", "grenade", "emp_grenade", "flare", "power_core", "oil_shock"};
    std::vector<std::string> rare = {"potion_red2", "elixir", "scroll_lightning", "ring_str", "amulet_wis", "steel_sword", "chain_mail", "potion_purple", "steel_ingot", "rift_dust", "oil_blessed", "arcane_dust"};
    auto& ltab = lootTable == "tech" ? tech : lootTable == "rare" ? rare : common;
    int nChests = 2 + rng.range(1, 3);
    for (int i = 0; i < nChests; i++) {
        Room& r = rooms[rng.range(rooms.size() > 1 ? 1 : 0, (int)rooms.size() - 1)];
        Chest c; if (!m.findFree(r.cx(), r.cy(), 3, c.x, c.y, rng)) continue;
        if (m.exitAt(c.x, c.y) || m.saveAt(c.x, c.y)) continue;
        c.flag = fmt("%s_chest_%d", id.c_str(), i); c.gold = rng.range(2, 10) * level + 3;
        c.items.push_back({rng.pick(ltab), 1}); if (rng.chance(20)) c.items.push_back({rng.pick(rare), 1});
        m.chests.push_back(c);
    }
    // things to harvest: ore veins and ember caps in caves and mines, scrap heaps in tech ruins, moonleaf and gel in sewers
    {
        struct G { std::string item, tool, kind; int lo, hi; const ResourceDef* r = nullptr; };
        std::vector<G> opts;
        // 1.1.0: the resource types in resources.txt whose styles= name this style (weight = how many lots in the draw);
        // the built-in table below is only used when no resource type is set up for dungeons at all
        bool fromData = false; for (auto& r : resourceDefs()) if (!r.styles.empty()) fromData = true;
        if (fromData) { for (auto& r : resourceDefs()) for (auto& st : r.styles) if (st.style == style) for (int k = 0; k < st.weight; k++) opts.push_back({r.item, r.tool, r.kind, st.lo >= 0 ? st.lo : r.lo, st.lo >= 0 ? st.hi : r.hi, &r}); }
        else if (style == "mine") opts = {{"iron_ore", "pickaxe", "rocks", 2, 4}, {"iron_ore", "pickaxe", "rocks", 2, 4}, {"ember_cap", "", "mushroom2", 1, 3}, {"gem_rough", "pickaxe", "crystal", 1, 1}};
        else if (style == "cave") opts = {{"iron_ore", "pickaxe", "rocks", 1, 3}, {"ember_cap", "", "mushroom2", 1, 2}, {"ember_cap", "", "mushroom2", 1, 2}};
        else if (style == "tech" || style == "ship") opts = {{"scrap", "", "box", 2, 4}, {"circuit", "", "terminal2", 1, 1}};
        else if (style == "sewer") opts = {{"herb_moon", "", "mushroom", 1, 2}, {"slime_gel", "", "mire", 1, 2}};
        else if (style == "crypt") opts = {{"bone_dust", "", "bones", 1, 2}};
        int want = opts.empty() ? 0 : rng.range(3, 5);
        for (int i = 0, tries = 0; i < want && tries < 200; tries++) {
            int x = rng.range(2, W - 3), y = rng.range(2, H - 3);
            if (!m.walkable(x, y) || m.occupied(x, y) || m.exitAt(x, y) || m.saveAt(x, y) || chebyshev(x, y, m.spawnX, m.spawnY) < 4) continue;
            bool near = false; for (auto& g : m.gathers) if (chebyshev(g.x, g.y, x, y) < 6) near = true; if (near) continue;
            const G& g = rng.pick(opts);
            GatherNode n; n.x = x; n.y = y; n.item = g.item; n.tool = g.tool; n.lo = g.lo; n.hi = g.hi; n.respawn = g.tool.empty() ? 3 : 5;
            if (g.r) { n.respawn = g.r->respawn; n.name = g.r->name; applyResource(n, *g.r); } else if (const ResourceDef* r = resourceFor("", g.item)) applyResource(n, *r);
            if (!g.kind.empty()) m.set(x, y, g.kind, rng); m.gathers.push_back(n); i++;
        }
    }
    // ensure spawn free of monsters
    for (auto& g : m.enemies) if (chebyshev(g.x, g.y, m.spawnX, m.spawnY) < 7) { int ox, oy; if (m.findFree(g.x + 6, g.y + 6, 6, ox, oy, rng)) { g.x = g.homeX = ox; g.y = g.homeY = oy; } }
}
