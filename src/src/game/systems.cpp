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

void Game::drawWeather(Canvas& cv, float dt) {
    if (!map) return;
    int viewH = SCREEN_H - 72;
    cv.setClip({0, 0, SCREEN_W, viewH});
    bool out = outdoors();
    // ---- night tint (outdoor maps only) ----
    // Pixel-art friendly: the light circle around the party is computed per 4x4 block anchored to the world (so it
    // doesn't swim when the camera moves) and stepped into a few hard bands instead of a smooth gradient.
    float n = out ? nightAmount() : 0;
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
