// Dev tool: renders the whole overworld to a PNG (1 tile = 4px) for checking layout.
#include "../src/core/gfx.h"
#include "../src/game/world.h"
#include <cstdio>
int main() {
    gAssetDir = "assets";
    Map m; generateOverworld(m, 1337);
    Canvas cv; cv.init(m.w * 4, m.h * 4);
    auto& kinds = allTileKinds();
    for (int y = 0; y < m.h; y++) for (int x = 0; x < m.w; x++) {
        const Cell& c = m.at(x, y); const TileKind& k = kinds[c.kind];
        Color8 col = k.bg; if (k.biome == "loc") col = C::Red; else if (k.name == "road" || k.name == "bridge") col = RGB(210, 190, 150); else if (!k.walkable && !k.water) col = RGB(col.r / 2 + 60, col.g / 2 + 60, col.b / 2 + 60);
        cv.fillRect(x * 4, y * 4, 4, 4, col);
    }
    for (auto& e : m.enemies) cv.fillRect(e.x * 4, e.y * 4, 4, 4, C::Yellow);
    savePNG(cv, "/tmp/claude-0/ow.png");
    printf("exits: %d enemies: %d chests: %d\n", (int)m.exits.size(), (int)m.enemies.size(), (int)m.chests.size());
    for (auto& e : m.exits) printf("  %s at %d,%d walkable=%d\n", e.to.c_str(), e.x, e.y, m.walkable(e.x, e.y));
}
