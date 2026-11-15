// World state: things the party changes on the map and that stay changed.
//  * obstacles - rubble, vines, ice, boulders, sealed doors - cleared with the right tool, spell or strength
//  * levers    - toggle a flag; doors (openif=) and map changes react to it
//  * changes   - a part of the map that looks different while a condition holds (bridge appears, wall collapses)
//  * gather    - ore veins, herb patches: harvest, then they grow back after a few days
//  * NPCs that step aside once something happened (moveif=), dialogue npcmove= / mapset= for scripted edits
#include "game.h"
#include "../core/audio.h"
#include "tiles.h"
#include <algorithm>

using namespace UI;

void Game::applyWorldState(Map& m) {
    // obstacles cleared earlier (flag) stay cleared
    for (auto& o : m.obstacles) if (!o.cleared && !o.flag.empty() && hasFlag(o.flag)) clearObstacle(m, o, false);
    // needs=flag:COND - the obstacle gives way by itself once the condition holds (a lever, a quest step, a deal struck)
    for (auto& o : m.obstacles) if (!o.cleared) for (auto& n : split(o.needs, '|')) { std::string t = trim(n); if (startsWith(t, "flag:") && evalCond(t.substr(5))) { clearObstacle(m, o, false); break; } }
    // conditional map changes; revert when the condition goes away again (a lever thrown back)
    for (auto& c : m.changes) {
        bool want = c.cond.empty() || evalCond(c.cond);
        if (want == c.applied) continue;
        int i = 0;
        for (int yy = c.y; yy < c.y + c.h; yy++) for (int xx = c.x; xx < c.x + c.w; xx++, i++) {
            if (!m.inBounds(xx, yy)) continue;
            if (want) m.set(xx, yy, c.kind, rng);
            else if (i < (int)c.orig.size() && c.orig[i] < allTileKinds().size()) m.set(xx, yy, allTileKinds()[c.orig[i]].name, rng);
        }
        c.applied = want;
    }
    // doors that open on a condition (a lever, a quest step); 1.1.7: they set their flag= like a door the party opens
    for (auto& d : m.doors) if (!d.open && !d.openIf.empty() && evalCond(d.openIf)) { d.open = true; if (!d.flag.empty()) setFlag(d.flag);   /* 1.1.7: its flag too, as when the party opens it */ m.at(d.x, d.y).walkable = true; m.at(d.x, d.y).blocksSight = false; m.at(d.x, d.y).prop = d.openTile; }
    // NPCs who moved
    for (auto& n : m.npcs) {
        auto it = extra.find("npcpos_" + m.id + "_" + n.id);
        if (it != extra.end()) { auto p = split(it->second, ','); if (p.size() == 2) { n.x = n.homeX = atoi(p[0].c_str()); n.y = n.homeY = atoi(p[1].c_str()); } }
        else if (!n.moveIf.empty() && n.moveX >= 0 && evalCond(n.moveIf)) { n.x = n.homeX = n.moveX; n.y = n.homeY = n.moveY; }
    }
    // scripted tile edits (dialogue mapset=x,y,kind)
    auto ms = extra.find("mapset_" + m.id);
    if (ms != extra.end()) for (auto& e : split(ms->second, ';')) { auto p = split(e, ','); if (p.size() == 3 && m.inBounds(atoi(p[0].c_str()), atoi(p[1].c_str()))) m.set(atoi(p[0].c_str()), atoi(p[1].c_str()), trim(p[2]), rng); }
    // levers show their state
    // 1.1.1: sprite = the lever's picture, onsprite = its picture while its flag is set (objects.txt [type] lever)
    for (auto& l : m.levers) if (m.inBounds(l.x, l.y)) { Tile off = l.sprite.valid() ? l.sprite : TL::LEVER; m.at(l.x, l.y).prop = l.onSprite.valid() && !l.flag.empty() && hasFlag(l.flag) ? l.onSprite : off; }
    if (&m == map) { revealAround(); }
}

void Game::clearObstacle(Map& m, Obstacle& o, bool announce) {
    if (o.cleared) return;
    o.cleared = true;
    for (int yy = o.y; yy < o.y + o.h; yy++) for (int xx = o.x; xx < o.x + o.w; xx++) if (m.inBounds(xx, yy)) m.set(xx, yy, o.clearKind.empty() ? "floor_cave" : o.clearKind, rng);
    if (!o.flag.empty()) setFlag(o.flag);
    if (announce) {
        log("The way is clear.");
        if (!o.onClear.empty()) runScript(o.onClear);
        if (&m == map) revealAround();
    }
}

// ---- what can clear an obstacle ----
// needs = any of (separated by |): rubble (= dig|blast), dig, blast, burn, melt, force:N, spell:UTILITY, item:ID, flag:COND
static bool blastSpell(const SpellDef& s) { return s.utility == "blast" || ((s.dmgType == DT_FIRE || s.dmgType == DT_PHYS || s.dmgType == DT_SHOCK) && s.area >= 1 && s.dmgMax >= 10 && s.friendlyFire); }
static bool fireSpell(const SpellDef& s) { return s.dmgType == DT_FIRE && s.dmgMax > 0; }
void Game::obstacleMenu(int idx) {
    if (idx < 0 || idx >= (int)map->obstacles.size()) return;
    Obstacle& o = map->obstacles[idx]; curObstacle = idx;
    std::vector<std::string> needs; for (auto& n : split(o.needs, '|')) { std::string t = trim(n); if (t == "rubble") { needs.push_back("dig"); needs.push_back("blast"); } else if (!t.empty()) needs.push_back(t); }
    Dialogue d; d.id = "_obstacle";
    DialogueNode start; start.id = "start"; start.speaker = o.name;
    std::string hint;
    for (auto& n : needs) {
        if (n == "dig") hint += " A shovel would dig through it.";
        else if (n == "blast") hint += " A blasting charge - or a big enough blast spell - would bring it down.";
        else if (n == "burn") hint += " It would burn.";
        else if (n == "melt") hint += " Fire would melt it.";
        else if (startsWith(n, "force:")) hint += " It might move with enough muscle.";
        else if (startsWith(n, "spell:")) hint += n.substr(6) == "unlock" ? " A knock or hack spell would open it." : " Magic of the right sort would open it.";
        else if (startsWith(n, "item:")) { const ItemDef* it = gd->item(n.substr(5)); hint += " It needs " + std::string(it ? it->name : n.substr(5)) + "."; }
        else if (startsWith(n, "flag:")) hint += " Nothing you can do here will shift it - something else must happen first.";
    }
    start.text = (o.msg.empty() ? "Something blocks the way." : o.msg) + hint;
    auto opt = [&](const std::string& text, const std::string& how) { DialogueOption op; op.text = text; op.target = "do_" + how; start.options.push_back(op); DialogueNode dn; dn.id = "do_" + how; dn.actions.push_back({"obstacle", how}); d.nodes[dn.id] = dn; };
    for (auto& n : needs) {
        if (n == "dig" && party.hasItem("shovel")) opt("Dig through it with the shovel. (It will break.)", "dig");
        if (n == "blast") {
            if (party.hasItem("blasting_charge")) opt("Set a blasting charge, light the fuse - and run.", "charge");
            for (size_t mi = 0; mi < party.members.size(); mi++) { auto& m = party.members[mi]; if (!m.alive) continue; for (auto& sid : m.spells) if (auto* sp = gd->spell(sid)) if (blastSpell(*sp)) { opt(fmt("%s casts %s at it. (%d MP - you're standing in the blast!)", m.name.c_str(), sp->name.c_str(), sp->cost), fmt("spell:%d:%s", (int)mi, sid.c_str())); break; } }
        }
        if (n == "burn" || n == "melt") {
            if (party.hasItem("torch_item")) opt("Set it alight with a torch.", "torch");
            if (party.hasItem("oil_fire")) opt("Douse it in fire oil and light it.", "oil");
            for (size_t mi = 0; mi < party.members.size(); mi++) { auto& m = party.members[mi]; if (!m.alive) continue; for (auto& sid : m.spells) if (auto* sp = gd->spell(sid)) if (fireSpell(*sp)) { opt(fmt("%s burns it with %s. (%d MP)", m.name.c_str(), sp->name.c_str(), sp->cost), fmt("burnspell:%d:%s", (int)mi, sid.c_str())); break; } }
        }
        if (startsWith(n, "force:")) { int need = atoi(n.substr(6).c_str()); int best = 0; std::string who; for (auto& m : party.members) if (m.alive && m.stat(ST_STR, *gd) > best) { best = m.stat(ST_STR, *gd); who = m.name; } opt(fmt("%s puts a shoulder to it. (STR %d, needs %d)", who.c_str(), best, need), "force:" + std::to_string(need)); }
        if (startsWith(n, "spell:")) { std::string ut = n.substr(6); for (size_t mi = 0; mi < party.members.size(); mi++) { auto& m = party.members[mi]; for (auto& sid : m.spells) if (auto* sp = gd->spell(sid)) if (sp->utility == ut) { opt(fmt("%s casts %s. (%d MP)", m.name.c_str(), sp->name.c_str(), sp->cost), fmt("utility:%d:%s", (int)mi, sid.c_str())); break; } } }
        if (startsWith(n, "item:") && party.hasItem(n.substr(5))) { const ItemDef* it = gd->item(n.substr(5)); opt("Use the " + std::string(it ? it->name : n.substr(5)) + ".", n); }
    }
    DialogueOption leave; leave.text = "Leave it."; leave.target = "end"; start.options.push_back(leave);
    d.nodes["start"] = start;
    dialogues["_obstacle"] = d;
    startDialogue("_obstacle", nullptr);
}
void Game::obstacleAction(const std::string& how) {
    if (mode == GMode::DIALOGUE) popMode();
    if (curObstacle < 0 || curObstacle >= (int)map->obstacles.size()) return;
    Obstacle& o = map->obstacles[curObstacle];
    int cx = o.x + o.w / 2, cy = o.y + o.h / 2;
    auto parts = split(how, ':');
    if (how == "dig") {
        party.removeItem("shovel", 1); advanceClock(30); gAudio.play("step");
        clearObstacle(*map, o, true); log("You dig until your arms burn. The shovel's handle snaps as the last stones roll free."); return;
    }
    if (how == "charge") {
        party.removeItem("blasting_charge", 1);
        fuseT = 3.0f; fuseX = cx; fuseY = cy; fuseObstacle = curObstacle; fuseMap = mapId;
        log("The fuse is lit! Get clear - at least three steps away!"); notify("FUSE LIT - RUN!"); gAudio.play("fuse_lit");
        return;
    }
    if (parts[0] == "spell" || parts[0] == "burnspell" || parts[0] == "utility") {
        int mi = atoi(parts[1].c_str()); const SpellDef* sp = gd->spell(parts.size() > 2 ? parts[2] : ""); if (!sp || mi < 0 || mi >= (int)party.members.size()) return;
        Character& c = party.members[mi];
        if (c.mp < sp->cost) { log(c.name + " doesn't have the MP."); gAudio.play("ui_error"); return; }
        c.mp -= sp->cost; gAudio.play(spellSound(*sp));
        if (parts[0] == "spell") {
            // the blast hits the obstacle - and whoever stands within its radius, which right now includes the caster
            int power = c.stat(sp->scaleStat, *gd);
            blastAt(cx, cy, std::max(1, (sp->dmgMin + (power - 8) / 2) / 2), std::max(2, (sp->dmgMax + (power - 8) / 2) / 2), sp->name, std::max(1, sp->area));
        }
        clearObstacle(*map, o, true);
        if (parts[0] == "burnspell") log("Flames run up it and it crumbles to ash.");
        return;
    }
    if (how == "torch" || how == "oil") {
        party.removeItem(how == "torch" ? "torch_item" : "oil_fire", 1); gAudio.play("burn");
        clearObstacle(*map, o, true); log("It catches, crackles, and burns away."); return;
    }
    if (parts[0] == "force") {
        int need = atoi(parts[1].c_str()); int best = 0; for (auto& m : party.members) if (m.alive) best = std::max(best, m.stat(ST_STR, *gd));
        if (best < need) { log("It won't budge. Someone stronger might shift it."); gAudio.play("ui_error"); advanceClock(5); return; }
        gAudio.play("mine_boom"); shake = 0.2f; clearObstacle(*map, o, true); log("With a grinding roar it gives way."); return;
    }
    if (parts[0] == "item") { std::string id = parts[1]; party.removeItem(id, 1); clearObstacle(*map, o, true); return; }
}
// an explosion in explore mode: party members within the radius get hurt (never below 1 HP outside a fight)
void Game::blastAt(int cx, int cy, int lo, int hi, const std::string& what, int radius) {
    gAudio.play("mine_boom"); shake = 0.35f;
    std::vector<std::pair<int, int>> pos; pos.push_back({px, py}); std::vector<int> fo = followOrder(); int ld = activeLeader();
    for (size_t k = 0; k < fo.size(); k++) pos.push_back(k < trail.size() ? trail[k] : std::make_pair(px, py));
    std::vector<int> who; who.push_back(ld); for (int f : fo) who.push_back(f);
    int hurt = 0;
    for (size_t i = 0; i < who.size() && i < pos.size(); i++) {
        Character& m = party.members[who[i]]; if (!m.alive) continue;
        int d = chebyshev(pos[i].first, pos[i].second, cx, cy); if (d > radius) continue;
        int dmg = rng.range(lo, hi) * (radius + 1 - d) / (radius + 1); dmg = std::max(1, dmg);
        if (godMode) dmg = 0;
        m.hp = std::max(1, m.hp - dmg); hurt++;
        addFloat(pos[i].first, pos[i].second, "-" + std::to_string(dmg), C::Red);
    }
    if (hurt) log(fmt("The %s's blast catches %d of you!", what.c_str(), hurt)); else log("BOOM. Rocks rain down where you stood a moment ago.");
}
void Game::updateFuse(float dt) {
    if (fuseT <= 0) return;
    fuseT -= dt;
    if (fuseT > 0) return;
    fuseT = 0;
    if (fuseMap != mapId || !map) { fuseObstacle = -1; return; }
    blastAt(fuseX, fuseY, 12, 22, "charge", 2);
    if (fuseObstacle >= 0 && fuseObstacle < (int)map->obstacles.size()) clearObstacle(*map, map->obstacles[fuseObstacle], true);
    fuseObstacle = -1;
}

void Game::pullLever(Lever& l) {
    bool on = hasFlag(l.flag);
    if (on && !l.toggle) { log("It's stuck in place."); return; }
    if (on) flags.erase(l.flag); else setFlag(l.flag);
    gAudio.play(l.sound.empty() ? "lever" : l.sound); shake = 0.05f;
    if (!l.msg.empty()) log(l.msg); else log(on ? "You push the lever back." : "You pull the lever. Somewhere, something grinds.");
    applyWorldState(*map);
    if (!l.onUse.empty()) runScript(l.onUse);
}

void Game::gatherFrom(GatherNode& g) {
    const ItemDef* d = gd->item(g.item); if (!d) return;
    std::string key = fmt("gather_%s_%d_%d", mapId.c_str(), g.x, g.y);
    auto it = extra.find(key);
    if (it != extra.end() && day - atoi(it->second.c_str()) < g.respawn) { log((g.name.empty() ? d->name : g.name) + fmt(": picked clean. Give it %d more day%s.", g.respawn - (day - atoi(it->second.c_str())), g.respawn - (day - atoi(it->second.c_str())) == 1 ? "" : "s")); return; }
    if (!g.tool.empty()) {
        bool have = party.hasItem(g.tool); for (auto& m : party.members) for (int s = 0; s < SL_COUNT; s++) if (m.equip[s] == g.tool || startsWith(m.equip[s], g.tool + "@")) have = true;
        if (!have) { const ItemDef* td = gd->item(g.tool); log("You need a " + std::string(td ? td->name : g.tool) + " to work this."); gAudio.play("ui_error"); return; }
    }
    int n = rng.range(g.lo, g.hi);
    giveItem(g.item, n); extra[key] = std::to_string(day); advanceClock(10);
    gAudio.play(!g.sound.empty() && gAudio.has(g.sound) ? g.sound : g.tool.empty() ? "gather" : "gather_tool");   // 1.3.3: sound= of the node / its type; 1.3.4 triggers (fall back to pickup / hit)
}
