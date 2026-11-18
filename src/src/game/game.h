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
#include "../core/theme.h"
#include "script.h"

// The dev console (~) only exists in debug builds: CMake -DTRIFFALL_DEV_CONSOLE=ON (build.ps1 -Dev), a Debug configuration,
// and the headless test runner. Release builds (Full, No Editor, Demo) never open it.
#if defined(TRIFFALL_DEV_CONSOLE)
static constexpr bool kDevConsole = true;
#else
static constexpr bool kDevConsole = false;
#endif

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
std::vector<std::string> comboHelpLines();   // 1.4.0: one plain line per status combo (the table in combat.cpp)
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
    // 1.4.0
    bool lastStandUsed = false;   // laststand perk: once a fight
    bool gone = false;            // fled the field / surrendered / a closed rift: no body, counts as beaten
    int lastWindup = -9;          // round of its last wind-up (windup= bosses)
    bool potionUsed = false;      // an enemy drinks at most one draught a fight (Normal: leaders and bosses, Hard: all)
    bool rift = false;            // goal=close: a rift tear that calls enemies every round until someone closes it
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
    long hpSum = -1; int harmlessRounds = 0;   // 1.8.6: rounds in a row in which nobody's hit points changed
    float endHold = 0;   // 1.8.7: how long the pad's End turn button has been held (Settings: hold to end a turn)
    float hitstop = 0;         // a beat of stillness after a kill or a critical hit
    float banner = 0; std::string bannerText;   // "Round 3" / "Your turn: Bram" under the top bar   // rounds without a blow: the fight breaks off
    // 1.4.0 (#58 #61): ground effects for this fight (kind 1 fire, 2 ice, 3 smoke) and damage soaked by breakable cover
    struct GroundFx { int x, y, kind, turns; };
    std::vector<GroundFx> groundFx;
    std::map<int, int> tileDmg;
    // 1.4.0 (#62): a boss's announced attack, landing on these tiles at the start of its next turn
    struct Windup { int unit = -1; std::string spell; std::vector<std::pair<int, int>> tiles; };
    std::vector<Windup> windups;
    int surprise = 0;             // 1.4.0 (#80): 1 the party gets a free first round, -1 the enemies do
    // 1.4.0 (#78): the fight's goal (from the enemy group's goal=): protect | reach | hold | close
    struct Goal { std::string kind, arg, text, onWin, onFail; int x = 0, y = 0, w = 1, h = 1, rounds = 0, unit = -1, group = -1; bool done = false, failed = false; };
    Goal goal;
    std::vector<int> reachCover;  // 1.4.0 (#55): per reachable tile, the cover on each side (2 bits each, N E S W) against foes that see it
};

enum class GMode { EXPLORE, DIALOGUE, COMBAT, MENU, SHOP, MESSAGE, TRANSITION, GAMEOVER, LEVELUP, NAME, VICTORY, PAUSE, CUTSCENE, SAVELOAD, JOURNAL, WORLDMAP, CRAFT, CONSOLE, BOUNTY, ENDING, SETTINGS };   // SETTINGS: 1.8.3 (#170) the in-game Settings screen

// ---- newer systems (journal.cpp, worldmap.cpp, craft.cpp, console.cpp, bounty.cpp) ----
struct GameStats { int kills = 0, fightsWon = 0, fightsFled = 0, goldEarned = 0, goldSpent = 0, chests = 0, crits = 0, damageDealt = 0, damageTaken = 0, spellsCast = 0, crafted = 0, bounties = 0, travels = 0, knockouts = 0, questsDone = 0; };
struct AchievementDef { std::string id, name, desc, cond; Tile tile; bool hidden = false; };
struct BanterLine { std::string cls, when, map, text, flag, cond; std::vector<std::string> seen; };   // 1.5.0: seen= creatures (when=firstsight)
struct Recipe { std::string id, name, result, station, category, reveal; int count = 1, gold = 0, level = 1, difficulty = 1, skill = 1; bool known = true; std::vector<std::pair<std::string, int>> mats; };
struct AffixDef { std::string id, name, dmgType; bool prefix = true; int tier = 1, valuePct = 160; std::vector<std::string> types; std::vector<std::pair<std::string, int>> mods; };
struct Bounty { std::string id, kind, target, targetName, where, whereName, town, title, text; int need = 1, have = 0, reward = 0, xp = 0, posted = 0; bool done = false, taken = false; };
// bounties.txt: [board] = a notice board and what it posts; [bounty] = a hand-written notice a board always offers
struct HintDef { std::string cond, title, text; };   // hints.txt: tutorial guidance shown while its condition holds (guide.cpp)
// factions.txt (1.1.2): reward= favours claimed once at a notice board when standing reaches rep; project= civic works,
// delivered one after another at the board for reputation (the only way past Friendly besides story choices)
struct FactionReward { int rep = 10; std::string items, title; };
struct FactionProject { std::string title, text; std::vector<std::pair<std::string, int>> mats; int rep = 10, gold = 0, xp = 0; };
struct FactionDef { std::string id, name, text; std::vector<FactionReward> rewards; std::vector<FactionProject> projects; };   // a town or group the party has a reputation with
struct BoardDef { std::string id, name, map, location; int offers = 3, reward = 100, rep = 3; std::vector<std::string> kinds{"kill", "fetch", "deliver"}, kill, creatures, fetch, deliver; bool fromFile = false; };
struct FixedBountyDef { std::string id, board, kind, target, where, title, text, require; int need = 1, reward = 30, xp = 20; bool repeat = false; };
struct WorldLocation { std::string id, name, mapId, kind; int x = 0, y = 0, tx = 0, ty = 0, reveal = 0; };   // reveal: entering its map uncovers the overworld this far around it
// A place on a non-overworld world map (worldmap=<map id> / current): one per exit of that map, named after where it leads.
struct WorldPlace { std::string name, mapId; int x = 0, y = 0; bool known = false, visited = false; };

struct EpilogueSlide { std::string group, title, text; Tile tile; bool hasTile = false, recolor = false; Color8 color = C::White; };   // epilogue.txt (ending.cpp)

struct SaveSlotInfo { bool exists = false; std::string hero, mapName, campaign, campaignId; int level = 0, playtime = 0; long long saved = 0; };   // campaign = its name ("" = the main story)

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
    std::string combatMusic;              // 1.3.1 (#47) combatmusic=: fight music (empty = the "combat" track); boss music is the boss creature's music= (1.3.2)
    std::string defeat;                   // 1.1.7 defeat=SCRIPT: a lost fight doesn't end the game - the party gets back up and this runs (the tutorial)
};
std::vector<CampaignInfo> listCampaigns(const std::string& assetDir);   // base game first
// 1.7.0 (#145): the campaigns that come with the game (the main story, the Tutorial) can't be deleted
inline bool isCoreCampaign(const std::string& id) { return id.empty() || id == "triffall" || id == "tutorial"; }
// removes assets/campaigns/<id> and its saves (saves/campaigns/<id>); false + err when it can't (core, unknown, locked files)
bool deleteCampaignFiles(const std::string& assetDir, const std::string& saveDir, const std::string& id, std::string& err);
CampaignInfo readCampaignInfo(const std::string& file, const std::string& id, const std::string& dir);
SaveSlotInfo readSaveSlotInfo(const std::string& path);
// 1.6.1 (#135): every campaign keeps its own 3 slots + autosave. The main story's stay in the save folder itself (as
// before); another campaign's are in <saves>/campaigns/<id>/. migrateCampaignSaves moves older saves of other
// campaigns out of the shared slots once (saves.cpp).
std::string campaignSaveDir(const std::string& saveDir, const std::string& campaignId);
std::string campaignSlotPath(const std::string& saveDir, const std::string& campaignId, int slot);   // slot 0-2, 3 = autosave
int migrateCampaignSaves(const std::string& saveDir);
bool writeSaveFile(const std::string& path, const std::string& text);   // atomic, keeps the previous file as path.bak
int creatureResistance(const CreatureDef& cd, int dmgType);   // 1.5.0 (#96): res= plus the undead / machine rules (journal.cpp)

struct GameSettings {
    bool fullscreen = true;
    int scale = 2;
    bool vsync = true;
    bool integerScaling = true;
    bool crt = false;
    bool classic43 = false;         // 1.2.2 (#39): the old 640x480 picture with bars at the sides; off = the game fills the screen
    int masterVolume = 80, musicVolume = 70, sfxVolume = 80;
    int ambientVolume = 60;         // wind, rain, waves, town bustle... under the music (its own slider since 1.0.7)
    bool gamepad = true;
    bool screenShake = true;
    bool fastCombat = false;
    bool interactWhileMoving = false;
    bool edgeScroll = true;
    bool padHoldEndTurn = false;    // 1.8.7: Settings > Controls > Gamepad: End turn on the pad needs a half-second hold
    bool padCursorFoe = false;      // 1.8.7: a turn's fight cursor starts on the nearest foe in sight (pad / keyboard), not on the unit
    int banter = 2;                 // companion chatter: 0 off, 1 rare, 2 normal, 3 often
    // accessibility (Settings > Access)
    bool largeText = false;         // dialogue, messages, cutscenes, tutorial hints and the epilogue in double-size text
    bool colorblind = false;        // blue / orange instead of green / red for sides, hit chances and cover
    int textSpeed = 1;              // dialogue text: 0 slow, 1 normal, 2 fast, 3 instant
    bool reduceFlashing = false;    // no lightning flashes, softer hit flashes
    bool nightClassic = false;      // 1.8.0 (#160): Settings > Display "Night colours": false = Stage (grey-blue palette + lantern circle), true = Darken (classic)
    int theme = 0;                  // 1.8.3 (#170): Settings > Display "Theme": ThemeId (0 frame = Adventurer's Frame, 1 native, 2 onebit); settings.ini theme=frame|native|onebit, missing/unknown = frame
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
    int difficulty = 2;   // 1.5.0 (#90): always 2 (the old Hard); kept for old saves, never chosen
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
    void rerollWeather(bool mustChange = false, bool quiet = false, Rng* own = nullptr);   // own: roll with that generator, not the game's
    void nightWeather();                // 1.8.6 (#202): a night slept through (an inn): the weather has most likely changed by morning
    void drawWeather(Canvas& cv, float dt);
    // 1.8.0 (#160-#162): night and dark maps. After ground, trails and props (before objects and people) drawWorld runs
    // this per tile in view: outdoors at night the scenery goes through the grey-blue Stage ramp, except the stepped
    // lantern circle around the party, which keeps the day colours; on dark maps lit tiles keep the day colours banded
    // by the nearest light, the rest takes the ramp. viewBg = the bg each view tile was filled with (water banks).
    // Returns the per-view-tile dim (256 = none) for NPCs / monsters / objects outside the circle, empty = no dimming.
    std::vector<uint16_t> sceneryLook(Canvas& cv, int cx, int cy, int x0, int y0, int x1, int y1, const std::vector<Color8>& viewBg, bool combat);
    bool partyLightOn() const;          // a torch / Light spell burning or light gear worn (the lantern circle doubles)
    // 1.8.1: the lantern circles at night outdoors (the leader / active fighter's radius 4 or 8, plus radius 2 round
    // every party unit in a fight), used by sceneryLook and darkHides
    struct NightLamp { int x, y; float r; };
    std::vector<NightLamp> nightLamps(bool combat) const;
    // 1.8.1 (#165): true = the tile lies outside every light, so objects, NPCs and monsters on it aren't drawn: on dark
    // maps any tile not both seen and lit; outdoors from half-dark on (19:30 - 06:00) any tile outside the lantern circles
    bool darkHides(int x, int y, bool combat) const;
    bool shopOpen(const std::string& shopId, std::string& hoursOut) const;
    std::map<std::string, std::string> extra;    // free-form persistent key/values (saved in [extra])
    // ---- stats, bestiary, companions, achievements (journal.cpp) ----
    GameStats stats;
    std::map<std::string, int> bestiary;          // creature id -> kills (present = seen)
    std::map<std::string, int> approval;          // companion class id -> approval
    // 1.1.2 companions (standing.cpp): approval only moves through choices they witness, gifts and their own quests.
    // Strained (<= -15) fight worse, at -40 they leave and go home; Loyal (talked to at 30+, own quest done) fight better.
    static const char* approvalTier(int a); int approvalOf(const std::string& cls) const; bool companionLoyal(const std::string& cls) const;
    void changeApproval(const std::string& cls, int d); void syncBonds(); void checkCompanionMoods(); void companionLeaves(size_t idx, const std::string& why = "");
    void companionFalls(size_t idx, const std::string& why);   // 1.5.0 (#87/#88): fell=CLASS - gone for good (fell_<class>)
    int memberOfClass(const std::string& cls) const;
    void dropMemberRefs(size_t idx);                            // after a companion leaves: fix level-up queue / banter indexes           // party index of the companion of that class (-1: not with you)
    void fightGroup(const std::string& flag);
    void postBoardNotes(const std::string& town);               // 1.5.0 (#98): bounties.txt kind=note (bounty.cpp)                   // 1.5.0 (#85): fight=FLAG - the map's enemy group with that flag= attacks now
    // 1.5.0 (#89): contradictions.txt - flag combinations that must never all be true; a warning the first time they are
    struct Contradiction { std::vector<std::string> terms; std::string note; };
    std::vector<Contradiction> contradictions; std::set<std::string> contradictionsWarned;
    void loadContradictions(); void checkContradictions(const std::string& changed);
    static std::vector<Contradiction> parseContradictions(const std::string& text);
    static std::string contradictionFlag(const std::string& term);
    // 1.4.0 (#81) taught abilities: teach=WHO:SPELL[|LOYAL_SPELL] (WHO = companion NPC id, class id, member name or hero).
    // Companions' lessons are kept in extra["taught_<class>"] and applied whenever they are in the party (syncBonds):
    // the LOYAL_SPELL replaces SPELL while the companion is Loyal, and goes back to SPELL if the loyalty breaks.
    void teachAction(const std::string& v); void applyTaught(const std::string& onlyCls, bool announce);
    // towns: standing page on the notice board (favours, civic projects, amends); Disliked = no work, Hated = no trade
    int repOf(const std::string& id) const; const FactionDef* faction(const std::string& id) const; bool shopRefuses(const std::string& shop);
    int amendsCost(const std::string& id) const; int projectsDone(const std::string& id) const;
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
    std::string boardTown; int bountySel = 0, boardPage = 0, standSel = 0;   // boardPage 0 = notices, 1 = town standing
    std::vector<BoardDef> boards; std::vector<FixedBountyDef> fixedBounties;
    void loadBoards(); const BoardDef* board(const std::string& id) const; std::string boardName(const std::string& id) const;
    std::vector<WorldLocation> locations;
    int jTab = 0, jSel = 0, jScroll = 0;          // journal
    int wmSel = -1, wmScroll = 0;                 // world map
    std::string craftStation; int craftSel = 0, craftScroll = 0;
    std::string consoleBuf; std::vector<std::string> consoleHist, consoleOut; int consoleHistPos = -1;
    void loadSystems();                           // banter, achievements, recipes, affixes, locations
    void onKill(const Character& victim, const Combatant* killer);
    void onSeen(const std::string& creatureId) { if (!bestiary.count(creatureId)) { bestiary[creatureId] = 0; freshSeen.push_back(creatureId); } }
    std::vector<std::string> freshSeen;   // 1.5.0 (#95): creature kinds met for the first time in the fight being set up
    void firstSightBanter();              // a companion names the trick against a family the first time the party meets it
    void addNote(const std::string& title, const std::string& text);
    void banterTrigger(const std::string& when);
    void drawBanter(Canvas& cv);
    void checkAchievements(bool force = false);
    void unlockAchievement(const std::string& id);
    void loadAchievementState(); void saveAchievementState() const;
    void updateJournal(InputState& in, float dt); void drawJournal(Canvas& cv, InputState& in);
    void drawAchievementPopups(Canvas& cv, float dt, bool inGame = false);
    std::string questText(const QuestDef& q) const; std::string questPin(const QuestDef& q) const;
    // world map (worldmap.cpp)
    void loadLocations();
    bool wmTravel = false;                        // opened by a coachman: travel allowed, for a fare
    int travelFare(const WorldLocation& l) const;
    bool wmPick = false; std::string wmPickPrompt, wmPickResult;   // 1.1.10: a script's pick: choose a visited place, nothing else happens
    void openWorldMap(bool travelMode = false, bool pick = false); void updateWorldMap(InputState& in, float dt); void drawWorldMap(Canvas& cv, InputState& in);
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
    bool canCraft(const Recipe& r) const; bool craft(const Recipe& r); int craftMax(const Recipe& r, int cap = 20) const;
    std::string craftSelect(const std::string& recipeId);   // headless: select a recipe on the Craft tab; "" = not listed
    std::string makeAffixed(const std::string& baseId, int tier);   // returns the generated item id (registered in gd->items)
    // crafting depth: station skills, recipe discovery, success/quality rolls, upgrades (+1..+3), enchanting, salvage
    int craftTab = 0, craftQty = 1;                                  // 0 craft, 1 upgrade, 2 enchant, 3 salvage
    int craftSkill(const std::string& station) const;                // 1..6
    void addCraftXP(const std::string& station, int xp);             // station: forge|alchemy|workbench|field; announces a new level
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
    void updateStanding(InputState& in); void drawStanding(Canvas& cv, InputState& in);
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
    bool faceRight = true;   // 1.1.5 (#27): which way the party sprites look; only a sideways step or turn changes it (up/down keep it)
    // utility-spell effects outside combat
    int lightSteps = 0, stealthSteps = 0, detectSteps = 0;
    int lightPower = 0;              // radius of the light the party carries while lightSteps > 0 (torch 5, Light spell 8)
    std::vector<uint8_t> litMask;    // dark maps: which tiles are lit (party light, torches, braziers, combat lights)
    std::vector<float> lightDist;    // 1.8.0 (#162): per lit tile, the distance (tiles) to the nearest light that reaches it
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
    bool playtest = false;           // started from the Editor's Play: the pause menu offers Back to Editor (1.1.3)
    bool requestEditor = false;      // pause menu > Back to Editor: the app reopens the Editor, the game keeps running
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

    bool init(GameData* data, GameSettings* s, const std::string& assets, Tileset* ts, Font* font);
    // demo build (TRIFFALL_DEMO, or the console's 'demo 1'): the base campaign is limited to demo.txt's maps
    bool demoMode = false; std::set<std::string> demoMaps; std::string demoEnd, demoBlockedText, demoTitle, demoText, storeUrl;
    void loadDemo(); bool demoBlocks(const std::string& map) const; void startDemoEnd();
    void newGame(const std::string& heroName, const std::string& classId, int difficulty = 1);
    void update(InputState& in, float dt, Canvas& cv);
    void draw(Canvas& cv, InputState& in);
    bool canSaveHere() const;
    std::string musicWanted() const;            // which music track fits the current mode/map
    std::string ambientWanted() const;          // the map's ambient= loop (1.3.1: none in the base game)
    std::string weaponSound(const Character& ch) const;
    std::string spellSound(const SpellDef& s) const;
    std::string deathSound(const Character& ch, bool friendly) const;   // 1.3.3: creature sound=, else death_enemy / death_party
    std::string serialize() const;
    bool deserialize(const std::string& text);
    bool saveToSlot(int slot);                     // slot 0-2, or AUTOSAVE_SLOT (saves.cpp)
    bool loadFromSlot(int slot);
    static const int AUTOSAVE_SLOT = 3;
    std::string slotPath(int slot) const;
    bool loadSaveFile(const std::string& path);   // falls back to the .bak copy when the file is damaged; loadError says why not
    std::string loadError;
    void autosave(); int lastAutosave = -1; bool loadingSave = false;
    std::string autosavedAt;           // 1.8.3: where the last autosave was made this session (the frame HUD shows it)
    void migrateSave(std::vector<ConfigSection>& secs, int fromFormat);
    std::string crashState() const; void emergencySave();
    void refreshSlots();
    void openSaveLoad(bool save);
    void reloadContent();            // after the editor changed data files
    bool mouseOverUI(const InputState& in) const;
    void uiRect(int x, int y, int w, int h) { uiRects.push_back({x, y, w, h}); }
    void camCenterOn(int tx, int ty, bool snap = false);
    void camClamp();
    void screenChanged();   // 1.2.2 (#39): App::setScreen changed SCREEN_W/H
    // 1.2.2: screens laid out for 640x480 (party menu, shop, crafting, journal, notice board, level up, world map) are
    // drawn into subCv and put in the middle of a larger screen; their update sees the mouse in those coordinates.
    static bool subViewMode(GMode m);
    void drawModeUI(Canvas& cv, InputState& in);   // the open screen, hotbar, hints and notifications (Game::draw)
    bool subViewOrigin(GMode m, int& x, int& y) const;   // 1.8.3: a 640x480 screen's place in the theme's menu area (false: drawn as it is)
    bool modeArea(GMode m, Rect& r) const;               // 1.8.3: where a mode draws with its own SCREEN_W/H: a 640x480 menu, or a popup (dialogue, message, victory, save/load, name) in the theme's world area
    void drawMenuBackdrop(Canvas& cv, GMode m, InputState& in);          // 1.8.3: the theme's panel / dialog / window behind a menu screen
    // 1.8.5 (#196 #199): every full-screen RPG menu sits in the theme's own host: the frame's carved panel, the native
    // dialog, the one-bit window, sized like the themed Party tab and with the same tab header on every Party tab.
    // The old 640x480 layouts are drawn inside it (SCREEN_H may then be under 480: lists size themselves to it).
    Rect menuChromeRect() const;                          // the host window / panel on the screen
    bool menuHosted(GMode m) const;                        // the screen fits in the host (else the old full-screen look)
    Rect menuHostedView(GMode m) const;                    // where the 640-wide layout goes inside the host
    std::string menuHostTitle(GMode m) const;
    int drawMenuTabs(Canvas& cv, InputState& in, const Rect& chrome);   // the Party screen's tabs; returns the content's top
    void hostTitle(int x, int y, const std::string& s);   // a hosted frame screen's own title line (native / one-bit: in the title bar)
    bool menuHostedNow = false;                            // set while a hosted screen updates / draws
    int invRows() const { return std::max(6, std::min(20, (UI::SCREEN_H - 110) / 16)); }    // pack list rows that fit
    int shopRows() const { return std::max(6, std::min(22, (UI::SCREEN_H - 100) / 16)); }   // shop list rows that fit
    int craftRows() const { return std::max(6, std::min(17, (UI::SCREEN_H - 122) / 18)); }  // crafting list rows that fit
    std::string interactVerb(std::string* what = nullptr); // 1.8.5 (#197): what Interact would do now ("Talk", "Open"...; empty = nothing in reach)
    Canvas subCv;
    // ---- 1.8.3 (#170-#183) UI themes: layout, the world view, the HUD's clickable parts ----
    void computeLayout();                 // UI::VIEW_* and the theme's HUD rects for this screen size
    Canvas worldCv;                       // the world is drawn here (VIEW_W x VIEW_H), then put at VIEW_X, VIEW_Y
    static bool inView(const InputState& in) { return in.mouseX >= UI::VIEW_X && in.mouseY >= UI::VIEW_Y && in.mouseX < UI::VIEW_X + UI::VIEW_W && in.mouseY < UI::VIEW_Y + UI::VIEW_H; }
    Rect menuArea() const;                // where full-screen menus (party, shop, journal...) go in this theme
    Rect popupArea() const;               // where dialogue, messages, victory, save/load go (the world view's part of the screen)
    // HUD hot spots: drawn controls register here; update acts on last frame's (HH_ACTION: press that action's key,
    // HH_MEMBER: open that member's sheet, HH_BELT: use hotbar slot, HH_COMBAT: a combat command, HH_UNIT: look at a
    // unit, HH_PAUSE: a pause entry id, HH_CRAFT: field crafting, HH_CLOSE: close a tool window)
    enum { HH_ACTION = 1, HH_MEMBER, HH_BELT, HH_COMBAT, HH_UNIT, HH_PAUSE, HH_CRAFT, HH_TOOL,
           HH_PTAB, HH_SLOT, HH_PICK, HH_LEAD, HH_RANKUP, HH_HOSTCLOSE,   // party screen: tab, equipment slot, equip-list row (0 = remove), lead, class +1
           HH_CLOSE, HH_FAVPICK };   // 1.8.6: HH_CLOSE = a window's close box (arg -1: Esc for the open popup; >= 0: one-bit HUD window to roll up), HH_FAVPICK = a favorites picker slot
    // 1.8.3 (#173 #177 #181): the Party tab drawn in the theme's own layout (frame: hud.cpp, native: hud_native.cpp,
    // onebit: hud_onebit.cpp) when the screen has room; its clicks come through the HUD hot spots (updateMenu).
    bool themedParty() const;
    void drawThemedParty(Canvas& cv, InputState& in);
    void drawFrameParty(Canvas& cv, InputState& in);
    void drawNativeParty(Canvas& cv, InputState& in);
    void drawOneBitParty(Canvas& cv, InputState& in);
    std::vector<std::string> equipCandidates(int slot) const;   // pack items for a slot (the equip picker)
    const ItemDef* partyPointedItem() const;                    // what the party screen points at (Compare panels)
    struct HudHit { Rect r; int kind, arg; };
    std::vector<HudHit> hudHits, hudHitsPrev;
    void hudHit(int x, int y, int w, int h, int kind, int arg) { hudHits.push_back({{x, y, w, h}, kind, arg}); }
    const HudHit* hudHitAt(int mx, int my) const { for (auto it = hudHitsPrev.rbegin(); it != hudHitsPrev.rend(); ++it) if (it->r.contains(mx, my)) return &*it; return nullptr; }
    bool hudClick(InputState& in);        // a click on a HUD hot spot: does what it stands for (true = handled)
    int injectAction = -1;                // a HUD button stands in for this action's key on the next update
    int hudPick = -1;                     // pad / keyboard focus inside the HUD's lists (combat actions, pause menu)
    // theme HUDs (hud.cpp)
    void drawFrameHud(Canvas& cv, InputState& in);
    void drawNativeHud(Canvas& cv, InputState& in);
    void nativeHudInput(InputState& in);   // 1.8.3 (#176-#178): native menu bar, Alt keys, tool windows (hud_native.cpp; update calls it first)
    void drawNativeNotes(Canvas& cv);      // 1.8.3 (#176): notifications as tray balloons (drawModeUI calls it in the native theme)
    void drawOneBitHud(Canvas& cv, InputState& in);
    void drawCombatWorld(Canvas& cv, InputState& in);   // combat overlays on the battlefield (world canvas)
    void drawCombatPopups(Canvas& cv, InputState& in);  // the action / spell / item lists, the goal, the round banner
    std::vector<std::pair<std::string, Color8>> tileInfoLines(int tx, int ty);   // what the combat cursor is on (target cards)
    Color8 logColor(const std::string& line) const;     // a message's meaning colour (joins, achievements, fights...)
    std::string hudPlaceTime() const;
    // in-game Settings (settings_ui.cpp)
    void openSettings();
    void drawSettingsScreen(Canvas& cv, InputState& in);
    void updateSettingsScreen(InputState& in);
    std::string settingsProbe(const std::string& what);   // 1.8.6: tests (page, focus, kind, x, y, combo, combosel, theme)
    PlatformRequests* req = nullptr;      // set by App: Settings asks the platform for fullscreen / scale / vsync
    bool spellUsableOutside(const SpellDef& s) const;
    bool spellUsableInCombat(const SpellDef& s) const { return s.dmgMax > 0 || s.heal > 0 || !s.effect.empty() || s.utility == "light" || !s.summon.empty() || s.smoke > 0; }
    bool castUtility(int member, const std::string& spellId, int target);
    void useFavorite(int member, int slot);
    // 1.8.6 (#206): the favorites picker. A controller has no number keys: the Favorites button (pad RT) opens a strip of
    // the 8 slots; Left / Right pick, Confirm uses the slot (or puts favAssign on it), Cancel / the button close it.
    // Taken in Game::update before the screen's own update, which then sees no input.
    int favPick = -1;                     // the chosen slot while open (-1 = closed)
    int favPickMember = 0;                // whose favorites
    std::string favAssign;                // non-empty ("spell:id" / "item:id"): put it on the chosen slot instead of using it
    void openFavPicker(int member, const std::string& assign);
    void updateFavPicker(InputState& in);
    void drawFavPicker(Canvas& cv, InputState& in);
    void combatUseFav(int slot);          // the acting unit uses its favorite (#201: the same slot again while aiming it cancels)
    std::vector<int> combatFoesInSight(int unit);   // 1.8.7: living foes that unit's side can see, nearest first (target jumping)
    int favLitSlot(int member) const;     // #201: the slot whose spell / item the fight is aiming now (-1 none): the HUDs light it
    int closeOffX = 0, closeOffY = 0;     // 1.8.6: where the screen being drawn sits (close boxes drawn in a sub-view)
    bool obShade[4] = {false, false, false, false};   // 1.8.6 (#205): one-bit HUD windows rolled up to their title bar (party, news, favorites, target)

    // helpers used by subsystems
    bool hasFlag(const std::string& f) const { auto it = flags.find(f); return it != flags.end() && it->second != 0; }
    void setFlag(const std::string& f, int v = 1) { flags[f] = v; if (!contradictions.empty()) checkContradictions(f); }
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
    // 1.1.10 (#34) scripts (script.h / script.cpp): data/scripts/<id>.txt, run by script=<id> on NPCs, script:<id> in any
    // action list (dialogue, onuse=, triggers, ondefeat=, defeat=), map onenter=, items effect=script:<id>
    std::vector<std::shared_ptr<ScriptRun>> scriptRuns; std::map<std::string, std::shared_ptr<ScriptProg>> scriptCache;
    std::string pendingOnEnter;   // the map's onenter=, run once the party stands on the map
    int scriptNextId = 1, scriptDlgOwner = 0; bool scriptDlgShown = false; bool lastFightWon = false; std::string lastScriptError;
    std::shared_ptr<ScriptProg> scriptProg(const std::string& id);
    bool startScript(const std::string& id, const std::vector<SVal>& args = {}, NPC* npc = nullptr, const std::map<std::string, SVal>& locals = {});
    bool startScriptText(const std::string& code, const std::string& name = "console");
    void tickScripts(float dt);
    void scriptDialogueDone(int option);
    std::string keptVar(const std::string& name) const { auto it = extra.find("var_" + name); return it == extra.end() ? std::string() : it->second; }
    void startCombat(const std::vector<std::string>& creatureIds, int cx, int cy, int levelBonus, const std::vector<int>& groupIndices, const std::string& onDefeat = "");
    std::vector<int> combatExtraGroups;   // map groups that join the next startCombat from round one
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
    // 1.4.0 (#75/#76): freeze/shock/emp/light obstacles, crates and pressure plates (worldstate.cpp)
    bool obstacleNeeds(const Obstacle& o, const std::string& what) const;   // its needs= lists this (freeze, shock, emp, light, force, spell:X...)
    int utilityCaster(const std::string& utility, std::string* spellId, bool needMp = false) const;   // a party member knowing a spell with that utility= (-1 none)
    bool utilityOnObstacles(const std::string& utility, int mi, const std::string& spellId);   // a utility spell cast from the menu next to an obstacle it clears
    void revealHidden(const std::string& by, int radius);                // light / detect: hidden things near the party show themselves
    void resetVisitObstacles(Map& m);                                     // frozen water thaws when the party leaves
    void resetCrates(Map& m);                                             // crates back to their placed spots (on arrival, action resetcrates)
    bool pushCrate(Crate& c, int dx, int dy);                             // one tile; false = blocked
    void crateMenu(int idx, int dx, int dy);                              // a heavy crate: heave it (STR / force spell) or leave it
    void updatePlates(bool fire);                                         // plates down under crates / the leader; fire = run onpress / onrelease
    int platesIn(const std::string& group, bool downOnly) const;
    // 1.4.0 (#76/#77): the text box (GMode::NAME): script input statement, terminals, combination locks
    std::string inputBuf, inputTitle, inputPrompt; int inputOwner = 0, inputMax = 24;
    void openTextInput(const std::string& title, const std::string& prompt, int owner);
    void textInputDone(bool ok);
    // 1.4.0 (#77): desktop files (data/lore) that a game unlocks (first line #locked); kept in files.ini, like achievements, for every save slot
    std::set<std::string> foundFiles;
    void loadFoundFiles(); void unlockLoreFile(const std::string& name);
    // 1.5.0 (#96) Field Notes: what the party has found out about each kind of creature - the damage types it was hit
    // with (bit per DT_*) and things it was seen doing (tags: spell:ID, windup:ID, ward:CREATURE, darkvision, studied).
    // Kept in fieldnotes.ini in the save folder for every slot (like files.ini), so a lost fight and a reload still teach.
    // res = the rk_<creature> int as a whole: bits 0-5 damage types, bits 8+ weapon traits (master's weaponTraits()).
    struct FieldNote { int res = 0; std::set<std::string> tags; };
    static const int FN_ALL = 0xFFFF;   // everything known (three slain)
    std::map<std::string, FieldNote> fieldNotes;
    void loadFieldNotes(); void saveFieldNotes() const;
    bool noteCreature(const std::string& creature, int resBits, const std::string& tag = "");   // true = something new (saved at once)
    int knownResBits(const std::string& creature) const;   // this save's rk_ bits | field notes | every type once 3 are slain
    std::vector<std::string> bestiaryIds() const;           // the Journal's bestiary list: met in this save + in the Field Notes
    void applyLearnMigrations();                            // 1.5.0 (#86): migrations.txt [learn] recipe=id|condition
    // 1.5.0 (#99) stealth and avoidance (stealth.cpp): noticing, thrown noisemakers (effect=lure), traps
    int groupAggro(const EnemyGroup& e) const;              // how near the party may come before the group notices it
    int distractGroups(int x, int y, int steps, int radius);   // groups near (x,y) go and look; returns how many
    std::string throwItem; int throwX = 0, throwY = 0;      // aiming a noisemaker on the map (empty = not aiming)
    void startThrowAim(const std::string& itemId);
    bool updateThrowAim(InputState& in);                    // true = aiming ate the input this frame
    bool throwTargetOk(int x, int y) const;
    void throwLure(const std::string& itemId, int x, int y);
    bool stepDistracted(EnemyGroup& e);                     // a timer step for a group busy with a noise / walking home
    void distractionStep();                                 // one party step: noises hold their groups a step less
    void resetTraps(Map& m);
    void springTrap(Trap& t);
    bool groupOnTrap(EnemyGroup& e);                        // the group stands on an armed trap: hurt or gone
    void partyOnTrap();
    void applyGroupHurt(Combatant& u, int groupIndex);
    void drawTraps(Canvas& cv, int cx, int cy, bool combat);
    void drawThrowAim(Canvas& cv);
    bool gatherReady(const GatherNode& g) const { auto it = extra.find("gather_" + mapId + "_" + std::to_string(g.x) + "_" + std::to_string(g.y)); return it == extra.end() || day - atoi(it->second.c_str()) >= g.respawn; }
    bool interactable(int x, int y) { return map->crateAt(x, y) || map->npcAt(x, y) || map->chestAt(x, y) || map->doorAt(x, y) || map->signAt(x, y) || map->enemyAt(x, y) || map->obstacleAt(x, y) || map->leverAt(x, y) || map->gatherAt(x, y); }
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
    bool visible(int x, int y) const;
    void revealAround();
    void randomEncounter(bool force = false, bool now = false);
    // 1.2.4 (#41) touch=1 maps: leash chase and ambushes that appear a few tiles away
    int leashOf(const EnemyGroup& e) const;
    bool groupShown(const EnemyGroup& e) const;
    void refreshGroupsShown(Map& m);   // 1.5.0 (#84): EnemyGroup::hidden from show=/hide=
    bool groupStepToward(EnemyGroup& e, int tx, int ty, int reach, int limit);
    void touchChase();
    bool spawnAmbushers(const std::vector<std::string>& creatures, int levelBonus);
    // dialogue
    void updateDialogue(InputState& in, float dt);
    void drawDialogue(Canvas& cv, InputState& in);
public:
    int textScale() const;                       // 2 with Settings > Access > Large text
    int dialogueOptionH() const; int dialogueOptionsY(int n) const;
    int dialogueOptionsVisible(int n) const { return std::min(n, textScale() == 2 ? 6 : 12); }   // 1.1.10: long answer lists scroll
    int dlgOptFirst = 0;
private:
    void showNode(const std::string& node);
    void runAction(const std::string& k, const std::string& v);
    void runActionInner(const std::string& k, const std::string& v);
    void resumeScript(std::shared_ptr<ScriptRun> r);
    bool scriptExec(ScriptRun& r);   // one statement; false = stop for now (waiting or finished)
    void scriptFail(ScriptRun& r, const std::string& msg);
    SVal scriptEval(ScriptRun& r, const SExpr& e, std::string& err);
    SVal scriptValue(ScriptRun& r, const std::string& name);
    SVal scriptFn(ScriptRun& r, const std::string& name, const std::vector<SVal>& a, std::string& err);
    void scriptAssign(ScriptRun& r, const std::string& name, const SVal& v);
    bool scriptShowTalk(ScriptRun& r, const std::string& text, bool withOptions);
    NPC* scriptNpc(const ScriptRun& r);
    const WorldLocation* scriptPlace(const std::string& id) const;
    bool loadDialogues();
    // combat (combat.cpp)
    void updateCombat(InputState& in, float dt);
    void combatComputeReachable();
    void combatNextTurn();
    void combatBeginTurn();
    void combatEndTurn();
    bool combatAttack(int attacker, int target, bool aimed);   // true = it hit
public:
    // 1.4.0 test hooks (headless "cb ..." command and "expect cb:..." keys; combat.cpp)
    int testForceHit = -1;                 // -1 roll as usual, 1 every attack hits, 0 every attack misses
    std::string combatProbe(const std::string& what);
    std::string combatDo(const std::vector<std::string>& args);
private:
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
    int  critChance(const Combatant& a, const Combatant& t, bool aimed) const;
    bool combatTryFlee(int unitIndex);   // 1.5.0: the Flee action (also the auto party's way out of a fight it can't win)   // 1.5.0 (#83): capped at CRIT_CAP (tagged = sure crit outside it)
    static constexpr int CRIT_CAP = 35;
    std::string dmgTrait;   // 1.5.0 (#92): the weapon trait of the hit applyDamage is handling ("" = not a weapon hit)
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
    struct TargetOdds { int unit = -1; bool foe = false, inRange = false, flanked = false, exposed = false; int hit = 0, crit = 0, dist = 0, range = 0, cover = 0; };
    TargetOdds targetOdds();   // 1.8.3: the active unit against what the combat cursor is on
    void addFloat(int tx, int ty, const std::string& s, Color8 c, int scale = 1);
    void spawnLoot(Combatant& u);
    // ---- 1.4.0 combat (combat.cpp) ----
    int nextSurprise = 0;                            // (#80) set just before startCombat: 1 the party catches them unaware, -1 ambushed
    struct FightChange { std::string map; int x = 0, y = 0; std::string kind; };
    std::vector<FightChange> fightChanges;           // (#58) cells changed in fights (broken cover, snuffed braziers): put back when the party leaves the map
    void revertFightChanges();
    bool combatLOS(int x0, int y0, int x1, int y1) const;   // line of sight that smoke blocks too
    int  groundAt(int x, int y, int kind) const;     // rounds left of a ground effect (0 = none)
    bool smokeAt(int x, int y) const { return groundAt(x, y, 3) > 0; }
    void addGround(int x, int y, int kind, int turns);
    void groundRoundTick();
    void groundHurt(int ui);
    int  coverVs(int x, int y, int fx, int fy, int self, bool friendlySide, int* sides = nullptr) const;
    int  effCover(const Combatant& a, const Combatant& d, bool* arced = nullptr) const;   // cover after the attacker's ways round it
    bool exposed(const Combatant& a, const Combatant& d) const;
    bool targetable(const Combatant& a, const Combatant& d) const;
    bool marked(const Combatant& a, const Combatant& d) const;
    SpellDef castSpell(const Character& c, const SpellDef& sp) const;   // (#66) the spell with the caster's rank upgrades
    std::vector<std::pair<int, int>> spellTiles(int ci, const SpellDef& sp, int tx, int ty) const;
    bool singleTargetSpell(const SpellDef& sp) const;
    int  spellCoverPct(const Combatant& c, const Combatant& t, const SpellDef& sp) const;
    std::vector<int> chainTargets(int ci, const SpellDef& sp, int first) const;
    int  applyCombos(Combatant& t, int dmg, int dmgType, Combatant* attacker);
    std::string comboPreview(const Combatant& t, int dmgType) const;
    void damageTiles(const std::vector<std::pair<int, int>>& tiles, int dmg, int dmgType);
    void changeCell(int x, int y, const std::string& kind);
    void conductShock(int x, int y, int dmg, Combatant* src, const std::vector<int>& already);
    void knockBack(int ci, int ti);
    void windupStart(int ui, int ti);
    bool windupResolve(int ui);
    void leaderFell(const Combatant& leader);
    void goalSetup(const EnemyGroup& g, int gi);
    void goalCheck(bool roundStart);
    void unitGone(Combatant& u, const std::string& why);
    int  aiSmart() const { return 2; }   // 1.5.0 (#90): enemies always play smart (the old Hard)
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
    int drawItemDetails(Canvas& cv, const ItemDef& d, int x, int y, int w, int maxY, const Character* who = nullptr);
    // 1.5.0 (#93): plain lines about who uses an item well and what it costs `who` (red = a drawback for them)
    std::vector<std::pair<std::string, Color8>> gearNotes(const ItemDef& d, const Character* who) const;
    std::string gearWarning(const ItemDef& d, const Character& who) const;   // one short line, "" = no drawback   // stats grid + description (1.1.3); returns the y below it
    void openPause();                // Esc in explore or combat, the combat bar's Menu button
    struct PauseEntry { int id; const char* label; bool enabled; };
    std::vector<PauseEntry> pauseEntries() const;
    void updateSaveLoad(InputState& in, float dt);
    void drawSaveLoad(Canvas& cv, InputState& in);
    void updateCameraInput(InputState& in, float dt);
    void updateCutscene(InputState& in, float dt);
    void drawCutscene(Canvas& cv, InputState& in);
    void drawCharacterSheet(Canvas& cv, InputState& in, int member, int x, int y, int w, int h);
    void drawUnitSprite(Canvas& cv, const Character& ch, int sx, int sy, bool flip);
    int pauseSel = 0;
    bool loadQuests();
    bool loadEncounters();
    std::vector<uint8_t> visMask;
public:
    // ---- 1.6.0 game UX (#115-#118) ----

    // ---- 1.6.0 rules data (#128-#131) ----

    // ---- 1.6.0 story/lore (#122) ----

    // ---- 1.6.0 hub/playtest (#114) ----

};

std::string flagDescribe(const std::string& f);
const char* statHelp(int stat);   // one line: what a stat point does
