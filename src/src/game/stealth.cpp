// 1.5.0 (#99) stealth and avoidance: ways past a group other than fighting it.
//  * noticing   - groupAggro(): how close the party can get before a group notices it (stealth, darkness, a noise)
//  * noise      - a thrown noisemaker (item effect=lure[:STEPS[:RADIUS]]) pulls nearby groups to where it lands; they
//                 stay there, looking, for STEPS party steps, then walk home. Aimed on the map like a spell.
//  * traps      - [trap] objects. A group that walks onto one while chasing the party, walking to a noise or walking
//                 home loses hurt% of its HP (100 = gone); the party stepping on it gets hurt instead.
// None of this is saved: a loaded game starts with every group at home and every trap set (flag= keeps one sprung).
#include "game.h"
#include "../core/audio.h"
#include "tiles.h"
#include <algorithm>

using namespace UI;

// ---- noticing ----
int Game::groupAggro(const EnemyGroup& e) const {
    int a = e.aggro;
    if (e.distractSteps > 0) return 1;                        // busy looking at a noise: only notices a party right beside it
    if (stealthSteps > 0) a = std::max(1, a / 3);              // Camouflage / a stealth draught
    // a dark map and no light on the party: what can't see in the dark only notices the party at half the distance
    if (map && map->dark && !evalCond("lit")) {
        bool sees = false; for (auto& c : e.creatures) if (const CreatureDef* cd = gd->creature(c)) if (cd->darkvision) sees = true;
        if (!sees) a = std::max(1, a / 2);
    }
    return a;
}

// ---- noise ----
int Game::distractGroups(int x, int y, int steps, int radius) {
    if (!map) return 0;
    int n = 0;
    for (auto& e : map->enemies) {
        if (!groupShown(e)) continue;
        if (groupHasBoss(e)) continue;                          // a boss isn't fooled by a rattle in the dark
        if (chebyshev(e.x, e.y, x, y) > radius) continue;
        e.distractX = x; e.distractY = y; e.distractSteps = steps; e.chasing = false; e.returning = false; e.wanderTimer = 0;
        n++;
    }
    return n;
}
// the item's lure numbers: effect=lure[:STEPS[:RADIUS]] (default 12 steps, radius 7); range= is how far it can be thrown (default 6)
static void lureNumbers(const ItemDef& d, int& steps, int& radius, int& range) {
    steps = 12; radius = 7; range = d.range > 1 ? d.range : 6;
    auto p = split(d.effect, ':');
    if (p.size() > 1 && atoi(p[1].c_str()) > 0) steps = atoi(p[1].c_str());
    if (p.size() > 2 && atoi(p[2].c_str()) > 0) radius = atoi(p[2].c_str());
}
bool Game::throwTargetOk(int x, int y) const {
    const ItemDef* d = gd->item(throwItem); if (!d || !map || !map->inBounds(x, y)) return false;
    int steps, radius, range; lureNumbers(*d, steps, radius, range);
    if (chebyshev(px, py, x, y) > range || (x == px && y == py)) return false;
    if (!map->walkable(x, y) && !map->at(x, y).water) return false;   // it lands on the floor (or in water: a splash is a noise too)
    if (map->dark && !visible(x, y)) return false;
    return lineOfSight(*map, px, py, x, y);
}
void Game::startThrowAim(const std::string& itemId) {
    while (mode == GMode::MENU || mode == GMode::PAUSE) popMode();
    throwItem = itemId; throwX = px; throwY = py;
    int fx = facing == 0 ? -1 : facing == 1 ? 1 : 0, fy = facing == 2 ? -1 : facing == 3 ? 1 : 0;
    for (int k = 4; k >= 1; k--) if (throwTargetOk(px + fx * k, py + fy * k)) { throwX = px + fx * k; throwY = py + fy * k; break; }
    const ItemDef* d = gd->item(itemId);
    log("Throw the " + (d ? d->name : itemId) + ": pick a spot (arrows or mouse), " + gKeys.label(A_CONFIRM) + " to throw, " + gKeys.label(A_CANCEL) + " to keep it.");
}
bool Game::updateThrowAim(InputState& in) {
    if (throwItem.empty()) return false;
    const ItemDef* d = gd->item(throwItem);
    if (!d || !party.hasItem(throwItem)) { throwItem.clear(); return false; }
    int steps, radius, range; lureNumbers(*d, steps, radius, range);
    if (actP(in, A_CANCEL) || in.mousePressed[1]) { log("You keep the " + d->name + "."); throwItem.clear(); in.eatKeys(); in.eatMouse(); return true; }
    int nx = throwX, ny = throwY;
    if (actP(in, A_LEFT)) nx--; if (actP(in, A_RIGHT)) nx++; if (actP(in, A_UP)) ny--; if (actP(in, A_DOWN)) ny++;
    if (in.mouseDX || in.mouseDY || in.mousePressed[0]) { int mx = (in.mouseX + camX) / TILE, my = (in.mouseY + camY) / TILE; if (in.mouseY < SCREEN_H - 72) { nx = mx; ny = my; } }
    if (map->inBounds(nx, ny) && chebyshev(px, py, nx, ny) <= range) { throwX = nx; throwY = ny; }
    if (actP(in, A_CONFIRM) || actP(in, A_INTERACT) || (in.mousePressed[0] && in.mouseY < SCREEN_H - 72)) {
        if (throwTargetOk(throwX, throwY)) throwLure(throwItem, throwX, throwY);
        else { log("It can't land there - pick a spot you can see, within reach."); gAudio.play("ui_error"); }
        in.eatKeys(); in.eatMouse();
    }
    return true;
}
void Game::throwLure(const std::string& itemId, int x, int y) {
    const ItemDef* d = gd->item(itemId); if (!d || !map) return;
    int steps, radius, range; lureNumbers(*d, steps, radius, range);
    party.removeItem(itemId, 1); throwItem.clear();
    gAudio.play(d->sound.empty() ? "lure" : d->sound);
    addFloat(x, y, "*clatter*", C::Yellow);
    int n = distractGroups(x, y, steps, radius);
    bool seen = false; for (auto& e : map->enemies) if (e.distractSteps > 0 && e.distractX == x && e.distractY == y && visible(e.x, e.y)) seen = true;
    if (n == 0) log("The " + d->name + " clatters away. Nothing comes to look.");
    else log(seen ? "Heads turn toward the noise - they go to look." : "Something stirs in the dark and moves toward the noise.");
}

// a group's step on the map while it is busy with a noise or walking home after one (called from updateNpcs)
bool Game::stepDistracted(EnemyGroup& e) {
    if (e.distractSteps > 0) {
        if (e.x != e.distractX || e.y != e.distractY) { int ox = e.x, oy = e.y; groupStepToward(e, e.distractX, e.distractY, 0, -1); if (e.x == px && e.y == py) { e.x = ox; e.y = oy; } groupOnTrap(e); }
        return true;
    }
    if (e.returning) {
        if (chebyshev(e.x, e.y, e.homeX, e.homeY) <= 1 || !groupStepToward(e, e.homeX, e.homeY, 0, -1)) e.returning = false;
        else groupOnTrap(e);
        return true;
    }
    return false;
}
// one party step: the noise holds them a step less
void Game::distractionStep() {
    if (!map) return;
    for (auto& e : map->enemies) if (e.distractSteps > 0 && --e.distractSteps == 0) { e.returning = true; e.wanderTimer = 0; }
}

// ---- traps ----
void Game::resetTraps(Map& m) { for (auto& t : m.traps) t.sprung = !t.flag.empty() && hasFlag(t.flag); }
static std::string groupName(const GameData& gd, const EnemyGroup& e) {
    const CreatureDef* cd = e.creatures.empty() ? nullptr : gd.creature(e.creatures[0]);
    std::string n = cd ? cd->name : std::string("Something"); if (e.creatures.size() > 1) n += fmt(" and %d more", (int)e.creatures.size() - 1);
    return n;
}
void Game::springTrap(Trap& t) {
    t.sprung = true; if (!t.flag.empty()) setFlag(t.flag);
    gAudio.play("trap");
    if (!t.onSpring.empty()) runScript(t.onSpring);
}
bool Game::groupOnTrap(EnemyGroup& e) {
    if (!map || e.defeated) return false;
    Trap* t = map->trapAt(e.x, e.y); if (!t) return false;
    std::string who = groupName(*gd, e); bool seen = visible(e.x, e.y);
    int hurt = t->hurt; bool boss = groupHasBoss(e); if (boss) hurt = std::min(hurt, 50);   // a boss is hurt, never killed, by a trap
    springTrap(*t);
    if (!t->text.empty() && seen) log(t->text);
    if (hurt >= 100 || e.hurt + hurt >= 100) {
        e.defeated = true; e.distractSteps = 0; e.chasing = false;
        if (!e.flag.empty()) setFlag(e.flag);
        int xp = 0; for (auto& c : e.creatures) if (const CreatureDef* cd = gd->creature(c)) xp += cd->xp / 2;
        log(seen ? who + (e.creatures.size() > 1 ? " are" : " is") + " caught by the " + toLower(t->name) + " and " + (e.creatures.size() > 1 ? "don't" : "doesn't") + " get up." : "From somewhere out of sight: a crash, a scream, then nothing.");
        if (xp > 0) giveXP(xp);
        addFloat(e.x, e.y, "trapped!", C::Yellow);
        if (!e.onDefeatScript.empty()) runScript(e.onDefeatScript);
    } else {
        e.hurt = std::min(90, e.hurt + hurt);
        if (seen) log(who + (e.creatures.size() > 1 ? " are" : " is") + " caught by the " + toLower(t->name) + fmt(" (-%d%% HP for the fight).", hurt));
        addFloat(e.x, e.y, fmt("-%d%%", hurt), C::Orange);
    }
    return true;
}
void Game::partyOnTrap() {
    if (!map) return;
    Trap* t = map->trapAt(px, py); if (!t) return;
    springTrap(*t);
    if (t->damage > 0) { for (auto& m : party.members) if (m.alive) m.hp = std::max(1, m.hp - t->damage); shake = std::max(shake, 0.25f); }
    log("You step on the " + toLower(t->name) + (t->damage > 0 ? fmt("! Everyone takes %d damage.", t->damage) : "."));
}
// the HP a trap took off a group, applied to its fighters when the fight starts
void Game::applyGroupHurt(Combatant& u, int groupIndex) {
    if (!map || groupIndex < 0 || groupIndex >= (int)map->enemies.size()) return;
    int h = map->enemies[groupIndex].hurt; if (h <= 0) return;
    u.ch.hp = std::max(1, u.ch.hp * (100 - h) / 100);
}

// ---- drawing ----
void Game::drawTraps(Canvas& cv, int cx, int cy) {
    if (!map) return;
    for (auto& t : map->traps) {
        if (!map->inBounds(t.x, t.y) || !map->at(t.x, t.y).explored) continue;
        bool vis = visible(t.x, t.y); int sx = t.x * TILE - cx, sy = t.y * TILE - cy;
        Tile pic = t.sprung && t.sprungSprite.valid() ? t.sprungSprite : t.sprite;
        if (pic.valid()) { Color8 c = t.hasColor ? t.color : C::White; if (!vis) c = RGB(c.r / 3, c.g / 3, c.b / 3); ctx.ts->drawRecolor(cv, pic, sx, sy, 2, c); continue; }
        // drawn: a dark pit with a row of spikes (armed) or a jumble (sprung)
        Color8 rim = t.hasColor ? t.color : RGB(170, 160, 150); if (!vis) rim = RGB(rim.r / 3, rim.g / 3, rim.b / 3);
        cv.fillRect(sx + 4, sy + 4, TILE - 8, TILE - 8, RGB(12, 10, 10)); cv.rect(sx + 4, sy + 4, TILE - 8, TILE - 8, RGB(rim.r / 2, rim.g / 2, rim.b / 2));
        if (!t.sprung) for (int i = 0; i < 4; i++) { int bx = sx + 7 + i * 5; cv.line(bx, sy + TILE - 8, bx + 2, sy + 9, rim); cv.line(bx + 4, sy + TILE - 8, bx + 2, sy + 9, rim); }
        else { cv.line(sx + 7, sy + TILE - 8, sx + TILE - 8, sy + 10, rim); cv.line(sx + 9, sy + 10, sx + TILE - 9, sy + TILE - 9, rim); }
    }
}
void Game::drawThrowAim(Canvas& cv) {
    if (throwItem.empty() || !map || mode != GMode::EXPLORE) return;
    const ItemDef* d = gd->item(throwItem); if (!d) return;
    int steps, radius, range; lureNumbers(*d, steps, radius, range);
    bool ok = throwTargetOk(throwX, throwY);
    int sx = throwX * TILE - camX, sy = throwY * TILE - camY;
    // the reach of the noise, then the spot
    int r0x = (throwX - radius) * TILE - camX, r0y = (throwY - radius) * TILE - camY, rw = (radius * 2 + 1) * TILE;
    if (ok) cv.rect(r0x, r0y, rw, rw, RGB(140, 120, 40));
    for (auto& e : map->enemies) if (ok && groupShown(e) && !groupHasBoss(e) && chebyshev(e.x, e.y, throwX, throwY) <= radius && visible(e.x, e.y)) cv.rect(e.x * TILE - camX + 1, e.y * TILE - camY + 1, TILE - 2, TILE - 2, C::Yellow);
    Color8 c = ok ? C::Yellow : C::Red;
    cv.rect(sx, sy, TILE, TILE, c); cv.rect(sx + 1, sy + 1, TILE - 2, TILE - 2, c);
    std::string hint = "Throw " + d->name + ": arrows / mouse aim, " + gKeys.label(A_CONFIRM) + " throws, " + gKeys.label(A_CANCEL) + " cancels";
    int w = ctx.font->width(hint) + 12; uiRect(SCREEN_W / 2 - w / 2, 6, w, 18);
    cv.fillRect(SCREEN_W / 2 - w / 2, 6, w, 18, RGB(20, 20, 30, 220)); cv.rect(SCREEN_W / 2 - w / 2, 6, w, 18, C::Gold);
    UI::text(ctx, SCREEN_W / 2 - w / 2 + 6, 9, hint, ok ? C::Cream : C::LtGray);
}
