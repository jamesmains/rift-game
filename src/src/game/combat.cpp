// Turn-based tactical combat (X-COM / Fallout style) fought in place on the current map.
#include "game.h"
#include "../core/audio.h"
#include "tiles.h"
static std::string padShort(int k) { std::string n = keyName(k); return n.rfind("Pad ", 0) == 0 ? n.substr(4) : n.rfind("DPad ", 0) == 0 ? "D-" + n.substr(5) : n; }
#include <algorithm>
#include <cmath>

using namespace UI;

static const char* ACTIONS[] = {"Attack", "Aimed", "Spell", "Item", "Defend", "Overwatch", "End Turn", "Flee"};
static const int NACTIONS = 8;
static const int ACTION_KEYS[NACTIONS] = {A_ATTACKMODE, A_ATTACKMODE, A_SPELLS, A_ITEMS, A_DEFEND, A_OVERWATCH, A_ENDTURN, A_FLEE};
#define HOTBAR_Y (UI::SCREEN_H - 72 - 30)   // 1.2.2: the screen height can change (#39)
static const int TURN_STRIP_Y = 40;   // the turn-order strip on the right; its TURN header sits just under the top bar (1.1.3: was 30, under the Menu button)
// The action bar: each button carries the key that does the same thing, drawn like the key hints in shops and menus
// (a controller's button glyph when a pad is in use). Aimed is the attack key pressed twice, so it shows that key twice.
static int actionChipsW(Ctx& c, int i) { int k = keyChip(c, 0, 0, ACTION_KEYS[i], false); if (!k) return 0; return i == 1 ? k * 2 + 2 : k; }
static int actionButtonW(Ctx& c, int i) { int k = actionChipsW(c, i); return (k ? k + 5 : 0) + c.font->width(ACTIONS[i]) + 10; }
// the Menu button at the right end of the bar (1.1.3): opens the pause menu, like Esc
static int combatMenuButtonW(Ctx& c) { int k = keyChip(c, 0, 0, A_CANCEL, false); return (k ? k + 5 : 0) + c.font->width("Menu") + 10; }

// distance from a unit (1x1 or 2x2) to a tile
static int unitDist(const Combatant& u, int x, int y) {
    int d = chebyshev(u.x, u.y, x, y);
    if (u.ch.size == 2) for (int b = 0; b < 2; b++) for (int a = 0; a < 2; a++) d = std::min(d, chebyshev(u.x + a, u.y + b, x, y));
    return d;
}

int Game::combatUnitAt(int x, int y) const {
    for (size_t i = 0; i < cb.units.size(); i++) {
        auto& u = cb.units[i]; if (!u.ch.alive) continue;
        if (u.ch.size == 2) { if (x >= u.x && x <= u.x + 1 && y >= u.y && y <= u.y + 1) return (int)i; }
        else if (u.x == x && u.y == y) return (int)i;
    }
    return -1;
}

void Game::addFloat(int tx, int ty, const std::string& s, Color8 c, int scale) {
    FloatText f; f.x = (float)(tx * TILE + 12); f.y = (float)(ty * TILE - 6); f.vy = scale > 1 ? -34 : -22; f.life = scale > 1 ? 1.1f : 1.3f; f.text = s; f.color = c; f.scale = scale;
    // stack on top of floats already rising from the same tile so numbers never sit on each other
    for (auto& o : floats) if (fabsf(o.x - f.x) < 12 && fabsf(o.y - f.y) < 14 * scale) f.y = o.y - 13 * std::max(scale, o.scale);
    floats.push_back(f);
}

void Game::startCombat(const std::vector<std::string>& creatureIds, int cx, int cy, int levelBonus, const std::vector<int>& groupIndices, const std::string& onDefeat) {
    if (hasFlag("_fleegrace") && flags["_fleegrace"] > 0 && groupIndices.size() && creatureIds.size()) { /* grace period: still fight if walked into */ }
    syncBonds();   // Loyal / Strained companions (standing.cpp)
    cb = CombatState(); cb.units.reserve(128);   // summons and reinforcements push units mid-turn; keep references stable
    cb.defeatedGroups = groupIndices; cb.onDefeatScript = onDefeat;
    std::vector<std::pair<int, int>> taken;
    // party: everyone starts exactly where they stand on the map (leader at px,py, followers on their trail tiles);
    // only a member whose tile is blocked, or who is stacked on someone, is moved to the nearest free tile.
    int ld = activeLeader(); std::vector<int> forder = followOrder();
    for (size_t i = 0; i < party.members.size(); i++) {
        if (!party.members[i].alive) continue;
        Combatant u; u.ch = party.members[i]; u.partyIndex = (int)i;
        int ux = px, uy = py;
        if ((int)i != ld) {
            bool placed = false;
            int k = -1; for (size_t q = 0; q < forder.size(); q++) if (forder[q] == (int)i) k = (int)q;
            if (k >= 0 && k < (int)trail.size()) { auto tp = trail[k]; bool free = map->walkable(tp.first, tp.second) && !map->occupied(tp.first, tp.second); for (auto& t : taken) if (t == tp) free = false; if (free) { ux = tp.first; uy = tp.second; placed = true; } }
            for (auto& tp : trail) { if (placed) break; bool free = map->walkable(tp.first, tp.second) && !map->occupied(tp.first, tp.second); for (auto& t : taken) if (t == tp) free = false; if (free && chebyshev(tp.first, tp.second, cx, cy) > 1) { ux = tp.first; uy = tp.second; placed = true; break; } }
            for (int radius = 1; radius <= 6 && !placed; radius++) {
                std::vector<std::pair<int, int>> cand;
                for (int yy = py - radius; yy <= py + radius; yy++) for (int xx = px - radius; xx <= px + radius; xx++) {
                    if (std::max(abs(xx - px), abs(yy - py)) != radius) continue;
                    if (!map->walkable(xx, yy) || map->occupied(xx, yy)) continue;
                    bool free = true; for (auto& t : taken) if (t.first == xx && t.second == yy) free = false;
                    if (free) cand.push_back({xx, yy});
                }
                if (!cand.empty()) { auto p = rng.pick(cand); ux = p.first; uy = p.second; placed = true; }
            }
        }
        taken.push_back({ux, uy});
        u.x = ux; u.y = uy; u.ch.x = ux; u.ch.y = uy;
        u.ch.ap = u.ch.maxAP(*gd); u.ch.move = u.ch.maxMove(*gd);
        u.initiative = u.ch.stat(ST_DEX, *gd) + rng.range(1, 8);
        cb.units.push_back(u);
    }
    // enemies
    for (auto& id : creatureIds) {
        const CreatureDef* cd = gd->creature(id); if (!cd) { log("(unknown creature " + id + ")"); continue; }
        Combatant u; u.ch.initFromCreature(*gd, *cd, cd->boss ? levelBonus / 2 : levelBonus); u.partyIndex = -1; u.groupIndex = groupIndices.empty() ? -1 : groupIndices[0]; onSeen(id);
        int ux = cx, uy = cy;
        {
            std::vector<std::pair<int, int>> cand;
            for (int radius = 2; radius <= 8 && cand.empty(); radius += 2) {
                for (int yy = cy - radius; yy <= cy + radius; yy++) for (int xx = cx - radius; xx <= cx + radius; xx++) {
                    if (!map->walkable(xx, yy) || map->occupied(xx, yy) || (xx == px && yy == py)) continue;
                    bool free = true; for (auto& t : taken) if (chebyshev(t.first, t.second, xx, yy) < 1 + (u.ch.size == 2 ? 1 : 0)) free = false;
                    if (u.ch.size == 2) { for (int b = 0; b < 2 && free; b++) for (int a = 0; a < 2; a++) if (!map->walkable(xx + a, yy + b) || map->occupied(xx + a, yy + b) || (xx + a == px && yy + b == py)) free = false; }
                    if (free && chebyshev(xx, yy, px, py) >= 2) cand.push_back({xx, yy});
                }
            }
            if (!cand.empty()) { auto p = rng.pick(cand); ux = p.first; uy = p.second; }
        }
        taken.push_back({ux, uy}); if (u.ch.size == 2) { taken.push_back({ux + 1, uy}); taken.push_back({ux, uy + 1}); taken.push_back({ux + 1, uy + 1}); }
        u.x = ux; u.y = uy;
        u.ch.ap = u.ch.maxAP(*gd); u.ch.move = u.ch.maxMove(*gd);
        u.initiative = u.ch.stat(ST_DEX, *gd) + rng.range(1, 8);
        cb.units.push_back(u);
    }
    // hide the enemy groups on the map while fighting
    for (int g : groupIndices) if (g >= 0 && g < (int)map->enemies.size()) map->enemies[g].defeated = true;
    for (size_t i = 0; i < cb.units.size(); i++) cb.order.push_back((int)i);
    std::stable_sort(cb.order.begin(), cb.order.end(), [&](int a, int b) { return cb.units[a].initiative > cb.units[b].initiative; });
    // groups that noticed the party at the same moment fight from round one (each placed around its own position)
    { std::vector<int> extra = combatExtraGroups; combatExtraGroups.clear(); for (int g : extra) combatAddGroup(g, false); }
    applyBlessing();
    std::stable_sort(cb.order.begin(), cb.order.end(), [&](int a, int b) { return cb.units[a].initiative > cb.units[b].initiative; });
    cb.turnPos = -1; cb.round = 1; cb.lastRoundJoined = 1;
    log("Combat begins!");
    firstSightBanter();   // 1.5.0 (#95)
    shake = 0.2f;
    if (mode != GMode::COMBAT) pushMode(GMode::COMBAT);
    revealAround();
    // 1.4.0 (#78): a group's goal= (protect / reach / hold / close)
    for (int g : std::vector<int>(cb.defeatedGroups)) if (g >= 0 && g < (int)map->enemies.size()) goalSetup(map->enemies[g], g);
    // 1.4.0 (#80 #79): surprise, set by the caller - the party caught them unaware (1), or monsters that see in the dark
    // struck from unlit tiles at a party walking without a light (-1, Game::onStep)
    int sur = nextSurprise; nextSurprise = 0;
    cb.surprise = sur;
    if (sur > 0) { cb.log.push_back("Surprise! They haven't noticed you - your party acts first."); log("Surprise! Your party gets a free round."); }
    else if (sur < 0) { cb.log.push_back("Ambush from the dark! They strike before you can react."); log("Ambushed from the dark!"); gAudio.play("ambush"); }
    combatNextTurn();
    if (sur != 0) { cb.bannerText = sur > 0 ? "Surprise!" : "Ambushed!"; cb.banner = 1.5f; }
    if (!cb.goal.kind.empty()) notify("Goal: " + cb.goal.text);
}

// ---------------- reinforcements ----------------
// A fight is loud. At the start of every round, enemy groups on the map that can hear it (close by, or within their
// aggro range with a line of sight to anyone fighting) come running and join in where they stand.
bool Game::combatAddGroup(int gi, bool announce) {
    if (gi < 0 || gi >= (int)map->enemies.size()) return false;
    EnemyGroup& g = map->enemies[gi];
    if (g.defeated) return false;
    if (std::find(cb.defeatedGroups.begin(), cb.defeatedGroups.end(), gi) != cb.defeatedGroups.end()) return false;
    std::vector<std::string> names;
    for (auto& id : g.creatures) {
        const CreatureDef* cd = gd->creature(id); if (!cd) continue;
        Combatant u; u.ch.initFromCreature(*gd, *cd, cd->boss ? g.levelBonus / 2 : g.levelBonus); u.partyIndex = -1; u.groupIndex = gi; onSeen(id);
        int ux = -1, uy = -1;
        for (int r = 0; r <= 5 && ux < 0; r++) {
            std::vector<std::pair<int, int>> cand;
            for (int yy = g.y - r; yy <= g.y + r; yy++) for (int xx = g.x - r; xx <= g.x + r; xx++) {
                if (std::max(abs(xx - g.x), abs(yy - g.y)) != r) continue;
                bool ok = true;
                for (int b = 0; b < u.ch.size && ok; b++) for (int a = 0; a < u.ch.size; a++) { int qx = xx + a, qy = yy + b; if (!map->walkable(qx, qy) || combatUnitAt(qx, qy) >= 0 || map->chestAt(qx, qy) || map->npcAt(qx, qy)) ok = false; }
                if (ok) cand.push_back({xx, yy});
            }
            if (!cand.empty()) { auto p = rng.pick(cand); ux = p.first; uy = p.second; }
        }
        if (ux < 0) continue;
        u.x = ux; u.y = uy; u.ch.ap = u.ch.maxAP(*gd); u.ch.move = u.ch.maxMove(*gd); u.initiative = u.ch.stat(ST_DEX, *gd) + rng.range(1, 8);
        cb.units.push_back(u); cb.order.push_back((int)cb.units.size() - 1); names.push_back(u.ch.name);
    }
    cb.defeatedGroups.push_back(gi); g.defeated = true;   // hidden on the map while it fights
    if (!g.onDefeatScript.empty() && cb.onDefeatScript.empty()) cb.onDefeatScript = g.onDefeatScript;
    if (announce && !names.empty()) {
        std::string s = names[0]; if (names.size() > 1) s += fmt(" and %d more", (int)names.size() - 1);
        cb.log.push_back(s + " join the fight!"); log(s + (names.size() > 1 ? " join" : " joins") + " the fight!");
        notify("Reinforcements!"); gAudio.play("reinforcements");
    }
    return !names.empty();
}
bool Game::groupHasBoss(const EnemyGroup& g) const { for (auto& c : g.creatures) if (auto* d = gd->creature(c)) if (d->boss) return true; return false; }
void Game::combatJoinGroups() {
    for (size_t gi = 0; gi < map->enemies.size(); gi++) {
        EnemyGroup& g = map->enemies[gi];
        if (g.defeated) continue;
        if (!g.showFlag.empty() && !evalCond(g.showFlag)) continue;
        if (!g.hideFlag.empty() && evalCond(g.hideFlag)) continue;
        // the party is what they hear and see (not other monsters, or one fight would pull in the whole dungeon)
        bool hears = false;
        // a boss keeps to its lair: it only joins someone else's fight when the party is right on top of it (1.0: the
        // Lich used to march into the first fight of Dunmarrow and wipe parties that hadn't reached his room yet)
        bool boss = groupHasBoss(g);
        for (auto& u : cb.units) {
            if (!u.ch.alive || !u.friendly()) continue;
            int d = chebyshev(u.x, u.y, g.x, g.y);
            if (map->touchCombat ? d <= 1 : (d <= (boss ? 3 : 4) || (!boss && d <= g.aggro + 2 && combatLOS(u.x, u.y, g.x, g.y)))) { hears = true; break; }   // 1.2.3 (#40) touch=1: only a group touching a fighter joins
        }
        // linked groups (same link= id) always come together
        if (!hears && !g.link.empty()) for (int o : cb.defeatedGroups) if (o >= 0 && o < (int)map->enemies.size() && map->enemies[o].link == g.link) hears = true;
        if (hears) combatAddGroup((int)gi, true);
    }
}

void Game::combatComputeReachable() {
    cb.reachable.clear();
    if (cb.selectedUnit < 0) return;
    Combatant& u = cb.units[cb.selectedUnit];
    int mv = u.ch.move;
    std::vector<std::pair<int, int>> blocked;
    for (auto& o : cb.units) if (o.ch.alive && &o != &u) { blocked.push_back({o.x, o.y}); if (o.ch.size == 2) { blocked.push_back({o.x + 1, o.y}); blocked.push_back({o.x, o.y + 1}); blocked.push_back({o.x + 1, o.y + 1}); } }
    for (auto& n : map->npcs) if (!n.hidden) blocked.push_back({n.x, n.y});
    // BFS limited by mv
    std::vector<int> dist((size_t)map->w * map->h, -1);
    std::vector<std::pair<int, int>> q; q.push_back({u.x, u.y}); dist[(size_t)u.y * map->w + u.x] = 0;
    const int DX[4] = {1, -1, 0, 0}, DY[4] = {0, 0, 1, -1};
    // cheapest-first: rough ground (a tile kind's move cost) eats more movement points per step
    for (size_t h = 0; h < q.size(); h++) {
        size_t best = h; for (size_t j = h + 1; j < q.size(); j++) if (dist[(size_t)q[j].second * map->w + q[j].first] < dist[(size_t)q[best].second * map->w + q[best].first]) best = j;
        std::swap(q[h], q[best]);
        auto [x, y] = q[h]; int d = dist[(size_t)y * map->w + x];
        if (d >= mv) continue;
        for (int k = 0; k < 4; k++) {
            int nx = x + DX[k], ny = y + DY[k];
            if (!map->walkable(nx, ny)) continue;
            int nd = d + tileMoveCost(nx, ny); if (nd > mv) continue;
            int& cur = dist[(size_t)ny * map->w + nx]; if (cur >= 0 && cur <= nd) continue;
            bool blk = false; for (auto& b : blocked) if (b.first == nx && b.second == ny) { blk = true; break; }
            if (blk || map->chestAt(nx, ny)) continue;
            bool fresh = cur < 0; cur = nd; q.push_back({nx, ny}); if (fresh) cb.reachable.push_back({nx, ny});
        }
    }
    // 1.4.0 (#55): the cover each reachable tile would give against the foes who could see it there
    cb.reachCover.assign(cb.reachable.size(), 0);
    for (size_t i = 0; i < cb.reachable.size(); i++) {
        int x = cb.reachable[i].first, y = cb.reachable[i].second, all = 0;
        for (auto& o : cb.units) {
            if (!o.ch.alive || o.friendly() == u.friendly() || chebyshev(x, y, o.x, o.y) > 12) continue;
            if (map->dark && !visible(o.x, o.y) && u.friendly()) continue;   // you don't know about what you can't see
            if (!combatLOS(o.x, o.y, x, y)) continue;
            int sides = 0; coverVs(x, y, o.x, o.y, cb.selectedUnit, u.friendly(), &sides);
            for (int k = 0; k < 4; k++) { int v = (sides >> (k * 2)) & 3, have = (all >> (k * 2)) & 3; if (v > have) all = (all & ~(3 << (k * 2))) | (v << (k * 2)); }
            if (smokeAt(x, y)) all |= 0x100;
        }
        cb.reachCover[i] = all;
    }
}

void Game::combatNextTurn() {
    if (cb.ending) return;
    for (int guard = 0; guard < (int)cb.order.size() + 2; guard++) {
        cb.turnPos++;
        if (cb.turnPos >= (int)cb.order.size()) {
            cb.turnPos = 0; cb.round++; cb.bannerText = fmt("Round %d", cb.round); cb.banner = 1.0f;
            // nobody has struck a blow (or cast at anyone) for three rounds and nobody can see a foe: the fight breaks
            // off instead of both sides waiting on overwatch forever (a door or a long way round between them)
            if (cb.actionThisRound) cb.quietRounds = 0; else cb.quietRounds++;
            cb.actionThisRound = false;
            if (cb.quietRounds >= 3) {
                bool seen = false;
                for (auto& a : cb.units) if (a.ch.alive && a.friendly()) for (auto& b : cb.units) if (b.ch.alive && !b.friendly() && chebyshev(a.x, a.y, b.x, b.y) <= 8 && combatLOS(a.x, a.y, b.x, b.y)) seen = true;
                if (!seen) { cb.log.push_back("Neither side can reach the other. The fight breaks off."); log("The fight breaks off - the enemy has lost sight of you."); cb.ending = true; cb.fled = true; cb.disengaged = true; cb.endTimer = 0.4f; return; }
            }
            groundRoundTick();                        // 1.4.0: fire / ice / smoke burn down, fire spreads
            if (lightSteps > 0) { lightSteps = std::max(0, lightSteps - 3); if (lightSteps == 0 && map->dark) cb.log.push_back("Your torch burns out."); }   // (#79) torches burn in a fight too
            goalCheck(true);
            if (cb.ending) return;
            for (auto& l : cb.lights) l.turns--;
            cb.lights.erase(std::remove_if(cb.lights.begin(), cb.lights.end(), [](const CombatState::Light& l) { return l.turns <= 0; }), cb.lights.end());
            if (cb.round > cb.lastRoundJoined) { cb.lastRoundJoined = cb.round; combatJoinGroups(); }
        }
        int ui = cb.order[cb.turnPos];
        if (!cb.units[ui].ch.alive) continue;
        if (cb.round == 1 && cb.surprise != 0 && (cb.surprise > 0 ? !cb.units[ui].friendly() : cb.units[ui].friendly())) continue;   // 1.4.0 (#80) surprised: no first round
        cb.selectedUnit = ui;
        combatBeginTurn();
        return;
    }
}

void Game::combatBeginTurn() {
    { Combatant& bu = cb.units[cb.selectedUnit]; if (bu.partyIndex >= 0 && !autoCombat) { cb.bannerText = bu.ch.name + "'s turn"; cb.banner = 0.9f; } }
    Combatant& u = cb.units[cb.selectedUnit];
    applyStatusTick(u);
    if (u.ch.alive) groundHurt(cb.selectedUnit);   // 1.4.0 (#61): starting a turn in fire
    if (!u.ch.alive) { combatCheckEnd(); if (!cb.ending) combatNextTurn(); return; }
    // 1.4.0 (#63): a routed unit runs for one turn, then is gone - it surrenders if a foe is next to it
    if (u.ch.hasEffect("routed") && u.ch.effect("routed")->turns <= 2) {
        bool adj = false; for (auto& o : cb.units) if (o.ch.alive && o.friendly() != u.friendly() && chebyshev(o.x, o.y, u.x, u.y) <= 1) adj = true;
        unitGone(u, adj ? "surrenders" : "flees the field"); combatCheckEnd(); if (!cb.ending) combatNextTurn(); return;
    }
    if (u.rift) { combatEndTurn(); return; }   // (#78) a rift tear doesn't act (it calls enemies at the start of each round)
    if (u.summoned && u.summonTurns > 0 && --u.summonTurns == 0) { u.ch.alive = false; u.ch.hp = 0; cb.reachDirty = true; cb.log.push_back(u.ch.name + " fades away."); addFloat(u.x, u.y, "fades", C::Gray); combatCheckEnd(); if (!cb.ending) combatNextTurn(); return; }
    u.ch.ap = u.ch.maxAP(*gd); u.ch.move = u.ch.maxMove(*gd);
    u.ch.removeEffect("defend"); u.overwatch = false; u.aooDone = false;
    windupResolve(cb.selectedUnit);   // 1.4.0 (#62): an announced attack lands first
    if (!u.ch.alive) { combatCheckEnd(); if (!cb.ending) combatNextTurn(); return; }
    if (int cm = u.ch.fx("covermove", *gd)) { bool cov = false; for (auto& o : cb.units) if (o.ch.alive && o.friendly() != u.friendly() && coverVs(u.x, u.y, o.x, o.y, cb.selectedUnit, u.friendly()) > 0) cov = true; if (cov) u.ch.move += cm; }
    if (u.ch.hasEffect("stun")) { log(u.ch.name + " is stunned!"); u.ch.ap = 0; u.ch.move = 0; }
    cb.playerTurn = u.partyIndex >= 0; gAudio.play(cb.playerTurn ? "turn_player" : "turn_enemy");
    cb.mode = 0; cb.path.clear(); cb.aiPath.clear(); cb.aiAction = 0; cb.aiTimer = settings && settings->fastCombat ? 0.15f : 0.45f; cb.aiTarget = -1;
    cb.selX = u.x; cb.selY = u.y;
    if (u.partyIndex >= 0) { px = u.x; py = u.y; }
    revealAround();
    if (u.friendly() || visible(u.x, u.y)) { camFree = false; camCenterOn(u.x, u.y); }   // don't give away a monster lurking in the dark
    combatComputeReachable(); cb.reachDirty = false;
    if (u.ch.ap == 0 && u.ch.move == 0) { combatEndTurn(); }
}

void Game::combatEndTurn() {
    if (cb.ending) return;
    combatCheckEnd();
    if (!cb.ending) combatNextTurn();
}

void Game::applyStatusTick(Combatant& u) {
    for (size_t i = 0; i < u.ch.effects.size(); i++) {
        auto& e = u.ch.effects[i];
        if (e.id == "burn") { int d = std::max(1, e.power); applyDamage(u, d, DT_FIRE, "burning", false, nullptr); }
        else if (e.id == "poison") { int d = std::max(1, e.power); applyDamage(u, d, DT_POISON, "poison", false, nullptr); }
        else if (e.id == "regen") { int h = std::max(1, e.power); u.ch.hp = std::min(u.ch.maxHP(*gd), u.ch.hp + h); addFloat(u.x, u.y, "+" + std::to_string(h), C::Green); }
        if (!u.ch.alive) break;
        e.turns--;
        if (e.turns <= 0) {
            if (e.id == "overridden" && u.partyIndex < 0) { u.ally = false; cb.log.push_back(u.ch.name + " shakes off the override!"); addFloat(u.x, u.y - 1, "override ends", C::Gray); cb.reachDirty = true; }
            u.ch.effects.erase(u.ch.effects.begin() + i); i--;
        }
    }
}

int Game::hitChance(const Combatant& a, const Combatant& d, bool aimed) const {
    // 1.5.0 (#91): a lower base, so cover and position decide fights. Was 72 + DEX*2 (+5 melee): a DEX 9 warrior hit 95% in
    // melee and half cover barely mattered. Now about 70% in the open, ~45% into half cover, ~30% into full cover.
    int dist = chebyshev(a.x, a.y, d.x, d.y);
    int chance = 72 + a.ch.stat(ST_DEX, *gd) + a.ch.accuracy(*gd) - d.ch.evasion(*gd);
    if (!a.ch.meleeWeapon(*gd)) { if (a.ch.trait(*gd) != "steady") chance -= std::max(0, dist - 2) * 4; } else chance += 5;   // (#68) rifles: steady
    if (aimed) chance -= 10;
    if (a.ch.hasEffect("reaction") && a.ch.fx("noowpenalty", *gd) <= 0) chance -= 10;
    if (d.ch.size == 2) chance += 15;
    // cover: half -25, full -40 (an aimed shot halves it); (#59) defending in cover doubles its penalty, defending in the open is -20
    int cov = effCover(a, d); int pen = cov == 1 ? (aimed ? 12 : 25) : cov == 2 ? (aimed ? 20 : 40) : 0;
    if (d.ch.hasEffect("defend")) chance -= pen > 0 ? pen * 2 : 20; else chance -= pen;
    // flanked / exposed: the best of the two (+20), they don't stack
    bool fl = flanked(a, d), ex = exposed(a, d);
    if (fl || ex) chance += 20 + (ex ? a.ch.fx("exposedhit", *gd) : 0);   // (#56)
    if (marked(a, d)) chance += 15;                                       // (#65)
    chance += a.ch.fx("hit", *gd);
    chance -= a.ch.gearHitPenalty(*gd);                                     // 1.5.0 (#93): off-class weapon, unmet requirements
    chance -= a.ch.woundHitPenalty();                                     // 1.5.0 (#94)
    chance -= darknessPenalty(a, d);
    return clampi(chance, 5, 95);
}
// 1.5.0 (#83): critical chance for a weapon attack. Was 5 + DEX/2 + weapon + aimed 30 + flanked 15 + exposed 15 + marked 15
// (all stacking, no cap): 35-80% was normal. Now 3 + DEX/4 + weapon crit=, +15 aimed, the best ONE of flanked / exposed /
// marked +10, perks; capped at CRIT_CAP. A tagged target (EXPLOIT) is still a sure crit, outside the cap.
int Game::critChance(const Combatant& a, const Combatant& t, bool aimed) const {
    const ItemDef* w = a.ch.weapon(*gd);
    bool fl = flanked(a, t), ex = exposed(a, t), mk = marked(a, t);
    std::string trait = a.ch.trait(*gd);
    int c = 3 + a.ch.stat(ST_DEX, *gd) / 4 + (w ? w->crit : 0) + (aimed ? 15 : 0) + ((fl || ex || mk) ? 10 : 0);
    if (fl || ex) c += a.ch.fx("flankcrit", *gd) + (trait == "backstab" ? 12 : 0);   // (#67 #68)
    if (t.ch.mechanical) c += a.ch.fx("machinecrit", *gd);
    c += a.ch.fx("crit", *gd);
    return clampi(c, 0, CRIT_CAP);
}
// Attacking someone who stands in the dark is hard - unless you see in the dark (undead, machines, cave dwellers).
int Game::darknessPenalty(const Combatant& a, const Combatant& d) const {
    if (!map || !map->dark || a.ch.darkvision) return 0;
    if (lit(d.x, d.y)) return 0;
    return chebyshev(a.x, a.y, d.x, d.y) <= 1 ? 10 : 25;
}
// Cover: an obstacle on the defender's side facing the attacker. Sight-blocking = full, merely impassable (table, rocks) = half.
int Game::coverAt(const Combatant& a, const Combatant& d) const {
    return coverVs(d.x, d.y, a.x, a.y, (int)(&d - &cb.units[0]), d.friendly());
}
// Flanked: the attacker is adjacent and so is another unit of the attacker's side.
bool Game::flanked(const Combatant& a, const Combatant& d) const {
    if (chebyshev(a.x, a.y, d.x, d.y) > 1) return false;
    for (auto& o : cb.units) if (&o != &a && o.ch.alive && o.friendly() == a.friendly() && chebyshev(o.x, o.y, d.x, d.y) <= 1) return true;
    return false;
}
std::string Game::hitBreakdown(const Combatant& a, const Combatant& d, bool aimed) const {
    std::string s; bool arced = false; int raw = coverAt(a, d), cov = effCover(a, d, &arced);
    if (cov == 1) s += " half cover"; else if (cov == 2) s += " full cover";
    else if (arced) s += " arcs over cover"; else if (raw == 1) s += " ignores cover";
    if (smokeAt(d.x, d.y)) s += " in smoke";
    if (flanked(a, d)) s += " flanked";
    if (exposed(a, d)) s += " exposed";
    if (marked(a, d)) s += " marked";
    if (d.ch.hasEffect("defend")) s += cov > 0 ? " hunkered" : " defending";
    if (darknessPenalty(a, d) > 0) s += " in darkness";
    (void)aimed; return s;
}
// ================= 1.4.0: cover, exposure, smoke, ground effects, spell shapes, combos =================
static std::string prettyName(const std::string& n) { std::string s = n; while (!s.empty() && isdigit((unsigned char)s.back())) s.pop_back(); for (auto& ch : s) if (ch == '_') ch = ' '; if (startsWith(s, "table ")) s = "table"; return s; }
// Line of sight in a fight: walls as always, and smoke between the two tiles (not on them) blocks it too.
bool Game::combatLOS(int x0, int y0, int x1, int y1) const {
    if (!lineOfSight(*map, x0, y0, x1, y1)) return false;
    if (cb.groundFx.empty()) return true;
    int n = std::max(abs(x1 - x0), abs(y1 - y0)); if (n <= 1) return true;
    for (int i = 1; i < n * 2; i++) {
        float t = i / (float)(n * 2); int x = (int)floorf(x0 + (x1 - x0) * t + 0.5f), y = (int)floorf(y0 + (y1 - y0) * t + 0.5f);
        if ((x == x0 && y == y0) || (x == x1 && y == y1)) continue;
        if (smokeAt(x, y)) return false;
    }
    return true;
}
int Game::groundAt(int x, int y, int kind) const { for (auto& g : cb.groundFx) if (g.x == x && g.y == y && g.kind == kind) return g.turns; return 0; }
void Game::addGround(int x, int y, int kind, int turns) {
    if (!map->inBounds(x, y) || turns <= 0) return;
    if (kind == 1) { const Cell& c = map->at(x, y); if (c.water || (!c.walkable && !allTileKinds()[c.kind].burns)) return; }   // fire needs something to burn on
    if (kind == 2 && !map->at(x, y).walkable && !map->at(x, y).water) return;
    if (kind == 1) cb.groundFx.erase(std::remove_if(cb.groundFx.begin(), cb.groundFx.end(), [&](const CombatState::GroundFx& g) { return g.x == x && g.y == y && g.kind == 2; }), cb.groundFx.end());   // fire melts ice
    if (kind == 2) cb.groundFx.erase(std::remove_if(cb.groundFx.begin(), cb.groundFx.end(), [&](const CombatState::GroundFx& g) { return g.x == x && g.y == y && g.kind == 1; }), cb.groundFx.end());   // and ice puts out fire
    for (auto& g : cb.groundFx) if (g.x == x && g.y == y && g.kind == kind) { g.turns = std::max(g.turns, turns); return; }
    cb.groundFx.push_back({x, y, kind, turns});
    if (kind == 1 && map->dark) cb.lights.push_back({x, y, 2, turns});
}
// Every new round: fire, ice and smoke burn down; fire spreads to flammable ground and catches wooden cover next to it.
void Game::groundRoundTick() {
    std::vector<CombatState::GroundFx> spread;
    const int DX[4] = {1, -1, 0, 0}, DY[4] = {0, 0, 1, -1};
    for (auto& g : cb.groundFx) {
        if (g.kind != 1 || g.turns < 2) continue;
        for (int k = 0; k < 4; k++) {
            int nx = g.x + DX[k], ny = g.y + DY[k]; if (!map->inBounds(nx, ny) || groundAt(nx, ny, 1)) continue;
            const TileKind& tk = allTileKinds()[map->at(nx, ny).kind];
            if (tk.flammable && map->at(nx, ny).walkable && rng.chance(35)) spread.push_back({nx, ny, 1, 2});
            else if (tk.burns && !tk.breaks.empty() && rng.chance(25)) { std::string nm = prettyName(tk.name); changeCell(nx, ny, tk.breaks); spread.push_back({nx, ny, 1, 2}); cb.log.push_back("The " + nm + " catches fire and burns away."); }
        }
    }
    for (auto& g : cb.groundFx) g.turns--;
    cb.groundFx.erase(std::remove_if(cb.groundFx.begin(), cb.groundFx.end(), [](const CombatState::GroundFx& g) { return g.turns <= 0; }), cb.groundFx.end());
    for (auto& s : spread) addGround(s.x, s.y, s.kind, s.turns);
    if (!spread.empty()) cb.reachDirty = true;
}
// Standing in fire (walking into it or starting a turn there) burns.
void Game::groundHurt(int ui) {
    Combatant& u = cb.units[ui]; if (!u.ch.alive) return;
    if (!groundAt(u.x, u.y, 1)) return;
    addFloat(u.x, u.y - 1, "fire!", C::Orange);
    cb.log.push_back(u.ch.name + " is caught in the flames!");
    applyDamage(u, rng.range(3, 6), DT_FIRE, "fire", false, nullptr);
    if (u.ch.alive) u.ch.addEffect("burn", 2, 3);
}
// Cover a unit at (x,y) gets against a shot from (fx,fy): the sides facing the shooter (sight-blocking = full,
// impassable = half), smoke on its tile (full), or a guard / shield-bearer of its own side next to it (at least half).
// sides (optional) gets 2 bits per side N E S W with the level each side gives.
int Game::coverVs(int x, int y, int fx, int fy, int self, bool friendlySide, int* sides) const {
    if (sides) *sides = 0;
    if (chebyshev(x, y, fx, fy) <= 1) return 0;   // point blank: no cover
    int dx = (fx > x) - (fx < x), dy = (fy > y) - (fy < y), best = 0;
    auto check = [&](int cx, int cy, int side) {
        if (!map->inBounds(cx, cy)) return; int o = combatUnitAt(cx, cy); if (o >= 0 && o != self) return;
        const Cell& c = map->at(cx, cy); const TileKind& k = allTileKinds()[c.kind];
        int v = c.blocksSight ? 2 : (!c.walkable && !k.water) ? 1 : 0;
        if (v) { best = std::max(best, v); if (sides) *sides |= v << (side * 2); }
    };
    if (dx) check(x + dx, y, dx > 0 ? 1 : 3);
    if (dy) check(x, y + dy, dy > 0 ? 2 : 0);
    if (smokeAt(x, y)) best = 2;
    if (best < 1) for (size_t i = 0; i < cb.units.size(); i++) { const Combatant& o = cb.units[i];
        if ((int)i == self || !o.ch.alive || o.friendly() != friendlySide || chebyshev(o.x, o.y, x, y) > 1) continue;
        if (o.ch.fx("guard", *gd) > 0 || (o.ch.fx("shieldcover", *gd) > 0 && o.ch.hasShield(*gd))) { best = 1; break; } }
    return best;
}
// The cover that counts for this attacker: rangers and some gear ignore half cover; bows (trait arc) shoot over it for
// less damage (arced = true).
int Game::effCover(const Combatant& a, const Combatant& d, bool* arced) const {
    if (arced) *arced = false;
    int self = (int)(&d - &cb.units[0]);
    int cov = coverVs(d.x, d.y, a.x, a.y, self, d.friendly());
    if (cov == 1 && a.ch.fx("ignorehalfcover", *gd) > 0) return 0;
    if (cov == 1 && a.ch.trait(*gd) == "arc" && unitDist(d, a.x, a.y) > 1) { if (arced) *arced = true; return 0; }
    return cov;
}
// Exposed: the target stands next to cover, but none of it faces this attacker (a ranged shot from the open side).
bool Game::exposed(const Combatant& a, const Combatant& d) const {
    if (unitDist(d, a.x, a.y) <= 1 || d.ch.size == 2 || smokeAt(d.x, d.y)) return false;
    int self = (int)(&d - &cb.units[0]);
    if (coverVs(d.x, d.y, a.x, a.y, self, d.friendly()) > 0) return false;
    static const int DX[4] = {0, 1, 0, -1}, DY[4] = {-1, 0, 1, 0};
    for (int k = 0; k < 4; k++) {
        int cx = d.x + DX[k], cy = d.y + DY[k]; if (!map->inBounds(cx, cy) || combatUnitAt(cx, cy) >= 0) continue;
        const Cell& c = map->at(cx, cy); if (c.blocksSight || (!c.walkable && !allTileKinds()[c.kind].water)) return true;
    }
    return false;
}
// Can a be aimed at d at all: a line of sight, and someone in smoke can only be hit from right next to them.
bool Game::targetable(const Combatant& a, const Combatant& d) const {
    if (smokeAt(d.x, d.y) && unitDist(d, a.x, a.y) > 1) return false;
    return combatLOS(a.x, a.y, d.x, d.y);
}
bool Game::marked(const Combatant& a, const Combatant& d) const { return d.ch.hasEffect("marked") && a.friendly() != d.friendly(); }

// (#66) the spell as this caster casts it: upgrade= lines for the ranks the caster has in the spell's class
SpellDef Game::castSpell(const Character& c, const SpellDef& sp) const {
    SpellDef o = sp;
    if (sp.upgrades.empty() || sp.cls.empty()) return o;
    int r = c.rank(sp.cls);
    for (auto& u : sp.upgrades) if (r >= u.rank) for (auto& kv : u.kv) applySpellKey(o, kv.first, kv.second);
    o.upgrades.clear();
    return o;
}
bool Game::singleTargetSpell(const SpellDef& sp) const { return sp.dmgMax > 0 && sp.area == 0 && sp.shape.empty() && sp.chainJumps == 0 && sp.target != "ally" && sp.target != "self"; }
// (#57) single-target damage spells lose a quarter of their damage against half cover and half against full cover
int Game::spellCoverPct(const Combatant& c, const Combatant& t, const SpellDef& sp) const {
    if (!singleTargetSpell(sp) || t.friendly() == c.friendly()) return 0;
    int self = (int)(&t - &cb.units[0]);
    int cov = coverVs(t.x, t.y, c.x, c.y, self, t.friendly());
    if (cov == 1 && c.ch.fx("ignorehalfcover", *gd) > 0) cov = 0;
    return cov == 2 ? 50 : cov == 1 ? 25 : 0;
}
// (#64) the tiles a spell covers: ball = area radius round the target; line = from the caster through the target, range
// long, stopped by walls; cone = from the caster toward the target, widening with distance
std::vector<std::pair<int, int>> Game::spellTiles(int ci, const SpellDef& sp, int tx, int ty) const {
    std::vector<std::pair<int, int>> out; const Combatant& c = cb.units[ci];
    auto add = [&](int x, int y) { if (!map->inBounds(x, y)) return; for (auto& p : out) if (p.first == x && p.second == y) return; out.push_back({x, y}); };
    if ((sp.shape == "line" || sp.shape == "cone") && !(tx == c.x && ty == c.y)) {
        float dx = (float)(tx - c.x), dy = (float)(ty - c.y), len = sqrtf(dx * dx + dy * dy); dx /= len; dy /= len;
        if (sp.shape == "line") {
            for (float t = 0.5f; t <= sp.range + 0.25f; t += 0.5f) {
                int x = (int)floorf(c.x + dx * t + 0.5f), y = (int)floorf(c.y + dy * t + 0.5f);
                if (x == c.x && y == c.y) continue;
                if (!map->inBounds(x, y) || map->at(x, y).blocksSight) break;
                add(x, y);
            }
        } else {
            int R = std::max(1, sp.range);
            for (int y = c.y - R; y <= c.y + R; y++) for (int x = c.x - R; x <= c.x + R; x++) {
                float vx = (float)(x - c.x), vy = (float)(y - c.y); float along = vx * dx + vy * dy, perp = fabsf(vx * dy - vy * dx);
                if (along < 0.5f || along > R + 0.5f || perp > along * 0.5f + 0.35f) continue;
                if (!map->inBounds(x, y) || map->at(x, y).blocksSight || !combatLOS(c.x, c.y, x, y)) continue;
                add(x, y);
            }
        }
        return out;
    }
    for (int y = ty - sp.area; y <= ty + sp.area; y++) for (int x = tx - sp.area; x <= tx + sp.area; x++) add(x, y);
    return out;
}
// (#64) chain=jumps:range:falloff - the first target, then each jump to the nearest living unit (either side) within
// range that hasn't been hit, in sight; standing in conducting water lets it jump two tiles farther
std::vector<int> Game::chainTargets(int ci, const SpellDef& sp, int first) const {
    std::vector<int> hit; if (first < 0) return hit;
    hit.push_back(first);
    for (int j = 0; j < sp.chainJumps; j++) {
        const Combatant& from = cb.units[hit.back()];
        bool wet = map->inBounds(from.x, from.y) && allTileKinds()[map->at(from.x, from.y).kind].conduct;
        int best = -1, bd = 999;
        for (size_t i = 0; i < cb.units.size(); i++) {
            const Combatant& o = cb.units[i];
            if (!o.ch.alive || (int)i == ci || std::find(hit.begin(), hit.end(), (int)i) != hit.end()) continue;
            int d = unitDist(o, from.x, from.y); bool owet = allTileKinds()[map->at(o.x, o.y).kind].conduct;
            int range = sp.chainRange + (wet && owet ? 2 : 0);
            if (d > range || !combatLOS(from.x, from.y, o.x, o.y)) continue;
            int score = d * 10 + (o.friendly() == cb.units[ci].friendly() ? 5 : 0);   // it prefers foes when they are as close
            if (score < bd) { bd = score; best = (int)i; }
        }
        if (best < 0) break;
        hit.push_back(best);
    }
    return hit;
}

// ---- (#60 #65) status combos: one table, so they can later come from data ----
// status on the target + incoming damage type -> named reaction. bonus = extra % of the hit; burst = extra damage of
// burstType; clears = statuses it ends; adds/addTurns = a status it leaves.
struct ComboRule { const char* status; int dmgType; const char* name; int bonus; int burst; int burstType; const char* clears; const char* adds; int addTurns; const char* text; };
static const ComboRule kCombos[] = {
    {"freeze",  DT_SHOCK, "SHATTER", 60, 0, -1,      "freeze,chilled", "",      0, "the ice shatters"},
    {"chilled", DT_SHOCK, "SHATTER", 40, 0, -1,      "chilled",        "",      0, "the frost on them shatters"},
    {"freeze",  DT_FIRE,  "THAW",     0, 8, DT_FIRE, "freeze,chilled", "",      0, "the ice bursts into steam"},
    {"poison",  DT_FIRE,  "FLARE",    0, -1, DT_POISON, "poison",      "",      0, "the poison in them flares up all at once"},
    {"burn",    DT_COLD,  "STEAM",    0, 0, -1,      "burn",           "blind", 2, "a cloud of scalding steam blinds them"},
};
std::vector<std::string> comboHelpLines() {
    std::vector<std::string> out;
    auto word = [](const std::string& s) { return s == "freeze" ? std::string("frozen") : s == "burn" ? std::string("burning") : s == "poison" ? std::string("poisoned") : s; };
    for (auto& r : kCombos) {
        std::string s = std::string(r.name) + ": " + dmgTypeName(r.dmgType) + " on a " + word(r.status) + " target - " + r.text;
        if (r.bonus) s += fmt(" (+%d%% damage)", r.bonus); if (r.burst > 0) s += " (an extra burst of damage)"; if (r.burst < 0) s += " (all of it at once)";
        out.push_back(s + ".");
    }
    return out;
}
int Game::applyCombos(Combatant& t, int dmg, int dmgType, Combatant* attacker) {
    if (!attacker || !t.ch.alive) return dmg;
    for (auto& r : kCombos) {
        if (r.dmgType != dmgType) continue;
        StatusEffect* e = t.ch.effect(r.status); if (!e) continue;
        int power = 100 + attacker->ch.fx("combopower", *gd);
        int extra = dmg * r.bonus / 100 * power / 100, burst = 0;
        if (r.burst > 0) burst = (r.burst + dmg / 2) * power / 100;
        else if (r.burst < 0) burst = std::max(1, e->power) * std::max(1, e->turns) * power / 100;   // FLARE: what the poison had left
        for (auto& c : split(r.clears, ',')) if (!c.empty()) t.ch.removeEffect(c);
        if (r.adds[0]) t.ch.addEffect(r.adds, r.addTurns, 0);
        addFloat(t.x, t.y - 1, r.name, C::Cyan, 2);
        std::string msg = std::string(r.name) + "! " + std::string(1, (char)toupper(r.text[0])) + (r.text + 1) + " (" + t.ch.name + ")";
        cb.log.push_back(msg); log(msg);
        dmg += extra;
        if (burst > 0) { int bt = r.burstType >= 0 ? r.burstType : dmgType; Combatant* keep = attacker; (void)keep; applyDamage(t, burst, bt, r.name, false, nullptr); }
        break;   // one reaction per hit
    }
    return dmg;
}
std::string Game::comboPreview(const Combatant& t, int dmgType) const {
    for (auto& r : kCombos) if (r.dmgType == dmgType && t.ch.hasEffect(r.status)) return r.name;
    return "";
}

// ---- (#58) cover that changes: broken / burnt furniture, snuffed and lit braziers ----
void Game::changeCell(int x, int y, const std::string& kind) {
    if (!map->inBounds(x, y) || !tileKind(kind)) return;
    bool have = false; for (auto& f : fightChanges) if (f.map == mapId && f.x == x && f.y == y) have = true;
    if (!have) fightChanges.push_back({mapId, x, y, allTileKinds()[map->at(x, y).kind].name});
    bool explored = map->at(x, y).explored;
    map->set(x, y, kind, rng); map->at(x, y).explored = explored;
    cb.reachDirty = true; computeLight(); revealAround();
}
void Game::revertFightChanges() {
    for (auto& f : fightChanges) { auto it = maps.find(f.map); if (it == maps.end() || !it->second) continue; Map& m = *it->second; if (!m.inBounds(f.x, f.y)) continue; bool ex = m.at(f.x, f.y).explored; m.set(f.x, f.y, f.kind, rng); m.at(f.x, f.y).explored = ex; }
    fightChanges.clear();
}
// area damage on the tiles: furniture with breaks= soaks it until breakhp (default 10), then breaks; burns=1 kinds go up
// in flames at the first fire damage
void Game::damageTiles(const std::vector<std::pair<int, int>>& tiles, int dmg, int dmgType) {
    for (auto& p : tiles) {
        if (!map->inBounds(p.first, p.second)) continue;
        const TileKind& k = allTileKinds()[map->at(p.first, p.second).kind];
        if (k.breaks.empty() || !tileKind(k.breaks)) continue;
        int idx = p.second * map->w + p.first; std::string nm = prettyName(k.name);
        bool burnt = dmgType == DT_FIRE && k.burns;
        if (!burnt) { cb.tileDmg[idx] += dmg; if (cb.tileDmg[idx] < (k.breakHp > 0 ? k.breakHp : 10)) continue; }
        std::string to = k.breaks;
        changeCell(p.first, p.second, to); cb.tileDmg.erase(idx);
        addFloat(p.first, p.second, burnt ? "burns!" : "smashed!", C::Orange);
        cb.log.push_back(burnt ? "The " + nm + " goes up in flames!" : "The " + nm + " is blown apart!");
        if (burnt) addGround(p.first, p.second, 1, 3);
        gAudio.play("explosion");
    }
}
// (#61) shock in water: everyone standing in the same body of conducting water (within 4 tiles) gets half of it
void Game::conductShock(int x, int y, int dmg, Combatant* src, const std::vector<int>& already) {
    if (!map->inBounds(x, y) || !allTileKinds()[map->at(x, y).kind].conduct) return;
    std::vector<std::pair<int, int>> q{{x, y}}; std::set<int> seen{y * map->w + x};
    for (size_t h = 0; h < q.size(); h++) {
        const int DX[4] = {1, -1, 0, 0}, DY[4] = {0, 0, 1, -1};
        for (int k = 0; k < 4; k++) { int nx = q[h].first + DX[k], ny = q[h].second + DY[k];
            if (!map->inBounds(nx, ny) || chebyshev(nx, ny, x, y) > 4 || seen.count(ny * map->w + nx) || !allTileKinds()[map->at(nx, ny).kind].conduct) continue;
            seen.insert(ny * map->w + nx); q.push_back({nx, ny}); }
    }
    int n = 0;
    for (size_t i = 0; i < cb.units.size(); i++) {
        Combatant& o = cb.units[i]; if (!o.ch.alive || std::find(already.begin(), already.end(), (int)i) != already.end()) continue;
        if (!seen.count(o.y * map->w + o.x)) continue;
        addFloat(o.x, o.y - 1, "shocked!", C::Cyan); applyDamage(o, std::max(1, dmg / 2), DT_SHOCK, "water", false, nullptr); n++;
    }
    if (n) { std::string m = fmt("The water carries the shock to %d more!", n); cb.log.push_back(m); log(m); }
    (void)src;
}
// (#58) Shield Bash and other knockback=1 spells: the target is shoved one tile away from the caster if there is room
void Game::knockBack(int ci, int ti) {
    Combatant& c = cb.units[ci]; Combatant& t = cb.units[ti];
    if (!t.ch.alive || t.ch.size == 2 || t.ch.boss || t.rift) { if (t.ch.alive) addFloat(t.x, t.y - 1, "holds firm", C::Gray); return; }
    int dx = (t.x > c.x) - (t.x < c.x), dy = (t.y > c.y) - (t.y < c.y); int nx = t.x + dx, ny = t.y + dy;
    if (!map->walkable(nx, ny) || combatUnitAt(nx, ny) >= 0 || map->chestAt(nx, ny) || map->npcAt(nx, ny)) { addFloat(t.x, t.y - 1, "braced", C::Gray); return; }
    bool hadCover = false; for (auto& o : cb.units) if (o.ch.alive && o.friendly() != t.friendly() && coverVs(t.x, t.y, o.x, o.y, ti, t.friendly()) > 0) hadCover = true;
    t.x = nx; t.y = ny; t.offX = -dx * 10.0f; t.offY = -dy * 10.0f;
    bool still = false; for (auto& o : cb.units) if (o.ch.alive && o.friendly() != t.friendly() && coverVs(t.x, t.y, o.x, o.y, ti, t.friendly()) > 0) still = true;
    addFloat(t.x, t.y - 1, hadCover && !still ? "out of cover!" : "knocked back", C::Orange);
    cb.log.push_back(t.ch.name + " is knocked back" + (hadCover && !still ? " out of cover!" : "."));
    cb.reachDirty = true; groundHurt(ti);
}

// ---- (#62) boss telegraphs: windup=spell announces a big attack a turn ahead on marked tiles ----
void Game::windupStart(int ui, int ti) {
    Combatant& u = cb.units[ui]; const SpellDef* sp = gd->spell(u.ch.windup); if (!sp) return;
    Combatant& t = cb.units[ti];
    SpellDef s = *sp; if (s.area == 0 && s.shape.empty()) s.area = 1;
    CombatState::Windup w; w.unit = ui; w.spell = sp->id; w.tiles = spellTiles(ui, s, t.x, t.y);
    cb.windups.push_back(w); u.lastWindup = cb.round; u.ch.ap = 0;
    cb.bannerText = u.ch.name + ": " + sp->name + "!"; cb.banner = 1.4f;
    addFloat(u.x, u.y - 1, sp->name + "!", C::Red, 2);
    std::string m = u.ch.name + " gathers power for " + sp->name + " - it lands on the marked tiles at the start of its next turn. Get clear!";
    cb.log.push_back(m); log(m); gAudio.play("spell_cast");
    shake = std::max(shake, 0.1f);
}
bool Game::windupResolve(int ui) {
    bool any = false;
    for (size_t k = 0; k < cb.windups.size(); k++) {
        if (cb.windups[k].unit != ui) continue;
        CombatState::Windup w = cb.windups[k]; cb.windups.erase(cb.windups.begin() + k); k--;
        Combatant& u = cb.units[ui]; const SpellDef* sp = gd->spell(w.spell); if (!sp || !u.ch.alive) continue;
        if (u.ch.hasEffect("stun") || u.ch.hasEffect("freeze")) { cb.log.push_back("The power " + u.ch.name + " gathered fizzles out."); addFloat(u.x, u.y - 1, "fizzles", C::Gray); continue; }
        any = true; int power = u.ch.stat(sp->scaleStat, *gd);
        cb.log.push_back(sp->name + " lands!"); gAudio.play(spellSound(*sp)); shake = std::max(shake, 0.25f);
        for (auto& p : w.tiles) { CombatAnim an; an.kind = 1; an.x0 = an.x1 = p.first; an.y0 = an.y1 = p.second; an.color = dmgColor(sp->dmgType); an.dur = 0.4f; cb.anims.push_back(an); }
        for (size_t i = 0; i < cb.units.size(); i++) {
            Combatant& o = cb.units[i]; if (!o.ch.alive || (int)i == ui) continue;
            bool in = false; for (auto& p : w.tiles) for (int b = 0; b < o.ch.size; b++) for (int a = 0; a < o.ch.size; a++) if (o.x + a == p.first && o.y + b == p.second) in = true;
            if (!in) continue;
            if (sp->dmgMax > 0) applyDamage(o, std::max(1, rng.range(sp->dmgMin, sp->dmgMax) + (power - 8) / 2), sp->dmgType, sp->name, false, &cb.units[ui]);
            if (o.ch.alive && !sp->effect.empty() && !(sp->effect == "stun" && o.ch.boss)) o.ch.addEffect(sp->effect, sp->duration > 0 ? sp->duration : 2, 3 + power / 3);
        }
        if (sp->dmgMax > 0) damageTiles(w.tiles, sp->dmgMax, sp->dmgType);
        if (sp->dmgType == DT_FIRE) for (auto& p : w.tiles) if (allTileKinds()[map->at(p.first, p.second).kind].flammable) addGround(p.first, p.second, 1, 2);
    }
    return any;
}

// ---- (#63) morale: when a group's leader (role=leader) falls, the rest break and run ----
void Game::leaderFell(const Combatant& leader) {
    int n = 0;   // (a random encounter's units all have group -1: they count as one band)
    for (auto& o : cb.units) if (o.ch.alive && !o.friendly() && o.groupIndex == leader.groupIndex && !o.ch.boss && !o.summoned && !o.rift && !o.ch.mechanical) { o.ch.addEffect("routed", 3, 0); o.ch.ai = "coward"; n++; }
    if (!n) return;
    std::string m = "With " + leader.ch.name + " down, the rest break and run!";
    cb.log.push_back(m); log(m); cb.bannerText = "They break!"; cb.banner = 1.0f;
}
// a unit leaves the fight without a body (fled, surrendered, a closed rift): beaten, with its XP and gold but no loot
void Game::unitGone(Combatant& u, const std::string& why) {
    if (!u.ch.alive) return;
    u.ch.alive = false; u.ch.hp = 0; u.gone = true; cb.reachDirty = true;
    addFloat(u.x, u.y - 1, why, C::LtGray);
    cb.log.push_back(u.ch.name + " " + why + "."); log(u.ch.name + " " + why + ".");
    if (!u.friendly() && !u.summoned) { cb.xpGained += u.ch.xpReward; int lo = 0, hi = 0; parseRange(u.ch.lootGold, lo, hi); cb.goldGained += rng.range(lo, hi); }
    for (size_t k = 0; k < cb.windups.size(); k++) if (&cb.units[cb.windups[k].unit] == &u) { cb.windups.erase(cb.windups.begin() + k); k--; }
}

// ---- (#78) fight goals ----
void Game::goalSetup(const EnemyGroup& g, int gi) {
    if (g.goal.empty() || !cb.goal.kind.empty()) return;
    auto p = split(g.goal, ':'); CombatState::Goal& G = cb.goal;
    G.kind = trim(p[0]); G.arg = p.size() > 1 ? trim(p[1]) : ""; G.onWin = g.onGoal; G.onFail = g.onGoalFail; G.text = g.goalText; G.group = gi;
    if (G.kind == "protect") {
        const CreatureDef* cd = gd->creature(G.arg);
        if (!cd) { G.kind.clear(); return; }
        Combatant u; u.ch.initFromCreature(*gd, *cd, 0); u.partyIndex = -1; u.ally = true; u.groupIndex = -1;
        int ux = -1, uy = -1;
        for (int r = 1; r <= 4 && ux < 0; r++) for (int yy = py - r; yy <= py + r && ux < 0; yy++) for (int xx = px - r; xx <= px + r; xx++) if (map->walkable(xx, yy) && combatUnitAt(xx, yy) < 0 && !map->chestAt(xx, yy) && !map->npcAt(xx, yy)) { ux = xx; uy = yy; break; }
        if (ux < 0) { G.kind.clear(); return; }
        u.x = ux; u.y = uy; u.ch.ap = u.ch.maxAP(*gd); u.ch.move = u.ch.maxMove(*gd); u.initiative = u.ch.stat(ST_DEX, *gd) + rng.range(1, 8);
        cb.units.push_back(u); cb.order.push_back((int)cb.units.size() - 1); G.unit = (int)cb.units.size() - 1;
        if (G.text.empty()) G.text = "Keep " + u.ch.name + " alive";
    } else if (G.kind == "reach") {
        auto q = split(G.arg, ','); G.x = q.size() > 0 ? atoi(q[0].c_str()) : 0; G.y = q.size() > 1 ? atoi(q[1].c_str()) : 0; G.w = q.size() > 2 ? std::max(1, atoi(q[2].c_str())) : 1; G.h = q.size() > 3 ? std::max(1, atoi(q[3].c_str())) : 1;
        if (G.text.empty()) G.text = "Get someone to the marked tiles";
    } else if (G.kind == "hold") {
        G.rounds = std::max(1, atoi(G.arg.c_str()));
        if (G.text.empty()) G.text = fmt("Hold out for %d rounds", G.rounds);
    } else if (G.kind == "close") {
        for (auto& u : cb.units) if (!u.friendly() && u.groupIndex == gi && u.ch.ai == "rift") u.rift = true;
        if (G.text.empty()) G.text = "Close the rifts (stand next to one: 2 AP)";
    } else { G.kind.clear(); return; }
    cb.log.push_back("Goal: " + G.text + ".");
}
void Game::goalCheck(bool roundStart) {
    CombatState::Goal& G = cb.goal; if (G.kind.empty() || G.done || G.failed || cb.ending) return;
    auto win = [&](const std::string& why) {
        G.done = true; cb.log.push_back("Goal done: " + why); log("Goal done: " + why); notify(why);
        // the fight is won where it stands: what is left of the enemy withdraws (counted as beaten, no loot)
        if (G.kind != "close") for (auto& u : cb.units) if (u.ch.alive && !u.friendly()) unitGone(u, u.ch.mechanical ? "shuts down" : "withdraws");
        cb.ending = true; cb.victory = true; cb.endTimer = 0.9f;
    };
    if (G.kind == "protect") {
        if (G.unit >= 0 && G.unit < (int)cb.units.size() && !cb.units[G.unit].ch.alive) {
            G.failed = true; std::string m = cb.units[G.unit].ch.name + " has fallen - the goal is lost.";
            cb.log.push_back(m); log(m); notify("Goal failed"); if (!G.onFail.empty()) { std::string s = G.onFail; runScript(s); }
        }
    } else if (G.kind == "reach") {
        for (auto& u : cb.units) if (u.ch.alive && u.partyIndex >= 0 && u.x >= G.x && u.y >= G.y && u.x < G.x + G.w && u.y < G.y + G.h) { win(u.ch.name + " made it through!"); return; }
    } else if (G.kind == "hold") {
        if (roundStart && cb.round > G.rounds) { win("You held out!"); return; }
    } else if (G.kind == "close") {
        bool any = false; for (auto& u : cb.units) if (u.ch.alive && u.rift) any = true;
        if (!any) { G.done = true; cb.log.push_back("Goal done: every rift is closed."); log("Every rift is closed."); notify("Rifts closed"); }
        else if (roundStart) {   // every open rift calls one more
            auto p = split(cb.goal.arg, ','); const CreatureDef* cd = gd->creature(trim(p[0]));
            int alive = 0; for (auto& u : cb.units) if (u.ch.alive && !u.friendly() && !u.rift) alive++;
            for (size_t i = 0; i < cb.units.size() && cd; i++) { if (!cb.units[i].ch.alive || !cb.units[i].rift || alive >= 8) continue;
                Combatant r = cb.units[i]; int ux = -1, uy = -1;
                for (int yy = r.y - 1; yy <= r.y + 1 && ux < 0; yy++) for (int xx = r.x - 1; xx <= r.x + 1; xx++) if (map->walkable(xx, yy) && combatUnitAt(xx, yy) < 0 && !map->chestAt(xx, yy)) { ux = xx; uy = yy; break; }
                if (ux < 0) continue;
                Combatant s; s.ch.initFromCreature(*gd, *cd, 0); s.partyIndex = -1; s.groupIndex = r.groupIndex; s.summoned = true; s.summoner = (int)i;
                s.ch.xpReward = 0; s.ch.loot.clear(); s.x = ux; s.y = uy; s.ch.ap = s.ch.maxAP(*gd); s.ch.move = s.ch.maxMove(*gd); s.initiative = s.ch.stat(ST_DEX, *gd) + rng.range(1, 8);
                cb.units.push_back(s); cb.order.push_back((int)cb.units.size() - 1); alive++;
                addFloat(ux, uy, "through the rift!", C::Purple); cb.log.push_back("Something claws its way out of the rift!");
            }
        }
    }
    if (G.done && !G.onWin.empty()) { std::string s = G.onWin; G.onWin.clear(); runScript(s); }
}

void Game::combatStepUnit(int ui, int nx, int ny) {
    Combatant& u = cb.units[ui]; int fx = u.x, fy = u.y;
    u.x = nx; u.y = ny; u.ch.move = std::max(0, u.ch.move - tileMoveCost(nx, ny));
    if (u.partyIndex == activeLeader()) { px = u.x; py = u.y; }
    revealAround();   // every party member's eyes count in a fight: walking around a corner shows what is there
    if (!camFree && (u.friendly() || visible(u.x, u.y))) camCenterOn(u.x, u.y);
    groundHurt(ui);   // 1.4.0 (#61): walking into fire
    if (u.partyIndex >= 0 || cb.goal.kind == "reach") goalCheck(false);
    combatReactions(ui, fx, fy);
}
void Game::combatReactions(int mover, int fromX, int fromY) {
    Combatant& m = cb.units[mover];
    for (size_t i = 0; i < cb.units.size(); i++) {
        Combatant& w = cb.units[i];
        if ((int)i == mover || !w.ch.alive || !m.ch.alive || w.friendly() == m.friendly() || w.ch.hasEffect("stun") || w.ch.hasEffect("freeze")) continue;
        bool wasAdj = chebyshev(w.x, w.y, fromX, fromY) <= 1, isAdj = chebyshev(w.x, w.y, m.x, m.y) <= 1;
        // attack of opportunity: a melee unit gets one free swing when a foe leaves its reach
        if (w.ch.meleeWeapon(*gd) && wasAdj && !isAdj && !w.aooDone) {
            w.aooDone = true; cb.log.push_back(w.ch.name + " gets a free attack as " + m.ch.name + " pulls away!"); addFloat(w.x, w.y, "opportunity!", C::Orange);
            int saveAP = w.ch.ap; w.ch.ap = 99;
            // the swing is resolved against the tile being left (still adjacent)
            int keepX = m.x, keepY = m.y; m.x = fromX; m.y = fromY; bool hit = combatAttack((int)i, mover, false); m.x = keepX; m.y = keepY; w.ch.ap = saveAP;
            // 1.4.0 (#71 #67): warriors' swings stop the mover; some gear slows it
            if (hit && m.ch.alive && w.ch.fx("aoostop", *gd) > 0) { m.ch.move = 0; addFloat(m.x, m.y - 1, "stopped!", C::Orange); cb.log.push_back(m.ch.name + " is stopped in their tracks."); if (mover == cb.selectedUnit) cb.aiPath.clear(); }
            else if (hit && m.ch.alive && w.ch.fx("aooslow", *gd) > 0) { m.ch.addEffect("slow", 2, 0); m.ch.move = m.ch.move / 2; addFloat(m.x, m.y - 1, "slowed", C::Cyan); }
            continue;
        }
        // overwatch: a reaction shot when a foe moves into range and sight
        if (w.overwatch && chebyshev(w.x, w.y, m.x, m.y) <= w.ch.attackRange(*gd) && targetable(w, m)) {
            w.overwatch = false; cb.log.push_back(w.ch.name + " fires on " + m.ch.name + " (overwatch)!"); addFloat(w.x, w.y, "overwatch!", C::Yellow);
            int saveAP = w.ch.ap; w.ch.ap = 99; w.ch.addEffect("reaction", 1, 0); combatAttack((int)i, mover, false); w.ch.removeEffect("reaction"); w.ch.ap = saveAP;
        }
    }
    combatCheckEnd();
}

void Game::applyDamage(Combatant& t, int dmg, int dmgType, const std::string& source, bool crit, Combatant* attacker) {
    if (!t.ch.alive) return;
    cb.actionThisRound = true;
    if (attacker) { dmg = applyCombos(t, dmg, dmgType, attacker); if (!t.ch.alive) return; }   // 1.4.0 (#60): SHATTER, THAW, FLARE, STEAM
    // 1.4.0 (#72) ward=creature: a boss shielded by its servants takes half damage while one of them stands
    if (!t.ch.ward.empty()) { for (auto& o : cb.units) if (o.ch.alive && &o != &t && o.ch.creatureId == t.ch.ward) { dmg = std::max(1, dmg / 2); addFloat(t.x, t.y - 1, "warded", C::Purple); break; } }
    int res = t.ch.resist(dmgType, *gd);
    // 1.5.0 (#92): a weapon trait the creature has a res= key for replaces its physical value (skeletons: crush:-100)
    int traitBit = -1;
    if (dmgType == DT_PHYS && !dmgTrait.empty()) { auto f = t.ch.traitRes.find(dmgTrait); if (f != t.ch.traitRes.end()) { res = f->second; traitBit = 8 + traitIndex(dmgTrait); } }
    bool shockBonus = t.ch.mechanical && dmgType == DT_SHOCK && res == 0;   // machines without a shock res= in their data
    dmg = dmg * (100 - res) / 100;
    if (dmg <= 0 && res < 100) dmg = 1;
    if (res >= 100) dmg = 0;
    if (godMode && t.partyIndex >= 0) dmg = 0;
    if (t.ch.undead && dmgType == DT_POISON) dmg = 0;
    if (t.ch.mechanical && dmgType == DT_POISON) dmg = 0;
    if (shockBonus) dmg = dmg * 3 / 2;
    // learning by hitting: the first time a damage type (or weapon trait) lands on a kind of creature, its reaction is noted
    if (!t.friendly() && !t.ch.creatureId.empty()) {
        int eff = res; if ((t.ch.undead || t.ch.mechanical) && dmgType == DT_POISON) eff = 100; if (shockBonus) eff = -50;
        std::string key = "rk_" + t.ch.creatureId; int known = atoi(extra[key].c_str());
        int bits = (1 << dmgType) | (traitBit >= 8 ? (1 << traitBit) : 0);
        bool fresh = (known & bits) != bits;
        if (fresh) { extra[key] = std::to_string(known | bits);
            if (traitBit >= 8 && !(known & (1 << traitBit)) && res < t.ch.resist(DT_PHYS, *gd)) cb.log.push_back(fmt("%s: %s weapons %s!", t.ch.name.c_str(), dmgTrait.c_str(), res < 0 ? "hit it hard" : "get through")); }
        if (eff <= -50) addFloat(t.x, t.y - 1, "WEAK!", C::Yellow); else if (eff >= 100) addFloat(t.x, t.y - 1, "IMMUNE", C::Gray, 2); else if (eff >= 40) addFloat(t.x, t.y - 1, "resists", C::Gray);
        if (traitBit >= 8 && res < t.ch.resist(DT_PHYS, *gd)) addFloat(t.x, t.y - 2, dmgTrait + " gets through", C::Cyan);
        if (eff >= 100 && fresh && attacker && attacker->partyIndex >= 0) cb.log.push_back(fmt("%s is immune to %s%s.", t.ch.name.c_str(), traitBit >= 8 ? (dmgTrait + "-trait ").c_str() : "", dmgTypeName(dmgType)));
    }
    t.ch.hp -= dmg;
    if (t.ch.hp <= 0 && dmg > 0 && !t.lastStandUsed && t.ch.fx("laststand", *gd) > 0) { t.ch.hp = 1; t.lastStandUsed = true; addFloat(t.x, t.y - 1, "LAST STAND", C::Yellow, 2); cb.log.push_back(t.ch.name + " refuses to fall!"); }   // (#71)
    addFloat(t.x, t.y, crit ? fmt("%d!", dmg) : std::to_string(dmg), crit ? C::Yellow : (t.friendly() ? foeColor() : UI::dmgColor(dmgType)), dmg > 0 ? 2 : 1);
    if (dmg > 0) {
        t.hitFlash = 0.2f;
        if (attacker && attacker != &t) { int ddx = (t.x > attacker->x) - (t.x < attacker->x), ddy = (t.y > attacker->y) - (t.y < attacker->y); t.offX = ddx * (crit ? 7.0f : 4.0f); t.offY = ddy * (crit ? 7.0f : 4.0f); }
        if (crit) { cb.hitstop = std::max(cb.hitstop, settings && settings->fastCombat ? 0.05f : 0.12f); addFloat(t.x, t.y - 1, "CRITICAL", C::Yellow); }
    }
    if (dmg > 0) cb.log.push_back(fmt("%s takes %d %s damage%s.", t.ch.name.c_str(), dmg, dmgTypeName(dmgType), crit ? " (CRITICAL)" : ""));
    gAudio.play(dmg <= 0 ? "block" : crit ? "crit" : "hit");
    if (t.friendly()) stats.damageTaken += dmg; else { stats.damageDealt += dmg; if (crit) stats.crits++; }
    if (t.ch.hp <= 0) {
        t.ch.hp = 0; t.ch.alive = false; cb.reachDirty = true;   // the corpse no longer blocks the way
        // 1.5.0 (#94): a party member who goes down is Wounded (up to 3) until an inn night or a healer
        if (t.partyIndex >= 0 && !t.summoned) { t.ch.wounds = std::min(3, t.ch.wounds + 1); cb.log.push_back(fmt("%s is down - Wounded (x%d) until treated.", t.ch.name.c_str(), t.ch.wounds)); }
        { int ti = (int)(&t - &cb.units[0]); for (auto& o : cb.units) if (o.summoned && o.summoner == ti && o.ch.alive) { o.ch.alive = false; o.ch.hp = 0; cb.log.push_back(o.ch.name + " crumbles without its master."); addFloat(o.x, o.y, "unmade", C::Gray); } }
        if (t.summoned) { cb.log.push_back(t.ch.name + " is unmade."); log(t.ch.name + " fades away."); addFloat(t.x, t.y, "unmade", C::Gray); return; }   // a summon: no body, no XP, no loot
        if (t.friendly()) stats.knockouts++; else onKill(t.ch, attacker);
        for (size_t k = 0; k < cb.windups.size(); k++) if (&cb.units[cb.windups[k].unit] == &t) { cb.windups.erase(cb.windups.begin() + k); k--; cb.log.push_back("The attack it was gathering dies with it."); }   // (#62)
        if (attacker && attacker != &t && attacker->ch.alive) if (int ka = attacker->ch.fx("killap", *gd)) attacker->ch.ap = std::min(attacker->ch.maxAP(*gd), attacker->ch.ap + ka);   // (#71)
        if (!t.friendly() && t.ch.role == "leader") leaderFell(t);   // (#63)
        // a kill lands: a beat of stillness and a burst where they stood
        cb.hitstop = std::max(cb.hitstop, settings && settings->fastCombat ? 0.12f : 0.22f);
        { CombatAnim an; an.kind = 3; an.x0 = an.x1 = t.x; an.y0 = an.y1 = t.y; an.color = t.friendly() ? friendColor() : foeColor(); an.dur = 0.5f; an.tile = Tile{}; cb.anims.push_back(an); }
        addFloat(t.x, t.y - 1, t.friendly() ? "DOWN" : "SLAIN", t.friendly() ? foeColor() : C::Cream);
        gAudio.play(deathSound(t.ch, t.friendly()));
        cb.log.push_back(t.ch.name + (t.friendly() ? " is knocked out!" : " is slain!"));
        log(t.ch.name + (t.friendly() ? " is knocked out!" : " dies."));
        if (!t.friendly()) { cb.xpGained += t.ch.xpReward; spawnLoot(t); }
    }
    (void)source; (void)attacker;
}

void Game::spawnLoot(Combatant& u) {
    int lo = 0, hi = 0; parseRange(u.ch.lootGold, lo, hi);
    int g = rng.range(lo, hi); cb.goldGained += g;
    for (auto& l : u.ch.loot) if (rng.chance(l.second)) { if (auto* d = gd->item(l.first)) { std::string id = l.first; if (d->equippable()) id = makeAffixed(id, lootTierRoll(u.ch.level)); const ItemDef* fd = gd->item(id); cb.itemsGained.push_back(fd ? fd->name : d->name); party.addItem(id, 1, *gd); bountyEvent("item", id, 1); } }
}

bool Game::combatAttack(int ai, int ti, bool aimed) {
    Combatant& a = cb.units[ai]; Combatant& t = cb.units[ti];
    int cost = a.ch.attackAP(*gd) + (aimed ? 2 : 0);
    if (a.ch.ap < cost) { if (a.partyIndex >= 0) log("Not enough AP."); return false; }
    const ItemDef* w = a.ch.weapon(*gd);
    // out of ammunition: a gun or bow can still be swung at someone standing right next to you (half damage)
    bool clubbing = false;
    if (w && !w->ammo.empty() && a.partyIndex >= 0) {
        if (!party.hasItem(w->ammo)) {
            if (unitDist(t, a.x, a.y) > 1) { log("Out of " + gd->item(w->ammo)->name + "! (You can still club someone standing next to you.)"); return false; }
            clubbing = true;
        } else party.removeItem(w->ammo, 1);
    }
    a.ch.ap -= cost;
    bool ranged = !a.ch.meleeWeapon(*gd) && !clubbing;
    std::string trait = clubbing ? "" : a.ch.trait(*gd);
    if (clubbing) cb.log.push_back(a.ch.name + " is out of " + gd->item(w->ammo)->name + " and swings the " + w->name + " like a club.");
    CombatAnim an; an.kind = ranged ? 0 : 2; an.x0 = a.x; an.y0 = a.y; an.x1 = t.x; an.y1 = t.y; an.color = w ? dmgColor(w->dmgType) : C::White; an.dur = ranged ? 0.25f : 0.18f; cb.anims.push_back(an);
    gAudio.play(weaponSound(a.ch));
    int chance = hitChance(a, t, aimed); cb.actionThisRound = true;
    if (testForceHit >= 0) chance = testForceHit ? 100 : 0;
    bool fl = flanked(a, t), ex = exposed(a, t), mk = marked(a, t); bool arced = false; effCover(a, t, &arced);
    if (!rng.chance(chance)) { addFloat(t.x, t.y, "miss", C::Gray, 2); gAudio.play("miss");
        { int ddx = (t.x > a.x) - (t.x < a.x), ddy = (t.y > a.y) - (t.y < a.y); int side = rng.chance(50) ? 1 : -1; t.offX = (float)(-ddy * side * 6); t.offY = (float)(ddx * side * 6); if (!ddx && !ddy) t.offX = 6; } cb.log.push_back(a.ch.name + " misses " + t.ch.name + "."); return false; }
    int lo, hi, type; a.ch.attackDamage(*gd, lo, hi, type);
    // weapon oils / special arrows: the next few attacks deal another element (+2 damage)
    int imbued = -1;
    for (auto& e : a.ch.effects) if (startsWith(e.id, "imbue_")) { imbued = dmgTypeFromName(e.id.substr(6)); type = imbued; lo += 2; hi += 2; if (--e.power <= 0) e.turns = 0; break; }
    a.ch.effects.erase(std::remove_if(a.ch.effects.begin(), a.ch.effects.end(), [](const StatusEffect& e) { return startsWith(e.id, "imbue_") && e.turns <= 0; }), a.ch.effects.end());
    int dmg = rng.range(lo, hi); if (clubbing) dmg = std::max(1, dmg / 2);
    int critC = critChance(a, t, aimed);   // 1.5.0 (#83)
    bool tagged = t.ch.hasEffect("tagged") && t.friendly() != a.friendly();   // (#65) a tagged target: the next hit is a critical
    bool crit = tagged || rng.chance(critC);
    if (tagged) { t.ch.removeEffect("tagged"); addFloat(t.x, t.y - 1, "EXPLOIT", C::Cyan, 2); cb.log.push_back(a.ch.name + " hits the weak point " + t.ch.name + " was tagged on!"); }
    if (crit) dmg = dmg * 3 / 2;   // 1.5.0 (#83): x1.5 (was x2); armour is still halved below
    if (a.ch.hasEffect("strength")) dmg += a.ch.effect("strength")->power / 2;
    // (#67 #71) damage against particular targets
    int pct = 100;
    if (arced) pct -= 25;
    if (t.ch.hasEffect("freeze") || t.ch.hasEffect("chilled")) pct += a.ch.fx("vsfrozen", *gd);
    if (t.ch.undead) pct += a.ch.fx("vsundead", *gd);
    if (mk) pct += a.ch.fx("markdmg", *gd);
    dmg = std::max(1, dmg * pct / 100);
    // armour soaks hits but can't make them harmless: at least 35% of the blow always lands (maces: 60%)
    int floorPct = trait == "crush" ? 60 : 35;
    dmg = std::max((dmg * floorPct + 99) / 100, dmg - t.ch.defense(*gd) / (crit ? 2 : 1)); dmg = std::max(1, dmg);
    dmgTrait = trait; applyDamage(t, dmg, type, a.ch.name, crit, &a); dmgTrait.clear();   // 1.5.0 (#92): trait res=
    if (t.ch.alive && !a.ch.onHitEffect.empty() && rng.chance(35)) { t.ch.addEffect(a.ch.onHitEffect, 3, 3); cb.log.push_back(t.ch.name + " is " + a.ch.onHitEffect + "ed!"); }
    int elem = imbued >= 0 ? imbued : (w ? w->dmgType : -1);
    if (t.ch.alive && crit && a.ch.fx("critburn", *gd) > 0) { t.ch.addEffect("burn", 2, 3); addFloat(t.x, t.y - 1, "set alight", C::Orange); }
    if (t.ch.alive && elem == DT_FIRE && rng.chance(25)) t.ch.addEffect("burn", 2, 3);
    if (t.ch.alive && elem == DT_COLD && rng.chance(25)) t.ch.addEffect("freeze", 2, 0);
    if (t.ch.alive && elem == DT_COLD) t.ch.addEffect("chilled", 2 + a.ch.fx("primerturns", *gd), 0);
    if (t.ch.alive && elem == DT_POISON && rng.chance(40)) t.ch.addEffect("poison", 3, 3);
    if (t.ch.alive && elem == DT_SHOCK && rng.chance(20) && !t.ch.boss) t.ch.addEffect("stun", 1, 0);
    // (#68) axes cleave into foes next to both of you; shotguns spread into a short cone past the target
    if (trait == "cleave" || trait == "spread") {
        std::vector<int> extra;
        if (trait == "cleave") { for (size_t i = 0; i < cb.units.size(); i++) { auto& o = cb.units[i]; if ((int)i != ti && o.ch.alive && o.friendly() != a.friendly() && unitDist(o, a.x, a.y) <= 1 && unitDist(o, t.x, t.y) <= 1) extra.push_back((int)i); } }
        else { SpellDef cone; cone.shape = "cone"; cone.range = std::min(4, chebyshev(a.x, a.y, t.x, t.y) + 2); auto tiles = spellTiles(ai, cone, t.x, t.y);
            for (size_t i = 0; i < cb.units.size(); i++) { auto& o = cb.units[i]; if ((int)i == ti || (int)i == ai || !o.ch.alive) continue; for (auto& q : tiles) if (q.first == o.x && q.second == o.y) { extra.push_back((int)i); break; } } }
        for (int i : extra) { auto& o = cb.units[i]; addFloat(o.x, o.y - 1, trait == "cleave" ? "cleaved" : "buckshot", C::Orange); dmgTrait = trait; applyDamage(o, std::max(1, dmg / 2 - o.ch.defense(*gd) / 2), type, a.ch.name, false, &a); dmgTrait.clear(); }
    }
    shake = std::max(shake, crit ? 0.2f : 0.08f);
    return true;
}

// Who a spell cast at (tx,ty) affects. Area damage is indiscriminate: the caster and allies inside the blast get hit too
// (spells with ff=0 - weapon skills like Cleave or Volley - only hit enemies). Healing and buffs only touch the caster's side.
std::vector<int> Game::spellVictims(int ci, const SpellDef& sp, int tx, int ty) const {
    std::vector<int> out; const Combatant& c = cb.units[ci];
    bool allyTarget = (sp.target == "ally" || sp.target == "self");
    bool harmful = sp.dmgMax > 0 || (!allyTarget && !sp.effect.empty() && sp.effect != "cure" && sp.effect != "revive");
    std::vector<std::pair<int, int>> shapeTiles; if (!sp.shape.empty()) shapeTiles = spellTiles(ci, sp, tx, ty);   // 1.4.0 (#64)
    for (size_t i = 0; i < cb.units.size(); i++) {
        const Combatant& t = cb.units[i];
        if (!t.ch.alive && sp.effect != "revive") continue;
        if (!sp.shape.empty()) { bool in = false; for (auto& q : shapeTiles) for (int b = 0; b < t.ch.size; b++) for (int a = 0; a < t.ch.size; a++) if (t.x + a == q.first && t.y + b == q.second) in = true; if (!in || (int)i == ci) continue; }
        else if (unitDist(t, tx, ty) > sp.area) continue;
        bool sameSide = t.friendly() == c.friendly();
        if (allyTarget) { if (!sameSide) continue; }
        else if ((sp.area > 0 || !sp.shape.empty()) && harmful && sp.friendlyFire) { /* everyone in the blast */ }
        else if (sameSide && sp.target != "any") continue;
        out.push_back((int)i);
    }
    return out;
}

void Game::combatCast(int ci, const std::string& spellId, int tx, int ty) {
    const SpellDef* sp0 = gd->spell(spellId); if (!sp0) return;
    SpellDef spv = castSpell(cb.units[ci].ch, *sp0); const SpellDef* sp = &spv;   // 1.4.0 (#66): rank upgrades
    int cost = std::max(0, sp->cost * (100 - cb.units[ci].ch.fx("mpcost", *gd) + cb.units[ci].ch.spellCostPct(*gd)) / 100);   // 1.5.0 (#93): heavy armour costs casters MP
    {
        Combatant& c = cb.units[ci];
        if (c.partyIndex >= 0 && c.ch.level < sp->level) { log(fmt("%s needs level %d to cast %s.", c.ch.name.c_str(), sp->level, sp->name.c_str())); return; }
        if (c.ch.ap < sp->ap) { if (c.partyIndex >= 0) log("Not enough AP."); return; }
        if (c.ch.mp < cost) { if (c.partyIndex >= 0) log("Not enough MP."); return; }
        c.ch.ap -= sp->ap; c.ch.mp -= cost; cb.actionThisRound = true;
        // 1.5.0 (#93): armour too heavy for the caster's class can make the spell fizzle (AP and MP are spent)
        if (int fail = c.ch.spellFail(*gd)) if (rng.chance(fail)) { cb.log.push_back(fmt("%s's spell fizzles - the armour is too heavy (%d%% fail).", c.ch.name.c_str(), fail)); addFloat(c.x, c.y - 1, "fizzle", C::Gray, 2); gAudio.play("miss"); return; }
        cb.log.push_back(c.ch.name + " casts " + sp->name + "!"); gAudio.play(spellSound(*sp)); if (c.partyIndex >= 0) stats.spellsCast++;
        if (sp->chainJumps <= 0) { CombatAnim an; an.kind = sp->shape == "line" ? 0 : 1; an.x0 = c.x; an.y0 = c.y; an.x1 = tx; an.y1 = ty; an.color = sp->utility == "light" ? C::Cream : dmgColor(sp->dmgType); an.dur = 0.35f; an.tile = sp->tile; cb.anims.push_back(an); }
    }
    Combatant& c = cb.units[ci];
    int power = c.ch.stat(sp->scaleStat, *gd);
    // light in a fight: a floating light at the target tile that shows what hides in the dark (and the party's own glow brightens)
    if (sp->utility == "light") {
        int r = 4 + power / 6; cb.lights.push_back({tx, ty, r, 5 + power / 4});
        if (c.partyIndex >= 0) { lightSteps = std::min(600, lightSteps + 60 + power * 3); lightPower = std::max(lightPower, sp->utilityPower > 0 ? sp->utilityPower : 7); }
        cb.log.push_back(fmt("A light blooms (radius %d).", r)); addFloat(tx, ty, "light", C::Cream);
        revealAround(); shake = std::max(shake, 0.05f); return;
    }
    if (!sp->summon.empty()) { combatSummon(ci, *sp, tx, ty); shake = std::max(shake, 0.06f); return; }
    std::vector<std::pair<int, int>> tiles = (sp->area > 0 || !sp->shape.empty()) ? spellTiles(ci, *sp, tx, ty) : std::vector<std::pair<int, int>>{{tx, ty}};
    // (#58) smoke=N: a cloud over the area that blocks sight; whoever stands in it can only be hit from next to them
    if (sp->smoke > 0) {
        int turns = sp->smoke + c.ch.fx("smoketurns", *gd);
        for (auto& q : tiles) if (map->inBounds(q.first, q.second) && (map->at(q.first, q.second).walkable || map->at(q.first, q.second).water)) addGround(q.first, q.second, 3, turns);
        cb.log.push_back(fmt("Smoke billows out (%d rounds).", turns)); addFloat(tx, ty, "smoke", C::LtGray); cb.reachDirty = true;
        if (sp->dmgMax <= 0 && sp->effect.empty()) { revealAround(); return; }
    }
    std::vector<int> victims;
    std::vector<int> chain;
    if (sp->chainJumps > 0) {   // (#64) a real chain: target to target, weaker with every jump
        int first = combatUnitAt(tx, ty); chain = chainTargets(ci, *sp, first); victims = chain;
        int fx0 = c.x, fy0 = c.y; float delay = 0;
        for (int v : chain) { CombatAnim an; an.kind = 0; an.x0 = fx0; an.y0 = fy0; an.x1 = cb.units[v].x; an.y1 = cb.units[v].y; an.color = dmgColor(sp->dmgType); an.dur = 0.2f; an.t = -delay; cb.anims.push_back(an); fx0 = an.x1; fy0 = an.y1; delay += 0.12f; }
    } else victims = spellVictims(ci, *sp, tx, ty);
    bool selfHit = false; int alliesHit = 0, jump = 0, avgDmg = 0;
    for (int i : victims) {
        Combatant& t = cb.units[i];
        bool sameSide = t.friendly() == c.friendly();
        if (sp->dmgMax > 0) {
            int dmg = rng.range(sp->dmgMin, sp->dmgMax) + (power - 8) / 2;
            dmg = dmg * (100 + c.ch.fx("spelldmg", *gd) + (t.ch.undead ? c.ch.fx("vsundead", *gd) : 0) + (marked(c, t) ? c.ch.fx("markdmg", *gd) : 0)) / 100;
            if (t.ch.hasEffect("freeze") || t.ch.hasEffect("chilled")) dmg = dmg * (100 + c.ch.fx("vsfrozen", *gd)) / 100;
            if (!chain.empty()) { for (int k = 0; k < jump; k++) dmg = dmg * (100 - sp->chainFalloff) / 100; jump++; }
            bool crit = rng.chance(clampi(3 + power / 5 + c.ch.fx("crit", *gd), 0, CRIT_CAP));   // 1.5.0 (#83): was 5 + power/3
            if (crit) dmg = dmg * 3 / 2;
            int cp = spellCoverPct(c, t, *sp);   // (#57) single-target spells lose damage to cover
            if (cp > 0) { dmg = dmg * (100 - cp) / 100; addFloat(t.x, t.y - 1, fmt("cover -%d%%", cp), C::Gray); }
            if (sameSide && (sp->area > 0 || !sp->shape.empty() || !chain.empty())) { if (i == ci) selfHit = true; else alliesHit++; }
            bool chilledBefore = t.ch.hasEffect("chilled");
            avgDmg = std::max(avgDmg, dmg);
            applyDamage(t, std::max(1, dmg), sp->dmgType, sp->name, crit, &c);
            if (t.ch.alive && sp->dmgType == DT_COLD) {   // (#65) cold primes the target: chilled (and a mage's Lingering Cold freezes a chilled one)
                if (chilledBefore && c.ch.fx("chillfreeze", *gd) > 0) { t.ch.addEffect("freeze", 2, 0); addFloat(t.x, t.y - 1, "frozen!", C::Cyan); }
                t.ch.addEffect("chilled", 2 + c.ch.fx("primerturns", *gd), 0);
            }
        }
        if (sp->heal > 0 && t.ch.alive) { int h = (sp->heal + (power - 8)) * (100 + c.ch.fx("healpower", *gd)) / 100; int before = t.ch.hp; t.ch.hp = std::min(t.ch.maxHP(*gd), t.ch.hp + h); addFloat(t.x, t.y, "+" + std::to_string(t.ch.hp - before), C::Green);
            if (c.ch.fx("healcure", *gd) > 0) { t.ch.removeEffect("poison"); t.ch.removeEffect("burn"); t.ch.removeEffect("weaken"); } }
        if (sp->effect == "revive" && !t.ch.alive && !t.gone) { t.ch.alive = true; t.ch.hp = std::max(1, t.ch.maxHP(*gd) / 3); addFloat(t.x, t.y, "revived", C::Green); cb.reachDirty = true; }
        else if (!sp->effect.empty() && t.ch.alive) {
            int dur = (sp->duration > 0 ? sp->duration : 3) + ((sp->effect == "marked" || sp->effect == "tagged") ? c.ch.fx("primerturns", *gd) : 0);
            if (sp->effect == "cure") { t.ch.removeEffect("poison"); t.ch.removeEffect("burn"); t.ch.removeEffect("weaken"); t.ch.removeEffect("blind"); t.ch.removeEffect("freeze"); t.ch.removeEffect("slow"); }
            else if (sp->effect == "stun" && t.ch.boss) { /* bosses resist stun */ }
            else if (sp->effect == "override") {   // 1.4.0 (#71): a Scavenger hijacks an enemy machine for a few turns
                if (t.ch.mechanical && !t.ch.boss && !t.friendly()) { t.ally = true; t.ch.addEffect("overridden", dur, 0); addFloat(t.x, t.y - 1, "HIJACKED", C::Cyan); cb.log.push_back(t.ch.name + " is hijacked and turns on its friends!"); cb.reachDirty = true; }
                else { addFloat(t.x, t.y - 1, "no effect", C::Gray); cb.log.push_back(t.ch.name + (t.ch.mechanical ? " is too well shielded to hijack." : " is no machine.")); }
            }
            else if (sp->effect == "marked" || sp->effect == "tagged") { t.ch.addEffect(sp->effect, dur, 0); addFloat(t.x, t.y - 1, sp->effect == "marked" ? "MARKED" : "TAGGED", C::Yellow); cb.log.push_back(t.ch.name + (sp->effect == "marked" ? " is marked: everyone hits it more easily and harder." : " is tagged: the next hit on it is a critical.")); }
            else if (rng.chance(sp->dmgMax > 0 ? 50 : 100)) t.ch.addEffect(sp->effect, dur, 3 + power / 3);
        }
        if (sp->knockback > 0 && t.ch.alive && !sameSide) knockBack(ci, i);   // (#58) Shield Bash
    }
    if (selfHit) { cb.log.push_back(c.ch.name + " is caught in the blast!"); addFloat(c.x, c.y, "caught in blast!", C::Orange); }
    if (alliesHit > 0) cb.log.push_back(fmt("The blast hits %d of %s's allies!", alliesHit, c.ch.name.c_str()));
    if (sp->dmgMax > 0) {
        // (#61) shock carries through water: from the target, or every link of a chain
        if (sp->dmgType == DT_SHOCK) { if (!chain.empty()) { for (int v : chain) conductShock(cb.units[v].x, cb.units[v].y, avgDmg, &c, victims); } else conductShock(tx, ty, avgDmg, &c, victims); }
        bool area = sp->area > 0 || !sp->shape.empty();
        if (area) damageTiles(tiles, avgDmg, sp->dmgType);   // (#58) blasts break furniture
        // (#61) fire leaves burning ground on flammable tiles (ground=fire: on every tile), cold area spells leave ice
        if (sp->ground == "fire") for (auto& q : tiles) addGround(q.first, q.second, 1, 3);
        else if (sp->dmgType == DT_FIRE && area) for (auto& q : tiles) if (map->inBounds(q.first, q.second) && allTileKinds()[map->at(q.first, q.second).kind].flammable) addGround(q.first, q.second, 1, 3);
        if (sp->ground == "ice" || (sp->dmgType == DT_COLD && area)) for (auto& q : tiles) addGround(q.first, q.second, 2, 2);
        if (!sp->ground.empty() || area) cb.reachDirty = true;
    }
    if ((sp->area > 0 || !sp->shape.empty()) && (sp->dmgType == DT_FIRE || sp->dmgType == DT_SHOCK) && sp->dmgMax > 0) cb.lights.push_back({tx, ty, sp->area + 2, 1});   // the flash lights the room for a moment
    revealAround();
    shake = std::max(shake, 0.12f);
}

// summon=creature[:count]: creatures appear on free tiles around the target and fight for the caster's side (the AI moves
// them). They last duration= of their own turns (0 = the whole fight), leave no body, give no XP or loot, and a caster keeps
// at most 3 at a time. A party caster's summons get +1 level per 3 caster levels.
int Game::summonsOf(int ci) const { int n = 0; for (auto& o : cb.units) if (o.summoned && o.ch.alive && o.summoner == ci) n++; return n; }
void Game::combatSummon(int ci, const SpellDef& sp, int tx, int ty) {
    auto p = split(sp.summon, ':'); const CreatureDef* cd = gd->creature(trim(p[0])); int count = p.size() > 1 ? clampi(atoi(p[1].c_str()), 1, 4) : 1;
    if (!cd) { cb.log.push_back("(unknown summon " + sp.summon + ")"); return; }
    Combatant c = cb.units[ci]; int made = 0;
    for (int k = 0; k < count && summonsOf(ci) < 3; k++) {
        int ux = -1, uy = -1;
        for (int r = 0; r <= 3 && ux < 0; r++) { std::vector<std::pair<int, int>> cand;
            for (int yy = ty - r; yy <= ty + r; yy++) for (int xx = tx - r; xx <= tx + r; xx++) { if (std::max(abs(xx - tx), abs(yy - ty)) != r) continue;
                bool ok = true; for (int b = 0; b < cd->size && ok; b++) for (int a = 0; a < cd->size; a++) { int qx = xx + a, qy = yy + b; if (!map->walkable(qx, qy) || combatUnitAt(qx, qy) >= 0 || map->chestAt(qx, qy) || map->npcAt(qx, qy)) ok = false; }
                if (ok) cand.push_back({xx, yy}); }
            if (!cand.empty()) { auto pk = rng.pick(cand); ux = pk.first; uy = pk.second; } }
        if (ux < 0) break;
        Combatant s; s.ch.initFromCreature(*gd, *cd, c.partyIndex >= 0 ? c.ch.level / 3 : 0);
        s.partyIndex = -1; s.ally = c.friendly(); s.summoned = true; s.summoner = ci; s.summonTurns = sp.duration; s.groupIndex = -1;
        s.ch.xpReward = 0; s.ch.loot.clear(); s.ch.lootGold = "0";
        s.x = ux; s.y = uy; s.ch.ap = s.ch.maxAP(*gd); s.ch.move = s.ch.maxMove(*gd); s.initiative = s.ch.stat(ST_DEX, *gd) + rng.range(1, 8);
        cb.units.push_back(s); cb.order.push_back((int)cb.units.size() - 1); made++;
        CombatAnim an; an.kind = 1; an.x0 = ux; an.y0 = uy; an.x1 = ux; an.y1 = uy; an.color = c.friendly() ? C::Cyan : C::Red; an.dur = 0.4f; cb.anims.push_back(an);
        addFloat(ux, uy, "summoned", c.friendly() ? C::Cyan : C::Orange);
    }
    if (made) { cb.log.push_back(fmt("%s calls %d %s%s.", c.ch.name.c_str(), made, cd->name.c_str(), made > 1 ? "s" : "")); cb.reachDirty = true; }
    else cb.log.push_back(summonsOf(ci) >= 3 ? c.ch.name + " can hold no more summons." : "There is no room for it to appear.");
}
static bool thrownItem(const ItemDef& d) { return d.dmgMax > 0 || d.effect == "flare" || d.effect == "smoke"; }   // grenades, flares, smoke bombs
int Game::itemAP(const ItemDef& d) const {
    int ap = thrownItem(d) ? 3 : 4;
    if (inCombat() && cb.selectedUnit >= 0 && cb.selectedUnit < (int)cb.units.size()) ap -= cb.units[cb.selectedUnit].ch.fx("itemap", *gd);   // (#71) Quick Hands
    return std::max(1, ap);
}
void Game::combatUseItem(int ui, const std::string& itemId, int target, int tx, int ty) {
    const ItemDef* d = gd->item(itemId); if (!d) return;
    int cost = itemAP(*d);
    if (cb.units[ui].ch.ap < cost) { log(fmt("Not enough AP (%s costs %d).", d->name.c_str(), cost)); return; }
    if (target >= 0 && tx < 0) { tx = cb.units[target].x; ty = cb.units[target].y; }
    if (!d->useSpell.empty()) { Combatant& u = cb.units[ui]; u.ch.ap -= cost; party.removeItem(itemId, 1); int saveAP = u.ch.ap, saveMP = u.ch.mp; u.ch.ap += 99; u.ch.mp += 999; combatCast(ui, d->useSpell, tx, ty); cb.units[ui].ch.ap = saveAP; cb.units[ui].ch.mp = saveMP; return; }
    Combatant& u = cb.units[ui];
    if (d->effect == "flare") {   // a thrown light: shows what lurks in the dark for several rounds
        u.ch.ap -= cost; party.removeItem(itemId, 1); gAudio.play("light");
        CombatAnim an; an.kind = 0; an.x0 = u.x; an.y0 = u.y; an.x1 = tx; an.y1 = ty; an.color = C::Orange; an.dur = 0.3f; cb.anims.push_back(an);
        cb.lights.push_back({tx, ty, d->range > 1 ? d->range : 4, 6}); cb.log.push_back(u.ch.name + " throws a " + d->name + "."); revealAround(); return;
    }
    if (d->effect == "smoke") {   // 1.4.0 (#58): a smoke bomb - a cloud that blocks sight for a few rounds
        u.ch.ap -= cost; party.removeItem(itemId, 1); gAudio.play(d->sound.empty() ? "grenade_throw" : d->sound);
        CombatAnim an; an.kind = 0; an.x0 = u.x; an.y0 = u.y; an.x1 = tx; an.y1 = ty; an.color = C::LtGray; an.dur = 0.3f; cb.anims.push_back(an);
        int r = std::max(1, d->range - 1), turns = (d->fx.count("smoke") ? d->fx.at("smoke") : 3) + u.ch.fx("smoketurns", *gd);
        for (int y = ty - r; y <= ty + r; y++) for (int x = tx - r; x <= tx + r; x++) if (map->inBounds(x, y) && (map->at(x, y).walkable || map->at(x, y).water)) addGround(x, y, 3, turns);
        cb.log.push_back(u.ch.name + " throws a " + d->name + fmt(" - smoke for %d rounds.", turns)); cb.reachDirty = true; revealAround(); return;
    }
    if (d->dmgMax > 0) { // thrown (grenade): the blast hurts everyone in it, friend or foe
        u.ch.ap -= cost; party.removeItem(itemId, 1); gAudio.play(d->sound.empty() ? "explosion" : d->sound);
        CombatAnim an; an.kind = 0; an.x0 = u.x; an.y0 = u.y; an.x1 = tx; an.y1 = ty; an.color = dmgColor(d->dmgType); an.dur = 0.3f; cb.anims.push_back(an);
        int r = std::max(0, d->range - 1);
        for (size_t i = 0; i < cb.units.size(); i++) { Combatant& o = cb.units[i]; if (!o.ch.alive || unitDist(o, tx, ty) > r) continue; if (o.partyIndex >= 0 && (int)i != ui) cb.log.push_back(o.ch.name + " is caught in the blast!"); else if ((int)i == ui) cb.log.push_back(u.ch.name + " is too close to the blast!"); applyDamage(o, rng.range(d->dmgMin, d->dmgMax), d->dmgType, d->name, false, &cb.units[ui]); if (o.ch.alive && d->dmgType == DT_FIRE && unitDist(o, tx, ty) <= 1) o.ch.addEffect("burn", 2, 3); if (o.ch.alive && d->dmgType == DT_SHOCK && o.ch.mechanical && rng.chance(50)) o.ch.addEffect("stun", 1, 0); if (o.ch.alive && d->dmgType == DT_COLD && rng.chance(40)) o.ch.addEffect("freeze", 2, 0); }
        if (d->dmgType == DT_FIRE) cb.lights.push_back({tx, ty, r + 2, 1});
        {   // 1.4.0: the blast breaks furniture (#58), fire catches (#61), shock runs through water
            std::vector<std::pair<int, int>> tiles; for (int y = ty - r; y <= ty + r; y++) for (int x = tx - r; x <= tx + r; x++) tiles.push_back({x, y});
            damageTiles(tiles, d->dmgMax + 4, d->dmgType);
            if (d->dmgType == DT_FIRE) for (auto& q : tiles) if (map->inBounds(q.first, q.second) && (allTileKinds()[map->at(q.first, q.second).kind].flammable || (q.first == tx && q.second == ty))) addGround(q.first, q.second, 1, 3);
            if (d->dmgType == DT_COLD) for (auto& q : tiles) addGround(q.first, q.second, 2, 2);
            if (d->dmgType == DT_SHOCK) conductShock(tx, ty, (d->dmgMin + d->dmgMax) / 2, &cb.units[ui], {});
            cb.reachDirty = true;
        }
        revealAround(); return;
    }
    if (target < 0) { log("Choose a character."); return; }
    Combatant& t = cb.units[target];
    if (useItemOn(itemId, t.ch, true)) { cb.units[ui].ch.ap -= cost; addFloat(t.x, t.y, d->name, C::Green); }
}

void Game::combatCheckEnd() {
    bool anyEnemy = false, anyParty = false;
    for (auto& u : cb.units) { if (!u.ch.alive) continue; if (u.partyIndex >= 0) anyParty = true; else if (!u.friendly()) anyEnemy = true; }
    if (!anyEnemy) { cb.ending = true; cb.victory = true; cb.endTimer = 0.8f; }
    else if (!anyParty) { cb.ending = true; cb.victory = false; cb.endTimer = 1.2f; }
}

void Game::combatFinish() {
    // sync party members back
    for (auto& u : cb.units) if (u.partyIndex >= 0) { Character& m = party.members[u.partyIndex]; int keepX = u.x, keepY = u.y; u.ch.x = keepX; u.ch.y = keepY; m = u.ch; m.effects.erase(std::remove_if(m.effects.begin(), m.effects.end(), [](const StatusEffect& e) { return e.id == "defend" || e.id == "aim" || e.id == "stun" || e.id == "freeze" || e.id == "chilled" || e.id == "slow" || e.id == "marked" || e.id == "tagged" || e.id == "routed" || e.id == "blind"; }), m.effects.end()); }
    int mapLeader = activeLeader();
    for (auto& u : cb.units) if (u.partyIndex == mapLeader) { px = u.x; py = u.y; break; }
    lastFightWon = cb.victory;   // 1.1.10: a script's "won" after a fight
    if (cb.victory) {
        for (auto& m : party.members) if (!m.alive) { m.alive = true; m.hp = 1; }
        for (int g : cb.defeatedGroups) if (g >= 0 && g < (int)map->enemies.size()) { map->enemies[g].defeated = true; if (!map->enemies[g].flag.empty()) setFlag(map->enemies[g].flag); }
        party.gold += cb.goldGained; stats.goldEarned += cb.goldGained; stats.fightsWon++;
        log(fmt("Victory! +%d XP, +%d gold.", cb.xpGained, cb.goldGained));
        banterVictoryPending = true;   // companions may comment once the party is back on the map (banter when=victory)
        popMode();
        trail.clear(); for (int i = 0; i < 8; i++) trail.push_back({px, py});
        revealAround(); updateCamera();
        giveXP(cb.xpGained);
        pushMode(GMode::VICTORY);
        if (!cb.onDefeatScript.empty()) { std::string s = cb.onDefeatScript; runScript(s); }
    } else if (cb.fled && cb.disengaged) {
        std::vector<std::string> defeatScripts;
        // the fight broke off: what was killed stays dead (its group too, if none of it is left), XP and gold are kept
        for (auto& m : party.members) if (!m.alive) { m.alive = true; m.hp = 1; }
        for (int g : cb.defeatedGroups) if (g >= 0 && g < (int)map->enemies.size()) {
            bool anyLeft = false; for (auto& u : cb.units) if (!u.friendly() && u.groupIndex == g && u.ch.alive && !u.summoned) anyLeft = true;
            if (!anyLeft) { map->enemies[g].defeated = true; if (!map->enemies[g].flag.empty()) setFlag(map->enemies[g].flag); if (!map->enemies[g].onDefeatScript.empty()) defeatScripts.push_back(map->enemies[g].onDefeatScript); }
            else { map->enemies[g].defeated = false; map->enemies[g].wanderTimer = 4000; }
        }
        party.gold += cb.goldGained; stats.goldEarned += cb.goldGained;
        flags["_fleegrace"] = 8;
        popMode(); trail.clear(); for (int i = 0; i < 8; i++) trail.push_back({px, py}); updateCamera();
        if (cb.xpGained > 0) giveXP(cb.xpGained);
        for (auto& sc : defeatScripts) runScript(sc);   // a boss that died before the fight broke off still ends its story
    } else if (cb.fled) {
        for (int g : cb.defeatedGroups) if (g >= 0 && g < (int)map->enemies.size()) { map->enemies[g].defeated = false; map->enemies[g].wanderTimer = 4000; }
        flags["_fleegrace"] = 8; stats.fightsFled++;
        popMode(); trail.clear(); for (int i = 0; i < 8; i++) trail.push_back({px, py}); updateCamera();
        log("You escaped.");
    } else if (!campaign.defeat.empty()) {
        // 1.1.7 campaign defeat=: a lost fight isn't the end (the tutorial). The party gets back up healed, the groups
        // in the fight are whole again and stay put a moment (like after fleeing), and the campaign's script runs.
        for (auto& m : party.members) { m.wounds = 0; m.fullHeal(*gd); }   // 1.5.0: a do-over, no wounds
        for (auto& u : cb.units) if (!u.friendly() && u.groupIndex >= 0 && u.groupIndex < (int)map->enemies.size()) { map->enemies[u.groupIndex].defeated = false; map->enemies[u.groupIndex].wanderTimer = 4000; }
        flags["_fleegrace"] = 8;
        popMode(); trail.clear(); for (int i = 0; i < 8; i++) trail.push_back({px, py}); updateCamera();
        log("The party is beaten - but gets back up.");
        std::string s = campaign.defeat; runScript(s);
    } else {
        gameOver = true; running = false;
        modeStack.clear(); mode = GMode::GAMEOVER;
    }
}

// ---------------- AI ----------------
// cover a tile would give against foe f: same rule as coverAt, for a hypothetical position
int Game::coverForTile(int x, int y, int fx, int fy, int self) const {
    return coverVs(x, y, fx, fy, self, self >= 0 && self < (int)cb.units.size() ? cb.units[self].friendly() : false);   // 1.4.0: same rule as coverAt (smoke, guards)
}
// A reachable tile from which unit ui can still shoot target ti and that has better cover against the foes who can see it.
bool Game::aiCoverSpot(int ui, int ti, int& bx, int& by) {
    const Combatant& u = cb.units[ui]; const Combatant& t = cb.units[ti]; int range = u.ch.attackRange(*gd);
    auto score = [&](int x, int y) { int s = 0; for (auto& o : cb.units) if (o.ch.alive && o.friendly() != u.friendly() && chebyshev(x, y, o.x, o.y) <= 9 && combatLOS(o.x, o.y, x, y)) { int c = coverForTile(x, y, o.x, o.y, ui); s += c == 2 ? 30 : c == 1 ? 15 : 0; if (chebyshev(x, y, o.x, o.y) <= 1) s -= 40; } return s; };
    // 1.4.0 (#59): shooting into the target's cover is poor; a spot it is exposed from (cover beside it, none facing you) is good
    int expo = aiSmart() == 0 ? 0 : aiSmart() == 1 ? 18 : 30;
    auto tscore = [&](int x, int y) {
        int c = coverVs(t.x, t.y, x, y, ti, t.friendly()); if (c > 0) return -c * (aiSmart() == 0 ? 4 : 12);
        if (chebyshev(x, y, t.x, t.y) <= 1) return 0;
        static const int DX[4] = {0, 1, 0, -1}, DY[4] = {-1, 0, 1, 0};
        for (int k = 0; k < 4; k++) { int cx = t.x + DX[k], cy = t.y + DY[k]; if (!map->inBounds(cx, cy)) continue; const Cell& cc = map->at(cx, cy); if (cc.blocksSight || (!cc.walkable && !allTileKinds()[cc.kind].water)) return expo; }
        return 0;
    };
    auto score0 = score; auto score2 = [&](int x, int y) { return score0(x, y) + tscore(x, y) - (groundAt(x, y, 1) ? 30 : 0); };
    int here = score2(u.x, u.y); int best = here + 14; bx = by = -1;   // only move for a real improvement
    int keepSel = cb.selectedUnit; cb.selectedUnit = ui; combatComputeReachable(); cb.selectedUnit = keepSel;
    for (auto& p : cb.reachable) {
        if (chebyshev(p.first, p.second, t.x, t.y) > range || !combatLOS(p.first, p.second, t.x, t.y)) continue;
        int s = score2(p.first, p.second) - chebyshev(p.first, p.second, u.x, u.y) * 2;
        if (s > best) { best = s; bx = p.first; by = p.second; }
    }
    cb.reachDirty = true;
    return bx >= 0;
}
std::vector<std::pair<int, int>> Game::aiWalkTo(int ui, int x, int y) {
    const Combatant& u = cb.units[ui]; std::vector<std::pair<int, int>> blocked;
    for (auto& o : cb.units) if (o.ch.alive && &o != &u) { blocked.push_back({o.x, o.y}); if (o.ch.size == 2) { blocked.push_back({o.x + 1, o.y}); blocked.push_back({o.x, o.y + 1}); blocked.push_back({o.x + 1, o.y + 1}); } }
    for (auto& n : map->npcs) if (!n.hidden) blocked.push_back({n.x, n.y});
    auto path = findPath(*map, u.x, u.y, x, y, blocked, 40, false);
    std::vector<std::pair<int, int>> walk; int spent = 0; for (auto& p : path) { spent += tileMoveCost(p.first, p.second); if (spent > u.ch.move) break; walk.push_back(p); }
    return walk;
}
// Flee: a whole turn, nobody in melee reach, and a roll. true = the attempt was made (escaped, or failed and the turn ended).
bool Game::combatTryFlee(int ui) {
    Combatant& u = cb.units[ui];
    bool adjacent = false; for (auto& o : cb.units) if (o.ch.alive && !o.friendly()) for (auto& p : cb.units) if (p.ch.alive && p.friendly() && chebyshev(o.x, o.y, p.x, p.y) <= 1) adjacent = true;
    if (adjacent) { log("Can't flee while engaged in melee!"); return false; }
    if (u.ch.ap < u.ch.maxAP(*gd)) { log("Fleeing takes a whole turn - try it before acting."); return false; }
    // base 20% + DEX; every enemy that can see the party and is within 5 tiles makes it harder; bosses nearly pin you down
    int chance = 20 + u.ch.stat(ST_DEX, *gd) * 2; int watchers = 0; bool boss = false;
    for (auto& o : cb.units) if (o.ch.alive && !o.friendly()) { int best = 99; for (auto& p : cb.units) if (p.ch.alive && p.friendly()) best = std::min(best, chebyshev(o.x, o.y, p.x, p.y)); if (best <= 5) watchers++; if (o.ch.boss) boss = true; }
    chance -= watchers * 12; if (boss) chance -= 25; if (u.ch.hasEffect("haste")) chance += 20; if (stealthSteps > 0) chance += 15;
    chance = clampi(chance, 5, 75);
    if (rng.chance(chance)) { cb.ending = true; cb.fled = true; cb.endTimer = 0.3f; gAudio.play("flee"); int drop = party.gold / 20; if (drop > 0) { party.gold -= drop; log(fmt("You escape, dropping %d gold in the scramble.", drop)); } }
    else { log(fmt("Failed to escape (%d%% chance). The enemy closes in!", chance)); u.ch.ap = 0; u.ch.move = 0; combatEndTurn(); }
    return true;
}

void Game::combatAI(float dt) {
    Combatant& u = cb.units[cb.selectedUnit];
    if (!u.ch.alive) { combatEndTurn(); return; }
    // party members on auto play to their spells: healers heal, casters cast (monsters use their ai= from creatures.txt)
    std::string uai = u.ch.ai;
    if (u.partyIndex >= 0) { bool heals = false, hurts = false; for (auto& sid : u.ch.spells) if (auto* sp = gd->spell(sid)) { if (sp->heal > 0) heals = true; if (sp->dmgMax > 0) hurts = true; } uai = heals ? "support" : hurts ? "caster" : "melee"; }
    // walking animation along aiPath
    if (!cb.aiPath.empty()) {
        cb.stepTimer -= dt;
        if (cb.stepTimer <= 0) {
            cb.stepTimer = settings && settings->fastCombat ? 0.04f : 0.09f;
            auto p = cb.aiPath.front(); cb.aiPath.erase(cb.aiPath.begin());
            if (tileMoveCost(p.first, p.second) > u.ch.move) { cb.aiPath.clear(); u.ch.move = 0; return; }   // not enough movement left for that rough tile
            if (u.ch.size == 2) { bool ok = true; for (int yy = 0; yy < 2; yy++) for (int xx = 0; xx < 2; xx++) { int o = combatUnitAt(p.first + xx, p.second + yy); if (!map->walkable(p.first + xx, p.second + yy) || (o >= 0 && o != cb.selectedUnit)) ok = false; } if (!ok) { cb.aiPath.clear(); return; } }
            combatStepUnit(cb.selectedUnit, p.first, p.second);
            if (!u.ch.alive || cb.ending) { cb.aiPath.clear(); return; }
            if (u.ch.move <= 0) cb.aiPath.clear();
        }
        return;
    }
    cb.aiTimer -= dt;
    if (cb.aiTimer > 0) return;
    cb.aiTimer = settings && settings->fastCombat ? 0.12f : 0.4f;
    if (cb.aiAction > 6) { combatEndTurn(); return; }
    cb.aiAction++;
    if (u.rift) { combatEndTurn(); return; }
    // 1.4.0 (#63) a draught when badly hurt - Hard: every living foe, Normal: leaders and bosses, Story: never; once a fight
    if (!u.friendly() && !u.potionUsed && !u.summoned && !u.ch.mechanical && !u.ch.undead && u.ch.ap >= 3 && u.ch.hp * 100 < u.ch.maxHP(*gd) * 35 && (aiSmart() >= 2 || (aiSmart() == 1 && (u.ch.boss || u.ch.role == "leader")))) {
        u.potionUsed = true; u.ch.ap -= 3; int h = u.ch.maxHP(*gd) * (aiSmart() >= 2 ? 35 : 20) / 100; u.ch.hp = std::min(u.ch.maxHP(*gd), u.ch.hp + h);
        addFloat(u.x, u.y, "+" + std::to_string(h), C::Green); cb.log.push_back(u.ch.name + " gulps down a healing draught."); gAudio.play("potion"); return;
    }
    // 1.4.0 (#63 routed): run for it - the reachable tile farthest from every foe
    if (u.ch.hasEffect("routed")) {
        if (u.ch.move <= 0) { combatEndTurn(); return; }
        int keepSel = cb.selectedUnit; combatComputeReachable(); cb.selectedUnit = keepSel; cb.reachDirty = true;
        int bx = -1, by = -1, bd = -1;
        for (auto& r : cb.reachable) { int dmin = 99; for (auto& o : cb.units) if (o.ch.alive && o.friendly() != u.friendly()) dmin = std::min(dmin, chebyshev(r.first, r.second, o.x, o.y)); if (dmin > bd) { bd = dmin; bx = r.first; by = r.second; } }
        if (bx >= 0) { auto pth = aiWalkTo(cb.selectedUnit, bx, by); if (!pth.empty()) { cb.aiPath = pth; cb.stepTimer = 0; return; } }
        combatEndTurn(); return;
    }
    // 1.4.0 (#79) snuff=1: put out the lights - a lit brazier next to it costs 2 AP, one within reach is walked to
    if (u.ch.snuff && map->dark && u.ch.ap >= 2) {
        auto snuffable = [&](int x, int y) { if (!map->inBounds(x, y)) return false; const TileKind& k = allTileKinds()[map->at(x, y).kind]; return !k.snuffTo.empty() && k.light > 0 && tileKind(k.snuffTo); };
        for (int yy = u.y - 1; yy <= u.y + 1; yy++) for (int xx = u.x - 1; xx <= u.x + 1; xx++) if (snuffable(xx, yy)) {
            std::string to = allTileKinds()[map->at(xx, yy).kind].snuffTo; changeCell(xx, yy, to); u.ch.ap -= 2;
            addFloat(xx, yy, "snuffed", C::Purple); cb.log.push_back(u.ch.name + " snuffs out a light! The dark closes in."); log(u.ch.name + " puts out a light."); return; }
        if (u.ch.move > 0 && cb.aiAction <= 2 && rng.chance(70)) {
            int bx = -1, by = -1, bd = 99;
            for (int yy = u.y - 6; yy <= u.y + 6; yy++) for (int xx = u.x - 6; xx <= u.x + 6; xx++) if (snuffable(xx, yy)) for (int ay = yy - 1; ay <= yy + 1; ay++) for (int ax = xx - 1; ax <= xx + 1; ax++) {
                if (!map->walkable(ax, ay) || combatUnitAt(ax, ay) >= 0) continue; int d = chebyshev(ax, ay, u.x, u.y); if (d < bd) { bd = d; bx = ax; by = ay; } }
            if (bx >= 0 && bd <= u.ch.move) { auto pth = aiWalkTo(cb.selectedUnit, bx, by); if (!pth.empty() && pth.back().first == bx && pth.back().second == by) { cb.aiPath = pth; cb.stepTimer = 0; return; } }
        }
    }
    // the party on auto (dev console 'auto', the playthrough bot): drink a potion when badly hurt, pick up a fallen friend
    if (u.partyIndex >= 0 && autoCombat) {
        auto bestPotion = [&](bool revive) { std::string id; int best = 0; for (auto& sl : party.inventory) { const ItemDef* d = gd->item(sl.id); if (!d || d->type != IT_CONSUMABLE || sl.count <= 0) continue; int v = revive ? (d->effect == "revive" ? 1 : 0) : d->heal; if (v > best) { best = v; id = sl.id; } } return id; };
        for (size_t i = 0; i < cb.units.size(); i++) { auto& o = cb.units[i]; if (o.partyIndex < 0 || o.ch.alive) continue; std::string rv = bestPotion(true); if (!rv.empty() && u.ch.ap >= itemAP(*gd->item(rv)) && chebyshev(u.x, u.y, o.x, o.y) <= 1) { combatUseItem(cb.selectedUnit, rv, (int)i); return; } }
        if (u.ch.hp * 100 < u.ch.maxHP(*gd) * 35) { std::string hp = bestPotion(false); if (!hp.empty() && u.ch.ap >= itemAP(*gd->item(hp))) { combatUseItem(cb.selectedUnit, hp, cb.selectedUnit); return; } }
    }
    // choose target: nearest alive opponent with LOS preference
    auto isFoe = [&](const Combatant& o) { return o.ch.alive && o.friendly() != u.friendly(); };
    // 1.5.0 (#92): what this unit's weapon does to a foe (resist %; 100 = can't hurt it)
    auto wres = [&](const Combatant& o) { int lo, hi, ty; u.ch.attackDamage(*gd, lo, hi, ty); for (auto& e : u.ch.effects) if (startsWith(e.id, "imbue_")) ty = dmgTypeFromName(e.id.substr(6));
        int r = o.ch.resist(ty, *gd); std::string tr = u.ch.trait(*gd); if (ty == DT_PHYS && !tr.empty()) { auto f = o.ch.traitRes.find(tr); if (f != o.ch.traitRes.end()) r = f->second; } return r; };
    if (u.partyIndex >= 0 && autoCombat) {   // the party on auto: oil the weapon with something that works, or get away
        bool anyHurt = false; for (auto& o : cb.units) if (isFoe(o) && wres(o) < 100) anyHurt = true;
        bool spells = false; for (auto& sid : u.ch.spells) if (auto* sp = gd->spell(sid)) if (sp->dmgMax > 0 && u.ch.mp >= sp->cost) spells = true;
        if (!anyHurt && !spells) {
            for (auto& sl : party.inventory) { const ItemDef* d = gd->item(sl.id); if (!d || sl.count <= 0 || !startsWith(d->effect, "imbue_") || u.ch.ap < itemAP(*d)) continue;
                int ty = dmgTypeFromName(d->effect.substr(6)); bool works = false; for (auto& o : cb.units) if (isFoe(o) && o.ch.resist(ty, *gd) < 100) works = true;
                if (works) { combatUseItem(cb.selectedUnit, sl.id, cb.selectedUnit); return; } }
            if (u.ch.ap >= u.ch.maxAP(*gd) && combatTryFlee(cb.selectedUnit)) return;
            u.ch.addEffect("defend", 1, 0); combatEndTurn(); return;
        }
    }
    // target choice: close, hittable right now, and wounded beats healthy (they focus fire); a little noise keeps it from being predictable
    int best = -1, bestScore = 1 << 30;
    bool noAmmo = false; { const ItemDef* w = u.ch.weapon(*gd); noAmmo = u.partyIndex >= 0 && w && !w->ammo.empty() && !party.hasItem(w->ammo); }
    int myRange = noAmmo ? 1 : u.ch.attackRange(*gd);
    for (size_t i = 0; i < cb.units.size(); i++) {
        const Combatant& o = cb.units[i]; if (!isFoe(o)) continue;
        int d = unitDist(o, u.x, u.y); if (u.ch.size == 2) d = std::max(0, d - 1);
        int sc = d * 10;
        if (d <= myRange && targetable(u, o)) sc -= 35;
        if (smokeAt(o.x, o.y) && d > 1) sc += 30;                                     // 1.4.0: hidden in smoke
        int mh = std::max(1, o.ch.maxHP(*gd)); int pct = o.ch.hp * 100 / mh;
        if (pct < 50) sc -= 15; if (pct < 25) sc -= 15;
        if (uai == "coward" || uai == "ranged" || uai == "caster") sc -= (100 - pct) / 5;
        if (o.ch.hasEffect("defend")) sc += 10;
        { int lo = 0, hi = 0, ty = 0; u.ch.attackDamage(*gd, lo, hi, ty); if (o.ch.hp <= (lo + hi) / 2 && d <= myRange + u.ch.move) sc -= 25; }   // can finish it this turn
        if ((int)i == (u.friendly() ? cb.focusParty : cb.focusEnemy)) sc -= u.friendly() ? 12 : (aiSmart() == 0 ? 5 : aiSmart() == 1 ? 12 : 24);   // the side piles onto one target (1.4.0: harder on Hard)
        if (!u.friendly() && aiSmart() >= 2 && (o.ch.hasEffect("stun") || o.ch.hasEffect("freeze") || exposed(u, o))) sc -= 10;   // Hard: goes for the helpless and the exposed
        if (o.summoned) sc += 8;                                                      // the summoner matters more than the summon
        { int r = wres(o); if (r >= 100) sc += 200; else if (r >= 50) sc += 15; else if (r < 0) sc -= 15; }   // 1.5.0 (#92): hit what the weapon can hurt
        { bool wards = false; for (auto& w : cb.units) if (w.ch.alive && &w != &o && w.friendly() == o.friendly() && !w.ch.ward.empty() && w.ch.ward == o.ch.creatureId) wards = true; if (wards) sc -= 25; }   // 1.4.0: kill whatever shields the boss first
        if (map->dark && !u.ch.darkvision && !lit(o.x, o.y)) sc += 12;
        sc += rng.range(0, !u.friendly() && aiSmart() == 0 ? 20 : 8);
        if (sc < bestScore) { bestScore = sc; best = (int)i; }
    }
    if (best < 0) { combatEndTurn(); return; }
    Combatant& t = cb.units[best];
    int range = myRange;
    int dist = unitDist(t, u.x, u.y);
    if (u.ch.size == 2) dist = std::max(0, dist - 1);
    bool los = targetable(u, t);
    if (los && u.ch.trait(*gd) == "reach" && dist == 2 && !(t.x == u.x || t.y == u.y || abs(t.x - u.x) == abs(t.y - u.y))) los = false;   // spears: straight lines only
    // 1.4.0 (#62) a boss with windup= announces its big attack a turn ahead (from round 2, every third round at most)
    if (!u.friendly() && !u.ch.windup.empty() && cb.round >= 2 && cb.round - u.lastWindup >= 3 && u.ch.ap >= 4 && dist <= 8 && combatLOS(u.x, u.y, t.x, t.y)) {
        bool pending = false; for (auto& w : cb.windups) if (w.unit == cb.selectedUnit) pending = true;
        if (!pending && gd->spell(u.ch.windup)) { windupStart(cb.selectedUnit, best); combatEndTurn(); return; }
    }
    // casters & support
    if ((uai == "caster" || uai == "support" || uai == "boss") && !u.ch.spells.empty() && u.ch.ap >= 4) {
        // heal ally if support
        if (uai == "support" || uai == "boss") {
            for (auto& sid : u.ch.spells) { const SpellDef* sp = gd->spell(sid); if (!sp || sp->heal <= 0 || u.ch.mp < sp->cost || u.ch.ap < sp->ap) continue;
                int low = -1; for (size_t i = 0; i < cb.units.size(); i++) { auto& o = cb.units[i]; if (!o.ch.alive || isFoe(o)) continue; if (o.ch.hp < o.ch.maxHP(*gd) / 2 && chebyshev(u.x, u.y, o.x, o.y) <= sp->range) { if (low < 0 || o.ch.hp < cb.units[low].ch.hp) low = (int)i; } }
                if (low >= 0) { combatCast(cb.selectedUnit, sid, cb.units[low].x, cb.units[low].y); return; } }
            // 1.4.0 (#63) support also buffs: bless / strength / shield / haste on an ally that lacks it (fighters first)
            for (auto& sid : u.ch.spells) { const SpellDef* sp = gd->spell(sid); if (!sp || sp->heal > 0 || sp->dmgMax > 0 || sp->target != "ally" || sp->effect.empty() || sp->effect == "cure" || sp->effect == "revive" || u.ch.mp < sp->cost || u.ch.ap < sp->ap || !rng.chance(60)) continue;
                int pick = -1, ps = -999; for (size_t i = 0; i < cb.units.size(); i++) { auto& o = cb.units[i]; if (!o.ch.alive || isFoe(o) || o.ch.hasEffect(sp->effect) || chebyshev(u.x, u.y, o.x, o.y) > sp->range) continue;
                    int s = 0; for (auto& f : cb.units) if (isFoe(f)) s = std::max(s, 10 - chebyshev(f.x, f.y, o.x, o.y)); if (o.ch.boss || o.ch.role == "leader") s += 4; if (s > ps) { ps = s; pick = (int)i; } }
                if (pick >= 0) { combatCast(cb.selectedUnit, sid, cb.units[pick].x, cb.units[pick].y); return; } }
        }
        // summoners call help early in a fight, while they have few summons out
        for (auto& sid : u.ch.spells) { const SpellDef* sp = gd->spell(sid); if (!sp || sp->summon.empty() || u.ch.mp < sp->cost || u.ch.ap < sp->ap || summonsOf(cb.selectedUnit) >= 2 || !rng.chance(cb.round <= 2 ? 100 : 40)) continue;
            int sx = u.x, sy = u.y; if (dist > 2) { sx = u.x + (t.x > u.x) - (t.x < u.x); sy = u.y + (t.y > u.y) - (t.y < u.y); }   // between itself and the foe
            combatCast(cb.selectedUnit, sid, sx, sy); return; }
        std::vector<std::string> dmgSpells;
        for (auto& sid : u.ch.spells) {
            const SpellDef* sp = gd->spell(sid);
            if (!sp || !(sp->dmgMax > 0 || (sp->target == "enemy" && !sp->effect.empty())) || u.ch.mp < sp->cost || u.ch.ap < sp->ap || dist > sp->range || !los) continue;
            if (sp->area > 0 || !sp->shape.empty() || sp->chainJumps > 0) {   // monsters aren't suicidal: skip a blast that would hit more of their own than of yours
                int own = 0, foes = 0; for (int v : (sp->chainJumps > 0 ? chainTargets(cb.selectedUnit, *sp, best) : spellVictims(cb.selectedUnit, *sp, t.x, t.y))) { if (cb.units[v].friendly() == u.friendly()) own++; else foes++; }
                if (own > 0 && own >= foes) continue;
            }
            dmgSpells.push_back(sid);
        }
        // 1.4.0 (#59): against a target in cover, casters use what cover doesn't stop (area spells); Hard also cashes in combos
        if (dmgSpells.size() > 1 && aiSmart() >= 1) {
            std::vector<std::string> area, single, combo;
            for (auto& sid : dmgSpells) { const SpellDef* sp = gd->spell(sid); (singleTargetSpell(*sp) ? single : area).push_back(sid); if (!comboPreview(t, sp->dmgType).empty()) combo.push_back(sid); }
            if (aiSmart() >= 2 && !combo.empty()) dmgSpells = combo;
            else if (!single.empty() && !area.empty() && spellCoverPct(u, t, *gd->spell(single[0])) > 0) dmgSpells = area;
        }
        if (!dmgSpells.empty() && rng.chance(80)) { (u.friendly() ? cb.focusParty : cb.focusEnemy) = best; combatCast(cb.selectedUnit, rng.pick(dmgSpells), t.x, t.y); return; }
    }
    if (uai == "coward" && dist <= 2 && u.ch.move > 0 && rng.chance(60)) {
        // step away
        int bx = u.x, by = u.y, bd = dist;
        const int DX[4] = {1, -1, 0, 0}, DY[4] = {0, 0, 1, -1};
        for (int k = 0; k < 4; k++) { int nx = u.x + DX[k], ny = u.y + DY[k]; if (!map->walkable(nx, ny) || combatUnitAt(nx, ny) >= 0) continue; int d = chebyshev(nx, ny, t.x, t.y); if (d > bd) { bd = d; bx = nx; by = ny; } }
        if (bx != u.x || by != u.y) { cb.aiPath.push_back({bx, by}); return; }
    }
    // archers and gunners back off from melee before shooting (and eat the attack of opportunity for it)
    if (range > 1 && dist <= 1 && u.ch.move > 0 && uai != "boss" && rng.chance(55)) {
        int bx = u.x, by = u.y, bd = dist; const int DX[4] = {1, -1, 0, 0}, DY[4] = {0, 0, 1, -1};
        for (int k = 0; k < 4; k++) { int nx = u.x + DX[k], ny = u.y + DY[k]; if (!map->walkable(nx, ny) || combatUnitAt(nx, ny) >= 0 || map->chestAt(nx, ny)) continue; int dd = chebyshev(nx, ny, t.x, t.y); if (dd > bd && combatLOS(nx, ny, t.x, t.y)) { bd = dd; bx = nx; by = ny; } }
        if (bx != u.x || by != u.y) { cb.aiPath.push_back({bx, by}); return; }
    }
    // ranged units in the open step into cover first (once, before they have moved), if they can still shoot from there
    if (range > 1 && u.ch.move >= u.ch.maxMove(*gd) && u.ch.ap >= u.ch.attackAP(*gd) && uai != "boss" && u.ch.size == 1) {
        int bx, by; if (aiCoverSpot(cb.selectedUnit, best, bx, by)) { auto pth = aiWalkTo(cb.selectedUnit, bx, by); if (!pth.empty()) { cb.aiPath = pth; cb.stepTimer = 0; return; } }
    }
    if (dist <= range && los && u.ch.ap >= u.ch.attackAP(*gd)) { (u.friendly() ? cb.focusParty : cb.focusEnemy) = best; combatAttack(cb.selectedUnit, best, false); return; }
    // 1.4.0 (#63) role=sniper: covers its lane on overwatch and rarely moves
    if (u.ch.role == "sniper" && range > 1 && u.ch.ap >= u.ch.attackAP(*gd) && !u.overwatch && !rng.chance(dist > range + 6 ? 40 : 10)) { u.overwatch = true; u.ch.ap = 0; cb.log.push_back(u.ch.name + " covers the lane (overwatch)."); addFloat(u.x, u.y, "overwatch", C::Yellow); combatEndTurn(); return; }
    // 1.4.0 (#63) role=guard: stays by the side's archers and shields them (allies next to it count as in half cover)
    if (u.ch.role == "guard" && dist > 2 && u.ch.move > 0) {
        int ward = -1, wd = 99; for (size_t i = 0; i < cb.units.size(); i++) { auto& o = cb.units[i]; if ((int)i == cb.selectedUnit || !o.ch.alive || isFoe(o) || o.ch.attackRange(*gd) <= 1) continue; int d = chebyshev(o.x, o.y, u.x, u.y); if (d < wd) { wd = d; ward = (int)i; } }
        if (ward >= 0) {
            auto& w = cb.units[ward];
            if (chebyshev(w.x, w.y, u.x, u.y) <= 1) { u.ch.addEffect("defend", 1, 0); u.ch.ap = 0; addFloat(u.x, u.y, "guards", C::LtGray); cb.log.push_back(u.ch.name + " raises a shield by " + w.ch.name + "."); combatEndTurn(); return; }
            int keepSel = cb.selectedUnit; combatComputeReachable(); cb.selectedUnit = keepSel; cb.reachDirty = true;
            int bx = -1, by = -1, bs = 999; for (auto& r : cb.reachable) { if (chebyshev(r.first, r.second, w.x, w.y) != 1) continue; int s = chebyshev(r.first, r.second, t.x, t.y); if (s < bs) { bs = s; bx = r.first; by = r.second; } }
            if (bx >= 0) { auto pth = aiWalkTo(cb.selectedUnit, bx, by); if (!pth.empty()) { cb.aiPath = pth; cb.stepTimer = 0; return; } }
        }
    }
    // 1.4.0 (#63) Hard (and sometimes Normal): melee foes walk round to flank instead of queueing up in front
    if (!u.friendly() && range <= 1 && u.ch.size == 1 && u.ch.move > 0 && dist > 1 && (aiSmart() >= 2 || (aiSmart() == 1 && rng.chance(40)))) {
        int keepSel = cb.selectedUnit; combatComputeReachable(); cb.selectedUnit = keepSel; cb.reachDirty = true;
        int bx = -1, by = -1, bs = -999;
        for (auto& r : cb.reachable) {
            if (unitDist(t, r.first, r.second) != 1) continue;
            int s = 0; for (auto& o : cb.units) if (&o != &u && o.ch.alive && !isFoe(o) && unitDist(t, o.x, o.y) <= 1) { s += 20; if ((r.first - t.x) * (o.x - t.x) + (r.second - t.y) * (o.y - t.y) < 0) s += 10; }
            if (groundAt(r.first, r.second, 1)) s -= 40;
            s -= chebyshev(r.first, r.second, u.x, u.y); if (s > bs) { bs = s; bx = r.first; by = r.second; }
        }
        if (bx >= 0 && bs >= 15) { auto pth = aiWalkTo(cb.selectedUnit, bx, by); if (!pth.empty() && pth.back().first == bx && pth.back().second == by) { cb.aiPath = pth; cb.stepTimer = 0; return; } }
    }
    if (range > 1 && !(dist <= range && los) && u.ch.ap >= u.ch.attackAP(*gd) && !u.overwatch && rng.chance(u.ch.move > 0 ? 35 : 80)) { u.overwatch = true; u.ch.ap = 0; cb.log.push_back(u.ch.name + " goes on overwatch."); addFloat(u.x, u.y, "overwatch", C::Yellow); combatEndTurn(); return; }
    if (u.ch.move > 0) {
        // move toward target
        std::vector<std::pair<int, int>> blocked;
        for (auto& o : cb.units) if (o.ch.alive && &o != &u) { blocked.push_back({o.x, o.y}); if (o.ch.size == 2) { blocked.push_back({o.x + 1, o.y}); blocked.push_back({o.x, o.y + 1}); blocked.push_back({o.x + 1, o.y + 1}); } }
        for (auto& n : map->npcs) if (!n.hidden) blocked.push_back({n.x, n.y});
        auto path = findPath(*map, u.x, u.y, t.x, t.y, blocked, 150, true);
        if (path.empty()) {
            // try to find a path to any tile adjacent to target
            int bestLen = 999; std::vector<std::pair<int, int>> bestPath;
            int ts = std::max(1, t.ch.size);   // a 2x2 creature can be reached from any tile around its whole body
            for (int dy = -1; dy <= ts; dy++) for (int dx = -1; dx <= ts; dx++) { if (dx >= 0 && dx < ts && dy >= 0 && dy < ts) continue; int gx = t.x + dx, gy = t.y + dy; if (!map->walkable(gx, gy) || combatUnitAt(gx, gy) >= 0) continue; auto p = findPath(*map, u.x, u.y, gx, gy, blocked, 150, false); if (!p.empty() && (int)p.size() < bestLen) { bestLen = (int)p.size(); bestPath = p; } }
            path = bestPath;
        }
        if (!path.empty()) {
            if (path.back().first == t.x && path.back().second == t.y) path.pop_back();
            // stop once within range (for ranged units)
            std::vector<std::pair<int, int>> walk;
            int spent = 0;
            for (auto& p : path) { spent += tileMoveCost(p.first, p.second); if (spent > u.ch.move) break; walk.push_back(p); if (range > 1 && chebyshev(p.first, p.second, t.x, t.y) <= range && combatLOS(p.first, p.second, t.x, t.y)) break; }
            if (!walk.empty()) { cb.aiPath = walk; cb.stepTimer = 0; return; }
        }
        // 1.4.0: no way to walk up to it (a turret behind a counter, a gap in a wall) - shooters and casters look for a
        // reachable tile they can hit it from instead of standing still all fight
        int reachR = range;
        for (auto& sid : u.ch.spells) if (const SpellDef* sp = gd->spell(sid)) if (sp->dmgMax > 0 && u.ch.mp >= sp->cost) reachR = std::max(reachR, sp->range);
        if (reachR > 1 && u.ch.size == 1) {
            combatComputeReachable(); cb.reachDirty = true;
            int bx = -1, by = -1, bs = 1 << 30;
            for (auto& r : cb.reachable) { if (chebyshev(r.first, r.second, t.x, t.y) > reachR || !combatLOS(r.first, r.second, t.x, t.y) || groundAt(r.first, r.second, 1)) continue; int s = chebyshev(r.first, r.second, u.x, u.y); if (s < bs) { bs = s; bx = r.first; by = r.second; } }
            if (bx >= 0) { auto pth = aiWalkTo(cb.selectedUnit, bx, by); if (!pth.empty()) { cb.aiPath = pth; cb.stepTimer = 0; return; } }
        }
    }
    combatEndTurn();
}

// ---------------- player input ----------------
// Runs one of the action-bar commands (shared by the bar, the action menu and hotkeys).
static bool g_dummy;
void Game::updateCombat(InputState& in, float dt) {
    for (auto& u : cb.units) { u.hitFlash = std::max(0.0f, u.hitFlash - dt); float k = std::min(1.0f, dt * 12); u.offX -= u.offX * k; u.offY -= u.offY * k; }
    if (cb.banner > 0) cb.banner -= dt;
    // a mouse moved at any point (during an enemy turn or an animation too) means the mouse has the cursor from now on
    if (in.gamepadActive) cb.cursorMouse = false; else if (in.mouseDX || in.mouseDY) cb.cursorMouse = true;
    // the player's input waits only while a swing or shot is in the air or during the short beat after a kill;
    // the death burst, spell flashes and damage numbers play on without holding anything up
    bool swingInFlight = false; for (auto& a : cb.anims) if (a.kind == 0 || a.kind == 2) swingInFlight = true;
    bool inputLocked = cb.hitstop > 0 || swingInFlight;
    // 1.1.3: Esc (or the bar's Menu button) pauses the fight at any moment - on an enemy turn or mid-animation too -
    // unless it is backing out of a target pick / list / the action menu on your own turn
    {
        bool myInput = cb.playerTurn && !autoCombat && cb.selectedUnit >= 0 && !cb.ending;
        bool menuClick = in.mousePressed[0] && in.mouseY <= 24 && in.mouseX >= SCREEN_W - combatMenuButtonW(ctx) - 4;
        if (menuClick || (actP(in, A_CANCEL, true) && !(myInput && cb.mode != 0))) { in.eatMouse(); openPause(); return; }
    }
    if (cb.bufT > 0) cb.bufT -= dt;
    if (inputLocked && cb.playerTurn && !autoCombat && cb.selectedUnit >= 0) {
        if (in.mousePressed[0]) { cb.bufClick = true; cb.bufKey = 0; cb.bufT = 0.6f; cb.bufUnit = cb.selectedUnit; }
        else if (in.lastKey != K_NONE && in.pressed[in.lastKey]) { cb.bufKey = in.lastKey; cb.bufClick = false; cb.bufT = 0.6f; cb.bufUnit = cb.selectedUnit; }
    } else if (!inputLocked && cb.bufT > 0 && (cb.bufClick || cb.bufKey)) {
        if (cb.playerTurn && cb.bufUnit == cb.selectedUnit && !in.mousePressed[0] && in.lastKey == K_NONE) {
            if (cb.bufClick) in.mousePressed[0] = true; else { in.pressed[cb.bufKey] = true; in.lastKey = cb.bufKey; }
        }
        cb.bufClick = false; cb.bufKey = 0; cb.bufT = 0;
    }
    if (cb.hitstop > 0) { cb.hitstop -= dt; return; }   // the beat after a kill / crit: everything holds still
    for (auto& a : cb.anims) a.t += dt;
    cb.anims.erase(std::remove_if(cb.anims.begin(), cb.anims.end(), [](const CombatAnim& a) { return a.t >= a.dur; }), cb.anims.end());
    if (!cb.ending) combatCheckEnd();                       // combat ends the moment the last enemy falls
    if (cb.ending) { cb.endTimer -= dt; if (cb.endTimer <= 0 && cb.anims.empty()) combatFinish(); return; }
    if (cb.selectedUnit < 0) { combatNextTurn(); return; }
    Combatant& u = cb.units[cb.selectedUnit];
    // clicking a portrait in the turn-order strip pans the camera to that unit
    if (in.mousePressed[0] && in.mouseX >= SCREEN_W - 30 && in.mouseY >= TURN_STRIP_Y && in.mouseY < TURN_STRIP_Y + 10 * 26) {
        int slot = (in.mouseY - TURN_STRIP_Y) / 26, shown = 0;
        for (int k = 0; k < (int)cb.order.size(); k++) { int idx = cb.order[(cb.turnPos + k) % cb.order.size()]; if (!cb.units[idx].ch.alive) continue; if (shown == slot) { camFree = true; camCenterOn(cb.units[idx].x, cb.units[idx].y); cb.selX = cb.units[idx].x; cb.selY = cb.units[idx].y; break; } shown++; }
        in.eatMouse(); return;
    }
    if (!cb.playerTurn || autoCombat) { if (cb.anims.empty()) combatAI(dt); return; }
    // walking animation for the player too
    if (!cb.aiPath.empty()) {
        cb.stepTimer -= dt;
        if (cb.stepTimer <= 0) { cb.stepTimer = 0.07f; auto p = cb.aiPath.front(); cb.aiPath.erase(cb.aiPath.begin()); if (tileMoveCost(p.first, p.second) > u.ch.move) { cb.aiPath.clear(); combatComputeReachable(); cb.selX = u.x; cb.selY = u.y; return; } combatStepUnit(cb.selectedUnit, p.first, p.second); if (!u.ch.alive || cb.ending) { cb.aiPath.clear(); if (!cb.ending) combatEndTurn(); return; } if (cb.aiPath.empty()) { combatComputeReachable(); cb.reachDirty = false; cb.selX = u.x; cb.selY = u.y; } }
        return;
    }
    // somebody fell (or was revived) since the move area was worked out: recompute, so a path through a fresh corpse opens up
    if (cb.reachDirty) { combatComputeReachable(); cb.reachDirty = false; }
    if (swingInFlight) return;
    const bool CB = true;
    if (actP(in, A_CANCEL, CB)) { cb.mode = 0; return; }   // (with nothing open Esc pauses, above)
    // ---- action commands (bar / menu / hotkeys) ----
    auto runCommand = [&](int cmd) {
        switch (cmd) {
            case 0: cb.aimed = false; cb.mode = 0; log("Normal attack."); break;
            case 1: cb.aimed = true; cb.mode = 0; log("Aimed shot: -15% to hit, +30% crit, +2 AP."); break;
            case 2: if (u.ch.spells.empty()) log("No spells known."); else { cb.mode = 4; cb.menuSel = 0; cb.menuScroll = 0; } break;
            case 3: { bool any = false; for (auto& it : party.inventory) if (gd->item(it.id) && gd->item(it.id)->type == IT_CONSUMABLE) any = true; if (!any) log("No usable items."); else { cb.mode = 5; cb.menuSel = 0; cb.menuScroll = 0; } } break;
            case 4: u.ch.addEffect("defend", 1, 0); u.ch.ap = 0; log(u.ch.name + " takes a defensive stance."); combatEndTurn(); break;
            case 5: if (u.ch.ap < u.ch.attackAP(*gd)) { log("Overwatch needs enough AP for one attack."); break; } u.overwatch = true; u.ch.ap = 0; log(u.ch.name + " is on overwatch: the first enemy to move into range and sight gets shot (-10% to hit)."); addFloat(u.x, u.y, "overwatch", C::Yellow); combatEndTurn(); break;
            case 6: combatEndTurn(); break;
            case 7: { if (!combatTryFlee(cb.selectedUnit)) cb.mode = 0; } break;
        }
        if (cb.mode == 3) cb.mode = 0;
    };
    // hotkeys work in every sub-mode
    if (actP(in, A_ENDTURN, CB)) { combatEndTurn(); return; }
    if (actP(in, A_ATTACKMODE, CB)) { runCommand(cb.aimed ? 0 : 1); return; }
    if (actP(in, A_SPELLS, CB)) { if (cb.mode == 4) cb.mode = 0; else runCommand(2); return; }
    if (actP(in, A_ITEMS, CB)) { if (cb.mode == 5) cb.mode = 0; else runCommand(3); return; }
    if (actP(in, A_DEFEND, CB)) { runCommand(4); return; }
    if (actP(in, A_OVERWATCH, CB)) { runCommand(5); return; }
    if (actP(in, A_FLEE, CB)) { runCommand(7); return; }
    if (actP(in, A_ACTIONMENU, CB)) { cb.mode = cb.mode == 3 ? 0 : 3; cb.menuSel = 0; return; }
    // favorites
    auto useFav = [&](int slot) {
        std::string f = u.partyIndex >= 0 ? party.members[u.partyIndex].favs[slot] : "";
        if (f.empty()) { log(fmt("Favorite %d is empty: open Spells or Items, highlight one and press %d.", slot + 1, slot + 1)); return; }
        std::string kind = f.substr(0, f.find(':')), id = f.substr(f.find(':') + 1);
        if (kind == "spell") {
            const SpellDef* sp = gd->spell(id); if (!sp) return;
            if (std::find(u.ch.spells.begin(), u.ch.spells.end(), id) == u.ch.spells.end()) { log(u.ch.name + " doesn't know " + sp->name + "."); return; }
            if (!spellUsableInCombat(*sp)) { log(sp->name + " has no effect in combat."); return; }
            if (u.ch.mp < sp->cost) { log("Not enough MP."); return; } if (u.ch.ap < sp->ap) { log("Not enough AP."); return; }
            cb.pendingSpell = id;
            if (sp->target == "self") { combatCast(cb.selectedUnit, id, u.x, u.y); cb.mode = 0; } else { cb.mode = 1; log("Choose a target for " + sp->name + "."); }
        } else {
            const ItemDef* d = gd->item(id); if (!d) return;
            if (!party.hasItem(id)) { log("No " + d->name + " left."); return; }
            cb.pendingItem = id; cb.mode = 2; log("Choose a target for " + d->name + ".");
        }
    };
    // with the Spells / Items list open, a favorite key puts the highlighted entry on that hotbar slot (as in the party
    // screen) instead of using whatever is there
    if ((cb.mode == 4 || cb.mode == 5) && u.partyIndex >= 0) {
        for (int f = 0; f < 8; f++) if (actP(in, A_FAV1 + f, CB)) {
            std::vector<std::string> list;
            if (cb.mode == 4) { for (auto& id : u.ch.spells) if (auto* sp = gd->spell(id)) if (spellUsableInCombat(*sp)) list.push_back(id); } else for (auto& it : party.inventory) if (gd->item(it.id) && gd->item(it.id)->type == IT_CONSUMABLE && std::find(list.begin(), list.end(), it.id) == list.end()) list.push_back(it.id);
            if (list.empty()) return;
            const std::string& id = list[clampi(cb.menuSel, 0, (int)list.size() - 1)];
            party.members[u.partyIndex].favs[f] = (cb.mode == 4 ? "spell:" : "item:") + id;
            std::string nm = cb.mode == 4 ? (gd->spell(id) ? gd->spell(id)->name : id) : (gd->item(id) ? gd->item(id)->name : id);
            log(fmt("%s set as favorite %d for %s.", nm.c_str(), f + 1, u.ch.name.c_str())); gAudio.play("ui_click");
            return;
        }
    }
    for (int f = 0; f < 8; f++) if (actP(in, A_FAV1 + f, CB)) { useFav(f); return; }
    if (in.mousePressed[0]) { int hs = hotbarSlotAt(in.mouseX, in.mouseY, HOTBAR_Y); if (hs >= 0) { in.eatMouse(); useFav(hs); return; } }
    // top action bar clicks
    if (in.mousePressed[0] && in.mouseY <= 24) {
        int bx = 4; for (int i = 0; i < NACTIONS; i++) { int bw = actionButtonW(ctx, i); if (in.mouseX >= bx && in.mouseX < bx + bw) { in.eatMouse(); runCommand(i); return; } bx += bw + 3; }
        in.eatMouse(); return;
    }
    // action menu (mode 3)
    if (cb.mode == 3) {
        int mx = SCREEN_W / 2 - 70, my = 40;
        if (actP(in, A_UP, CB)) cb.menuSel = (cb.menuSel + NACTIONS - 1) % NACTIONS;
        if (actP(in, A_DOWN, CB)) cb.menuSel = (cb.menuSel + 1) % NACTIONS;
        for (int i = 0; i < NACTIONS; i++) if (in.mouseX >= mx && in.mouseX < mx + 140 && in.mouseY >= my + 24 + i * 20 && in.mouseY < my + 44 + i * 20 && (in.mouseDX || in.mouseDY)) cb.menuSel = i;
        bool click = in.mousePressed[0] && in.mouseX >= mx && in.mouseX < mx + 140 && in.mouseY >= my + 24 && in.mouseY < my + 24 + NACTIONS * 20;
        if (click) cb.menuSel = (in.mouseY - my - 24) / 20;
        if (actP(in, A_CONFIRM, CB) || click) { in.eatMouse(); runCommand(cb.menuSel); }
        else if (in.mousePressed[0]) { in.eatMouse(); cb.mode = 0; }
        return;
    }
    if (cb.mode == 4 || cb.mode == 5) { // pick spell / item from list
        std::vector<std::string> list;
        if (cb.mode == 4) { for (auto& id : u.ch.spells) if (auto* sp = gd->spell(id)) if (spellUsableInCombat(*sp)) list.push_back(id); } else for (auto& it : party.inventory) if (gd->item(it.id) && gd->item(it.id)->type == IT_CONSUMABLE && std::find(list.begin(), list.end(), it.id) == list.end()) list.push_back(it.id);
        int n = (int)list.size(); if (n == 0) { cb.mode = 0; log("Nothing usable in combat."); return; }
        const int VIS = 10; int mx = SCREEN_W / 2 - 120, my = 40;
        if (actP(in, A_UP, CB)) cb.menuSel = (cb.menuSel + n - 1) % n;
        if (actP(in, A_DOWN, CB)) cb.menuSel = (cb.menuSel + 1) % n;
        cb.menuSel = clampi(cb.menuSel, 0, n - 1);
        if (in.wheel && in.mouseX >= mx && in.mouseX < mx + 240) cb.menuScroll -= in.wheel;
        if (cb.menuSel < cb.menuScroll) cb.menuScroll = cb.menuSel; if (cb.menuSel >= cb.menuScroll + VIS) cb.menuScroll = cb.menuSel - VIS + 1;
        cb.menuScroll = clampi(cb.menuScroll, 0, std::max(0, n - VIS));
        for (int i = 0; i < VIS && cb.menuScroll + i < n; i++) if (in.mouseX >= mx && in.mouseX < mx + 240 && in.mouseY >= my + 24 + i * 20 && in.mouseY < my + 44 + i * 20 && (in.mouseDX || in.mouseDY)) cb.menuSel = cb.menuScroll + i;
        bool click = in.mousePressed[0] && in.mouseX >= mx && in.mouseX < mx + 240 && in.mouseY >= my + 24 && in.mouseY < my + 24 + std::min(n - cb.menuScroll, VIS) * 20;
        if (click) cb.menuSel = cb.menuScroll + (in.mouseY - my - 24) / 20;
        if (actP(in, A_CONFIRM, CB) || click) {
            in.eatMouse();
            cb.menuSel = clampi(cb.menuSel, 0, n - 1);
            if (cb.mode == 4) { const SpellDef* sp = gd->spell(list[cb.menuSel]); if (!sp) return; if (u.ch.mp < sp->cost) { log("Not enough MP."); return; } if (u.ch.ap < sp->ap) { log("Not enough AP."); return; } cb.pendingSpell = list[cb.menuSel]; if (sp->target == "self") { combatCast(cb.selectedUnit, cb.pendingSpell, u.x, u.y); cb.mode = 0; } else { cb.mode = 1; log("Choose a target for " + sp->name + "."); } }
            else { cb.pendingItem = list[cb.menuSel]; cb.mode = 2; log("Choose a target for " + gd->item(cb.pendingItem)->name + "."); }
        } else if (in.mousePressed[0] && !mouseOverUI(in)) { in.eatMouse(); cb.mode = 0; }
        return;
    }
    // ---- cursor / targeting (modes 0,1,2) ----
    int viewH = SCREEN_H - 72;
    bool overUI = mouseOverUI(in) || in.mouseY <= 24;
    bool overMap = in.mouseY < viewH && !overUI;
    if (overMap && (in.mouseDX || in.mouseDY || in.mousePressed[0])) { cb.selX = (in.mouseX + camX) / TILE; cb.selY = (in.mouseY + camY) / TILE; cb.cursorMouse = true; }
    // while the mouse has the cursor it stays on the tile under the pointer, even when the camera moves or a move or a
    // new turn puts the cursor back on the unit (those resets are for keyboard and pad players)
    if (in.gamepadActive) cb.cursorMouse = false;
    else if (cb.cursorMouse && overMap && in.mouseInWindow) { cb.selX = (in.mouseX + camX) / TILE; cb.selY = (in.mouseY + camY) / TILE; }
    bool moved = false;
    if (actP(in, A_LEFT, CB)) { cb.selX--; moved = true; } if (actP(in, A_RIGHT, CB)) { cb.selX++; moved = true; }
    if (actP(in, A_UP, CB)) { cb.selY--; moved = true; } if (actP(in, A_DOWN, CB)) { cb.selY++; moved = true; }
    if (moved) cb.cursorMouse = false;
    cb.selX = clampi(cb.selX, 0, map->w - 1); cb.selY = clampi(cb.selY, 0, map->h - 1);
    if (moved) { // keep the keyboard cursor on screen
        int sx = cb.selX * TILE - camTX, sy = cb.selY * TILE - camTY;
        if (sx < TILE * 2 || sx > SCREEN_W - TILE * 3 || sy < TILE * 2 || sy > viewH - TILE * 3) { camFree = true; camCenterOn(cb.selX, cb.selY); }
    }
    // path preview
    cb.path.clear();
    int targetUnit = combatUnitAt(cb.selX, cb.selY);
    if (cb.mode == 0 && targetUnit < 0) {
        bool reach = false; for (auto& r : cb.reachable) if (r.first == cb.selX && r.second == cb.selY) reach = true;
        if (reach) { std::vector<std::pair<int, int>> blocked; for (auto& o : cb.units) if (o.ch.alive && &o != &u) blocked.push_back({o.x, o.y}); for (auto& n : map->npcs) if (!n.hidden) blocked.push_back({n.x, n.y}); cb.path = findPath(*map, u.x, u.y, cb.selX, cb.selY, blocked, 40); }
    }
    bool confirm = actP(in, A_CONFIRM, CB) || actP(in, A_INTERACT, CB) || (in.mousePressed[0] && in.mouseY < viewH && !overUI);
    if (!confirm) return;
    in.eatMouse();
    int distSel = chebyshev(u.x, u.y, cb.selX, cb.selY);
    bool seeSel = !map->dark || visible(cb.selX, cb.selY) || distSel <= 1;
    if (targetUnit >= 0 && !cb.units[targetUnit].friendly() && !seeSel) targetUnit = -1;   // you can't pick out what you can't see
    if (cb.mode == 1) {
        const SpellDef* sp = gd->spell(cb.pendingSpell); if (!sp) { cb.mode = 0; return; }
        if (distSel > sp->range) { log("Out of range."); return; }
        if (!combatLOS(u.x, u.y, cb.selX, cb.selY)) { log("No line of sight."); return; }
        if (!seeSel && sp->utility != "light") { log("Too dark to aim there - light it up first."); return; }
        if (sp->target != "ground" && targetUnit < 0 && sp->area == 0 && sp->shape.empty() && sp->smoke <= 0) { log("No target there."); return; }
        if (targetUnit >= 0 && sp->area == 0 && sp->shape.empty() && smokeAt(cb.selX, cb.selY) && distSel > 1 && !cb.units[targetUnit].friendly()) { log("You can't make them out in the smoke."); return; }
        combatCast(cb.selectedUnit, cb.pendingSpell, cb.selX, cb.selY); cb.mode = 0; return;
    }
    if (cb.mode == 2) {
        const ItemDef* d = gd->item(cb.pendingItem); if (!d) { cb.mode = 0; return; }
        bool thrown = thrownItem(*d);
        if (thrown) {
            if (distSel > 6) { log("Too far to throw."); return; }
            if (!combatLOS(u.x, u.y, cb.selX, cb.selY)) { log("No line of sight."); return; }
            if (!map->walkable(cb.selX, cb.selY) && combatUnitAt(cb.selX, cb.selY) < 0) { log("It would just bounce off that."); return; }
            combatUseItem(cb.selectedUnit, cb.pendingItem, combatUnitAt(cb.selX, cb.selY), cb.selX, cb.selY); cb.mode = 0; return;
        }
        if (!d->useSpell.empty()) {
            const SpellDef* sp = gd->spell(d->useSpell);
            if (sp && distSel > sp->range) { log("Out of range."); return; }
            if (!combatLOS(u.x, u.y, cb.selX, cb.selY)) { log("No line of sight."); return; }
            if (!seeSel) { log("Too dark to aim there."); return; }
            if (sp && sp->area == 0 && targetUnit < 0) { log("No target there."); return; }
            combatUseItem(cb.selectedUnit, cb.pendingItem, targetUnit, cb.selX, cb.selY); cb.mode = 0; return;
        }
        if (targetUnit < 0) { log("Choose a character."); return; }
        if (!cb.units[targetUnit].friendly()) { log("Use it on an ally."); return; }
        combatUseItem(cb.selectedUnit, cb.pendingItem, targetUnit); cb.mode = 0; return;
    }
    // mode 0: attack or move
    if (targetUnit < 0 && !seeSel && combatUnitAt(cb.selX, cb.selY) >= 0) { log("Something shifts in the dark there."); return; }
    if (targetUnit >= 0 && targetUnit != cb.selectedUnit) {
        Combatant& t = cb.units[targetUnit];
        if (t.friendly()) { log("That's an ally."); return; }
        if (t.rift && unitDist(t, u.x, u.y) <= 1) {   // 1.4.0 (#78): close a rift from next to it
            if (u.ch.ap < 2) { log("Closing a rift takes 2 AP."); return; }
            u.ch.ap -= 2; unitGone(t, "is sealed shut"); gAudio.play("teleport"); goalCheck(false); combatCheckEnd();
            if (!cb.ending && u.ch.ap <= 0 && u.ch.move <= 0) combatEndTurn();
            return;
        }
        int dist = chebyshev(u.x, u.y, t.x, t.y); if (t.ch.size == 2) dist = std::min(dist, std::min(chebyshev(u.x, u.y, t.x + 1, t.y + 1), std::min(chebyshev(u.x, u.y, t.x + 1, t.y), chebyshev(u.x, u.y, t.x, t.y + 1))));
        if (dist > u.ch.attackRange(*gd)) { log("Out of range."); return; }
        if (u.ch.trait(*gd) == "reach" && dist == 2 && !(t.x == u.x || t.y == u.y || abs(t.x - u.x) == abs(t.y - u.y))) { log("A spear reaches 2 tiles only in a straight line."); return; }
        if (!combatLOS(u.x, u.y, t.x, t.y)) { log("No line of sight."); return; }
        if (!targetable(u, t)) { log("You can't make them out in the smoke."); return; }
        combatAttack(cb.selectedUnit, targetUnit, cb.aimed);
        combatCheckEnd();
        if (!cb.ending && u.ch.ap <= 0 && u.ch.move <= 0) combatEndTurn();
        return;
    }
    if (targetUnit == cb.selectedUnit) { cb.mode = 3; cb.menuSel = 0; return; }
    // 1.4.0 (#79): light an unlit brazier next to you (2 AP)
    if (targetUnit < 0 && map->inBounds(cb.selX, cb.selY) && chebyshev(u.x, u.y, cb.selX, cb.selY) <= 1) {
        const TileKind& k = allTileKinds()[map->at(cb.selX, cb.selY).kind];
        if (!k.kindleTo.empty() && tileKind(k.kindleTo)) {
            if (u.ch.ap < 2) { log("Lighting it takes 2 AP."); return; }
            u.ch.ap -= 2; std::string to = k.kindleTo; changeCell(cb.selX, cb.selY, to); gAudio.play("light");
            addFloat(cb.selX, cb.selY, "lit", C::Cream); cb.log.push_back(u.ch.name + " lights it. The room brightens."); log(u.ch.name + " lights a brazier.");
            return;
        }
    }
    if (!cb.path.empty()) { cb.aiPath = cb.path; cb.stepTimer = 0; cb.path.clear(); }
    (void)g_dummy;
}

// ---------------- drawing ----------------
void Game::drawCombat(Canvas& cv, InputState& in) {
    int viewH = SCREEN_H - 72;
    cv.setClip({0, 0, SCREEN_W, viewH});
    int cx = camX, cy = camY;
    bool myTurn = cb.playerTurn && !cb.ending && cb.selectedUnit >= 0;
    // 1.4.0 (#58 #61): ground effects - fire, ice, smoke
    for (auto& g : cb.groundFx) {
        if (map->dark && !visible(g.x, g.y) && !map->at(g.x, g.y).explored) continue;
        int gx = g.x * TILE - cx, gy = g.y * TILE - cy;
        if (g.kind == 1) { int fl = (ctx.frame / 6 + g.x * 3 + g.y) % 3; cv.fillRect(gx, gy, TILE, TILE, RGB(255, 80 + fl * 25, 20, 45)); ctx.ts->drawRecolor(cv, T(1, 42), gx + 6, gy + 5 - fl, 1, fl == 1 ? RGB(255, 220, 90, 230) : RGB(255, 130, 40, 230)); }
        else if (g.kind == 2) { cv.fillRect(gx, gy, TILE, TILE, RGB(170, 220, 255, 60)); cv.line(gx + 4, gy + 6, gx + 10, gy + 4, RGB(230, 250, 255, 160)); cv.line(gx + 13, gy + 18, gx + 19, gy + 15, RGB(230, 250, 255, 160)); }
        else if (g.kind == 3) { cv.fillRect(gx, gy, TILE, TILE, RGB(150, 150, 160, 120)); for (int k = 0; k < 4; k++) { int ox = (k * 7 + ctx.frame / 10 + g.x * 5) % 18, oy = (k * 11 + g.y * 3) % 18; cv.fillRect(gx + ox, gy + oy, 6, 5, RGB(200, 200, 210, 110)); } }
    }
    // 1.4.0 (#62): the tiles an announced boss attack will land on
    for (auto& w : cb.windups) for (auto& q : w.tiles) { int wx = q.first * TILE - cx, wy = q.second * TILE - cy; bool pulse = (ctx.frame / 8) % 2; cv.fillRect(wx, wy, TILE, TILE, RGB(255, 30, 30, pulse ? 60 : 35)); cv.rect(wx, wy, TILE, TILE, RGB(255, 40, 40)); cv.rect(wx + 1, wy + 1, TILE - 2, TILE - 2, RGB(255, 40, 40, 140)); }
    // 1.4.0 (#78): the goal's tiles
    if (cb.goal.kind == "reach" && !cb.goal.done) for (int y = cb.goal.y; y < cb.goal.y + cb.goal.h; y++) for (int x = cb.goal.x; x < cb.goal.x + cb.goal.w; x++) { int gx = x * TILE - cx, gy = y * TILE - cy; cv.rect(gx + 1, gy + 1, TILE - 2, TILE - 2, (ctx.frame / 10) % 2 ? C::Green : RGB(60, 200, 90)); }
    // reachable tiles, each with the cover it would give (#55): a bar on every side that has cover against a foe who can see it
    if (myTurn && cb.mode == 0 && cb.aiPath.empty()) for (size_t i = 0; i < cb.reachable.size(); i++) {
        auto& r = cb.reachable[i]; int rx = r.first * TILE - cx, ry = r.second * TILE - cy;
        cv.fillRect(rx + 2, ry + 2, TILE - 4, TILE - 4, RGB(80, 160, 255, 50));
        int cvr = i < cb.reachCover.size() ? cb.reachCover[i] : 0; if (!cvr) continue;
        if (cvr & 0x100) { cv.rect(rx + 3, ry + 3, TILE - 6, TILE - 6, coverColor(2)); continue; }   // in smoke: full cover all round
        for (int k = 0; k < 4; k++) {
            int v = (cvr >> (k * 2)) & 3; if (!v) continue; Color8 cc = coverColor(v); int len = v >= 2 ? TILE - 4 : TILE / 2, th = v >= 2 ? 4 : 3;
            int bx = k == 1 ? rx + TILE - 1 - th : k == 3 ? rx + 1 : rx + (TILE - len) / 2, by = k == 2 ? ry + TILE - 1 - th : k == 0 ? ry + 1 : ry + (TILE - len) / 2;
            int bw = (k == 1 || k == 3) ? th : len, bh = (k == 1 || k == 3) ? len : th;
            cv.fillRect(bx - 1, by - 1, bw + 2, bh + 2, RGB(0, 0, 0, 200)); cv.fillRect(bx, by, bw, bh, cc);   // a dark rim so it reads on any ground
        }
    }
    // targeting preview: range, blast area, and everyone the blast would catch (allies flash orange)
    std::string aoeWarn;
    if (myTurn && (cb.mode == 1 || cb.mode == 2)) {
        auto& u = cb.units[cb.selectedUnit];
        const SpellDef* sp = cb.mode == 1 ? gd->spell(cb.pendingSpell) : nullptr; const ItemDef* it = cb.mode == 2 ? gd->item(cb.pendingItem) : nullptr;
        if (it && !it->useSpell.empty()) sp = gd->spell(it->useSpell);
        int range = sp ? sp->range : 6, area = sp ? sp->area : (it && (it->dmgMax > 0 || it->effect == "flare") ? std::max(0, it->range - 1) : -1);
        bool showRange = sp || (it && (it->dmgMax > 0 || it->effect == "flare"));
        if (showRange) for (int y = u.y - range; y <= u.y + range; y++) for (int x = u.x - range; x <= u.x + range; x++) if (map->inBounds(x, y) && combatLOS(u.x, u.y, x, y)) cv.fillRect(x * TILE - cx + 2, y * TILE - cy + 2, TILE - 4, TILE - 4, RGB(200, 80, 255, 40));
        SpellDef spUp; if (sp) { spUp = castSpell(u.ch, *sp); sp = &spUp; area = sp->area; }   // 1.4.0: as this caster casts it (rank upgrades)
        if (area >= 0 && map->inBounds(cb.selX, cb.selY) && !(sp && (sp->target == "ally" || sp->target == "self") && area == 0)) {
            bool lightArea = (sp && sp->utility == "light") || (it && it->effect == "flare");
            if (lightArea) area = sp ? 4 : (it->range > 1 ? it->range : 4);
            std::vector<std::pair<int, int>> tl;
            if (sp && (!sp->shape.empty() || sp->area > 0)) tl = spellTiles(cb.selectedUnit, *sp, cb.selX, cb.selY);   // (#64) lines and cones
            else for (int y = cb.selY - area; y <= cb.selY + area; y++) for (int x = cb.selX - area; x <= cb.selX + area; x++) tl.push_back({x, y});
            Color8 fillC = lightArea ? RGB(255, 240, 160, 50) : (it && it->effect == "smoke") || (sp && sp->smoke > 0) ? RGB(190, 190, 200, 70) : RGB(255, 120, 60, 70);
            for (auto& q : tl) cv.fillRect(q.first * TILE - cx, q.second * TILE - cy, TILE, TILE, fillC);
            if (sp && sp->chainJumps > 0) {   // (#64) the jumps it would make, with the damage left at each
                auto ch = chainTargets(cb.selectedUnit, *sp, combatUnitAt(cb.selX, cb.selY)); int lx = u.x, ly = u.y, pct = 100;
                for (size_t k = 0; k < ch.size(); k++) { auto& t = cb.units[ch[k]]; cv.line(lx * TILE + 12 - cx, ly * TILE + 12 - cy, t.x * TILE + 12 - cx, t.y * TILE + 12 - cy, RGB(140, 200, 255, 200));
                    cv.rect(t.x * TILE - cx - 1, t.y * TILE - cy - 1, TILE + 2, TILE + 2, t.friendly() ? C::Orange : foeColor());
                    ctx.font->drawShadow(cv, *ctx.ts, t.x * TILE - cx + 2, t.y * TILE - cy - 11, fmt("%d%%", pct), C::Cyan, C::Black); pct = pct * (100 - sp->chainFalloff) / 100; lx = t.x; ly = t.y; }
            }
            if (!lightArea && !(it && it->effect == "smoke") && !(sp && sp->smoke > 0 && sp->dmgMax <= 0)) {
                std::vector<int> hit;
                if (sp && sp->chainJumps > 0) hit = chainTargets(cb.selectedUnit, *sp, combatUnitAt(cb.selX, cb.selY));
                else if (sp) hit = spellVictims(cb.selectedUnit, *sp, cb.selX, cb.selY);
                else for (size_t i = 0; i < cb.units.size(); i++) if (cb.units[i].ch.alive && unitDist(cb.units[i], cb.selX, cb.selY) <= area) hit.push_back((int)i);
                int allies = 0; bool self = false;
                for (int v : hit) { auto& t = cb.units[v]; if (!t.friendly() && map->dark && !visible(t.x, t.y)) continue; bool own = t.friendly(); int tx = t.x * TILE - cx, ty = t.y * TILE - cy, tsz = t.ch.size * TILE;
                    if (own && !(sp && (sp->target == "ally" || sp->target == "self"))) { if (v == cb.selectedUnit) self = true; else allies++; if ((ctx.frame / 8) % 2) { cv.rect(tx - 1, ty - 1, tsz + 2, tsz + 2, C::Orange); cv.rect(tx - 2, ty - 2, tsz + 4, tsz + 4, C::Orange); } }
                    else cv.rect(tx - 1, ty - 1, tsz + 2, tsz + 2, own ? friendColor() : foeColor()); }
                if (self && allies) aoeWarn = fmt("hits YOU and %d all%s!", allies, allies == 1 ? "y" : "ies"); else if (self) aoeWarn = "hits YOU!"; else if (allies) aoeWarn = fmt("hits %d all%s!", allies, allies == 1 ? "y" : "ies");
            }
        }
    }
    // attack range ring for the active unit
    if (myTurn && cb.mode == 0) { auto& u = cb.units[cb.selectedUnit]; int r = u.ch.attackRange(*gd); if (r > 1) for (int y = u.y - r; y <= u.y + r; y++) for (int x = u.x - r; x <= u.x + r; x++) if (map->inBounds(x, y) && chebyshev(x, y, u.x, u.y) == r) cv.rect(x * TILE - cx + 10, y * TILE - cy + 10, 4, 4, RGB(255, 200, 80, 120)); }
    for (auto& p : cb.path) cv.fillRect(p.first * TILE - cx + 8, p.second * TILE - cy + 8, 8, 8, RGB(255, 255, 255, 140));
    // units, in three passes (1.3.4, #53): bodies of the fallen first, then the living, then every health bar and status
    // mark on top. Before, a body drawn later in the list covered a living unit standing on it (its health bar too), and
    // the active unit's yellow ring or a neighbour's status marks could cover the bar of the unit above.
    auto unitShown = [&](const Combatant& u) { return u.friendly() || !map->dark || visible(u.x, u.y); };   // enemies lurk in the dark
    auto unitScreen = [&](const Combatant& u, int& sx, int& sy, int& sz) { sx = u.x * TILE - cx + (int)u.offX; sy = u.y * TILE - cy + (int)u.offY; sz = u.ch.size * TILE; };
    for (auto& u : cb.units) {
        if (u.ch.alive || u.summoned || u.gone || !unitShown(u)) continue;   // summons (and those who fled, 1.4.0) leave no body
        bool under = false;   // a living unit stands on it: it hides the body
        for (auto& o : cb.units) if (o.ch.alive && u.x + u.ch.size > o.x && o.x + o.ch.size > u.x && u.y + u.ch.size > o.y && o.y + o.ch.size > u.y) under = true;
        if (under) continue;
        int sx, sy, sz; unitScreen(u, sx, sy, sz);
        if (!u.friendly()) ctx.ts->draw(cv, u.ch.size == 2 ? TL::SKULL : (u.ch.tile.s ? u.ch.tile : T(TL::CHAR_COL_DEAD, u.ch.tile.r)), sx, sy, 2, RGB(150, 150, 150));
        else ctx.ts->draw(cv, (u.ch.tile.s ? u.ch.tile : T(TL::CHAR_COL_DEAD, u.ch.tile.r)), sx, sy, 2, RGB(200, 120, 120));
    }
    for (size_t i = 0; i < cb.units.size(); i++) {   // solid backdrop so they stand out from the terrain
        auto& u = cb.units[i];
        if (!u.ch.alive || !unitShown(u)) continue;
        int sx, sy, sz; unitScreen(u, sx, sy, sz);
        bool active = (int)i == cb.selectedUnit && !cb.ending;
        bool enemy = !u.friendly();
        Color8 team = enemy ? foeColor() : u.ally ? allyColor() : friendColor();
        cv.fillRect(sx + 1, sy + 1, sz - 2, sz - 2, enemy ? foeBack() : u.ally ? RGB(0, 30, 70, 190) : friendBack());
        cv.rect(sx, sy, sz, sz, team);
        if (active) { Color8 c = C::Yellow; cv.rect(sx - 1, sy - 1, sz + 2, sz + 2, c); if ((ctx.frame / 10) % 2) cv.rect(sx - 2, sy - 2, sz + 4, sz + 4, c); }
        drawUnitSprite(cv, u.ch, sx, sy, false);
        if (u.hitFlash > 0 && ((int)(u.hitFlash * 30) % 2 == 0 || (settings && settings->reduceFlashing))) {   // struck: the sprite flashes
            Color8 fc = settings && settings->reduceFlashing ? RGB(255, 200, 200, 110) : C::White;
            if (u.ch.size == 2) { for (int yy = 0; yy < 2; yy++) for (int xx = 0; xx < 2; xx++) ctx.ts->drawRecolor(cv, tileAt(u.ch.tile, xx, yy), sx + xx * TILE, sy + yy * TILE, 2, fc); }
            else ctx.ts->drawRecolor(cv, u.ch.tile, sx, sy, 2, fc);
        }
    }
    for (auto& u : cb.units) {   // health bars, status marks and overwatch: always on top, inside the unit's own cell
        if (!u.ch.alive || !unitShown(u)) continue;
        int sx, sy, sz; unitScreen(u, sx, sy, sz);
        bool enemy = !u.friendly();
        int hpw = (sz - 2) * u.ch.hp / std::max(1, u.ch.maxHP(*gd));
        cv.fillRect(sx, sy + sz - 3, sz, 3, C::Black); cv.fillRect(sx + 1, sy + sz - 2, hpw, 1, enemy ? foeColor() : friendColor());
        if (!u.ch.effects.empty()) { int ex = sx + 1; for (auto& e : u.ch.effects) { if (ex + 4 > sx + sz - 1) break; Color8 ec = e.id == "burn" ? C::Orange : e.id == "poison" ? C::Green : e.id == "freeze" ? C::Cyan : e.id == "stun" ? C::Yellow : e.id == "shield" || e.id == "defend" ? C::Blue : C::Purple; cv.fillRect(ex, sy + 1, 4, 3, ec); ex += 5; } }
        if (u.overwatch) { cv.fillRect(sx + sz - 9, sy, 9, 7, RGB(0, 0, 0, 200)); ctx.font->draw(cv, *ctx.ts, sx + sz - 8, sy - 2, "o", C::Yellow); }
    }
    // anims
    for (auto& a : cb.anims) {
        if (a.t < 0) continue;   // 1.4.0: a queued anim (the next link of a chain)
        float f = a.t / a.dur;
        int x0 = a.x0 * TILE + 12 - cx, y0 = a.y0 * TILE + 12 - cy, x1 = a.x1 * TILE + 12 - cx, y1 = a.y1 * TILE + 12 - cy;
        if (a.kind == 0) { int x = (int)(x0 + (x1 - x0) * f), y = (int)(y0 + (y1 - y0) * f); cv.fillRect(x - 2, y - 2, 5, 5, a.color); cv.line(x0, y0, x, y, Color8{a.color.r, a.color.g, a.color.b, 80}); }
        else if (a.kind == 1) { int r = (int)(4 + f * 14); Color8 c = a.color; c.a = (uint8_t)(255 * (1 - f)); cv.rect(x1 - r, y1 - r, r * 2, r * 2, c); cv.rect(x1 - r / 2, y1 - r / 2, r, r, c); if (a.tile.valid()) ctx.ts->draw(cv, a.tile, x1 - 12, y1 - 12 - (int)(f * 10), 2, c); }
        else if (a.kind == 3) {   // a kill: eight sparks flying out and a fading ring
            Color8 c = a.color; c.a = (uint8_t)(255 * (1 - f));
            for (int k = 0; k < 8; k++) { float ang = k * 0.785f; int px2 = x1 + (int)(cosf(ang) * f * 22), py2 = y1 + (int)(sinf(ang) * f * 22); cv.fillRect(px2 - 1, py2 - 1, 3, 3, c); }
            int r = (int)(6 + f * 16); cv.rect(x1 - r, y1 - r, r * 2, r * 2, c);
        }
        else { int dx = x1 - x0, dy = y1 - y0; int x = x0 + dx / 2, y = y0 + dy / 2; Color8 c = a.color; c.a = (uint8_t)(255 * (1 - f)); cv.line(x - 6 + (int)(f * 12), y - 6, x + 6 - (int)(f * 12), y + 6, c); cv.line(x - 6 + (int)(f * 12) + 1, y - 6, x + 6 - (int)(f * 12) + 1, y + 6, c); }
    }
    // off-screen enemy indicators at the view edges
    for (auto& u : cb.units) {
        if (!u.ch.alive || u.friendly()) continue;
        if (map->dark && !visible(u.x, u.y)) continue;
        int sx = u.x * TILE - cx + TILE / 2, sy = u.y * TILE - cy + TILE / 2;
        if (sx >= 0 && sx < SCREEN_W && sy >= 0 && sy < viewH) continue;
        int ex = clampi(sx, 6, SCREEN_W - 7), ey = clampi(sy, 30, viewH - 7);
        cv.fillRect(ex - 5, ey - 5, 11, 11, RGB(200, 30, 30)); cv.rect(ex - 5, ey - 5, 11, 11, C::White);
    }
    // cursor: bold corner brackets that pulse, a tinted tile, and a readout next to it
    if (myTurn && map->inBounds(cb.selX, cb.selY) && cb.aiPath.empty()) {
        int tu = combatUnitAt(cb.selX, cb.selY);
        auto& u = cb.units[cb.selectedUnit];
        if (tu >= 0 && !cb.units[tu].friendly() && map->dark && !visible(cb.selX, cb.selY) && chebyshev(u.x, u.y, cb.selX, cb.selY) > 1) tu = -1;
        int ox = cb.selX, oy = cb.selY, sz = TILE;
        if (tu >= 0) { ox = cb.units[tu].x; oy = cb.units[tu].y; sz = cb.units[tu].ch.size * TILE; }
        int sx = ox * TILE - cx, sy = oy * TILE - cy;
        bool reach = false; for (auto& r : cb.reachable) if (r.first == cb.selX && r.second == cb.selY) reach = true;
        Color8 c = C::White;
        if (tu >= 0 && !cb.units[tu].friendly()) c = foeColor(); else if (tu >= 0) c = friendColor();
        else if (cb.mode == 1 || cb.mode == 2) c = RGB(255, 150, 60); else if (reach) c = RGB(120, 220, 255); else if (!map->walkable(cb.selX, cb.selY)) c = RGB(150, 150, 150);
        int pulse = (ctx.frame / 8) % 2;
        cv.fillRect(sx, sy, sz, sz, Color8{c.r, c.g, c.b, 45});
        int bx = sx - 2 - pulse, by = sy - 2 - pulse, bs = sz + 4 + pulse * 2, L = 8;
        auto bracket = [&](int x0, int y0, int dx, int dy, Color8 col) { cv.fillRect(dx > 0 ? x0 : x0 - L + 1, y0, L, 2, col); cv.fillRect(x0, dy > 0 ? y0 : y0 - L + 1, 2, L, col); };
        Color8 dk = RGB(0, 0, 0, 220);
        for (int pass = 0; pass < 2; pass++) { int o = pass == 0 ? 1 : 0; Color8 col = pass == 0 ? dk : c;
            bracket(bx + o, by + o, 1, 1, col); bracket(bx + bs - 2 + o, by + o, -1, 1, col); bracket(bx + o, by + bs - 2 + o, 1, -1, col); bracket(bx + bs - 2 + o, by + bs - 2 + o, -1, -1, col); }
        auto label = [&](int lx, int ly, const std::string& t, Color8 col) { int w = ctx.font->width(t) + 6; cv.fillRect(lx - 3, ly + 1, w, 12, RGB(0, 0, 0, 190)); ctx.font->draw(cv, *ctx.ts, lx, ly, t, col); };
        int lx = sx + sz + 6, ly = sy - 2; if (lx > SCREEN_W - 150) lx = sx - 150;
        if (tu >= 0 && !cb.units[tu].friendly() && cb.mode == 0) {
            auto& t = cb.units[tu];
            int dist = unitDist(t, u.x, u.y); bool inRange = dist <= u.ch.attackRange(*gd) && combatLOS(u.x, u.y, t.x, t.y);
            int hc = hitChance(u, t, cb.aimed);
            std::string st = inRange ? fmt("%d%% to hit, %d%% crit%s%s", hc, t.ch.hasEffect("tagged") ? 100 : critChance(u, t, cb.aimed), cb.aimed ? " (aimed)" : "", hitBreakdown(u, t, cb.aimed).c_str()) : fmt("out of range (%d/%d)", dist, u.ch.attackRange(*gd));
            label(lx, ly, st, inRange ? hitColor(hc) : C::Gray);
            label(lx, ly + 13, fmt("%s %d/%d", t.ch.name.c_str(), t.ch.hp, t.ch.maxHP(*gd)), C::White);
            // cover: a thick bar on the side of the target that faces you, and a word for it
            int cov = inRange ? coverAt(u, t) : 0;
            if (cov > 0) {
                int ddx = (u.x > t.x) - (u.x < t.x), ddy = (u.y > t.y) - (u.y < t.y); int tsx = t.x * TILE - cx, tsy = t.y * TILE - cy, tsz = t.ch.size * TILE; Color8 cc = coverColor(cov);
                if (ddx) cv.fillRect(ddx > 0 ? tsx + tsz - 1 : tsx - 3, tsy + 2, 4, tsz - 4, cc);
                if (ddy) cv.fillRect(tsx + 2, ddy > 0 ? tsy + tsz - 1 : tsy - 3, tsz - 4, 4, cc);
                label(lx, ly + 26, cov >= 2 ? "in FULL cover" : "in half cover", cc);
            } else if (inRange && flanked(u, t)) label(lx, ly + 26, "FLANKED - no cover", hitColor(90));
            else if (inRange && exposed(u, t)) label(lx, ly + 26, "EXPOSED - no cover", hitColor(90));   // 1.4.0 (#56)
            if (t.rift && dist <= 1) label(lx, ly + 39, "click: close the rift (2 AP)", C::Purple);
        } else if (tu >= 0 && !cb.units[tu].friendly() && cb.mode == 1) {   // 1.4.0 (#57): what cover does to the spell, and any combo it sets off
            auto& t = cb.units[tu]; const SpellDef* sp0 = gd->spell(cb.pendingSpell);
            if (sp0) { SpellDef sp = castSpell(u.ch, *sp0); int pw = u.ch.stat(sp.scaleStat, *gd); int lo2 = std::max(0, sp.dmgMin + (pw - 8) / 2), hi2 = std::max(0, sp.dmgMax + (pw - 8) / 2);
                int cp = spellCoverPct(u, t, sp); int ly2 = ly;
                if (sp.dmgMax > 0) { std::string st = cp > 0 ? fmt("%s %d-%d  -%d%% (%s cover)", sp.name.c_str(), lo2 * (100 - cp) / 100, hi2 * (100 - cp) / 100, cp, cp >= 50 ? "full" : "half") : fmt("%s %d-%d", sp.name.c_str(), lo2, hi2); label(lx, ly2, st, cp > 0 ? C::Gray : C::White); ly2 += 13;
                    if (cp == 0 && !singleTargetSpell(sp) && coverAt(u, t) > 0) { label(lx, ly2, "area: ignores cover", C::Green); ly2 += 13; } }
                std::string cmb = comboPreview(t, sp.dmgType); if (!cmb.empty() && sp.dmgMax > 0) { label(lx, ly2, cmb + "!", C::Cyan); ly2 += 13; }
                if (!aoeWarn.empty()) label(lx, ly2, aoeWarn, C::Orange); }
        } else if (!aoeWarn.empty()) label(lx, ly, aoeWarn, C::Orange);
        else if (!cb.path.empty()) label(lx, ly, fmt("%d move%s", (int)cb.path.size(), cb.path.size() == 1 ? "" : "s"), C::Cyan);
        else if (cb.mode == 0 && tu < 0 && map->walkable(cb.selX, cb.selY) && !reach && !(cb.selX == u.x && cb.selY == u.y)) label(lx, ly, "out of reach", C::Gray);
        cv.resetClip();
        drawTileInfo(cv, cb.selX, cb.selY);
        cv.setClip({0, 0, SCREEN_W, viewH});
    }
    cv.resetClip();
    // top bar
    cv.fillRect(0, 0, SCREEN_W, 24, RGB(0, 0, 0, 170));
    uiRect(0, 0, SCREEN_W, 24);
    if (myTurn) {
        auto& u = cb.units[cb.selectedUnit];
        int bx = 4;
        for (int i = 0; i < NACTIONS; i++) {
            int bw = actionButtonW(ctx, i);
            bool hl = (cb.mode == 3 && cb.menuSel == i) || (i == 0 && !cb.aimed && cb.mode == 0) || (i == 1 && cb.aimed && cb.mode == 0) || (i == 2 && (cb.mode == 4 || cb.mode == 1)) || (i == 3 && (cb.mode == 5 || cb.mode == 2));
            bool hover = in.mouseX >= bx && in.mouseX < bx + bw && in.mouseY <= 24;
            cv.fillRect(bx, 3, bw, 18, hl ? RGB(150, 110, 30) : hover ? RGB(80, 80, 100) : RGB(50, 50, 66)); cv.rect(bx, 3, bw, 18, hl ? C::Yellow : C::Gold);
            int cx = bx + 3, kw = keyChip(ctx, cx, 5, ACTION_KEYS[i]);
            if (kw && i == 1) kw += 2 + keyChip(ctx, cx + kw + 2, 5, ACTION_KEYS[i]);
            ctx.font->draw(cv, *ctx.ts, cx + (kw ? kw + 5 : 2), 6, ACTIONS[i], C::White);
            bx += bw + 3;
        }
        int right = SCREEN_W - combatMenuButtonW(ctx) - 8;   // the Menu button sits at the far right
        // whose turn, AP / move and the round, right-aligned in what is left of the bar (it is on the HUD below too)
        std::string st = fmt("%s  AP %d  MV %d  R%d", u.ch.name.c_str(), u.ch.ap, u.ch.move, cb.round);
        if (bx + 8 + ctx.font->width(st) > right - 6) st = fmt("AP %d MV %d R%d", u.ch.ap, u.ch.move, cb.round);
        if (bx + 8 + ctx.font->width(st) > right - 6) st = fmt("Round %d", cb.round);
        if (bx + 8 + ctx.font->width(st) > right - 6) st = fmt("R%d", cb.round);
        if (bx + 8 + ctx.font->width(st) <= right - 6) textRight(ctx, right - 6, 6, st, C::Yellow);
    } else if (cb.selectedUnit >= 0 && !cb.ending) ctx.font->draw(cv, *ctx.ts, 8, 6, cb.units[cb.selectedUnit].ch.name + "'s turn...", C::Red);
    else if (cb.ending) ctx.font->draw(cv, *ctx.ts, 8, 6, cb.victory ? "Victory!" : cb.fled ? "Escaped!" : "Defeat...", cb.victory ? C::Yellow : C::Red);
    {   // Menu button (pause: settings, load, exit), on every turn
        int bw = combatMenuButtonW(ctx), bx = SCREEN_W - bw - 4;
        bool hover = in.mouseX >= bx && in.mouseY <= 24;
        cv.fillRect(bx, 3, bw, 18, hover ? RGB(80, 80, 100) : RGB(50, 50, 66)); cv.rect(bx, 3, bw, 18, C::Gold);
        int kw = keyChip(ctx, bx + 3, 5, A_CANCEL);
        ctx.font->draw(cv, *ctx.ts, bx + 3 + (kw ? kw + 5 : 2), 6, "Menu", C::White);
    }
    // 1.4.0 (#78): the fight's goal, top middle under the bar
    if (!cb.goal.kind.empty()) {
        std::string gt = "Goal: " + cb.goal.text;
        if (cb.goal.kind == "hold" && !cb.goal.done) gt += fmt(" (round %d of %d)", std::min(cb.round, cb.goal.rounds), cb.goal.rounds);
        if (cb.goal.kind == "close" && !cb.goal.done) { int n = 0; for (auto& r : cb.units) if (r.ch.alive && r.rift) n++; gt += fmt(" (%d open)", n); }
        if (cb.goal.done) gt += " - done!"; else if (cb.goal.failed) gt += " - FAILED";
        int gw = ctx.font->width(gt) + 12, gx = SCREEN_W / 2 - gw / 2;
        cv.fillRect(gx, 26, gw, 14, RGB(0, 0, 0, 190)); cv.rect(gx, 26, gw, 14, cb.goal.failed ? C::Red : cb.goal.done ? C::Green : C::Gold);
        ctx.font->draw(cv, *ctx.ts, gx + 6, 27, gt, cb.goal.failed ? C::Red : cb.goal.done ? C::Green : C::Yellow);
    }
    // round / turn banner under the top bar
    if (cb.banner > 0 && !cb.bannerText.empty()) {
        float a = std::min(1.0f, cb.banner * 3); int bw = ctx.font->width(cb.bannerText, 2) + 24, bx = SCREEN_W / 2 - bw / 2;
        cv.fillRect(bx, 30, bw, 28, RGB(0, 0, 0, (uint8_t)(170 * a))); cv.rect(bx, 30, bw, 28, Color8{200, 170, 80, (uint8_t)(255 * a)});
        ctx.font->draw(cv, *ctx.ts, bx + 12, 32, cb.bannerText, Color8{255, 225, 140, (uint8_t)(255 * a)}, 2);
    }
    // turn order strip (right side) - click a portrait to look at that unit
    int ox = SCREEN_W - 30, oy = TURN_STRIP_Y; int shown = 0;
    uiRect(ox, oy, 30, 10 * 26);
    { std::string hdr = "TURN"; ctx.font->drawShadow(cv, *ctx.ts, ox + 13 - ctx.font->width(hdr) / 2, oy - 13, hdr, C::Gold, RGB(0, 0, 0)); }
    for (int k = 0; k < (int)cb.order.size() && shown < 10; k++) {
        int idx = cb.order[(cb.turnPos + k) % cb.order.size()]; auto& u = cb.units[idx]; if (!u.ch.alive) continue;
        bool hover = in.mouseX >= ox && in.mouseY >= oy + shown * 26 && in.mouseY < oy + shown * 26 + 26;
        cv.fillRect(ox, oy + shown * 26, 26, 26, k == 0 ? RGB(120, 90, 30, 220) : hover ? RGB(60, 60, 80, 220) : RGB(0, 0, 0, 170)); cv.rect(ox, oy + shown * 26, 26, 26, u.friendly() ? friendColor() : foeColor());
        if (u.ch.recolor) ctx.ts->drawRecolor(cv, u.ch.tile, ox + 7, oy + shown * 26 + 7, 1, u.ch.color); else ctx.ts->draw(cv, u.ch.tile, ox + 7, oy + shown * 26 + 7, 1);
        { int hpw = 24 * u.ch.hp / std::max(1, u.ch.maxHP(*gd)); cv.fillRect(ox + 1, oy + shown * 26 + 23, 24, 2, RGB(0, 0, 0)); cv.fillRect(ox + 1, oy + shown * 26 + 23, hpw, 2, u.friendly() ? friendColor() : foeColor()); }
        if (k == 0 || (shown == 1 && !hover)) { std::string tag = k == 0 ? "NOW" : "NEXT"; int tw = ctx.font->width(tag) + 6; cv.fillRect(ox - tw - 1, oy + shown * 26 + 7, tw, 12, RGB(0, 0, 0, 180)); ctx.font->draw(cv, *ctx.ts, ox - tw + 2, oy + shown * 26 + 7, tag, k == 0 ? C::Yellow : C::LtGray); }
        if (hover) { std::string nm = fmt("%s %d/%d", u.ch.name.c_str(), u.ch.hp, u.ch.maxHP(*gd)); int tw = ctx.font->width(nm) + 8; cv.fillRect(ox - tw - 2, oy + shown * 26 + 6, tw, 14, RGB(0, 0, 0, 200)); ctx.font->draw(cv, *ctx.ts, ox - tw + 2, oy + shown * 26 + 7, nm, C::White); }
        shown++;
    }
    // action menus
    if (myTurn && cb.mode == 3) {
        int mx = SCREEN_W / 2 - 70, my = 40;
        panelTitle(ctx, mx, my, 140, 30 + NACTIONS * 20, "Action"); uiRect(mx, my - 4, 140, 34 + NACTIONS * 20);
        for (int i = 0; i < NACTIONS; i++) { bool sel = cb.menuSel == i; if (sel) cv.fillRect(mx + 6, my + 24 + i * 20, 128, 18, RGB(90, 70, 20)); ctx.font->draw(cv, *ctx.ts, mx + 12, my + 27 + i * 20, ACTIONS[i], sel ? C::Yellow : C::White); int cw = actionChipsW(ctx, i); if (cw) { int kx = mx + 130 - cw, kw = keyChip(ctx, kx, my + 26 + i * 20, ACTION_KEYS[i]); if (i == 1) keyChip(ctx, kx + kw + 2, my + 26 + i * 20, ACTION_KEYS[i]); } }
    }
    if (myTurn && (cb.mode == 4 || cb.mode == 5)) {
        auto& u = cb.units[cb.selectedUnit];
        std::vector<std::string> list; if (cb.mode == 4) { for (auto& id : u.ch.spells) if (auto* sp = gd->spell(id)) if (spellUsableInCombat(*sp)) list.push_back(id); } else for (auto& it : party.inventory) if (gd->item(it.id) && gd->item(it.id)->type == IT_CONSUMABLE && std::find(list.begin(), list.end(), it.id) == list.end()) list.push_back(it.id);
        // a fixed-size window: ten rows that scroll with the selection, names cut to fit, three lines of description
        const int VIS = 10, PW = 240; int n = (int)list.size(), mx = SCREEN_W / 2 - 120, my = 40, ph = 34 + VIS * 20 + 44;
        panelTitle(ctx, mx, my, PW, ph, cb.mode == 4 ? "Spells" : "Items"); uiRect(mx, my - 4, PW, ph + 4);
        auto fit = [&](std::string t, int w) { if (ctx.font->width(t) <= w) return t; while (!t.empty() && ctx.font->width(t + "..") > w) t.pop_back(); return t + ".."; };
        for (int r = 0; r < VIS && cb.menuScroll + r < n; r++) {
            int i = cb.menuScroll + r, ry = my + 24 + r * 20; bool sel = cb.menuSel == i; if (sel) cv.fillRect(mx + 6, ry, PW - 18, 18, RGB(90, 70, 20));
            if (cb.mode == 4) { const SpellDef* sp = gd->spell(list[i]); if (!sp) continue; std::string cost = fmt("%dMP %dAP", sp->cost, sp->ap); int cw = ctx.font->width(cost);
                ctx.ts->draw(cv, sp->tile, mx + 8, ry + 1, 1); ctx.font->draw(cv, *ctx.ts, mx + 24, ry + 3, fit(sp->name, PW - 46 - cw), u.ch.mp >= sp->cost ? (sel ? C::Yellow : C::White) : C::Gray); textRight(ctx, mx + PW - 16, ry + 3, cost, u.ch.mp >= sp->cost ? C::LtGray : C::Gray); }
            else { const ItemDef* d = gd->item(list[i]); if (!d) continue; std::string cnt = fmt("x%d", party.countItem(list[i])); int cw = ctx.font->width(cnt);
                drawItemTile(*ctx.ts, cv, *d, mx + 8, ry + 1, 1); ctx.font->draw(cv, *ctx.ts, mx + 24, ry + 3, fit(d->name, PW - 46 - cw), sel ? C::Yellow : C::White); textRight(ctx, mx + PW - 16, ry + 3, cnt, C::LtGray); }
        }
        if (n > VIS) {   // scrollbar
            int sx = mx + PW - 9, sy = my + 24, sh = VIS * 20 - 2, th = std::max(10, sh * VIS / n), ty = sy + (sh - th) * cb.menuScroll / std::max(1, n - VIS);
            cv.fillRect(sx, sy, 4, sh, RGB(40, 40, 54)); cv.fillRect(sx, ty, 4, th, C::Gold);
        }
        if (n > 0) { std::string desc; int k = clampi(cb.menuSel, 0, n - 1); if (cb.mode == 4) { if (auto* sp = gd->spell(list[k])) desc = sp->desc; } else if (auto* d = gd->item(list[k])) desc = d->desc;
            auto ls = ctx.font->wrap(desc, PW - 20); for (size_t i = 0; i < ls.size() && i < 3; i++) ctx.font->draw(cv, *ctx.ts, mx + 10, my + 30 + VIS * 20 + (int)i * 13, i == 2 && ls.size() > 3 ? fit(ls[i], PW - 30) + ".." : ls[i], C::LtGray); }
        if (n > VIS) textRight(ctx, mx + PW - 10, my + 4, fmt("%d/%d", cb.menuSel + 1, n), C::Gray);
        if (u.partyIndex >= 0) {   // which hotbar slot each entry is on, and how to put one there
            auto& favs = party.members[u.partyIndex].favs;
            for (int r = 0; r < VIS && cb.menuScroll + r < n; r++) { std::string key = (cb.mode == 4 ? "spell:" : "item:") + list[cb.menuScroll + r];
                for (int f = 0; f < 8; f++) if (favs[f] == key) { ctx.font->drawShadow(cv, *ctx.ts, mx + 4, my + 24 + r * 20 + 9, std::to_string(f + 1), C::Cyan, C::Black); break; } }
            std::string hint = gPadGlyphs ? "" : "1-8: put on the hotbar";
            if (!hint.empty()) ctx.font->drawShadow(cv, *ctx.ts, mx + 10, my + ph + 2, hint, C::Gray, C::Black);
        }
    }
    if (myTurn && cb.units[cb.selectedUnit].partyIndex >= 0) drawHotbar(cv, in, cb.units[cb.selectedUnit].partyIndex, HOTBAR_Y);
    if (myTurn && cb.mode == 0) { std::string hint = fmt("[%s] center camera   right-drag / edges: pan", gKeys.label(A_CENTER).c_str()); ctx.font->drawShadow(cv, *ctx.ts, SCREEN_W - 8 - ctx.font->width(hint), HOTBAR_Y - 16, hint, C::Gray, C::Black); }
}

// Info box for the tile under the combat cursor (top-left): what stands there, what the ground does, light and reach.
static std::string prettyKind(const std::string& n) {
    std::string s = n; for (auto& ch : s) if (ch == '_') ch = ' ';
    if (startsWith(s, "floor ")) s = s.substr(6) + " floor"; else if (startsWith(s, "wall ")) s = s.substr(5) + " wall";
    if (!s.empty()) s[0] = (char)toupper(s[0]);
    return s;
}
void Game::drawTileInfo(Canvas& cv, int tx, int ty) {
    if (!map || !map->inBounds(tx, ty) || cb.selectedUnit < 0) return;
    const Cell& c = map->at(tx, ty); const TileKind& k = allTileKinds()[c.kind];
    auto& me = cb.units[cb.selectedUnit];
    std::vector<std::pair<std::string, Color8>> lines;
    int tu = combatUnitAt(tx, ty);
    bool seen = !map->dark || visible(tx, ty) || chebyshev(me.x, me.y, tx, ty) <= 1;
    if (tu >= 0 && (cb.units[tu].friendly() || seen)) {
        auto& t = cb.units[tu];
        lines.push_back({fmt("%s  (Lv %d)", t.ch.name.c_str(), t.ch.level), t.friendly() ? friendColor() : foeColor()});
        lines.push_back({fmt("HP %d/%d  AP %d  Move %d", t.ch.hp, t.ch.maxHP(*gd), t.ch.ap, t.ch.maxMove(*gd)), C::White});
        if (!t.friendly()) {
            int lo = t.ch.mDmgMin, hi = t.ch.mDmgMax;
            lines.push_back({fmt("Hits %d-%d %s, range %d", lo, hi, dmgTypeName(t.ch.mDmgType), t.ch.attackRange(*gd)), C::LtGray});
            int known = 0; { auto it = extra.find("rk_" + t.ch.creatureId); if (it != extra.end()) known = atoi(it->second.c_str()); }
            auto bit = bestiary.find(t.ch.creatureId); bool all = bit != bestiary.end() && bit->second >= 3;
            std::string weak, strong;
            for (int d = 0; d < DT_COUNT; d++) {
                if (!all && !(known & (1 << d))) continue;
                int r = t.ch.resist(d, *gd); if ((t.ch.undead || t.ch.mechanical) && d == DT_POISON) r = 100; if (t.ch.mechanical && d == DT_SHOCK) r = std::min(r, -50);
                if (r < 0) weak += std::string(weak.empty() ? "" : ", ") + dmgTypeName(d); else if (r >= 30) strong += std::string(strong.empty() ? "" : ", ") + dmgTypeName(d) + (r >= 100 ? " (immune)" : "");
            }
            // 1.5.0 (#92): weapon traits that get through (or break it): learned by hitting it with such a weapon
            std::string through;
            for (auto& kv : t.ch.traitRes) { int b = 8 + traitIndex(kv.first); if (b < 8 || (!all && !(known & (1 << b)))) continue;
                if (kv.second < 0) weak += std::string(weak.empty() ? "" : ", ") + kv.first + " weapons"; else if (kv.second < t.ch.resist(DT_PHYS, *gd)) through += std::string(through.empty() ? "" : ", ") + kv.first; }
            if (!through.empty()) lines.push_back({"Gets through: " + through + " weapons", C::Cyan});
            if (!weak.empty()) lines.push_back({"Weak: " + weak, C::Yellow});
            if (!strong.empty()) lines.push_back({"Resists: " + strong, C::Gray});
            if (weak.empty() && strong.empty()) lines.push_back({all ? "No notable resistances" : "Resistances: hit it to find out", C::DkGray});
            if (t.ch.darkvision && map->dark) lines.push_back({"Sees in the dark", C::Purple});
        }
        if (!t.ch.effects.empty()) { std::string ef; for (auto& e : t.ch.effects) ef += (ef.empty() ? "" : ", ") + e.id + fmt(" %d", e.turns); lines.push_back({ef, C::Cyan}); }
    } else if (tu >= 0) lines.push_back({"Something moves in the dark...", C::Purple});
    if (!seen && !c.explored) lines.push_back({"Unexplored", C::DkGray});
    else {
        std::string ground = prettyKind(k.name);
        std::string props;
        if (!c.walkable) props = c.blocksSight ? "solid - full cover" : (c.water ? "water - impassable" : "obstacle - half cover");
        else if (k.moveCost > 1) props = fmt("rough ground (%d MV)", k.moveCost);
        if (map->chestAt(tx, ty)) props = "a chest";
        lines.push_back({ground + (props.empty() ? "" : "  - " + props), C::LtGray});
        // 1.4.0: what the ground does in a fight
        { std::string ex; if (!k.breaks.empty()) ex += k.burns ? "breaks or burns" : "can be smashed"; if (k.conduct) ex += std::string(ex.empty() ? "" : ", ") + "carries shock"; if (k.flammable) ex += std::string(ex.empty() ? "" : ", ") + "burns"; if (!k.kindleTo.empty()) ex += std::string(ex.empty() ? "" : ", ") + "click next to it: light it (2 AP)"; if (!ex.empty()) lines.push_back({"  " + ex, C::Gray}); }
        if (int f = groundAt(tx, ty, 1)) lines.push_back({fmt("On fire (%d round%s) - burns who stands in it", f, f == 1 ? "" : "s"), C::Orange});
        if (int f = groundAt(tx, ty, 2)) lines.push_back({fmt("Ice (%d) - costs 1 more move", f), C::Cyan});
        if (int f = groundAt(tx, ty, 3)) lines.push_back({fmt("Smoke (%d) - blocks sight, full cover", f), C::LtGray});
        { bool danger = false; for (auto& w : cb.windups) for (auto& q : w.tiles) if (!danger && q.first == tx && q.second == ty) { const SpellDef* ws = gd->spell(w.spell); lines.push_back({std::string("DANGER: ") + (ws ? ws->name : "attack") + " lands here next turn", C::Red}); danger = true; } }
        if (map->dark) lines.push_back({!seen ? "Dark - can't see" : lit(tx, ty) ? "Lit" : "In shadow (-25% to hit)", lit(tx, ty) ? C::Cream : C::Purple});
    }
    int w = 210; for (auto& l : lines) w = std::max(w, ctx.font->width(l.first) + 14);
    int h = 8 + (int)lines.size() * 12, x = 4, y = 28;
    cv.fillRect(x, y, w, h, RGB(10, 10, 18, 215)); cv.rect(x, y, w, h, C::DkGray);
    for (size_t i = 0; i < lines.size(); i++) ctx.font->draw(cv, *ctx.ts, x + 7, y + 3 + (int)i * 12, lines[i].first, lines[i].second);
    uiRect(x, y, w, h);
}

// ---------------- 1.4.0 test hooks ----------------
std::string Game::combatProbe(const std::string& what) {
    auto q = split(what, ':'); auto I = [&](size_t k) { return k < q.size() ? atoi(q[k].c_str()) : -1; };
    auto unit = [&](int i) -> Combatant* { return inCombat() && i >= 0 && i < (int)cb.units.size() ? &cb.units[i] : nullptr; };
    const std::string& k = q[0];
    if (k == "units") return std::to_string(cb.units.size());
    if (k == "unit") { Combatant* u = unit(I(1)); if (!u) return "none"; return fmt("%d,%d,%d,%d,%d", u->x, u->y, u->ch.hp, u->ch.alive ? 1 : 0, u->friendly() ? 1 : 0); }
    if (k == "hp") { Combatant* u = unit(I(1)); return u ? std::to_string(u->ch.hp) : "-1"; }
    if (k == "alive") { Combatant* u = unit(I(1)); return u && u->ch.alive ? "1" : "0"; }
    if (k == "pos") { Combatant* u = unit(I(1)); return u ? fmt("%d,%d", u->x, u->y) : "none"; }
    if (k == "fx") { Combatant* u = unit(I(1)); if (!u) return "none"; std::string o; for (auto& e : u->ch.effects) o += (o.empty() ? "" : ",") + e.id; return o.empty() ? "none" : o; }
    if (k == "range") { Combatant* u = unit(I(1)); return u ? std::to_string(u->ch.attackRange(*gd)) : "-1"; }
    if (k == "move") { Combatant* u = unit(I(1)); return u ? std::to_string(u->ch.move) : "-1"; }
    if (k == "ap") { Combatant* u = unit(I(1)); return u ? std::to_string(u->ch.ap) : "-1"; }
    if (k == "hit" || k == "why" || k == "cover" || k == "exposed" || k == "flanked" || k == "target") {
        Combatant* a = unit(I(1)); Combatant* d = unit(I(2)); if (!a || !d) return "-1";
        if (k == "hit") return std::to_string(hitChance(*a, *d, false));
        if (k == "why") { std::string s = hitBreakdown(*a, *d, false); return s.empty() ? "none" : trim(s); }
        if (k == "cover") return std::to_string(effCover(*a, *d));
        if (k == "exposed") return exposed(*a, *d) ? "1" : "0";
        if (k == "flanked") return flanked(*a, *d) ? "1" : "0";
        return targetable(*a, *d) ? "1" : "0";
    }
    if (k == "spellcover") { Combatant* a = unit(I(1)); Combatant* d = unit(I(2)); const SpellDef* sp = q.size() > 3 ? gd->spell(q[3]) : nullptr; if (!a || !d || !sp) return "-1"; return std::to_string(spellCoverPct(*a, *d, castSpell(a->ch, *sp))); }
    if (k == "ground") { int x = I(1), y = I(2); return fmt("fire:%d,ice:%d,smoke:%d", groundAt(x, y, 1), groundAt(x, y, 2), groundAt(x, y, 3)); }
    if (k == "cell") { int x = I(1), y = I(2); if (!map || !map->inBounds(x, y)) return "none"; return allTileKinds()[map->at(x, y).kind].name; }
    if (k == "reachcover") { int x = I(1), y = I(2); for (size_t i = 0; i < cb.reachable.size(); i++) if (cb.reachable[i].first == x && cb.reachable[i].second == y) return std::to_string(i < cb.reachCover.size() ? cb.reachCover[i] : 0); return "-1"; }
    if (k == "goal") return cb.goal.kind.empty() ? "none" : cb.goal.kind + (cb.goal.done ? ",done" : cb.goal.failed ? ",failed" : ",open");
    if (k == "surprise") return std::to_string(cb.surprise);
    if (k == "windups") return std::to_string(cb.windups.size());
    if (k == "windtiles") { std::string o; for (auto& w : cb.windups) for (auto& t : w.tiles) o += (o.empty() ? "" : ";") + fmt("%d,%d", t.first, t.second); return o.empty() ? "none" : o; }
    if (k == "active") return std::to_string(cb.selectedUnit);
    if (k == "round") return std::to_string(cb.round);
    if (k == "spelltiles") { const SpellDef* sp = q.size() > 2 ? gd->spell(q[1]) : nullptr; int ci = I(2); if (!sp || !unit(ci)) return "none"; std::string o; for (auto& t : spellTiles(ci, castSpell(cb.units[ci].ch, *sp), I(3), I(4))) o += (o.empty() ? "" : ";") + fmt("%d,%d", t.first, t.second); return o.empty() ? "none" : o; }
    if (k == "chain") { const SpellDef* sp = q.size() > 2 ? gd->spell(q[1]) : nullptr; int ci = I(2); if (!sp || !unit(ci)) return "none"; std::string o; for (int v : chainTargets(ci, castSpell(cb.units[ci].ch, *sp), I(3))) o += (o.empty() ? "" : ",") + std::to_string(v); return o.empty() ? "none" : o; }
    if (k == "spell") { const SpellDef* sp = q.size() > 2 ? gd->spell(q[1]) : nullptr; int mi = I(2); if (!sp || mi < 0 || mi >= (int)party.members.size()) return "none"; SpellDef s = castSpell(party.members[mi], *sp); return fmt("area=%d,shape=%s,ground=%s,effect=%s,dmg=%d-%d", s.area, s.shape.c_str(), s.ground.c_str(), s.effect.c_str(), s.dmgMin, s.dmgMax); }
    // 1.5.0 (#83 #92 #93 #94)
    if (k == "crit" || k == "critaim") { Combatant* a = unit(I(1)); Combatant* d = unit(I(2)); return a && d ? std::to_string(critChance(*a, *d, k == "critaim")) : "-1"; }
    if (k == "apcost") { Combatant* u = unit(I(1)); return u ? std::to_string(u->ch.attackAP(*gd)) : "-1"; }
    if (k == "spellfail") { Combatant* u = unit(I(1)); return u ? std::to_string(u->ch.spellFail(*gd)) : "-1"; }
    if (k == "evasion") { Combatant* u = unit(I(1)); return u ? std::to_string(u->ch.evasion(*gd)) : "-1"; }
    if (k == "wresist") { Combatant* t = unit(I(1)); Combatant* a = unit(I(2)); if (!t || !a) return "none"; int lo, hi, ty; a->ch.attackDamage(*gd, lo, hi, ty);
        int r = t->ch.resist(ty, *gd); std::string tr = a->ch.trait(*gd); if (ty == DT_PHYS && !tr.empty()) { auto f = t->ch.traitRes.find(tr); if (f != t->ch.traitRes.end()) r = f->second; } return std::to_string(r); }
    if (k == "known") { Combatant* t = unit(I(1)); if (!t) return "0"; auto it = extra.find("rk_" + t->ch.creatureId); return it == extra.end() ? "0" : it->second; }
    if (k == "wounds") { int mi = I(1); return mi >= 0 && mi < (int)party.members.size() ? std::to_string(party.members[mi].wounds) : "-1"; }
    if (k == "maxhp") { int mi = I(1); return mi >= 0 && mi < (int)party.members.size() ? std::to_string(party.members[mi].maxHP(*gd)) : "-1"; }
    if (k == "gearwarn") { int mi = I(1); const ItemDef* d = q.size() > 2 ? gd->item(q[2]) : nullptr; if (mi < 0 || mi >= (int)party.members.size() || !d) return "none"; std::string w = gearWarning(*d, party.members[mi]); return w.empty() ? "none" : w; }
    if (k == "memberfx") { int mi = I(1); if (mi < 0 || mi >= (int)party.members.size() || q.size() < 3) return "-1"; return std::to_string(party.members[mi].fx(q[2], *gd)); }
    if (k == "knows") { int mi = I(1); if (mi < 0 || mi >= (int)party.members.size() || q.size() < 3) return "0"; auto& s = party.members[mi].spells; return std::find(s.begin(), s.end(), q[2]) != s.end() ? "1" : "0"; }
    return "?";
}
std::string Game::combatDo(const std::vector<std::string>& a) {
    auto I = [&](size_t k) { return k < a.size() ? atoi(a[k].c_str()) : 0; };
    std::string c = a.size() > 1 ? a[1] : "";
    auto ok = [&](int i) { return inCombat() && i >= 0 && i < (int)cb.units.size(); };
    if (c == "forcehit") { testForceHit = I(2); return "forcehit " + std::to_string(testForceHit); }
    if (c == "cellkind") { if (map) { map->set(I(2), I(3), a.size() > 4 ? a[4] : "floor_stone", rng); cb.reachDirty = true; computeLight(); } return "cell"; }
    if (c == "arena") { if (map) for (int y = I(3); y < I(3) + I(5); y++) for (int x = I(2); x < I(2) + I(4); x++) map->set(x, y, a.size() > 6 ? a[6] : "floor_stone", rng); computeLight(); return "arena"; }
    if (c == "rank") { int mi = I(2); if (mi < 0 || mi >= (int)party.members.size() || a.size() < 5) return "bad"; Character& m = party.members[mi]; m.ranks[a[3]] = I(4); m.level = 0; for (auto& r : m.ranks) m.level += r.second;
        if (const ClassDef* cd = gd->cls(a[3])) { for (auto& s0 : cd->startSpells) if (std::find(m.spells.begin(), m.spells.end(), s0) == m.spells.end()) m.spells.push_back(s0); for (auto& sl : cd->spellLevels) if (sl.first <= I(4) && std::find(m.spells.begin(), m.spells.end(), sl.second) == m.spells.end()) m.spells.push_back(sl.second); }
        for (auto& u : cb.units) if (inCombat() && u.partyIndex == mi) u.ch = m; return "rank"; }
    if (c == "equip") { int mi = I(2); if (mi < 0 || mi >= (int)party.members.size() || a.size() < 4) return "bad"; const ItemDef* d = gd->item(a[3]); if (!d || d->slot() < 0) return "bad item"; party.members[mi].equip[d->slot()] = a[3]; for (auto& u : cb.units) if (inCombat() && u.partyIndex == mi) u.ch.equip[d->slot()] = a[3]; return "equip"; }
    if (c == "surprise") { nextSurprise = I(2); return "surprise"; }
    if (c == "dark") { if (map) { map->dark = I(2) != 0; computeLight(); } return "dark"; }
    if (!inCombat()) return "not in combat";
    if (c == "attack") { int x = I(2), t = I(3); if (!ok(x) || !ok(t)) return "bad unit"; cb.units[x].ch.ap += 99; bool hit = combatAttack(x, t, a.size() > 4 && a[4] == "aimed"); cb.units[x].ch.ap -= 99; if (cb.units[x].ch.ap < 0) cb.units[x].ch.ap = 0; return hit ? "hit" : "miss"; }
    if (c == "cast") { int x = I(2); if (!ok(x) || a.size() < 6) return "bad"; auto& u = cb.units[x]; int sap = u.ch.ap, smp = u.ch.mp, slv = u.ch.level; u.ch.ap += 99; u.ch.mp += 999; u.ch.level = std::max(u.ch.level, 30); combatCast(x, a[3], I(4), I(5)); cb.units[x].ch.ap = sap; cb.units[x].ch.mp = smp; cb.units[x].ch.level = slv; return "cast " + a[3]; }
    if (c == "status") { int x = I(2); if (!ok(x) || a.size() < 4) return "bad"; cb.units[x].ch.addEffect(a[3], a.size() > 4 ? I(4) : 3, a.size() > 5 ? I(5) : 3); return "status"; }
    if (c == "clearfx") { int x = I(2); if (!ok(x)) return "bad"; cb.units[x].ch.effects.clear(); return "cleared"; }
    if (c == "hp") { int x = I(2); if (!ok(x)) return "bad"; cb.units[x].ch.hp = I(3); return "hp"; }
    if (c == "item") { int x = I(2); if (!ok(x) || a.size() < 6) return "bad"; party.addItem(a[3], 1, *gd); cb.units[x].ch.ap += 99; combatUseItem(x, a[3], combatUnitAt(I(4), I(5)), I(4), I(5)); cb.units[x].ch.ap -= 99; if (cb.units[x].ch.ap < 0) cb.units[x].ch.ap = 0; return "item"; }
    if (c == "turn") { int x = I(2); if (!ok(x)) return "bad"; for (size_t k = 0; k < cb.order.size(); k++) if (cb.order[k] == x) cb.turnPos = (int)k; cb.selectedUnit = x; cb.aiPath.clear(); combatBeginTurn(); return "turn " + std::to_string(x); }
    if (c == "reach") { combatComputeReachable(); cb.reachDirty = false; return fmt("reach %d", (int)cb.reachable.size()); }
    if (c == "step") { int x = I(2); if (!ok(x)) return "bad"; cb.units[x].ch.move += 5; combatStepUnit(x, I(3), I(4)); return "step"; }
    if (c == "windup") { int x = I(2), t = I(3); if (!ok(x) || !ok(t)) return "bad"; windupStart(x, t); return "windup"; }
    if (c == "goal") { if (a.size() < 3) return "bad"; EnemyGroup g; g.goal = a[2]; cb.goal = CombatState::Goal(); goalSetup(g, -1); return "goal " + cb.goal.kind; }
    if (c == "roundtick") { groundRoundTick(); goalCheck(true); return "tick"; }
    if (c == "end") { combatEndTurn(); return "end"; }
    return "?";
}
