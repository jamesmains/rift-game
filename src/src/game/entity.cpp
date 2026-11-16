#include "entity.h"
#include "tiles.h"
#include <algorithm>

int Character::stat(int s, const GameData& gd) const {
    int v = baseStats[s];
    for (int i = 0; i < SL_COUNT; i++) if (!equip[i].empty()) if (auto* it = gd.item(equip[i])) v += it->bonus[s];
    for (auto& e : effects) {
        if (e.id == "strength" && s == ST_STR) v += e.power;
        if (e.id == "weaken" && s == ST_STR) v -= e.power;
        if (e.id == "haste" && s == ST_DEX) v += e.power;
        if (e.id == "focus" && s == ST_INT) v += e.power;
    }
    return std::max(1, v);
}
// party members: 8 + CON x3 + each class rank's hit die (Warrior 8, Ranger/Cleric/Scavenger 6, Mage 4)
static int rankSum(const Character& c, const GameData& gd, bool hp) {
    int v = 0;
    if (c.ranks.empty()) { if (auto* cd = gd.cls(c.classId)) v = c.level * (hp ? cd->hitDie : cd->mpDie); else v = c.level * (hp ? 6 : 2); return v; }
    for (auto& r : c.ranks) { const ClassDef* cd = gd.cls(r.first); v += r.second * (cd ? (hp ? cd->hitDie : cd->mpDie) : (hp ? 6 : 2)); }
    return v;
}
std::string Character::classSummary(const GameData& gd) const {
    std::string s;
    std::vector<std::pair<int, std::string>> v; for (auto& r : ranks) if (r.second > 0) v.push_back({-r.second * 2 - (r.first == startClass ? 1 : 0), r.first});
    std::sort(v.begin(), v.end());
    for (auto& p : v) { const ClassDef* cd = gd.cls(p.second); s += (s.empty() ? "" : " / ") + (cd ? cd->name : p.second) + " " + std::to_string(rank(p.second)); }
    if (s.empty()) { const ClassDef* cd = gd.cls(classId); s = cd ? cd->name : classId; }
    return s;
}
int Character::maxHP(const GameData& gd) const {
    int v = baseHP > 0 ? baseHP : 8 + stat(ST_CON, gd) * 3 + rankSum(*this, gd, true);
    for (int i = 0; i < SL_COUNT; i++) if (!equip[i].empty()) if (auto* it = gd.item(equip[i])) v += it->bonusHP;
    return v;
}
int Character::maxMP(const GameData& gd) const {
    int v = baseMP > 0 ? baseMP : (baseHP > 0 ? 0 : stat(ST_INT, gd) * 2 + stat(ST_WIS, gd) * 2 + rankSum(*this, gd, false));
    for (int i = 0; i < SL_COUNT; i++) if (!equip[i].empty()) if (auto* it = gd.item(equip[i])) v += it->bonusMP;
    return v;
}
int Character::maxAP(const GameData& gd) const {
    int v = baseHP > 0 ? mApMax : 8 + stat(ST_DEX, gd) / 5;
    for (int i = 0; i < SL_COUNT; i++) if (!equip[i].empty()) if (auto* it = gd.item(equip[i])) v += it->bonusAP;
    if (hasEffect("haste")) v += 2;
    if (hasEffect("freeze")) v -= 2;
    return std::max(2, v);
}
int Character::maxMove(const GameData& gd) const {
    int v = baseHP > 0 ? mMove : 3 + stat(ST_DEX, gd) / 3;
    for (int i = 0; i < SL_COUNT; i++) if (!equip[i].empty()) if (auto* it = gd.item(equip[i])) v += it->bonusMove;
    if (hasEffect("haste")) v += 2;
    if (hasEffect("freeze") || hasEffect("slow")) v = std::max(1, v / 2);
    v += fx("move", gd);
    return std::max(1, v);
}
int Character::defense(const GameData& gd) const {
    int v = baseDef;
    for (int i = 0; i < SL_COUNT; i++) if (!equip[i].empty()) if (auto* it = gd.item(equip[i])) v += it->def;
    if (hasEffect("shield")) v += 4;
    return v;
}
int Character::evasion(const GameData& gd) const {
    int v = baseEvasion + stat(ST_DEX, gd);
    for (int i = 0; i < SL_COUNT; i++) if (!equip[i].empty()) if (auto* it = gd.item(equip[i])) v += it->evasion;
    if (hasEffect("stun") || hasEffect("freeze")) v -= 15;
    v += bond * 5;
    return v;
}
int Character::resist(int dt, const GameData& gd) const {
    int v = baseRes[dt];
    for (int i = 0; i < SL_COUNT; i++) if (!equip[i].empty()) if (auto* it = gd.item(equip[i])) v += it->res[dt];
    return std::min(90, v);
}
int Character::accuracy(const GameData& gd) const {
    int v = baseHP > 0 ? mAcc : 0;
    if (auto* w = weapon(gd)) v += w->accuracy;
    if (hasEffect("aim")) v += 25;
    if (hasEffect("blind")) v -= 30;
    v += bond * 10;
    return v;
}
const ItemDef* Character::weapon(const GameData& gd) const {
    if (equip[SL_WEAPON].empty()) return nullptr;
    return gd.item(equip[SL_WEAPON]);
}
int Character::attackRange(const GameData& gd) const { if (auto* w = weapon(gd)) return w->trait == "reach" && w->range <= 1 ? 2 : w->range; return baseHP > 0 ? mRange : 1; }
int Character::attackAP(const GameData& gd) const { if (auto* w = weapon(gd)) return w->trait == "quick" ? std::max(1, w->ap - 1) : w->ap; return baseHP > 0 ? mAp : 3; }
bool Character::meleeWeapon(const GameData& gd) const { if (auto* w = weapon(gd)) return w->range <= 1; return (baseHP > 0 ? mRange : 1) <= 1; }
std::string Character::trait(const GameData& gd) const { if (auto* w = weapon(gd)) return w->trait; return ""; }
bool Character::hasShield(const GameData& gd) const { if (equip[SL_OFFHAND].empty()) return false; const ItemDef* d = gd.item(equip[SL_OFFHAND]); return d && d->type == IT_OFFHAND && d->def > 0; }
int Character::fx(const std::string& key, const GameData& gd) const {
    int v = 0;
    for (int i = 0; i < SL_COUNT; i++) if (!equip[i].empty()) if (auto* it = gd.item(equip[i])) { auto f = it->fx.find(key); if (f != it->fx.end()) v += f->second; }
    { auto f = cfx.find(key); if (f != cfx.end()) v += f->second; }
    for (auto& r : ranks) if (r.second > 0) if (const ClassDef* cd = gd.cls(r.first)) for (auto& pk : cd->perks) if (pk.rank <= r.second) { auto f = pk.fx.find(key); if (f != pk.fx.end()) v += f->second; }
    return v;
}
void Character::attackDamage(const GameData& gd, int& lo, int& hi, int& type) const {
    if (auto* w = weapon(gd)) {
        int s = stat(w->scaleStat, gd);
        int bonus = (s - 8) / 2;
        lo = std::max(1, w->dmgMin + bonus); hi = std::max(lo, w->dmgMax + bonus); type = w->dmgType;
    } else if (baseHP > 0) { lo = mDmgMin; hi = mDmgMax; type = mDmgType; }
    else { int s = stat(ST_STR, gd); lo = 1 + s / 6; hi = 2 + s / 3; type = DT_PHYS; }
}
void Character::addEffect(const std::string& id, int turns, int power) {
    for (auto& e : effects) if (e.id == id) { e.turns = std::max(e.turns, turns); e.power = std::max(e.power, power); return; }
    effects.push_back({id, turns, power});
}
void Character::removeEffect(const std::string& id) {
    for (size_t i = 0; i < effects.size(); i++) if (effects[i].id == id) { effects.erase(effects.begin() + i); return; }
}

void Character::initFromClass(const GameData& gd, const ClassDef& c, const std::string& nm) {
    name = nm; classId = c.id; creatureId.clear(); startClass = c.id; ranks.clear(); ranks[c.id] = 1; rankPoints = 0;
    for (int i = 0; i < ST_COUNT; i++) baseStats[i] = c.stats[i];
    tile = T(c.spriteCol, c.spriteRow); size = 1; recolor = false;
    level = 1; xp = 0; baseHP = baseMP = 0; baseDef = 0; baseEvasion = 0;
    for (int i = 0; i < SL_COUNT; i++) equip[i].clear();
    if (!c.startWeapon.empty()) equip[SL_WEAPON] = c.startWeapon;
    if (!c.startArmor.empty()) equip[SL_BODY] = c.startArmor;
    spells = c.startSpells;
    isPlayerSide = true; faction = "player"; alive = true; effects.clear();
    hp = maxHP(gd); mp = maxMP(gd);
}

void Character::initFromCreature(const GameData& gd, const CreatureDef& c, int lvlBonus) {
    name = c.name; creatureId = c.id; classId.clear();
    tile = c.tile; size = c.size; recolor = c.recolor; color = c.color;
    level = c.level + lvlBonus;
    for (int i = 0; i < ST_COUNT; i++) baseStats[i] = c.stats[i] + lvlBonus;
    baseHP = c.hp + lvlBonus * 6; baseMP = c.mp; baseDef = c.def + lvlBonus / 2; baseEvasion = c.evasion;
    for (int i = 0; i < DT_COUNT; i++) baseRes[i] = c.res[i];
    mDmgMin = c.dmgMin + lvlBonus; mDmgMax = c.dmgMax + lvlBonus; mDmgType = c.dmgType; mRange = c.range; mAp = c.ap; mApMax = c.apMax; mMove = c.move; mAcc = c.accuracy;
    ai = c.ai; spells = c.spells; boss = c.boss; undead = c.undead; mechanical = c.mechanical; darkvision = c.darkvision; faction = c.faction;
    role = c.role; windup = c.windup; ward = c.ward; snuff = c.snuff; cfx = c.fx;
    onHitEffect = c.onHitEffect; xpReward = c.xp + lvlBonus * 8; lootGold = c.lootGold; loot = c.loot;
    for (int i = 0; i < SL_COUNT; i++) equip[i].clear();
    isPlayerSide = false; alive = true; effects.clear();
    hp = maxHP(gd); mp = maxMP(gd);
}

int Party::addItem(const std::string& id, int count, const GameData& gd) {
    const ItemDef* d = gd.item(id);
    if (!d) return 0;
    if (d->stackable()) {
        // stack= caps one inventory slot; the rest spills into a new slot
        int cap = std::max(1, d->stackMax), left = count;
        for (auto& s : inventory) if (s.id == id && s.count < cap && left > 0) { int add = std::min(left, cap - s.count); s.count += add; left -= add; }
        while (left > 0) { int add = std::min(left, cap); inventory.push_back({id, add}); left -= add; }
    } else {
        for (int i = 0; i < count; i++) inventory.push_back({id, 1});
    }
    return count;
}
bool Party::removeItem(const std::string& id, int count) {
    for (size_t i = 0; i < inventory.size(); i++) {
        if (inventory[i].id != id) continue;
        if (inventory[i].count > count) { inventory[i].count -= count; return true; }
        count -= inventory[i].count;
        inventory.erase(inventory.begin() + i); i--;
        if (count <= 0) return true;
    }
    return count <= 0;
}
int Party::countItem(const std::string& id) const { int n = 0; for (auto& s : inventory) if (s.id == id) n += s.count; return n; }
