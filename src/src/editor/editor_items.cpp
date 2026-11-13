// 1.1.8 (owner's #32): the Items tab's "Look" card, above the item's fields.
// Every item picture on the sheet is drawn in a single colour, so an item can be shown as a white shape in any colour
// (items.txt color=r,g,b; the game draws it with drawItemTile). Without color= it keeps the sheet's own colour, which
// the card shows as the starting value. The tile picker still shows the sheet as it is. The palette offers the colours
// the items already use plus the sheet's most used ones.
#include "editor.h"
#include <algorithm>
#include <map>
#include <cmath>

using namespace ED;

namespace {
int lum(Color8 c) { return (c.r * 3 + c.g * 6 + c.b) / 10; }
bool closeTo(Color8 a, Color8 b, int d) { return std::abs(a.r - b.r) <= d && std::abs(a.g - b.g) <= d && std::abs(a.b - b.b) <= d; }
// hue order for the palette: greys first (dark to light), then by hue, then by lightness
float hueKey(Color8 c) {
    int mx = std::max({c.r, c.g, c.b}), mn = std::min({c.r, c.g, c.b});
    if (mx - mn < 24) return -1000.f + lum(c) / 1000.f;
    float d = (float)(mx - mn), h;
    if (mx == c.r) h = std::fmod((c.g - c.b) / d, 6.f); else if (mx == c.g) h = (c.b - c.r) / d + 2.f; else h = (c.r - c.g) / d + 4.f;
    if (h < 0) h += 6.f;
    return (float)(int)(h * 2.f) * 10.f + lum(c) / 1000.f;   // 12 hue bands, light within a band
}
std::string rgbText(Color8 c) { return std::to_string(c.r) + "," + std::to_string(c.g) + "," + std::to_string(c.b); }
}

// the colour a picture is drawn in on the sheet (its most common non-black pixel); white if it has none
Color8 Editor::tileOwnColor(Tile t) {
    if (!t.valid() || !ts) return C::White;
    auto ck = ownColCache.find(t.c * 1000 + t.r); if (ck != ownColCache.end()) return ck->second;
    const Image8& im = ts->img; std::map<uint32_t, int> n;
    int bx = 1 + t.c * Tileset::PITCH, by = 1 + t.r * Tileset::PITCH;
    for (int y = 0; y < Tileset::TS; y++) for (int x = 0; x < Tileset::TS; x++) {
        int sx = bx + x, sy = by + y; if (sx >= im.w || sy >= im.h) continue;
        Color8 c = im.get(sx, sy); if (c.r == 0 && c.g == 0 && c.b == 0) continue;
        n[(uint32_t)c.r << 16 | (uint32_t)c.g << 8 | c.b]++;
    }
    uint32_t best = 0xFFFFFF; int bn = 0; for (auto& p : n) if (p.second > bn) { bn = p.second; best = p.first; }
    return ownColCache[t.c * 1000 + t.r] = RGB((uint8_t)(best >> 16), (uint8_t)(best >> 8), (uint8_t)best);
}

// the sheet's colours, most used first (each counted once per picture; near-identical shades merged)
const std::vector<std::pair<Color8, int>>& Editor::sheetColors() {
    if (!sheetPal.empty() || !ts || ts->img.w == 0) return sheetPal;
    const Image8& im = ts->img; std::map<uint32_t, int> cnt;
    int cols = (im.w - 1) / Tileset::PITCH, rows = (im.h - 1) / Tileset::PITCH;
    for (int r = 0; r < rows; r++) for (int c = 0; c < cols; c++) {
        std::vector<uint32_t> seen;
        for (int y = 0; y < Tileset::TS; y++) for (int x = 0; x < Tileset::TS; x++) {
            Color8 p = im.get(1 + c * Tileset::PITCH + x, 1 + r * Tileset::PITCH + y);
            if (lum(p) < 24) continue;
            uint32_t k = (uint32_t)p.r << 16 | (uint32_t)p.g << 8 | p.b;
            if (std::find(seen.begin(), seen.end(), k) == seen.end()) seen.push_back(k);
        }
        for (auto k : seen) cnt[k]++;
    }
    std::vector<std::pair<uint32_t, int>> all(cnt.begin(), cnt.end());
    std::sort(all.begin(), all.end(), [](auto& a, auto& b) { return a.second > b.second; });
    for (auto& a : all) {
        Color8 c = RGB((uint8_t)(a.first >> 16), (uint8_t)(a.first >> 8), (uint8_t)a.first); bool merged = false;
        for (auto& p : sheetPal) if (closeTo(p.first, c, 10)) { p.second += a.second; merged = true; break; }
        if (!merged) sheetPal.push_back({c, a.second});
    }
    return sheetPal;
}


int Editor::drawItemLook(GenericEditor& g, EdSection& s, int x, int y, int w) { return drawLookCard(g, s, x, y, w, false); }
// 1.1.9: the same card for NPCs (npcs.txt sprite= / color=; NPC pictures are one colour on the sheet too)
int Editor::drawLookCard(GenericEditor& g, EdSection& s, int x, int y, int w, bool npc) {
    const int H = 124;
    const std::string tk = npc ? "sprite" : "tile", defTile = npc ? "104,0" : "26,46";   // the game's defaults when the key is missing
    ui.cv->fillRect(x + 1, y, w - 2, H, PANEL2); ui.cv->fillRect(x + 1, y + H - 1, w - 2, 1, BORDER);
    std::string tileStr = s.get(tk);
    Tile t = parseTile(s.has(tk) && !trim(tileStr).empty() ? tileStr : defTile);
    std::string cs = trim(s.get("color")); bool own = cs.empty();
    Color8 sheet = tileOwnColor(t), col = own ? sheet : parseColor(cs);
    auto drawPic = [&](int px, int py, int scale) { if (own) ts->draw(*ui.cv, t, px, py, scale); else ts->drawRecolor(*ui.cv, t, px, py, scale, col); };
    bool clear = false; bool setC = false; Color8 newC = col;
    // big preview on the black the game draws the world on
    int bx = x + 8, by = y + 6;
    ui.cv->fillRect(bx, by, 100, 100, C::Black); ui.cv->rect(bx - 1, by - 1, 102, 102, BORDER);
    drawPic(bx + 2, by + 2, 8);
    if (!t.valid()) ui.text(bx + 22, by + 44, "no sprite", DIM);
    // how it looks in the game: a menu row (1x) and on the ground (2x)
    int vx = x + w - 62;
    if (npc) { ui.cv->fillRect(vx, y + 6, 16, 16, C::Black); ts->drawBacking(*ui.cv, t, vx + 2, y + 8, 1); drawPic(vx + 2, y + 8, 1); if (ui.hover(vx, y + 6, 16, 16)) ui.tip("On the map, as the game draws it");
        ui.cv->fillRect(vx + 22, y + 6, 28, 28, C::Black); ts->drawBacking(*ui.cv, t, vx + 24, y + 8, 2); drawPic(vx + 24, y + 8, 2); if (ui.hover(vx + 22, y + 6, 28, 28)) ui.tip("On the map, zoomed in"); }
    else {
    ui.cv->fillRect(vx, y + 6, 16, 16, C::Panel); drawPic(vx + 2, y + 8, 1); if (ui.hover(vx, y + 6, 16, 16)) ui.tip("In a menu (pack, shop, crafting)");
    ui.cv->fillRect(vx + 22, y + 6, 28, 28, C::Black); drawPic(vx + 24, y + 8, 2); if (ui.hover(vx + 22, y + 6, 28, 28)) ui.tip("On the ground and on the hotbar"); }
    // sprite
    int cx = x + 120, cy = y + 6;
    ui.label(cx, cy + 2, "sprite");
    if (ui.textField(cx + 50, cy, 64, tileStr, 20)) { s.set(tk, tileStr); g.dirty = true; }
    if (ui.button(cx + 120, cy, 92, 16, "Pick sprite...")) { EdSection* sp = &s; GenericEditor* gp = &g; std::string key = tk; openTilePicker(t, [sp, gp, key](Tile nt) { if (!nt.valid()) return; sp->set(key, fmt("%d,%d", nt.c, nt.r)); gp->dirty = true; }); }
    if (ui.hover(cx + 120, cy, 92, 16)) ui.tip(std::string(own ? "The picker shows the sheet as it is; the " : "The picker shows the sheet as it is; the new picture is drawn in this ") + (npc ? "NPC" : "item") + (own ? " keeps the picture's own colour" : "'s colour"));
    if (npc && ui.hover(cx + 50, cy, 64, 16)) ui.tip("col,row on the sheet. People are in columns 104-123 (and 141, 156, 166); animals in rows 14-16 near the left");
    // colour: the RGB of the tint (the sheet's own colour when color= isn't set)
    cy += 22;
    ui.label(cx, cy + 2, "colour");
    int r = col.r, gg = col.g, b = col.b; bool ch = false;
    if (ui.intField(cx + 50, cy, 58, r, 0, 255)) ch = true; if (ui.intField(cx + 112, cy, 58, gg, 0, 255)) ch = true; if (ui.intField(cx + 174, cy, 58, b, 0, 255)) ch = true;
    if (ch) { setC = true; newC = RGB((uint8_t)r, (uint8_t)gg, (uint8_t)b); }
    ui.cv->fillRect(cx + 236, cy, 20, 16, col); ui.cv->rect(cx + 236, cy, 20, 16, BORDER);
    if (ui.button(cx + 262, cy, 90, 16, "Sheet colour", !own, own)) clear = true;
    if (ui.hover(cx + 262, cy, 90, 16)) ui.tip("The picture's own colour on the sheet (" + rgbText(sheet) + "): no color= key");
    if (ui.button(cx + 356, cy, 54, 16, "Lighter")) { setC = true; newC = RGB((uint8_t)(col.r + (255 - col.r) / 5), (uint8_t)(col.g + (255 - col.g) / 5), (uint8_t)(col.b + (255 - col.b) / 5)); }
    if (ui.button(cx + 414, cy, 52, 16, "Darker")) { setC = true; newC = RGB((uint8_t)(col.r * 4 / 5), (uint8_t)(col.g * 4 / 5), (uint8_t)(col.b * 4 / 5)); }
    cy += 20;
    ui.text(cx, cy, own ? "Its own colour from the sheet. Pick a colour to draw it as a white shape in that colour."
                        : "Drawn as a white shape in " + rgbText(col) + ".  (Sheet colour: " + rgbText(sheet) + ")", own ? DIM : ACCENT2);
    // palette: the colours the items use (as drawn now) plus the sheet's most used, in hue order
    struct Sw { Color8 c; int sheetN = 0, items = 0; std::string example; };
    std::vector<Sw> pal;
    auto add = [&](Color8 c, int sheetN, const std::string& item) {
        for (auto& p : pal) if (closeTo(p.c, c, item.empty() ? 10 : 4)) { p.sheetN += sheetN; if (!item.empty()) { p.items++; if (p.example.empty()) p.example = item; } return; }
        Sw sw; sw.c = c; sw.sheetN = sheetN; if (!item.empty()) { sw.items = 1; sw.example = item; } pal.push_back(sw);
    };
    for (auto& it : g.secs) { std::string ic = trim(it.get("color")); Tile itT = parseTile(it.has(tk) && !trim(it.get(tk)).empty() ? it.get(tk) : defTile);
        add(ic.empty() ? tileOwnColor(itT) : parseColor(ic), 0, it.get("name", it.get("id"))); }
    int pcols = std::max(8, (w - 128) / 17), cap = pcols * 3;
    for (auto& sc : sheetColors()) { if ((int)pal.size() >= cap) break; add(sc.first, sc.second, ""); }
    if ((int)pal.size() > cap) pal.resize(cap);
    std::stable_sort(pal.begin(), pal.end(), [](const Sw& a, const Sw& b2) { return hueKey(a.c) < hueKey(b2.c); });
    int py0 = y + 66;
    for (size_t i = 0; i < pal.size(); i++) {
        int sx = cx + (int)(i % pcols) * 17, sy = py0 + (int)(i / pcols) * 17;
        ui.cv->fillRect(sx, sy, 15, 15, pal[i].c);
        bool cur = !own ? closeTo(pal[i].c, col, 0) : false;
        if (cur) { ui.cv->rect(sx - 2, sy - 2, 19, 19, C::White); }
        if (ui.hover(sx, sy, 15, 15)) {
            ui.cv->rect(sx - 1, sy - 1, 17, 17, ACCENT2);
            std::string tip = rgbText(pal[i].c);
            if (pal[i].items) tip += pal[i].items == 1 ? " - used by " + pal[i].example : " - used by " + std::to_string(pal[i].items) + (npc ? " NPCs (" : " items (") + pal[i].example + "...)";
            else tip += " - a sheet colour";
            ui.tip(tip);
        }
        if (ui.clicked(sx, sy, 15, 15)) { setC = true; newC = pal[i].c; }
    }
    if (clear) { for (size_t i = 0; i < s.kv.size(); i++) if (s.kv[i].first == "color") { s.kv.erase(s.kv.begin() + (long)i); break; } g.dirty = true; }
    else if (setC) { s.set("color", rgbText(newC)); g.dirty = true; }
    return H;
}
