// Healers (temples, shrines, the village healer). Inns are where you rest: full HP and MP overnight for a few coins.
// Healers do what an inn can't, for a fee and without the night going by:
//   tend wounds (HP only, right now), cleanse lingering poison/burns/curses, raise a fallen companion,
//   and a blessing that shields and mends the party at the start of their next few fights.
// Dialogue use: an action node with  healer=1  (or healer=<greeting text>).
#include "game.h"
#include "../core/audio.h"
#include <algorithm>

using namespace UI;

static bool lingering(const StatusEffect& e) { return e.id == "poison" || e.id == "burn" || e.id == "weaken" || e.id == "shock" || e.id == "curse" || e.id == "freeze" || e.id == "stun"; }

int Game::healerCost(const std::string& svc) const {
    int lvl = party.members.empty() ? 1 : party.members[0].level;
    if (svc == "wounds") { int missing = 0; for (auto& m : party.members) if (m.alive) missing += std::max(0, m.maxHP(*gd) - m.hp); return missing <= 0 ? 0 : std::max(3, (missing + 4) / 5); }
    if (svc == "cleanse") return 6 + lvl;
    if (svc == "raise") { int c = 0; for (auto& m : party.members) if (!m.alive) c += 20 + 6 * m.level; return c; }
    if (svc == "bless") return 15 + 5 * lvl;
    return 0;
}

void Game::healerMenu(const std::string& greeting) {
    healerGreeting = greeting;
    std::string who = dlgSpeakerOverride.empty() ? (dlgNpc ? dlgNpc->name : std::string("Healer")) : dlgSpeakerOverride;
    Dialogue d; d.id = "_healer";
    DialogueNode start; start.id = "start"; start.speaker = who;
    start.text = greeting.empty() || greeting == "1" ? "The sick and the hurt come first. What do you need? (For a night's rest, find an inn.)" : greeting;
    auto opt = [&](const std::string& text, const std::string& svc) {
        DialogueOption op; op.text = text; op.target = "do_" + svc; start.options.push_back(op);
        DialogueNode dn; dn.id = "do_" + svc; dn.actions.push_back({"healsvc", svc}); d.nodes[dn.id] = dn;
    };
    int missing = 0; for (auto& m : party.members) if (m.alive) missing += std::max(0, m.maxHP(*gd) - m.hp);
    bool ail = false; for (auto& m : party.members) for (auto& e : m.effects) if (lingering(e)) ail = true;
    bool down = false; for (auto& m : party.members) if (!m.alive) down = true;
    if (missing > 0) opt(fmt("Tend our wounds. (%d HP, %d gold)", missing, healerCost("wounds")), "wounds");
    if (ail) opt(fmt("Cleanse poison, burns and curses. (%d gold)", healerCost("cleanse")), "cleanse");
    if (down) opt(fmt("Raise our fallen. (%d gold)", healerCost("raise")), "raise");
    int bl = atoi(extra.count("blessing") ? extra.at("blessing").c_str() : "0");
    if (bl > 0) opt(fmt("Renew the blessing. (%d fights left - %d gold)", bl, healerCost("bless")), "bless");
    else opt(fmt("Bless us for the road. (next 3 fights - %d gold)", healerCost("bless")), "bless");
    DialogueOption leave; leave.text = "That's all."; leave.target = "end"; start.options.push_back(leave);
    d.nodes["start"] = start;
    NPC* npc = dlgNpc;
    dialogues["_healer"] = d;
    if (mode == GMode::DIALOGUE) popMode();
    startDialogue("_healer", npc);
    if (!who.empty()) dlgSpeakerOverride = who;
}

void Game::healerService(const std::string& svc) {
    int cost = healerCost(svc);
    if (party.gold < cost) { log(fmt("That costs %d gold.", cost)); gAudio.play("ui_error"); healerReopen = true; return; }
    if (svc == "wounds") { for (auto& m : party.members) if (m.alive) m.hp = m.maxHP(*gd); log("Your wounds are dressed and stitched."); }
    else if (svc == "cleanse") { for (auto& m : party.members) m.effects.erase(std::remove_if(m.effects.begin(), m.effects.end(), lingering), m.effects.end()); log("The poison and the fever are drawn out."); }
    else if (svc == "raise") { for (auto& m : party.members) if (!m.alive) { m.alive = true; m.hp = std::max(1, m.maxHP(*gd) / 2); log(m.name + " opens their eyes."); } }
    else if (svc == "bless") { extra["blessing"] = "3"; log("A blessing settles over the party: the next three fights start shielded and mending."); }
    else return;
    party.gold -= cost; stats.goldSpent += cost;
    gAudio.play("rest");
    healerReopen = true;   // back to the healer's list (rebuilt next frame, so prices are current)
}

// at the start of each fight while a blessing lasts
void Game::applyBlessing() {
    auto it = extra.find("blessing"); if (it == extra.end()) return;
    int left = atoi(it->second.c_str()); if (left <= 0) { extra.erase(it); return; }
    for (auto& u : cb.units) if (u.partyIndex >= 0 && u.ch.alive) { u.ch.addEffect("shield", 3); u.ch.addEffect("regen", 3, 2 + u.ch.level / 3); }
    left--; if (left > 0) it->second = std::to_string(left); else extra.erase(it);
    log(left > 0 ? fmt("The blessing shields the party. (%d more fights)", left) : "The blessing shields the party one last time.");
}
