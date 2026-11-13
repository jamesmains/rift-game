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
    std::string lead;   // 1.1.3: the comment lines just above this [section] in the file, written back on save
    bool inherited = false; std::vector<std::pair<std::string, std::string>> origKv;   // shown from the base game; edited -> becomes the campaign's own copy
    std::string get(const std::string& k, const std::string& def = "") const { for (auto& p : kv) if (p.first == k) return p.second; return def; }
    void set(const std::string& k, const std::string& v) { for (auto& p : kv) if (p.first == k) { p.second = v; return; } kv.push_back({k, v}); }
    bool has(const std::string& k) const { for (auto& p : kv) if (p.first == k) return true; return false; }
};
struct CampaignForm {
    std::string path, headerComments, name, desc, author, version, icon, startMap, items, flags, dialogue, worldMap, defeat;
    int startX = 0, startY = 0, gold = 25; std::vector<std::string> intro; std::vector<std::pair<std::string, std::string>> extra; bool dirty = false;
};
struct PlayState { bool on = false, dirty = false; int level = 1, gold = 500; std::string cls, flags, items, party; };   // Campaign tab > Play from a state (playtest.txt)
std::set<std::string> edFilterTokens(const std::string& v);   // 1.1.6: the values a list filter offers for one key=value
struct GenericEditor {
    std::string path, title, sectionName, listKey = "id";
    std::string basePath; bool replaceWhole = false;   // campaigns: base-game file shown underneath; replaceWhole = the game reads one file or the other, not both
    std::vector<std::string> templateKeys;     // keys offered for a new section
    std::string altSection; std::vector<std::string> altTemplateKeys;   // a second kind of entry in the same file (bounties.txt: [board] and [bounty])
    std::vector<EdSection> secs;
    std::string tail;                          // comment lines after the last section (written back on save, 1.1.3)
    int sel = -1, scroll = 0, fieldScroll = 0; bool dirty = false; std::string filter, newKey, status;
    std::string filterKey, filterVal;          // 1.1.6 (#30): list filter "key = value" (filterVal "\x01has" = has the key, "\x01none" = lacks it)
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
    bool has(const std::string& k) const { for (auto& p : kv) if (p.first == k) return true; return false; }
    void erase(const std::string& k) { for (size_t i = 0; i < kv.size(); i++) if (kv[i].first == k) { kv.erase(kv.begin() + i); i--; } }
};
// the 12 kinds of map object and the keys each one reads (editor.cpp)
const std::vector<std::string>& edObjTypes();
std::vector<std::string> edObjKeys(const std::string& type);
std::vector<std::string> edObjLookKeys(const std::string& type);   // 1.1.1: the look / use keys a [type] entry starts with
const std::vector<std::string>& edNpcKeys();                       // 1.1.9: every key an NPC (npcs.txt) can have
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

// Multi-line text editor state (dialogue and script files, the big box). 1.1.11 (#35): Notepad-style editing, see
// editor_text.cpp (Editor::textEditWidget) for the keys.
struct TextEdit {
    std::vector<std::string> lines{""};
    int cx = 0, cy = 0, scroll = 0; bool dirty = false; std::string path;   // scroll = first line shown
    int ax = -1, ay = -1;                       // selection anchor (-1 = none)
    struct Snap { std::vector<std::string> lines; int cx = 0, cy = 0; };
    std::vector<Snap> undo, redo; int lastKind = -1;   // a run of typing / deleting is one undo step (lastKind)
    int scrollSub = 0, hscroll = 0, goalX = -1;        // wrapped: row inside line 'scroll'; px scroll without wrap; the x Up/Down keep
    bool wrap = false, wide = false, single = false, overwrite = false;   // single = the big box (one line, Enter is OK)
    bool refocus = false;                       // Wide changed the layout (widget ids move): take the focus back next frame
    bool dragging = false, dragWord = false, dragLine = false; int dwx0 = 0, dwx1 = 0, dwy = 0;
    int bar = 0;                                // 1 find, 2 find + replace, 3 go to line
    std::string findText, replText, gotoText, findLast; bool findCase = false, findWord = false, barFocus = false;
    std::map<int, std::pair<bool, std::string>> marks;   // line -> (error, message): the live check's gutter dots
    size_t checkedHash = 0; float editT = -10;           // live check: the text last checked, when it last changed
    std::vector<std::string> ac; int acSel = 0, acStart = 0; bool acOpen = false;   // script word suggestions
    bool hasSel() const { return ay >= 0 && !(ax == cx && ay == cy); }
    void selRange(int& x0, int& y0, int& x1, int& y1) const { if (ay < cy || (ay == cy && ax <= cx)) { x0 = ax; y0 = ay; x1 = cx; y1 = cy; } else { x0 = cx; y0 = cy; x1 = ax; y1 = ay; } }
    std::string selText() const;
    void deleteSel();
    void insert(const std::string& t);
    void snapshot(int kind = 0) { if (kind > 0 && kind == lastKind && !undo.empty()) return; undo.push_back({lines, cx, cy}); if (undo.size() > 400) undo.erase(undo.begin()); redo.clear(); lastKind = kind; }
    bool undoStep() { if (undo.empty()) return false; redo.push_back({lines, cx, cy}); lines = undo.back().lines; cx = undo.back().cx; cy = undo.back().cy; undo.pop_back(); fixCaret(); ay = -1; dirty = true; lastKind = -1; return true; }
    bool redoStep() { if (redo.empty()) return false; undo.push_back({lines, cx, cy}); lines = redo.back().lines; cx = redo.back().cx; cy = redo.back().cy; redo.pop_back(); fixCaret(); ay = -1; dirty = true; lastKind = -1; return true; }
    void clearKeepView() { bool wr = wrap, wd = wide; *this = TextEdit(); wrap = wr; wide = wd; }   // no file open, same wrap / wide
    void fixCaret() { if (lines.empty()) lines.push_back(""); cy = cy < 0 ? 0 : cy >= (int)lines.size() ? (int)lines.size() - 1 : cy; if (cx < 0) cx = 0; if (cx > (int)lines[cy].size()) cx = (int)lines[cy].size(); }
    void load(const std::string& p);
    bool save();
    std::string text() const;
};
struct MapSnapshot { int w, h; std::vector<std::string> cells; std::vector<EdObj> objs; };
struct MapClip { int w = 0, h = 0; std::vector<std::string> cells; std::vector<EdObj> objs; };

class Editor {
public:
    // 1.2.2 (#39): the Editor's canvas follows the screen's shape too (App::setScreen); 960x720 = Classic 4:3
    static constexpr int BASE_W = 960, BASE_H = 720;
    static inline int W = BASE_W, H = BASE_H;
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
    enum PopKind { P_NONE, P_TILE, P_LIST, P_CONFIRM, P_TEXT, P_HELP, P_REPORT, P_IMPORT, P_BIGTEXT, P_SHEET } pop = P_NONE;
    std::function<void()> popCancel;   // 1.2.0: run when a list / text popup is cancelled (the tile picker reopens itself)
    std::function<void(Tile)> popTile; std::function<void(int)> popList; std::function<void()> popYes; std::function<void(const std::string&)> popText;
    std::vector<std::string> popItems; std::string popTitle, popValue; int popSel = 0, popScroll = 0; int tileScrollX = 0, tileScrollY = 0; Tile popCur;
    int frameNo = 0, popOpenedFrame = -1;   // a popup ignores the click that opened it
    void openTilePicker(Tile current, std::function<void(Tile)> cb, char cat = 0);   // cat: the category shown first (1.2.0 #37)
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
    struct MapScan { std::string id, name, table; int encounter = 0; std::vector<std::pair<std::string, std::string>> header; std::vector<EdObj> objs, raw; std::set<std::string> kinds; };   // objs = with template / resource keys filled in, raw = as written
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
    std::vector<std::string> mapFiles; int mapListSel = -1, mapListScroll = 0; std::string mapFilter;   // 1.1.6: map list search
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
    void drawMapCell(int sx, int sy, const std::string& spec, int scale, int mx = -1, int my = -1);
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
    std::string dlgFilter, dlgFilterLast; size_t dlgFilterN = 0; std::set<std::string> dlgFilterHits;   // 1.1.6: dialogue search (names + text)
    void drawDialogue(InputState& in);
    void textEditWidget(TextEdit& t, int x, int y, int w, int h, InputState& in, int syntax = 0);   // syntax 1 = script colours (editor_text.cpp)
    void liveCheck(TextEdit& t, int syntax);   // 1.1.11: runs the tab's Check a moment after the text changes, for the gutter dots
    std::vector<std::string> scriptWords(const TextEdit& t, const std::string& prefix);   // autocomplete candidates
    TextEdit bigTE; int bigTarget = -1, bigMax = 200; bool bigRefocus = false;   // 1.1.11: "Edit in a big box" for a single-line field
    void openBigText(int fieldId, const std::string& value, int maxLen);
    void drawBigText(InputState& in, bool& close);
    // ---- 1.1.10 (#34) scripts (editor_scripts.cpp): data/scripts/<id>.txt ----
    std::vector<std::string> scriptFiles; int scriptSel = -1, scriptScroll = 0; TextEdit ste; std::string scriptFilter;
    void drawScripts(InputState& in);
    void openScript(const std::string& id, int line = -1);
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
    // ---- object templates and resource nodes (editor_objects.cpp): gens[15] = objects.txt, gens[16] = resources.txt ----
    const EdSection* templateSec(const std::string& id) const;
    const EdSection* typeSec(const std::string& type) const;   // 1.1.1: objects.txt [type] entry for an object type
    void drawObjectLook(const EdObj& resolved, int sx, int sy, int scale, bool used);   // a chest / sign / savepoint / lever / door as the game draws it
    void drawObjectTypeInfo(int x, int y, int w, int h, InputState& in);   // Objects tab: preview of the selected [type] / template
    const EdSection* resourceSec(const std::string& node, const std::string& item) const;   // node= id, else the first one giving the item
    EdObj resolvedObj(const EdObj& o) const;   // the object as the game sees it: template type + keys, resource keys
    std::vector<std::pair<std::string, std::string>> inheritedKeys(const EdObj& o, std::string& from) const;
    mutable std::vector<EdSection> baseRes; mutable bool baseResLoaded = false;   // 1.1.7: the base game's resource types, for campaigns with inherit=1   // keys it gets from its template / resource
    int poType = 0, poScroll = 0, poSel = -1; std::string poFilter, poKey, poVal; bool poThisTmpl = false, poNeedScan = true;
    struct PlacedRef { std::string map; int obj = -1; };
    std::vector<PlacedRef> placedMatches();    // the placed objects the Objects tab's filter picks (from mapScan)
    bool placedMatch(const std::string& mapId, const EdObj& raw) const;
    int bulkObjects(const std::function<bool(EdObj&)>& fn, int& mapsChanged);   // runs fn on every matching placed object, saves the maps it changed
    void drawObjectsTab(InputState& in); void drawPlacedObjects(int x, int y, int w, int h, InputState& in);
    void drawResourcesTab(InputState& in); void drawResourceInfo(int x, int y, int w, int h, InputState& in);
    // 1.1.8 (#32): Items tab Look card (editor_items.cpp): preview, sprite, color= tint over the white shape, palette
    int drawItemLook(GenericEditor& g, EdSection& s, int x, int y, int w);
    Color8 tileOwnColor(Tile t); const std::vector<std::pair<Color8, int>>& sheetColors();
    std::vector<std::pair<Color8, int>> sheetPal; std::map<int, Color8> ownColCache;
    void drawGatherNode(const EdObj& resolved, int sx, int sy, int scale, const std::string& cellSpec, bool spent = false);
    void templateFromObject(const EdObj& o, const std::string& mapId, int objIndex);
    std::string placeTmpl; int objPanelScroll = 0;   // Maps tab: the template the Objects tool places; the object panel's scroll
    // ---- NPCs (editor_npcs.cpp, 1.1.9 #33): gens[17] = npcs.txt; a map [npc] object is a placement with npc=<id> ----
public:
    const EdSection* npcSec(const std::string& id) const;   // the NPC entry (the base game's too, for a campaign with inherit=1)
    std::string joinClassOf(const std::string& v) const;     // the class a join= value adds (an NPC id, or class:Name:col,row)
    std::string newNpc(const EdObj* from);                   // a new NPC (from a placed object's keys), saved; returns its id
    std::string npcPlacementId(const EdMap& m, const std::string& npcId) const;
    void renameNpc(const std::string& from, const std::string& to);
    void startPlacingNpc(const std::string& id);              // Maps tab, Objects tool, placing this NPC
    std::string placeNpc;                                     // the NPC the Objects tool places (npc type)
private:
    mutable std::vector<EdSection> baseNpcs; mutable bool baseNpcsLoaded = false;
    std::map<std::string, std::vector<std::string>> npcJoins; bool npcJoinsDirty = true;   // join= value -> dialogue files
    int npcInfoScroll = 0;
    std::string newNpcId(const std::string& name) const;
    void scanNpcJoins();
    void npcProblems(const EdSection& s, std::vector<std::pair<bool, std::string>>& out);
    void drawNpcInfo(int x, int y, int w, int h, InputState& in);
    int drawLookCard(GenericEditor& g, EdSection& s, int x, int y, int w, bool npc);
    // ---- import from another campaign (editor_import.cpp, 1.1.6 #29): every tab can copy entries in ----
public:
    enum ImpKind { IMP_GEN, IMP_MAPS, IMP_DLG, IMP_KINDS, IMP_CAMP, IMP_FLAGS, IMP_SCRIPTS };
    struct ImpEntry { std::string id, label, detail; bool have = false, on = false; EdSection sec; };
    int impKind = IMP_GEN, impGen = -1, impSrc = -1, impSrcScroll = 0, impScroll = 0; std::string impFilter, impTitle;
    std::vector<CampaignInfo> impSrcs; std::vector<ImpEntry> impEntries;
    void openImport(int kind, int gen = -1);   // also used by the headless tests
    void impSelectSource(int i);
    int doImport();                            // copies the ticked entries in; returns how many
    // headless tests (1.1.6): a data tab's entry count, how many its search / filter show, set them, save it
    int genCount(int g) const { return (int)gens[g].secs.size(); }
    int genShown(int g) const { return (int)gens[g].filtered().size(); }
    void genSearch(int g, const std::string& q, const std::string& key = "", const std::string& val = "") { gens[g].filter = q; gens[g].filterKey = key; gens[g].filterVal = val; }
    bool genSave(int g) { return gens[g].save(); }
    void closePopup() { pop = P_NONE; ui.modal = false; ui.blur(); }
    // 1.1.11 headless tests: the text editors ("te" dialogue, "ste" scripts, "big" the big box); what 0 text, 1 selection, 2 caret, 3 state
    std::string edText(const std::string& which, int what) {
        TextEdit& t = which == "ste" ? ste : which == "big" ? bigTE : te;
        if (what == 1) return t.selText();
        if (what == 2) return std::to_string(t.cy + 1) + "," + std::to_string(t.cx + 1);
        if (what == 3) return "wrap=" + std::to_string(t.wrap) + " wide=" + std::to_string(t.wide) + " bar=" + std::to_string(t.bar) + " ac=" + std::to_string(t.acOpen ? (int)t.ac.size() : 0) + " pop=" + std::to_string((int)pop) + " marks=" + std::to_string(t.marks.size()) + " menu=" + std::to_string(ui.menuOpen) + " focus=" + std::to_string(ui.focus) + " ids=" + std::to_string(ui.idCounter);
        std::string o; for (size_t i = 0; i < t.lines.size(); i++) o += (i ? "|" : "") + t.lines[i]; return o;
    }
    std::string genValue(int g, int sec, const std::string& key) const { if (g < 0 || g >= (int)gens.size() || sec < 0 || sec >= (int)gens[g].secs.size()) return ""; return gens[g].secs[sec].get(key); }
private:
    bool importBtn(int x, int y, int kind, int gen = -1);   // the small "Import" button in a list header
    void listHeader(int x, int y, int w, const std::string& title, int kind, int gen = -1);   // header + Import button (title clipped before it)
    std::string impRoot(const CampaignInfo& c) const { return c.dir.empty() ? baseRoot : c.dir; }
    std::string impReadPath(const CampaignInfo& c, const std::string& rel) const;   // the campaign's file, else the base game's when it inherits
    std::vector<std::string> impFiles(const CampaignInfo& c, const std::string& sub) const;   // maps / dialogue ids it has
    void drawImport(InputState& in, bool& close);
    void openFieldFilter(GenericEditor& g);    // 1.1.6 (#30): pick a field, then a value, to filter the list by
    // ---- tileset ----
    int tsScroll = 0; Tile tsHover, tsPick;
    void drawTileset(InputState& in);
    // ---- sprite sheets (editor_sheets.cpp, 1.2.0): #36 import / copy sheets, #37 tile categories and groups ----
public:
    struct SheetGroup { std::string name; std::vector<std::pair<int, int>> cells; };
    struct SheetBook { std::string name; int cols = 0, rows = 0; std::string cats; std::vector<SheetGroup> groups; SheetGrid grid; bool loaded = false; };   // cats: a letter per cell (row * cols + col)
    std::vector<SheetBook> books;                  // index = Tile::s; sheets/<name>.txt of each sheet
    void syncBooks(bool force = false);            // (re)reads them when the sheets changed
    char cellCat(Tile t);                          // the tile's category letter ('.' = not sorted)
    bool saveBook(int s);
    void reloadSheets();                           // after an import / copy / grid change: the Tileset and the books
    int sheetImport(const std::string& png, const std::string& name, const SheetGrid& g, std::string& err);   // returns the sheet (-1 = failed)
    bool copyBaseSheets(std::string& err);
    bool removeSheet(int s, std::string& err);
    std::string sheetsDir() const { return dataRoot + "/sheets"; }
    int pickerCount(int s, char cat, int group) { int n = 0; return (int)pickerTiles(s, cat, group, n).size(); }   // headless tests
    int pkSheet = 0, pkGroup = -1; char pkCat = 0;   // the picker's sheet, group (-1 = none) and category (0 = all)
    std::vector<Tile> pickerTiles(int s, char cat, int group, int& sortedCount);
    std::vector<int> visibleSheets() const;
private:
    std::string bookSig;
    int pkScroll = 0, pkGroupScroll = 0; std::string pkFind; bool pkLayout = false; std::vector<Tile> pkMenuTiles; Tile pkHover;
    std::function<void(Tile)> pkCb; Tile pkCur;
    void reopenTilePicker() { popOpenedFrame = frameNo; pop = P_TILE; ui.modal = true; ui.blur(); }
    void drawTilePicker(InputState& in, bool& close);
    std::vector<int> shownGroups(int s, char cat, const std::string& find);   // indexes into books[s].groups
    void openTileMenu(const std::vector<Tile>& tiles, int menuId);
    void applyTileMenu(int cmd, bool fromPicker);
    void setCats(const std::vector<Tile>& tiles, char k);
    std::string tileUses(Tile t);
    // Tileset tab
    int tsSheet = 0, tsSheetScroll = 0, tsGroupSel = -1, tsGroupScroll = 0; char tsCatFilter = 0; std::string tsGroupFind;
    std::set<int> tsSel; int tsAnchor = -1, tsDragC = -1, tsDragR = -1; bool tsDragging = false, tsAdding = false; int tsRDownX = 0, tsRDownY = 0;
    std::vector<Tile> tsSelTiles();
    // import window (P_SHEET)
    std::string siPath, siName, siTitle, siErr; SheetGrid siGrid; Image8 siImg; int siEdit = -1, siZoom = 2, siScrollX = 0, siScrollY = 0; bool siPickKey = false, siGame = false;
    Image8 siConv; int siConvCols = 0, siConvRows = 0; std::string siConvSig;
public:
    void openSheetImport(const std::string& path, int editSheet = -1);
private:
    void siLoad(const std::string& path);
    void drawSheetImport(InputState& in, bool& close);
    // ---- cross-tab tools (editor_tools.cpp) ----
    int kindReturnTab = -1;                   // Tiles tab was entered from the map view: offer "back"
    void jumpToKind(const std::string& name);
    void requestPlay(int x, int y);           // save everything dirty and ask the app to playtest here
    bool saveAllDirty();
    struct LintItem { int line; std::string msg; bool warn; };
    std::vector<LintItem> lintDialogue(const std::string& text, const std::string& id);
    std::vector<LintItem> lint; bool lintShown = false; int lintScroll = 0; std::string lintTitle;
    std::vector<LintItem> lintScript(const std::string& text, const std::string& id);
    std::vector<LintItem> sLint; bool sLintShown = false; int sLintScroll = 0; std::string sLintTitle; int scriptRefTab = 0;
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
