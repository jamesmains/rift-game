// The RPG proper: exploration, dialogue, combat, menus. Runs inside a 640x480 canvas.
#pragma once
#include <string>
#include <vector>
#include <map>
#include <set>
#include <memory>
#include <functional>
#include "../core/gfx.h"
#include "../core/input.h"
#include "../core/util.h"
#include "data.h"
#include "entity.h"
#include "world.h"
#include "ui.h"

// objective=STAGE | DONE-CONDITION | TEXT : a checklist line under stage STAGE (1-based, 0 = every stage); TEXT may hold
// counters {itemn:id} {kills:creature} {flag:name} {gold}
struct QuestObjective { int stage = 0; std::string cond, text; };
struct QuestDef { std::string id, name; std::vector<std::pair<std::string, std::string>> stages, fails; std::vector<QuestObjective> objectives; }; // fails: cond -> why (the quest ends as FAILED) // flag -> description

// ---- dialogue ----
struct DialogueOption { std::string text, target; std::vector<std::string> conds; };
struct DialogueNode {
    std::string id, speaker, text;
    std::vector<DialogueOption> options;
    std::vector<std::pair<std::string, std::string>> actions; // key, value (executed when node shown)
    std::string next; // auto-advance target after text
};
struct Dialogue { std::string id; std::map<std::string, DialogueNode> nodes; };

struct EncounterTable { std::string id; int minLevel = 1; std::vector<std::vector<std::string>> groups; };

// ---- combat ----
struct Combatant {
    Character ch;             // copy for enemies; for party members a copy synced back after combat
    int partyIndex = -1;      // >=0 → party member
    int x = 0, y = 0;
    bool acted = false;
    int initiative = 0;
    int groupIndex = -1;      // which EnemyGroup it came from
    bool overwatch = false;   // reaction shot armed until the unit's next turn
    bool aooDone = false;     // attack of opportunity used this round
    bool ally = false;        // a summoned creature fighting for the party (AI-controlled, gone after the fight)
    bool summoned = false; int summoner = -1;   // made by a summon= spell (either side): no body, no XP, no loot
    int summonTurns = 0;      // >0: it fades after that many of its own turns
    float hitFlash = 0, offX = 0, offY = 0;   // feedback: a white flash when hit, a shove away from the blow / a sidestep on a miss
    bool friendly() const { return partyIndex >= 0 || ally; }
};
struct FloatText { float x, y, vy, life; std::string text; Color8 color; int scale = 1; };
struct CombatAnim { int kind = 0; float t = 0, dur = 0.3f; int x0, y0, x1, y1; Color8 color; Tile tile; }; // kind 0 projectile,1 flash,2 slash

struct CombatState {
    std::vector<Combatant> units;
    std::vector<int> order;    // indices into units, turn order
    int turnPos = 0;
    int round = 1;
    bool playerTurn = false;
    int selX = -1, selY = -1;  // cursor
    bool cursorMouse = false;
    // input given while an attack plays out (the swing, the kill beat) is kept and carried out the moment it ends,
    // so the next move or End Turn never feels swallowed
    bool bufClick = false; int bufKey = 0; float bufT = 0; int bufUnit = -1;  // the mouse drives the cursor: it stays on the tile under the pointer (keys or a pad hand it back)
    std::vector<std::pair<int, int>> reachable; // tiles reachable with current move
    std::vector<std::pair<int, int>> path;       // preview path
    std::vector<int> defeatedGroups;
    int mode = 0;              // 0 move/attack, 1 choose spell target, 2 choose item target, 3 action menu
    std::string pendingSpell;  std::string pendingItem;
    float aiTimer = 0;         // delay between AI actions
    int focusParty = -1, focusEnemy = -1;   // the unit each side last attacked (the AI focuses fire on it)
    std::vector<CombatAnim> anims;
    std::vector<std::string> log;
    bool ending = false; float endTimer = 0; bool victory = false; bool fled = false;
    int xpGained = 0, goldGained = 0; std::vector<std::string> itemsGained;
    int menuSel = 0, menuScroll = 0;   // action / spell / item menus (menuScroll: first visible row of the spell / item list)
    bool aimed = false;        // attack mode: false = normal, true = aimed (toggle)
    std::string onDefeatScript;
    std::vector<std::pair<int, int>> aiPath; float stepTimer = 0; int aiAction = 0; int aiTarget = -1; // AI execution
    int selectedUnit = -1;
    bool reachDirty = false;   // someone died / moved out of the way: recompute the active unit's reachable tiles
    struct Light { int x, y, r, turns; };
    std::vector<Light> lights; // temporary combat lights (Light spell, flares, explosions)
    int lastRoundJoined = 0;   // reinforcement check runs once per round
    bool actionThisRound = false; int quietRounds = 0; bool disengaged = false;
    float hitstop = 0;         // a beat of stillness after a kill or a critical hit
    float banner = 0; std::string bannerText;   // "Round 3" / "Your turn: Bram" under the top bar   // rounds without a blow: the fight breaks off
};

enum class GMode { EXPLORE, DIALOGUE, COMBAT, MENU, SHOP, MESSAGE, TRANSITION, GAMEOVER, LEVELUP, NAME, VICTORY, PAUSE, CUTSCENE, SAVELOAD, JOURNAL, WORLDMAP, CRAFT, CONSOLE, BOUNTY, ENDING };

// ---- newer systems (journal.cpp, worldmap.cpp, craft.cpp, console.cpp, bounty.cpp) ----
struct GameStats { int kills = 0, fightsWon = 0, fightsFled = 0, goldEarned = 0, goldSpent = 0, chests = 0, crits = 0, damageDealt = 0, damageTaken = 0, spellsCast = 0, crafted = 0, bounties = 0, travels = 0, knockouts = 0, questsDone = 0; };
struct AchievementDef { std::string id, name, desc, cond; Tile tile; bool hidden = false; };
struct BanterLine { std::string cls, when, map, text, flag, cond; };
struct Recipe { std::string id, name, result, station, category, reveal; int count = 1, gold = 0, level = 1, difficulty = 1; bool known = true; std::vector<std::pair<std::string, int>> mats; };
struct AffixDef { std::string id, name, dmgType; bool prefix = true; int tier = 1, valuePct = 160; std::vector<std::string> types; std::vector<std::pair<std::string, int>> mods; };
struct Bounty { std::string id, kind, target, targetName, where, whereName, town, title, text; int need = 1, have = 0, reward = 0, xp = 0, posted = 0; bool done = false, taken = false; };
// bounties.txt: [board] = a notice board and what it posts; [bounty] = a hand-written notice a board always offers
struct HintDef { std::string cond, title, text; };   // hints.txt: tutorial guidance shown while its condition holds (guide.cpp)
struct FactionDef { std::string id, name, text; };   // a town or group the party has a reputation with (factions.txt)
struct BoardDef { std::string id, name, map, location; int offers = 3, reward = 100, rep = 3; std::vector<std::string> kinds{"kill", "fetch", "deliver"}, kill, creatures, fetch, deliver; bool fromFile = false; };
struct FixedBountyDef { std::string id, board, kind, target, where, title, text, require; int need = 1, reward = 30, xp = 20; bool repeat = false; };
struct WorldLocation { std::string id, name, mapId, kind; int x = 0, y = 0, tx = 0, ty = 0, reveal = 0; };   // reveal: entering its map uncovers the overworld this far around it
// A place on a non-overworld world map (worldmap=<map id> / current): one per exit of that map, named after where it leads.
struct WorldPlace { std::string name, mapId; int x = 0, y = 0; bool known = false, visited = false; };

struct EpilogueSlide { std::string group, title, text; Tile tile; bool hasTile = false, recolor = false; Color8 color = C::White; };   // epilogue.txt (ending.cpp)

struct SaveSlotInfo { bool exists = false; std::string hero, mapName, campaign; int level = 0, playtime = 0; };

// A campaign: the base game (assets/data, id "triffall") or a folder under assets/campaigns/<id>/ holding its own
// campaign.txt plus any data files and maps/ dialogue/ folders. Files it has add to / replace the base game's by id;
// files it doesn't have are inherited (inherit=0 in campaign.txt makes it standalone apart from tiles and sounds).
struct CampaignInfo {
    std::string id = "triffall", name = "Triffall", desc, dir;   // dir = data root ("" = base game)
    int iconC = 0, iconR = 7;                                       // desktop icon in icons.png (col,row)
    std::string startMap = "hollowmere_home"; int startX = 0, startY = 0;
    std::vector<std::string> intro; std::string startDialogue = "intro";
    int gold = 25; bool inherit = true; std::string startItems, startFlags, author, version;
    int order = 100;                      // desktop icon / campaign list order (lower first; the tutorial is 0)
    bool achievements = true;             // achievements=0: nothing unlocks while playing it (the tutorial)
    std::string worldMap = "overworld";   // what M shows: overworld | off | current (the map you are on) | <map id>
};
std::vector<CampaignInfo> listCampaigns(const std::string& assetDir);   // base game first
CampaignInfo readCampaignInfo(const std::string& file, const std::string& id, const std::string& dir);
SaveSlotInfo readSaveSlotInfo(const std::string& path);
bool writeSaveFile(const std::string& path, const std::string& text);   // atomic, keeps the previous file as path.bak

struct GameSettings {
    bool fullscreen = true;
    int scale = 2;
    bool vsync = true;
    bool integerScaling = true;
    bool crt = false;
    int masterVolume = 80, musicVolume = 70, sfxVolume = 80;
    int ambientVolume = 60;         // wind, rain, waves, town bustle... under the music (its own slider since 1.0.7)
    bool gamepad = true;
    bool screenShake = true;
    bool fastCombat = false;
    bool interactWhileMoving = false;
    bool edgeScroll = true;
    int banter = 2;                 // companion chatter: 0 off, 1 rare, 2 normal, 3 often
    // accessibility (Settings > Access)
    bool largeText = false;         // dialogue, messages, cutscenes, tutorial hints and the epilogue in double-size text
    bool colorblind = false;        // blue / orange instead of green / red for sides, hit chances and cover
    int textSpeed = 1;              // dialogue text: 0 slow, 1 normal, 2 fast, 3 instant
    bool reduceFlashing = false;    // no lightning flashes, softer hit flashes
    int fontStyle = 1;              // always Font::STYLE_CLEAR since 1.1.1 (the font setting was removed)
    std::string discord;            // settings.ini discord=: where the desktop's Discord icon points (empty = the built-in invite)
    void save(const std::string& path) const;   // also writes the keybinds
    bool load(const std::string& path);
};

class Game {
public:
    Ctx ctx;
    GameData* gd = nullptr;
    Rng rng;
    GameSettings* settings = nullptr;
    std::string assetDir, saveDir;
    std::string campaignId = "triffall", campaignDir;   // campaign data folder (empty = the base game in assets/data)
    CampaignInfo campaign;
    bool switchCampaign(const std::string& id);          // reloads all data for that campaign (no-op if already active)
    std::string dataPath(const std::string& rel) const;   // a data file: the campaign's copy if it has one, else the base game's
    std::vector<ConfigSection> mergedConfig(const std::string& rel) const;   // base game sections, then the campaign's

    // ---- persistent state ----
    Party party;
    std::map<std::string, int> flags;
    std::set<std::string> visited;
    std::string mapId;
    int px = 0, py = 0;              // hero position
    bool movementInputWasDown = false, movedDuringMoveHold = false;
    int playtimeSec = 0; float playtimeAcc = 0;
    int worldSeed = 1337;
    uint64_t fixedSeed = 0;          // tests: a fixed RNG seed instead of the clock (0 = clock)
    int difficulty = 1;
    std::string heroName;
    // ---- world clock & weather (systems.cpp) ----
    int clockMin = 8 * 60, day = 1;      // minutes into the day, day counter
    std::string weather = "clear";      // clear | rain | storm | fog | snow
    int weatherSteps = 0;               // steps until the weather rerolls
    float stormFlash = 0, thunderT = 0, frameDt = 0;
    std::vector<std::pair<float, float>> precip; // falling particles (screen space)
    bool outdoors() const;
    float nightAmount() const;          // 0 = day, 1 = deep night
    std::string clockText() const;
    void advanceClock(int minutes);
    void rerollWeather();
    void drawWeather(Canvas& cv, float dt);
    bool shopOpen(const std::string& shopId, std::string& hoursOut) const;
    std::map<std::string, std::string> extra;    // free-form persistent key/values (saved in [extra])
    // ---- stats, bestiary, companions, achievements (journal.cpp) ----
    GameStats stats;
    std::map<std::string, int> bestiary;          // creature id -> kills (present = seen)
    std::map<std::string, int> approval;          // companion class id -> approval
    std::vector<HintDef> hintDefs; int hintShown = -1; float hintFade = 0;
    void loadHints(); int currentHint() const; void updateHints(float dt); int drawHint(Canvas& cv, int y); std::string keyText(const std::string& raw) const;
    std::string desktopAction;                   // run on the desktop when the game hands back to it (finishtutorial: "campaign:triffall")
    std::map<std::string, int> reputation;        // faction id -> reputation (-100..100); rep= action, rep:id:n condition
    std::vector<FactionDef> factions; std::string shopFaction;   // shopFaction: the open shop's faction (prices follow it)
    std::string factionName(const std::string& id) const; static const char* repTier(int r);
    void changeRep(const std::string& id, int d);
    int leader = 0;                               // party member chosen to walk in front on the map
    int activeLeader() const;                     // the chosen leader, or the first living member while they are down
    std::vector<int> followOrder() const;         // everyone else, in the order they follow the trail (trail[k] = followOrder()[k])
    std::vector<std::string> notes;               // lore picked up (signs, messages)
    std::vector<BanterLine> banters; bool banterVictoryPending = false; int banterCooldown = 40; std::string banterText; float banterT = 0; int banterMember = -1;
    std::vector<AchievementDef> achievements; std::set<std::string> unlocked; std::vector<std::pair<std::string, float>> achPopups; float achCheckT = 0;
    std::vector<Recipe> recipes; std::vector<AffixDef> affixes;
    std::map<std::string, std::string> dynItems;  // generated item id -> "base|affix,affix"
    std::vector<Bounty> bounties;      // taken by the party
    std::vector<Bounty> bountyOffers;  // posted on boards, not taken yet
    std::string boardTown; int bountySel = 0;
    std::vector<BoardDef> boards; std::vector<FixedBountyDef> fixedBounties;
    void loadBoards(); const BoardDef* board(const std::string& id) const; std::string boardName(const std::string& id) const;
    std::vector<WorldLocation> locations;
    int jTab = 0, jSel = 0, jScroll = 0;          // journal
    int wmSel = -1, wmScroll = 0;                 // world map
    std::string craftStation; int craftSel = 0, craftScroll = 0;
    std::string consoleBuf; std::vector<std::string> consoleHist, consoleOut; int consoleHistPos = -1;
    void loadSystems();                           // banter, achievements, recipes, affixes, locations
    void onKill(const Character& victim, const Combatant* killer);
    void onSeen(const std::string& creatureId) { if (!bestiary.count(creatureId)) bestiary[creatureId] = 0; }
    void addNote(const std::string& title, const std::string& text);
    void banterTrigger(const std::string& when);
    void drawBanter(Canvas& cv);
    void checkAchievements(bool force = false);
    void unlockAchievement(const std::string& id);
    void loadAchievementState(); void saveAchievementState() const;
    void updateJournal(InputState& in, float dt); void drawJournal(Canvas& cv, InputState& in);
    void drawAchievementPopups(Canvas& cv, float dt);
    std::string questText(const QuestDef& q) const; std::string questPin(const QuestDef& q) const;
    // world map (worldmap.cpp)
    void loadLocations();
    bool wmTravel = false;                        // opened by a coachman: travel allowed, for a fare
    int travelFare(const WorldLocation& l) const;
    void openWorldMap(bool travelMode = false); void updateWorldMap(InputState& in, float dt); void drawWorldMap(Canvas& cv, InputState& in);
    void fastTravel(const WorldLocation& l);
    std::string worldMapSource() const;           // "" = this campaign has no world map; "overworld"; or a map id
    std::string wmSource;                         // what the open world map shows
    std::vector<WorldPlace> worldPlaces(Map* m) const;
    std::string mapDisplayName(const std::string& id) const;
    mutable std::map<std::string, std::string> mapNameCache;
    void revealMap(const std::string& id, int x = -1, int y = -1, int r = -1);   // revealmap= action: the whole map, or a circle
    void updateLocalMap(InputState& in); void drawLocalMap(Canvas& cv, InputState& in);
    // crafting / affixes (craft.cpp)
    void openCraft(const std::string& station); void updateCraft(InputState& in, float dt); void drawCraft(Canvas& cv, InputState& in);
    bool canCraft(const Recipe& r) const; bool craft(const Recipe& r);
    std::string makeAffixed(const std::string& baseId, int tier);   // returns the generated item id (registered in gd->items)
    // crafting depth: station skills, recipe discovery, success/quality rolls, upgrades (+1..+3), enchanting, salvage
    int craftTab = 0, craftQty = 1;                                  // 0 craft, 1 upgrade, 2 enchant, 3 salvage
    int craftSkill(const std::string& station) const;                // 1..6
    int craftXP(const std::string& station) const;
    int craftStat(const std::string& station) const;                 // best party stat for the station
    int craftSuccess(const std::string& station, int difficulty) const;
    bool recipeVisible(const Recipe& r) const;
    void learnRecipes(const std::string& csv, bool announce);
    void markSeenItems();
    std::string buildDynItem(const std::string& base, const std::vector<std::string>& affixIds, int plus);
    bool parseDynId(const std::string& id, std::string& base, std::vector<std::string>& affixIds, int& plus) const;
    struct CraftTarget { int member = -1, slot = -1, inv = -1; std::string id; };
    std::vector<CraftTarget> craftTargets(int tab) const;
    std::vector<std::pair<std::string, int>> upgradeCost(const ItemDef& d, int plus, int& gold) const;
    std::vector<std::pair<std::string, int>> enchantCost(int affixCount, int& gold) const;
    std::vector<std::pair<std::string, int>> salvageYield(const ItemDef& d) const;
    void replaceTargetItem(const CraftTarget& t, const std::string& newId);
    void restoreDynItems();
    int lootTierRoll(int level);
    // the ending (ending.cpp): epilogue slides, the journey in numbers, credits, keep exploring / desktop
    std::vector<EpilogueSlide> endSlides; std::vector<std::string> creditLines; int endPage = 0, endSel = 0; float endT = 0, endScroll = 0;
    void startEnding(); void updateEnding(InputState& in, float dt); void drawEnding(Canvas& cv, InputState& in); std::string endingText(const std::string& raw) const;
    // console (console.cpp)
    void runConsole(const std::string& cmd); void updateConsole(InputState& in, float dt); void drawConsole(Canvas& cv, InputState& in);
    // bounties (bounty.cpp)
    void openBoard(const std::string& town); void updateBounty(InputState& in, float dt); void drawBounty(Canvas& cv, InputState& in);
    void generateBounties(const std::string& town); void bountyEvent(const std::string& kind, const std::string& id, int n = 1);

    // ---- runtime ----
    std::map<std::string, std::unique_ptr<Map>> maps; // loaded maps
    std::map<std::string, std::string> exploredSave;  // explored-tile masks of maps not currently loaded (from the save file)
    std::string encodeExplored(const Map& m) const;
    void decodeExplored(Map& m, const std::string& s) const;
    void stashExplored();
    Map* map = nullptr;
    GMode mode = GMode::EXPLORE;
    std::vector<GMode> modeStack;
    std::vector<std::pair<int, int>> trail;   // party follow positions
    int camX = 0, camY = 0;          // current camera (top-left, pixels)
    float camFX = 0, camFY = 0;      // smooth camera position
    int camTX = 0, camTY = 0;        // camera target
    bool camFree = false;            // user panned the camera; stops auto-follow until recentered
    float moveRepeat = 0; int lastDirX = 0, lastDirY = 0; int facing = 1;
    // utility-spell effects outside combat
    int lightSteps = 0, stealthSteps = 0, detectSteps = 0;
    int lightPower = 0;              // radius of the light the party carries while lightSteps > 0 (torch 5, Light spell 8)
    std::vector<uint8_t> litMask;    // dark maps: which tiles are lit (party light, torches, braziers, combat lights)
    void computeLight();
    bool lit(int x, int y) const;
    int partyGlow() const;           // light radius around each party member on dark maps
    bool inCombat() const;
    int tileMoveCost(int x, int y) const;   // movement points to enter a tile (its kind's move cost, at least 1)
    std::string lastTownMap; int lastTownX = 0, lastTownY = 0;
    std::vector<std::string> msgLog; std::vector<float> msgTime;
    std::vector<FloatText> floats;
    float shake = 0;
    bool running = false;            // a game is in progress (desktop checks this)
    bool requestDesktop = false;     // return to desktop
    bool requestSettings = false;    // open the desktop settings window and come back
    std::vector<Rect> uiRects, uiRectsPrev; // screen regions covered by UI this frame (clicks there never reach the map)
    bool gameOver = false;
    bool godMode = false;            // debug: party takes no damage
    bool autoCombat = false;         // debug: the party fights on its own (balance testing)
    std::string pendingMessage, pendingMessageTitle; std::function<void()> messageDone;
    float transition = 0; std::string transitionTo; int transX = 0, transY = 0; std::string transText;
    float fadeIn = 0;
    int stepCounter = 0;
    std::map<std::string, Dialogue> dialogues;
    std::map<std::string, EncounterTable> encounters;
    std::vector<QuestDef> quests;
    std::vector<std::string> notifications; std::vector<float> notifTime;

    // dialogue runtime
    std::string dlgId, dlgNode; int dlgSel = 0; std::string dlgSpeakerOverride; Tile dlgPortrait; bool dlgPortraitRecolor = false; Color8 dlgPortraitColor;
    int dlgTextShown = 0; float dlgCharTimer = 0; NPC* dlgNpc = nullptr;
    float dlgOptT = 0; bool dlgOptArmed = false;   // 1.1.1: time the answers have been shown; armed = they take a press
    static constexpr float DLG_ARM_SECONDS = 0.35f;

    // menu runtime
    int menuTab = 0, menuSel = 0, menuSub = 0, menuMember = 0, menuScroll = 0; bool menuSubFocus = false; int spellCol = 0; std::string menuPrompt;
    // item lists (inventory, shop): category filter + sort order. sort: 0 default order, 1 name, 2 value, 3 category
    std::string invCat = "All", shopCat = "All"; int invSort = 0, shopSort = 0;
    std::vector<int> itemView(const std::vector<std::string>& ids, const std::string& cat, int sort, bool buying) const;
    std::vector<std::pair<std::string, int>> itemCats(const std::vector<std::string>& ids) const;   // categories present, with counts
    std::vector<int> invView() const;                 // inventory slots shown in the Items tab, in order
    std::vector<int> shopView() const;                // stock rows (Buy) or inventory slots (Sell) shown in the shop
    int statAlloc[ST_COUNT] = {};
    int luRow = 0, luClass = -1; int sheetEquipY = 270;                 // level-up screen: focused row, class picked for the new rank
    void applyRank(Character& m, const std::string& cls);
    std::string rankPreview(const Character& m, const std::string& cls) const;
    void respecMember(int memberIndex);
    bool respecFree = false; std::string respecItem; int respecCost(int memberIndex) const;
    // shop
    std::string shopId; int shopTab = 0, shopSel = 0, shopScroll = 0; std::vector<std::string> shopStock;
    int shopQty = -1; float shopQtyHold = 0, shopQtyRep = 0;
    int shopStockLeft(const std::string& shop, const std::string& item, int listed) const;   // -1 = unlimited
    void shopRestockCheck(const std::string& shop);
    int shopMaxQty(int tab, int sel);
    void shopTransact(int qty);
    // combat
    CombatState cb;
    // level up queue
    std::vector<int> levelUpQueue;
    // name entry
    std::string nameBuf; std::string classChoice; int classSel = 0; int namePhase = 0;
    // cutscene text
    std::vector<std::string> cutLines; int cutIndex = 0; float cutTimer = 0; std::function<void()> cutDone;
    // in-game save/load
    bool slMode = false; int slSel = 0; int slConfirm = -1; SaveSlotInfo slots[3]; std::string slStatus; // slMode: false=load, true=save
    // favorites assignment prompt
    int favAssign = -1; // index of pending favorite slot while picking (unused now)

    bool init(GameData* data, GameSettings* s, const std::string& assets, Tileset* ts, Font* font);
    // demo build (TRIFFALL_DEMO, or the console's 'demo 1'): the base campaign is limited to demo.txt's maps
    bool demoMode = false; std::set<std::string> demoMaps; std::string demoEnd, demoBlockedText, demoTitle, demoText, storeUrl;
    void loadDemo(); bool demoBlocks(const std::string& map) const; void startDemoEnd();
    void newGame(const std::string& heroName, const std::string& classId, int difficulty = 1);
    void update(InputState& in, float dt, Canvas& cv);
    void draw(Canvas& cv, InputState& in);
    bool canSaveHere() const;
    std::string musicWanted() const;            // which music track fits the current mode/map
    std::string ambientWanted() const;          // which ambient loop fits (map header ambient=, else by place, weather, time)
    std::string weaponSound(const Character& ch) const;
    std::string spellSound(const SpellDef& s) const;
    std::string serialize() const;
    bool deserialize(const std::string& text);
    bool saveToSlot(int slot);                     // slot 0-2, or AUTOSAVE_SLOT (saves.cpp)
    bool loadFromSlot(int slot);
    static const int AUTOSAVE_SLOT = 3;
    std::string slotPath(int slot) const;
    bool loadSaveFile(const std::string& path);   // falls back to the .bak copy when the file is damaged; loadError says why not
    std::string loadError;
    void autosave(); int lastAutosave = -1; bool loadingSave = false;
    void migrateSave(std::vector<ConfigSection>& secs, int fromFormat);
    std::string crashState() const; void emergencySave();
    void refreshSlots();
    void openSaveLoad(bool save);
    void reloadContent();            // after the editor changed data files
    bool mouseOverUI(const InputState& in) const;
    void uiRect(int x, int y, int w, int h) { uiRects.push_back({x, y, w, h}); }
    void camCenterOn(int tx, int ty, bool snap = false);
    void camClamp();
    bool spellUsableOutside(const SpellDef& s) const;
    bool spellUsableInCombat(const SpellDef& s) const { return s.dmgMax > 0 || s.heal > 0 || !s.effect.empty() || s.utility == "light"; }
    bool castUtility(int member, const std::string& spellId, int target);
    void useFavorite(int member, int slot);

    // helpers used by subsystems
    bool hasFlag(const std::string& f) const { auto it = flags.find(f); return it != flags.end() && it->second != 0; }
    void setFlag(const std::string& f, int v = 1) { flags[f] = v; }
    void log(const std::string& s);
    void notify(const std::string& s);
    void showMessage(const std::string& title, const std::string& text, std::function<void()> done = nullptr);
    void pushMode(GMode m) { modeStack.push_back(mode); mode = m; }
    void popMode() { if (!modeStack.empty()) { mode = modeStack.back(); modeStack.pop_back(); } else mode = GMode::EXPLORE; }
    Map* loadMap(const std::string& id);
    void enterMap(const std::string& id, int x, int y, bool fade = true);
    void startTransition(const std::string& to, int x, int y, const std::string& text = "");
    void giveXP(int xp);
    void giveItem(const std::string& id, int n = 1, bool silent = false);
    void startDialogue(const std::string& id, NPC* npc = nullptr, const std::string& node = "start");
    void runScript(const std::string& script);
    void startCombat(const std::vector<std::string>& creatureIds, int cx, int cy, int levelBonus, const std::vector<int>& groupIndices, const std::string& onDefeat = "");
    std::vector<int> combatExtraGroups;   // map groups that join the next startCombat from round one
    void applyDifficulty(Character& enemy) const;
    void startCutscene(const std::vector<std::string>& lines, std::function<void()> done);
    bool evalCond(const std::string& cond) const;
    void applyMapFlags(Map& m);
    // world state (worldstate.cpp)
    void applyWorldState(Map& m);
    int curObstacle = -1; float fuseT = 0; int fuseX = 0, fuseY = 0, fuseObstacle = -1; std::string fuseMap;
    // healers (healer.cpp)
    std::string healerGreeting; bool healerReopen = false; int doorMsgCd = 0;
    int healerCost(const std::string& svc) const; void healerMenu(const std::string& greeting); void healerService(const std::string& svc); void applyBlessing();
    const std::pair<std::string, std::vector<int>>& doorShop(int x, int y); std::string closedShopDoor(int x, int y);
    void obstacleMenu(int idx); void obstacleAction(const std::string& how); void clearObstacle(Map& m, Obstacle& o, bool announce);
    void blastAt(int cx, int cy, int lo, int hi, const std::string& what, int radius);
    void updateFuse(float dt);
    void pullLever(Lever& l); void gatherFrom(GatherNode& g);
    bool gatherReady(const GatherNode& g) const { auto it = extra.find("gather_" + mapId + "_" + std::to_string(g.x) + "_" + std::to_string(g.y)); return it == extra.end() || day - atoi(it->second.c_str()) >= g.respawn; }
    bool interactable(int x, int y) { return map->npcAt(x, y) || map->chestAt(x, y) || map->doorAt(x, y) || map->signAt(x, y) || map->enemyAt(x, y) || map->obstacleAt(x, y) || map->leverAt(x, y) || map->gatherAt(x, y); }
    void respawnParty();
    void hideNpcForGood(NPC& n);   // hidenpc= / join= / combat= / hostile=: stays hidden (saved)
    std::string questStatus(const QuestDef& q) const;
    bool questFailed(const QuestDef& q) const; bool questClosed(const QuestDef& q) const;   // FAILED / DONE or FAILED
    int questStageIndex(const QuestDef& q) const;           // 1-based index of the stage the journal shows (0 = none)
    std::string questCounters(const std::string& text) const;   // fills {itemn:id} {kills:c} {flag:f} {gold}
    std::vector<std::pair<std::string, int>> shopItems(const std::string& shop);
    int itemBuyPrice(const ItemDef& d) const; std::string shopFactionOf(const std::string& shop) const;
    int itemSellPrice(const ItemDef& d) const;
    bool useItemOn(const std::string& itemId, Character& target, bool inCombat);
    bool equipItem(int memberIdx, const std::string& itemId);
    void unequip(int memberIdx, int slot);
    void checkLevelUps();

private:
    // explore
    void updateExplore(InputState& in, float dt);
    void tryMove(int dx, int dy, bool interactOnBlocked = true);
    void interactAt(int x, int y);
    void onStep();
    void updateNpcs(float dt);
    void updateCamera();
    void drawWorld(Canvas& cv, bool combat);
    void drawHUD(Canvas& cv, InputState& in);
    void drawLog(Canvas& cv);
    bool visible(int x, int y) const;
    void revealAround();
    void randomEncounter(bool force = false);
    // dialogue
    void updateDialogue(InputState& in, float dt);
    void drawDialogue(Canvas& cv, InputState& in);
public:
    int textScale() const;                       // 2 with Settings > Access > Large text
    int dialogueOptionH() const; int dialogueOptionsY(int n) const;
private:
    void showNode(const std::string& node);
    void runAction(const std::string& k, const std::string& v);
    void runActionInner(const std::string& k, const std::string& v);
    bool loadDialogues();
    // combat (combat.cpp)
    void updateCombat(InputState& in, float dt);
    void drawCombat(Canvas& cv, InputState& in);
    void combatComputeReachable();
    void combatNextTurn();
    void combatBeginTurn();
    void combatEndTurn();
    void combatAttack(int attacker, int target, bool aimed);
    int coverForTile(int x, int y, int fx, int fy, int self) const; bool aiCoverSpot(int ui, int ti, int& bx, int& by); std::vector<std::pair<int, int>> aiWalkTo(int ui, int x, int y);
    void combatSummon(int ci, const SpellDef& sp, int tx, int ty); int summonsOf(int ci) const;
    void combatCast(int caster, const std::string& spellId, int tx, int ty);
    void combatUseItem(int user, const std::string& itemId, int target, int tx = -1, int ty = -1);
    int itemAP(const ItemDef& d) const;   // AP to use an item in combat (thrown 3, drink/apply 4)
    void combatAI(float dt);
    void combatCheckEnd();
    void combatFinish();
    int  combatUnitAt(int x, int y) const;
    int  hitChance(const Combatant& a, const Combatant& d, bool aimed) const;
    int  coverAt(const Combatant& a, const Combatant& d) const;      // 0 none, 1 half, 2 full
    bool flanked(const Combatant& a, const Combatant& d) const;
    std::string hitBreakdown(const Combatant& a, const Combatant& d, bool aimed) const;
    void combatStepUnit(int ui, int nx, int ny);                     // move one tile + reactions (overwatch, attacks of opportunity)
    void combatReactions(int mover, int fromX, int fromY);
    void applyDamage(Combatant& target, int dmg, int dmgType, const std::string& source, bool crit, Combatant* attacker);
    void applyStatusTick(Combatant& u);
    void combatJoinGroups();
public:
    bool groupHasBoss(const EnemyGroup& g) const;                    // a group with a boss creature stays in its lair
private:                                         // nearby map groups that hear the fight join it
    bool combatAddGroup(int groupIndex, bool announce);
    int  darknessPenalty(const Combatant& a, const Combatant& d) const; // to-hit penalty for attacking into the dark
    std::vector<int> spellVictims(int caster, const SpellDef& sp, int tx, int ty) const; // who an area spell would hit (friend and foe)
    void drawTileInfo(Canvas& cv, int tx, int ty);
    void addFloat(int tx, int ty, const std::string& s, Color8 c, int scale = 1);
    void spawnLoot(Combatant& u);
    // menus (menus.cpp)
    void updateMenu(InputState& in, float dt);
    void drawMenu(Canvas& cv, InputState& in);
    void updateShop(InputState& in, float dt);
    void drawShop(Canvas& cv, InputState& in);
    void updateLevelUp(InputState& in, float dt);
    void drawLevelUp(Canvas& cv, InputState& in);
    void updateName(InputState& in, float dt);
    void drawName(Canvas& cv, InputState& in);
    void updateMessage(InputState& in, float dt);
    void drawMessage(Canvas& cv, InputState& in);
    void drawGameOver(Canvas& cv, InputState& in);
    void drawVictory(Canvas& cv, InputState& in);
    void updatePause(InputState& in, float dt);
    void drawPause(Canvas& cv, InputState& in);
    void updateSaveLoad(InputState& in, float dt);
    void drawSaveLoad(Canvas& cv, InputState& in);
    void drawHotbar(Canvas& cv, InputState& in, int member, int y);
    void updateCameraInput(InputState& in, float dt);
    void updateCutscene(InputState& in, float dt);
    void drawCutscene(Canvas& cv, InputState& in);
    void drawCharacterSheet(Canvas& cv, InputState& in, int member, int x, int y, int w, int h);
    void drawUnitSprite(Canvas& cv, const Character& ch, int sx, int sy, bool flip);
    int pauseSel = 0;
    bool loadQuests();
    bool loadEncounters();
    std::vector<uint8_t> visMask;
};

std::string flagDescribe(const std::string& f);
const char* statHelp(int stat);   // one line: what a stat point does
int hotbarSlotAt(int mx, int my, int y);
