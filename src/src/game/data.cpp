#include "data.h"
#include "tiles.h"

static const char* kDmgNames[] = {"physical", "fire", "cold", "shock", "poison", "magic"};
const char* dmgTypeName(int t) { return (t >= 0 && t < DT_COUNT) ? kDmgNames[t] : "?"; }
int dmgTypeFromName(const std::string& s) { for (int i = 0; i < DT_COUNT; i++) if (s == kDmgNames[i]) return i; return DT_PHYS; }

static const char* kStatNames[] = {"STR", "DEX", "CON", "INT", "WIS", "CHA"};
const char* statName(int s) { return (s >= 0 && s < ST_COUNT) ? kStatNames[s] : "?"; }
int statFromName(const std::string& s) { std::string u = toUpper(s); for (int i = 0; i < ST_COUNT; i++) if (u == kStatNames[i]) return i; return ST_STR; }

static const char* kSlotNames[] = {"Weapon", "Off-hand", "Head", "Body", "Feet", "Accessory"};
const char* slotName(int s) { return (s >= 0 && s < SL_COUNT) ? kSlotNames[s] : "?"; }

std::string itemCategory(const ItemDef& d) {
    if (d.quest) return "Quest";
    if (!d.category.empty()) return d.category;
    switch (d.type) {
        case IT_WEAPON: return "Weapons";
        case IT_ARMOR: case IT_HELM: case IT_BOOTS: case IT_OFFHAND: return "Armor";
        case IT_ACCESSORY: return "Accessories";
        case IT_CONSUMABLE: return "Consumables";
        case IT_AMMO: return "Ammo";
        case IT_KEY: return "Quest";
        default: return "Materials";
    }
}
int itemCategoryOrder(const std::string& c) {
    static const char* order[] = {"Weapons", "Armor", "Accessories", "Consumables", "Ammo", "Tools", "Materials", "Formulas", "Trophies", "Quest"};
    for (int i = 0; i < 10; i++) if (c == order[i]) return i;
    return 50;
}
Tile parseTile(const std::string& s) {
    auto p = split(s, ',');
    if (p.size() < 2) return Tile{};
    return T(atoi(trim(p[0]).c_str()), atoi(trim(p[1]).c_str()));
}
Color8 parseColor(const std::string& s) {
    auto p = split(s, ',');
    if (p.size() < 3) return C::White;
    return RGB((uint8_t)atoi(p[0].c_str()), (uint8_t)atoi(p[1].c_str()), (uint8_t)atoi(p[2].c_str()));
}
void parseRange(const std::string& s, int& lo, int& hi) {
    auto p = split(s, '-');
    if (p.size() == 1) { auto q = split(s, ','); if (q.size() == 2) { lo = atoi(q[0].c_str()); hi = atoi(q[1].c_str()); return; } lo = hi = atoi(s.c_str()); return; }
    lo = atoi(p[0].c_str()); hi = atoi(p[1].c_str());
}

static void parseStatList(const std::string& s, int* out, int count, int (*fromName)(const std::string&)) {
    for (auto& part : split(s, ',')) {
        auto kv = split(trim(part), ':');
        if (kv.size() != 2) continue;
        int idx = fromName(trim(kv[0]));
        if (idx >= 0 && idx < count) out[idx] = atoi(trim(kv[1]).c_str());
    }
}

static ItemType itemTypeFrom(const std::string& s) {
    static const char* n[] = {"weapon", "armor", "helm", "boots", "accessory", "offhand", "consumable", "key", "material", "ammo"};
    for (int i = 0; i < IT_TYPE_COUNT; i++) if (s == n[i]) return (ItemType)i;
    return IT_MATERIAL;
}

bool GameData::load(const std::string& dir) {
    dataDir = dir; overlayDir.clear();
    items.clear(); spells.clear(); creatures.clear(); classes.clear(); classOrder.clear(); lastError.clear();
    return loadFiles(dir, true);
}
// A campaign folder adds to (or replaces by id) whatever it defines; files it doesn't have come from the base game.
bool GameData::loadOverlay(const std::string& dir) { overlayDir = dir; return loadFiles(dir, false); }
bool GameData::loadFiles(const std::string& dir, bool required) {
    std::string text;
    // ---- items ----
    if (!readFile(dir + "/items.txt", text)) { if (required) { lastError = "missing items.txt"; return false; } text.clear(); }
    for (auto& sec : parseConfig(text)) {
        if (sec.name != "item") continue;
        ItemDef d; d.id = sec.get("id"); d.name = sec.get("name", d.id); d.desc = sec.get("desc");
        d.type = itemTypeFrom(sec.get("type", "material")); d.tile = parseTile(sec.get("tile", "26,46"));
        d.value = sec.geti("value", 0);
        parseRange(sec.get("dmg", "0-0"), d.dmgMin, d.dmgMax);
        d.dmgType = dmgTypeFromName(sec.get("dmgtype", "physical"));
        d.range = sec.geti("range", 1); d.ap = sec.geti("ap", 4); d.scaleStat = statFromName(sec.get("stat", "str"));
        d.twoHanded = sec.geti("twohanded", 0) != 0; d.isGun = sec.geti("gun", 0) != 0; d.ammo = sec.get("ammo");
        d.accuracy = sec.geti("accuracy", 0); d.crit = sec.geti("crit", 0); d.sound = sec.get("sound");
        d.def = sec.geti("def", 0); d.evasion = sec.geti("evasion", 0);
        parseStatList(sec.get("res"), d.res, DT_COUNT, dmgTypeFromName);
        parseStatList(sec.get("bonus"), d.bonus, ST_COUNT, statFromName);
        d.bonusHP = sec.geti("hp", 0); d.bonusMP = sec.geti("mp", 0); d.bonusAP = sec.geti("apbonus", 0); d.bonusMove = sec.geti("move", 0);
        d.heal = sec.geti("heal", 0); d.mana = sec.geti("mana", 0); d.effect = sec.get("effect"); d.useSpell = sec.get("spell");
        d.stackMax = sec.geti("stack", 99); d.salvage = sec.get("salvage"); d.light = sec.geti("light", 0); d.tech = sec.geti("tech", d.isGun ? 1 : 0) != 0;
        d.quest = sec.geti("quest", d.type == IT_KEY ? 1 : 0) != 0; d.category = trim(sec.get("category")); d.unique = sec.geti("unique", 0) != 0;
        items[d.id] = d;
    }
    // ---- spells ----
    text.clear();
    if (readFile(dir + "/spells.txt", text)) {
        for (auto& sec : parseConfig(text)) {
            if (sec.name != "spell") continue;
            SpellDef d; d.id = sec.get("id"); d.name = sec.get("name", d.id); d.desc = sec.get("desc");
            d.tile = parseTile(sec.get("tile", "82,33"));
            d.cost = sec.geti("cost", 5); d.ap = sec.geti("ap", 4); d.range = sec.geti("range", 5); d.area = sec.geti("area", 0); d.level = sec.geti("level", 1);
            parseRange(sec.get("dmg", "0-0"), d.dmgMin, d.dmgMax); d.dmgType = dmgTypeFromName(sec.get("dmgtype", "magic"));
            d.scaleStat = statFromName(sec.get("stat", "int")); d.heal = sec.geti("heal", 0);
            d.effect = sec.get("effect"); d.duration = sec.geti("duration", 0); d.target = sec.get("target", "enemy");
            d.summon = sec.get("summon"); if (d.summon == "0" || d.summon == "1") d.summon.clear(); d.utility = sec.get("utility"); d.utilityPower = sec.geti("power", 0); d.sound = sec.get("sound");
            d.friendlyFire = sec.geti("ff", 1) != 0;
            spells[d.id] = d;
        }
    }
    // ---- creatures ----
    if (!readFile(dir + "/creatures.txt", text)) { if (required) { lastError = "missing creatures.txt"; return false; } text.clear(); }
    for (auto& sec : parseConfig(text)) {
        if (sec.name != "creature") continue;
        CreatureDef d; d.id = sec.get("id"); d.name = sec.get("name", d.id);
        if (sec.has("sprite")) d.tile = parseTile(sec.get("sprite"));
        else d.tile = T(sec.geti("spritecol", TL::CHAR_COL), sec.geti("spriterow", 0));
        d.size = sec.geti("size", 1);
        if (sec.has("color")) { d.recolor = true; d.color = parseColor(sec.get("color")); }
        d.hp = sec.geti("hp", 10); d.mp = sec.geti("mp", 0);
        parseStatList(sec.get("stats", "str:8,dex:8,con:8,int:8,wis:8,cha:8"), d.stats, ST_COUNT, statFromName);
        d.def = sec.geti("def", 0); d.evasion = sec.geti("evasion", 0);
        parseStatList(sec.get("res"), d.res, DT_COUNT, dmgTypeFromName);
        parseRange(sec.get("dmg", "1-3"), d.dmgMin, d.dmgMax); d.dmgType = dmgTypeFromName(sec.get("dmgtype", "physical"));
        d.range = sec.geti("range", 1); d.ap = sec.geti("ap", 4); d.apMax = sec.geti("apmax", 6); d.move = sec.geti("move", 4);
        d.accuracy = sec.geti("accuracy", 0);
        d.ai = sec.get("ai", "melee");
        for (auto& s : split(sec.get("spells"), ',')) if (!trim(s).empty()) d.spells.push_back(trim(s));
        d.xp = sec.geti("xp", 10); d.level = sec.geti("level", 1); d.lootGold = sec.get("gold", "0-0");
        for (auto& l : split(sec.get("loot"), ',')) { auto kv = split(trim(l), ':'); if (kv.size() == 2) d.loot.push_back({trim(kv[0]), atoi(kv[1].c_str())}); }
        d.onHitEffect = sec.get("onhit"); d.boss = sec.geti("boss", 0) != 0; d.undead = sec.geti("undead", 0) != 0; d.mechanical = sec.geti("mechanical", 0) != 0;
        d.darkvision = sec.geti("darkvision", (d.undead || d.mechanical) ? 1 : 0) != 0;
        d.faction = sec.get("faction", "monster"); d.sound = sec.get("sound"); d.attackSound = sec.get("attacksound");
        creatures[d.id] = d;
    }
    // ---- classes ----
    text.clear();
    if (readFile(dir + "/classes.txt", text)) {
        for (auto& sec : parseConfig(text)) {
            if (sec.name != "class") continue;
            ClassDef d; d.id = sec.get("id"); d.name = sec.get("name", d.id); d.desc = sec.get("desc");
            parseStatList(sec.get("stats", "str:8,dex:8,con:8,int:8,wis:8,cha:8"), d.stats, ST_COUNT, statFromName);
            for (auto& s : split(sec.get("items"), ',')) if (!trim(s).empty()) d.startItems.push_back(trim(s));
            for (auto& s : split(sec.get("spells"), ',')) if (!trim(s).empty()) d.startSpells.push_back(trim(s));
            d.startWeapon = sec.get("weapon"); d.startArmor = sec.get("armor");
            d.spriteRow = sec.geti("spriterow", 0); d.spriteCol = sec.geti("spritecol", 104);
            if (sec.has("sprite")) { Tile t = parseTile(sec.get("sprite")); if (t.valid()) { d.spriteCol = t.c; d.spriteRow = t.r; } }
            for (auto& s : split(sec.get("learn"), ',')) { auto kv = split(trim(s), ':'); if (kv.size() == 2) d.spellLevels.push_back({atoi(kv[0].c_str()), trim(kv[1])}); }
            d.hitDie = sec.geti("hd", 6); d.mpDie = sec.geti("md", 2); d.role = sec.get("role");
            if (!classes.count(d.id)) classOrder.push_back(d.id);
            classes[d.id] = d;
        }
    }
    return true;
}
