// Newer game systems: world clock & weather, shop hours. (More live in journal.cpp / bounty.cpp / console.cpp.)
#include "game.h"
#include "../core/audio.h"
#include "tiles.h"
#include <algorithm>
#include <cmath>

using namespace UI;

bool Game::outdoors() const { return map && !map->dark && map->kind != "interior"; }

float Game::nightAmount() const {
    int m = clockMin;
    if (m >= 7 * 60 && m < 18 * 60) return 0;
    if (m >= 21 * 60 || m < 5 * 60) return 1;
    if (m >= 18 * 60) return (m - 18 * 60) / 180.0f;      // dusk 18-21
    return 1.0f - (m - 5 * 60) / 120.0f;                  // dawn 5-7
}
std::string Game::clockText() const { return fmt("Day %d  %02d:%02d", day, clockMin / 60, clockMin % 60); }
void Game::advanceClock(int minutes) { clockMin += minutes; while (clockMin >= 24 * 60) { clockMin -= 24 * 60; day++; } }

void Game::rerollWeather() {
    weatherSteps = rng.range(180, 420);
    std::string biome; if (map && map->inBounds(px, py)) biome = allTileKinds()[map->at(px, py).kind].biome;
    int r = rng.range(1, 100);
    std::string w = "clear";
    if (biome == "snow") w = r < 45 ? "clear" : r < 85 ? "snow" : "fog";
    else if (biome == "desert" || biome == "wasteland") w = r < 85 ? "clear" : "fog";
    else if (biome == "swamp") w = r < 35 ? "clear" : r < 65 ? "fog" : r < 90 ? "rain" : "storm";
    else w = r < 55 ? "clear" : r < 78 ? "rain" : r < 90 ? "fog" : "storm";
    // the first half hour of a new game stays calm: a new player should see the world before the weather hides it
    if (playtimeSec < 30 * 60 && (w == "fog" || w == "storm")) w = biome == "snow" ? "snow" : "clear";
    if (w != weather) { weather = w; precip.clear(); if (w == "rain") notify("It starts to rain."); else if (w == "storm") notify("A storm rolls in."); else if (w == "fog") notify("Fog creeps over the land."); else if (w == "snow") notify("Snow begins to fall."); else notify("The sky clears."); }
}

// ---------------- 1.8.0 (#160-#162): the Stage palette, the lantern circle, dark-map light ----------------
// The Stage ramp: luminance -> grey-blue. Input = the day pixel's luminance x scale (0.82 at night, 1 for dark-map
// shadow, whose pixels drawWorld has already darkened), then L < 120 -> L x 1.15, else 138 + (L - 120) x 0.6.
namespace {
struct StageRamp { Color8 c[256]; };
void buildStageRamp(StageRamp& out, float scale) {
    static const int P[6][4] = {{0, 14, 17, 26}, {40, 26, 31, 46}, {90, 52, 60, 82}, {150, 98, 108, 134}, {220, 160, 170, 194}, {255, 205, 212, 228}};
    for (int i = 0; i < 256; i++) {
        float l = i * scale; l = l < 120 ? l * 1.15f : 138 + (l - 120) * 0.6f; l = std::max(0.0f, std::min(255.0f, l));
        int k = 0; while (k < 4 && l > P[k + 1][0]) k++;
        float t = std::max(0.0f, std::min(1.0f, (l - P[k][0]) / float(P[k + 1][0] - P[k][0])));
        auto ch = [&](int j) { return (uint8_t)std::lround(P[k][j] + (P[k + 1][j] - P[k][j]) * t); };
        out.c[i] = RGB(ch(1), ch(2), ch(3));
    }
}
const StageRamp& nightRamp() { static StageRamp r; static bool made = false; if (!made) { buildStageRamp(r, 0.82f); made = true; } return r; }
const StageRamp& shadeRamp() { static StageRamp r; static bool made = false; if (!made) { buildStageRamp(r, 1.0f); made = true; } return r; }
// per view tile: what the pass does to its pixels
struct TileLook {
    uint8_t mode = 0;              // 0 untouched, 1 Stage ramp, 2 day colours x mul (lantern / lit), 3 black (1.8.1 enclosed)
    bool water = false, bank = false; Color8 bankBg;   // water: the ramp keeps 45% of #284096 (not on its bank pixels)
    const StageRamp* ramp = nullptr;
    int mr = 256, mg = 256, mb = 256;   // mode 2: brightness x warm tint, 8.8 fixed point
    int blend = 256;               // how far from the day pixel to go (nightAmount at dusk / dawn), 0-256
};
// a ring index 0-3 inside a lantern circle of radius r tiles (the four steps spread over the radius), 4 = outside
int lanternRing(float d, float r) { for (int i = 0; i < 4; i++) if (d <= r / 4.0f * (i + 1) + 0.5f) return i; return 4; }
}

bool Game::partyLightOn() const {
    if (lightSteps > 0) return true;
    for (auto& m : party.members) if (m.alive) for (int s = 0; s < SL_COUNT; s++) if (!m.equip[s].empty()) if (auto* it = gd->item(m.equip[s])) if (it->light > 0) return true;
    return false;
}

std::vector<Game::NightLamp> Game::nightLamps(bool combat) const {
    // the party: explore = the leader; a fight = every living party unit (the active one carries the big lantern)
    std::vector<std::pair<int, int>> partyAt; int bigX = px, bigY = py;
    if (combat) {
        for (auto& u : cb.units) if (u.partyIndex >= 0 && u.ch.alive) partyAt.push_back({u.x, u.y});
        int a = -1;
        if (cb.turnPos >= 0 && cb.turnPos < (int)cb.order.size()) a = cb.order[cb.turnPos];
        if (a < 0 || a >= (int)cb.units.size() || cb.units[a].partyIndex < 0 || !cb.units[a].ch.alive) a = cb.selectedUnit;
        if (a >= 0 && a < (int)cb.units.size() && cb.units[a].partyIndex >= 0 && cb.units[a].ch.alive) { bigX = cb.units[a].x; bigY = cb.units[a].y; }
        else if (!partyAt.empty()) { bigX = partyAt[0].first; bigY = partyAt[0].second; }
    }
    std::vector<NightLamp> lamps{{bigX, bigY, partyLightOn() ? 8.0f : 4.0f}};
    if (combat) for (auto& p : partyAt) lamps.push_back({p.first, p.second, 2.0f});   // every party unit stays readable in a night fight
    return lamps;
}

bool Game::darkHides(int x, int y, bool combat) const {
    if (!map) return false;
    if (map->dark) return !(visible(x, y) && lit(x, y));
    if (!outdoors() || nightAmount() < 0.5f) return false;
    for (auto& l : nightLamps(combat)) if (lanternRing(std::sqrt((float)((x - l.x) * (x - l.x) + (y - l.y) * (y - l.y))), l.r) < 4) return false;
    return true;
}

std::vector<uint16_t> Game::sceneryLook(Canvas& cv, int cx, int cy, int x0, int y0, int x1, int y1, const std::vector<Color8>& viewBg, bool combat) {
    std::vector<uint16_t> dim;
    if (!map || x1 < x0 || y1 < y0 || (settings && settings->nightClassic)) return dim;
    bool out = outdoors(), dark = map->dark, sky = map->skyOpen();
    float n = out ? nightAmount() : 0;
    if (!dark && n <= 0.01f) return dim;   // day, or an interior: the day look as drawn
    const int vw = x1 - x0 + 1, vh = y1 - y0 + 1, viewH = SCREEN_H - 72;
    const auto& kinds = allTileKinds();
    std::vector<TileLook> look((size_t)vw * vh);
    static const float WARM[3] = {1.00f, 0.95f, 0.84f};
    auto setMul = [](TileLook& t, float bright, float warm) {
        t.mode = 2; t.mr = (int)std::lround(256 * bright * (1 - warm + warm * WARM[0]));
        t.mg = (int)std::lround(256 * bright * (1 - warm + warm * WARM[1])); t.mb = (int)std::lround(256 * bright * (1 - warm + warm * WARM[2]));
    };
    auto waterOf = [&](TileLook& t, const Cell& c, Color8 bg) {
        const TileKind* k = c.kind < kinds.size() ? &kinds[c.kind] : nullptr;
        if (!k || !k->water || k->name.find("lava") != std::string::npos) return;   // chosen by kind; lava isn't water
        t.water = true;
        // a shore / river piece stands on the land next to it (under=1): its bg pixels are that land, not water
        if (!(bg.r == k->dayBg.r && bg.g == k->dayBg.g && bg.b == k->dayBg.b)) { t.bank = true; t.bankBg = bg; }
    };
    const std::vector<NightLamp> lamps = nightLamps(combat);   // [0] = the leader / active fighter, then (fights) every party unit
    if (dark) {
        // 1.8.0 (#162): lit tiles (computeLight) keep the day colours, banded by the nearest light; the warm tint only in
        // the three rings round the party. Seen-but-unlit and remembered tiles (drawn darker already) take the ramp.
        static const float BAND[5] = {1.00f, 0.90f, 0.78f, 0.68f, 0.60f};
        static const float WARMD[3] = {0.45f, 0.30f, 0.15f};
        for (int ty = y0; ty <= y1; ty++) for (int tx = x0; tx <= x1; tx++) {
            const Cell& c = map->at(tx, ty); TileLook& t = look[(size_t)(ty - y0) * vw + (tx - x0)];
            if (!c.explored) continue;   // unexplored stays black
            size_t i = (size_t)ty * map->w + tx;
            bool seenLit = visible(tx, ty) && lit(tx, ty);
            if (!seenLit && !sky) continue;   // 1.8.1 (#166) enclosed: drawWorld left it pitch black
            if (seenLit) {
                float d = i < lightDist.size() ? lightDist[i] : 0;
                int b = d <= 1.5f ? 0 : d <= 2.5f ? 1 : d <= 3.5f ? 2 : d <= 4.5f ? 3 : 4;
                float pd = 1e9f; for (auto& p : lamps) pd = std::min(pd, std::sqrt((float)((tx - p.x) * (tx - p.x) + (ty - p.y) * (ty - p.y))));
                float w = pd <= 1.5f ? WARMD[0] : pd <= 2.5f ? WARMD[1] : pd <= 3.5f ? WARMD[2] : 0.0f;
                setMul(t, BAND[b], w);
            } else { t.mode = 1; t.ramp = &shadeRamp(); waterOf(t, c, viewBg[(size_t)(ty - y0) * vw + (tx - x0)]); }
        }
    } else {
        // 1.8.0 (#160 #161): the Stage ramp at night, blended in through dusk / dawn; around the party a lantern circle
        // stepped in whole tiles (anchored to the map, so it never swims) shows the day colours with a warm tint.
        static const float BRIGHT[4] = {1.00f, 0.85f, 0.68f, 0.50f};
        int blend = (int)std::lround(256 * std::min(1.0f, n));
        int k = 256 - (int)std::lround(51 * std::min(1.0f, n));   // x0.8 at full night
        if (!combat) dim.assign((size_t)vw * vh, 256);
        for (int ty = y0; ty <= y1; ty++) for (int tx = x0; tx <= x1; tx++) {
            const Cell& c = map->at(tx, ty); size_t vi = (size_t)(ty - y0) * vw + (tx - x0); TileLook& t = look[vi];
            int ring = 4;
            for (auto& l : lamps) ring = std::min(ring, lanternRing(std::sqrt((float)((tx - l.x) * (tx - l.x) + (ty - l.y) * (ty - l.y))), l.r));
            if (ring < 4) setMul(t, BRIGHT[ring], 0.35f);
            else if (!sky) { t.mode = 3; if (!dim.empty()) dim[vi] = (uint16_t)k; }   // 1.8.1 (#166) enclosed: black outside the lantern
            else { t.mode = 1; t.ramp = &nightRamp(); waterOf(t, c, viewBg[vi]); if (!dim.empty()) dim[vi] = (uint16_t)k; }
            t.blend = blend;
        }
    }
    // the pass: per tile, over that tile's pixels
    for (int ty = y0; ty <= y1; ty++) for (int tx = x0; tx <= x1; tx++) {
        const TileLook& t = look[(size_t)(ty - y0) * vw + (tx - x0)];
        if (t.mode == 0) continue;
        int sx0 = std::max(0, tx * TILE - cx), sy0 = std::max(0, ty * TILE - cy);
        int sx1 = std::min(std::min(SCREEN_W, cv.w), tx * TILE - cx + TILE), sy1 = std::min(std::min(viewH, cv.h), ty * TILE - cy + TILE);
        for (int y = sy0; y < sy1; y++) {
            Color8* row = &cv.px[(size_t)y * cv.w];
            for (int x = sx0; x < sx1; x++) {
                Color8& p = row[x]; int r = p.r, g = p.g, b = p.b, tr, tg, tb;
                if (t.mode == 3) { tr = tg = tb = 0; }
                else if (t.mode == 1) {
                    const Color8& m = t.ramp->c[(r * 77 + g * 150 + b * 29) >> 8]; tr = m.r; tg = m.g; tb = m.b;
                    if (t.water && !(t.bank && r == t.bankBg.r && g == t.bankBg.g && b == t.bankBg.b)) { tr = (tr * 141 + 40 * 115) >> 8; tg = (tg * 141 + 64 * 115) >> 8; tb = (tb * 141 + 150 * 115) >> 8; }
                } else { tr = std::min(255, r * t.mr >> 8); tg = std::min(255, g * t.mg >> 8); tb = std::min(255, b * t.mb >> 8); }
                if (t.blend < 256) { tr = r + (tr - r) * t.blend / 256; tg = g + (tg - g) * t.blend / 256; tb = b + (tb - b) * t.blend / 256; }
                p.r = (uint8_t)tr; p.g = (uint8_t)tg; p.b = (uint8_t)tb;
            }
        }
    }
    return dim;
}

void Game::drawWeather(Canvas& cv, float dt) {
    if (!map) return;
    int viewH = SCREEN_H - 72;
    cv.setClip({0, 0, SCREEN_W, viewH});
    bool out = outdoors();
    // ---- night tint (outdoor maps only) ----
    // 1.8.0 (#160): only with Settings > Display "Night colours: Darken (classic)"; the default Stage look is drawn by
    // sceneryLook inside drawWorld. Classic: the light circle around the party is computed per 4x4 block anchored to the
    // world (so it doesn't swim when the camera moves) and stepped into a few hard bands instead of a smooth gradient.
    float n = out && settings && settings->nightClassic ? nightAmount() : 0;
    if (n > 0.01f) {
        const int B = 4;
        int lx = px * TILE + TILE / 2, ly = py * TILE + TILE / 2;       // world pixels
        float lr = (float)TILE * (4.0f + (lightSteps > 0 ? 4.0f : 0.0f));
        int ox = ((camX % B) + B) % B, oy = ((camY % B) + B) % B;
        for (int by = -oy; by < viewH; by += B) for (int bx = -ox; bx < SCREEN_W; bx += B) {
            float dx = (float)(bx + camX + B / 2 - lx), dy = (float)(by + camY + B / 2 - ly); float d2 = dx * dx + dy * dy;
            float k = n;
            if (d2 < lr * lr) { float t = std::sqrt(d2) / lr; float band = std::floor(t * 5.0f) / 5.0f; k = n * (0.25f + 0.75f * band * band); }
            int fr = (int)(256 * (1.0f - 0.68f * k)), fg = (int)(256 * (1.0f - 0.62f * k)), fb = (int)(256 * (1.0f - 0.40f * k));
            for (int y = std::max(0, by); y < std::min(viewH, by + B); y++) for (int x = std::max(0, bx); x < std::min(SCREEN_W, bx + B); x++) {
                Color8& p = cv.px[(size_t)y * cv.w + x];
                p.r = (uint8_t)(p.r * fr >> 8); p.g = (uint8_t)(p.g * fg >> 8); p.b = (uint8_t)(p.b * fb >> 8);
            }
        }
    }
    if (!out || weather == "clear") { cv.resetClip(); return; }
    // ---- precipitation ----
    bool rain = weather == "rain" || weather == "storm", snow = weather == "snow";
    if (rain || snow) {
        int want = weather == "storm" ? 160 : rain ? 90 : 70;
        while ((int)precip.size() < want) precip.push_back({(float)rng.range(0, SCREEN_W), (float)rng.range(0, viewH)});
        for (auto& p : precip) {
            if (rain) { p.first += dt * 90; p.second += dt * (weather == "storm" ? 520 : 380); cv.line((int)p.first, (int)p.second, (int)p.first + 1, (int)p.second + 6, RGB(170, 190, 230, 150)); }
            else { p.first += std::sin(p.second * 0.05f + ctx.time) * dt * 20; p.second += dt * 45; cv.put((int)p.first, (int)p.second, RGB(240, 240, 255, 200)); cv.put((int)p.first + 1, (int)p.second, RGB(240, 240, 255, 120)); }
            if (p.second > viewH) { p.second = (float)-rng.range(0, 20); p.first = (float)rng.range(-10, SCREEN_W); }
            if (p.first > SCREEN_W + 10) p.first -= SCREEN_W + 20;
        }
        if (rain) cv.fillRect(0, 0, SCREEN_W, viewH, RGB(40, 50, 80, weather == "storm" ? 60 : 35));
    }
    if (weather == "storm") {
        thunderT -= dt;
        if (thunderT <= 0) { thunderT = (float)rng.range(6, 16); stormFlash = 0.18f; gAudio.play("thunder"); if (settings && settings->screenShake) shake = std::max(shake, 0.15f); }
        if (stormFlash > 0 && settings && settings->reduceFlashing) stormFlash = 0;   // Settings > Access: no lightning flashes
        if (stormFlash > 0) { stormFlash -= dt; cv.fillRect(0, 0, SCREEN_W, viewH, RGB(255, 255, 255, (uint8_t)(140 * std::min(1.0f, stormFlash / 0.18f)))); }
    }
    if (weather == "fog") {
        // Chunky drifting fog: 6px blocks locked to the world grid, density stepped into five levels with an
        // ordered (Bayer) dither between them, so it reads like the rest of the pixel art instead of a smooth blur.
        static const int bayer[16] = {0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5};
        auto hash = [](int x, int y) { uint32_t h = (uint32_t)x * 374761393u + (uint32_t)y * 668265263u; h = (h ^ (h >> 13)) * 1274126177u; h ^= h >> 16; return (h & 0xFFFF) / 65535.0f; };
        auto vnoise = [&](float x, float y) { int xi = (int)std::floor(x), yi = (int)std::floor(y); float fx = x - xi, fy = y - yi; fx = fx * fx * (3 - 2 * fx); fy = fy * fy * (3 - 2 * fy);
            float a = hash(xi, yi), b = hash(xi + 1, yi), c = hash(xi, yi + 1), d = hash(xi + 1, yi + 1); return a + (b - a) * fx + (c - a) * fy + (a - b - c + d) * fx * fy; };
        const int B = 6;
        int lx = px * TILE + TILE / 2, ly = py * TILE + TILE / 2; float fr = (float)TILE * 5.5f;
        int ox = ((camX % B) + B) % B, oy = ((camY % B) + B) % B;
        float drift = ctx.time * 0.35f;
        for (int by = -oy; by < viewH; by += B) for (int bx = -ox; bx < SCREEN_W; bx += B) {
            int wx = bx + camX, wy = by + camY; int gx = (int)std::floor(wx / (float)B), gy = (int)std::floor(wy / (float)B);
            float dx = (float)(wx + B / 2 - lx), dy = (float)(wy + B / 2 - ly); float d = std::sqrt(dx * dx + dy * dy);
            float a = d < fr ? (d / fr) * (d / fr) * 0.55f : std::min(0.85f, 0.55f + (d - fr) / (TILE * 6.0f) * 0.3f);
            a += (vnoise(gx * 0.11f + drift, gy * 0.16f + drift * 0.3f) - 0.5f) * 0.35f;
            float lvl = std::max(0.0f, std::min(1.0f, a)) * 4.0f; int base = (int)lvl; float frac = lvl - base;
            if (frac * 16.0f > bayer[((gy & 3) << 2) | (gx & 3)]) base++;
            if (base <= 0) continue;
            cv.fillRect(bx, by, B, B, Color8{200, 205, 215, (uint8_t)std::min(215, base * 52)});
        }
    }
    cv.resetClip();
}

bool Game::shopOpen(const std::string& shop, std::string& hoursOut) const {
    hoursOut = "all day";
    auto secs = mergedConfig("shops.txt"); std::reverse(secs.begin(), secs.end());
    for (auto& sec : secs) {
        if (sec.name != "shop" || sec.get("id") != shop) continue;
        std::string h = sec.get("hours"); if (h.empty()) return true;
        auto p = split(h, '-'); if (p.size() != 2) return true;
        int a = atoi(p[0].c_str()), b = atoi(p[1].c_str()); int hr = clockMin / 60;
        hoursOut = fmt("%02d:00-%02d:00", a, b);
        if (a <= b) return hr >= a && hr < b;
        return hr >= a || hr < b;
    }
    return true;
}

// ---- shop doors ----
// A building's door is locked while the shop inside is closed (inns never lock). The building is found by flooding
// in from the door: the side that stays small and walled in is the inside; any NPC there whose dialogue opens a shop
// with opening hours locks it. Party members already inside can always walk out.
const std::pair<std::string, std::vector<int>>& Game::doorShop(int x, int y) {
    int key = y * map->w + x;
    auto it = map->doorShops.find(key); if (it != map->doorShops.end()) return it->second;
    auto& out = map->doorShops[key];
    const auto& kinds = allTileKinds();
    auto kindName = [&](int cx, int cy) -> std::string { const Cell& c = map->at(cx, cy); return c.kind < kinds.size() ? kinds[c.kind].name : ""; };
    auto isDoor = [&](int cx, int cy) { return startsWith(kindName(cx, cy), "door"); };
    auto passable = [&](int cx, int cy) {
        const Cell& c = map->at(cx, cy); if (c.walkable) return true;
        std::string n = kindName(cx, cy);
        return !c.blocksSight && n.find("window") == std::string::npos && n.find("fence") == std::string::npos && n.find("wall") == std::string::npos && n.find("tree") == std::string::npos && n.find("water") == std::string::npos;
    };
    const int dirs[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
    for (auto& d : dirs) {
        int sx = x + d[0], sy = y + d[1]; if (!map->inBounds(sx, sy) || isDoor(sx, sy) || !passable(sx, sy)) continue;
        std::vector<int> seen, stack = {sy * map->w + sx}; std::vector<char> mark(map->cells.size(), 0); mark[stack[0]] = 1; mark[key] = 1;
        bool leaked = false;
        while (!stack.empty() && !leaked) {
            int c = stack.back(); stack.pop_back(); seen.push_back(c);
            if ((int)seen.size() > 160) { leaked = true; break; }
            int cx = c % map->w, cy = c / map->w;
            if (cx == 0 || cy == 0 || cx == map->w - 1 || cy == map->h - 1) { leaked = true; break; }
            for (auto& e : dirs) { int nx = cx + e[0], ny = cy + e[1]; if (!map->inBounds(nx, ny)) continue; int ni = ny * map->w + nx; if (mark[ni] || isDoor(nx, ny) || !passable(nx, ny)) continue; mark[ni] = 1; stack.push_back(ni); }
        }
        if (leaked) continue;
        std::string shop; bool inn = false;
        for (auto& n : map->npcs) {
            if (!mark[n.y * map->w + n.x] || n.hidden) continue;
            if (!n.shop.empty() && shop.empty()) shop = n.shop;
            auto dl = dialogues.find(n.dialogue); if (dl == dialogues.end()) continue;
            for (auto& nd : dl->second.nodes) for (auto& a : nd.second.actions) { if (a.first == "rest") inn = true; if (a.first == "shop" && shop.empty()) shop = a.second; }
        }
        std::string hrs; bool hasHours = false;
        if (!shop.empty()) { bool open = shopOpen(shop, hrs); (void)open; hasHours = hrs != "all day"; }
        if (!inn && hasHours) { out.first = shop; out.second = seen; }
        break;
    }
    return out;
}
std::string Game::closedShopDoor(int x, int y) {
    if (!map || !map->inBounds(x, y)) return "";
    const auto& kinds = allTileKinds(); const Cell& c = map->at(x, y);
    if (c.kind >= kinds.size() || !startsWith(kinds[c.kind].name, "door")) return "";
    const auto& ds = doorShop(x, y); if (ds.first.empty()) return "";
    std::string hrs; if (shopOpen(ds.first, hrs)) return "";
    int here = py * map->w + px; for (int i : ds.second) if (i == here) return "";   // already inside: you can always leave
    return "The door is locked - closed for the night. Open " + hrs + ".";
}
