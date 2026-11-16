#include "data.h"
#include "tiles.h"
#include <algorithm>

static const char* kDmgNames[] = {"physical", "fire", "cold", "shock", "poison", "magic"};
const char* dmgTypeName(int t) { return (t >= 0 && t < DT_COUNT) ? kDmgNames[t] : "?"; }
int dmgTypeFromName(const std::string& s) { for (int i = 0; i < DT_COUNT; i++) if (s == kDmgNames[i]) return i; return DT_PHYS; }

static const char* kStatNames[] = {"STR", "DEX", "CON", "INT", "WIS", "CHA"};
const char* statName(int s) { return (s >= 0 && s < ST_COUNT) ? kStatNames[s] : "?"; }
int statFromName(const std::string& s) { std::string u = toUpper(s); for (int i = 0; i < ST_COUNT; i++) if (u == kStatNames[i]) return i; return ST_STR; }
bool parseStatCheck(const std::string& body, int& stat, std::string& op, int& value) {
    std::string b = trim(body); size_t i = 0; while (i < b.size() && isalpha((unsigned char)b[i])) i++;
    std::string name = toUpper(b.substr(0, i)); stat = -1; for (int s = 0; s < ST_COUNT; s++) if (name == kStatNames[s]) stat = s;
    if (stat < 0) return false;
    size_t j = i; while (j < b.size() && (b[j] == '>' || b[j] == '<' || b[j] == '=')) j++;
    op = b.substr(i, j - i); if (op == "==") op = "=";
    if (op != ">=" && op != ">" && op != "<=" && op != "<" && op != "=") return false;
    std::string num = trim(b.substr(j)); if (num.empty()) return false;
    for (size_t k = 0; k < num.size(); k++) if (!isdigit((unsigned char)num[k]) && !(k == 0 && num[k] == '-')) return false;
    value = atoi(num.c_str()); return true;
}
bool statCheckPasses(int have, const std::string& op, int value) {
    return op == ">=" ? have >= value : op == ">" ? have > value : op == "<=" ? have <= value : op == "<" ? have < value : have == value;
}
std::string dialogueCheckTag(const std::vector<std::string>& conds) {
    std::string tag;
    for (auto& cond : conds) for (auto& part : split(cond, '&')) {
        std::string c = trim(part); if (c.empty() || c[0] == '!') continue;
        bool party = startsWith(c, "partystat:");
        if (party || startsWith(c, "stat:")) {
            int st, v; std::string op; if (!parseStatCheck(c.substr(party ? 10 : 5), st, op, v)) continue;
            std::string n = op == ">=" ? std::to_string(v) : op == ">" ? std::to_string(v + 1) : op == "<=" ? std::to_string(v) + " or less" : op == "<" ? std::to_string(v - 1) + " or less" : "= " + std::to_string(v);
            tag += std::string("[") + (party ? "Party " : "") + kStatNames[st] + " " + n + "] ";
        } else if (c == "stealthy") tag += "[Sneaking] ";
    }
    return tag;
}

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
Tile parseTile(const std::string& s0) {
    std::string s = s0; int sheet = 0;
    size_t at = s.find('@'); if (at != std::string::npos) { sheet = sheetIndex(s.substr(at + 1)); s = s.substr(0, at); }
    auto p = split(s, ',');
    if (p.size() < 2) return Tile{};
    Tile t = T(atoi(trim(p[0]).c_str()), atoi(trim(p[1]).c_str())); t.s = (uint8_t)sheet; return t;
}
std::string tileText(Tile t) { if (!t.valid()) return ""; std::string o = fmt("%d,%d", t.c, t.r); if (t.s) o += "@" + sheetName(t.s); return o; }
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

const std::vector<std::string>& weaponTraits() { static const std::vector<std::string> t = {"reach", "cleave", "backstab", "crush", "arc", "steady", "quick", "spread"}; return t; }
int traitIndex(const std::string& tr) { auto& t = weaponTraits(); for (size_t i = 0; i < t.size(); i++) if (t[i] == tr) return (int)i; return -1; }
void parseResList(const std::string& s, int* res, std::map<std::string, int>* traitRes) {
    for (auto& part : split(s, ',')) {
        auto kv = split(trim(part), ':'); if (kv.size() != 2) continue;
        std::string k = trim(kv[0]); int v = atoi(trim(kv[1]).c_str());
        if (traitIndex(k) >= 0) { if (traitRes) (*traitRes)[k] = v; continue; }
        for (int i = 0; i < DT_COUNT; i++) if (k == kDmgNames[i]) res[i] = v;   // unknown words are ignored (they used to count as physical)
    }
}

static ItemType itemTypeFrom(const std::string& s) {
    static const char* n[] = {"weapon", "armor", "helm", "boots", "accessory", "offhand", "consumable", "key", "material", "ammo"};
    for (int i = 0; i < IT_TYPE_COUNT; i++) if (s == n[i]) return (ItemType)i;
    return IT_MATERIAL;
}

const char* weightName(int w) { return w >= 2 ? "heavy" : w == 1 ? "medium" : "light"; }
int weightFromName(const std::string& s0) { std::string s = trim(s0); return s == "heavy" || s == "2" ? 2 : s == "medium" || s == "1" ? 1 : 0; }

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
        if (!trim(sec.get("color")).empty()) { d.recolor = true; d.color = parseColor(sec.get("color")); }
        d.trait = trim(sec.get("trait")); parseFx(sec.get("fx"), d.fx);   // 1.4.0 (#67/#68)
        d.family = trim(sec.get("family")); d.weight = weightFromName(sec.get("weight")); parseStatList(sec.get("req"), d.req, ST_COUNT, statFromName);   // 1.5.0 (#93)
        for (auto& s : split(sec.get("wornby"), ',')) if (!trim(s).empty()) d.wornby.push_back(trim(s));
        items[d.id] = d;
    }
    // ---- spells ----
    text.clear();
    if (readFile(dir + "/spells.txt", text)) {
        for (auto& sec : parseConfig(text)) {
            if (sec.name != "spell") continue;
            SpellDef d;
            for (auto& kv : sec.kv) applySpellKey(d, kv.first, kv.second);
            if (d.name.empty()) d.name = d.id;
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
        parseResList(sec.get("res"), d.res, &d.traitRes);   // 1.5.0 (#92): trait keys too
        parseRange(sec.get("dmg", "1-3"), d.dmgMin, d.dmgMax); d.dmgType = dmgTypeFromName(sec.get("dmgtype", "physical"));
        d.range = sec.geti("range", 1); d.ap = sec.geti("ap", 4); d.apMax = sec.geti("apmax", 6); d.move = sec.geti("move", 4);
        d.accuracy = sec.geti("accuracy", 0);
        d.ai = sec.get("ai", "melee");
        for (auto& s : split(sec.get("spells"), ',')) if (!trim(s).empty()) d.spells.push_back(trim(s));
        d.xp = sec.geti("xp", 10); d.level = sec.geti("level", 1); d.lootGold = sec.get("gold", "0-0");
        for (auto& l : split(sec.get("loot"), ',')) { auto kv = split(trim(l), ':'); if (kv.size() == 2) d.loot.push_back({trim(kv[0]), atoi(kv[1].c_str())}); }
        d.onHitEffect = sec.get("onhit"); d.boss = sec.geti("boss", 0) != 0; d.undead = sec.geti("undead", 0) != 0; d.mechanical = sec.geti("mechanical", 0) != 0;
        d.darkvision = sec.geti("darkvision", (d.undead || d.mechanical) ? 1 : 0) != 0;
        d.faction = sec.get("faction", "monster"); d.sound = sec.get("sound"); d.attackSound = sec.get("attacksound"); d.music = trim(sec.get("music"));
        d.role = trim(sec.get("role")); d.windup = trim(sec.get("windup")); d.ward = trim(sec.get("ward")); d.snuff = sec.geti("snuff", 0) != 0; parseFx(sec.get("fx"), d.fx);   // 1.4.0
        if (d.role == "guard") d.fx["guard"] = 1;
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
            for (auto& s : split(sec.get("weapons"), ',')) if (!trim(s).empty()) d.weapons.push_back(trim(s));   // 1.5.0 (#93)
            d.wears = sec.has("wears") ? weightFromName(sec.get("wears")) : 2;
            // 1.4.0 (#71): passive= (rank 1) and perkN= "Name|fx|Description"
            for (auto& kv : sec.kv) {
                int rank = kv.first == "passive" ? 1 : startsWith(kv.first, "perk") ? atoi(kv.first.c_str() + 4) : 0;
                if (rank <= 0 || trim(kv.second).empty()) continue;
                auto p = split(kv.second, '|'); ClassDef::Perk pk; pk.rank = rank; pk.name = trim(p[0]);
                if (p.size() > 1) for (auto& part : split(p[1], ';')) { auto q = split(trim(part), ':'); if (q.size() == 2 && trim(q[0]) == "spell") pk.spell = trim(q[1]); else if (!trim(part).empty()) parseFx(part, pk.fx); }
                if (p.size() > 2) pk.desc = trim(p[2]);
                d.perks.push_back(pk);
            }
            std::sort(d.perks.begin(), d.perks.end(), [](const ClassDef::Perk& a, const ClassDef::Perk& b) { return a.rank < b.rank; });
            for (auto& pk : d.perks) if (!pk.spell.empty()) d.spellLevels.push_back({pk.rank, pk.spell});   // a perk's spell:id is learned at that rank
            if (!classes.count(d.id)) classOrder.push_back(d.id);
            classes[d.id] = d;
        }
    }
    // 1.4.0 (#66): which class teaches each spell (its upgrade= ranks count in that class)
    for (auto& sp : spells) sp.second.cls.clear();
    // starting spells first (Heal belongs to the Cleric, who starts with it), then learn= lists
    for (auto& cid : classOrder) { auto it = classes.find(cid); if (it == classes.end()) continue;
        for (auto& s0 : it->second.startSpells) { auto f = spells.find(s0); if (f != spells.end() && f->second.cls.empty()) f->second.cls = cid; } }
    for (auto& cid : classOrder) { auto it = classes.find(cid); if (it == classes.end()) continue;
        for (auto& l : it->second.spellLevels) { auto f = spells.find(l.second); if (f != spells.end() && f->second.cls.empty()) f->second.cls = cid; } }
    return true;
}

// ---- 1.4.0 helpers ----
bool applySpellKey(SpellDef& d, const std::string& k0, const std::string& v0) {
    std::string k = trim(k0), v = trim(v0);
    if (k == "id") d.id = v; else if (k == "name") d.name = v; else if (k == "desc") d.desc = v;
    else if (k == "tile") d.tile = parseTile(v.empty() ? "82,33" : v);
    else if (k == "cost") d.cost = atoi(v.c_str()); else if (k == "ap") d.ap = atoi(v.c_str()); else if (k == "range") d.range = atoi(v.c_str());
    else if (k == "area") d.area = atoi(v.c_str()); else if (k == "level") d.level = atoi(v.c_str());
    else if (k == "dmg") parseRange(v, d.dmgMin, d.dmgMax); else if (k == "dmgtype") d.dmgType = dmgTypeFromName(v);
    else if (k == "stat") d.scaleStat = statFromName(v); else if (k == "heal") d.heal = atoi(v.c_str());
    else if (k == "effect") d.effect = v; else if (k == "duration") d.duration = atoi(v.c_str()); else if (k == "target") d.target = v.empty() ? "enemy" : v;
    else if (k == "summon") { d.summon = v; if (d.summon == "0" || d.summon == "1") d.summon.clear(); }
    else if (k == "utility") d.utility = v; else if (k == "power") d.utilityPower = atoi(v.c_str()); else if (k == "sound") d.sound = v;
    else if (k == "ff") d.friendlyFire = atoi(v.c_str()) != 0;
    else if (k == "shape") d.shape = v == "ball" ? "" : v;
    else if (k == "chain") { auto p = split(v, ':'); d.chainJumps = p.size() > 0 ? atoi(p[0].c_str()) : 0; if (p.size() > 1) d.chainRange = atoi(p[1].c_str()); if (p.size() > 2) d.chainFalloff = atoi(p[2].c_str()); }
    else if (k == "knockback") d.knockback = atoi(v.c_str()); else if (k == "smoke") d.smoke = atoi(v.c_str()); else if (k == "ground") d.ground = v;
    else if (k == "upgrade") {   // RANK:key=value,key=value:text
        size_t c1 = v.find(':'); if (c1 == std::string::npos) return true;
        SpellDef::Upgrade u; u.rank = atoi(v.substr(0, c1).c_str()); std::string rest = v.substr(c1 + 1);
        size_t c2 = rest.find(':'); std::string kvs = c2 == std::string::npos ? rest : rest.substr(0, c2); if (c2 != std::string::npos) u.text = trim(rest.substr(c2 + 1));
        for (auto& part : split(kvs, ',')) { size_t eq = part.find('='); if (eq != std::string::npos) u.kv.push_back({trim(part.substr(0, eq)), trim(part.substr(eq + 1))}); }
        if (u.rank > 0) d.upgrades.push_back(u);
    }
    else return false;
    return true;
}
void parseFx(const std::string& s, std::map<std::string, int>& out) {
    for (auto& part : split(s, s.find(';') != std::string::npos ? ';' : ',')) {
        std::string t = trim(part); if (t.empty()) continue;
        size_t c = t.find(':'); std::string k = trim(c == std::string::npos ? t : t.substr(0, c)); int v = c == std::string::npos ? 1 : atoi(t.c_str() + c + 1);
        if (!k.empty()) out[k] += v;
    }
}
std::string fxText(const std::map<std::string, int>& fx) { std::string o; for (auto& kv : fx) o += (o.empty() ? "" : ",") + kv.first + ":" + std::to_string(kv.second); return o; }
std::string fxDescribe(const std::string& k, int v) {
    if (k == "ignorehalfcover") return "Ignores half cover";
    if (k == "flankcrit") return fmt("+%d%% crit against flanked or exposed targets", v);
    if (k == "noowpenalty") return "No -10% on overwatch shots";
    if (k == "vsfrozen") return fmt("+%d%% damage against frozen or chilled targets", v);
    if (k == "critburn") return "Critical hits set the target burning";
    if (k == "covermove") return fmt("+%d move when the turn starts in cover", v);
    if (k == "aooslow") return "Attacks of opportunity also slow the target";
    if (k == "aoostop") return "Attacks of opportunity stop the target where it stands";
    if (k == "shieldcover") return "With a shield: allies next to them count as in half cover";
    if (k == "guard") return "Allies next to it count as in half cover";
    if (k == "combopower") return fmt("Status combos hit %d%% harder", v);
    if (k == "primerturns") return fmt("Chill, marks and tags last %d turn%s longer", v, v == 1 ? "" : "s");
    if (k == "vsundead") return fmt("+%d%% damage against the undead", v);
    if (k == "lightaura") return fmt("Lights %d tiles around them: no darkness penalty there", v);
    if (k == "killap") return fmt("A kill gives back %d AP", v);
    if (k == "laststand") return "Once a fight, a killing blow leaves them at 1 HP";
    if (k == "move") return fmt("+%d move in fights", v);
    if (k == "markdmg") return fmt("+%d%% damage against marked targets", v);
    if (k == "spelldmg") return fmt("+%d%% spell damage", v);
    if (k == "chillfreeze") return "Cold spells freeze a chilled target outright";
    if (k == "mpcost") return fmt("Spells cost %d%% less MP", v);
    if (k == "itemap") return fmt("Items cost %d less AP in fights", v);
    if (k == "machinecrit") return fmt("+%d%% crit against machines", v);
    if (k == "healpower") return fmt("Heals restore %d%% more", v);
    if (k == "healcure") return "Their heals also cure poison, burning and weakness";
    if (k == "smoketurns") return fmt("Their smoke lasts %d more round%s", v, v == 1 ? "" : "s");
    if (k == "hit") return fmt("%+d%% to hit", v);
    if (k == "exposedhit") return fmt("+%d%% to hit exposed targets", v);
    return fmt("%s %d", k.c_str(), v);
}
std::string traitDescribe(const std::string& t) {
    if (t == "reach") return "Reach: hits enemies 2 tiles away in a straight line";
    if (t == "cleave") return "Cleave: also hits foes next to both you and the target (half damage)";
    if (t == "backstab") return "Backstab: +25% crit against flanked or exposed targets";
    if (t == "crush") return "Crush: armour soaks at most 40% of a blow (others: 65%)";
    if (t == "arc") return "Arc: shoots over half cover, for 25% less damage";
    if (t == "steady") return "Steady: loses no accuracy at long range";
    if (t == "quick") return "Quick: attacks cost 1 less AP";
    if (t == "spread") return "Spread: also hits others in a short cone past the target (half damage)";
    return t;
}
