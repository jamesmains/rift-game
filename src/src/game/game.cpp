#include "game.h"
#include "../core/version.h"
#include <cstring>
#include "../core/audio.h"
#include "tiles.h"
#include <cmath>
#include <sstream>
#include <algorithm>
#include <filesystem>
#include <ctime>

using namespace UI;

// ---------------- settings ----------------
void GameSettings::save(const std::string& path) const {
    std::string s = "[settings]\n";
    s += fmt("fullscreen=%d\nscale=%d\nvsync=%d\ninteger=%d\ncrt=%d\nmaster=%d\nmusic=%d\nsfx=%d\ngamepad=%d\nshake=%d\nfastcombat=%d\ninteractmoving=%d\nedgescroll=%d\nbanter=%d\nlargetext=%d\ncolorblind=%d\ntextspeed=%d\nreduceflash=%d\nambient=%d\nfont=%d\n\n",
             fullscreen, scale, vsync, integerScaling, crt, masterVolume, musicVolume, sfxVolume, gamepad, screenShake, fastCombat, interactWhileMoving, edgeScroll, banter, largeText, colorblind, textSpeed, reduceFlashing, ambientVolume, fontStyle);
    if (!discord.empty()) s += "[links]\ndiscord=" + discord + "\n\n";
    s += gKeys.serialize();
    writeFile(path, s);
}
bool GameSettings::load(const std::string& path) {
    std::string t; if (!readFile(path, t)) return false;
    gKeys.deserialize(t);
    for (auto& sec : parseConfig(t)) {
        if ((sec.name == "settings" || sec.name == "links") && sec.has("discord")) discord = trim(sec.get("discord"));
        if (sec.name != "settings") continue;
        edgeScroll = sec.geti("edgescroll", edgeScroll); banter = clampi(sec.geti("banter", banter), 0, 3);
        fullscreen = sec.geti("fullscreen", fullscreen); scale = sec.geti("scale", scale); vsync = sec.geti("vsync", vsync);
        integerScaling = sec.geti("integer", integerScaling); crt = sec.geti("crt", crt); masterVolume = sec.geti("master", masterVolume); musicVolume = sec.geti("music", musicVolume);
        sfxVolume = sec.geti("sfx", sfxVolume); gamepad = sec.geti("gamepad", gamepad); screenShake = sec.geti("shake", screenShake); fastCombat = sec.geti("fastcombat", fastCombat);
        interactWhileMoving = sec.geti("interactmoving", interactWhileMoving);
        largeText = sec.geti("largetext", largeText) != 0; colorblind = sec.geti("colorblind", colorblind) != 0; textSpeed = clampi(sec.geti("textspeed", textSpeed), 0, 3); reduceFlashing = sec.geti("reduceflash", reduceFlashing) != 0;
        ambientVolume = clampi(sec.geti("ambient", ambientVolume), 0, 100); fontStyle = Font::STYLE_CLEAR;   // 1.1.1: the clear font is the only text face (no setting any more)
    }
    return true;
}

// base sections followed by the campaign's (so later = campaign overrides when looking up by id)
std::vector<ConfigSection> Game::mergedConfig(const std::string& rel) const {
    std::vector<ConfigSection> out; std::string t;
    bool base = campaignDir.empty() || campaign.inherit;   // a standalone campaign reads only its own folder
    if (base && readFile(assetDir + "/data/" + rel, t)) out = parseConfig(t);
    if (!campaignDir.empty() && readFile(campaignDir + "/" + rel, t)) { auto more = parseConfig(t); out.insert(out.end(), more.begin(), more.end()); }
    return out;
}
std::string Game::dataPath(const std::string& rel) const {
    if (!campaignDir.empty() && (fileExists(campaignDir + "/" + rel) || !campaign.inherit)) return campaignDir + "/" + rel;
    return assetDir + "/data/" + rel;
}

// ---------------- campaigns ----------------
CampaignInfo readCampaignInfo(const std::string& file, const std::string& id, const std::string& dir) {
    CampaignInfo c; c.id = id; c.dir = dir;
    std::string t;
    if (!readFile(file, t)) {
        if (dir.empty()) c.intro = {"The Rift opened forty years ago."};
        return c;
    }
    for (auto& sec : parseConfig(t)) {
        if (sec.name != "campaign") continue;
        c.name = sec.get("name", c.id); c.desc = sec.get("desc"); c.author = sec.get("author"); c.version = sec.get("version");
        auto ic = split(sec.get("icon", "0,7"), ','); if (ic.size() == 2) { c.iconC = atoi(ic[0].c_str()); c.iconR = atoi(ic[1].c_str()); }
        auto st = split(sec.get("start", c.startMap + ",0,0"), ','); c.startMap = trim(st[0]); if (st.size() >= 3) { c.startX = atoi(st[1].c_str()); c.startY = atoi(st[2].c_str()); }
        c.intro.clear(); for (auto& kv : sec.kv) if (kv.first == "intro") c.intro.push_back(kv.second);
        c.startDialogue = sec.get("dialogue", ""); c.gold = sec.geti("gold", 25); c.inherit = sec.geti("inherit", 0) != 0;   // campaigns stand alone unless they ask to build on the base game
        c.startItems = sec.get("items"); c.startFlags = sec.get("flags"); c.worldMap = trim(sec.get("worldmap", "overworld"));
        c.order = sec.geti("order", 100); c.achievements = sec.geti("achievements", 1) != 0;
    }
    return c;
}
std::vector<CampaignInfo> listCampaigns(const std::string& assetDir) {
    std::vector<CampaignInfo> out;
    out.push_back(readCampaignInfo(assetDir + "/data/campaign.txt", "triffall", ""));
    std::error_code ec; std::vector<std::string> ids;
    for (auto& e : std::filesystem::directory_iterator(assetDir + "/campaigns", ec)) if (e.is_directory() && fileExists(e.path().string() + "/campaign.txt")) ids.push_back(e.path().filename().string());
    std::sort(ids.begin(), ids.end());
    for (auto& id : ids) out.push_back(readCampaignInfo(assetDir + "/campaigns/" + id + "/campaign.txt", id, assetDir + "/campaigns/" + id));
    std::stable_sort(out.begin() + 1, out.end(), [](const CampaignInfo& a, const CampaignInfo& b) { return a.order < b.order; });   // base game stays first
    return out;
}
bool Game::switchCampaign(const std::string& id) {
    std::string want = id.empty() ? "triffall" : id;
    auto all = listCampaigns(assetDir);
    const CampaignInfo* c = nullptr; for (auto& x : all) if (x.id == want) c = &x;
    if (!c) { notify("Campaign '" + want + "' not found."); return false; }
    bool same = want == campaignId && !gd->items.empty() && campaign.id == want;
    campaign = *c;
    if (same) return true;
    campaignId = want; campaignDir = c->dir;
    if (campaignDir.empty() || c->inherit) gd->load(assetDir + "/data"); else { gd->load(campaignDir); }
    if (!campaignDir.empty() && c->inherit) gd->loadOverlay(campaignDir);
    loadTileKinds(assetDir + "/data/tilekinds.txt"); if (!campaignDir.empty()) loadTileKindsOverlay(campaignDir + "/tilekinds.txt");
    dialogues.clear(); quests.clear(); encounters.clear(); maps.clear(); map = nullptr; mapNameCache.clear();
    loadSystems(); loadDialogues(); loadQuests(); loadEncounters();
    return true;
}

// ---------------- init / new game ----------------
bool Game::init(GameData* data, GameSettings* s, const std::string& assets, Tileset* ts, Font* font) {
    gd = data; assetDir = assets;
    loadSystems();
    gd = data; settings = s; assetDir = assets;
    if (saveDir.empty()) saveDir = "saves";
    ctx.ts = ts; ctx.font = font; ctx.gd = data;
    loadDialogues(); loadQuests(); loadEncounters();
    campaign = readCampaignInfo(assetDir + "/data/campaign.txt", "triffall", ""); campaignId = "triffall"; campaignDir.clear();
#ifdef TRIFFALL_DEMO
    demoMode = true;
#endif
    loadDemo();
    return true;
}

void Game::newGame(const std::string& hn, const std::string& classId, int diff) {
    difficulty = clampi(diff, 0, 2);
    party = Party(); flags.clear(); visited.clear(); maps.clear(); exploredSave.clear(); map = nullptr; msgLog.clear(); msgTime.clear(); floats.clear();
    modeStack.clear(); mode = GMode::EXPLORE; trail.clear(); levelUpQueue.clear(); gameOver = false; running = true; requestDesktop = false;
    playtimeSec = 0; lastAutosave = 0; stepCounter = 0; heroName = hn; worldSeed = 1337; rng.reseed(fixedSeed ? fixedSeed : (uint64_t)time(nullptr));
    lightSteps = stealthSteps = detectSteps = 0; lastTownMap.clear(); clockMin = 8 * 60; day = 1; weather = "clear"; weatherSteps = 900; extra.clear();   // a new game starts on a calm, clear morning
    stats = GameStats(); bestiary.clear(); approval.clear(); reputation.clear(); leader = 0; notes.clear(); bounties.clear(); bountyOffers.clear(); dynItems.clear(); consoleOut.clear(); banterT = 0; banterCooldown = 60;
    const ClassDef* c = gd->cls(classId); if (!c) c = gd->cls(gd->classOrder.empty() ? "" : gd->classOrder[0]);
    Character hero; if (c) hero.initFromClass(*gd, *c, hn);
    party.members.push_back(hero);
    if (c) for (auto& it : c->startItems) { auto p = split(it, ':'); party.addItem(trim(p[0]), p.size() > 1 ? atoi(p[1].c_str()) : 1, *gd); }
    party.gold = campaign.gold;
    for (auto& it : split(campaign.startItems, ',')) { auto p = split(trim(it), ':'); if (!trim(p[0]).empty()) party.addItem(trim(p[0]), p.size() > 1 ? atoi(p[1].c_str()) : 1, *gd); }
    for (auto& f : split(campaign.startFlags, ',')) if (!trim(f).empty()) { auto p = split(trim(f), '='); setFlag(trim(p[0]), p.size() > 1 ? atoi(p[1].c_str()) : 1); }
    enterMap(campaign.startMap, campaign.startX, campaign.startY, false);
    fadeIn = 1.0f;
    std::vector<std::string> lines;
    for (auto l : campaign.intro) { size_t pos; while ((pos = l.find("$hero")) != std::string::npos) l.replace(pos, 5, hn); lines.push_back(l); }
    std::string dlg = campaign.startDialogue;
    if (!lines.empty()) startCutscene(lines, [this, dlg]() { if (!dlg.empty() && dialogues.count(dlg)) startDialogue(dlg, nullptr); });
    else if (!dlg.empty() && dialogues.count(dlg)) startDialogue(dlg, nullptr);
}

// ---------------- maps ----------------
Map* Game::loadMap(const std::string& id) {
    auto it = maps.find(id);
    if (it != maps.end()) return it->second.get();
    auto m = std::make_unique<Map>();
    Rng mr((uint64_t)worldSeed * 977 + std::hash<std::string>()(id));
    if (id == "overworld") { buildOverworld(*m, worldSeed, dataPath("maps/overworld.txt"), dataPath("overworld.txt")); }
    else {
        std::string err;
        bool ok = false;
        // dungeon definitions (a hand-made or baked map file with the same id takes priority)
        std::string dtext;
        if (!fileExists(dataPath("maps/" + id + ".txt"))) {
            auto secs = mergedConfig("dungeons.txt"); std::reverse(secs.begin(), secs.end());   // a campaign's definition wins
            for (auto& sec : secs) {
                if (sec.name != "dungeon" || sec.get("id") != id) continue;
                std::vector<std::string> pool; for (auto& p : split(sec.get("monsters"), ',')) if (!trim(p).empty()) pool.push_back(trim(p));
                auto ex = split(sec.get("exit"), ',');
                generateDungeon(*m, id, sec.get("style", "cave"), sec.geti("w", 48), sec.geti("h", 40), worldSeed + sec.geti("seed", 1), sec.geti("level", 1),
                                ex.size() > 0 ? trim(ex[0]) : "overworld", ex.size() > 2 ? atoi(ex[1].c_str()) : 0, ex.size() > 2 ? atoi(ex[2].c_str()) : 0,
                                pool, sec.get("boss"), sec.get("bossflag"), sec.get("loot", "common"));
                m->name = sec.get("name", id); m->encounterTable = sec.get("table");
                std::string st = sec.get("style", "cave"); m->music = sec.get("music", st == "tech" || st == "ship" ? "tech" : "dungeon"); m->ambient = sec.get("ambient", "");
                if (sec.has("bossdefeat")) for (auto& e : m->enemies) if (e.flag == sec.get("bossflag")) e.onDefeatScript = sec.get("bossdefeat");
                ok = true; break;
            }
        }
        if (!ok) {
            if (!loadMapFile(dataPath("maps/" + id + ".txt"), *m, mr, err)) {
                log("Map error: " + err);
                m->init(20, 15, "floor_stone", mr); m->id = id; m->name = "Nowhere"; m->rect(0, 0, 20, 15, "wall_stone", mr);
                Exit e; e.x = 10; e.y = 13; e.to = "overworld"; e.tx = 0; e.ty = 0; m->exits.push_back(e); m->spawnX = 10; m->spawnY = 7;
            }
        }
    }
    m->id = id;
    applyMapFlags(*m);
    { auto ex = exploredSave.find(id); if (ex != exploredSave.end()) decodeExplored(*m, ex->second); }
    Map* raw = m.get();
    maps[id] = std::move(m);
    return raw;
}

// explored tiles are saved per map as "w,h:" + run lengths of alternating unexplored/explored cells
std::string Game::encodeExplored(const Map& m) const {
    std::string o = std::to_string(m.w) + "," + std::to_string(m.h) + ":";
    bool cur = false; int run = 0; bool first = true;
    for (auto& c : m.cells) { if (c.explored == cur) { run++; continue; } o += (first ? "" : ".") + std::to_string(run); first = false; cur = !cur; run = 1; }
    o += (first ? "" : ".") + std::to_string(run);
    return o;
}
void Game::decodeExplored(Map& m, const std::string& s) const {
    size_t colon = s.find(':'); if (colon == std::string::npos) return;
    auto wh = split(s.substr(0, colon), ','); if (wh.size() != 2 || atoi(wh[0].c_str()) != m.w || atoi(wh[1].c_str()) != m.h) return;   // the map was resized: start fresh
    size_t i = 0; bool cur = false;
    for (auto& r : split(s.substr(colon + 1), '.')) { int n = atoi(r.c_str()); for (int k = 0; k < n && i < m.cells.size(); k++) m.cells[i++].explored = cur; cur = !cur; }
}
void Game::stashExplored() { for (auto& kv : maps) if (kv.second) exploredSave[kv.first] = encodeExplored(*kv.second); }

void Game::applyMapFlags(Map& m) {
    for (auto& c : m.chests) if (!c.flag.empty() && hasFlag(c.flag)) { c.opened = true; c.items.clear(); c.gold = 0; }
    for (auto& e : m.enemies) if (!e.flag.empty() && hasFlag(e.flag)) e.defeated = true;
    for (auto& d : m.doors) if (!d.flag.empty() && hasFlag(d.flag)) { d.open = true; m.at(d.x, d.y).walkable = true; m.at(d.x, d.y).blocksSight = false; m.at(d.x, d.y).prop = d.openTile; }
    applyWorldState(m);
}

void Game::enterMap(const std::string& id, int x, int y, bool fade) {
    bool newPlace = id != mapId;
    Map* m = loadMap(id);
    map = m; mapId = id;
    if (x == 0 && y == 0) { x = m->spawnX; y = m->spawnY; }
    if (!m->walkable(x, y)) { int ox, oy; if (m->findFree(x, y, 6, ox, oy, rng)) { x = ox; y = oy; } }
    px = x; py = y;
    trail.clear(); for (int i = 0; i < 8; i++) trail.push_back({px, py});
    visited.insert(id);
    // overworld locations with reveal=N uncover the overworld around them when their map is entered, so a campaign
    // played mostly indoors still fills in its world map
    for (auto& l : locations) if (l.mapId == id && l.reveal > 0 && id != "overworld") revealMap("overworld", l.x, l.y, l.reveal);
    if (m->kind == "town") { lastTownMap = id; lastTownX = px; lastTownY = py; }
    if (outdoors() && weatherSteps <= 0) rerollWeather();
    banterCooldown = std::min(banterCooldown, 12); if (!visited.count(id)) {} banterTrigger("enter");
    visMask.assign((size_t)m->w * m->h, 0);
    revealAround();
    camFree = false; updateCamera(); camFX = (float)camX; camFY = (float)camY;
    if (fade) fadeIn = 0.6f;
    if (!m->name.empty()) notify(m->name);
    if (newPlace && running) autosave();
}

int Game::activeLeader() const {
    if (party.members.empty()) return 0;
    int ld = clampi(leader, 0, (int)party.members.size() - 1);
    if (party.members[ld].alive) return ld;
    for (size_t i = 0; i < party.members.size(); i++) if (party.members[i].alive) return (int)i;
    return ld;
}
std::vector<int> Game::followOrder() const {
    std::vector<int> order; int ld = activeLeader();
    for (int i = 0; i < (int)party.members.size(); i++) if (i != ld) order.push_back(i);
    return order;
}
std::string Game::musicWanted() const {
    if (!running && gameOver) return "gameover";
    if (!map) return "";
    GMode m = mode; for (size_t i = modeStack.size(); i-- > 0 && (m == GMode::MENU || m == GMode::PAUSE || m == GMode::SAVELOAD || m == GMode::MESSAGE || m == GMode::LEVELUP || m == GMode::SHOP || m == GMode::DIALOGUE); ) m = modeStack[i];
    if (m == GMode::GAMEOVER) return "gameover";
    if (m == GMode::VICTORY) return "fanfare";
    if (m == GMode::ENDING) return "ending";
    if (map->kind == "overworld" && map->music.empty() && nightAmount() > 0.6f && gAudio.has("overworld_night")) return "overworld_night";
    if (m == GMode::COMBAT) { for (auto& u : cb.units) if (u.ch.alive && !u.friendly() && u.ch.boss) return "boss"; return "combat"; }
    if (!map->music.empty()) return map->music;
    if (map->kind == "overworld") return "overworld";
    if (map->kind == "town" || map->kind == "interior") return "town";
    if (map->kind == "dungeon") return "dungeon";
    return "forest";
}
// 1.1.5 (owner's #28): no ambient loop is picked by place, weather or time any more (waves, birds, drips often didn't
// fit the place - crickets in a crypt). Only a map that asks for one plays it: header ambient=<id> (a Maps tab picker),
// or ambient= on a dungeons.txt entry for generated dungeons. "none" = silence, as before.
std::string Game::ambientWanted() const {
    if (!running || !map || mode == GMode::ENDING || mode == GMode::GAMEOVER) return "";
    return map->ambient == "none" ? "" : map->ambient;
}
std::string Game::weaponSound(const Character& ch) const {
    if (!ch.creatureId.empty()) { if (auto* c = gd->creature(ch.creatureId)) if (!c->attackSound.empty()) return c->attackSound; }
    const ItemDef* w = ch.weapon(*gd);
    if (w && !w->sound.empty()) return w->sound;
    if (w && w->isGun) return (w->dmgType == DT_SHOCK || w->dmgType == DT_FIRE || w->dmgType == DT_MAGIC) ? "attack_laser" : "attack_gun";
    if (ch.attackRange(*gd) > 1) return "attack_bow";
    return "attack_melee";
}
std::string Game::spellSound(const SpellDef& s) const {
    if (!s.sound.empty()) return s.sound;
    if (s.heal > 0 || s.effect == "revive" || s.effect == "cure") return "spell_heal";
    if (s.dmgMax > 0) switch (s.dmgType) { case DT_FIRE: return "spell_fire"; case DT_COLD: return "spell_ice"; case DT_SHOCK: return "spell_lightning"; case DT_POISON: return "spell_poison"; default: return "spell_cast"; }
    if (!s.effect.empty()) return "spell_buff";
    return "spell_cast";
}

void Game::loadDemo() {
    std::string t; if (!readFile(assetDir + "/data/demo.txt", t)) return;
    for (auto& sec : parseConfig(t)) if (sec.name == "demo") {
        demoMaps.clear(); for (auto& m : split(sec.get("maps"), ',')) if (!trim(m).empty()) demoMaps.insert(trim(m));
        demoEnd = sec.get("end"); demoBlockedText = sec.get("blocked"); demoTitle = sec.get("title"); demoText = sec.get("text"); storeUrl = sec.get("store");
    }
}
bool Game::demoBlocks(const std::string& m) const { return demoMode && campaignId == "triffall" && !demoMaps.empty() && !demoMaps.count(m); }
void Game::startDemoEnd() {
    extra["demo_end_shown"] = "1";
    startEnding();   // the usual ending screens, with the demo's own first slide and no epilogue
    endSlides.clear(); EpilogueSlide sl; sl.title = demoTitle.empty() ? "Thank you for playing" : demoTitle; sl.text = endingText(demoText); sl.tile = T(0, 0); sl.hasTile = false; endSlides.push_back(sl);
}
void Game::startTransition(const std::string& to, int x, int y, const std::string& text) {
    if (demoBlocks(to)) { if (mode == GMode::TRANSITION) popMode(); showMessage("Triffall Demo", demoBlockedText.empty() ? "That way is closed in the demo." : demoBlockedText); gAudio.play("door_locked"); return; }
    transitionTo = to; transX = x; transY = y; transition = 0.01f; transText = text; pushMode(GMode::TRANSITION); gAudio.play("teleport"); advanceClock(10);
}

// ---------------- messages ----------------
void Game::log(const std::string& s) { msgLog.push_back(s); msgTime.push_back(6.0f); if (msgLog.size() > 60) { msgLog.erase(msgLog.begin()); msgTime.erase(msgTime.begin()); } }
void Game::notify(const std::string& s) { notifications.push_back(s); notifTime.push_back(2.5f); }
void Game::showMessage(const std::string& title, const std::string& text, std::function<void()> done) {
    pendingMessageTitle = title; pendingMessage = text; messageDone = done; pushMode(GMode::MESSAGE);
}
void Game::startCutscene(const std::vector<std::string>& lines, std::function<void()> done) {
    cutLines = lines; cutIndex = 0; cutTimer = 0; cutDone = done; pushMode(GMode::CUTSCENE);
}

void Game::giveXP(int xp) {
    int alive = party.aliveCount(); if (alive == 0) return;
    // 1.0 balance: every member standing gets the whole award (it used to be split between them, which left a full
    // party around level 7 at the end of a game whose last dungeon is level 11 - see tools/pacing.sh)
    int each = std::max(1, xp);
    for (auto& m : party.members) if (m.alive) m.xp += each;
    log(fmt("Party gains %d XP each.", each));
    checkLevelUps();
}
void Game::checkLevelUps() {
    for (size_t i = 0; i < party.members.size(); i++) {
        auto& m = party.members[i];
        while (m.xp >= m.xpToNext() && m.level < 30) {
            m.xp -= m.xpToNext(); m.level++; m.statPoints += 3; m.rankPoints += 1;   // the class rank (and its spells) is chosen on the level-up screen
            m.hp = std::min(m.maxHP(*gd), m.hp + 10); m.mp = m.maxMP(*gd);
            log(m.name + " reached level " + std::to_string(m.level) + "!"); gAudio.play("levelup");
            if (std::find(levelUpQueue.begin(), levelUpQueue.end(), (int)i) == levelUpQueue.end()) levelUpQueue.push_back((int)i);
        }
    }
}
void Game::giveItem(const std::string& id, int n, bool silent) {
    const ItemDef* d = gd->item(id); if (!d) return;
    party.addItem(id, n, *gd);
    if (!silent) { log("Got " + itemLabel(*d, n) + "."); gAudio.play("pickup"); }
    for (auto& r : recipes) if (r.reveal == id && !extra.count("seen_" + id)) { extra["seen_" + id] = "1"; notify("New recipe idea: " + r.name + " (" + r.station + ")"); }
    bountyEvent("item", id, n);
}

bool Game::canSaveHere() const { return running && map && mode == GMode::EXPLORE && map->saveAt(px, py) != nullptr; }

// ---------------- visibility ----------------
bool Game::visible(int x, int y) const {
    if (!map) return false;
    if (!map->dark) return true;
    if (!map->inBounds(x, y)) return false;
    if (visMask.size() != (size_t)map->w * map->h) return false;
    return visMask[(size_t)y * map->w + x] != 0;
}
int Game::tileMoveCost(int x, int y) const { if (!map || !map->inBounds(x, y)) return 1; const auto& ks = allTileKinds(); unsigned k = map->at(x, y).kind; return k < ks.size() ? std::max(1, ks[k].moveCost) : 1; }
bool Game::inCombat() const { if (mode == GMode::COMBAT) return true; for (auto m : modeStack) if (m == GMode::COMBAT) return true; return false; }
// Each party member glows a little on dark maps (map light= is the bare lantern); a torch or the Light spell widens it.
int Game::partyGlow() const {
    int g = map ? map->lightRadius : 3; if (lightSteps > 0) g = std::max(g, lightPower > 0 ? lightPower : 5);
    for (auto& m : party.members) if (m.alive) for (int s = 0; s < SL_COUNT; s++) if (!m.equip[s].empty()) if (auto* it = gd->item(m.equip[s])) g = std::max(g, it->light);   // a lantern never burns out
    return g;
}
bool Game::lit(int x, int y) const {
    if (!map || !map->dark) return true;
    if (!map->inBounds(x, y) || litMask.size() != (size_t)map->w * map->h) return false;
    return litMask[(size_t)y * map->w + x] != 0;
}
// Light on dark maps: party glow, light-giving tiles (torches, braziers, crystals, lava...), temporary combat lights and burning units.
void Game::computeLight() {
    if (!map || !map->dark) { litMask.clear(); return; }
    litMask.assign((size_t)map->w * map->h, 0);
    auto spread = [&](int cx, int cy, int r) {
        if (r <= 0) return;
        for (int y = cy - r; y <= cy + r; y++) for (int x = cx - r; x <= cx + r; x++) {
            if (!map->inBounds(x, y) || litMask[(size_t)y * map->w + x]) continue;
            if ((x - cx) * (x - cx) + (y - cy) * (y - cy) > r * r + 1) continue;
            if (lineOfSight(*map, cx, cy, x, y)) litMask[(size_t)y * map->w + x] = 1;
        }
    };
    const auto& kinds = allTileKinds();
    for (int y = 0; y < map->h; y++) for (int x = 0; x < map->w; x++) { const Cell& c = map->at(x, y); if (c.kind < kinds.size() && kinds[c.kind].light > 0) spread(x, y, kinds[c.kind].light); }
    int g = partyGlow();
    if (inCombat()) {
        for (auto& u : cb.units) if (u.ch.alive) { if (u.partyIndex >= 0) spread(u.x, u.y, g); if (u.ch.hasEffect("burn")) spread(u.x, u.y, 2); }
        for (auto& l : cb.lights) spread(l.x, l.y, l.r);
    } else spread(px, py, g);
}
void Game::revealAround() {
    if (!map) return;
    if (visMask.size() != (size_t)map->w * map->h) visMask.assign((size_t)map->w * map->h, 0);
    // eyes: the leader while exploring; every standing party member during a fight
    std::vector<std::pair<int, int>> eyes;
    if (inCombat()) { for (auto& u : cb.units) if (u.partyIndex >= 0 && u.ch.alive) eyes.push_back({u.x, u.y}); }
    if (eyes.empty()) eyes.push_back({px, py});
    if (map->dark) {
        computeLight();
        std::fill(visMask.begin(), visMask.end(), 0);
        const int R = 14;   // you can see a lit room from across the cave, but not what stands in the dark
        int dim = partyGlow() + 2;   // just past the glow you can make out the shape of the room (explored, drawn dim) but not who is in it
        for (auto& e : eyes) {
            int ex = e.first, ey = e.second;
            for (int y = ey - R; y <= ey + R; y++) for (int x = ex - R; x <= ex + R; x++) {
                if (!map->inBounds(x, y)) continue;
                size_t i = (size_t)y * map->w + x; if (visMask[i]) continue;
                int d2 = (x - ex) * (x - ex) + (y - ey) * (y - ey); if (d2 > R * R) continue;
                bool near = chebyshev(x, y, ex, ey) <= 1;
                bool shape = d2 <= dim * dim + 1 && !map->at(x, y).explored;
                if (!near && !litMask[i] && !shape) continue;
                if (lineOfSight(*map, ex, ey, x, y)) { map->at(x, y).explored = true; if (near || litMask[i]) visMask[i] = 1; }
            }
        }
    } else {
        for (auto& e : eyes) for (int y = e.second - 12; y <= e.second + 12; y++) for (int x = e.first - 16; x <= e.first + 16; x++) if (map->inBounds(x, y)) map->at(x, y).explored = true;
    }
}

// ---------------- camera ----------------
void Game::camClamp() {
    if (!map) return;
    int viewW = SCREEN_W, viewH = SCREEN_H - 72;
    int maxX = map->w * TILE - viewW, maxY = map->h * TILE - viewH;
    if (map->w * TILE <= viewW) camTX = -(viewW - map->w * TILE) / 2; else camTX = clampi(camTX, 0, maxX);
    if (map->h * TILE <= viewH) camTY = -(viewH - map->h * TILE) / 2; else camTY = clampi(camTY, 0, maxY);
}
void Game::camCenterOn(int tx, int ty, bool snap) {
    int viewW = SCREEN_W, viewH = SCREEN_H - 72;
    camTX = tx * TILE + TILE / 2 - viewW / 2; camTY = ty * TILE + TILE / 2 - viewH / 2;
    camClamp();
    if (snap) { camFX = (float)camTX; camFY = (float)camTY; camX = camTX; camY = camTY; }
}
void Game::updateCamera() {
    if (!map) return;
    if (!camFree) camCenterOn(px, py, mode != GMode::COMBAT);
}
// Player camera control: edge scrolling, right-drag panning, right stick. Used in combat (free camera) and explore.
void Game::updateCameraInput(InputState& in, float dt) {
    if (!map) return;
    int viewH = SCREEN_H - 72;
    bool combat = mode == GMode::COMBAT;
    float speed = 420.0f * dt;
    int dx = 0, dy = 0;
    if (in.mouseInWindow && !in.gamepadActive && settings && settings->edgeScroll && combat && !mouseOverUI(in) && in.mouseY < viewH) {
        if (in.mouseX < 10) dx = -1; else if (in.mouseX >= SCREEN_W - 10) dx = 1;
        if (in.mouseY < 10) dy = -1; else if (in.mouseY >= viewH - 10) dy = 1;
    }
    if (in.mouseDown[2] && (in.mouseDX || in.mouseDY)) { camTX -= in.mouseDX; camTY -= in.mouseDY; camFree = true; camClamp(); camFX = (float)camTX; camFY = (float)camTY; }
    if (in.stick2X != 0 || in.stick2Y != 0) { camTX += (int)(in.stick2X * speed); camTY += (int)(in.stick2Y * speed); camFree = true; camClamp(); camFX = (float)camTX; camFY = (float)camTY; }
    if (dx || dy) { camTX += (int)(dx * speed); camTY += (int)(dy * speed); camFree = true; camClamp(); camFX = (float)camTX; camFY = (float)camTY; }
    if (actP(in, A_CENTER, combat)) { camFree = false; if (combat && cb.selectedUnit >= 0) camCenterOn(cb.units[cb.selectedUnit].x, cb.units[cb.selectedUnit].y); else camCenterOn(px, py); }
    // smooth follow toward target
    float k = std::min(1.0f, dt * 10.0f);
    camFX += (camTX - camFX) * k; camFY += (camTY - camFY) * k;
    if (fabsf(camTX - camFX) < 1.0f) camFX = (float)camTX; if (fabsf(camTY - camFY) < 1.0f) camFY = (float)camTY;
    camX = (int)camFX; camY = (int)camFY;
}
bool Game::mouseOverUI(const InputState& in) const {
    for (auto& r : uiRectsPrev) if (r.contains(in.mouseX, in.mouseY)) return true;
    return false;
}

// ---------------- main update ----------------
void Game::update(InputState& in, float dt, Canvas& cv) {
    ctx.cv = &cv; ctx.frame++; ctx.time += dt; frameDt = dt; gPadGlyphs = in.gamepadActive; gColorblind = settings && settings->colorblind;
    if (doorMsgCd > 0) doorMsgCd--;
    if (healerReopen && mode == GMode::EXPLORE) { healerReopen = false; healerMenu(healerGreeting); }
    if (running && !gameOver) { playtimeAcc += dt; while (playtimeAcc >= 1) { playtimeAcc -= 1; playtimeSec++; } }
    for (size_t i = 0; i < msgTime.size(); i++) msgTime[i] -= dt;
    for (size_t i = 0; i < notifTime.size(); i++) notifTime[i] -= dt;
    while (!notifTime.empty() && notifTime[0] <= 0) { notifTime.erase(notifTime.begin()); notifications.erase(notifications.begin()); }
    for (auto& f : floats) { f.y += f.vy * dt; f.vy *= std::max(0.0f, 1.0f - dt * 2.5f); f.life -= dt; }   // pop up fast, then hang
    floats.erase(std::remove_if(floats.begin(), floats.end(), [](const FloatText& f) { return f.life <= 0; }), floats.end());
    if (shake > 0) shake -= dt;
    if (fadeIn > 0) fadeIn -= dt;
    if (!map) return;
    uiRectsPrev = uiRects; uiRects.clear();
    GMode modeBefore = mode; size_t stackBefore = modeStack.size();
    if (mode == GMode::EXPLORE || mode == GMode::COMBAT) updateCameraInput(in, dt);
    if (kDevConsole && running && actP(in, A_CONSOLE) && (mode == GMode::EXPLORE || mode == GMode::COMBAT || mode == GMode::MENU || mode == GMode::JOURNAL)) { pushMode(GMode::CONSOLE); in.eatKeys(); }
    if (banterT > 0) banterT -= dt;
    checkAchievements(); updateHints(dt);
    switch (mode) {
        case GMode::EXPLORE: updateExplore(in, dt); break;
        case GMode::DIALOGUE: updateDialogue(in, dt); break;
        case GMode::COMBAT: updateCombat(in, dt); break;
        case GMode::MENU: updateMenu(in, dt); break;
        case GMode::SHOP: updateShop(in, dt); break;
        case GMode::MESSAGE: updateMessage(in, dt); break;
        case GMode::LEVELUP: updateLevelUp(in, dt); break;
        case GMode::NAME: updateName(in, dt); break;
        case GMode::PAUSE: updatePause(in, dt); break;
        case GMode::CUTSCENE: updateCutscene(in, dt); break;
        case GMode::SAVELOAD: updateSaveLoad(in, dt); break;
        case GMode::JOURNAL: updateJournal(in, dt); break;
        case GMode::WORLDMAP: updateWorldMap(in, dt); break;
        case GMode::CRAFT: updateCraft(in, dt); break;
        case GMode::CONSOLE: updateConsole(in, dt); break;
        case GMode::BOUNTY: updateBounty(in, dt); break;
        case GMode::ENDING: updateEnding(in, dt); break;
        case GMode::VICTORY: if (actP(in, A_CONFIRM) || actP(in, A_CANCEL) || in.mousePressed[0]) { popMode(); checkLevelUps(); if (!levelUpQueue.empty()) pushMode(GMode::LEVELUP); } break;
        case GMode::GAMEOVER: if (actP(in, A_CONFIRM) || in.mousePressed[0]) { requestDesktop = true; } break;
        case GMode::TRANSITION: {
            transition += dt * 2.2f;
            if (transition >= 1.0f && !transitionTo.empty()) { enterMap(transitionTo, transX, transY, false); transitionTo.clear(); if (extra.count("_ambush")) { extra.erase("_ambush"); transition = 0; popMode(); randomEncounter(true); break; } }
            if (transition >= 2.0f) { transition = 0; popMode(); }
        } break;
    }
    if (mode == GMode::EXPLORE && !levelUpQueue.empty()) pushMode(GMode::LEVELUP);
    if (mode == GMode::EXPLORE && modeStack.empty() && demoMode && campaignId == "triffall" && !demoEnd.empty() && !extra.count("demo_end_shown") && evalCond(demoEnd)) startDemoEnd();
    // A click or key that opened/closed a screen must not also reach whatever is now underneath it.
    if (mode != modeBefore || modeStack.size() != stackBefore) {
        in.eatMouse(); in.eatKeys();
        bool opened = modeStack.size() > stackBefore;
        if (mode == GMode::COMBAT) gAudio.play("combat_start");
        else if (mode == GMode::VICTORY) {}
        else if (mode == GMode::GAMEOVER) gAudio.play("defeat");
        else if (mode == GMode::LEVELUP || mode == GMode::TRANSITION || mode == GMode::CUTSCENE) {}
        else if (modeBefore == GMode::COMBAT || modeBefore == GMode::TRANSITION || modeBefore == GMode::VICTORY) {}
        else gAudio.play(opened ? "ui_open" : "ui_close");
    } else if (mode == GMode::MENU || mode == GMode::PAUSE || mode == GMode::SHOP || mode == GMode::SAVELOAD || mode == GMode::LEVELUP || mode == GMode::DIALOGUE) {
        if (actP(in, A_UP) || actP(in, A_DOWN) || (mode != GMode::DIALOGUE && (actP(in, A_LEFT) || actP(in, A_RIGHT)))) gAudio.play("ui_move");
        else if (actP(in, A_CONFIRM)) gAudio.play("ui_confirm");
        else if (in.mousePressed[0]) gAudio.play("ui_click");   // a soft tick for mouse clicks: it never sounds like a purchase went through
    }
}

// ---------------- explore ----------------
void Game::updateExplore(InputState& in, float dt) {
    if (banterVictoryPending) { banterVictoryPending = false; banterTrigger("victory"); }
    if (party.members.size() > 1) { size_t before = party.members.size(); checkCompanionMoods(); if (party.members.size() != before) return; }
    if (actP(in, A_CANCEL)) { openPause(); return; }
    if (actP(in, A_MENU)) { menuTab = 0; menuSel = 0; menuScroll = 0; menuSubFocus = false; pushMode(GMode::MENU); return; }
    if (actP(in, A_SPELLS)) { menuTab = 2; menuSel = 0; menuScroll = 0; menuSubFocus = false; menuMember = clampi(menuMember, 0, (int)party.members.size() - 1); spellCol = party.members[menuMember].spells.empty() ? 0 : 1; pushMode(GMode::MENU); return; }
    if (actP(in, A_ITEMS)) { menuTab = 1; menuSel = 0; menuScroll = 0; menuSubFocus = false; pushMode(GMode::MENU); return; }
    if (actP(in, A_JOURNAL)) { jSel = 0; jScroll = 0; pushMode(GMode::JOURNAL); return; }
    if (actP(in, A_WORLDMAP)) { openWorldMap(); return; }
    for (int f = 0; f < 8; f++) if (actP(in, A_FAV1 + f)) { useFavorite(activeLeader(), f); return; }
    if (in.mousePressed[0]) { int hs = hotbarSlotAt(in.mouseX, in.mouseY, SCREEN_H - 72 - 30); if (hs >= 0) { in.eatMouse(); useFavorite(activeLeader(), hs); return; } }
    // clicking a party portrait in the HUD opens that member's sheet
    if (in.mousePressed[0] && in.mouseY >= SCREEN_H - 72) { int i = (in.mouseX - 6) / 150; if (i >= 0 && i < (int)party.members.size() && in.mouseX < 6 + i * 150 + 144) { menuTab = 0; menuMember = i; menuSel = 0; menuSubFocus = false; pushMode(GMode::MENU); return; } }
    updateNpcs(dt);
    int dx = 0, dy = 0;
    if (actD(in, A_LEFT)) dx = -1; else if (actD(in, A_RIGHT)) dx = 1; else if (actD(in, A_UP)) dy = -1; else if (actD(in, A_DOWN)) dy = 1;
    if (in.gamepadActive) { if (in.stickX < -0.5f) dx = -1; else if (in.stickX > 0.5f) dx = 1; else if (in.stickY < -0.5f) dy = -1; else if (in.stickY > 0.5f) dy = 1; }
    bool pressedDir = actP(in, A_LEFT) || actP(in, A_RIGHT) || actP(in, A_UP) || actP(in, A_DOWN);
    if (dx || dy) {
        if (!movementInputWasDown) movedDuringMoveHold = false;
        movementInputWasDown = true;
        // one constant pace: a step as soon as a direction is pressed, then one every MOVE_STEP while it is held.
        // Tapping can't go faster than holding (the cooldown keeps running after the key is released).
        const float MOVE_STEP = 0.12f; (void)pressedDir;
        moveRepeat -= dt;
        if (moveRepeat <= 0) {
            int oldX = px, oldY = py;
            bool interactOnBlocked = !settings || settings->interactWhileMoving || !movedDuringMoveHold;
            tryMove(dx, dy, interactOnBlocked);
            if (px != oldX || py != oldY) movedDuringMoveHold = true;
            moveRepeat = MOVE_STEP;
        }
    } else { moveRepeat = std::max(0.0f, moveRepeat - dt); movementInputWasDown = false; movedDuringMoveHold = false; }
    if (actP(in, A_INTERACT) || actP(in, A_CONFIRM)) {
        // interact in facing direction, else on current tile
        int fx = px + (facing == 0 ? -1 : facing == 1 ? 1 : 0), fy = py + (facing == 2 ? -1 : facing == 3 ? 1 : 0);
        if (interactable(fx, fy)) interactAt(fx, fy);
        else interactAt(px, py);
    }
    // mouse: click adjacent tile to interact, click further to walk one step toward
    if (in.mousePressed[0] && in.mouseY < SCREEN_H - 72 && !mouseOverUI(in)) {
        int tx = (in.mouseX + camX) / TILE, ty = (in.mouseY + camY) / TILE;
        if (map->inBounds(tx, ty)) {
            if (chebyshev(tx, ty, px, py) <= 1 && (tx != px || ty != py)) {
                if (interactable(tx, ty)) interactAt(tx, ty);
                else tryMove(tx - px, ty - py);
            } else if (tx == px && ty == py) interactAt(px, py);
            else { int mdx = tx - px, mdy = ty - py; if (abs(mdx) > abs(mdy)) tryMove(mdx > 0 ? 1 : -1, 0); else tryMove(0, mdy > 0 ? 1 : -1); }
        }
    }
    if (actP(in, A_QUICKSAVE)) { if (canSaveHere()) openSaveLoad(true); else log("You can only save at a save shrine."); }
    updateFuse(dt);
}

void Game::tryMove(int dx, int dy, bool interactOnBlocked) {
    if (dx && dy) dy = 0;
    if (dx < 0) facing = 0; else if (dx > 0) facing = 1; else if (dy < 0) facing = 2; else if (dy > 0) facing = 3;
    if (dx) faceRight = dx > 0;
    int nx = px + dx, ny = py + dy;
    if (!map->inBounds(nx, ny)) return;
    auto contact = [&](bool present) { if (!present) return false; if (interactOnBlocked) interactAt(nx, ny); return true; };
    if (contact(map->npcAt(nx, ny) != nullptr)) return;
    if (contact(map->chestAt(nx, ny) != nullptr)) return;
    if (Door* d = map->doorAt(nx, ny)) { if (!d->open) { contact(true); return; } }
    if (contact(map->enemyAt(nx, ny) != nullptr)) return;
    { std::string shut = closedShopDoor(nx, ny); if (!shut.empty()) { if (doorMsgCd <= 0) { log(shut); gAudio.play("door_locked"); doorMsgCd = 30; } return; } }
    if (!map->walkable(nx, ny)) { if (contact(map->signAt(nx, ny) != nullptr || map->obstacleAt(nx, ny) != nullptr || map->leverAt(nx, ny) != nullptr || map->gatherAt(nx, ny) != nullptr)) return; return; }
    trail.insert(trail.begin(), {px, py}); if (trail.size() > 8) trail.pop_back();
    px = nx; py = ny; stepCounter++; gAudio.play("step");
    advanceClock((map->kind == "overworld" ? 3 : 1) * tileMoveCost(px, py));   // rough ground takes longer to cross
    if (outdoors() && --weatherSteps <= 0) rerollWeather();
    banterTrigger("idle");
    camFree = false;
    if (lightSteps > 0) lightSteps--; if (stealthSteps > 0) stealthSteps--; if (detectSteps > 0) detectSteps--;
    onStep();
}

void Game::onStep() {
    revealAround(); updateCamera();
    // exits
    if (Exit* e = map->exitAt(px, py)) {
        if (!e->requireFlag.empty() && !evalCond(e->requireFlag)) { if (!e->lockedMsg.empty()) { log(e->lockedMsg); gAudio.play("door_locked"); } }
        else if (!e->requireItem.empty() && !party.hasItem(e->requireItem)) { log(e->lockedMsg.empty() ? "It is locked." : e->lockedMsg); gAudio.play("door_locked"); }
        else { startTransition(e->to, e->tx, e->ty); return; }
    }
    for (auto& t : map->triggers) {
        if (px >= t.x && py >= t.y && px < t.x + t.w && py < t.y + t.h) {
            if (!t.flag.empty() && hasFlag(t.flag) && t.once) continue;
            if (!t.requireFlag.empty() && !evalCond(t.requireFlag)) continue;
            if (!t.flag.empty()) setFlag(t.flag);
            runScript(t.script);
            return;
        }
    }
    if (SavePoint* sp = map->saveAt(px, py)) log((sp->text.empty() ? std::string("A save shrine.") : sp->text) + " Press " + gKeys.label(A_INTERACT) + " or " + gKeys.label(A_QUICKSAVE) + " to save.");
    if (GatherNode* g = map->gatherAt(px, py)) if (gatherReady(*g)) if (auto* it = gd->item(g->item)) log(it->name + " here. Press " + gKeys.label(A_INTERACT) + " to gather" + (g->tool.empty() ? "." : " (needs a " + (gd->item(g->tool) ? gd->item(g->tool)->name : g->tool) + ")."));
    // proximity combat (skipped for a few steps after fleeing)
    { auto fg = flags.find("_fleegrace"); if (fg != flags.end()) { if (--fg->second <= 0) flags.erase(fg); return; } }
    for (size_t i = 0; i < map->enemies.size(); i++) {
        auto& e = map->enemies[i];
        if (e.defeated) continue;
        if (!e.showFlag.empty() && !evalCond(e.showFlag)) continue;
        if (!e.hideFlag.empty() && evalCond(e.hideFlag)) continue;
        int aggro = stealthSteps > 0 ? std::max(1, e.aggro / 3) : e.aggro;
        if (chebyshev(e.x, e.y, px, py) <= aggro && lineOfSight(*map, px, py, e.x, e.y)) {
            // everyone who would notice joins from the start: groups close to the party, groups that can see the group that
            // spotted you, and groups linked to it (link= in the map file). The rest may still come running once the noise starts.
            combatExtraGroups.clear();
            for (size_t j = 0; j < map->enemies.size(); j++) {
                if (j == i || map->enemies[j].defeated) continue;
                auto& e2 = map->enemies[j];
                if (!e2.showFlag.empty() && !evalCond(e2.showFlag)) continue;
                if (!e2.hideFlag.empty() && evalCond(e2.hideFlag)) continue;
                int dp = chebyshev(e2.x, e2.y, px, py), dg = chebyshev(e2.x, e2.y, e.x, e.y);
                bool joins = dp <= 4 || (dp <= 7 && lineOfSight(*map, px, py, e2.x, e2.y)) || (dg <= 7 && lineOfSight(*map, e.x, e.y, e2.x, e2.y)) || (!e.link.empty() && e2.link == e.link);
                if (groupHasBoss(e2) && dp > 3 && !(!e.link.empty() && e2.link == e.link)) joins = false;   // bosses stay in their lair
                if (joins) combatExtraGroups.push_back((int)j);
            }
            startCombat(e.creatures, e.x, e.y, e.levelBonus, {(int)i}, e.onDefeatScript);
            return;
        }
    }
    if (map->encounterRate > 0) randomEncounter();
}

void Game::randomEncounter(bool force) {
    const Cell& c = map->at(px, py);
    const TileKind& k = allTileKinds()[c.kind];
    int chance = force ? 100 : map->encounterRate * k.encounter / 2;
    if (stealthSteps > 0) chance /= 4;
    if (nightAmount() > 0.5f) chance = chance * 3 / 2;
    if (weather == "rain" || weather == "storm") chance = chance * 3 / 4; else if (weather == "fog") chance = chance * 5 / 4;
    if (chance <= 0 || !rng.chance(chance)) return;
    std::string table = k.biome; if (table == "loc") { if (!force) return; table = "plains"; }
    auto it = encounters.end();
    if (nightAmount() > 0.5f) it = encounters.find(table + "_night");
    if (it == encounters.end()) it = encounters.find(table);
    if (it == encounters.end()) it = encounters.find(map->encounterTable);
    if (it == encounters.end()) return;
    int heroLvl = party.hero() ? party.hero()->level : 1;
    std::vector<std::vector<std::string>> cand;
    for (size_t i = 0; i < it->second.groups.size(); i++) cand.push_back(it->second.groups[i]);
    if (cand.empty()) return;
    auto grp = rng.pick(cand);
    int lvlBonus = std::max(0, (heroLvl - it->second.minLevel) / 3);
    log("Ambush!"); gAudio.play("ambush");
    startCombat(grp, px, py, lvlBonus, {}, "");
}

void Game::interactAt(int x, int y) {
    if (NPC* n = map->npcAt(x, y)) {
        facing = x < px ? 0 : x > px ? 1 : y < py ? 2 : 3; if (x != px) faceRight = x > px;
        if (!n->hostileCreature.empty()) { startCombat(split(n->hostileCreature, ','), n->x, n->y, 0, {}, ""); hideNpcForGood(*n); return; }
        if (!n->board.empty()) { openBoard(n->board); return; }
        if (n->travel) { log(n->name + ": \"Where to? Coach fare is by the league.\""); openWorldMap(true); return; }
        if (!n->craft.empty() && n->dialogue.empty()) { openCraft(n->craft); return; }
        if (!n->dialogue.empty()) startDialogue(n->dialogue, n);
        else if (!n->shop.empty()) { std::string hrs; if (shopRefuses(n->shop)) return; if (!shopOpen(n->shop, hrs)) { log(n->name + ": \"We're closed. Open " + hrs + ".\""); gAudio.play("door_locked"); return; } shopId = n->shop; shopTab = 0; shopSel = 0; shopScroll = 0; shopQty = -1; shopRestockCheck(shopId); shopFaction = shopFactionOf(shopId); pushMode(GMode::SHOP); }
        else log(n->name + " has nothing to say.");
        return;
    }
    if (Chest* c = map->chestAt(x, y)) {
        if (c->opened) { log("The chest is empty."); return; }
        if (!c->locked.empty() && !party.hasItem(c->locked)) { log("Locked. It needs a " + (gd->item(c->locked) ? gd->item(c->locked)->name : c->locked) + "."); gAudio.play("door_locked"); return; }
        c->opened = true; if (!c->flag.empty()) setFlag(c->flag); gAudio.play(c->sound.empty() ? "chest_open" : c->sound); stats.chests++;
        if (c->gold > 0) { party.gold += c->gold; stats.goldEarned += c->gold; log(fmt("Found %d gold.", c->gold)); gAudio.play("gold"); }
        for (auto& it : c->items) { std::string id = it.first; if (auto* d = gd->item(id)) if (d->equippable()) id = makeAffixed(id, lootTierRoll(party.hero() ? party.hero()->level : 1)); giveItem(id, it.second); }
        if (c->gold == 0 && c->items.empty() && c->onUse.empty()) log("The chest is empty.");
        if (!c->onUse.empty()) runScript(c->onUse);
        return;
    }
    if (Door* d = map->doorAt(x, y)) {
        if (d->open) return;
        if (!d->key.empty() && !party.hasItem(d->key)) { log("Locked. It needs a " + (gd->item(d->key) ? gd->item(d->key)->name : d->key) + "."); gAudio.play("door_locked"); return; }
        if (!d->require.empty() && !evalCond(d->require)) { log(d->lockedMsg.empty() ? "It won't open. Not yet." : d->lockedMsg); gAudio.play("door_locked"); return; }
        if (!d->openIf.empty() && !evalCond(d->openIf)) { log(d->lockedMsg.empty() ? "It won't open from this side. There must be a mechanism somewhere." : d->lockedMsg); gAudio.play("door_locked"); return; }
        d->open = true; gAudio.play(!d->sound.empty() ? d->sound : d->key.empty() ? "door_open" : "unlock"); if (!d->flag.empty()) setFlag(d->flag);
        map->at(d->x, d->y).walkable = true; map->at(d->x, d->y).blocksSight = false; map->at(d->x, d->y).prop = d->openTile;
        log(!d->msg.empty() ? d->msg : d->key.empty() ? "The door opens." : "You unlock the door.");
        revealAround();
        if (!d->onUse.empty()) runScript(d->onUse);
        return;
    }
    if (SignPost* s = map->signAt(x, y)) { if (!s->sound.empty()) gAudio.play(s->sound); std::string title = s->title.empty() ? "Sign" : s->title; showMessage(title, s->text); addNote(map->name + " " + toLower(title), s->text); if (!s->onUse.empty()) runScript(s->onUse); return; }
    if (Obstacle* o = map->obstacleAt(x, y)) { obstacleMenu((int)(o - &map->obstacles[0])); return; }
    if (Lever* l = map->leverAt(x, y)) { pullLever(*l); return; }
    if (GatherNode* g = map->gatherAt(x, y)) { gatherFrom(*g); return; }
    if (EnemyGroup* e = map->enemyAt(x, y)) {
        size_t idx = e - &map->enemies[0];
        startCombat(e->creatures, e->x, e->y, e->levelBonus, {(int)idx}, e->onDefeatScript); return;
    }
    if (SavePoint* sp = map->saveAt(x, y)) if (x == px && y == py) { if (!sp->sound.empty()) gAudio.play(sp->sound); if (!sp->onUse.empty()) runScript(sp->onUse); openSaveLoad(true); return; }
    if (Exit* e = map->exitAt(x, y)) { if (x == px && y == py) { onStep(); (void)e; } }
}

void Game::updateNpcs(float dt) {
    for (auto& n : map->npcs) {
        n.hidden = extra.count("hidenpc_" + mapId + "_" + n.id) > 0;
        if (!n.showFlag.empty() && !evalCond(n.showFlag)) n.hidden = true;
        if (!n.hideFlag.empty() && evalCond(n.hideFlag)) n.hidden = true;
        if (n.hidden || !n.wander) continue;
        n.wanderTimer -= (int)(dt * 1000);
        if (n.wanderTimer <= 0) {
            n.wanderTimer = rng.range(800, 3000);
            int d = rng.range(0, 3); int nx = n.x + (d == 0) - (d == 1), ny = n.y + (d == 2) - (d == 3);
            if (map->walkable(nx, ny) && !map->occupied(nx, ny) && !(nx == px && ny == py) && chebyshev(nx, ny, n.homeX, n.homeY) <= 3 && !map->exitAt(nx, ny)) { n.x = nx; n.y = ny; }
        }
    }
    for (auto& e : map->enemies) {
        if (e.defeated) continue;
        e.wanderTimer -= (int)(dt * 1000);
        if (e.wanderTimer <= 0) {
            e.wanderTimer = rng.range(600, 2000);
            int dist = chebyshev(e.x, e.y, px, py);
            int nx = e.x, ny = e.y;
            if (dist <= e.aggro + 3 && stealthSteps <= 0 && lineOfSight(*map, e.x, e.y, px, py)) { // approach
                if (abs(px - e.x) > abs(py - e.y)) nx += px > e.x ? 1 : -1; else ny += py > e.y ? 1 : -1;
            } else { int d = rng.range(0, 5); nx += (d == 0) - (d == 1); ny += (d == 2) - (d == 3); if (chebyshev(nx, ny, e.homeX, e.homeY) > 4) { nx = e.x; ny = e.y; } }
            if ((nx != e.x || ny != e.y) && map->walkable(nx, ny) && !map->occupied(nx, ny) && !(nx == px && ny == py)) { e.x = nx; e.y = ny; }
            if (chebyshev(e.x, e.y, px, py) <= 1 && mode == GMode::EXPLORE) { onStep(); }
        }
    }
}

// ---------------- scripting / conditions ----------------
bool Game::evalCond(const std::string& cond) const {
    // conditions separated by '&' ; each: flag | !flag | item:id | gold:n | level:n | flag>=n
    for (auto& part : split(cond, '&')) {
        std::string c = trim(part); if (c.empty()) continue;
        bool neg = false; if (c[0] == '!') { neg = true; c = c.substr(1); }
        bool val = false;
        if (startsWith(c, "item:")) val = party.hasItem(c.substr(5));
        else if (startsWith(c, "gold:")) val = party.gold >= atoi(c.substr(5).c_str());
        else if (startsWith(c, "level:")) val = party.members.size() && party.members[0].level >= atoi(c.substr(6).c_str());
        else if (startsWith(c, "party:")) { for (auto& m : party.members) if (m.classId == c.substr(6) || toLower(m.name) == toLower(c.substr(6))) val = true; }
        else if (startsWith(c, "kills:")) { auto p = split(c.substr(6), ':'); auto it = bestiary.find(p[0]); val = it != bestiary.end() && it->second >= (p.size() > 1 ? atoi(p[1].c_str()) : 1); }
        else if (startsWith(c, "itemn:")) { auto p = split(c.substr(6), ':'); val = party.countItem(p[0]) >= (p.size() > 1 ? atoi(p[1].c_str()) : 1); }
        else if (startsWith(c, "rep:")) { auto p = split(c.substr(4), ':'); auto it = reputation.find(p[0]); int a = it == reputation.end() ? 0 : it->second; val = a >= (p.size() > 1 ? atoi(p[1].c_str()) : 1); }
        else if (startsWith(c, "approval:")) { auto p = split(c.substr(9), ':'); auto it = approval.find(p[0]); int a = it == approval.end() ? 0 : it->second; val = a >= (p.size() > 1 ? atoi(p[1].c_str()) : 1); }
        else if (c == "room") val = party.members.size() < 4;   // a free place in the party
        else if (c == "night") val = nightAmount() > 0.5f; else if (c == "day") val = nightAmount() <= 0.5f;
        else if (startsWith(c, "weather:")) val = weather == c.substr(8);
        else if (startsWith(c, "mode:")) { static const char* MN[] = {"explore", "dialogue", "combat", "menu", "shop", "message", "transition", "gameover", "levelup", "name", "victory", "pause", "cutscene", "saveload", "journal", "worldmap", "craft", "console", "bounty", "ending"};
            std::string want = c.substr(5); int mi = (int)mode; val = mi >= 0 && mi < 20 && want == MN[mi];
            if (!val && want == "combat") val = inCombat(); }
        else if (startsWith(c, "equipped:")) { std::string id = c.substr(9); for (auto& m : party.members) for (int s = 0; s < SL_COUNT; s++) if (m.equip[s] == id) val = true; }
        else if (startsWith(c, "fav:")) { std::string id = c.substr(4); for (auto& m : party.members) for (auto& f : m.favs) if (!f.empty() && (id == "any" || f == "spell:" + id || f == "item:" + id)) val = true; }
        else if (startsWith(c, "map:")) val = mapId == c.substr(4);
        else if (startsWith(c, "kills:")) { auto p = split(c.substr(6), ':'); auto it = bestiary.find(p[0]); val = it != bestiary.end() && it->second >= (p.size() > 1 ? atoi(p[1].c_str()) : 1); }
        else if (c.find(">=") != std::string::npos) { auto p = split(c, '>'); auto it = flags.find(trim(p[0])); int v = it == flags.end() ? 0 : it->second; val = v >= atoi(p[1].c_str() + 1); }
        else val = hasFlag(c);
        if (neg) val = !val;
        if (!val) return false;
    }
    return true;
}

void Game::runScript(const std::string& script) {
    // script forms: "dialogue:id", "message:Title|Text", "combat:a,b,c", "teleport:map,x,y", "flag:name", "item:id:n", "gold:n", "heal", "cutscene:line1|line2"
    for (auto& cmd : split(script, ';')) {
        std::string s = trim(cmd); if (s.empty()) continue;
        size_t col = s.find(':'); std::string k = col == std::string::npos ? s : s.substr(0, col), v = col == std::string::npos ? "" : s.substr(col + 1);
        runAction(k, v);
    }
}

// ---------------- dialogue ----------------
bool Game::loadDialogues() {
    std::vector<std::string> files;
    std::error_code ec;
    bool base = campaignDir.empty() || campaign.inherit;
    for (std::string dir : {base ? assetDir + "/data/dialogue" : std::string(), campaignDir.empty() ? std::string() : campaignDir + "/dialogue"}) {
        if (dir.empty()) continue;
        for (auto& entry : std::filesystem::directory_iterator(dir, ec)) {
            if (!entry.is_regular_file()) continue;
            auto path = entry.path();
            if (path.extension() != ".txt") continue;
            if (std::find(files.begin(), files.end(), path.stem().string()) == files.end()) files.push_back(path.stem().string());
        }
    }
    for (auto& f : files) {
        std::string text; if (!readFile(dataPath("dialogue/" + f + ".txt"), text)) continue;
        Dialogue d; d.id = f; DialogueNode* cur = nullptr;
        for (auto& raw : split(text, '\n')) {
            std::string l = trim(raw); if (l.empty() || l[0] == '#') continue;
            if (l[0] == '[') { DialogueNode n; n.id = trim(l.substr(1, l.find(']') - 1)); d.nodes[n.id] = n; cur = &d.nodes[n.id]; continue; }
            if (!cur) continue;
            size_t eq = l.find('='); if (eq == std::string::npos) continue;
            std::string k = trim(l.substr(0, eq)), v = trim(l.substr(eq + 1));
            if (k == "speaker") cur->speaker = v;
            else if (k == "text") { if (!cur->text.empty()) cur->text += "\n"; cur->text += v; }
            else if (k == "option") { auto p = split(v, '|'); DialogueOption o; o.text = trim(p[0]); o.target = p.size() > 1 ? trim(p[1]) : "end"; for (size_t i = 2; i < p.size(); i++) o.conds.push_back(trim(p[i])); cur->options.push_back(o); }
            else if (k == "next") cur->next = v;
            else cur->actions.push_back({k, v});
        }
        dialogues[d.id] = d;
    }
    return true;
}

void Game::startDialogue(const std::string& id, NPC* npc, const std::string& node) {
    if (dialogues.find(id) == dialogues.end()) { log("(missing dialogue " + id + ")"); return; }
    dlgId = id; dlgNpc = npc; dlgSpeakerOverride = npc ? npc->name : "";
    if (npc) { dlgPortrait = npc->tile; dlgPortraitRecolor = npc->recolor; dlgPortraitColor = npc->color; } else dlgPortrait = Tile{};
    if (mode != GMode::DIALOGUE) pushMode(GMode::DIALOGUE);
    showNode(node);
}

void Game::showNode(const std::string& node) {
    Dialogue& d = dialogues[dlgId];
    auto it = d.nodes.find(node);
    if (node == "end" || it == d.nodes.end()) { if (mode == GMode::DIALOGUE) popMode(); return; }
    dlgNode = node; dlgSel = 0; dlgTextShown = 0; dlgCharTimer = 0; dlgOptT = 0; dlgOptArmed = false; gAudio.play("text_blip");
    // run actions
    GMode before = mode;
    for (auto& a : it->second.actions) runAction(a.first, a.second);
    if (mode != before && mode != GMode::DIALOGUE) return; // an action changed mode (combat, shop...)
    // substitute hero name
    (void)before;
}

void Game::runAction(const std::string& k, const std::string& v) {
    runActionInner(k, v);
    if (map && (k == "set" || k == "flag" || k == "clear" || k == "inc")) applyWorldState(*map);
    if (k == "set" || k == "flag" || k == "clear") syncBonds();
}
void Game::runActionInner(const std::string& k, const std::string& v) {
    if (k == "obstacle") { obstacleAction(v); return; }
    if (k == "healer") { healerMenu(v); return; }
    if (k == "healsvc") { if (mode == GMode::DIALOGUE) popMode(); healerService(v); return; }
    if (k == "npcmove") { auto p = split(v, ','); if (p.size() >= 3 && map) { extra["npcpos_" + mapId + "_" + trim(p[0])] = trim(p[1]) + "," + trim(p[2]); applyWorldState(*map); } return; }
    if (k == "mapset") { auto p = split(v, ','); if (p.size() >= 3 && map) { std::string& e = extra["mapset_" + mapId]; e += (e.empty() ? "" : ";") + trim(p[0]) + "," + trim(p[1]) + "," + trim(p[2]); applyWorldState(*map); } return; }
    if (k == "set") { for (auto& f : split(v, ',')) { auto p = split(trim(f), '='); if (p.size() == 2) setFlag(trim(p[0]), atoi(p[1].c_str())); else if (!trim(f).empty()) setFlag(trim(f)); } }
    else if (k == "flag") setFlag(v);
    else if (k == "clear") for (auto& f : split(v, ',')) flags.erase(trim(f));
    else if (k == "inc") { flags[v] = flags[v] + 1; }
    else if (k == "give" || k == "item") { for (auto& it : split(v, ',')) { auto p = split(trim(it), ':'); if (trim(p[0]).empty()) continue; giveItem(trim(p[0]), p.size() > 1 ? atoi(p[1].c_str()) : 1); } }
    else if (k == "take") { for (auto& it : split(v, ',')) { auto p = split(trim(it), ':'); party.removeItem(trim(p[0]), p.size() > 1 ? atoi(p[1].c_str()) : 1); if (auto* d = gd->item(trim(p[0]))) log("Gave " + d->name + "."); } }
    else if (k == "gold") { int g = atoi(v.c_str()); party.gold = std::max(0, party.gold + g); if (g > 0) stats.goldEarned += g; else stats.goldSpent -= g; log(g >= 0 ? fmt("Received %d gold.", g) : fmt("Paid %d gold.", -g)); gAudio.play("gold"); }
    else if (k == "xp") giveXP(atoi(v.c_str()));
    else if (k == "heal") { for (auto& m : party.members) m.fullHeal(*gd); log("The party is fully rested."); gAudio.play("rest"); if (clockMin > 7 * 60) { day++; } clockMin = 7 * 60; }
    else if (k == "shop") { std::string hrs; if (shopRefuses(v)) return; if (!shopOpen(v, hrs)) { if (mode == GMode::DIALOGUE) popMode(); showMessage("Closed", "The shop is closed right now. Opening hours: " + hrs + "."); } else { shopId = v; shopTab = 0; shopSel = 0; shopScroll = 0; shopQty = -1; shopRestockCheck(shopId); shopFaction = shopFactionOf(shopId); if (mode == GMode::DIALOGUE) popMode(); pushMode(GMode::SHOP); } }
    else if (k == "join") {
        auto jp = split(v, ':'); std::string cls = trim(jp[0]); std::string cname = jp.size() > 1 ? trim(jp[1]) : ""; Tile ctile = jp.size() > 2 ? parseTile(jp[2]) : Tile{};
        const ClassDef* c = gd->cls(cls);
        if (c && party.members.size() < 4) {
            bool already = hasFlag("joined_" + cls);
            if (!already) { if (mode == GMode::DIALOGUE) extra["dlg_" + cls] = dlgId; Character ch; ch.initFromClass(*gd, *c, cname.empty() ? c->name : cname); int lvl = party.members[0].level;
                // companions arrive trained in their own class at the hero's level (respec at a shrine to change that)
                while (ch.level < lvl) { ch.level++; ch.ranks[cls]++; for (int s = 0; s < ST_COUNT; s++) if (ch.level % 2 == 0) ch.baseStats[s]++; }
                for (auto& sl : c->spellLevels) if (sl.first <= ch.rank(cls) && std::find(ch.spells.begin(), ch.spells.end(), sl.second) == ch.spells.end()) ch.spells.push_back(sl.second); if (ctile.valid()) ch.tile = ctile; ch.hp = ch.maxHP(*gd); ch.mp = ch.maxMP(*gd); party.members.push_back(ch); log(ch.name + " joined the party!"); setFlag("joined_" + cls); flags.erase("left_" + cls); if (dlgNpc) { hideNpcForGood(*dlgNpc); if (!dlgNpc->id.empty()) extra["home_" + cls] = "hidenpc_" + mapId + "_" + dlgNpc->id; } syncBonds(); }
        } else if (party.members.size() >= 4) log("Your party is full.");
    }
    else if (k == "leave") { for (size_t i = 1; i < party.members.size(); i++) if (party.members[i].classId == v) { log(party.members[i].name + " left the party."); party.members.erase(party.members.begin() + i); break; } }
    else if (k == "combat") { std::vector<std::string> cs; for (auto& c : split(v, ',')) if (!trim(c).empty()) cs.push_back(trim(c)); if (mode == GMode::DIALOGUE) popMode(); int cx = dlgNpc ? dlgNpc->x : px, cy = dlgNpc ? dlgNpc->y : py; if (dlgNpc) hideNpcForGood(*dlgNpc); startCombat(cs, cx, cy, 0, {}, ""); }
    else if (k == "teleport") { auto p = split(v, ','); if (p.size() >= 3) { if (mode == GMode::DIALOGUE) popMode(); startTransition(trim(p[0]), atoi(p[1].c_str()), atoi(p[2].c_str())); } }
    else if (k == "message") { auto p = split(v, '|'); showMessage(p.size() > 1 ? p[0] : "Note", p.size() > 1 ? p[1] : p[0]); if (p.size() > 1) addNote(p[0], p[1]); }
    else if (k == "dialogue") { startDialogue(v, dlgNpc); }
    else if (k == "cutscene") { std::vector<std::string> ls; for (auto& l : split(v, '|')) ls.push_back(l); startCutscene(ls, nullptr); }
    else if (k == "log") log(v);
    else if (k == "notify") notify(v);
    else if (k == "hidenpc") { for (auto& n : map->npcs) if (n.id == v) hideNpcForGood(n); }
    else if (k == "shownpc") { extra.erase("hidenpc_" + mapId + "_" + v); }
    else if (k == "spawn") { auto p = split(v, ','); if (p.size() >= 3) { EnemyGroup e; e.x = e.homeX = atoi(p[1].c_str()); e.y = e.homeY = atoi(p[2].c_str()); e.creatures.push_back(trim(p[0])); for (size_t i = 3; i < p.size(); i++) e.creatures.push_back(trim(p[i])); e.aggro = 6; map->enemies.push_back(e); } }
    else if (k == "quest") { notify("Quest updated: " + v); gAudio.play("quest"); }
    else if (k == "craft") { if (mode == GMode::DIALOGUE) popMode(); openCraft(v); }
    else if (k == "board") { if (mode == GMode::DIALOGUE) popMode(); openBoard(v); }
    else if (k == "travel") { if (mode == GMode::DIALOGUE) popMode(); openWorldMap(true); }
    else if (k == "revealmap") { auto p = split(v, ','); std::string id = trim(p[0]); if (id.empty() || id == "here") id = mapId; if (p.size() >= 4) revealMap(id, atoi(p[1].c_str()), atoi(p[2].c_str()), atoi(p[3].c_str())); else revealMap(id); notify("Map updated."); }
    else if (k == "rep") { for (auto& part : split(v, ',')) { auto p = split(trim(part), ':'); if (p.size() >= 2) changeRep(trim(p[0]), atoi(p[1].c_str())); } }
    // approve=CLASS:N,...: only companions in the party see it happen (all = every companion with you)
    else if (k == "approve") { for (auto& part : split(v, ',')) { auto p = split(trim(part), ':'); if (p.size() < 2) continue; int d = atoi(p[1].c_str()); std::string cls = trim(p[0]); for (size_t i = 1; i < party.members.size(); i++) if (cls == "all" || party.members[i].classId == cls) changeApproval(party.members[i].classId, d); } }
    // amends=CLASS:N: approval for a companion who isn't with you (making up with one who left)
    else if (k == "amends") { auto p = split(v, ':'); if (p.size() >= 2) { approval[trim(p[0])] += atoi(p[1].c_str()); syncBonds(); } }
    // craftxp=STATION:N: practice at a station (forge, alchemy, workbench, field) - master crafters' trials
    else if (k == "craftxp") { auto p = split(v, ':'); if (p.size() >= 2) addCraftXP(trim(p[0]), atoi(p[1].c_str())); }
    else if (k == "note") { auto p = split(v, '|'); addNote(p.size() > 1 ? p[0] : "Note", p.size() > 1 ? p[1] : p[0]); notify("Added to journal."); }
    else if (k == "win") { if (mode == GMode::DIALOGUE) popMode(); std::vector<std::string> ls; for (auto& l : split(v, '|')) ls.push_back(endingText(l)); startCutscene(ls, [this]() { setFlag("game_won"); checkAchievements(true); startEnding(); }); }
    else if (k == "savepoint") { if (map) { SavePoint s; s.x = px; s.y = py; applyTypeLook(s); map->savepoints.push_back(s); } }
    // finish:CAMPAIGN|line|line... - the end of a tutorial-style campaign: closing lines, then back to the desktop with New
    // Game open for CAMPAIGN (no epilogue or credits)
    else if (k == "finish") { if (mode == GMode::DIALOGUE) popMode(); auto p = split(v, '|'); std::string next = p.empty() ? "" : trim(p[0]); std::vector<std::string> ls; for (size_t i = 1; i < p.size(); i++) ls.push_back(endingText(p[i]));
        auto done = [this, next]() { setFlag("campaign_finished"); desktopAction = next.empty() ? "" : "campaign:" + next; requestDesktop = true; };
        if (ls.empty()) done(); else startCutscene(ls, done); }
    else if (k == "respec") { if (mode == GMode::DIALOGUE) popMode(); menuTab = 0; menuMember = 0; menuSel = 0; menuSubFocus = false; menuPrompt = "respec"; respecFree = v == "free"; pushMode(GMode::MENU); }
    else if (k == "rest") { for (auto& m : party.members) { m.hp = m.maxHP(*gd); m.mp = m.maxMP(*gd); m.alive = true; m.effects.clear(); } log("You rest until morning."); gAudio.play("rest"); if (clockMin > 7 * 60) day++; clockMin = 7 * 60; }
}

void Game::updateDialogue(InputState& in, float dt) {
    Dialogue& d = dialogues[dlgId];
    auto it = d.nodes.find(dlgNode); if (it == d.nodes.end()) { popMode(); return; }
    DialogueNode& n = it->second;
    std::string text = n.text;
    { size_t pos; while ((pos = text.find("$hero")) != std::string::npos) text.replace(pos, 5, heroName); } text = keyText(text);   // as drawDialogue shows it
    dlgCharTimer += dt;
    { static const float per[4] = {0.03f, 0.012f, 0.005f, 0.0f}; float step = per[clampi(settings ? settings->textSpeed : 1, 0, 3)];
      if (step <= 0) dlgTextShown = (int)text.size();
      else while (dlgCharTimer > step && dlgTextShown < (int)text.size()) { dlgCharTimer -= step; dlgTextShown++; } }
    std::vector<int> avail;
    for (size_t i = 0; i < n.options.size(); i++) { bool ok = true; for (auto& c : n.options[i].conds) if (!evalCond(c)) ok = false; if (ok) avail.push_back((int)i); }
    bool textDone = dlgTextShown >= (int)text.size();
    if (actP(in, A_UP) && textDone) dlgSel = (dlgSel + (int)avail.size() - 1) % std::max(1, (int)avail.size());
    if (actP(in, A_DOWN) && textDone) dlgSel = (dlgSel + 1) % std::max(1, (int)avail.size());
    // mouse hover over options
    if (textDone && !avail.empty()) {
        int oh = dialogueOptionH(), oy = dialogueOptionsY((int)avail.size());
        if (in.mouseDX || in.mouseDY || in.mousePressed[0]) for (size_t i = 0; i < avail.size(); i++) if (in.mouseY >= oy + (int)i * oh && in.mouseY < oy + (int)i * oh + oh && in.mouseX > 20 && in.mouseX < SCREEN_W - 20) dlgSel = (int)i;
    }
    bool confirm = actP(in, A_CONFIRM) || actP(in, A_INTERACT) || in.mousePressed[0];
    // 1.1.1: skipping is its own press and never picks an answer. Once the text is all there the answers are shown dim
    // and take no press for a moment (DLG_ARM_SECONDS) and not before every confirm key / button has been let go, so
    // mashing to skip text (or a press just as the text finishes) can't choose an option by accident.
    if (textDone) {
        dlgOptT += dt;
        bool held = actD(in, A_CONFIRM) || actD(in, A_INTERACT) || in.mouseDown[0];
        if (!dlgOptArmed && dlgOptT >= DLG_ARM_SECONDS && !held && !confirm) dlgOptArmed = true;
    }
    if (confirm) {
        if (!textDone) { dlgTextShown = (int)text.size(); dlgOptT = 0; dlgOptArmed = false; return; }
        if (avail.empty()) { if (!n.next.empty()) showNode(n.next); else popMode(); return; }
        if (!dlgOptArmed) return;
        dlgSel = clampi(dlgSel, 0, (int)avail.size() - 1);
        showNode(n.options[avail[dlgSel]].target);
    }
    if (actP(in, A_CANCEL)) { if (!textDone) { dlgTextShown = (int)text.size(); dlgOptT = 0; dlgOptArmed = false; } else if (avail.empty()) { if (!n.next.empty()) showNode(n.next); else popMode(); } }
}

int Game::textScale() const { return settings && settings->largeText ? 2 : 1; }
int Game::dialogueOptionH() const { return textScale() == 2 ? 26 : 14; }
int Game::dialogueOptionsY(int n) const { return SCREEN_H - (textScale() == 2 ? 30 : 56) - n * dialogueOptionH(); }
void Game::drawDialogue(Canvas& cv, InputState& in) {
    Dialogue& d = dialogues[dlgId];
    auto it = d.nodes.find(dlgNode); if (it == d.nodes.end()) return;
    DialogueNode& n = it->second;
    std::vector<int> avail;
    for (size_t i = 0; i < n.options.size(); i++) { bool ok = true; for (auto& c : n.options[i].conds) if (!evalCond(c)) ok = false; if (ok) avail.push_back((int)i); }
    int ts = textScale(), lh = 12 * ts + 1, oh = dialogueOptionH();
    std::string speaker = n.speaker.empty() ? dlgSpeakerOverride : n.speaker;
    if (speaker == "$hero") speaker = heroName;
    std::string text = n.text;
    size_t pos; while ((pos = text.find("$hero")) != std::string::npos) text.replace(pos, 5, heroName);
    text = keyText(text);   // {journal}, {menu}... name the keys of the device in use
    int tx = dlgPortrait.valid() ? 80 : 20;
    auto lines = ctx.font->wrap(text, SCREEN_W - 40 - (tx - 20), ts);
    int boxH = 130 + (int)avail.size() * 14;
    if (ts == 2) boxH = std::max(150, 34 + (int)lines.size() * lh + 12 + (int)avail.size() * oh + (avail.empty() ? 24 : 18));
    boxH = std::min(boxH, SCREEN_H - 8);
    int by = SCREEN_H - boxH - 4;
    uiRect(0, 0, SCREEN_W, SCREEN_H);
    panel(ctx, 8, by, SCREEN_W - 16, boxH, RGB(16, 16, 28), C::Gold);
    if (dlgPortrait.valid()) {
        cv.fillRect(18, by + 10, 52, 52, RGB(40, 40, 60)); cv.rect(18, by + 10, 52, 52, C::Gold);
        if (dlgPortraitRecolor) ctx.ts->drawRecolor(cv, dlgPortrait, 20, by + 12, 4, dlgPortraitColor); else ctx.ts->draw(cv, dlgPortrait, 20, by + 12, 4);
    }
    if (!speaker.empty()) { ctx.font->bold = true; UI::text(ctx, tx, by + 8, speaker, C::Gold, ts == 2 ? 2 : 1); ctx.font->bold = false; }
    int shown = std::min(dlgTextShown, (int)text.size());
    // keep wrapping stable: wrap full text, then draw prefix per line
    int cy = by + (ts == 2 ? 36 : 26), remaining = shown;
    for (auto& l : lines) { int take = std::min((int)l.size(), remaining); if (take <= 0) break; UI::text(ctx, tx, cy, l.substr(0, take), C::White, ts); remaining -= (int)l.size() + 1; cy += lh; }
    bool textDone = dlgTextShown >= (int)text.size();
    if (!textDone) textRight(ctx, SCREEN_W - 24, SCREEN_H - 22, "[" + gKeys.label(A_CONFIRM) + "] skip", C::Gray);   // 1.1.1: skipping is its own step
    if (textDone) {
        int oy = dialogueOptionsY((int)avail.size());
        if (avail.empty()) { textRight(ctx, SCREEN_W - 24, SCREEN_H - 22, "[" + gKeys.label(A_CONFIRM) + "] continue", C::Gray); }
        bool armed = dlgOptArmed;   // dim until the answers can be picked (see updateDialogue)
        for (size_t i = 0; i < avail.size(); i++) {
            bool sel = (int)i == dlgSel;
            if (sel) cv.fillRect(20, oy + (int)i * oh - 1, SCREEN_W - 40, oh, armed ? RGB(70, 55, 20) : RGB(40, 34, 20));
            std::string label = (sel ? "> " : "  ") + n.options[avail[i]].text;
            if (ts == 2) while (label.size() > 4 && ctx.font->width(label, 2) > SCREEN_W - 52) label = label.substr(0, label.size() - 4) + "..";
            UI::text(ctx, 26, oy + (int)i * oh, label, !armed ? C::Gray : sel ? C::Yellow : C::LtGray, ts);
        }
    }
    (void)in;
}

void Game::updateCutscene(InputState& in, float dt) {
    cutTimer += dt;
    if (actP(in, A_CONFIRM) || actP(in, A_INTERACT) || in.mousePressed[0] || actP(in, A_CANCEL)) {
        if (cutTimer < 0.6f) return;
        cutIndex++; cutTimer = 0;
        if (cutIndex >= (int)cutLines.size()) { popMode(); auto done = cutDone; cutDone = nullptr; if (done) done(); }
    }
}
void Game::drawCutscene(Canvas& cv, InputState& in) {
    cv.clear(C::Black);
    if (cutIndex < (int)cutLines.size()) {
        int alpha = (int)std::min(1.0f, cutTimer * 2.0f) * 255;
        (void)alpha;
        int ts = textScale();
        auto lines = ctx.font->wrap(cutLines[cutIndex], ts == 2 ? 580 : 440, ts);
        int lh = ts == 2 ? 28 : 16, y = SCREEN_H / 2 - (int)lines.size() * lh / 2;
        for (auto& l : lines) { textCenter(ctx, SCREEN_W / 2, y, l, C::Cream, ts); y += lh; }
        if (cutTimer > 0.6f) textCenter(ctx, SCREEN_W / 2, SCREEN_H - 40, "[Enter]", C::DkGray);
    }
    (void)in;
}
void Game::updateMessage(InputState& in, float dt) {
    (void)dt;
    if (actP(in, A_CONFIRM) || actP(in, A_CANCEL) || actP(in, A_INTERACT) || in.mousePressed[0]) { popMode(); auto d = messageDone; messageDone = nullptr; if (d) d(); }
}
void Game::drawMessage(Canvas& cv, InputState& in) {
    int ts = textScale(), bw = ts == 2 ? 600 : 420;
    auto lines = ctx.font->wrap(keyText(pendingMessage), bw - 40, ts);
    int lh = 12 * ts + 1, h = 50 + (int)lines.size() * lh;
    h = std::min(h, SCREEN_H - 10);
    int x = (SCREEN_W - bw) / 2, y = (SCREEN_H - h) / 2;
    cv.dim(0, 0, SCREEN_W, SCREEN_H, 90); uiRect(0, 0, SCREEN_W, SCREEN_H);
    panelTitle(ctx, x, y, bw, h, pendingMessageTitle);
    int cy = y + 22; for (auto& l : lines) { if (cy + lh > y + h - 20) break; UI::text(ctx, x + 20, cy, l, C::White, ts); cy += lh; }
    textCenter(ctx, SCREEN_W / 2, y + h - 18, "[Enter]", C::Gray);
    (void)in;
}
// The pause menu (Esc). 1.1.3: also opens in combat (Party & Items and World Map are greyed out there: they would
// let you re-equip or travel mid-fight) and, in a playtest started from the Editor, offers Back to Editor.
std::vector<Game::PauseEntry> Game::pauseEntries() const {
    bool fight = inCombat();
    std::vector<PauseEntry> e = {{0, "Resume", true}, {1, "Party & Items", !fight}, {2, "Load Game", true},
        {3, "World Map", !fight && !worldMapSource().empty()}, {4, "Journal", true}, {5, "Settings", true}};
    if (playtest) e.push_back({7, "Back to Editor", true});
    e.push_back({6, "Exit to Desktop", true});
    return e;
}
void Game::openPause() { pauseSel = 0; pushMode(GMode::PAUSE); }
static int pauseH(int n) { return 30 + n * 26 + 24; }   // the buttons plus the "save at a shrine" line under them
static int pauseTop(int n) { return (SCREEN_H - 72 - pauseH(n)) / 2; }   // centred between the top of the screen and the party bar
void Game::updatePause(InputState& in, float dt) {
    (void)dt;
    auto entries = pauseEntries();
    const int n = (int)entries.size();
    pauseSel = clampi(pauseSel, 0, n - 1);
    if (actP(in, A_UP)) pauseSel = (pauseSel + n - 1) % n;
    if (actP(in, A_DOWN)) pauseSel = (pauseSel + 1) % n;
    int x = SCREEN_W / 2 - 90, y = pauseTop(n);
    for (int i = 0; i < n; i++) if (in.mouseX >= x && in.mouseX < x + 180 && in.mouseY >= y + 30 + i * 26 && in.mouseY < y + 52 + i * 26 && (in.mouseDX || in.mouseDY)) pauseSel = i;
    if (actP(in, A_CANCEL)) { popMode(); return; }
    if (actP(in, A_CONFIRM) || in.mousePressed[0]) {
        if (in.mousePressed[0]) { if (!(in.mouseX >= x && in.mouseX < x + 180 && in.mouseY >= y + 30 && in.mouseY < y + 30 + n * 26)) return; pauseSel = (in.mouseY - y - 30) / 26; }
        const PauseEntry& e = entries[clampi(pauseSel, 0, n - 1)];
        if (!e.enabled) {
            if (e.id == 3 && !inCombat()) log("There is no map of this place.");
            else if (inCombat()) log("Not during a fight.");
            gAudio.play("ui_error"); return;
        }
        switch (e.id) {
            case 0: popMode(); break;
            case 1: popMode(); menuTab = 0; menuSel = 0; menuScroll = 0; menuSubFocus = false; pushMode(GMode::MENU); break;
            case 2: popMode(); openSaveLoad(false); break;
            case 3: popMode(); openWorldMap(false); break;
            case 4: popMode(); jTab = 0; jSel = 0; jScroll = 0; pushMode(GMode::JOURNAL); break;
            case 5: popMode(); requestSettings = true; break;
            case 6: popMode(); requestDesktop = true; break;
            case 7: popMode(); requestEditor = true; break;
        }
    }
}
void Game::drawPause(Canvas& cv, InputState& in) {
    cv.dim(0, 0, SCREEN_W, SCREEN_H, 110);
    auto entries = pauseEntries();
    const int n = (int)entries.size();
    int x = SCREEN_W / 2 - 90, y = pauseTop(n);
    panelTitle(ctx, x, y, 180, pauseH(n), inCombat() ? "Paused (combat)" : "Paused");
    uiRect(x, y, 180, pauseH(n));
    for (int i = 0; i < n; i++) button(ctx, in, x + 12, y + 30 + i * 26, 156, 22, entries[i].label, entries[i].enabled, pauseSel == i);
    textCenter(ctx, SCREEN_W / 2, y + 30 + n * 26 + 6, "Save at any save shrine.", C::Gray);
}

// ---------------- in-game save / load ----------------
static std::string unescName(const std::string& s) { std::string o; for (size_t i = 0; i < s.size(); i++) { if (s[i] == '\\' && i + 1 < s.size()) { i++; o += s[i] == 'n' ? ' ' : s[i]; } else o += s[i]; } return o; }
SaveSlotInfo readSaveSlotInfo(const std::string& path) {
    SaveSlotInfo si; std::string t; if (!readFile(path, t)) return si;
    auto secs = parseConfig(t); if (secs.empty() || secs[0].name != "game") return si;
    si.exists = true; si.hero = secs[0].get("hero", "?"); si.campaign = secs[0].get("campaign", "triffall") == "triffall" ? "" : unescName(secs[0].get("campaignname", secs[0].get("campaign"))); si.level = secs[0].geti("level", 1); si.playtime = secs[0].geti("playtime", 0); si.mapName = secs[0].get("mapname", secs[0].get("map"));
    return si;
}
void Game::refreshSlots() { for (int i = 0; i < 3; i++) slots[i] = readSaveSlotInfo(slotPath(i)); }
void Game::openSaveLoad(bool save) { slMode = save; slSel = 0; slConfirm = -1; slStatus.clear(); refreshSlots(); pushMode(GMode::SAVELOAD); }
void Game::updateSaveLoad(InputState& in, float dt) {
    (void)dt;
    int x = SCREEN_W / 2 - 170, y = 110;
    if (slConfirm >= 0) {
        bool yes = false, no = false;
        if (actP(in, A_CONFIRM)) yes = true; if (actP(in, A_CANCEL)) no = true;
        if (in.mousePressed[0]) { if (in.mouseY >= y + 220 && in.mouseY < y + 242) { if (in.mouseX >= x + 60 && in.mouseX < x + 150) yes = true; else if (in.mouseX >= x + 190 && in.mouseX < x + 280) no = true; } }
        if (yes) { int slot = slConfirm; slConfirm = -1; if (slMode) saveToSlot(slot); else { if (loadFromSlot(slot)) return; } popMode(); }
        else if (no) slConfirm = -1;
        return;
    }
    if (actP(in, A_CANCEL)) { popMode(); return; }
    if (actP(in, A_UP)) slSel = (slSel + 2) % 3; if (actP(in, A_DOWN)) slSel = (slSel + 1) % 3;
    if (actP(in, A_LEFT) || actP(in, A_RIGHT) || actP(in, A_PAGEUP) || actP(in, A_PAGEDOWN)) { if (!slMode || canSaveHere()) slMode = !slMode; }
    for (int i = 0; i < 3; i++) if (in.mouseX >= x + 10 && in.mouseX < x + 330 && in.mouseY >= y + 50 + i * 50 && in.mouseY < y + 94 + i * 50 && (in.mouseDX || in.mouseDY)) slSel = i;
    bool go = actP(in, A_CONFIRM);
    if (in.mousePressed[0]) {
        if (in.mouseY >= y + 14 && in.mouseY < y + 34) { if (in.mouseX >= x + 60 && in.mouseX < x + 160 && canSaveHere()) slMode = true; else if (in.mouseX >= x + 180 && in.mouseX < x + 280) slMode = false; return; }
        for (int i = 0; i < 3; i++) if (in.mouseX >= x + 10 && in.mouseX < x + 330 && in.mouseY >= y + 50 + i * 50 && in.mouseY < y + 94 + i * 50) { slSel = i; go = true; }
        if (in.mouseY >= y + 220 && in.mouseY < y + 242 && in.mouseX >= x + 120 && in.mouseX < x + 220) { popMode(); return; }
    }
    if (go) {
        if (slMode) { if (slots[slSel].exists) slConfirm = slSel; else { saveToSlot(slSel); popMode(); } }
        else { if (!slots[slSel].exists) { slStatus = "That slot is empty."; return; } slConfirm = slSel; }
    }
}
void Game::drawSaveLoad(Canvas& cv, InputState& in) {
    cv.dim(0, 0, SCREEN_W, SCREEN_H, 120);
    int x = SCREEN_W / 2 - 170, y = 110, w = 340, h = 260;
    panelTitle(ctx, x, y, w, h, slMode ? "Save Game" : "Load Game");
    uiRect(x, y, w, h);
    button(ctx, in, x + 60, y + 14, 100, 20, "Save", canSaveHere(), slMode);
    button(ctx, in, x + 180, y + 14, 100, 20, "Load", true, !slMode);
    for (int i = 0; i < 3; i++) {
        int sy = y + 50 + i * 50; bool sel = slSel == i;
        cv.fillRect(x + 10, sy, w - 20, 44, sel ? RGB(90, 70, 20) : RGB(30, 30, 44)); cv.rect(x + 10, sy, w - 20, 44, sel ? C::Gold : C::DkGray);
        ctx.ts->draw(cv, slots[i].exists ? TL::UI_FLOPPY : TL::UI_BOX, x + 18, sy + 10, 2, slots[i].exists ? C::White : C::DkGray);
        if (slots[i].exists) { UI::text(ctx, x + 52, sy + 6, fmt("Slot %d: %s  (Lv %d)", i + 1, slots[i].hero.c_str(), slots[i].level), C::White); UI::text(ctx, x + 52, sy + 22, fmt("%s - %02d:%02d played", slots[i].mapName.c_str(), slots[i].playtime / 3600, (slots[i].playtime / 60) % 60), C::LtGray); }
        else UI::text(ctx, x + 52, sy + 14, fmt("Slot %d: <empty>", i + 1), C::Gray);
    }
    if (slConfirm >= 0) {
        cv.fillRect(x + 20, y + 190, w - 40, 60, RGB(20, 20, 30)); cv.rect(x + 20, y + 190, w - 40, 60, C::Gold);
        textCenter(ctx, SCREEN_W / 2, y + 198, slMode ? fmt("Overwrite slot %d?", slConfirm + 1) : "Discard unsaved progress and load?", C::Yellow);
        button(ctx, in, x + 60, y + 220, 90, 22, "Yes", true, true); button(ctx, in, x + 190, y + 220, 90, 22, "No", true, false);
    } else {
        button(ctx, in, x + 120, y + 220, 100, 22, "Back", true, false);
        if (!slStatus.empty()) textCenter(ctx, SCREEN_W / 2, y + 200, slStatus, C::Red);
    }
}

// ---------------- favorites & utility spells ----------------
bool Game::spellUsableOutside(const SpellDef& s) const {
    return !s.utility.empty() || s.heal > 0 || s.effect == "cure" || s.effect == "revive";
}
bool Game::castUtility(int mi, const std::string& spellId, int target) {
    Character& caster = party.members[mi];
    const SpellDef* s = gd->spell(spellId); if (!s) return false;
    if (!caster.alive) { log(caster.name + " is down."); return false; }
    if (caster.level < s->level) { log(fmt("%s needs level %d to cast %s.", caster.name.c_str(), s->level, s->name.c_str())); gAudio.play("ui_error"); return false; }
    if (caster.mp < s->cost) { log("Not enough MP."); return false; }
    int power = caster.stat(s->scaleStat, *gd);
    Character& t = party.members[clampi(target, 0, (int)party.members.size() - 1)];
    bool did = false;
    if (s->utility == "light") { lightSteps = 120 + power * 6; log("A soft light surrounds the party."); did = true; }
    else if (s->utility == "stealth") { stealthSteps = 80 + power * 4; log("The party moves unseen. Monsters will keep their distance for a while."); did = true; }
    else if (s->utility == "detect") { detectSteps = 60 + power * 3; int n = 0; for (auto& c : map->chests) if (!c.opened) n++; log(fmt("You sense %d unopened treasure%s on this map. Chests now glow through the dark.", n, n == 1 ? "" : "s")); did = true; }
    else if (s->utility == "unlock") {
        bool any = false;
        for (int d = 0; d < 4; d++) { int nx = px + (d == 0) - (d == 1), ny = py + (d == 2) - (d == 3);
            if (Door* dr = map->doorAt(nx, ny)) if (!dr->open) { dr->open = true; if (!dr->flag.empty()) setFlag(dr->flag); map->at(dr->x, dr->y).walkable = true; map->at(dr->x, dr->y).blocksSight = false; map->at(dr->x, dr->y).prop = dr->openTile; any = true; }
            if (Chest* c = map->chestAt(nx, ny)) if (!c->locked.empty()) { c->locked.clear(); any = true; } }
        if (!any) { log("There is nothing locked next to you."); return false; }
        log("The lock clicks open."); revealAround(); did = true;
    }
    else if (s->utility == "recall") {
        if (lastTownMap.empty() || mapId == lastTownMap) { log("There is nowhere to recall to from here."); return false; }
        if (mode == GMode::MENU) popMode();
        startTransition(lastTownMap, lastTownX, lastTownY, "Recalling to " + lastTownMap + "..."); log("The world folds and you are elsewhere."); did = true;
    }
    if (s->effect == "revive") { if (!t.alive) { t.alive = true; t.hp = std::max(1, t.maxHP(*gd) / 3); log(t.name + " is revived."); did = true; } else { log(t.name + " is fine."); if (!did) return false; } }
    else if (t.alive) {
        if (s->heal > 0) { int before = t.hp; t.hp = std::min(t.maxHP(*gd), t.hp + s->heal + (power - 8)); log(fmt("%s heals %s for %d.", caster.name.c_str(), t.name.c_str(), t.hp - before)); did = true; }
        if (s->effect == "cure") { t.removeEffect("poison"); t.removeEffect("burn"); t.removeEffect("weaken"); log(t.name + " is cured."); did = true; }
    }
    if (did) { caster.mp -= s->cost; gAudio.play(!s->sound.empty() ? s->sound : s->utility == "light" ? "light" : s->utility == "stealth" ? "stealth" : s->utility == "detect" ? "detect" : s->utility == "unlock" ? "unlock" : s->utility == "recall" ? "teleport" : spellSound(*s)); }
    return did;
}
void Game::useFavorite(int mi, int slot) {
    if (mi < 0 || mi >= (int)party.members.size()) return;
    Character& m = party.members[mi];
    std::string f = m.favs[slot];
    if (f.empty()) { log(fmt("Favorite %d is empty. Assign one from the Spells or Items menu.", slot + 1)); return; }
    std::string kind = f.substr(0, f.find(':')), id = f.substr(f.find(':') + 1);
    if (kind == "spell") {
        const SpellDef* s = gd->spell(id); if (!s) return;
        if (!spellUsableOutside(*s)) { log(s->name + " can only be used in combat."); return; }
        if (s->target == "self" || !s->utility.empty()) { castUtility(mi, id, mi); return; }
        // needs a target: open the spell menu on that spell
        menuTab = 2; menuMember = mi; menuSel = 0; spellCol = 1; for (size_t i = 0; i < m.spells.size(); i++) if (m.spells[i] == id) menuSel = (int)i;
        menuSubFocus = true; menuSub = 0; pushMode(GMode::MENU); log("Choose who receives " + s->name + ".");
    } else {
        const ItemDef* d = gd->item(id); if (!d) return;
        if (!party.hasItem(id)) { log("You have no " + d->name + " left."); return; }
        if (d->type != IT_CONSUMABLE || d->dmgMax > 0 || !d->useSpell.empty()) { log(d->name + " can only be used in combat."); return; }
        menuTab = 1; menuSel = 0; for (size_t i = 0; i < party.inventory.size(); i++) if (party.inventory[i].id == id) menuSel = (int)i;
        menuSubFocus = true; menuSub = 0; menuPrompt = "use"; menuMember = 0; pushMode(GMode::MENU); log("Choose who uses " + d->name + ".");
    }
}
void Game::reloadContent() {
    // re-read every data file of the active campaign (the editor saved something), then re-enter the current map
    std::string keep = mapId; int kx = px, ky = py;
    stashExplored();
    std::string id = campaignId; campaignId.clear(); switchCampaign(id);
    restoreDynItems();
    maps.clear(); map = nullptr;
    if (running && !keep.empty()) { enterMap(keep, kx, ky, false); }
}

// ---------------- drawing ----------------
// 1.0.15: no outlines. The ground is black again (as the sheet is drawn) and sprites get their own black inside their
// outline (Tileset::drawBacking), so a character on a trail or floor reads as it does on the sheet.
void Game::drawUnitSprite(Canvas& cv, const Character& ch, int sx, int sy, bool flip) {
    if (ch.size == 2) {
        for (int yy = 0; yy < 2; yy++) for (int xx = 0; xx < 2; xx++) {
            Tile t = T(ch.tile.c + xx, ch.tile.r + yy);
            ctx.ts->drawBacking(cv, t, sx + xx * TILE, sy + yy * TILE, 2);
            if (ch.recolor) ctx.ts->drawRecolor(cv, t, sx + xx * TILE, sy + yy * TILE, 2, ch.color); else ctx.ts->draw(cv, t, sx + xx * TILE, sy + yy * TILE, 2);
        }
        return;
    }
    ctx.ts->drawBacking(cv, ch.tile, sx, sy, 2, flip && !ch.recolor);
    if (ch.recolor) ctx.ts->drawRecolor(cv, ch.tile, sx, sy, 2, ch.color); else ctx.ts->drawLit(cv, ch.tile, sx, sy, 2, flip);
}

void Game::drawWorld(Canvas& cv, bool combat) {
    if (!map) return;
    int viewH = SCREEN_H - 72;
    cv.setClip({0, 0, SCREEN_W, viewH});
    cv.fillRect(0, 0, SCREEN_W, viewH, map->bg);
    int shx = 0, shy = 0;
    if (shake > 0 && settings && settings->screenShake) { shx = rng.range(-3, 3); shy = rng.range(-3, 3); }
    int cx = camX - shx, cy = camY - shy;
    int x0 = std::max(0, cx / TILE), y0 = std::max(0, cy / TILE);
    int x1 = std::min(map->w - 1, (cx + SCREEN_W) / TILE + 1), y1 = std::min(map->h - 1, (cy + viewH) / TILE + 1);
    bool anim = (ctx.frame / 20) % 2 == 0;
    const auto& kinds = allTileKinds();
    // 1.0.15: three layers - ground, then the shifted trail pieces (moved down a few pixels as one layer, running into
    // the cell below unless that is a wall), then props - so the trail lies under people's feet and trees still overlap it.
    struct Later { const TileKind* k; Tile g; int sx, sy, mask, corners; Color8 tint; bool recolor, opaque, spill; };
    struct Prop { Tile p; int sx, sy; Color8 t; };
    std::vector<Later> trails; std::vector<Prop> props;
    // 1.1.0: a resource node with its own picture (resources.txt sprite=) replaces its cell's picture while it is in view
    std::set<int> nodeCells; for (auto& gn : map->gathers) if (gn.sprite.valid() && map->inBounds(gn.x, gn.y) && visible(gn.x, gn.y)) nodeCells.insert(gn.y * map->w + gn.x);
    for (int y = y0; y <= y1; y++) for (int x = x0; x <= x1; x++) {
        const Cell& c = map->at(x, y);
        if (map->dark && !c.explored) continue;
        bool vis = visible(x, y);
        int sx = x * TILE - cx, sy = y * TILE - cy;
        Color8 tint = c.tint;
        Color8 bg; Tile g; int mask = 0, corners = 0; map->look(x, y, g, bg, &mask, &corners);   // joined pieces (paths, shores, rivers...) pick their piece here
        if (map->dark && !vis) { tint = RGB(tint.r / 3, tint.g / 3, tint.b / 3); bg = RGB(bg.r / 3, bg.g / 3, bg.b / 3); }
        else if (map->dark && !lit(x, y)) { tint = RGB(tint.r * 3 / 5, tint.g * 3 / 5, tint.b * 3 / 5); bg = RGB(bg.r / 2, bg.g / 2, bg.b / 2); }  // seen, but standing in shadow
        cv.fillRect(sx, sy, TILE, TILE, bg);
        if (!nodeCells.empty() && nodeCells.count(y * map->w + x)) continue;
        const TileKind* k = c.kind < kinds.size() ? &kinds[c.kind] : nullptr;
        if (k && k->shift > 0) trails.push_back({k, g, sx, sy, mask, corners, tint, c.recolor, c.opaque, y + 1 >= map->h || !map->at(x, y + 1).opaque});
        else drawGroundPiece(cv, *ctx.ts, k, g, sx, sy, 2, tint, c.recolor, c.opaque, mask, corners);
        if (c.prop.valid()) {
            Tile p = c.prop;
            if (anim && p.c == TL::BRAZIER.c && p.r == TL::BRAZIER.r) p = TL::BRAZIER2;   // the lit brazier flickers
            Color8 pt = (map->dark && !vis) ? RGB(80, 80, 80) : C::White;
            props.push_back({p, sx, sy, pt});
        }
    }
    for (auto& t : trails) drawGroundPiece(cv, *ctx.ts, t.k, t.g, t.sx, t.sy, 2, t.tint, t.recolor, t.opaque, t.mask, t.corners, t.spill);
    for (auto& p : props) ctx.ts->draw(cv, p.p, p.sx, p.sy, 2, p.t);
    // objects
    for (auto& ch : map->chests) {
        bool detect = detectSteps > 0 && !ch.opened;
        if (!detect && !visible(ch.x, ch.y) && !(map->dark && map->at(ch.x, ch.y).explored)) continue;
        if (detect && (ctx.frame / 10) % 2) cv.rect(ch.x * TILE - cx - 1, ch.y * TILE - cy - 1, TILE + 2, TILE + 2, C::Yellow);
        // 1.1.1: pictures and colour from the chest's type / template / own keys (objects.txt), else the built-in chest
        Tile pic = ch.opened ? (ch.openSprite.valid() ? ch.openSprite : ch.sprite.valid() ? ch.sprite : TL::CHEST_OPEN) : (ch.sprite.valid() ? ch.sprite : TL::CHEST);
        ctx.ts->drawBacking(cv, pic, ch.x * TILE - cx, ch.y * TILE - cy, 2);
        if (ch.hasColor) ctx.ts->drawRecolor(cv, pic, ch.x * TILE - cx, ch.y * TILE - cy, 2, ch.color); else ctx.ts->draw(cv, pic, ch.x * TILE - cx, ch.y * TILE - cy, 2);
    }
    for (auto& s : map->signs) if (visible(s.x, s.y)) { Tile pic = s.sprite.valid() ? s.sprite : TL::SIGN; ctx.ts->drawBacking(cv, pic, s.x * TILE - cx, s.y * TILE - cy, 2);
        if (s.hasColor) ctx.ts->drawRecolor(cv, pic, s.x * TILE - cx, s.y * TILE - cy, 2, s.color); else ctx.ts->draw(cv, pic, s.x * TILE - cx, s.y * TILE - cy, 2); }
    for (auto& s : map->savepoints) {
        if (!visible(s.x, s.y)) continue;
        int sx = s.x * TILE - cx, sy = s.y * TILE - cy;
        Tile pic = s.sprite.valid() ? s.sprite : TL::ALTAR;
        if (s.hasColor) { ctx.ts->drawBacking(cv, pic, sx, sy, 2); ctx.ts->drawRecolor(cv, pic, sx, sy, 2, s.color); } else ctx.ts->draw(cv, pic, sx, sy, 2);
        if (s.sparkle && (ctx.frame / 12) % 2) ctx.ts->draw(cv, TL::SPARKLE, sx, sy - 8, 2, s.sparkleColor);
    }
    for (auto& g : map->gathers) {
        if (!visible(g.x, g.y) || !map->at(g.x, g.y).explored) continue;
        int sx = g.x * TILE - cx, sy = g.y * TILE - cy;
        // the node's own tile drawn again in a colour that says what it is (ore copper, herbs green, tech cyan...)
        // while it can be harvested, and in dull grey once it has been picked clean
        bool ready = gatherReady(g);
        // 1.1.0: colours and picture come from the node's resource type (resources.txt), else guessed from the item
        Color8 col = g.hasColor ? g.color : gatherGuessColor(g.item);
        if (!ready) col = g.spent;
        const Cell& cell = map->at(g.x, g.y);
        Tile pic = g.sprite; if (!ready && g.spentSprite.valid()) pic = g.spentSprite;
        if (pic.valid()) { ctx.ts->drawBacking(cv, pic, sx, sy, 2); ctx.ts->drawRecolor(cv, pic, sx, sy, 2, col); }
        else if (cell.prop.valid()) ctx.ts->drawRecolor(cv, cell.prop, sx, sy, 2, col); else if (cell.ground.valid()) ctx.ts->drawRecolor(cv, cell.ground, sx, sy, 2, col);
        if (ready && (ctx.frame / 15) % 3 == 0) ctx.ts->draw(cv, TL::SPARKLE, sx, sy - 6, 2, C::Yellow);
    }
    if (fuseT > 0 && fuseMap == mapId) {
        int sx = fuseX * TILE - cx, sy = fuseY * TILE - cy;
        if ((ctx.frame / 4) % 2) cv.rect(sx - 2 * TILE, sy - 2 * TILE, 5 * TILE, 5 * TILE, C::Red);
        ctx.font->drawShadow(cv, *ctx.ts, sx, sy - 14, fmt("%.1f", fuseT), C::Orange, C::Black);
    }
    for (auto& d : map->drops) if (visible(d.x, d.y)) { const ItemDef* it = gd->item(d.item); Tile t = it ? it->tile : TL::IT_COINS; ctx.ts->drawBacking(cv, t, d.x * TILE - cx, d.y * TILE - cy, 2); ctx.ts->draw(cv, t, d.x * TILE - cx, d.y * TILE - cy, 2); }
    if (!combat) {
        for (auto& n : map->npcs) {
            if (n.hidden || !visible(n.x, n.y)) continue;
            int sx = n.x * TILE - cx, sy = n.y * TILE - cy;
            ctx.ts->drawBacking(cv, n.tile, sx, sy, 2);
            if (n.recolor) ctx.ts->drawRecolor(cv, n.tile, sx, sy, 2, n.color); else ctx.ts->drawLit(cv, n.tile, sx, sy, 2);
            if (!n.dialogue.empty() && chebyshev(n.x, n.y, px, py) <= 1) ctx.ts->draw(cv, TL::UI_QUESTION, sx + 8, sy - 16, 1);
        }
        for (auto& e : map->enemies) {
            if (e.defeated || !visible(e.x, e.y)) continue;
            if (!e.showFlag.empty() && !evalCond(e.showFlag)) continue;
            if (!e.hideFlag.empty() && evalCond(e.hideFlag)) continue;
            const CreatureDef* cd = e.creatures.empty() ? nullptr : gd->creature(e.creatures[0]);
            int sx = e.x * TILE - cx, sy = e.y * TILE - cy;
            cv.fillRect(sx + 1, sy + 1, TILE - 2, TILE - 2, RGB(60, 0, 0, 150)); cv.rect(sx, sy, TILE, TILE, RGB(220, 40, 40));
            if (cd) { if (cd->size == 2) { for (int yy = 0; yy < 2; yy++) for (int xx = 0; xx < 2; xx++) { ctx.ts->drawBacking(cv, T(cd->tile.c + xx, cd->tile.r + yy), sx + xx * TILE - 12, sy + yy * TILE - 12, 2); ctx.ts->draw(cv, T(cd->tile.c + xx, cd->tile.r + yy), sx + xx * TILE - 12, sy + yy * TILE - 12, 2); } }
                      else { ctx.ts->drawBacking(cv, cd->tile, sx, sy, 2); if (cd->recolor) ctx.ts->drawRecolor(cv, cd->tile, sx, sy, 2, cd->color); else ctx.ts->draw(cv, cd->tile, sx, sy, 2); } }
            if (e.creatures.size() > 1) { ctx.font->draw(cv, *ctx.ts, sx + 16, sy - 4, "x" + std::to_string(e.creatures.size()), C::Red); }
        }
        // party trail
        // the leader walks in front; everyone else follows the trail in party order (a downed leader hands over to the next living member)
        int ld = activeLeader();
        { int slot = 0; std::vector<int> order = followOrder();
          for (int k = (int)order.size() - 1; k >= 0; k--) { int i = order[k]; if (k >= (int)trail.size()) continue; auto p = trail[k]; if (!party.members[i].alive) continue; drawUnitSprite(cv, party.members[i], p.first * TILE - cx, p.second * TILE - cy, faceRight); } (void)slot; }
        if (!party.members.empty()) drawUnitSprite(cv, party.members[ld], px * TILE - cx, py * TILE - cy, faceRight);
    }
    for (auto& f : floats) {
        float a = std::min(1.0f, f.life);
        Color8 c = f.color; c.a = (uint8_t)(255 * a);
        int fw = ctx.font->width(f.text, f.scale);
        if (f.scale > 1) { Color8 o = RGB(0, 0, 0, c.a); for (int k = 0; k < 4; k++) ctx.font->draw(cv, *ctx.ts, (int)f.x - cx - fw / 2 + (k == 0) - (k == 1), (int)f.y - cy + (k == 2) - (k == 3), f.text, o, f.scale); ctx.font->draw(cv, *ctx.ts, (int)f.x - cx - fw / 2, (int)f.y - cy, f.text, c, f.scale); }
        else ctx.font->drawShadow(cv, *ctx.ts, (int)f.x - cx - fw / 2, (int)f.y - cy, f.text, c, RGB(0, 0, 0, c.a));
    }
    cv.resetClip();
}

void Game::drawHUD(Canvas& cv, InputState& in) {
    int y = SCREEN_H - 72;
    uiRect(0, y, SCREEN_W, 72);
    cv.fillRect(0, y, SCREEN_W, 72, RGB(18, 16, 24));
    cv.hline(0, y, SCREEN_W, C::Gold); cv.hline(0, y + 1, SCREEN_W, RGB(120, 90, 30));
    int n = (int)party.members.size();
    int slotW = n > 0 ? std::min(150, (SCREEN_W - 6 - 136) / n) : 150;   // keep clear of the gold/place/clock column on the right
    int pw = slotW - 6;
    for (int i = 0; i < n; i++) {
        const Character* mp = &party.members[i];
        if (mode == GMode::COMBAT) for (auto& u : cb.units) if (u.partyIndex == i) mp = &u.ch;
        const Character& m = *mp;
        int x = 6 + i * slotW;
        cv.fillRect(x, y + 6, pw, 60, m.alive ? RGB(30, 28, 40) : RGB(40, 20, 20));
        cv.rect(x, y + 6, pw, 60, mode == GMode::COMBAT && cb.selectedUnit >= 0 && cb.units[cb.selectedUnit].partyIndex == i ? C::Yellow : C::DkGray);
        cv.fillRect(x + 3, y + 9, 30, 30, RGB(50, 50, 70));
        if (m.alive) drawUnitSprite(cv, m, x + 6, y + 12, false); else ctx.ts->draw(cv, TL::UI_SKULL, x + 9, y + 15, 2);
        { std::string nm = m.name; while (nm.size() > 1 && ctx.font->width(nm) > pw - 42) nm.pop_back(); ctx.font->draw(cv, *ctx.ts, x + 38, y + 8, nm, C::White); }
        ctx.font->draw(cv, *ctx.ts, x + 38, y + 20, fmt("Lv%d", m.level), C::Gray);
        bar(ctx, x + 38, y + 34, pw - 44, 8, m.hp, m.maxHP(*gd), m.hp < m.maxHP(*gd) / 4 ? C::Red : RGB(60, 200, 60));
        ctx.font->draw(cv, *ctx.ts, x + 38, y + 43, fmt("%d/%d", m.hp, m.maxHP(*gd)), C::LtGray);
        if (m.maxMP(*gd) > 0) { int hw = ctx.font->width(fmt("%d/%d", m.hp, m.maxHP(*gd))) + 4; bar(ctx, x + 38 + hw, y + 44, pw - 44 - hw, 6, m.mp, m.maxMP(*gd), RGB(80, 120, 255)); }
        if (mode == GMode::COMBAT) { ctx.font->draw(cv, *ctx.ts, x + 3, y + 42, fmt("AP%d", m.ap), C::Yellow); ctx.font->draw(cv, *ctx.ts, x + 3, y + 53, fmt("MV%d", m.move), C::Cyan); }
        int ex = x + 38; for (auto& e : m.effects) { if (ex + 24 > x + pw) break; ctx.font->draw(cv, *ctx.ts, ex, y + 55, e.id.substr(0, 4), e.id == "poison" || e.id == "burn" || e.id == "freeze" || e.id == "weaken" || e.id == "stun" ? C::Red : C::Green); ex += 28; }
    }
    // right side: gold, location
    int rx = SCREEN_W - 6;
    textRight(ctx, rx, y + 8, fmt("%d gold", party.gold), C::Gold);
    if (map) textRight(ctx, rx, y + 22, map->name, C::LtGray);
    if (lightSteps <= 0 && stealthSteps <= 0 && detectSteps <= 0) textRight(ctx, rx, y + 36, clockText() + (outdoors() && weather != "clear" ? "  " + weather : ""), nightAmount() > 0.5f ? RGB(140, 150, 255) : C::Gray);
    if (mode == GMode::EXPLORE) {
        if (map && map->saveAt(px, py)) { textRight(ctx, rx, y + 52, "[" + gKeys.label(A_QUICKSAVE) + "] Save here", C::Cyan); }
        else textRight(ctx, rx, y + 52, "[" + gKeys.label(A_MENU) + "] Menu  [" + gKeys.label(A_INTERACT) + "] Act", C::DkGray);
        std::string eff; if (lightSteps > 0) eff += fmt("light %d  ", lightSteps); if (stealthSteps > 0) eff += fmt("stealth %d  ", stealthSteps); if (detectSteps > 0) eff += fmt("sense %d", detectSteps);
        if (!eff.empty()) textRight(ctx, rx, y + 36, eff, C::Cyan);
    }
    (void)in;
}

static const int HB_SLOT = 28, HB_X = UI::SCREEN_W - 8 * HB_SLOT - 8;
void Game::drawLog(Canvas& cv) {
    int shown = 0;
    for (int i = (int)msgLog.size() - 1; i >= 0 && shown < 4; i--) {
        if (msgTime[i] <= 0 && mode != GMode::COMBAT) continue;
        Color8 c = C::White; if (msgTime[i] < 1) c.a = (uint8_t)std::max(0.0f, msgTime[i] * 255);
        std::string line = msgLog[i];   // keep clear of the favorites hotbar on the right
        if (ctx.font->width(line) > HB_X - 16) { while (line.size() > 3 && ctx.font->width(line + "...") > HB_X - 16) line.pop_back(); line += "..."; }
        ctx.font->drawShadow(cv, *ctx.ts, 8, SCREEN_H - 72 - 16 - shown * 13, line, c, RGB(0, 0, 0, c.a));
        shown++;
    }
}

void Game::draw(Canvas& cv, InputState& in) {
    ctx.cv = &cv;
    if (!map) { cv.clear(C::Black); return; }
    if (mode == GMode::CUTSCENE) { drawCutscene(cv, in); return; }
    if (mode == GMode::GAMEOVER) { drawGameOver(cv, in); return; }
    if (mode == GMode::ENDING) { drawEnding(cv, in); return; }
    if (mode == GMode::TRANSITION) {
        drawWorld(cv, false); drawWeather(cv, frameDt); drawHUD(cv, in);
        float t = transition; uint8_t a = (uint8_t)(255 * (t < 1 ? t : (2 - t)));
        cv.dim(0, 0, SCREEN_W, SCREEN_H, a);
        if (!transText.empty()) textCenter(ctx, SCREEN_W / 2, SCREEN_H / 2, transText, C::White);
        return;
    }
    drawWorld(cv, mode == GMode::COMBAT || (!modeStack.empty() && modeStack.back() == GMode::COMBAT));
    drawWeather(cv, frameDt);
    if (mode == GMode::COMBAT || (!modeStack.empty() && std::find(modeStack.begin(), modeStack.end(), GMode::COMBAT) != modeStack.end())) drawCombat(cv, in);
    drawBanter(cv);
    drawHUD(cv, in);
    drawLog(cv);
    switch (mode) {
        case GMode::DIALOGUE: drawDialogue(cv, in); break;
        case GMode::MENU: drawMenu(cv, in); break;
        case GMode::SHOP: drawShop(cv, in); break;
        case GMode::MESSAGE: drawMessage(cv, in); break;
        case GMode::LEVELUP: drawLevelUp(cv, in); break;
        case GMode::NAME: drawName(cv, in); break;
        case GMode::VICTORY: drawVictory(cv, in); break;
        case GMode::PAUSE: drawPause(cv, in); break;
        case GMode::SAVELOAD: drawSaveLoad(cv, in); break;
        case GMode::JOURNAL: drawJournal(cv, in); break;
        case GMode::WORLDMAP: drawWorldMap(cv, in); break;
        case GMode::CRAFT: drawCraft(cv, in); break;
        case GMode::CONSOLE: drawConsole(cv, in); break;
        case GMode::BOUNTY: drawBounty(cv, in); break;
        default: break;
    }
    if (mode == GMode::EXPLORE && !party.members.empty()) drawHotbar(cv, in, activeLeader(), SCREEN_H - 72 - 30);
    // notifications (top center, below the combat bar)
    bool fullScreenUI = mode == GMode::MENU || mode == GMode::SHOP || mode == GMode::CRAFT || mode == GMode::JOURNAL || mode == GMode::BOUNTY || mode == GMode::LEVELUP || mode == GMode::WORLDMAP;
    int ny = (mode == GMode::COMBAT) ? 30 : fullScreenUI ? SCREEN_H - 120 : 8;
    if (mode == GMode::COMBAT) { drawHint(cv, -(SCREEN_H - 72 - 34)); }   // in a fight: just above the hotbar and the log, clear of the action bar
    else if (fullScreenUI) { int hh = drawHint(cv, -(SCREEN_H - 58)); if (hh) ny = SCREEN_H - 58 - hh - 18 * std::min<int>(3, (int)notifications.size()); }   // over menus: near the bottom, notifications above it
    else ny += drawHint(cv, ny);
    for (size_t i = 0; i < notifications.size() && i < 3; i++) {
        float a = std::min(1.0f, notifTime[i]);
        Color8 c = C::Cream; c.a = (uint8_t)(255 * a);
        int w = ctx.font->width(notifications[i]) + 20;
        cv.fillRect(SCREEN_W / 2 - w / 2, ny + (int)i * 18, w, 16, RGB(0, 0, 0, (uint8_t)(160 * a)));
        ctx.font->draw(cv, *ctx.ts, SCREEN_W / 2 - w / 2 + 10, ny + 2 + (int)i * 18, notifications[i], c);
    }
    if (fadeIn > 0) cv.dim(0, 0, SCREEN_W, SCREEN_H, (uint8_t)(std::min(1.0f, fadeIn / 0.6f) * 255));
    // mouse cursor (the platform hides the OS cursor)
    if (!in.gamepadActive && in.mouseInWindow) {
        int mx = in.mouseX, my = in.mouseY;
        static const char* cur[] = {"X........", "XX.......", "XOX......", "XOOX.....", "XOOOX....", "XOOOOX...", "XOOOOOX..", "XOOOOOOX.", "XOOOOXXXX", "XOXXOX...", "XX.XOOX..", "...XOOX..", "....XX..."};
        for (int y = 0; y < 13; y++) for (int x = 0; x < 9; x++) { char ch = cur[y][x]; if (ch == 'X') cv.put(mx + x, my + y, C::Black); else if (ch == 'O') cv.put(mx + x, my + y, mode == GMode::COMBAT ? C::Yellow : C::White); }
    }
}

void Game::drawGameOver(Canvas& cv, InputState& in) {
    cv.clear(RGB(20, 0, 0));
    ctx.font->bold = true; textCenter(ctx, SCREEN_W / 2, 160, "You have fallen.", C::Red, 2); ctx.font->bold = false;
    textCenter(ctx, SCREEN_W / 2, 220, "The Rift takes another.", C::Gray);
    textCenter(ctx, SCREEN_W / 2, 300, "[Enter] Return to desktop (load a save from there)", C::LtGray);
    (void)in;
}
void Game::drawVictory(Canvas& cv, InputState& in) {
    cv.dim(0, 0, SCREEN_W, SCREEN_H - 72, 100);
    int x = SCREEN_W / 2 - 160, y = 90, h = 150 + (int)cb.itemsGained.size() * 13;
    panelTitle(ctx, x, y, 320, h, "Victory!");
    UI::text(ctx, x + 20, y + 24, fmt(party.members.size() > 1 ? "Experience: %d each" : "Experience: %d", cb.xpGained), C::White);
    UI::text(ctx, x + 20, y + 38, fmt("Gold: %d", cb.goldGained), C::Gold);
    int cy = y + 56;
    if (!cb.itemsGained.empty()) UI::text(ctx, x + 20, cy, "Loot:", C::LtGray), cy += 13;
    for (auto& s : cb.itemsGained) { UI::text(ctx, x + 32, cy, s, C::Cream); cy += 13; }
    textCenter(ctx, SCREEN_W / 2, y + h - 20, "[Enter]", C::Gray);
    (void)in;
}

// ---------------- quests / encounters ----------------
bool Game::loadQuests() {
    for (auto& sec : mergedConfig("quests.txt")) {
        if (sec.name != "quest") continue;
        QuestDef q; q.id = sec.get("id"); q.name = sec.get("name", q.id);
        for (auto& kv : sec.kv) if (kv.first == "stage") { auto p = split(kv.second, '|'); if (p.size() == 2) q.stages.push_back({trim(p[0]), trim(p[1])}); }
        for (auto& kv : sec.kv) if (kv.first == "fail") { auto p = split(kv.second, '|'); q.fails.push_back({trim(p[0]), p.size() > 1 ? trim(p[1]) : "The chance has passed."}); }
        for (auto& kv : sec.kv) if (kv.first == "objective") { auto p = split(kv.second, '|'); if (p.size() >= 3) { QuestObjective o; o.stage = atoi(trim(p[0]).c_str()); o.cond = trim(p[1]); o.text = trim(p[2]); for (size_t i = 3; i < p.size(); i++) o.text += "|" + p[i]; q.objectives.push_back(o); } }
        bool dup = false; for (auto& o : quests) if (o.id == q.id) { o = q; dup = true; } if (!dup) quests.push_back(q);
    }
    return true;
}
int Game::questStageIndex(const QuestDef& q) const { int k = 0; for (size_t i = 0; i < q.stages.size(); i++) if (evalCond(q.stages[i].first)) k = (int)i + 1; return k; }
std::string Game::questCounters(const std::string& text) const {
    std::string o; size_t i = 0;
    while (i < text.size()) {
        size_t a = text.find('{', i); if (a == std::string::npos) { o += text.substr(i); break; }
        size_t b = text.find('}', a); if (b == std::string::npos) { o += text.substr(i); break; }
        o += text.substr(i, a - i); std::string t = text.substr(a + 1, b - a - 1), val = "{" + t + "}";
        if (startsWith(t, "itemn:")) val = std::to_string(party.countItem(t.substr(6)));
        else if (startsWith(t, "kills:")) { auto it = bestiary.find(t.substr(6)); val = std::to_string(it == bestiary.end() ? 0 : it->second); }
        else if (startsWith(t, "flag:")) { auto it = flags.find(t.substr(5)); val = std::to_string(it == flags.end() ? 0 : it->second); }
        else if (t == "gold") val = std::to_string(party.gold);
        o += val; i = b + 1;
    }
    return o;
}
std::string Game::questStatus(const QuestDef& q) const {
    std::string s; for (auto& st : q.stages) if (evalCond(st.first)) s = st.second;
    if (s.empty() || startsWith(s, "DONE")) return s;               // finishing wins over failing
    for (auto& f : q.fails) if (!f.first.empty() && evalCond(f.first)) return "FAILED: " + f.second;
    return s;
}
bool Game::questFailed(const QuestDef& q) const { return startsWith(questStatus(q), "FAILED"); }
bool Game::questClosed(const QuestDef& q) const { std::string s = questStatus(q); return startsWith(s, "DONE") || startsWith(s, "FAILED"); }
std::string Game::factionName(const std::string& id) const { for (auto& f : factions) if (f.id == id) return f.name; for (auto& b : boards) if (b.id == id) return b.name; std::string n = id; if (!n.empty()) n[0] = (char)toupper((unsigned char)n[0]); return n; }
const char* Game::repTier(int r) { return r <= -50 ? "Hated" : r <= -10 ? "Disliked" : r < 10 ? "Neutral" : r < 30 ? "Liked" : r < 60 ? "Friendly" : "Honoured"; }
void Game::changeRep(const std::string& id, int d) {
    if (id.empty() || d == 0) return; int before = reputation[id]; int& r = reputation[id]; r = clampi(r + d, -100, 100);
    notify(fmt("%s reputation %+d", factionName(id).c_str(), d)); if (strcmp(repTier(before), repTier(r)) != 0) log(factionName(id) + " now sees you as " + repTier(r) + ".");
}
bool Game::loadEncounters() {
    for (auto& sec : mergedConfig("encounters.txt")) {
        if (sec.name != "table") continue;
        EncounterTable e; e.id = sec.get("id"); e.minLevel = sec.geti("level", 1);
        for (auto& kv : sec.kv) if (kv.first == "group") { std::vector<std::string> g; for (auto& c : split(kv.second, ',')) if (!trim(c).empty()) g.push_back(trim(c)); if (!g.empty()) e.groups.push_back(g); }
        encounters[e.id] = e;
    }
    return true;
}

// ---------------- items / equipment ----------------
// reputation with the open shop's faction: up to 15% off when Honoured, up to 25% more when Hated (sell: +10% / -20%)
static int repPct(const Game& g, bool buy) { if (g.mode != GMode::SHOP || g.shopFaction.empty()) return 0; auto it = g.reputation.find(g.shopFaction); int r = it == g.reputation.end() ? 0 : it->second; return buy ? clampi(-r / 4, -15, 25) : clampi(r / 6, -20, 10); }
int Game::itemBuyPrice(const ItemDef& d) const { int cha = party.members.empty() ? 8 : party.members[0].stat(ST_CHA, *gd); return std::max(1, d.value * (110 - std::min(30, cha * 2) + repPct(*this, true)) / 100); }
int Game::itemSellPrice(const ItemDef& d) const { int cha = party.members.empty() ? 8 : party.members[0].stat(ST_CHA, *gd); return std::max(d.value > 0 ? 1 : 0, d.value * (18 + std::min(17, cha)) * (100 + repPct(*this, false)) / 10000); }

std::string Game::shopFactionOf(const std::string& shop) const {
    auto secs = mergedConfig("shops.txt"); std::reverse(secs.begin(), secs.end());
    for (auto& sec : secs) if (sec.name == "shop" && sec.get("id") == shop) return sec.get("faction");
    return "";
}
std::vector<std::pair<std::string, int>> Game::shopItems(const std::string& shop) {
    std::vector<std::pair<std::string, int>> out;
    auto secs = mergedConfig("shops.txt"); std::reverse(secs.begin(), secs.end());
    for (auto& sec : secs) {
        if (sec.name != "shop" || sec.get("id") != shop) continue;
        // item=id[:stock[:rep]] - with rep, only shown once the party's standing with the shop's faction reaches it
        std::string fac = sec.get("faction");
        for (auto& kv : sec.kv) if (kv.first == "item") { auto p = split(kv.second, ':'); if (p.size() > 2 && !trim(p[2]).empty() && repOf(fac) < atoi(p[2].c_str())) continue;
            if (gd->item(trim(p[0]))) out.push_back({trim(p[0]), p.size() > 1 && !trim(p[1]).empty() ? atoi(p[1].c_str()) : -1}); }
        break;   // the campaign's shop (if any) replaces the base one
    }
    return out;
}

bool Game::useItemOn(const std::string& itemId, Character& target, bool inCombat) {
    const ItemDef* d = gd->item(itemId); if (!d || d->type != IT_CONSUMABLE) return false;
    if (!target.alive && d->effect != "revive") { log(target.name + " cannot use that now."); return false; }
    bool used = false;
    if (d->effect == "revive") { if (target.alive) { log(target.name + " is not down."); return false; } target.alive = true; target.hp = std::max(1, target.maxHP(*gd) / 2); log(target.name + " is revived!"); used = true; }
    if (d->heal > 0) { int before = target.hp; target.hp = std::min(target.maxHP(*gd), target.hp + d->heal); log(fmt("%s recovers %d HP.", target.name.c_str(), target.hp - before)); used = true; }
    if (d->mana > 0) { int before = target.mp; target.mp = std::min(target.maxMP(*gd), target.mp + d->mana); log(fmt("%s recovers %d MP.", target.name.c_str(), target.mp - before)); used = true; }
    if (d->effect == "cure") { target.removeEffect("poison"); target.removeEffect("burn"); target.removeEffect("weaken"); target.removeEffect("blind"); log(target.name + " is cured."); used = true; }
    else if (d->effect == "strength") { target.addEffect("strength", 5, 4); log(target.name + " feels stronger."); used = true; }
    else if (d->effect == "haste") { target.addEffect("haste", 4, 3); log(target.name + " speeds up."); used = true; }
    else if (d->effect == "shield") { target.addEffect("shield", 5, 4); log(target.name + " is shielded."); used = true; }
    else if (d->effect == "regen") { target.addEffect("regen", 5, 6); log(target.name + " begins to regenerate."); used = true; }
    else if (d->effect == "aim") { target.addEffect("aim", 3, 25); log(target.name + " steadies their aim."); used = true; }
    else if (d->effect == "focus") { target.addEffect("focus", 5, 4); log(target.name + " focuses."); used = true; }
    else if (startsWith(d->effect, "learn:")) { learnRecipes(d->effect.substr(6), true); party.removeItem(itemId, 1); gAudio.play("quest"); return true; }
    else if (d->effect == "dig" || d->effect == "blast") {
        if (Game::inCombat() && d->effect == "dig") { log("Not in the middle of a fight."); return false; }
        for (int k = 0; k < 4; k++) { int nx = px + (k == 0) - (k == 1), ny = py + (k == 2) - (k == 3); if (Obstacle* o = map->obstacleAt(nx, ny)) { if (mode == GMode::MENU) popMode(); obstacleMenu((int)(o - &map->obstacles[0])); return false; } }
        log(d->effect == "dig" ? "There's nothing here worth digging through. Stand next to rubble or loose earth." : "Stand next to rubble or a weak wall to set the charge."); return false;
    }
    else if (d->effect == "flare") { log("Throw it during a fight to light up the battlefield."); return false; }
    else if (d->effect == "torch") { lightSteps = std::max(lightSteps, 220); lightPower = std::max(lightPower, 5); log(map && map->dark ? "You light a torch. The dark pulls back a few steps." : "You light a torch. (It matters most underground.)"); revealAround(); used = true; }
    else if (startsWith(d->effect, "imbue_")) { int n = d->mana > 0 ? d->mana : 5; target.removeEffect("imbue_fire"); target.removeEffect("imbue_cold"); target.removeEffect("imbue_shock"); target.removeEffect("imbue_poison"); target.removeEffect("imbue_magic"); target.addEffect(d->effect, 99, n); log(fmt("%s coats their weapon: the next %d attacks deal %s damage.", target.name.c_str(), n, d->effect.substr(6).c_str())); used = true; }
    else if (d->effect == "respec") { int idx = -1; for (size_t i = 0; i < party.members.size(); i++) if (&party.members[i] == &target) idx = (int)i; if (idx < 0 || Game::inCombat()) { log("Not now."); return false; } if (target.level <= 1) { log(target.name + " has nothing to forget yet."); return false; } party.removeItem(itemId, 1); respecMember(idx); gAudio.play("levelup"); return true; }
    else if (startsWith(d->effect, "revealmap:")) {   // a bought or found map: uncovers a map (or part of one) on the world map
        if (Game::inCombat()) { log("Not in the middle of a fight."); return false; }
        auto p = split(d->effect.substr(10), ','); std::string id = trim(p[0]); if (id.empty() || id == "here") id = mapId;
        if (p.size() >= 4) revealMap(id, atoi(p[1].c_str()), atoi(p[2].c_str()), atoi(p[3].c_str())); else revealMap(id);
        log("You study the " + d->name + " and mark it on your map."); notify("Map updated."); used = true;
    }
    (void)inCombat;
    if (used) { party.removeItem(itemId, 1); gAudio.play(d->sound.empty() ? "potion" : d->sound); }
    return used;
}

bool Game::equipItem(int mi, const std::string& itemId) {
    const ItemDef* d = gd->item(itemId); if (!d || !d->equippable()) return false;
    Character& m = party.members[mi]; int slot = d->slot();
    if (!m.equip[slot].empty()) party.addItem(m.equip[slot], 1, *gd);
    if (d->twoHanded && slot == SL_WEAPON && !m.equip[SL_OFFHAND].empty()) { party.addItem(m.equip[SL_OFFHAND], 1, *gd); m.equip[SL_OFFHAND].clear(); }
    if (slot == SL_OFFHAND && !m.equip[SL_WEAPON].empty()) { if (auto* w = gd->item(m.equip[SL_WEAPON])) if (w->twoHanded) { party.addItem(m.equip[SL_WEAPON], 1, *gd); m.equip[SL_WEAPON].clear(); } }
    m.equip[slot] = itemId; party.removeItem(itemId, 1);
    m.hp = std::min(m.hp, m.maxHP(*gd)); m.mp = std::min(m.mp, m.maxMP(*gd));
    log(m.name + " equips " + d->name + "."); gAudio.play("equip");
    return true;
}
void Game::unequip(int mi, int slot) {
    Character& m = party.members[mi]; if (m.equip[slot].empty()) return;
    party.addItem(m.equip[slot], 1, *gd); m.equip[slot].clear(); gAudio.play("equip");
    m.hp = std::min(m.hp, std::max(1, m.maxHP(*gd))); m.mp = std::min(m.mp, m.maxMP(*gd));
}

void Game::hideNpcForGood(NPC& n) { n.hidden = true; if (!n.id.empty()) extra["hidenpc_" + mapId + "_" + n.id] = "1"; }
void Game::respawnParty() { for (auto& m : party.members) { m.alive = true; m.hp = std::max(1, m.maxHP(*gd) / 2); m.effects.clear(); } }

// ---------------- save / load ----------------
static std::string esc(const std::string& s) { std::string o; for (char c : s) { if (c == '\n') o += "\\n"; else if (c == '\\') o += "\\\\"; else o += c; } return o; }
static std::string unesc(const std::string& s) { std::string o; for (size_t i = 0; i < s.size(); i++) { if (s[i] == '\\' && i + 1 < s.size()) { i++; o += s[i] == 'n' ? '\n' : s[i]; } else o += s[i]; } return o; }

std::string Game::serialize() const {
    std::ostringstream o;
    o << "[game]\nversion=" << TRIFFALL_SAVE_FORMAT << "\ngameversion=" << TRIFFALL_VERSION << "\nsaved=" << (long long)time(nullptr) << "\ncampaign=" << campaignId << "\ncampaignname=" << esc(campaign.name) << "\nhero=" << esc(heroName) << "\nmap=" << mapId << "\nx=" << px << "\ny=" << py << "\nplaytime=" << playtimeSec << "\nseed=" << worldSeed << "\ngold=" << party.gold << "\nsteps=" << stepCounter << "\n";
    o << "level=" << (party.members.empty() ? 1 : party.members[0].level) << "\nmapname=" << (map ? esc(map->name) : "") << "\ndifficulty=" << difficulty << "\n";
    for (auto& f : flags) o << "flag." << f.first << "=" << f.second << "\n";
    for (auto& v : visited) o << "visited=" << v << "\n";
    for (auto& it : party.inventory) o << "inv=" << it.id << ":" << it.count << "\n";
    for (auto& m : party.members) {
        o << "[member]\nname=" << esc(m.name) << "\nclass=" << m.classId << "\nlevel=" << m.level << "\nxp=" << m.xp << "\nhp=" << m.hp << "\nmp=" << m.mp << "\nalive=" << (m.alive ? 1 : 0) << "\nstatpoints=" << m.statPoints << "\n";
        o << "tile=" << m.tile.c << "," << m.tile.r << "\n";
        o << "stats="; for (int i = 0; i < ST_COUNT; i++) o << (i ? "," : "") << m.baseStats[i]; o << "\n";
        for (int i = 0; i < SL_COUNT; i++) if (!m.equip[i].empty()) o << "equip" << i << "=" << m.equip[i] << "\n";
        o << "spells="; for (size_t i = 0; i < m.spells.size(); i++) o << (i ? "," : "") << m.spells[i]; o << "\n";
        o << "favs="; for (int i = 0; i < 8; i++) o << (i ? "," : "") << m.favs[i]; o << "\n";
        o << "ranks="; { bool f = true; for (auto& r : m.ranks) { o << (f ? "" : ",") << r.first << ":" << r.second; f = false; } } o << "\nrankpoints=" << m.rankPoints << "\nstartclass=" << m.startClass << "\n";
    }
    o << "[extra]\ntown=" << lastTownMap << "\ntownx=" << lastTownX << "\ntowny=" << lastTownY << "\nclock=" << clockMin << "\nday=" << day << "\nweather=" << weather << "\nweathersteps=" << weatherSteps << "\n";
    for (auto& kv : extra) o << kv.first << "=" << kv.second << "\n";
    o << "leader=" << leader << "\n";
    o << "stats=" << stats.kills << "," << stats.fightsWon << "," << stats.fightsFled << "," << stats.goldEarned << "," << stats.goldSpent << "," << stats.chests << "," << stats.crits << "," << stats.damageDealt << "," << stats.damageTaken << "," << stats.spellsCast << "," << stats.crafted << "," << stats.bounties << "," << stats.travels << "," << stats.knockouts << "\n";
    o << "bestiary="; { bool f = true; for (auto& b : bestiary) { o << (f ? "" : ",") << b.first << ":" << b.second; f = false; } } o << "\n";
    o << "approval="; { bool f = true; for (auto& a : approval) { o << (f ? "" : ",") << a.first << ":" << a.second; f = false; } } o << "\n";
    o << "reputation="; { bool f = true; for (auto& a : reputation) { o << (f ? "" : ",") << a.first << ":" << a.second; f = false; } } o << "\n";
    for (auto& n : notes) o << "[note]\ntext=" << esc(n) << "\n";
    for (auto& d : dynItems) o << "[dynitem]\nid=" << d.first << "\nspec=" << d.second << "\n";
    for (auto& b : bountyOffers) o << "[bountyoffer]\nid=" << b.id << "\nkind=" << b.kind << "\ntarget=" << b.target << "\ntargetname=" << esc(b.targetName) << "\nwhere=" << b.where << "\nwherename=" << esc(b.whereName) << "\ntown=" << b.town << "\nneed=" << b.need << "\nreward=" << b.reward << "\nxp=" << b.xp << "\nposted=" << b.posted << "\ntitle=" << esc(b.title) << "\ntext=" << esc(b.text) << "\n";
    {   // explored map tiles: loaded maps from memory, the rest as they came from the save
        std::map<std::string, std::string> ex = exploredSave;
        for (auto& kv : maps) if (kv.second) ex[kv.first] = encodeExplored(*kv.second);
        for (auto& kv : ex) o << "[explored]\nmap=" << kv.first << "\ndata=" << kv.second << "\n";
    }
    for (auto& b : bounties) o << "[bounty]\nid=" << b.id << "\nkind=" << b.kind << "\ntarget=" << b.target << "\ntargetname=" << esc(b.targetName) << "\nwhere=" << b.where << "\nwherename=" << esc(b.whereName) << "\ntown=" << b.town << "\nneed=" << b.need << "\nhave=" << b.have << "\nreward=" << b.reward << "\nxp=" << b.xp << "\ndone=" << (b.done ? 1 : 0) << "\ntitle=" << esc(b.title) << "\ntext=" << esc(b.text) << "\n";
    return o.str();
}

bool Game::deserialize(const std::string& text) {
    auto secs = parseConfig(text);
    if (secs.empty() || secs[0].name != "game") { loadError = "The save file is damaged."; return false; }
    int format = secs[0].geti("version", 1);
    if (format > TRIFFALL_SAVE_FORMAT) { loadError = "This save was made by a newer version of Triffall (" + secs[0].get("gameversion", "?") + "). Update the game to load it."; return false; }
    migrateSave(secs, format);
    struct LoadingGuard { bool& b; LoadingGuard(bool& x) : b(x) { b = true; } ~LoadingGuard() { b = false; } } guard(loadingSave);
    int dropped = 0;
    if (!switchCampaign(secs[0].get("campaign", "triffall"))) return false;   // a save from another campaign loads that campaign's data first
    party = Party(); flags.clear(); visited.clear(); maps.clear(); exploredSave.clear(); map = nullptr; msgLog.clear(); msgTime.clear(); modeStack.clear(); mode = GMode::EXPLORE; levelUpQueue.clear(); difficulty = 1;
    gameOver = false; running = true; requestDesktop = false; floats.clear(); notifications.clear(); notifTime.clear(); hintShown = -1; hintFade = 0; desktopAction.clear();
    notes.clear(); dynItems.clear(); bounties.clear(); bountyOffers.clear(); stats = GameStats(); bestiary.clear(); approval.clear(); reputation.clear(); leader = 0; banterT = 0;
    for (auto& sec : secs) if (sec.name == "dynitem") dynItems[sec.get("id")] = sec.get("spec");
    restoreDynItems();   // generated items must exist before the inventory is read
    std::string mid; int sx = 1, sy = 1;
    for (auto& sec : secs) {
        if (sec.name == "game") {
            for (auto& kv : sec.kv) {
                if (kv.first == "hero") heroName = unesc(kv.second); else if (kv.first == "map") mid = kv.second; else if (kv.first == "x") sx = atoi(kv.second.c_str());
                else if (kv.first == "y") sy = atoi(kv.second.c_str()); else if (kv.first == "playtime") playtimeSec = atoi(kv.second.c_str()); else if (kv.first == "seed") worldSeed = atoi(kv.second.c_str());
                else if (kv.first == "gold") party.gold = atoi(kv.second.c_str()); else if (kv.first == "steps") stepCounter = atoi(kv.second.c_str());
                else if (kv.first == "difficulty") difficulty = clampi(atoi(kv.second.c_str()), 0, 2);
                else if (startsWith(kv.first, "flag.")) flags[kv.first.substr(5)] = atoi(kv.second.c_str());
                else if (kv.first == "visited") visited.insert(kv.second);
                else if (kv.first == "inv") { auto p = split(kv.second, ':'); if (gd->item(p[0])) party.inventory.push_back({p[0], p.size() > 1 ? atoi(p[1].c_str()) : 1}); else dropped++; }
            }
        } else if (sec.name == "member") {
            Character m; m.isPlayerSide = true; m.faction = "player";
            m.name = unesc(sec.get("name")); m.classId = sec.get("class"); m.level = sec.geti("level", 1); m.xp = sec.geti("xp", 0);
            m.alive = sec.geti("alive", 1) != 0; m.statPoints = sec.geti("statpoints", 0); m.tile = parseTile(sec.get("tile", "104,0"));
            auto st = split(sec.get("stats"), ','); for (int i = 0; i < ST_COUNT && i < (int)st.size(); i++) m.baseStats[i] = atoi(st[i].c_str());
            for (int i = 0; i < SL_COUNT; i++) { m.equip[i] = sec.get("equip" + std::to_string(i)); if (!m.equip[i].empty() && !gd->item(m.equip[i])) { m.equip[i].clear(); dropped++; } }
            for (auto& s : split(sec.get("spells"), ',')) if (!trim(s).empty()) m.spells.push_back(trim(s));
            { auto fv = split(sec.get("favs"), ','); for (int i = 0; i < 8 && i < (int)fv.size(); i++) m.favs[i] = trim(fv[i]); }
            for (auto& r : split(sec.get("ranks"), ',')) { auto p = split(trim(r), ':'); if (p.size() == 2 && !trim(p[0]).empty()) m.ranks[trim(p[0])] = atoi(p[1].c_str()); }
            if (m.ranks.empty()) m.ranks[m.classId] = m.level;   // saves from before multiclassing: every level was in the starting class
            m.rankPoints = sec.geti("rankpoints", 0); m.startClass = sec.get("startclass", m.classId);
            m.hp = std::min(sec.geti("hp", 1), m.maxHP(*gd)); m.mp = std::min(sec.geti("mp", 0), m.maxMP(*gd));
            if (m.alive && m.hp <= 0) m.hp = 1;
            party.members.push_back(m);
        } else if (sec.name == "extra") { lastTownMap = sec.get("town"); lastTownX = sec.geti("townx", 0); lastTownY = sec.geti("towny", 0); clockMin = sec.geti("clock", 8 * 60); day = sec.geti("day", 1); weather = sec.get("weather", "clear"); weatherSteps = sec.geti("weathersteps", 0); extra.clear(); for (auto& kv : sec.kv) if (kv.first != "town" && kv.first != "townx" && kv.first != "towny" && kv.first != "clock" && kv.first != "day" && kv.first != "weather" && kv.first != "weathersteps" && kv.first != "leader" && kv.first != "stats" && kv.first != "bestiary" && kv.first != "approval" && kv.first != "reputation") extra[kv.first] = kv.second;
            leader = sec.geti("leader", 0); stats = GameStats(); { auto st = split(sec.get("stats"), ','); int* f[] = {&stats.kills, &stats.fightsWon, &stats.fightsFled, &stats.goldEarned, &stats.goldSpent, &stats.chests, &stats.crits, &stats.damageDealt, &stats.damageTaken, &stats.spellsCast, &stats.crafted, &stats.bounties, &stats.travels, &stats.knockouts}; for (size_t i = 0; i < st.size() && i < 14; i++) *f[i] = atoi(st[i].c_str()); }
            bestiary.clear(); for (auto& b : split(sec.get("bestiary"), ',')) { auto p = split(trim(b), ':'); if (p.size() == 2 && !p[0].empty()) bestiary[p[0]] = atoi(p[1].c_str()); }
            approval.clear(); for (auto& a : split(sec.get("approval"), ',')) { auto p = split(trim(a), ':'); if (p.size() == 2 && !p[0].empty()) approval[p[0]] = atoi(p[1].c_str()); }
            reputation.clear(); for (auto& a : split(sec.get("reputation"), ',')) { auto p = split(trim(a), ':'); if (p.size() == 2 && !p[0].empty()) reputation[p[0]] = atoi(p[1].c_str()); }
        } else if (sec.name == "note") { notes.push_back(unesc(sec.get("text"))); }
        else if (sec.name == "dynitem") { dynItems[sec.get("id")] = sec.get("spec"); }
        else if (sec.name == "explored") { exploredSave[sec.get("map")] = sec.get("data"); }
        else if (sec.name == "bountyoffer") { Bounty b; b.id = sec.get("id"); b.kind = sec.get("kind"); b.target = sec.get("target"); b.targetName = unesc(sec.get("targetname")); b.where = sec.get("where"); b.whereName = unesc(sec.get("wherename")); b.town = sec.get("town"); b.need = sec.geti("need", 1); b.reward = sec.geti("reward", 0); b.xp = sec.geti("xp", 0); b.posted = sec.geti("posted", 0); b.title = unesc(sec.get("title")); b.text = unesc(sec.get("text")); bountyOffers.push_back(b); }
        else if (sec.name == "bounty") { Bounty b; b.id = sec.get("id"); b.kind = sec.get("kind"); b.target = sec.get("target"); b.targetName = unesc(sec.get("targetname")); b.where = sec.get("where"); b.whereName = unesc(sec.get("wherename")); b.town = sec.get("town"); b.need = sec.geti("need", 1); b.have = sec.geti("have", 0); b.reward = sec.geti("reward", 0); b.xp = sec.geti("xp", 0); b.done = sec.geti("done", 0) != 0; b.title = unesc(sec.get("title")); b.text = unesc(sec.get("text")); bounties.push_back(b); }
    }
    if (party.members.empty() || mid.empty()) { loadError = "The save file is damaged."; return false; }
    lastAutosave = playtimeSec;
    if (dropped) log(fmt("%d item%s from an older version of the game no longer exist%s and were removed.", dropped, dropped == 1 ? "" : "s", dropped == 1 ? "s" : ""));
    for (size_t i = 0; i < party.members.size(); i++) if (party.members[i].statPoints > 0 || party.members[i].rankPoints > 0) levelUpQueue.push_back((int)i);
    lightSteps = stealthSteps = detectSteps = 0;
    syncBonds();
    rng.reseed(fixedSeed ? fixedSeed : (uint64_t)time(nullptr));
    enterMap(mid, sx, sy, false);
    fadeIn = 0.8f;
    return true;
}

// ---------------- favorites hotbar ----------------
void Game::drawHotbar(Canvas& cv, InputState& in, int member, int y) {
    if (member < 0 || member >= (int)party.members.size()) return;
    Character& m = party.members[member];
    uiRect(HB_X - 2, y - 2, 8 * HB_SLOT + 4, 28);
    for (int i = 0; i < 8; i++) {
        int x = HB_X + i * HB_SLOT;
        bool hover = in.mouseX >= x && in.mouseX < x + 26 && in.mouseY >= y && in.mouseY < y + 26;
        cv.fillRect(x, y, 26, 26, hover ? RGB(70, 60, 30, 230) : RGB(20, 20, 30, 210)); cv.rect(x, y, 26, 26, hover ? C::Gold : C::DkGray);
        const std::string& f = m.favs[i];
        if (!f.empty()) {
            std::string kind = f.substr(0, f.find(':')), id = f.substr(f.find(':') + 1);
            bool ok = true; std::string tip;
            if (kind == "spell") { if (auto* s = gd->spell(id)) { ctx.ts->draw(cv, s->tile, x + 1, y + 1, 2); ok = m.mp >= s->cost && (mode == GMode::COMBAT || spellUsableOutside(*s)); tip = s->name + fmt(" (%d MP)", s->cost); } }
            else if (auto* d = gd->item(id)) { ctx.ts->draw(cv, d->tile, x + 1, y + 1, 2); int n = party.countItem(id); ok = n > 0; if (n > 0) ctx.font->draw(cv, *ctx.ts, x + 16, y + 15, std::to_string(std::min(n, 99)), C::White); tip = d->name; }
            if (!ok) cv.fillRect(x + 1, y + 1, 24, 24, RGB(0, 0, 0, 140));
            if (hover && !tip.empty()) { int tw = ctx.font->width(tip) + 8; cv.fillRect(x, y - 17, tw, 16, RGB(0, 0, 0, 200)); ctx.font->draw(cv, *ctx.ts, x + 4, y - 15, tip, C::Cream); }
        }
        std::string k = gKeys.key1[A_FAV1 + i] ? keyName(gKeys.key1[A_FAV1 + i]) : "";
        if (!k.empty()) { cv.fillRect(x, y, ctx.font->width(k) + 3, 9, RGB(0, 0, 0, 180)); ctx.font->draw(cv, *ctx.ts, x + 1, y - 2, k, C::Yellow); }
    }
}
int hotbarSlotAt(int mx, int my, int y) { if (my < y || my >= y + 26) return -1; int i = (mx - HB_X) / HB_SLOT; if (i < 0 || i >= 8 || mx < HB_X || (mx - HB_X) % HB_SLOT >= 26) return -1; return i; }
