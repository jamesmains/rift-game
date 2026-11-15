// Crafting: stations with their own skill, recipe discovery, success and quality rolls, upgrades (+1..+3),
// enchanting (affixes from rift dust) and salvage; plus randomly affixed loot ("Rusty Pistol of Burning").
#include "game.h"
#include "../core/audio.h"
#include "tiles.h"
#include <algorithm>

using namespace UI;

static const char* kTypeNames[] = {"weapon", "armor", "helm", "boots", "accessory", "offhand", "consumable", "key", "material", "ammo"};

// ---------------- dynamic items: affixes and +N ----------------
// ids look like  base@affix+affix#2  (affixes and/or upgrade level); dynItems[id] = "base|affix,affix|2"
bool Game::parseDynId(const std::string& id, std::string& base, std::vector<std::string>& affixIds, int& plus) const {
    affixIds.clear(); plus = 0;
    size_t at = id.find('@'); if (at == std::string::npos) { base = id; return false; }
    base = id.substr(0, at); std::string rest = id.substr(at + 1);
    size_t hash = rest.find('#'); if (hash != std::string::npos) { plus = atoi(rest.substr(hash + 1).c_str()); rest = rest.substr(0, hash); }
    for (auto& a : split(rest, '+')) if (!trim(a).empty()) affixIds.push_back(trim(a));
    return true;
}
std::string Game::buildDynItem(const std::string& baseId, const std::vector<std::string>& affixIds, int plus) {
    const ItemDef* base = gd->item(baseId); if (!base) return baseId;
    if (affixIds.empty() && plus <= 0) return baseId;
    std::string id = baseId + "@"; for (size_t i = 0; i < affixIds.size(); i++) id += (i ? "+" : "") + affixIds[i];
    if (plus > 0) id += "#" + std::to_string(plus);
    std::string csv; for (size_t i = 0; i < affixIds.size(); i++) csv += (i ? "," : "") + affixIds[i];
    dynItems[id] = baseId + "|" + csv + "|" + std::to_string(plus);
    if (gd->items.count(id)) return id;
    ItemDef d = *base; d.id = id;
    std::string name = base->name; int n = 0;
    for (auto& aid : affixIds) for (auto& a : affixes) if (a.id == aid) {
        n++; if (a.prefix) name = a.name + " " + name; else name = name + " of " + a.name;
        for (auto& m : a.mods) {
            const std::string& k = m.first; int v = m.second;
            if (k == "dmg") { d.dmgMin += v; d.dmgMax += v; } else if (k == "dmgmin") d.dmgMin += v; else if (k == "dmgmax") d.dmgMax += v;
            else if (k == "def") d.def += v; else if (k == "evasion") d.evasion += v; else if (k == "accuracy") d.accuracy += v; else if (k == "crit") d.crit += v;
            else if (k == "hp") d.bonusHP += v; else if (k == "mp") d.bonusMP += v; else if (k == "ap") d.bonusAP += v; else if (k == "move") d.bonusMove += v; else if (k == "range") d.range += v;
            else if (k == "str" || k == "dex" || k == "con" || k == "int" || k == "wis" || k == "cha") d.bonus[statFromName(k)] += v;
            else if (startsWith(k, "res_")) d.res[dmgTypeFromName(k.substr(4))] += v;
        }
        if (!a.dmgType.empty()) d.dmgType = dmgTypeFromName(a.dmgType);
        d.value = d.value * a.valuePct / 100;
    }
    if (plus > 0) {
        if (d.type == IT_WEAPON) { d.dmgMin += plus; d.dmgMax += plus * 2; d.accuracy += plus * 2; }
        else if (d.type == IT_ACCESSORY) { d.bonusHP += plus * 5; }
        else { d.def += plus; if (plus >= 3) d.bonusHP += 5; }
        d.value = d.value * (100 + 25 * plus) / 100;
        name += " +" + std::to_string(plus);
    }
    d.name = name;
    if (n > 0) d.desc = base->desc + (n > 1 ? "  [rare]" : "  [uncommon]");
    if (plus > 0) d.desc += fmt("  [reinforced +%d]", plus);
    gd->items[id] = d;
    return id;
}

int Game::lootTierRoll(int level) {
    int r = rng.range(1, 100);
    if (r <= 3 + level / 2) return 3;
    if (r <= 14 + level) return 2;
    return 1;
}

std::string Game::makeAffixed(const std::string& baseId, int tier) {
    const ItemDef* base = gd->item(baseId);
    if (!base || !base->equippable() || tier <= 1 || affixes.empty()) return baseId;
    std::string b0; std::vector<std::string> have; int plus = 0; parseDynId(baseId, b0, have, plus);
    const ItemDef* root = gd->item(b0); if (!root || root->unique) return baseId;   // unique=1 items stay exactly as made
    std::string tname = kTypeNames[root->type];
    std::vector<const AffixDef*> pre, suf;
    for (auto& a : affixes) {
        if (a.tier > tier) continue;
        if (std::find(a.types.begin(), a.types.end(), tname) == a.types.end() && std::find(a.types.begin(), a.types.end(), "any") == a.types.end()) continue;
        if (std::find(have.begin(), have.end(), a.id) != have.end()) continue;
        (a.prefix ? pre : suf).push_back(&a);
    }
    // never two prefixes or two suffixes
    bool hasPre = false, hasSuf = false;
    for (auto& h : have) for (auto& a : affixes) if (a.id == h) { if (a.prefix) hasPre = true; else hasSuf = true; }
    if (hasPre) pre.clear(); if (hasSuf) suf.clear();
    std::vector<std::string> chosen = have;
    if (have.empty() && tier >= 3 && !pre.empty() && !suf.empty()) { chosen.push_back(pre[rng.range(0, (int)pre.size() - 1)]->id); chosen.push_back(suf[rng.range(0, (int)suf.size() - 1)]->id); }
    else { std::vector<const AffixDef*> all = pre; all.insert(all.end(), suf.begin(), suf.end()); if (all.empty()) return baseId; chosen.push_back(all[rng.range(0, (int)all.size() - 1)]->id); }
    return buildDynItem(b0, chosen, plus);
}

void Game::restoreDynItems() {
    for (auto kv : dynItems) {
        if (gd->items.count(kv.first)) continue;
        auto p = split(kv.second, '|'); if (p.size() < 2) continue;
        std::vector<std::string> af; for (auto& a : split(p[1], ',')) if (!trim(a).empty()) af.push_back(trim(a));
        int plus = p.size() > 2 ? atoi(p[2].c_str()) : 0;
        if (!gd->item(p[0])) continue;
        std::string id = buildDynItem(p[0], af, plus);
        if (id != kv.first && gd->items.count(id)) { ItemDef d = gd->items[id]; d.id = kv.first; gd->items[kv.first] = d; }   // ids from older saves
    }
}

// ---------------- skills ----------------
static const int kSkillXP[6] = {0, 40, 120, 250, 450, 700};
int Game::craftXP(const std::string& station) const { auto it = extra.find("cskill_" + station); return it == extra.end() ? 0 : atoi(it->second.c_str()); }
void Game::addCraftXP(const std::string& station0, int xp) {
    std::string st = station0 == "any" ? "field" : station0; if (xp == 0) return;
    std::string station = st == "field" ? "any" : st;
    int before = craftSkill(station);
    extra["cskill_" + st] = std::to_string(std::max(0, craftXP(st) + xp));
    int now = craftSkill(station);
    if (now > before) { notify(fmt("%s skill rises to %d!", st == "field" ? "Field craft" : st == "forge" ? "Smithing" : st == "alchemy" ? "Alchemy" : "Tinkering", now)); gAudio.play("craft_skill"); }
}
int Game::craftSkill(const std::string& station) const { int xp = craftXP(station == "any" ? "field" : station), lv = 1; for (int i = 0; i < 6; i++) if (xp >= kSkillXP[i]) lv = i + 1; return lv; }
int Game::craftStat(const std::string& station) const {
    int best = 0;
    for (auto& m : party.members) {
        int v = station == "forge" ? m.stat(ST_STR, *gd) : station == "alchemy" ? std::max(m.stat(ST_INT, *gd), m.stat(ST_WIS, *gd)) : station == "workbench" ? std::max(m.stat(ST_INT, *gd), m.stat(ST_DEX, *gd)) : std::max(m.stat(ST_DEX, *gd), m.stat(ST_WIS, *gd));
        best = std::max(best, v);
    }
    return best;
}
// success chance: skill against difficulty, nudged by the best party member's stat for that bench
int Game::craftSuccess(const std::string& station, int difficulty) const {
    int sk = craftSkill(station);
    if (difficulty <= 1 || sk >= difficulty + 1) return 100;
    return clampi(70 + (sk - difficulty) * 15 + (craftStat(station) - 10) * 2, 15, 95);
}
bool Game::recipeVisible(const Recipe& r) const {
    if (!r.known && !extra.count("recipe_" + r.id)) return false;
    if (!r.reveal.empty() && !party.hasItem(r.reveal) && !extra.count("seen_" + r.reveal) && !extra.count("crafted_" + r.id)) return false;
    return true;
}
void Game::learnRecipes(const std::string& csv, bool announce) {
    for (auto& id0 : split(csv, ',')) {
        std::string id = trim(id0); if (id.empty() || extra.count("recipe_" + id)) continue;
        extra["recipe_" + id] = "1";
        if (announce) for (auto& r : recipes) if (r.id == id) { notify("Recipe learned: " + r.name); log("You learn to make " + r.name + " (" + r.station + ")."); }
    }
}
void Game::markSeenItems() { for (auto& r : recipes) if (!r.reveal.empty() && party.hasItem(r.reveal)) extra["seen_" + r.reveal] = "1"; }

void Game::openCraft(const std::string& station) { craftStation = station.empty() ? "any" : station; craftSel = 0; craftScroll = 0; craftTab = 0; craftQty = 1; markSeenItems(); pushMode(GMode::CRAFT); }

static std::vector<const Recipe*> recipesFor(const Game& g, const std::string& station) {
    std::vector<const Recipe*> out;
    for (auto& r : g.recipes) if ((r.station == "any" || r.station == station || station == "all") && g.recipeVisible(r)) out.push_back(&r);
    std::stable_sort(out.begin(), out.end(), [&](const Recipe* a, const Recipe* b) { bool ca = g.canCraft(*a), cb = g.canCraft(*b); if (ca != cb) return ca; return a->difficulty < b->difficulty; });
    return out;
}
bool Game::canCraft(const Recipe& r) const {
    if (party.gold < r.gold) return false;
    int lvl = party.members.empty() ? 1 : party.members[0].level; if (lvl < r.level) return false;
    if (craftSkill(r.station) < r.skill) return false;   // 1.1.2: harder recipes need the station's skill, not just luck
    for (auto& m : r.mats) if (party.countItem(m.first) < m.second) return false;
    return gd->item(r.result) != nullptr;
}
bool Game::craft(const Recipe& r) {
    if (!canCraft(r)) { log("You lack the materials."); gAudio.play("ui_error"); return false; }
    std::string st = r.station == "any" ? "field" : r.station;
    int chance = craftSuccess(r.station, r.difficulty);
    party.gold -= r.gold; stats.goldSpent += r.gold;
    std::string xpKey = "cskill_" + st;
    if (!rng.chance(chance)) {
        // a botched job: half of each material (at least one of the first) is ruined, the rest is saved
        std::string lost;
        for (size_t i = 0; i < r.mats.size(); i++) { int n = r.mats[i].second / 2; if (i == 0) n = std::max(1, n); if (n <= 0) continue; party.removeItem(r.mats[i].first, n); if (auto* md = gd->item(r.mats[i].first)) lost += (lost.empty() ? "" : ", ") + fmt("%d %s", n, md->name.c_str()); }
        extra[xpKey] = std::to_string(craftXP(st) + r.difficulty * 4);
        log("The work goes wrong. Lost: " + lost + "."); gAudio.play("ui_error"); shake = 0.1f;
        return false;
    }
    for (auto& m : r.mats) party.removeItem(m.first, m.second);
    std::string id = r.result;
    const ItemDef* rd = gd->item(id);
    int quality = 0;
    if (rd && rd->equippable() && !rd->unique) {   // unique=1 items always come out exactly as written
        // fine (one affix) or masterwork (two) when the crafter is better than the job needs
        int over = craftSkill(r.station) - r.difficulty, stat = craftStat(r.station);
        int fine = clampi(8 + over * 10 + (stat - 10), 3, 60), master = clampi(over * 5 + (stat - 12), 0, 25);
        if (rng.chance(master)) { id = makeAffixed(id, 3); quality = 2; } else if (rng.chance(fine)) { id = makeAffixed(id, 2); quality = 1; }
    }
    int before = craftSkill(r.station);
    extra[xpKey] = std::to_string(craftXP(st) + r.difficulty * 12);
    extra["crafted_" + r.id] = "1";
    party.addItem(id, r.count, *gd); stats.crafted += r.count;
    giveXP(r.difficulty * 4 * std::max(1, (int)party.members.size()) / 2);
    log("Crafted " + itemLabel(*gd->item(id), r.count) + (quality == 2 ? " - a masterwork!" : quality == 1 ? " - fine work." : ".")); gAudio.play("craft");
    if (craftSkill(r.station) > before) { notify(fmt("%s skill rises to %d!", r.station == "any" ? "Field craft" : r.station == "forge" ? "Smithing" : r.station == "alchemy" ? "Alchemy" : "Tinkering", craftSkill(r.station))); gAudio.play("craft_skill"); }
    bountyEvent("craft", r.result, r.count);
    return true;
}

// ---------------- upgrade / enchant / salvage ----------------
static bool forgeKind(const ItemDef& d) { return d.equippable() && !d.tech && d.type != IT_ACCESSORY; }
std::vector<Game::CraftTarget> Game::craftTargets(int tab) const {
    std::vector<CraftTarget> out;
    auto ok = [&](const std::string& id) {
        const ItemDef* d = gd->item(id); if (!d || !d->equippable()) return false;
        if (tab == 1) { if (craftStation == "forge" && !forgeKind(*d)) return false; if (craftStation == "workbench" && !(d->tech && d->type != IT_ACCESSORY)) return false; if (craftStation != "forge" && craftStation != "workbench" && craftStation != "all") return false; }
        if (tab == 2) { std::string b; std::vector<std::string> af; int p; parseDynId(id, b, af, p); if (af.size() >= 2) return false; }
        return true;
    };
    for (size_t m = 0; m < party.members.size(); m++) for (int s = 0; s < SL_COUNT; s++) if (!party.members[m].equip[s].empty() && ok(party.members[m].equip[s])) { CraftTarget t; t.member = (int)m; t.slot = s; t.id = party.members[m].equip[s]; out.push_back(t); }
    for (size_t i = 0; i < party.inventory.size(); i++) if (ok(party.inventory[i].id)) { CraftTarget t; t.inv = (int)i; t.id = party.inventory[i].id; out.push_back(t); }
    return out;
}
std::vector<std::pair<std::string, int>> Game::upgradeCost(const ItemDef& d, int plus, int& gold) const {
    static const int g[3] = {30, 80, 160};
    gold = g[clampi(plus, 0, 2)];
    // 1.1.2: +3 takes the top-tier material of each craft (mithril for smiths, rift crystal for tinkers)
    if (d.tech) { if (plus == 0) return {{"circuit", 1}, {"scrap", 2}}; if (plus == 1) return {{"circuit", 2}, {"iron_ingot", 1}}; return {{"power_core", 1}, {"circuit", 2}, {"rift_crystal", 1}}; }
    if (plus == 0) return {{"iron_ingot", 1}};
    if (plus == 1) return {{"iron_ingot", 2}, {"steel_ingot", 1}};
    return {{"steel_ingot", 1}, {"mithril_ingot", 1}};
}
std::vector<std::pair<std::string, int>> Game::enchantCost(int affixCount, int& gold) const {
    gold = affixCount == 0 ? 60 : 150;
    if (affixCount == 0) return {{"rift_dust", 1}, {"arcane_dust", 2}};
    return {{"rift_dust", 2}, {"arcane_dust", 3}, {"rift_crystal", 1}};   // a second enchantment needs a rift crystal (1.1.2)
}
std::vector<std::pair<std::string, int>> Game::salvageYield(const ItemDef& d) const {
    std::vector<std::pair<std::string, int>> out;
    std::string b; std::vector<std::string> af; int plus = 0; parseDynId(d.id, b, af, plus);
    const ItemDef* base = gd->item(b); const ItemDef& src = base ? *base : d;
    if (!src.salvage.empty()) { for (auto& p : split(src.salvage, ',')) { auto kv = split(trim(p), ':'); if (!trim(kv[0]).empty()) out.push_back({trim(kv[0]), kv.size() > 1 ? atoi(kv[1].c_str()) : 1}); } }
    else {
        std::string n = toLower(src.id + " " + src.name);
        int v = src.value;
        if (src.tech) { out.push_back({"scrap", 2 + v / 120}); if (v >= 300) out.push_back({"circuit", 1 + v / 900}); if (v >= 1200) out.push_back({"power_core", 1}); }
        else if (n.find("leather") != std::string::npos || n.find("hood") != std::string::npos || n.find("cloak") != std::string::npos || n.find("boots") != std::string::npos || n.find("sandal") != std::string::npos || n.find("cap") != std::string::npos) out.push_back({"tanned_leather", 1 + v / 200});
        else if (n.find("robe") != std::string::npos || n.find("hat") != std::string::npos || n.find("staff") != std::string::npos || n.find("wand") != std::string::npos || n.find("book") != std::string::npos || n.find("symbol") != std::string::npos || src.type == IT_ACCESSORY) out.push_back({"arcane_dust", 1 + v / 250});
        else if (src.dmgType == DT_PHYS && src.ammo == "arrows") out.push_back({"tanned_leather", 1});
        else { if (v >= 120) out.push_back({"iron_ingot", std::min(4, v / 160 + 1)}); else out.push_back({"scrap", 2 + v / 40}); if (v >= 700) out.push_back({"steel_ingot", 1}); }
    }
    if (!af.empty()) out.push_back({"rift_dust", (int)af.size()});
    if (plus >= 2) out.push_back({"iron_ingot", plus - 1});
    return out;
}
void Game::replaceTargetItem(const CraftTarget& t, const std::string& newId) {
    if (t.member >= 0) { party.members[t.member].equip[t.slot] = newId; Character& m = party.members[t.member]; m.hp = std::min(m.hp, m.maxHP(*gd)); }
    else if (t.inv >= 0 && t.inv < (int)party.inventory.size()) party.inventory[t.inv].id = newId;
}

// ---------------- UI ----------------
static const char* kCraftTabs[4] = {"Craft", "Reinforce", "Enchant", "Salvage"};
static bool tabAvailable(const std::string& st, int tab) {
    if (tab == 0) return true;
    if (tab == 1 || tab == 3) return st == "forge" || st == "workbench" || st == "all";
    if (tab == 2) return st == "alchemy" || st == "all";
    return false;
}
void Game::updateCraft(InputState& in, float dt) {
    (void)dt;
    if (actP(in, A_CANCEL)) { popMode(); return; }
    markSeenItems();
    auto nextTab = [&](int dir) { for (int k = 1; k <= 4; k++) { int t = (craftTab + dir * k + 8) % 4; if (tabAvailable(craftStation, t)) { craftTab = t; craftSel = 0; craftScroll = 0; craftQty = 1; return; } } };
    if (actP(in, A_PAGEUP)) nextTab(-1); if (actP(in, A_PAGEDOWN) || actP(in, A_MENU)) nextTab(1);
    for (int i = 0; i < 4; i++) { int tx = 20 + i * 96; if (tabAvailable(craftStation, i) && in.mousePressed[0] && in.mouseX >= tx && in.mouseX < tx + 92 && in.mouseY >= 30 && in.mouseY < 48) { craftTab = i; craftSel = 0; craftScroll = 0; craftQty = 1; } }
    int n = 0; std::vector<const Recipe*> list; std::vector<CraftTarget> targets;
    if (craftTab == 0) { list = recipesFor(*this, craftStation); n = (int)list.size(); } else { targets = craftTargets(craftTab); n = (int)targets.size(); }
    int before = craftSel;
    if (n > 0) { if (actP(in, A_UP)) craftSel = (craftSel + n - 1) % n; if (actP(in, A_DOWN)) craftSel = (craftSel + 1) % n; }
    craftSel = clampi(craftSel, 0, std::max(0, n - 1));
    if (in.wheel) craftScroll = clampi(craftScroll - in.wheel, 0, std::max(0, n - 17));
    if (craftSel < craftScroll) craftScroll = craftSel; if (craftSel >= craftScroll + 17) craftScroll = craftSel - 16;
    bool clicked = false;
    for (int i = 0; i < 17; i++) { int idx = craftScroll + i; if (idx >= n) break; if (in.mouseX >= 20 && in.mouseX < 330 && in.mouseY >= 56 + i * 18 && in.mouseY < 74 + i * 18) { if (in.mouseDX || in.mouseDY) craftSel = idx; if (in.mousePressed[0]) { craftSel = idx; clicked = true; } } }
    if (craftSel != before) craftQty = 1;
    if (n == 0) return;
    bool craftBtn = in.mousePressed[0] && in.mouseX >= 350 && in.mouseX < 500 && in.mouseY >= SCREEN_H - 64 && in.mouseY < SCREEN_H - 40;
    bool go = actP(in, A_CONFIRM) || craftBtn || (clicked && in.down[K_SHIFT]);
    if (craftTab == 0) {
        const Recipe& r = *list[craftSel];
        const ItemDef* rd = gd->item(r.result);
        bool batch = rd && !rd->equippable();
        if (batch) { if (actP(in, A_RIGHT)) craftQty = std::min(craftQty + 1, 20); if (actP(in, A_LEFT)) craftQty = std::max(1, craftQty - 1); }
        if (go) { int q = batch ? craftQty : 1; for (int i = 0; i < q && canCraft(r); i++) craft(r); craftQty = 1; }
        return;
    }
    CraftTarget t = targets[craftSel];
    const ItemDef* d = gd->item(t.id); if (!d) return;
    std::string base; std::vector<std::string> af; int plus = 0; parseDynId(t.id, base, af, plus);
    if (!go) return;
    if (craftTab == 1) {
        if (plus >= 3) { log("It can't be reinforced any further."); return; }
        int gold = 0; auto mats = upgradeCost(*d, plus, gold);
        for (auto& m : mats) if (party.countItem(m.first) < m.second) { log("You lack the materials."); gAudio.play("ui_error"); return; }
        if (party.gold < gold) { log("Not enough gold."); gAudio.play("ui_error"); return; }
        if (craftSkill(craftStation) < plus + 1) { log(fmt("Needs %s skill %d.", craftStation == "forge" ? "smithing" : "tinkering", plus + 1)); gAudio.play("ui_error"); return; }
        for (auto& m : mats) party.removeItem(m.first, m.second); party.gold -= gold; stats.goldSpent += gold;
        int chance = craftSuccess(craftStation, plus + 2);
        std::string st = craftStation;
        if (!rng.chance(chance)) { extra["cskill_" + st] = std::to_string(craftXP(st) + 6); log("The reinforcement fails; the materials are spent but the item is unharmed."); gAudio.play("ui_error"); return; }
        std::string nid = buildDynItem(base, af, plus + 1); replaceTargetItem(t, nid);
        extra["cskill_" + st] = std::to_string(craftXP(st) + (plus + 2) * 12);
        log("Reinforced: " + gd->item(nid)->name + "."); gAudio.play("reinforce"); stats.crafted++;
        if (plus + 1 >= 3) setFlag("reinforced3");
    } else if (craftTab == 2) {
        if (d->unique) { log(d->name + " is one of a kind: no enchantment will take."); gAudio.play("ui_error"); return; }
        int gold = 0; auto mats = enchantCost((int)af.size(), gold);
        for (auto& m : mats) if (party.countItem(m.first) < m.second) { log("You lack the materials."); gAudio.play("ui_error"); return; }
        if (party.gold < gold) { log("Not enough gold."); gAudio.play("ui_error"); return; }
        for (auto& m : mats) party.removeItem(m.first, m.second); party.gold -= gold; stats.goldSpent += gold;
        int chance = craftSuccess("alchemy", af.empty() ? 3 : 5);
        if (!rng.chance(chance)) { extra["cskill_alchemy"] = std::to_string(craftXP("alchemy") + 8); log("The enchantment fizzles. The dust is spent."); gAudio.play("ui_error"); return; }
        std::string nid = makeAffixed(t.id, af.empty() ? 2 : 3);
        if (nid == t.id) { log("Nothing takes hold on that item."); return; }
        replaceTargetItem(t, nid); extra["cskill_alchemy"] = std::to_string(craftXP("alchemy") + (af.empty() ? 36 : 60));
        log("Enchanted: " + gd->item(nid)->name + "."); gAudio.play("enchant"); stats.crafted++;
    } else if (craftTab == 3) {
        if (t.member >= 0) { log("Unequip it first - salvaging only takes items from the pack."); gAudio.play("ui_error"); return; }
        auto yield = salvageYield(*d);
        std::string got;
        for (auto& y : yield) if (gd->item(y.first) && y.second > 0) { party.addItem(y.first, y.second, *gd); got += (got.empty() ? "" : ", ") + itemLabel(*gd->item(y.first), y.second); }
        party.removeItem(t.id, 1);
        extra["cskill_" + craftStation] = std::to_string(craftXP(craftStation) + 3);
        log("Salvaged " + d->name + ": " + (got.empty() ? "nothing useful" : got) + "."); gAudio.play("salvage");
        craftSel = clampi(craftSel, 0, std::max(0, n - 2));
    }
}
void Game::drawCraft(Canvas& cv, InputState& in) {
    cv.dim(0, 0, SCREEN_W, SCREEN_H, 150); uiRect(0, 0, SCREEN_W, SCREEN_H);
    std::string skillName = craftStation == "forge" ? "Smithing" : craftStation == "alchemy" ? "Alchemy" : craftStation == "workbench" ? "Tinkering" : craftStation == "all" ? "Crafting" : "Field craft";
    std::string title = craftStation == "forge" ? "Forge" : craftStation == "alchemy" ? "Alchemy bench" : craftStation == "workbench" ? "Workbench" : craftStation == "all" ? "Crafting" : "Field crafting";
    panelTitle(ctx, 10, 6, SCREEN_W - 20, SCREEN_H - 12, title + fmt("   (%d gold)", party.gold));
    { std::string st = craftStation == "any" ? "field" : craftStation; int lv = craftSkill(craftStation), xp = craftXP(st); std::string s = fmt("%s skill %d", skillName.c_str(), lv) + (lv < 6 ? fmt("  (%d/%d)", xp, kSkillXP[lv]) : "  (master)"); textRight(ctx, SCREEN_W - 22, 32, s, C::Cyan); }
    for (int i = 0; i < 4; i++) { if (!tabAvailable(craftStation, i)) continue; int tx = 20 + i * 96; bool sel = craftTab == i; cv.fillRect(tx, 30, 92, 18, sel ? RGB(120, 90, 30) : RGB(40, 40, 54)); cv.rect(tx, 30, 92, 18, sel ? C::Gold : C::DkGray); textCenter(ctx, tx + 46, 33, kCraftTabs[i], sel ? C::Yellow : C::LtGray); }
    int x = 350, y = 56;
    auto matLines = [&](int& cy, const std::vector<std::pair<std::string, int>>& mats, int gold) {
        UI::text(ctx, x, cy, "Needs:", C::Gold); cy += 14;
        for (auto& m : mats) { const ItemDef* md = gd->item(m.first); int have = party.countItem(m.first); UI::text(ctx, x + 8, cy, fmt("%s  %d / %d", md ? md->name.c_str() : m.first.c_str(), have, m.second), have >= m.second ? C::Green : C::Red); cy += 13; }
        if (gold > 0) { UI::text(ctx, x + 8, cy, fmt("%d gold", gold), party.gold >= gold ? C::Green : C::Red); cy += 13; }
    };
    if (craftTab == 0) {
        auto list = recipesFor(*this, craftStation); int n = (int)list.size();
        int hidden = 0; for (auto& r : recipes) if ((r.station == "any" || r.station == craftStation || craftStation == "all") && !recipeVisible(r)) hidden++;
        if (n == 0) UI::text(ctx, 20, 58, "No recipes known for this station.", C::Gray);
        for (int i = 0; i < 17; i++) {
            int idx = craftScroll + i; if (idx >= n) break; const Recipe& r = *list[idx];
            bool ok = canCraft(r); listRow(ctx, in, 20, 56 + i * 18, 310, 18, idx == craftSel);
            if (auto* d = gd->item(r.result)) drawItemTile(*ctx.ts, cv, *d, 24, 58 + i * 18, 1);
            UI::text(ctx, 42, 59 + i * 18, r.name + (r.count > 1 ? fmt(" x%d", r.count) : ""), ok ? C::White : C::Gray);
            std::string stars; for (int k = 0; k < r.difficulty; k++) stars += "*";
            textRight(ctx, 326, 59 + i * 18, stars, r.difficulty > craftSkill(r.station) ? C::Red : C::Gold);
        }
        if (hidden) UI::text(ctx, 20, SCREEN_H - 62, fmt("%d more recipe%s to discover.", hidden, hidden == 1 ? "" : "s"), C::DkGray);
        if (n > 0) {
            const Recipe& r = *list[clampi(craftSel, 0, n - 1)];
            if (auto* d = gd->item(r.result)) {
                drawItemTile(*ctx.ts, cv, *d, x, y, 2); ctx.font->bold = true; UI::text(ctx, x + 30, y + 2, d->name, C::Gold); ctx.font->bold = false;
                int cy = y + 30; cy += wrapped(ctx, x, cy, SCREEN_W - x - 24, d->desc, C::LtGray) + 4;
                if (d->dmgMax > 0) { UI::text(ctx, x, cy, fmt("Damage %d-%d %s, range %d, %d AP", d->dmgMin, d->dmgMax, dmgTypeName(d->dmgType), d->range, d->ap), C::White); cy += 13; }
                if (d->def || d->evasion) { UI::text(ctx, x, cy, fmt("Defense %d  Evasion %d", d->def, d->evasion), C::White); cy += 13; }
                std::string bon; for (int b = 0; b < ST_COUNT; b++) if (d->bonus[b]) bon += fmt("%s %+d  ", statName(b), d->bonus[b]); for (int rr = 0; rr < DT_COUNT; rr++) if (d->res[rr]) bon += fmt("%s res %+d%%  ", dmgTypeName(rr), d->res[rr]);
                if (!bon.empty()) { cy += wrapped(ctx, x, cy, SCREEN_W - x - 24, bon, C::Cyan); }
                if (d->heal || d->mana) { UI::text(ctx, x, cy, fmt("Heals %d HP / %d MP", d->heal, d->mana), C::White); cy += 13; }
                cy += 6; matLines(cy, r.mats, r.gold);
                if (r.level > 1) { UI::text(ctx, x + 8, cy, fmt("hero level %d", r.level), (party.hero() && party.hero()->level >= r.level) ? C::Green : C::Red); cy += 13; }
                if (r.skill > 1) { int have = craftSkill(r.station); UI::text(ctx, x + 8, cy, fmt("%s skill %d (you have %d)", r.station == "forge" ? "Smithing" : r.station == "alchemy" ? "Alchemy" : r.station == "workbench" ? "Tinkering" : "Field craft", r.skill, have), have >= r.skill ? C::Green : C::Red); cy += 13; }
                int ch = craftSuccess(r.station, r.difficulty);
                cy += 4; UI::text(ctx, x, cy, fmt("Difficulty %d  -  success %d%%", r.difficulty, ch), ch >= 100 ? C::Green : ch >= 60 ? C::Yellow : C::Red); cy += 13;
                if (d->equippable()) { int over = craftSkill(r.station) - r.difficulty, stat = craftStat(r.station); UI::text(ctx, x, cy, fmt("Fine %d%%  Masterwork %d%%", clampi(8 + over * 10 + (stat - 10), 3, 60), clampi(over * 5 + (stat - 12), 0, 25)), C::Cyan); cy += 13; }
                if (ch < 100) { UI::text(ctx, x, cy, "A failure ruins half the materials.", C::DkGray); cy += 13; }
                bool batch = !d->equippable();
                if (batch) UI::text(ctx, x, SCREEN_H - 84, fmt("Quantity: < %d >  (Left/Right)", craftQty), C::LtGray);
            }
            button(ctx, in, 350, SCREEN_H - 64, 150, 24, craftQty > 1 ? fmt("Craft x%d", craftQty) : "Craft", canCraft(r), canCraft(r));
        }
    } else {
        auto targets = craftTargets(craftTab); int n = (int)targets.size();
        if (n == 0) UI::text(ctx, 20, 58, craftTab == 3 ? "Nothing to salvage in your pack." : "Nothing here can be worked on at this station.", C::Gray);
        for (int i = 0; i < 17; i++) {
            int idx = craftScroll + i; if (idx >= n) break; auto& t = targets[idx]; const ItemDef* d = gd->item(t.id); if (!d) continue;
            listRow(ctx, in, 20, 56 + i * 18, 310, 18, idx == craftSel);
            drawItemTile(*ctx.ts, cv, *d, 24, 58 + i * 18, 1);
            UI::text(ctx, 42, 59 + i * 18, d->name.substr(0, 34), C::White);
            if (t.member >= 0) textRight(ctx, 326, 59 + i * 18, party.members[t.member].name.substr(0, 6), C::DkGray);
        }
        if (n > 0) {
            auto& t = targets[clampi(craftSel, 0, n - 1)]; const ItemDef* d = gd->item(t.id);
            std::string base; std::vector<std::string> af; int plus = 0; parseDynId(t.id, base, af, plus);
            if (d) {
                drawItemTile(*ctx.ts, cv, *d, x, y, 2); ctx.font->bold = true; UI::text(ctx, x + 30, y + 2, d->name.substr(0, 30), C::Gold); ctx.font->bold = false;
                int cy = y + 30;
                if (d->dmgMax > 0) { UI::text(ctx, x, cy, fmt("Damage %d-%d %s  Acc %+d", d->dmgMin, d->dmgMax, dmgTypeName(d->dmgType), d->accuracy), C::White); cy += 13; }
                if (d->def || d->evasion) { UI::text(ctx, x, cy, fmt("Defense %d  Evasion %d", d->def, d->evasion), C::White); cy += 13; }
                cy += 6;
                bool can = true; std::string label;
                if (craftTab == 1) {
                    if (plus >= 3) { UI::text(ctx, x, cy, "Fully reinforced (+3).", C::Green); can = false; label = "Maxed"; }
                    else {
                        UI::text(ctx, x, cy, fmt("Reinforce to +%d: ", plus + 1) + (d->type == IT_WEAPON ? "+1/+2 damage, +2 accuracy" : d->type == IT_ACCESSORY ? "+5 HP" : "+1 defense"), C::Cream); cy += 16;
                        int gold = 0; auto mats = upgradeCost(*d, plus, gold); matLines(cy, mats, gold);
                        for (auto& m : mats) if (party.countItem(m.first) < m.second) can = false; if (party.gold < gold) can = false;
                        int need = plus + 1; UI::text(ctx, x, cy, fmt("Needs %s skill %d", craftStation == "forge" ? "smithing" : "tinkering", need), craftSkill(craftStation) >= need ? C::Green : C::Red); cy += 13;
                        if (craftSkill(craftStation) < need) can = false;
                        UI::text(ctx, x, cy, fmt("Success %d%% (failure keeps the item)", craftSuccess(craftStation, plus + 2)), C::Yellow); cy += 13;
                        label = fmt("Reinforce +%d", plus + 1);
                    }
                } else if (craftTab == 2 && d->unique) {
                    UI::text(ctx, x, cy, "One of a kind: it can't be enchanted.", C::Gray); can = false; label = "Unique";
                } else if (craftTab == 2) {
                    UI::text(ctx, x, cy, af.empty() ? "Bind a random enchantment to it." : "Bind a second enchantment (rare).", C::Cream); cy += 16;
                    int gold = 0; auto mats = enchantCost((int)af.size(), gold); matLines(cy, mats, gold);
                    for (auto& m : mats) if (party.countItem(m.first) < m.second) can = false; if (party.gold < gold) can = false;
                    UI::text(ctx, x, cy, fmt("Success %d%%", craftSuccess("alchemy", af.empty() ? 3 : 5)), C::Yellow); cy += 13;
                    label = "Enchant";
                } else {
                    UI::text(ctx, x, cy, "Break it down into:", C::Cream); cy += 14;
                    for (auto& yv : salvageYield(*d)) if (auto* md = gd->item(yv.first)) { UI::text(ctx, x + 8, cy, itemLabel(*md, yv.second), C::Green); cy += 13; }
                    if (t.member >= 0) { UI::text(ctx, x, cy + 4, "Equipped - unequip it first.", C::Red); can = false; }
                    label = "Salvage";
                }
                button(ctx, in, 350, SCREEN_H - 64, 150, 24, label, can, can);
            }
        }
    }
    int hx = keyHint(ctx, 20, SCREEN_H - 30, "Esc", "leave"); hx = keyHint(ctx, hx, SCREEN_H - 30, "Up/Down", "select");
    if (craftTab == 0) hx = keyHint(ctx, hx, SCREEN_H - 30, "L/R", "quantity");
    int avail = 0; for (int i = 0; i < 4; i++) if (tabAvailable(craftStation, i)) avail++;
    if (avail > 1) keyHint(ctx, hx, SCREEN_H - 30, gPadGlyphs ? "LB/RB" : "PgUp/PgDn", "tabs");
}
