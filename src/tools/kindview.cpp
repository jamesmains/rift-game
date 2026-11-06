// Dev tool: renders every tile kind (as the game would draw it) into a PNG contact sheet.
#include "../src/core/gfx.h"
#include "../src/game/world.h"
#include "../src/game/tiles.h"
#include <cstdio>

int main(int argc, char** argv) {
    Tileset ts; if (!ts.load("assets/tiles.png")) { printf("no tileset\n"); return 1; }
    Font font; font.build(ts);
    auto& kinds = allTileKinds();
    int cols = 6, cellW = 150, cellH = 30;
    int rows = ((int)kinds.size() + cols - 1) / cols;
    Canvas cv; cv.init(cols * cellW, rows * cellH + 20);
    cv.clear(RGB(30, 30, 40));
    Rng rng(1);
    for (size_t i = 0; i < kinds.size(); i++) {
        auto& k = kinds[i];
        int x = (int)(i % cols) * cellW, y = (int)(i / cols) * cellH + 4;
        for (int v = 0; v < 2 && v < (int)k.ground.size(); v++) {
            Tile g = k.ground[v];
            cv.fillRect(x + 4 + v * 26, y, 24, 24, k.bg);
            if (g.valid()) { if (k.opaque) ts.drawOpaque(cv, g, x + 4 + v * 26, y, 2, k.tint); else ts.draw(cv, g, x + 4 + v * 26, y, 2, k.tint); }
            if (k.prop.valid()) ts.draw(cv, k.prop, x + 4 + v * 26, y, 2);
        }
        font.draw(cv, ts, x + 58, y + 6, k.name, k.walkable ? C::White : RGB(255, 150, 150));
    }
    savePNG(cv, argc > 1 ? argv[1] : "kinds.png");
    // also a text sample
    Canvas cv2; cv2.init(400, 120); cv2.clear(C::Teal);
    font.draw(cv2, ts, 4, 4, "The quick brown fox jumps over 1234567890", C::White);
    font.bold = true; font.draw(cv2, ts, 4, 20, "Bold: Triffall Save Load Settings!", C::Yellow); font.bold = false;
    font.draw(cv2, ts, 4, 36, "HP 42/60  AP 6  STR 12 DEX 9 (x,y) [ok] {a} <b> ?!", C::Cream);
    font.drawWrapped(cv2, ts, 4, 52, 380, "A long paragraph of dialogue that should wrap nicely across several lines in the box without overflowing the edge.", C::White);
    savePNG(cv2, "font.png");
    return 0;
}
