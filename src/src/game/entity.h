// Characters (party members, NPCs, monsters) and the party.
#pragma once
#include <string>
#include <vector>
#include <map>
#include "data.h"

struct StatusEffect {
    std::string id;   // burn, freeze, shock, poison, regen, haste, shield, weaken, stun, aim
    int turns = 0;
    int power = 0;
};

struct ItemStack { std::string id; int count = 1; };

struct Character {
    std::string name;
    std::string classId;      // for party members
    std::string creatureId;   // for monsters/NPC combatants
    Tile tile; int size = 1; bool recolor = false; Color8 color = C::White;
    int level = 1, xp = 0;
    int baseStats[ST_COUNT] = {8, 8, 8, 8, 8, 8};
    int hp = 1, mp = 0;
    int ap = 0, move = 0;               // current pool during combat
    int baseDef = 0, baseEvasion = 0, baseRes[DT_COUNT] = {};
    int baseHP = 0, baseMP = 0;         // monsters: fixed max; party: 0 = derived
    // monster attack
    int mDmgMin = 1, mDmgMax = 3, mDmgType = DT_PHYS, mRange = 1, mAp = 4, mApMax = 6, mMove = 4, mAcc = 0;
    std::string ai = "melee";
    std::string equip[SL_COUNT];        // item ids
    std::vector<std::string> spells;
    std::vector<StatusEffect> effects;
    int statPoints = 0;
    bool isPlayerSide = false, alive = true, boss = false, undead = false, mechanical = false, darkvision = false;
    std::string faction = "player";
    std::string onHitEffect;
    int xpReward = 0; std::string lootGold; std::vector<std::pair<std::string, int>> loot;
    // position during combat / on map
    int x = 0, y = 0;
    int combatId = -1;
    std::string portraitHint;
    std::string favs[8];                // favorites hotbar: "spell:id" or "item:id"
    // 1.4.0: creature extras (creatures.txt role= windup= ward= snuff= fx=); party members get fx from gear and class perks
    std::string role, windup, ward; bool snuff = false; std::map<std::string, int> cfx;
    int bond = 0;                       // companions: +1 Loyal (+10 hit, +5 evasion), -1 Strained (-10 / -5); Game::syncBonds
    // multiclassing: every level is a rank in some class (D&D style). level == sum of ranks.
    std::map<std::string, int> ranks;   // class id -> rank
    int rankPoints = 0;                 // class ranks waiting to be assigned on the level-up screen
    std::string startClass;             // the class the character began as (respec goes back to it)
    int rank(const std::string& cls) const { auto it = ranks.find(cls); return it == ranks.end() ? 0 : it->second; }
    std::string classSummary(const GameData& gd) const;   // "Warrior 3 / Mage 1"

    // ---- derived ----
    int stat(int s, const GameData& gd) const;
    int maxHP(const GameData& gd) const;
    int maxMP(const GameData& gd) const;
    int maxAP(const GameData& gd) const;
    int maxMove(const GameData& gd) const;
    int defense(const GameData& gd) const;
    int evasion(const GameData& gd) const;
    int resist(int dt, const GameData& gd) const;
    int accuracy(const GameData& gd) const;
    const ItemDef* weapon(const GameData& gd) const;
    int attackRange(const GameData& gd) const;
    int attackAP(const GameData& gd) const;
    bool meleeWeapon(const GameData& gd) const;   // 1.4.0: fights up close (range 1 weapon, spears included)
    std::string trait(const GameData& gd) const;  // 1.4.0 (#68): the weapon's family trait ("" = none)
    int fx(const std::string& key, const GameData& gd) const;   // 1.4.0: gear fx= + class passives/perks + creature fx
    bool hasShield(const GameData& gd) const;     // an off-hand with def (a shield) is worn
    void attackDamage(const GameData& gd, int& lo, int& hi, int& type) const;
    bool hasEffect(const std::string& id) const { for (auto& e : effects) if (e.id == id) return true; return false; }
    StatusEffect* effect(const std::string& id) { for (auto& e : effects) if (e.id == id) return &e; return nullptr; }
    void addEffect(const std::string& id, int turns, int power = 0);
    void removeEffect(const std::string& id);
    int xpToNext() const { return level * level * 40 + level * 60; }
    void fullHeal(const GameData& gd) { hp = maxHP(gd); mp = maxMP(gd); effects.clear(); alive = true; }
    void initFromClass(const GameData& gd, const ClassDef& c, const std::string& nm);
    void initFromCreature(const GameData& gd, const CreatureDef& c, int lvlBonus = 0);
    bool canUseSpells() const { return !spells.empty(); }
};

struct Party {
    std::vector<Character> members;   // index 0 = hero
    std::vector<ItemStack> inventory;
    int gold = 0;
    int addItem(const std::string& id, int count, const GameData& gd);
    bool removeItem(const std::string& id, int count = 1);
    int countItem(const std::string& id) const;
    bool hasItem(const std::string& id) const { return countItem(id) > 0; }
    Character* hero() { return members.empty() ? nullptr : &members[0]; }
    bool allDead() const { for (auto& m : members) if (m.alive) return false; return true; }
    int aliveCount() const { int n = 0; for (auto& m : members) if (m.alive) n++; return n; }
};
