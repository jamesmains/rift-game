// Dev tool: loads every map and reports objects placed on blocked tiles or unreachable spots.
#include "../src/core/gfx.h"
#include "../src/game/world.h"
#include <cstdio>
#include <filesystem>
int main() {
    gAssetDir = "assets"; int problems = 0;
    for (auto& e : std::filesystem::directory_iterator("assets/data/maps")) {
        Map m; Rng rng(1); std::string err;
        if (!loadMapFile(e.path().string(), m, rng, err)) { printf("%s: %s\n", e.path().c_str(), err.c_str()); problems++; continue; }
        auto chk = [&](const char* what, const std::string& id, int x, int y) { if (!m.inBounds(x, y) || !m.walkable(x, y)) { printf("%s: %s %s at %d,%d is not walkable (kind %s)\n", m.id.c_str(), what, id.c_str(), x, y, m.inBounds(x, y) ? allTileKinds()[m.at(x, y).kind].name.c_str() : "OOB"); problems++; } };
        for (auto& n : m.npcs) chk("npc", n.id, n.x, n.y);
        for (auto& c : m.chests) chk("chest", c.flag, c.x, c.y);
        for (auto& s : m.savepoints) chk("savepoint", "", s.x, s.y);
        for (auto& x : m.exits) for (int yy = x.y; yy < x.y + x.h; yy++) for (int xx = x.x; xx < x.x + x.w; xx++) chk("exit", x.to, xx, yy);
        for (auto& g : m.enemies) chk("enemy", g.flag, g.x, g.y);
        chk("spawn", "", m.spawnX, m.spawnY);
        // reachability from spawn (doors count as passable)
        std::vector<uint8_t> seen((size_t)m.w * m.h, 0); std::vector<std::pair<int, int>> q{{m.spawnX, m.spawnY}}; seen[(size_t)m.spawnY * m.w + m.spawnX] = 1;
        for (size_t h = 0; h < q.size(); h++) { auto [x, y] = q[h]; for (int d = 0; d < 4; d++) { int nx = x + (d == 0) - (d == 1), ny = y + (d == 2) - (d == 3); if (!m.inBounds(nx, ny) || seen[(size_t)ny * m.w + nx]) continue; if (!m.walkable(nx, ny) && !m.doorAt(nx, ny)) continue; seen[(size_t)ny * m.w + nx] = 1; q.push_back({nx, ny}); } }
        auto reach = [&](const char* what, const std::string& id, int x, int y) { bool ok = false; for (int d = 0; d < 5; d++) { int nx = x + (d == 0) - (d == 1), ny = y + (d == 2) - (d == 3); if (m.inBounds(nx, ny) && seen[(size_t)ny * m.w + nx]) ok = true; } if (!ok) { printf("%s: %s %s at %d,%d unreachable from spawn\n", m.id.c_str(), what, id.c_str(), x, y); problems++; } };
        for (auto& n : m.npcs) reach("npc", n.id, n.x, n.y);
        for (auto& c : m.chests) reach("chest", c.flag, c.x, c.y);
        for (auto& s : m.savepoints) reach("savepoint", "", s.x, s.y);
        for (auto& x : m.exits) reach("exit", x.to, x.x, x.y);
        // NPC adjacent to a walkable tile?
        for (auto& n : m.npcs) { bool adj = false; for (int d = 0; d < 4; d++) { int nx = n.x + (d == 0) - (d == 1), ny = n.y + (d == 2) - (d == 3); if (m.walkable(nx, ny) && !m.occupied(nx, ny)) adj = true; } if (!adj) { printf("%s: npc %s at %d,%d has no walkable neighbour\n", m.id.c_str(), n.id.c_str(), n.x, n.y); problems++; } }
    }
    printf("%d problems\n", problems); return problems ? 1 : 0;
}
