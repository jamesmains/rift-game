// World map (M): a fold-out map of the overworld with discovered locations, quest pins and fast travel.
#include "game.h"
#include "../core/audio.h"
#include "tiles.h"
#include <algorithm>
#include <cmath>

using namespace UI;

void Game::loadLocations() {
    locations.clear(); std::string t;
    if (!readFile(dataPath("overworld.txt"), t)) return;
    for (auto& sec : parseConfig(t)) {
        if (sec.name != "location") continue;
        WorldLocation l; l.id = sec.get("id"); l.name = sec.get("name", l.id); l.kind = sec.get("kind"); l.mapId = sec.get("map", l.id); l.x = sec.geti("x"); l.y = sec.geti("y"); l.tx = sec.geti("tx"); l.ty = sec.geti("ty"); l.reveal = sec.geti("reveal", 0);
        if (!l.id.empty()) locations.push_back(l);
    }
}

static bool locKnown(const Game& g, const WorldLocation& l, const Map* ow) {
    if (g.visited.count(l.mapId)) return true;
    if (ow && ow->inBounds(l.x, l.y) && ow->at(l.x, l.y).explored) return true;
    return false;
}

int Game::travelFare(const WorldLocation& l) const {
    int fx = px, fy = py; if (map->kind != "overworld") for (auto& o : locations) if (o.mapId == mapId) { fx = o.x; fy = o.y; }
    int dist = chebyshev(fx, fy, l.x, l.y);
    return 8 + (dist / 10 + 1) * 6;   // 8 gold flat + 6 per league
}
// campaign.txt worldmap=: overworld (default) | off | current (an automap of the map you are on) | <map id> (that map
// always, e.g. a castle's great hall for a campaign that never goes outside)
std::string Game::worldMapSource() const {
    std::string w = toLower(trim(campaign.worldMap));
    // a map can name its own world map (a sewer that shows the sewer, a castle wing that shows the castle)
    if (map && !trim(map->worldMap).empty()) { w = toLower(trim(map->worldMap)); if (w == "self" || w == "current") return mapId; if (w == "off" || w == "none" || w == "0") return ""; if (w == "overworld") return "overworld"; return trim(map->worldMap); }
    if (w.empty() || w == "overworld") return "overworld";
    if (w == "off" || w == "none" || w == "0") return "";
    if (w == "current") return mapId.empty() ? std::string("overworld") : mapId;
    return trim(campaign.worldMap);
}
std::string Game::mapDisplayName(const std::string& id) const {
    auto c = mapNameCache.find(id); if (c != mapNameCache.end()) return c->second;
    std::string nm;
    auto it = maps.find(id); if (it != maps.end() && it->second && !it->second->name.empty()) nm = it->second->name;
    std::string t;
    if (nm.empty() && readFile(dataPath("maps/" + id + ".txt"), t)) { bool inMap = false; for (auto& raw : split(t, '\n')) { std::string l = trim(raw); if (l == "[map]") { inMap = true; continue; } if (!l.empty() && l[0] == '[') { if (inMap) break; continue; } if (inMap && startsWith(l, "name=")) { nm = trim(l.substr(5)); break; } } }
    if (nm.empty()) for (auto& sec : mergedConfig("dungeons.txt")) if (sec.name == "dungeon" && sec.get("id") == id) nm = sec.get("name");
    if (nm.empty()) for (auto& l : locations) if (l.mapId == id) nm = l.name;
    if (nm.empty()) nm = id == "overworld" ? "Outside" : id;
    mapNameCache[id] = nm; return nm;
}
// the places shown on a non-overworld world map: one per exit, named after where it leads
std::vector<WorldPlace> Game::worldPlaces(Map* m) const {
    std::vector<WorldPlace> out; if (!m) return out;
    for (auto& e : m->exits) {
        if (e.to.empty() || e.to == m->id) continue;
        bool dup = false; for (auto& p : out) if (p.mapId == e.to) dup = true; if (dup) continue;
        WorldPlace p; p.mapId = e.to; p.name = mapDisplayName(e.to); p.x = e.x + std::max(1, e.w) / 2; p.y = e.y + std::max(1, e.h) / 2; p.visited = visited.count(e.to) > 0;
        for (int yy = e.y; yy < e.y + std::max(1, e.h); yy++) for (int xx = e.x; xx < e.x + std::max(1, e.w); xx++) if (m->inBounds(xx, yy) && m->at(xx, yy).explored) p.known = true;
        out.push_back(p);
    }
    return out;
}
void Game::revealMap(const std::string& id, int x, int y, int r) {
    Map* m = loadMap(id); if (!m) return;
    for (int yy = 0; yy < m->h; yy++) for (int xx = 0; xx < m->w; xx++) if (r < 0 || (xx - x) * (xx - x) + (yy - y) * (yy - y) <= r * r) m->at(xx, yy).explored = true;
}

void Game::openWorldMap(bool travelMode, bool pick) {
    if (!running || !map) return;
    wmPick = pick; if (!pick) wmPickPrompt.clear();
    std::string src = travelMode ? std::string("overworld") : worldMapSource();   // the coach always works on the overworld
    if (src.empty()) { log("There is no map of this place."); gAudio.play("ui_error"); return; }
    wmTravel = travelMode; wmSource = src;
    Map* src0 = loadMap(src);
    wmSel = -1; wmScroll = 0;
    // preselect the location we are in
    if (src == "overworld") { for (size_t i = 0; i < locations.size(); i++) if (locations[i].mapId == mapId) wmSel = (int)i; }
    else { auto pl = worldPlaces(src0); for (size_t i = 0; i < pl.size(); i++) if (pl[i].mapId == mapId && pl[i].known) wmSel = (int)i; }
    pushMode(GMode::WORLDMAP);
}

void Game::updateWorldMap(InputState& in, float dt) {
    (void)dt;
    if (actP(in, A_CANCEL) || actP(in, A_WORLDMAP)) { popMode(); return; }
    if (wmSource != "overworld") { updateLocalMap(in); return; }
    Map* ow = loadMap("overworld");
    std::vector<int> known; for (size_t i = 0; i < locations.size(); i++) if (locKnown(*this, locations[i], ow)) known.push_back((int)i);
    if (known.empty()) return;
    int ki = -1; for (size_t i = 0; i < known.size(); i++) if (known[i] == wmSel) ki = (int)i;
    if (actP(in, A_DOWN) || actP(in, A_RIGHT)) { ki = (ki + 1) % (int)known.size(); wmSel = known[ki]; }
    if (actP(in, A_UP) || actP(in, A_LEFT)) { ki = (ki + (int)known.size() - 1) % (int)known.size(); wmSel = known[ki]; }
    // mouse pick on the map
    const int S = 3, MX = 10, MY = 30;
    if (in.mouseDX || in.mouseDY || in.mousePressed[0]) {
        int best = -1, bd = 10 * 10;
        for (int k : known) { int sx = MX + locations[k].x * S, sy = MY + locations[k].y * S; int d = (in.mouseX - sx) * (in.mouseX - sx) + (in.mouseY - sy) * (in.mouseY - sy); if (d < bd) { bd = d; best = k; } }
        if (best >= 0 && (in.mousePressed[0] || in.mouseDX || in.mouseDY)) wmSel = best;
        // list on the right
        int lx = MX + 160 * S + 10, ly = MY + 8;
        for (size_t i = 0; i < known.size(); i++) { int ry = ly + (int)i * 14 - wmScroll * 14; if (ry < ly || ry > SCREEN_H - 90) continue; if (in.mouseX >= lx && in.mouseX < SCREEN_W - 8 && in.mouseY >= ry && in.mouseY < ry + 14) { if (in.mousePressed[0] || in.mouseDX || in.mouseDY) wmSel = known[i]; } }
    }
    if (in.wheel) wmScroll = clampi(wmScroll - in.wheel, 0, std::max(0, (int)known.size() - 20));
    bool travelKey = actP(in, A_CONFIRM) || actP(in, A_INTERACT) || (in.mousePressed[0] && in.mouseX >= SCREEN_W - 150 && in.mouseY >= SCREEN_H - 50 && in.mouseY < SCREEN_H - 24) || (in.mousePressed[0] && in.mouseX < MX + 160 * S && in.mouseY > MY && in.mouseY < MY + 140 * S && in.mousePressed[0] && wmSel >= 0 && std::abs(in.mouseX - (MX + locations[wmSel].x * S)) < 6 && std::abs(in.mouseY - (MY + locations[wmSel].y * S)) < 6 && in.down[K_SHIFT]);
    if (travelKey && wmSel >= 0) {
        const WorldLocation& l = locations[wmSel];
        bool here = l.mapId == mapId;
        if (!wmTravel) { log("To travel, hire the coach in any town square."); return; }
        if (here) { log("You are already there."); return; }
        if (wmPick) {   // 1.1.10: a script's pick - the chosen place is handed back, the script does the rest
            if (!visited.count(l.mapId)) { log("You have not been to " + l.name + " yet."); gAudio.play("ui_error"); return; }
            wmPickResult = l.id; popMode(); return;
        }
        if (!visited.count(l.mapId)) { log("You have not been to " + l.name + " yet. The coach only goes where you have been."); return; }
        int fare = travelFare(l); if (party.gold < fare) { log(fmt("The fare to %s is %d gold. You have %d.", l.name.c_str(), fare, party.gold)); gAudio.play("ui_error"); return; }
        party.gold -= fare; stats.goldSpent += fare; log(fmt("Paid %d gold for the coach to %s.", fare, l.name.c_str()));
        popMode(); fastTravel(l);
    }
}

void Game::fastTravel(const WorldLocation& l) {
    // where are we on the overworld right now?
    int fx = px, fy = py;
    if (map->kind != "overworld") { for (auto& o : locations) if (o.mapId == mapId) { fx = o.x; fy = o.y; } }
    int dist = chebyshev(fx, fy, l.x, l.y);
    int minutes = dist * 4; advanceClock(minutes);
    stats.travels++;
    // encounter roll: one per 12 tiles, 22% each (stealth halves it), never on day 1's first trip
    int rolls = std::max(1, dist / 12); bool ambush = false;
    for (int i = 0; i < rolls; i++) if (rng.chance(stealthSteps > 0 ? 11 : 22)) ambush = true;
    Map* ow = loadMap("overworld");
    int ax = l.x, ay = l.y + 1; if (!ow->walkable(ax, ay)) { int ox, oy; if (ow->findFree(l.x, l.y, 4, ox, oy, rng)) { ax = ox; ay = oy; } }
    gAudio.play("travel");
    std::string msg = fmt("Travelling to %s (%d leagues, %dh %02dm)...", l.name.c_str(), dist / 10 + 1, minutes / 60, minutes % 60);
    startTransition("overworld", ax, ay, msg);
    if (ambush) {
        extra["_ambush"] = "1";
        log("Ambushed on the road!");
    }
    weatherSteps = 0; // new weather on arrival
}

void Game::drawWorldMap(Canvas& cv, InputState& in) {
    if (wmSource != "overworld" && !wmSource.empty()) { drawLocalMap(cv, in); return; }
    cv.dim(0, 0, SCREEN_W, SCREEN_H, 200); uiRect(0, 0, SCREEN_W, SCREEN_H);
    Map* ow = loadMap("overworld");
    const int S = 3, MX = 10, MY = 30;
    panel(ctx, 4, 4, SCREEN_W - 8, SCREEN_H - 8, RGB(60, 48, 30), RGB(180, 140, 70));
    ctx.font->bold = true; UI::text(ctx, 12, 10, wmPick && !wmPickPrompt.empty() ? wmPickPrompt : "Map of Aldmoor", C::Cream); ctx.font->bold = false;
    textRight(ctx, SCREEN_W - 12, 10, clockText() + "  " + weather, C::Cream);
    // terrain
    cv.fillRect(MX - 2, MY - 2, ow->w * S + 4, ow->h * S + 4, RGB(210, 190, 140)); cv.rect(MX - 2, MY - 2, ow->w * S + 4, ow->h * S + 4, RGB(90, 70, 40));
    const auto& kinds = allTileKinds();
    for (int y = 0; y < ow->h; y++) for (int x = 0; x < ow->w; x++) {
        const Cell& c = ow->at(x, y);
        if (!c.explored) { cv.fillRect(MX + x * S, MY + y * S, S, S, RGB(214, 196, 150)); if (((x + y) & 7) == 0) cv.put(MX + x * S + 1, MY + y * S + 1, RGB(190, 170, 125)); continue; }
        const TileKind& k = kinds[c.kind];
        Color8 col;
        if (k.water) col = RGB(70, 110, 190);
        else if (startsWith(k.name, "loc_")) col = RGB(230, 60, 60);
        else if (k.name == "road" || k.name == "path" || k.name == "bridge") col = RGB(150, 110, 60);
        else { Color8 t = c.bg; float m = k.walkable ? 1.0f : 0.7f; col = RGB((uint8_t)(t.r * m), (uint8_t)(t.g * m), (uint8_t)(t.b * m)); if (k.biome == "snow") col = RGB(220, 225, 235); }
        cv.fillRect(MX + x * S, MY + y * S, S, S, col);
    }
    // locations
    bool blink = (ctx.frame / 15) % 2 == 0;
    std::set<std::string> pins; for (auto& q : quests) { std::string p = questPin(q); if (!p.empty() && !questClosed(q)) pins.insert(p); }
    std::set<std::string> bpins; for (auto& b : bounties) if (!b.done && !b.where.empty()) bpins.insert(b.where); else if (b.done) { std::string pin = b.town; if (const BoardDef* bd = board(b.town)) for (auto& l : locations) if ((!bd->location.empty() && l.id == bd->location) || (bd->location.empty() && l.mapId == bd->map)) pin = l.id; bpins.insert(pin); }
    for (size_t i = 0; i < locations.size(); i++) {
        auto& l = locations[i]; if (!locKnown(*this, l, ow)) continue;
        int sx = MX + l.x * S + 1, sy = MY + l.y * S + 1; bool vis = visited.count(l.mapId) > 0;
        Color8 c = vis ? C::White : RGB(200, 200, 200);
        bool isTown = l.kind == "loc_village" || l.kind == "loc_town" || l.kind == "loc_city" || l.kind == "loc_citymod" || l.kind == "loc_farm";
        if (isTown) { cv.fillRect(sx - 3, sy - 3, 7, 7, c); cv.rect(sx - 3, sy - 3, 7, 7, C::Black); } else { cv.fillRect(sx - 2, sy - 2, 5, 5, c); cv.rect(sx - 2, sy - 2, 5, 5, C::Black); }
        if ((int)i == wmSel) { cv.rect(sx - 6, sy - 6, 13, 13, C::Yellow); }
        if (pins.count(l.id) && blink) { cv.fillRect(sx - 1, sy - 12, 3, 8, C::Yellow); cv.fillRect(sx - 4, sy - 12, 9, 4, C::Yellow); }
        if (bpins.count(l.id) && !blink) { cv.fillRect(sx - 1, sy - 12, 3, 8, C::Cyan); cv.fillRect(sx - 4, sy - 12, 9, 4, C::Cyan); }
        if (vis || (int)i == wmSel) { std::string nm = vis ? l.name : "?"; int tw = ctx.font->width(nm); int tx = clampi(sx - tw / 2, MX, MX + ow->w * S - tw); ctx.font->drawShadow(cv, *ctx.ts, tx, sy + 5, nm, (int)i == wmSel ? C::Yellow : RGB(40, 30, 10), RGB(255, 245, 220, 180)); }
    }
    // the party
    { int hx = px, hy = py; if (map->kind != "overworld") for (auto& o : locations) if (o.mapId == mapId) { hx = o.x; hy = o.y; }
      int sx = MX + hx * S + 1, sy = MY + hy * S + 1; if (blink) { cv.fillRect(sx - 2, sy - 2, 5, 5, C::Cyan); cv.rect(sx - 3, sy - 3, 7, 7, C::Black); } }
    // right column: list + details
    int lx = MX + ow->w * S + 10, ly = MY + 8; int lw = SCREEN_W - lx - 8;
    UI::text(ctx, lx, MY - 8, "Discovered places", C::Cream);
    std::vector<int> known; for (size_t i = 0; i < locations.size(); i++) if (locKnown(*this, locations[i], ow)) known.push_back((int)i);
    if (known.empty()) { auto ls = ctx.font->wrap("Nothing charted yet. Step outside and look around - the map fills in as you explore the overworld, and every place you visit is added here.", lw); for (size_t i = 0; i < ls.size(); i++) UI::text(ctx, lx, ly + (int)i * 13, ls[i], C::Gray); }
    for (size_t i = 0; i < known.size(); i++) {
        int ry = ly + (int)i * 14 - wmScroll * 14; if (ry < ly || ry > SCREEN_H - 90) continue;
        auto& l = locations[known[i]]; bool vis = visited.count(l.mapId) > 0; bool sel = known[i] == wmSel;
        if (sel) cv.fillRect(lx - 2, ry - 1, lw, 14, RGB(120, 90, 40));
        UI::text(ctx, lx, ry, vis ? l.name : "? (seen)", sel ? C::Yellow : vis ? C::White : C::Gray);
        if (pins.count(l.id)) UI::text(ctx, lx + lw - 14, ry, "!", C::Yellow); else if (bpins.count(l.id)) UI::text(ctx, lx + lw - 14, ry, "$", C::Cyan);
    }
    // details of selection
    int dy = SCREEN_H - 80;
    if (wmSel >= 0 && wmSel < (int)locations.size()) {
        auto& l = locations[wmSel]; bool vis = visited.count(l.mapId) > 0;
        int fx = px, fy = py; if (map->kind != "overworld") for (auto& o : locations) if (o.mapId == mapId) { fx = o.x; fy = o.y; }
        int dist = chebyshev(fx, fy, l.x, l.y);
        ctx.font->bold = true; UI::text(ctx, lx, dy, vis ? l.name : "Unexplored place", C::Cream); ctx.font->bold = false;
        int leagues = dist / 10 + 1;
        UI::text(ctx, lx, dy + 14, fmt("%d league%s | %dh%02dm", leagues, leagues == 1 ? "" : "s", dist * 4 / 60, dist * 4 % 60), C::LtGray);
        for (auto& q : quests) if (questPin(q) == l.id && !questClosed(q)) UI::text(ctx, lx, dy + 28, "! " + q.name, C::Yellow);
        bool can = wmTravel && vis && l.mapId != mapId;
        if (wmPick) button(ctx, in, SCREEN_W - 150, SCREEN_H - 50, 140, 24, can ? std::string("Choose") : !vis ? "Not visited yet" : "You are here", can, can);
        else if (wmTravel) { int fare = travelFare(l); button(ctx, in, SCREEN_W - 150, SCREEN_H - 50, 140, 24, can ? fmt("Travel - %d gold", fare) : !vis ? "Not visited yet" : "You are here", can && party.gold >= fare, can); }
    }
    UI::text(ctx, MX, SCREEN_H - 24, wmPick ? "Quests: yellow  Bounties: cyan  Enter: choose  Esc: none" : "Quests: yellow  Bounties: cyan  Coach: travel (time, ambush)  Esc: close", C::Cream);
}

// ---------------- a world map made from any other map (worldmap=<map id> or current) ----------------
// The map is scaled into the same frame as the overworld map; it fills in as its tiles are explored (or revealmap=).
namespace { struct LocalFrame { float sc; int ox, oy; }; }
static LocalFrame localFrame(const Map& m) {
    const float AW = 480, AH = 420; float sc = std::min(AW / std::max(1, m.w), AH / std::max(1, m.h)); sc = std::min(sc, 12.0f);
    return {sc, 10 + (int)((AW - m.w * sc) / 2), 30 + (int)((AH - m.h * sc) / 2)};
}
static std::set<std::string> localPins(const Game& g) {
    std::set<std::string> pins;
    for (auto& q : g.quests) { std::string p = g.questPin(q); if (p.empty() || g.questClosed(q)) continue; pins.insert(p); for (auto& l : g.locations) if (l.id == p) pins.insert(l.mapId); }
    return pins;
}
void Game::updateLocalMap(InputState& in) {
    Map* m = loadMap(wmSource); if (!m) return;
    auto places = worldPlaces(m);
    std::vector<int> known; for (size_t i = 0; i < places.size(); i++) if (places[i].known) known.push_back((int)i);
    if (known.empty()) return;
    int ki = -1; for (size_t i = 0; i < known.size(); i++) if (known[i] == wmSel) ki = (int)i;
    if (actP(in, A_DOWN) || actP(in, A_RIGHT)) { ki = (ki + 1) % (int)known.size(); wmSel = known[ki]; }
    if (actP(in, A_UP) || actP(in, A_LEFT)) { ki = (ki + (int)known.size() - 1) % (int)known.size(); wmSel = known[ki]; }
    LocalFrame f = localFrame(*m);
    if (in.mouseDX || in.mouseDY || in.mousePressed[0]) {
        int best = -1, bd = 12 * 12;
        for (int k : known) { int sx = f.ox + (int)((places[k].x + 0.5f) * f.sc), sy = f.oy + (int)((places[k].y + 0.5f) * f.sc); int d = (in.mouseX - sx) * (in.mouseX - sx) + (in.mouseY - sy) * (in.mouseY - sy); if (d < bd) { bd = d; best = k; } }
        if (best >= 0) wmSel = best;
        int lx = 10 + 480 + 10, ly = 30 + 8;
        for (size_t i = 0; i < known.size(); i++) { int ry = ly + (int)i * 14 - wmScroll * 14; if (ry < ly || ry > SCREEN_H - 90) continue; if (in.mouseX >= lx && in.mouseX < SCREEN_W - 8 && in.mouseY >= ry && in.mouseY < ry + 14) wmSel = known[i]; }
    }
    if (in.wheel) wmScroll = clampi(wmScroll - in.wheel, 0, std::max(0, (int)known.size() - 20));
    if ((actP(in, A_CONFIRM) || actP(in, A_INTERACT)) && wmSel >= 0) log("This map shows the way - there is no coach to take you there.");
}
void Game::drawLocalMap(Canvas& cv, InputState& in) {
    (void)in;
    cv.dim(0, 0, SCREEN_W, SCREEN_H, 200); uiRect(0, 0, SCREEN_W, SCREEN_H);
    Map* m = loadMap(wmSource); if (!m) return;
    panel(ctx, 4, 4, SCREEN_W - 8, SCREEN_H - 8, RGB(60, 48, 30), RGB(180, 140, 70));
    ctx.font->bold = true; UI::text(ctx, 12, 10, "Map of " + (m->name.empty() ? mapDisplayName(wmSource) : m->name), C::Cream); ctx.font->bold = false;
    textRight(ctx, SCREEN_W - 12, 10, clockText() + "  " + weather, C::Cream);
    LocalFrame f = localFrame(*m);
    int fw = (int)(m->w * f.sc), fh = (int)(m->h * f.sc);
    cv.fillRect(f.ox - 2, f.oy - 2, fw + 4, fh + 4, RGB(210, 190, 140)); cv.rect(f.ox - 2, f.oy - 2, fw + 4, fh + 4, RGB(90, 70, 40));
    const auto& kinds = allTileKinds();
    for (int y = 0; y < m->h; y++) for (int x = 0; x < m->w; x++) {
        int x0 = f.ox + (int)(x * f.sc), y0 = f.oy + (int)(y * f.sc), x1 = f.ox + (int)((x + 1) * f.sc), y1 = f.oy + (int)((y + 1) * f.sc);
        if (x1 <= x0) x1 = x0 + 1; if (y1 <= y0) y1 = y0 + 1;
        const Cell& c = m->at(x, y);
        if (!c.explored) { cv.fillRect(x0, y0, x1 - x0, y1 - y0, RGB(214, 196, 150)); continue; }
        const TileKind& k = kinds[c.kind < kinds.size() ? c.kind : 0];
        Color8 col;
        if (k.water || c.water) col = RGB(70, 110, 190);
        else if (!c.walkable && k.name != "void") col = RGB(84, 64, 40);
        else if (k.name == "void") col = RGB(40, 32, 22);
        else { Color8 t = c.bg; col = RGB((uint8_t)(t.r * 0.35f + 120), (uint8_t)(t.g * 0.35f + 104), (uint8_t)(t.b * 0.35f + 76)); }
        cv.fillRect(x0, y0, x1 - x0, y1 - y0, col);
    }
    for (auto& d : m->doors) if (m->inBounds(d.x, d.y) && m->at(d.x, d.y).explored) cv.fillRect(f.ox + (int)(d.x * f.sc), f.oy + (int)(d.y * f.sc), std::max(1, (int)f.sc), std::max(1, (int)f.sc), d.open ? RGB(200, 150, 80) : RGB(150, 90, 40));
    bool blink = (ctx.frame / 15) % 2 == 0;
    auto places = worldPlaces(m); auto pins = localPins(*this);
    for (size_t i = 0; i < places.size(); i++) {
        auto& p = places[i]; if (!p.known) continue;
        int sx = f.ox + (int)((p.x + 0.5f) * f.sc), sy = f.oy + (int)((p.y + 0.5f) * f.sc);
        cv.fillRect(sx - 2, sy - 2, 5, 5, p.visited ? C::White : RGB(200, 200, 200)); cv.rect(sx - 2, sy - 2, 5, 5, C::Black);
        if ((int)i == wmSel) cv.rect(sx - 6, sy - 6, 13, 13, C::Yellow);
        if (pins.count(p.mapId) && blink) { cv.fillRect(sx - 1, sy - 12, 3, 8, C::Yellow); cv.fillRect(sx - 4, sy - 12, 9, 4, C::Yellow); }
        if (p.visited || (int)i == wmSel) { std::string nm = p.visited ? p.name : "?"; int tw = ctx.font->width(nm); int tx = clampi(sx - tw / 2, f.ox, std::max(f.ox, f.ox + fw - tw)); ctx.font->drawShadow(cv, *ctx.ts, tx, sy + 5, nm, (int)i == wmSel ? C::Yellow : RGB(40, 30, 10), RGB(255, 245, 220, 180)); }
    }
    // the party: on this map, or at the exit that leads to the map they are on
    { int hx = -1, hy = -1; if (mapId == wmSource) { hx = px; hy = py; } else for (auto& p : places) if (p.mapId == mapId) { hx = p.x; hy = p.y; }
      if (hx >= 0 && blink) { int sx = f.ox + (int)((hx + 0.5f) * f.sc), sy = f.oy + (int)((hy + 0.5f) * f.sc); cv.fillRect(sx - 2, sy - 2, 5, 5, C::Cyan); cv.rect(sx - 3, sy - 3, 7, 7, C::Black); } }
    int lx = 10 + 480 + 10, ly = 30 + 8, lw = SCREEN_W - lx - 8;
    UI::text(ctx, lx, 30 - 8, "Places", C::Cream);
    std::vector<int> known; for (size_t i = 0; i < places.size(); i++) if (places[i].known) known.push_back((int)i);
    if (known.empty()) { auto ls = ctx.font->wrap("Nothing charted yet. The map fills in as you explore; doorways and passages you find are listed here.", lw); for (size_t i = 0; i < ls.size(); i++) UI::text(ctx, lx, ly + (int)i * 13, ls[i], C::Gray); }
    for (size_t i = 0; i < known.size(); i++) {
        int ry = ly + (int)i * 14 - wmScroll * 14; if (ry < ly || ry > SCREEN_H - 90) continue;
        auto& p = places[known[i]]; bool sel = known[i] == wmSel;
        if (sel) cv.fillRect(lx - 2, ry - 1, lw, 14, RGB(120, 90, 40));
        UI::text(ctx, lx, ry, p.visited ? p.name : "? (unexplored)", sel ? C::Yellow : p.visited ? C::White : C::Gray);
        if (pins.count(p.mapId)) UI::text(ctx, lx + lw - 14, ry, "!", C::Yellow);
    }
    if (wmSel >= 0 && wmSel < (int)places.size()) {
        auto& p = places[wmSel]; int dy = SCREEN_H - 80;
        ctx.font->bold = true; UI::text(ctx, lx, dy, p.visited ? p.name : "Unexplored", C::Cream); ctx.font->bold = false;
        UI::text(ctx, lx, dy + 14, p.mapId == mapId ? "You are here" : p.visited ? "Visited" : "Not visited yet", C::LtGray);
        for (auto& q : quests) { std::string pin = questPin(q); if (pin.empty() || questClosed(q)) continue; bool match = pin == p.mapId; for (auto& l : locations) if (l.id == pin && l.mapId == p.mapId) match = true; if (match) { UI::text(ctx, lx, dy + 28, "! " + q.name, C::Yellow); break; } }
    }
    UI::text(ctx, 10, SCREEN_H - 24, "Quests: yellow  You: cyan  Esc: close", C::Cream);
}
