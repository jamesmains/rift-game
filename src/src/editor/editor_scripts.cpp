// 1.1.10 (owner's #34): the Scripts tab - data/scripts/<id>.txt in a text editor with script colours, Check (the game's
// own compiler, plus links to scripts / dialogue / items / creatures / shops / maps that don't exist), a cheat sheet
// and Used by. The language is described in game/script.h and the guide's scripting.* topics.
#include "editor.h"
#include <filesystem>
#include <algorithm>

namespace fs = std::filesystem;
using namespace ED;
extern int gEdTop;
#define TOP gEdTop
static const int BOTTOM = 18;

static const char* NEW_SCRIPT =
    "# What this script does, in a line or two.\n"
    "# Run it from an NPC (script=<id> in the NPCs tab), a dialogue node (script=<id>),\n"
    "# any object's onuse= / a trigger's script= (script:<id>), a map's onenter=, or an\n"
    "# item (effect=script:<id>).\n"
    "\n"
    "say Hello, {herename}.\n"
    "option \"Who are you?\" -> \"who\"\n"
    "option \"Goodbye.\" -> \"bye\"\n"
    "ask answer What can I do for you?\n"
    "if answer == \"who\"\n"
    "  say Nobody important.\n"
    "end\n";

void Editor::openScript(const std::string& id, int line) {
    auto go = [this, id, line]() {
        std::string p = dataRoot + "/scripts/" + id + ".txt";
        if (ste.path != p) { ste.load(readPath("scripts/" + id + ".txt")); ste.path = p; }
        for (size_t i = 0; i < scriptFiles.size(); i++) if (scriptFiles[i] == id) scriptSel = (int)i;
        if (line >= 0) { ste.cy = clampi(line, 0, (int)ste.lines.size() - 1); ste.cx = 0; ste.ax = ste.ay = -1; ste.scroll = std::max(0, ste.cy - 6); }
        tab = 25; sLintShown = false;
    };
    if (ste.dirty && ste.path != dataRoot + "/scripts/" + id + ".txt") openConfirm("Discard unsaved changes to the open script?", go); else go();
}

std::vector<Editor::LintItem> Editor::lintScript(const std::string& text, const std::string& id) {
    std::vector<LintItem> out;
    ScriptProg p = compileScript(id, text);
    for (auto& e : p.errors) out.push_back({e.line, e.msg, false});
    auto known = [&](const std::string& sid) { if (std::find(scriptFiles.begin(), scriptFiles.end(), sid) != scriptFiles.end()) return true; return fileExists(readPath("scripts/" + sid + ".txt")); };
    std::vector<std::string> maps{"overworld"}; for (auto& m : mapFiles) maps.push_back(m); for (auto& d : gens[7].secs) maps.push_back(d.get("id"));
    for (auto& r : p.refs) {
        if (r.value.find('{') != std::string::npos) continue;   // worked out while it runs: can't check it here
        std::string v = trim(r.value);
        if (r.key == "call") { if (!known(v)) out.push_back({r.line, "there is no script '" + v + "'", false}); continue; }
        if (r.key == "dialogue" && std::find(dlgFiles.begin(), dlgFiles.end(), v) == dlgFiles.end() && !fileExists(readPath("dialogue/" + v + ".txt"))) out.push_back({r.line, "there is no dialogue '" + v + "'", false});
        if (r.key == "give" || r.key == "take" || r.key == "item") for (auto& it : split(v, ',')) { std::string iid = trim(split(trim(it), ':')[0]); if (!iid.empty() && !gd->item(iid)) out.push_back({r.line, "unknown item '" + iid + "'", false}); }
        if (r.key == "combat") for (auto& c : split(v, ',')) if (!trim(c).empty() && !gd->creature(trim(c))) out.push_back({r.line, "unknown creature '" + trim(c) + "'", false});
        if (r.key == "spawn") { std::string c = trim(split(v, ',')[0]); if (!c.empty() && !gd->creature(c)) out.push_back({r.line, "unknown creature '" + c + "'", false}); }
        if (r.key == "shop") { bool ok = false; for (auto& s : gens[4].secs) if (s.get("id") == v) ok = true; if (!ok) out.push_back({r.line, "unknown shop '" + v + "'", false}); }
        if (r.key == "teleport") { std::string m = trim(split(v, ',')[0]); if (split(v, ',').size() < 3) out.push_back({r.line, "teleport needs map,x,y", false}); else if (std::find(maps.begin(), maps.end(), m) == maps.end()) out.push_back({r.line, "unknown map '" + m + "'", false}); }
        if (r.key == "sound" && !v.empty() && !gAudio.defs.count(v)) out.push_back({r.line, "unknown sound '" + v + "' (Sounds tab)", true});
    }
    // a say / ask / pick in a script that may run without the party standing still is fine: the game waits for the map
    if (p.code.empty() && p.errors.empty()) out.push_back({-1, "the script is empty", true});
    std::stable_sort(out.begin(), out.end(), [](const LintItem& a, const LintItem& b) { return a.warn < b.warn; });
    return out;
}

void Editor::drawScripts(InputState& in) {
    int x = 4, y = TOP + 4, h = H - TOP - BOTTOM - 8, lw = 200;
    ui.panel(x, y, lw, h); listHeader(x, y, lw, fmt("Scripts (%d)", (int)scriptFiles.size()), IMP_SCRIPTS); helpBtn(x + lw - 16, y + 1, "tab.scripts");
    ui.searchField(x + 4, y + 20, lw - 8, scriptFilter, "search names and text...");
    std::vector<std::string> sl; std::vector<int> sIdx; std::string t;
    for (size_t i = 0; i < scriptFiles.size(); i++) {
        if (!scriptFilter.empty()) { std::string body; readFile(readPath("scripts/" + scriptFiles[i] + ".txt"), body); if (!edMatch(scriptFiles[i] + " " + body, scriptFilter)) continue; }
        sl.push_back(scriptFiles[i]); sIdx.push_back((int)i);
    }
    int cur = -1; for (size_t i = 0; i < sIdx.size(); i++) if (sIdx[i] == scriptSel) cur = (int)i;
    int hit = ui.listBox(x + 4, y + 38, lw - 8, h - 98, sl, cur, scriptScroll);
    if (scriptFiles.empty()) ui.text(x + 10, y + 42, "No scripts yet: New or Import.", DIM);
    if (hit >= 0) openScript(scriptFiles[sIdx[hit]]);
    int by = y + h - 54;
    if (ui.button(x + 4, by, 50, 20, "New")) openTextPrompt("New script id (letters, digits, _)", "my_script", [this](const std::string& nm0) {
        std::string nm; for (char c : toLower(nm0)) if (isalnum((unsigned char)c) || c == '_' || c == '-') nm += c; if (nm.empty()) return;
        std::error_code ec; fs::create_directories(dataRoot + "/scripts", ec);
        std::string p = dataRoot + "/scripts/" + nm + ".txt"; if (!fileExists(p)) writeFile(p, NEW_SCRIPT);
        refreshFiles(); ste = TextEdit(); openScript(nm); changed = true; });
    if (ui.button(x + 60, by, 50, 20, "Save", ste.dirty && !ste.path.empty(), false, OK)) { std::error_code ec; fs::create_directories(dataRoot + "/scripts", ec); if (ste.save()) { changed = true; flagIndexDirty = true; say("Saved " + ste.path); } }
    if (ui.button(x + 116, by, 60, 20, "Delete", !ste.path.empty(), false, DANGER)) openConfirm("Delete this script file?", [this]() { remove(ste.path.c_str()); ste = TextEdit(); refreshFiles(); scriptSel = -1; changed = true; });
    std::string curId = ste.path.empty() ? "" : fs::path(ste.path).stem().string();
    if (ui.button(x + 4, by + 24, 60, 20, "Check", !ste.path.empty(), false, ACCENT2)) { sLint = lintScript(ste.text(), curId); sLintShown = true; sLintScroll = 0; sLintTitle = curId; say(sLint.empty() ? "Check: no problems found." : fmt("Check: %d finding(s).", (int)sLint.size())); }
    if (ui.button(x + 68, by + 24, 76, 20, "Check all", true, false, ACCENT2)) {
        sLint.clear(); int files = 0;
        for (auto& f : scriptFiles) { std::string body; if (f == curId) body = ste.text(); else if (!readFile(readPath("scripts/" + f + ".txt"), body)) continue; files++; for (auto& it : lintScript(body, f)) { it.msg = f + (it.line >= 0 ? fmt(" line %d", it.line + 1) : std::string()) + ": " + it.msg; it.line = -1; sLint.push_back(it); } }
        std::stable_sort(sLint.begin(), sLint.end(), [](const LintItem& a, const LintItem& b) { return a.warn < b.warn; });
        sLintShown = true; sLintScroll = 0; sLintTitle = fmt("all %d scripts", files); say(fmt("Checked %d scripts: %d finding(s).", files, (int)sLint.size()));
    }
    if (sLintShown && ui.button(x + 148, by + 24, 48, 20, "Hide")) sLintShown = false;
    // editor
    int ex = x + lw + 6, ew = W - ex - 4 - 300;
    ui.panel(ex, y, ew, h); ui.header(ex, y, ew, ste.path.empty() ? "no script open" : fs::path(ste.path).filename().string() + (ste.dirty ? " *" : ""));
    if (!ste.path.empty()) textEditWidget(ste, ex + 2, y + 18, ew - 4, h - 20, in, 1);
    else font->drawWrapped(*ui.cv, *ts, ex + 10, y + 30, ew - 20, "Scripts are little programs for the game: talk with answers, prices worked out on the spot, counters that are saved, loops, random chances, and every dialogue action. Pick one on the left (coachman is the coach in every town) or press New.", DIM);
    // right: check results, or the cheat sheet + Used by
    int rx = ex + ew + 4, rw = W - rx - 4;
    ui.panel(rx, y, rw, h);
    if (sLintShown) {
        int errs = 0; for (auto& l : sLint) if (!l.warn) errs++;
        ui.header(rx, y, rw, fmt("Check: %s - %d error(s), %d warning(s)", sLintTitle.c_str(), errs, (int)sLint.size() - errs));
        int rowH = 26, rows = (h - 40) / rowH;
        if (ui.hover(rx, y + 18, rw, h - 18) && in.wheel) sLintScroll -= in.wheel * 2;
        sLintScroll = clampi(sLintScroll, 0, std::max(0, (int)sLint.size() - rows));
        if (sLint.empty()) ui.text(rx + 8, y + 24, "No problems found.", OK);
        for (int r = 0; r < rows && sLintScroll + r < (int)sLint.size(); r++) {
            auto& it = sLint[sLintScroll + r]; int ry = y + 20 + r * rowH;
            bool hv = ui.hover(rx + 2, ry, rw - 4, rowH) && it.line >= 0;
            if (hv) ui.cv->fillRect(rx + 2, ry, rw - 4, rowH, HOVER);
            font->draw(*ui.cv, *ts, rx + 6, ry + 1, (it.line >= 0 ? fmt("line %d: ", it.line + 1) : std::string()) + (it.warn ? "warning" : "ERROR"), it.warn ? ACCENT2 : DANGER);
            auto wl = font->wrap(it.msg, rw - 12); font->draw(*ui.cv, *ts, rx + 6, ry + 13, wl.empty() ? "" : wl[0], TEXT);
            if (ui.hover(rx + 2, ry, rw - 4, rowH) && wl.size() > 1) ui.tip(it.msg);
            if (hv && in.mousePressed[0] && !ui.mouseUsed) { ui.mouseUsed = true; ste.cy = clampi(it.line, 0, (int)ste.lines.size() - 1); ste.cx = 0; ste.ax = ste.ay = -1; ste.scroll = std::max(0, ste.cy - 6); }
        }
        ui.text(rx + 6, y + h - 16, "click a finding to jump to its line", DIM);
        return;
    }
    if (!curId.empty()) drawUsedBy("script:" + curId, rx + 2, y + h - 130, rw - 4, 126, in);
    ui.header(rx, y, rw, "");
    static const char* TABN[] = {"Commands", "Values", "Actions"};
    { int bx = rx + 4; for (int i = 0; i < 3; i++) { int bw = font->width(TABN[i]) + 10; if (ui.button(bx, y + 1, bw, 14, TABN[i], true, scriptRefTab == i)) { scriptRefTab = i; ui.blur(); } bx += bw + 2; } }
    helpBtn(rx + rw - 16, y + 1, scriptRefTab == 0 ? "scripting.commands" : scriptRefTab == 1 ? "scripting.values" : "scripting.actions", "tab.scripts");
    static const char* ref0[] = {
        "# a comment (to the end of the line)",
        "let n = 5          a variable (this run)",
        "keep visits = visits + 1",
        "  saved with the game; any script reads it",
        "n = n * 2   n += 1   n -= 1",
        "if gold >= 10 and not met_mara",
        "elif ...   else   end",
        "while n < 3 ... end",
        "for p in places() ... end",
        "for i in 5 ... end   (1 to 5)",
        "break   continue   stop",
        "call other_script a, b",
        "  (arg1, arg2 inside; return x -> result)",
        "start other   runs alongside",
        "",
        "say Hello, {herename}!",
        "speaker Old Tam   (\"\" = no name)",
        "option \"Yes\" -> 1",
        "option Not today",
        "ask answer Will you help?",
        "  (the picked option's value)",
        "pick dest Where to?",
        "  (a visited place on the world map,",
        "   \"\" if none was chosen)",
        "wait 1.5   (seconds on the map)",
        "",
        "{...} puts a value into any text.",
        "\"text\" + n joins, = and == compare,",
        "+ - * / % on whole numbers."};
    static const char* ref1[] = {
        "Names not set by let/keep are flags",
        "(0 when not set): if met_mara ...",
        "",
        "gold day hour minute here (map id)",
        "herename x y level partysize night",
        "stealth weather npc npcname",
        "won (1 = the last fight was won)",
        "useditem user (in item scripts)",
        "",
        "flag(\"name\")   cond(\"item:key&!x\")",
        "  cond() = any dialogue condition",
        "has(item)  visited(map)  kills(c)",
        "inparty(class)  rep(f)  approval(c)",
        "random(1, 6)   chance(25)",
        "places()  visited overworld places",
        "placename(p) placemap(p)",
        "placex(p) placey(p)",
        "distance(p) leagues(p)  from here",
        "mapname(id)  itemname(id)",
        "",
        "Lists are text with commas:",
        "count(l) at(l, 2) contains(l, x)",
        "add(l, x)",
        "len(t) upper(t) lower(t) find(t, s)",
        "min(a, b) max(a, b) abs(n) var(name)"};
    static const char* ref2[] = {
        "Every dialogue action, as a line:",
        "give potion:2    take key:1",
        "gold -{fare}     xp 50",
        "set met=1,x      clear x    inc n",
        "teleport map,x,y[,travel text]",
        "dialogue id      shop id",
        "combat goblin,goblin  (then: won)",
        "message Title|Text   log text",
        "notify text   note Title|Text",
        "cutscene line|line   quest Name",
        "join npc_id   leave class",
        "hidenpc id    shownpc id",
        "npcmove id,x,y   spawn c,x,y",
        "heal  rest  healer  respec",
        "craft station   board town",
        "travel (the coach map)",
        "rep faction:n   approve class:n",
        "revealmap map   win line|line",
        "",
        "New in 1.1.10 (dialogue too):",
        "script id[,args]  time minutes",
        "sound id   encounter (random fight)",
        "weather new|rain|clear...",
        "stat travels:1",
        "",
        "Screens (fight, shop, teleport...)",
        "pause the script until the party",
        "is back on the map."};
    const char* const* ref = scriptRefTab == 0 ? ref0 : scriptRefTab == 1 ? ref1 : ref2;
    int n = scriptRefTab == 0 ? (int)(sizeof(ref0) / sizeof(ref0[0])) : scriptRefTab == 1 ? (int)(sizeof(ref1) / sizeof(ref1[0])) : (int)(sizeof(ref2) / sizeof(ref2[0]));
    ui.cv->setClip({rx + 1, y + 18, rw - 2, h - 152});
    for (int i = 0; i < n; i++) font->draw(*ui.cv, *ts, rx + 6, y + 22 + i * 13, ref[i], ref[i][0] == ' ' ? DIM : TEXT);
    ui.cv->resetClip();
}
