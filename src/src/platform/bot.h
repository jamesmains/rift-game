// Playthrough bot for the headless driver: walks the party across maps with real key presses, talks to NPCs, picks
// dialogue options by their text and lets the combat AI fight. Used by tests/main_quest.txt and tests/pacing_*.txt.
//
//   newgame NAME CLASS [DIFF] [SEED]  start a fresh game directly (skips the desktop), then settle through the intro
//   settle                      run until the party is free to walk (fights, level-ups, messages, cutscenes handled)
//   goto MAP                    walk there through exits (overworld included); fails if no route or a path is blocked
//   walk X Y                    walk to a tile on the current map
//   talk NPC_ID                 walk up to an NPC and talk (bump into it)
//   bump X Y                    walk next to a tile and press into it (levers, obstacles, chests, enemies, doors)
//   fight FLAG                  walk into the map enemy group with that flag= and fight it to the end
//   choose TEXT                 pick the dialogue option containing TEXT (case-insensitive), then settle
//   waituntil HOUR              let time pass until that hour (shops open...)
//   careful 0/1                 (default 1) torches, potions, inns, restocking and better gear like a careful player;
//                               towns reached with goto are visited for that automatically ('prep' does it by hand)
//   checkpoint NAME             print a pacing line (time, steps, level, gold, kills...) and remember it for the report
//   report FILE                 write every checkpoint as CSV into the output directory
//
// Any failure (no path, option missing, party wiped) counts as an EXPECT failure, so the run exits with code 1.
#pragma once
#include "../app.h"
#include <deque>
#include <functional>
#include <set>
#include <algorithm>

struct Bot {
    App& app; InputState& in; std::function<void(int)> step; int& fails;
    bool verbose = false, strict = false;
    bool autoEnding = true;                // settle clicks through the ending (off: the script drives it, for screenshots)
    bool careful = true;                   // play like a careful player: torches underground, potions, inns, restocking, better gear   // strict: stop the script at the first failure
    long frames = 0;                       // simulated frames since newgame (60 per second)
    struct Check { std::string name; long frames; int playSec, steps, day, clock, level, members, gold, kills, fights, fled, ko, xp; std::string map; };
    std::vector<Check> checks;
    Bot(App& a, InputState& i, std::function<void(int)> s, int& f) : app(a), in(i), step(s), fails(f) {}

    Game& g() { return app.game; }
    void run(int n) { step(n); frames += n; }
    void tap(Key k, int after = 1) { in.press(k); run(1); in.release(k); run(after); }
    void fail(const std::string& why) { fails++; printf("BOT FAIL: %s (map=%s pos=%d,%d mode=%d)\n", why.c_str(), g().mapId.c_str(), g().px, g().py, (int)g().mode); }

    // ---- level-ups: a rank in the member's own class, stat points into the class's two best stats ----
    void autoLevel() {
        Game& G = g();
        while (!G.levelUpQueue.empty()) {
            Character& m = G.party.members[G.levelUpQueue[0]];
            const ClassDef* c = G.gd->cls(m.classId);
            int a = ST_STR, b = ST_CON;
            if (c) { std::vector<int> idx = {0, 1, 2, 3, 4, 5}; std::stable_sort(idx.begin(), idx.end(), [&](int x, int y) { return c->stats[x] > c->stats[y]; }); a = idx[0]; b = idx[1]; }
            for (int i = 0; i < m.statPoints; i++) m.baseStats[i % 3 == 2 ? b : a]++;
            m.statPoints = 0;
            while (m.rankPoints > 0) { G.applyRank(m, m.classId.empty() ? G.gd->classOrder[0] : m.classId); m.rankPoints--; }
            m.mp = m.maxMP(*G.gd);
            G.levelUpQueue.erase(G.levelUpQueue.begin());
        }
        if (G.mode == GMode::LEVELUP) G.popMode();
    }

    // ---- careful-player habits (outside fights) ----
    int healingPotions() { Game& G = g(); int n = 0; for (auto& sl : G.party.inventory) if (auto* d = G.gd->item(sl.id)) if (d->type == IT_CONSUMABLE && d->heal > 0 && d->effect.empty()) n += sl.count; return n; }
    void tend() {
        Game& G = g(); if (!careful || G.mode != GMode::EXPLORE || !G.map) return;
        if (G.map->dark && G.lightSteps < 20 && G.party.hasItem("torch_item")) G.useItemOn("torch_item", G.party.members[0], false);
        for (auto& m : G.party.members) {
            if (!m.alive) { for (auto& sl : std::vector<ItemStack>(G.party.inventory)) if (auto* d = G.gd->item(sl.id)) if (d->effect == "revive") { G.useItemOn(sl.id, m, false); break; } }
            for (int guard = 0; guard < 6 && m.alive && m.hp * 2 < m.maxHP(*G.gd); guard++) {
                // the smallest potion that covers most of the wound
                std::string pick; int pickHeal = 0, need = m.maxHP(*G.gd) - m.hp;
                for (auto& sl : G.party.inventory) if (auto* d = G.gd->item(sl.id)) if (d->type == IT_CONSUMABLE && d->heal > 0 && d->effect.empty()) {
                    bool better = pick.empty() || (pickHeal < need * 2 / 3 ? d->heal > pickHeal : (d->heal >= need * 2 / 3 && d->heal < pickHeal));
                    if (better) { pick = sl.id; pickHeal = d->heal; } }
                if (pick.empty()) break;
                G.useItemOn(pick, m, false);
            }
        }
    }
    int gearScore(const ItemDef& d) {
        int sc = d.def * 10 + d.evasion * 3 + (d.dmgMin + d.dmgMax) * 5 + d.accuracy + d.crit + d.bonusHP + d.bonusMP / 2 + d.bonusAP * 15 + d.bonusMove * 10;
        for (int i = 0; i < ST_COUNT; i++) sc += d.bonus[i] * 6; for (int i = 0; i < DT_COUNT; i++) sc += d.res[i] / 3;
        return sc;
    }
    void equipBest() {
        Game& G = g();
        for (size_t mi = 0; mi < G.party.members.size(); mi++) {
            Character& m = G.party.members[mi];
            for (int pass = 0; pass < 8; pass++) {
                std::string bestId; int bestGain = 0;
                for (auto& sl : G.party.inventory) {
                    const ItemDef* d = G.gd->item(sl.id); if (!d || !d->equippable() || d->light > 0) continue;
                    int slot = d->slot(); const ItemDef* cur = m.equip[slot].empty() ? nullptr : G.gd->item(m.equip[slot]);
                    if (slot == SL_WEAPON && cur) { if ((d->range > 1) != (cur->range > 1) || d->isGun != cur->isGun || d->scaleStat != cur->scaleStat) continue; if (d->twoHanded && !m.equip[SL_OFFHAND].empty()) continue; }
                    if (slot == SL_OFFHAND) { const ItemDef* w = m.equip[SL_WEAPON].empty() ? nullptr : G.gd->item(m.equip[SL_WEAPON]); if (w && (w->twoHanded || w->range > 1)) continue; }
                    int gain = gearScore(*d) - (cur ? gearScore(*cur) : 0);
                    if (gain > bestGain) { bestGain = gain; bestId = sl.id; }
                }
                if (bestId.empty()) break;
                if (verbose) printf("  bot: %s equips %s\n", m.name.c_str(), G.gd->item(bestId)->name.c_str());
                G.equipItem((int)mi, bestId);
            }
        }
    }
    // the first option of this NPC's opening node that leads (directly) to a node with one of these actions
    std::string optionLeadingTo(const NPC& n, const std::vector<std::string>& actions, std::string* shopOut = nullptr) {
        Game& G = g(); auto d = G.dialogues.find(n.dialogue); if (d == G.dialogues.end()) return "";
        auto st = d->second.nodes.find("start"); if (st == d->second.nodes.end()) return "";
        for (auto& o : st->second.options) {
            bool ok = true; for (auto& c : o.conds) if (!G.evalCond(c)) ok = false; if (!ok) continue;
            auto t = d->second.nodes.find(o.target); if (t == d->second.nodes.end()) continue;
            for (auto& a : t->second.actions) if (std::find(actions.begin(), actions.end(), a.first) != actions.end()) { if (shopOut) *shopOut = a.second; return o.text; }
        }
        return "";
    }
    // in a town: rest at the inn when hurt, top up healing potions and torches, put on better gear
    void prep() {
        Game& G = g(); if (!careful || !settle()) return;
        equipBest();
        bool hurt = false; for (auto& m : G.party.members) if (!m.alive || m.hp < m.maxHP(*G.gd) * 9 / 10 || m.mp < m.maxMP(*G.gd) / 2) hurt = true;
        if (hurt) for (auto& n : G.map->npcs) { if (n.hidden) continue; std::string opt = optionLeadingTo(n, {"rest"}); if (opt.empty()) continue;
            if (talk(n.id) && G.mode == GMode::DIALOGUE) { choose(opt); if (verbose) printf("  bot: rested at %s\n", n.name.c_str()); } settle(); break; }
        if (G.clockMin < 9 * 60 || G.clockMin > 17 * 60) { int guard = 0; while (G.clockMin / 60 != 9 && guard++ < 100) G.advanceClock(15); }
        int want = 3 * (int)G.party.members.size();
        for (auto& n : G.map->npcs) {
            if (n.hidden) continue; std::string shop; std::string opt = n.shop.empty() ? optionLeadingTo(n, {"shop"}, &shop) : ""; if (!n.shop.empty()) shop = n.shop;
            if (shop.empty()) continue;
            auto stock = G.shopItems(shop); std::string potion, torch; int potionHeal = 0;
            for (auto& it : stock) if (auto* d = G.gd->item(it.first)) { if (d->type == IT_CONSUMABLE && d->heal > 0 && d->effect.empty() && G.itemBuyPrice(*d) * 4 <= G.party.gold && d->heal > potionHeal) { potion = it.first; potionHeal = d->heal; } if (it.first == "torch_item") torch = it.first; }
            int torches = G.party.countItem("torch_item");
            bool oilsWanted = false; for (auto& it : stock) if (auto* d = G.gd->item(it.first)) if (startsWith(d->effect, "imbue_") && d->effect != "imbue_poison" && G.party.countItem(it.first) < 2) oilsWanted = true;
            if ((potion.empty() || healingPotions() >= want) && (torch.empty() || torches >= 4) && !oilsWanted) continue;
            if (!talk(n.id)) continue; if (G.mode == GMode::DIALOGUE && !opt.empty()) choose(opt); if (G.mode != GMode::SHOP) { settle(); continue; }
            int budget = G.party.gold * 2 / 5;
            if (!torch.empty()) while (G.party.countItem("torch_item") < 4) { int pr = G.itemBuyPrice(*G.gd->item(torch)); if (pr > budget) break; budget -= pr; G.party.gold -= pr; G.stats.goldSpent += pr; G.party.addItem(torch, 1, *G.gd); }
            // 1.5.0 (#92): a careful player also carries a couple of each weapon oil the shop has (spirits, machines, bones)
            for (auto& it : stock) if (auto* d = G.gd->item(it.first)) if (startsWith(d->effect, "imbue_") && d->effect != "imbue_poison") while (G.party.countItem(it.first) < 2) { int pr = G.itemBuyPrice(*d); if (pr > budget) break; budget -= pr; G.party.gold -= pr; G.stats.goldSpent += pr; G.party.addItem(it.first, 1, *G.gd); }
            if (!potion.empty()) { int bought = 0; while (healingPotions() < want) { int pr = G.itemBuyPrice(*G.gd->item(potion)); if (pr > budget) break; budget -= pr; G.party.gold -= pr; G.stats.goldSpent += pr; G.party.addItem(potion, 1, *G.gd); bought++; } if (verbose && bought) printf("  bot: bought %d %s\n", bought, potion.c_str()); }
            settle();
        }
    }

    std::vector<int> dialogueOptions() {
        Game& G = g(); std::vector<int> avail;
        auto d = G.dialogues.find(G.dlgId); if (d == G.dialogues.end()) return avail;
        auto n = d->second.nodes.find(G.dlgNode); if (n == d->second.nodes.end()) return avail;
        for (size_t i = 0; i < n->second.options.size(); i++) { bool ok = true; for (auto& c : n->second.options[i].conds) if (!G.evalCond(c)) ok = false; if (ok) avail.push_back((int)i); }
        return avail;
    }
    std::string optionText(int i) { Game& G = g(); return G.dialogues[G.dlgId].nodes[G.dlgNode].options[i].text; }

    // Run until the party can walk again. Stops early (returning true) at a dialogue that has choices.
    bool settle(bool stopAtScreens = false, bool clickThrough = true, int limitSec = 900) {
        Game& G = g();
        for (long i = 0; i < (long)limitSec * 60; i++) {
            if (app.state != AppState::GAME) { if (G.gameOver) { dumpCombat(); fail("the party was wiped out"); return false; } fail("left the game (state " + std::to_string((int)app.state) + ")"); return false; }
            switch (G.mode) {
                case GMode::EXPLORE: if (G.levelUpQueue.empty()) { run(1); if (G.mode == GMode::EXPLORE) return true; } else { G.pushMode(GMode::LEVELUP); autoLevel(); } break;
                case GMode::COMBAT: G.autoCombat = true; run(1); if (G.cb.round > (G.godMode ? 200 : 60)) { dumpCombat(); fail("combat stalemate (too many rounds)"); return false; } break;
                case GMode::LEVELUP: autoLevel(); run(1); break;
                case GMode::VICTORY: case GMode::MESSAGE: tap(K_ENTER, 2); break;
                case GMode::CUTSCENE: G.cutTimer = 1.0f; tap(K_ENTER, 1); break;
                case GMode::TRANSITION: run(1); break;
                case GMode::DIALOGUE: {
                    auto opts = dialogueOptions();
                    // a node whose only way on is a single option, or whose options all just close it, is clicked through
                    bool closeOnly = !opts.empty(); for (int o : opts) if (G.dialogues[G.dlgId].nodes[G.dlgNode].options[o].target != "end") closeOnly = false;
                    if (clickThrough && (opts.size() == 1 || closeOnly)) { G.dlgTextShown = 1 << 20; G.dlgOptArmed = true; G.dlgSel = 0; tap(K_ENTER, 1); break; }
                    if (!opts.empty()) return true;
                    G.dlgTextShown = 1 << 20; tap(K_ENTER, 1); break;
                }
                case GMode::GAMEOVER: dumpCombat(); fail("the party was wiped out"); return false;
                case GMode::ENDING: {   // read every epilogue slide, skip the credits, then "keep exploring"
                    if (!autoEnding) return true;
                    int n = (int)G.endSlides.size();
                    if (G.endPage < n) printf("  ending: %s - %s\n", G.endSlides[G.endPage].title.c_str(), G.endSlides[G.endPage].text.substr(0, 70).c_str());
                    if (G.endPage == n + 1) G.endScroll = 1e6f;
                    if (G.endPage == n + 2) G.endSel = 0;
                    G.endT = 2.0f; tap(K_ENTER, 2); break;
                }
                default: if (stopAtScreens && (G.mode == GMode::SHOP || G.mode == GMode::CRAFT || G.mode == GMode::BOUNTY || G.mode == GMode::WORLDMAP)) return true; tap(K_ESCAPE, 2); break;
            }
        }
        if (G.mode == GMode::COMBAT) dumpCombat();
        fail("settle timed out"); return false;
    }
    void dumpCombat() { Game& G = g(); for (auto& m : G.party.members) printf("  member %s lvl %d hp %d/%d\n", m.name.c_str(), m.level, m.hp, m.maxHP(*G.gd)); { printf("  combat round %d, turn %d, active %d, anims %d:\n", G.cb.round, G.cb.turnPos, G.cb.selectedUnit, (int)G.cb.anims.size()); for (auto& u : G.cb.units) printf("    %-16s party=%d ally=%d pos=%d,%d hp=%d alive=%d ap=%d mv=%d\n", u.ch.name.c_str(), u.partyIndex, u.ally, u.x, u.y, u.ch.hp, u.ch.alive, u.ch.ap, u.ch.move); for (size_t i = G.cb.log.size() > 40 ? G.cb.log.size() - 40 : 0; i < G.cb.log.size(); i++) printf("    log: %s\n", G.cb.log[i].c_str()); }
    }

    bool choose(const std::string& want) {
        if (!settle(false, false)) return false;
        Game& G = g();
        if (G.mode != GMode::DIALOGUE) { fail("choose '" + want + "': not in a dialogue"); return false; }
        auto opts = dialogueOptions(); std::string lw = toLower(want);
        for (size_t k = 0; k < opts.size(); k++) if (toLower(optionText(opts[k])).find(lw) != std::string::npos) {
            if (verbose) printf("  bot: %s > %s\n", G.dlgId.c_str(), optionText(opts[k]).c_str());
            G.dlgTextShown = 1 << 20; G.dlgOptArmed = true; G.dlgSel = (int)k; tap(K_ENTER, 1);
            return settle(true);
        }
        std::string have; for (int o : opts) have += " [" + optionText(o) + "]";
        fail("no dialogue option '" + want + "' in " + G.dlgId + ":" + G.dlgNode + " - have" + have); return false;
    }

    // ---- walking ----
    bool doorPassable(const Door& d) {
        Game& G = g(); if (d.open) return true;
        if (!d.key.empty() && !G.party.hasItem(d.key)) return false;
        if (!d.require.empty() && !G.evalCond(d.require)) return false;
        if (!d.openIf.empty() && !G.evalCond(d.openIf)) return false;
        return true;
    }
    bool passable(int x, int y, const std::set<std::pair<int, int>>& goals) {
        Game& G = g(); Map* m = G.map;
        if (!m->inBounds(x, y)) return false;
        bool goal = goals.count({x, y}) > 0;
        if (m->npcAt(x, y) || m->chestAt(x, y)) return goal;
        if (m->enemyAt(x, y)) return true;   // walking into a group just starts that fight
        if (Door* d = m->doorAt(x, y)) return doorPassable(*d);
        if (!G.closedShopDoor(x, y).empty()) return false;
        if (!m->walkable(x, y)) return goal;
        if (m->exitAt(x, y) && !goal) return false;
        return true;
    }
    // BFS from the party to any goal tile; returns the path (excluding the start).
    std::vector<std::pair<int, int>> pathTo(const std::set<std::pair<int, int>>& goals) {
        Game& G = g(); Map* m = G.map; int W = m->w, H = m->h;
        std::vector<int> prev((size_t)W * H, -2); std::deque<int> q;
        int s = G.py * W + G.px; prev[s] = -1; q.push_back(s); int found = -1;
        while (!q.empty()) {
            int c = q.front(); q.pop_front(); int cx = c % W, cy = c / W;
            if (goals.count({cx, cy}) && c != s) { found = c; break; }
            static const int D[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
            for (auto& d : D) {
                int nx = cx + d[0], ny = cy + d[1]; if (!m->inBounds(nx, ny)) continue;
                int ni = ny * W + nx; if (prev[ni] != -2) continue;
                if (!passable(nx, ny, goals)) continue;
                // goal tiles that block (an NPC, a lever) are entered only as the last step
                prev[ni] = c; if (goals.count({nx, ny})) { found = ni; q.clear(); break; }
                q.push_back(ni);
            }
        }
        std::vector<std::pair<int, int>> path;
        if (found < 0) return path;
        for (int c = found; c != s && c >= 0; c = prev[c]) path.push_back({c % W, c / W});
        std::reverse(path.begin(), path.end());
        return path;
    }
    Key dirKey(int dx, int dy) { return dx > 0 ? K_RIGHT : dx < 0 ? K_LEFT : dy > 0 ? K_DOWN : K_UP; }

    // Walk until the party stands on (or, for blocking goals, has bumped into) a goal tile. 'bumpOnly': stop after the bump.
    enum WalkResult { W_ARRIVED, W_MAPCHANGED, W_FAILED };
    WalkResult walkTo(std::set<std::pair<int, int>> goals, const std::string& what, bool bump) {
        Game& G = g(); std::string startMap = G.mapId; int stuck = 0;
        for (int iter = 0; iter < 20000; iter++) {
            if (!settle()) return W_FAILED;
            if (G.mode == GMode::DIALOGUE) return W_ARRIVED;   // something started talking to us on the way (a trigger)
            if (G.mapId != startMap) return W_MAPCHANGED;
            if (goals.count({G.px, G.py})) return W_ARRIVED;
            tend();
            auto path = pathTo(goals);
            if (path.empty()) {   // perhaps a shop door locked for the night: wait for the morning and look again
                int guard = 0; G.advanceClock(15); while (G.clockMin / 60 != 10 && guard++ < 100) G.advanceClock(15);
                path = pathTo(goals);
            }
            if (path.empty()) { fail("no path to " + what); return W_FAILED; }
            auto nxt = path[0]; int ox = G.px, oy = G.py; GMode om = G.mode;
            bool last = path.size() == 1;
            Door* dr = G.map->doorAt(nxt.first, nxt.second); bool doorWasClosed = dr && !dr->open;
            G.moveRepeat = 0; tap(dirKey(nxt.first - G.px, nxt.second - G.py), 8);
            bool moved = G.px != ox || G.py != oy;
            if (last && !moved && goals.count(nxt)) { if (bump) return W_ARRIVED; }
            if (!moved && G.mode == om && !(doorWasClosed && dr->open)) { if (++stuck > 60) { fail("stuck walking to " + what); return W_FAILED; } run(10); }
            else stuck = 0;
        }
        fail("gave up walking to " + what); return W_FAILED;
    }
    bool walk(int x, int y) { return walkTo({{x, y}}, fmt("%d,%d", x, y), false) != W_FAILED; }
    bool bumpAt(int x, int y, const std::string& what) {
        auto r = walkTo({{x, y}}, what, true); if (r == W_FAILED) return false;
        return settle();
    }
    // walk away from a conversation: pick its "end" option (or the last one) until it closes
    void closeDialogue() {
        Game& G = g();
        for (int guard = 0; guard < 20 && G.mode == GMode::DIALOGUE; guard++) {
            auto opts = dialogueOptions(); int pick = 0;
            for (size_t k = 0; k < opts.size(); k++) if (G.dialogues[G.dlgId].nodes[G.dlgNode].options[opts[k]].target == "end") pick = (int)k;
            if (!opts.empty() && pick == 0 && G.dialogues[G.dlgId].nodes[G.dlgNode].options[opts[0]].target != "end") pick = (int)opts.size() - 1;
            G.dlgTextShown = 1 << 20; G.dlgOptArmed = true; G.dlgSel = pick; tap(K_ENTER, 1); settle();
        }
    }
    bool talk(const std::string& id) {
        Game& G = g(); NPC* n = nullptr;
        closeDialogue();
        for (auto& c : G.map->npcs) if (c.id == id) n = &c;
        if (!n) { fail("no NPC '" + id + "' on " + G.mapId); return false; }
        if (n->hidden) { fail("NPC '" + id + "' is hidden"); return false; }
        for (int tries = 0; tries < 5; tries++) {   // wandering NPCs move: re-aim a few times
            n->wanderTimer = 1 << 30;                 // (and a player would wait for them to stand still)
            auto r = walkTo({{n->x, n->y}}, "NPC " + id, true); if (r == W_FAILED) return false;
            if (G.mode == GMode::DIALOGUE) { G.dlgTextShown = 1 << 20; G.dlgOptArmed = true; return true; }   // the conversation is open: the script picks
            if (!settle(true)) return false;
            if (G.mode == GMode::DIALOGUE || G.mode == GMode::SHOP || G.mode == GMode::WORLDMAP || G.mode == GMode::BOUNTY || G.mode == GMode::CRAFT) return true;
        }
        fail("could not start talking to " + id); return false;
    }
    bool fight(const std::string& flag) {
        Game& G = g(); EnemyGroup* e = nullptr; int idx = -1;
        for (size_t i = 0; i < G.map->enemies.size(); i++) if (G.map->enemies[i].flag == flag) { e = &G.map->enemies[i]; idx = (int)i; }
        if (!e) { fail("no enemy group '" + flag + "' on " + G.mapId); return false; }
        if (G.hasFlag(flag)) { printf("  bot: %s already beaten\n", flag.c_str()); return true; }
        for (int tries = 0; tries < 30 && !G.hasFlag(flag); tries++) {
            EnemyGroup& eg = G.map->enemies[idx];
            if (eg.defeated) break;
            auto r = walkTo({{eg.x, eg.y}}, "enemy " + flag, true); if (r == W_FAILED) return false;
            if (!settle()) return false;
            if (G.mode == GMode::DIALOGUE && !G.hasFlag(flag)) { std::string have; for (int o : dialogueOptions()) have += " [" + optionText(o) + "]"; fail("a conversation opened on the way to " + flag + " (" + G.dlgId + "):" + have + " - add a choose line"); return false; }
        }
        if (!G.hasFlag(flag)) { fail("fight " + flag + " did not set the flag"); return false; }
        return true;
    }

    // map-level route through exits (BFS over maps), then walk exit by exit
    bool exitUsable(const Exit& e) { Game& G = g(); if (!e.requireFlag.empty() && !G.evalCond(e.requireFlag)) return false; if (!e.requireItem.empty() && !G.party.hasItem(e.requireItem)) return false; return true; }
    std::vector<std::string> route(const std::string& from, const std::string& to) {
        Game& G = g(); std::map<std::string, std::string> prev; std::deque<std::string> q; prev[from] = ""; q.push_back(from);
        std::string keep = G.mapId; Map* keepMap = G.map;
        while (!q.empty()) {
            std::string c = q.front(); q.pop_front(); if (c == to) break;
            Map* m = G.loadMap(c);
            for (auto& e : m->exits) if (!e.to.empty() && !prev.count(e.to) && exitUsable(e)) { prev[e.to] = c; q.push_back(e.to); }
        }
        G.mapId = keep; G.map = keepMap;
        std::vector<std::string> r; if (!prev.count(to)) return r;
        for (std::string c = to; c != from; c = prev[c]) r.push_back(c);
        std::reverse(r.begin(), r.end()); return r;
    }
    bool gotoMap(const std::string& target) {
        Game& G = g();
        for (int hop = 0; hop < 12 && G.mapId != target; hop++) {
            if (!settle()) return false;
            auto r = route(G.mapId, target);
            if (r.empty()) { fail("no route from " + G.mapId + " to " + target); return false; }
            std::set<std::pair<int, int>> goals;
            for (auto& e : G.map->exits) if (e.to == r[0] && exitUsable(e)) for (int yy = e.y; yy < e.y + e.h; yy++) for (int xx = e.x; xx < e.x + e.w; xx++) goals.insert({xx, yy});
            std::string before = G.mapId;
            auto res = walkTo(goals, "exit to " + r[0], false);
            if (res == W_FAILED) return false;
            if (!settle()) return false;
            if (G.mapId == before) { // standing on the exit: step off and on again
                G.moveRepeat = 0; tap(K_ENTER, 4); if (!settle()) return false;
                if (G.mapId == before) { fail("exit to " + r[0] + " did not work"); return false; }
            }
            if (verbose) printf("  bot: -> %s (%d,%d)\n", G.mapId.c_str(), G.px, G.py);
            if (G.map && G.map->kind == "town") prep();
        }
        if (G.mapId != target) { fail("never reached " + target); return false; }
        return true;
    }

    void checkpoint(const std::string& name) {
        Game& G = g(); Check c; c.name = name; c.frames = frames; c.playSec = G.playtimeSec; c.steps = G.stepCounter; c.day = G.day; c.clock = G.clockMin;
        c.level = G.party.members.empty() ? 0 : G.party.members[0].level; c.members = (int)G.party.members.size(); c.gold = G.party.gold;
        c.kills = G.stats.kills; c.fights = G.stats.fightsWon; c.fled = G.stats.fightsFled; c.ko = G.stats.knockouts; c.xp = 0; c.map = G.mapId;
        for (auto& m : G.party.members) c.xp += m.xp;
        checks.push_back(c);
        printf("CHECKPOINT %-22s t=%3ldm%02lds steps=%5d day=%d lvl=%2d party=%d gold=%5d kills=%4d fights=%3d ko=%d map=%s\n", name.c_str(), c.frames / 3600, (c.frames / 60) % 60, c.steps, c.day, c.level, c.members, c.gold, c.kills, c.fights, c.ko, c.map.c_str());
    }
    void report(const std::string& path) {
        FILE* f = fopen(path.c_str(), "w"); if (!f) return;
        fprintf(f, "checkpoint,minutes,steps,day,level,party,gold,kills,fights,fled,knockouts,map\n");
        for (auto& c : checks) fprintf(f, "%s,%.1f,%d,%d,%d,%d,%d,%d,%d,%d,%d,%s\n", c.name.c_str(), c.frames / 3600.0, c.steps, c.day, c.level, c.members, c.gold, c.kills, c.fights, c.fled, c.ko, c.map.c_str());
        fclose(f);
    }

    // returns true if the command was a bot command
    bool command(const std::string& cmd, const std::vector<std::string>& p, const std::string& line) {
        Game& G = g();
        auto arg = [&](size_t i) { return i < p.size() ? p[i] : std::string(); };
        if (cmd == "newgame") {
            app.state = AppState::GAME; app.desktop.windows.clear();
            G.newGame(arg(1).empty() ? "Bot" : arg(1), arg(2).empty() ? "warrior" : arg(2), p.size() > 3 ? atoi(p[3].c_str()) : 1);
            G.rng.reseed(p.size() > 4 ? (uint64_t)atoll(p[4].c_str()) : 1);   // newgame NAME CLASS DIFF SEED: the same run every time
            frames = 0; checks.clear(); settle(); return true;
        }
        if (cmd == "autoending") { autoEnding = atoi(arg(1).c_str()) != 0; return true; }
        if (cmd == "careful") { careful = atoi(arg(1).c_str()) != 0; return true; }
        if (cmd == "prep") { prep(); return true; }
        if (cmd == "strict") { strict = atoi(arg(1).c_str()) != 0; return true; }
        if (cmd == "verbose") { verbose = atoi(arg(1).c_str()) != 0; return true; }
        if (cmd == "settle") { settle(); return true; }
        if (cmd == "goto") { closeDialogue(); gotoMap(arg(1)); return true; }
        if (cmd == "walk") { walk(atoi(arg(1).c_str()), atoi(arg(2).c_str())); return true; }
        if (cmd == "bump") { bumpAt(atoi(arg(1).c_str()), atoi(arg(2).c_str()), "tile " + arg(1) + "," + arg(2)); return true; }
        if (cmd == "talk") { talk(arg(1)); return true; }
        if (cmd == "fight") { fight(arg(1)); return true; }
        if (cmd == "choose") { choose(trim(line.substr(6))); return true; }
        if (cmd == "buy") {   // buy ITEM [N] in the open shop at its real price
            int n = p.size() > 2 ? atoi(p[2].c_str()) : 1; const ItemDef* d = G.gd->item(arg(1));
            if (G.mode != GMode::SHOP || !d) { fail("buy " + arg(1) + ": no shop open or unknown item"); return true; }
            bool stocked = false; for (auto& it : G.shopItems(G.shopId)) if (it.first == arg(1)) stocked = true;
            if (!stocked) { fail(G.shopId + " does not sell " + arg(1)); return true; }
            int price = G.itemBuyPrice(*d) * n; if (G.party.gold < price) { fail(fmt("can't afford %d %s (%d gold, have %d)", n, arg(1).c_str(), price, G.party.gold)); return true; }
            G.party.gold -= price; G.stats.goldSpent += price; G.party.addItem(arg(1), n, *G.gd); printf("  bot: bought %d %s for %d\n", n, arg(1).c_str(), price);
            return true;
        }
        if (cmd == "waituntil") {   // waituntil HOUR: pass time (as a player would by wandering about) until that hour
            int h = atoi(arg(1).c_str()); int guard = 0; while (G.clockMin / 60 != h && guard++ < 24 * 4) G.advanceClock(15); return true;
        }
        if (cmd == "checkpoint") { checkpoint(arg(1)); return true; }
        if (cmd == "report") { report(app.saveDir + "/" + arg(1)); return true; }
        return false;
    }
};
