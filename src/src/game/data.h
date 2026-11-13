// Static game data: item, creature and spell definitions loaded from assets/data/*.txt
#pragma once
#include <string>
#include <vector>
#include <map>
#include "../core/gfx.h"
#include "../core/util.h"

enum DmgType { DT_PHYS = 0, DT_FIRE, DT_COLD, DT_SHOCK, DT_POISON, DT_MAGIC, DT_COUNT };
const char* dmgTypeName(int t);
int dmgTypeFromName(const std::string& s);

enum Stat { ST_STR = 0, ST_DEX, ST_CON, ST_INT, ST_WIS, ST_CHA, ST_COUNT };
const char* statName(int s);
int statFromName(const std::string& s);

enum ItemType { IT_WEAPON = 0, IT_ARMOR, IT_HELM, IT_BOOTS, IT_ACCESSORY, IT_OFFHAND, IT_CONSUMABLE, IT_KEY, IT_MATERIAL, IT_AMMO, IT_TYPE_COUNT };
enum EquipSlot { SL_WEAPON = 0, SL_OFFHAND, SL_HEAD, SL_BODY, SL_FEET, SL_ACCESSORY, SL_COUNT };
const char* slotName(int s);

struct ItemDef {
    std::string id, name, desc;
    ItemType type = IT_MATERIAL;
    Tile tile;
    int value = 0;
    // weapon
    int dmgMin = 0, dmgMax = 0, dmgType = DT_PHYS, range = 1, ap = 4, scaleStat = ST_STR;
    bool twoHanded = false, isGun = false;
    std::string ammo; // item id consumed per shot (empty = none)
    int accuracy = 0; // to-hit bonus
    int crit = 0;
    // armor
    int def = 0, evasion = 0;
    int res[DT_COUNT] = {};
    int bonus[ST_COUNT] = {};
    int bonusHP = 0, bonusMP = 0, bonusAP = 0, bonusMove = 0;
    // consumable
    int heal = 0, mana = 0;
    std::string effect;   // e.g. "cure_poison", "haste", "strength", "revive"
    std::string useSpell; // casts spell when used (scrolls)
    std::string sound;    // sfx event played on attack/use (empty = by type)
    int stackMax = 99;
    std::string salvage;  // what breaking it down at a forge/workbench yields ("iron_ingot:1,scrap:2"; empty = worked out from the item)
    bool tech = false;
    int light = 0;        // carried light radius on dark maps while equipped (lanterns)    // guns and machine-made gear: upgraded and salvaged at the workbench instead of the forge
    bool quest = false;          // quest item: can't be dropped or sold (default for type=key)
    std::string category;        // inventory / shop category (empty = by type)
    bool unique = false;         // unique=1 (1.1.3): hand-made one-off - never rolls or takes affixes (loot, crafting quality, enchanting)
    bool recolor = false; Color8 color = C::White;   // color=r,g,b (1.1.8): the sprite drawn as a white shape in this colour; none = the sheet's own colour
    bool stackable() const { return type == IT_CONSUMABLE || type == IT_MATERIAL || type == IT_AMMO || type == IT_KEY; }
    int slot() const {
        switch (type) { case IT_WEAPON: return SL_WEAPON; case IT_OFFHAND: return SL_OFFHAND; case IT_HELM: return SL_HEAD;
                        case IT_ARMOR: return SL_BODY; case IT_BOOTS: return SL_FEET; case IT_ACCESSORY: return SL_ACCESSORY; default: return -1; }
    }
    bool equippable() const { return slot() >= 0; }
};

// 1.1.8: an item's icon, in its color= when it has one (every item picture on the sheet is a single colour)
inline void drawItemTile(const Tileset& ts, Canvas& cv, const ItemDef& d, int x, int y, int scale) { if (d.recolor) ts.drawRecolor(cv, d.tile, x, y, scale, d.color); else ts.draw(cv, d.tile, x, y, scale); }

struct SpellDef {
    std::string id, name, desc;
    Tile tile;
    int cost = 5, ap = 4, range = 5, area = 0, level = 1;
    int dmgMin = 0, dmgMax = 0, dmgType = DT_MAGIC, scaleStat = ST_INT;
    int heal = 0;
    std::string effect; // buff/debuff id applied: "burn","freeze","shock","poison","regen","haste","shield","weaken"
    int duration = 0;
    std::string target = "enemy"; // enemy | ally | self | any | ground
    std::string summon;           // creature[:count] called into the fight (see Game::combatSummon); duration = its turns
    std::string utility;          // out-of-combat use: light | stealth | detect | unlock | recall | heal (heal/cure/revive also work automatically)
    int utilityPower = 0;
    std::string sound;            // sfx event (empty = by damage type)
    bool friendlyFire = true;     // area damage also hits the caster and allies standing in the blast (ff=0 for weapon skills like Cleave)
};

struct CreatureDef {
    std::string id, name;
    Tile tile; int size = 1; bool recolor = false; Color8 color = C::White;
    int hp = 10, mp = 0, stats[ST_COUNT] = {8, 8, 8, 8, 8, 8};
    int def = 0, evasion = 0, res[DT_COUNT] = {};
    int dmgMin = 1, dmgMax = 3, dmgType = DT_PHYS, range = 1, ap = 4, apMax = 6, move = 4, accuracy = 0;
    std::string ai = "melee"; // melee | ranged | caster | support | coward | boss
    std::vector<std::string> spells;
    int xp = 10, level = 1;
    std::string lootGold = "0-0"; // min-max
    std::vector<std::pair<std::string, int>> loot; // item id, percent chance
    std::string onHitEffect; // e.g. poison
    bool boss = false, undead = false, mechanical = false, darkvision = false;
    std::string faction = "monster"; // monster | bandit | machine | wild | undead | rift
    std::string sound;   // death sfx event (empty = death_enemy)
    std::string attackSound; // attack sfx event (empty = by weapon type)
};

struct ClassDef {
    std::string id, name, desc;
    int stats[ST_COUNT] = {8, 8, 8, 8, 8, 8};
    std::vector<std::string> startItems, startSpells;
    std::string startWeapon, startArmor;
    int spriteRow = 0; int spriteCol = 104;
    std::vector<std::pair<int, std::string>> spellLevels; // class rank -> spell id learned
    int hitDie = 6, mpDie = 2;   // max HP / MP gained per rank in this class
    std::string role;            // one-line summary shown when choosing a class rank
};

// the category an item is filed under in the inventory and shop windows
std::string itemCategory(const ItemDef& d);
int itemCategoryOrder(const std::string& cat);   // fixed order for the built-in categories, custom ones after
struct GameData {
    std::map<std::string, ItemDef> items;
    std::map<std::string, SpellDef> spells;
    std::map<std::string, CreatureDef> creatures;
    std::map<std::string, ClassDef> classes;
    std::vector<std::string> classOrder;
    std::string dataDir;
    std::string overlayDir;   // campaign folder layered over dataDir ("" = base game only)
    bool load(const std::string& dataDir);
    bool loadOverlay(const std::string& dir);
    bool loadFiles(const std::string& dir, bool required);
    const ItemDef* item(const std::string& id) const { auto it = items.find(id); return it == items.end() ? nullptr : &it->second; }
    const SpellDef* spell(const std::string& id) const { auto it = spells.find(id); return it == spells.end() ? nullptr : &it->second; }
    const CreatureDef* creature(const std::string& id) const { auto it = creatures.find(id); return it == creatures.end() ? nullptr : &it->second; }
    const ClassDef* cls(const std::string& id) const { auto it = classes.find(id); return it == classes.end() ? nullptr : &it->second; }
    std::string lastError;
};

Tile parseTile(const std::string& s);   // "col,row" or "col,row@sheet" (1.2.0)
std::string tileText(Tile t);           // the same back ("" for no tile)
Color8 parseColor(const std::string& s);
void parseRange(const std::string& s, int& lo, int& hi);
