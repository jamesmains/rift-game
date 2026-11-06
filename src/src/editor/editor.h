// Editor: in-app content editor (maps, tile kinds, data files, overworld, dialogue).
#pragma once
#include <string>
#include <vector>
#include <map>
#include <functional>
#include <set>
#include "../core/gfx.h"
#include "../core/input.h"
#include "../core/util.h"
#include "../game/data.h"
#include "../game/world.h"
#include "../game/game.h"
#include "edui.h"
#include "../core/audio.h"

// A key=value section list that round-trips a config file (comments are not preserved).
struct EdSection { std::string name; std::vector<std::pair<std::string, std::string>> kv;
    bool inherited = false; std::vector<std::pair<std::string, std::string>> origKv;   // shown from the base game; edited -> becomes the campaign's own copy
    std::string get(const std::string& k, const std::string& def = "") const { for (auto& p : kv) if (p.first == k) return p.second; return def; }
    void set(const std::string& k, const std::string& v) { for (auto& p : kv) if (p.first == k) { p.second = v; return; } kv.push_back({k, v}); }
    bool has(const std::string& k) const { for (auto& p : kv) if (p.first == k) return true; return false; }
};
struct CampaignForm {
    std::string path, headerComments, name, desc, author, version, icon, startMap, items, flags, dialogue, worldMap;
    int startX = 0, startY = 0, gold = 25; std::vector<std::string> intro; std::vector<std::pair<std::string, std::string>> extra; bool dirty = false;
};
struct PlayState { bool on = false, dirty = false; int level = 1, gold = 500; std::string cls, flags, items, party; };   // Campaign tab > Play from a state (playtest.txt)
struct GenericEditor {
    std::string path, title, sectionName, listKey = "id";
    std::string basePath; bool replaceWhole = false;   // campaigns: base-game file shown underneath; replaceWhole = the game reads one file or the other, not both
    std::vector<std::string> templateKeys;     // keys offered for a new section
    std::string altSection; std::vector<std::string> altTemplateKeys;   // a second kind of entry in the same file (bounties.txt: [board] and [bounty])
    std::vector<EdSection> secs;
    int sel = -1, scroll = 0, fieldScroll = 0; bool dirty = false; std::string filter, newKey, status;
    // undo / redo: the editor compares the entries with last frame's copy and keeps snapshots (typing is merged)
    std::vector<std::vector<EdSection>> undo, redo; std::vector<EdSection> shadow; bool shadowInit = false; float lastChangeT = -10;
    bool load();
    bool save();
    std::string toText() const;              // what save() writes
    std::vector<int> filtered() const;
};

// Map model for the editor: cells hold legend specs ("tree|tree2"), objects keep raw key/values.
struct EdObj { std::string type; std::vector<std::pair<std::string, std::string>> kv;
    int geti(const std::string& k, int d = 0) const { for (auto& p : kv) if (p.first == k) return atoi(p.second.c_str()); return d; }
    std::string get(const std::string& k) const { for (auto& p : kv) if (p.first == k) return p.second; return ""; }
    void set(const std::string& k, const std::string& v) { for (auto& p : kv) if (p.first == k) { p.second = v; return; } kv.push_back({k, v}); }
};
struct EdMap {
    std::string path, id;
    std::vector<std::pair<std::string, std::string>> header; // [map] keys
    int w = 0, h = 0;
    std::vector<std::string> cells;                           // spec per cell
    std::vector<EdObj> objs;
    bool load(const std::string& p);
    bool save();
    std::string toText() const;               // the map file as save() writes it
    void init(int W, int H, const std::string& spec);
    void resize(int W, int H, const std::string& fillSpec);
    std::string& at(int x, int y) { return cells[(size_t)y * w + x]; }
    std::string hget(const std::string& k) const { for (auto& p : header) if (p.first == k) return p.second; return ""; }
    void hset(const std::string& k, const std::string& v) { for (auto& p : header) if (p.first == k) { p.second = v; return; } header.push_back({k, v}); }
};

// Simple multi-line text editor state (dialogue files).
struct TextEdit {
    std::vector<std::string> lines{""};
    int cx = 0, cy = 0, scroll = 0; bool dirty = false; std::string path;
    int ax = -1, ay = -1;                       // selection anchor (-1 = none)
    std::vector<std::vector<std::string>> undo; // snapshots
    bool hasSel() const { return ay >= 0 && !(ax == cx && ay == cy); }
    void selRange(int& x0, int& y0, int& x1, int& y1) const { if (ay < cy || (ay == cy && ax <= cx)) { x0 = ax; y0 = ay; x1 = cx; y1 = cy; } else { x0 = cx; y0 = cy; x1 = ax; y1 = ay; } }
    std::string selText() const;
    void deleteSel();
    void insert(const std::string& t);
    void snapshot() { undo.push_back(lines); if (undo.size() > 60) undo.erase(undo.begin()); }
    void load(const std::string& p);
    bool save();
    std::string text() const;
};
struct MapSnapshot { int w, h; std::vector<std::string> cells; std::vector<EdObj> objs; };
struct MapClip { int w = 0, h = 0; std::vector<std::string> cells; std::vector<EdObj> objs; };

class Editor {
public:
    static constexpr int W = 960, H = 720;
    bool wantsClose = false, changed = false, requestNewGame = false;
    int tab = 0;
    // playtest request ("Play here"): the app starts/moves the game to this spot after the editor closes
    bool playRequest = false; std::string playMap; int playX = 0, playY = 0;
    bool playFresh = false;                   // this playtest must start a new game (Play from the start with a state)
    PlayState play; std::string exportDir, lastExport;   // editor_export.cpp
    void loadPlayState(); bool savePlayState(); void drawPlayState(int x, int y, int w, int h, InputState& in);
    bool exportCampaign(std::string& outPath, int& fileCount, std::string& err); void drawExportPanel(int x, int y, int w, int h, InputState& in);
    bool previewingMusic = false;             // Sounds tab is previewing a track (the app leaves music alone)
    ~Editor() { delete worldPreview; }
    bool init(Tileset* ts, Font* font, GameData* gd, const std::string& assets, PlatformRequests* req);
    void fieldWidgets(GenericEditor& g, EdSection& s, std::pair<std::string, std::string>& kv, int x, int y, int w);
    void open();                                   // (re)load everything from disk
    void update(InputState& in, float dt, Canvas& cv);

private:
    EdUI ui; Tileset* ts = nullptr; Font* font = nullptr; GameData* gd = nullptr; std::string assetDir;
    std::string toast; float toastT = 0;
    void say(const std::string& s) { toast = s; toastT = 3; }
    // ---- popups ----
    enum PopKind { P_NONE, P_TILE, P_LIST, P_CONFIRM, P_TEXT, P_HELP, P_REPORT } pop = P_NONE;
    std::function<void(Tile)> popTile; std::function<void(int)> popList; std::function<void()> popYes; std::function<void(const std::string&)> popText;
    std::vector<std::string> popItems; std::string popTitle, popValue; int popSel = 0, popScroll = 0; int tileScrollX = 0, tileScrollY = 0; Tile popCur;
    int frameNo = 0, popOpenedFrame = -1;   // a popup ignores the click that opened it
    void openTilePicker(Tile current, std::function<void(Tile)> cb);
    void openListPicker(const std::string& title, const std::vector<std::string>& items, int cur, std::function<void(int)> cb);
    void openConfirm(const std::string& title, std::function<void()> yes);
    void openTextPrompt(const std::string& title, const std::string& initial, std::function<void(const std::string&)> cb);
    void drawPopup(InputState& in);
    // ---- guide (editor_help.cpp): assets/editor_guide.txt, opened by "?" buttons and F1 ----
    struct GuideTopic { std::string id, title; std::vector<std::string> see, body; };
    std::vector<GuideTopic> guide; std::map<std::string, int> guideIdx;
    int guideCur = 0, guideScroll = 0, guideTocScroll = 0; std::vector<int> guideHist; std::string guideFilter;
    void loadGuide();
    std::string helpTopicFor(const std::string& topic) const;   // the topic, or its nearest parent that exists ("" = none)
    std::string tabTopic() const;
    bool helpBtn(int x, int y, const std::string& topic, const std::string& fallback = "");
    void drawGuide(InputState& in, bool& close);
    // ---- where things are used (editor_flags.cpp) ----
    struct FlagUse { bool set = false; std::string where, detail; int tab = -1; std::string file; int line = -1, obj = -1, gen = -1, sec = -1; };
    struct MapScan { std::string id, name, table; int encounter = 0; std::vector<std::pair<std::string, std::string>> header; std::vector<EdObj> objs; std::set<std::string> kinds; };
    std::vector<MapScan> mapScan;                                  // every map's header, objects and tile kinds (for the indexes)
    std::map<std::string, std::vector<FlagUse>> flagIndex; bool flagIndexDirty = true, indexBuilt = false;
    std::map<std::string, std::string> flagDocs; bool flagDocsDirty = false;   // flags.txt notes
    std::string flagSelName, flagFilter; bool flagStoryOnly = true; int flagScroll = 0, flagUseScroll = 0, lastTab = -1;
    std::map<std::string, int> questPreview;
    std::map<std::string, std::vector<FlagUse>> useIndex;       // "item:id" / "creature:id" / "shop:id" / "dialogue:id" / "spell:id" -> where it is used
    int useScroll = 0;
    void drawUsedBy(const std::string& key, int x, int y, int w, int h, InputState& in);
    void renameFlag(const std::string& from, const std::string& to);
    struct Finding { bool err = false; std::string text; FlagUse jump; };
public:
    std::vector<Finding> report; int reportScroll = 0; bool reportErrOnly = false;
    void runCampaignCheck(); void drawReport(InputState& in, bool& close);
private:
    void genUndo(GenericEditor& g, InputState& in);
    void genStep(GenericEditor& g, bool undo);   // one undo (true) or redo (false) step
    void buildUseIndex();
    void scanMaps(); void buildFlagIndex(); void ensureIndex();
    void loadFlagDocs(); bool saveFlagDocs();
    void jumpToUse(const FlagUse& u);
    void flagPicker(int x, int y, int w, bool appendCond, std::function<void(const std::string&)> onPick, const std::string& label = "");
    std::string flagSetSummary(const std::string& f);
    std::string condEnglish(const std::string& cond) const;
    void drawFlags(InputState& in);
    int drawQuestStages(GenericEditor& g, EdSection& s, int x, int y, int w, int clipTop, int clipBottom, InputState& in);
    struct EncUsage { std::string base; bool night = false, hasNight = false, hasDay = false; std::vector<std::pair<std::string, int>> kinds, tableMaps, biomeMaps; std::vector<std::string> dungeons; };
    EncUsage encounterUsage(const std::string& id);
    bool encounterUnused(const std::string& id);
    void drawEncounterUsage(int x, int y, int w, int h, InputState& in);
    void drawBoardInfo(int x, int y, int w, int h, InputState& in);
    void drawFactionInfo(int x, int y, int w, int h, InputState& in); bool knownFaction(const std::string& id);
    std::vector<std::string> boardIds();          // [board] ids, or the board= values in the maps when there are none
    void boardProblems(const EdSection& s, std::vector<std::pair<bool, std::string>>& out);
    // ---- generic data editors ----
    std::vector<GenericEditor> gens;               // items, creatures, spells, classes, shops, encounters, quests, dungeons, overworld
    void drawGeneric(GenericEditor& g, int x, int y, int w, int h, InputState& in);
    // ---- tile kinds ----
    int kindSel = 0, kindScroll = 0; std::string kindFilter; bool kindsDirty = false;
    void drawKinds(InputState& in);
    // ---- maps ----
    std::vector<std::string> mapFiles; int mapListSel = -1, mapListScroll = 0;
    EdMap em; bool mapLoaded = false, mapDirty = false;
    int mapTool = 0;                                // 0 paint, 1 fill, 2 rect, 3 objects, 4 erase object
    int zoom = 1; int viewX = 0, viewY = 0;         // scroll in tiles * px
    std::string brush = "grass"; int paletteSel = 0, paletteScroll = 0; std::string paletteFilter;
    int objSel = -1, objType = 0, objScroll = 0; bool dragObj = false;
    int rectX0 = -1, rectY0 = -1; bool rectDragging = false;
    int resizeW = 0, resizeH = 0; bool showGrid = true, showObjs = true;
    int mapScrollDragX = 0, mapScrollDragY = 0; bool mapPanning = false;
    int selX0 = -1, selY0 = -1, selX1 = -1, selY1 = -1; bool selDragging = false; // select tool
    MapClip mapClip; bool pasting = false;
    std::vector<MapSnapshot> mapUndo;
    void mapPushUndo();
    void mapUndoPop();
    void drawMaps(InputState& in);
    void loadMapFile(const std::string& f);
    void paintCell(int x, int y, const std::string& spec);
    void floodFill(int x, int y, const std::string& spec);
    void drawMapCell(int sx, int sy, const std::string& spec, int scale);
    std::vector<std::string> paletteList() const;
    // ---- world ----
    Map* worldPreview = nullptr; int worldSeed = 1337; bool worldNeedsGen = true; int worldSel = -1; size_t worldSig = 0;
    void drawWorld(InputState& in);
    // ---- campaign tab (editor_campaign.cpp) ----
    CampaignForm camp;
    void loadCampaignForm(); bool saveCampaignForm(); void drawCampaign(InputState& in);
    bool campaignStart(const std::string& mapId, int& x, int& y); void setCampaignStart(const std::string& mapId, int x, int y);
    // ---- dialogue ----
    std::vector<std::string> dlgFiles; int dlgSel = -1, dlgScroll = 0; TextEdit te;
    void drawDialogue(InputState& in);
    void textEditWidget(TextEdit& t, int x, int y, int w, int h, InputState& in);
    // ---- dialogue node graph (editor_graph.cpp): a view of te.lines that edits through the text ----
    struct DlgGOpt { std::string label, target, cond; int line = -1; bool hasPipe = true; };
    struct DlgGNode { std::string id, speaker, next; std::vector<std::string> text, acts; std::vector<DlgGOpt> opts; std::string jumpFile;
        int line = -1, nextLine = -1, x = 0, y = 0, w = 150, h = 40, depth = -1; bool reached = false, dup = false; };
    bool dlgGraphMode = false;                   // Dialogue tab: middle panel shows the graph instead of the text editor
    std::vector<DlgGNode> dlgG; std::map<std::string, int> dlgGIdx; size_t dlgGHash = 0; std::string dlgGPath;
    std::map<std::string, std::map<std::string, std::pair<int, int>>> dlgGPos;   // per file path: node id -> dragged position (session only)
    int dlgGPanX = 0, dlgGPanY = 0, dlgGSel = -1, dlgGDrag = -1, dlgGDragOX = 0, dlgGDragOY = 0, dlgGLastClick = -1;
    int dlgGLinkNode = -1, dlgGLinkOpt = -1;      // shift-drag from an option row (-2 = the next= row) to retarget it
    bool dlgGPanning = false, dlgGDragMoved = false, dlgGFocusText = false; float dlgGLastClickT = -10; std::string dlgGSelId;
    void drawDialogueGraph(int x, int y, int w, int h, InputState& in);
    void dlgGraphParse(); void dlgGraphLayout(bool keepDragged);
    void dlgGraphJump(int node);                  // switch to Text mode with the cursor on the node's header line
    void dlgGraphAddNode();
    void dlgGraphRetarget(int node, int opt, const std::string& target);
    // ---- end dialogue node graph ----
    // ---- tileset ----
    int tsScroll = 0; Tile tsHover, tsPick;
    void drawTileset(InputState& in);
    // ---- cross-tab tools (editor_tools.cpp) ----
    int kindReturnTab = -1;                   // Tiles tab was entered from the map view: offer "back"
    void jumpToKind(const std::string& name);
    void requestPlay(int x, int y);           // save everything dirty and ask the app to playtest here
    bool saveAllDirty();
    struct LintItem { int line; std::string msg; bool warn; };
    std::vector<LintItem> lintDialogue(const std::string& text, const std::string& id);
    std::vector<LintItem> lint; bool lintShown = false; int lintScroll = 0; std::string lintTitle;
    std::set<std::string> knownFlags();
    // ---- sounds ----
    int sndSel = 0, sndScroll = 0; std::string sndFilter; bool sndDirty = false;
    void drawSounds(InputState& in);
    // helpers
    void refreshFiles();
    std::string kindsPath() const { return dataRoot + "/tilekinds.txt"; }
public:
    void openGuide(const std::string& topic);   // show the guide on a topic (also used by the headless tests)
    // campaigns: the editor works on one data root at a time (the base game or a folder in assets/campaigns).
    // Files the campaign doesn't have yet are read from the base game and saved into the campaign (copy on write).
    std::string dataRoot, baseRoot, campaignId = "triffall"; bool campaignInherit = true;
    std::function<bool(const std::string&)> onSwitchCampaign;   // set by the app: loads the campaign's data into the game
    std::string readPath(const std::string& rel) const { std::string p = dataRoot + "/" + rel; if (dataRoot == baseRoot || !campaignInherit || fileExists(p)) return p; return baseRoot + "/" + rel; }
    void setCampaign(const std::string& id);
    void newCampaign(const std::string& name);
    void bakeOverworld();
    bool bakeDungeon(const EdSection& s, std::string& err);
    const std::vector<EdSection>& dungeonSections() { return gens[7].secs; }
private:
};
