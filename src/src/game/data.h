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
// 1.4.0 (#73) stat checks: "cha>=12" (the part after stat: / partystat:) -> stat index, operator (>= > <= < =), number
bool parseStatCheck(const std::string& body, int& stat, std::string& op, int& value);
bool statCheckPasses(int have, const std::string& op, int value);
// the tag a dialogue answer shows in front of its text for the checks in its conditions: "[CHA 12] ", "[Party STR 14] ",
// "[Sneaking] " (empty when it has none)
std::string dialogueCheckTag(const std::vector<std::string>& conds);

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
    // 1.4.0 (#67/#68): trait= weapon family trait (reach cleave backstab crush arc steady quick spread) and fx= effects
    // (key:value list, also filled by affix mods; see fxText / Character::fx): ignorehalfcover flankcrit noowpenalty ...
    std::string trait;
    std::map<std::string, int> fx;
    // 1.5.0 (#93) class gearing: family= weapon family (blade dagger mace axe spear staff wand bow crossbow gun; empty = anyone
    // uses it well), weight= light|medium|heavy (armour, helms, boots, shields), req=str:14 (stat needs), wornby= classes
    // that wear it without a weight penalty whatever it weighs (power armour for the scavenger)
    std::string family; int weight = 0; int req[ST_COUNT] = {}; std::vector<std::string> wornby;
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
    // 1.4.0: shape= ball (default: area radius) | line (caster through the target, range long) | cone (widens with distance);
    // chain=jumps:range:falloff% (hits the target, then jumps to the nearest unit not yet hit); knockback=1 shoves the target
    // one tile away; smoke=N rounds of smoke over the area; ground=fire|ice leaves burning ground / ice on every tile it covers
    std::string shape;
    int chainJumps = 0, chainRange = 3, chainFalloff = 25;
    int knockback = 0, smoke = 0;
    std::string ground;
    // 1.4.0 (#66) upgrade=RANK:key=value,key=value:text - from that rank in the spell's class (cls) the spell changes
    struct Upgrade { int rank = 0; std::vector<std::pair<std::string, std::string>> kv; std::string text; };
    std::vector<Upgrade> upgrades;
    std::string cls;              // the class whose learn= / spells= teaches it (worked out after loading)
};
// applies one spells.txt key to a spell (used for loading and for upgrade= at cast time); false = unknown key
bool applySpellKey(SpellDef& d, const std::string& k, const std::string& v);
// "key:value,key:value" (or ;) into a map, adding to what is there
void parseFx(const std::string& s, std::map<std::string, int>& out);
std::string fxText(const std::map<std::string, int>& fx);
// plain words for an fx key / a weapon trait (item details, level-up screen, Editor)
std::string fxDescribe(const std::string& key, int v);
std::string traitDescribe(const std::string& trait);
// 1.5.0 (#92): the weapon traits in a fixed order (their discovery bits are 8 + index in rk_<creature>)
const std::vector<std::string>& weaponTraits();
int traitIndex(const std::string& trait);   // -1 = not a trait
// res= text into damage-type values and weapon-trait values
void parseResList(const std::string& s, int* res, std::map<std::string, int>* traitRes);

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
    std::string music;       // 1.3.2: fight music while it is in the fight (bosses: empty = the "boss" track)
    // 1.4.0: role= guard (adjacent allies count as in half cover) | sniper (holds overwatch on a lane) | support (buffs
    // allies) | leader (the rest of its group breaks and flees when it dies); windup=spell (bosses: announced a turn ahead,
    // lands on the marked tiles); snuff=1 puts out braziers in a fight; ward=creature: takes half damage while one lives
    std::string role, windup, ward;
    bool snuff = false;
    std::map<std::string, int> fx;
    std::map<std::string, int> traitRes;   // 1.5.0 (#92): res= keys naming a weapon trait (crush:-100): replaces physical for such weapons
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
    // 1.4.0 (#71): passive= (rank 1) and perk4= / perk8= / perk12= ... "Name|fx|Description"; fx = key:value;key:value
    // (spell:id teaches a spell). Character::fx adds them up for the ranks a character has in the class.
    struct Perk { int rank = 1; std::string name, desc, spell; std::map<std::string, int> fx; };
    std::vector<Perk> perks;
    // 1.5.0 (#93): weapons= the weapon families it is trained in (empty = all), wears= the heaviest armour it wears well
    std::vector<std::string> weapons; int wears = 2;
    bool trained(const std::string& family) const { if (family.empty() || weapons.empty()) return true; for (auto& w : weapons) if (w == family) return true; return false; }
};
const char* weightName(int w);   // 1.5.0 (#93): light / medium / heavy
int weightFromName(const std::string& s);

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
