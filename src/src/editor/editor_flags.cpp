// Editor: where things are used.
//  * the flag index: every flag the campaign sets or checks (dialogue, maps, quests, dungeons, achievements, banter,
//    locations, campaign start) and the Flags tab built on it, with notes saved in flags.txt
//  * quest stages broken out into cards (condition / text / DONE / map pin) with a "which stage shows" preview
//  * the Encounters tab's "where this table is used" panel
#include "editor.h"
#include <filesystem>
#include <algorithm>
#include <cstring>

namespace fs = std::filesystem;
using namespace ED;
extern int gEdTop;
#define TOP gEdTop
static const int BOTTOM = 18;

// ---------------------------------------------------------------- condition helpers
// the flag names a condition reads ("q_main>=2&!item:key&seen_ghost" -> q_main, seen_ghost)
static bool nonFlagTerm(const std::string& c) {
    static const char* pre[] = {"item:", "gold:", "level:", "party:", "kills:", "itemn:", "approval:", "weather:", "rep:", "mode:", "equipped:", "fav:", "map:", "stat:", "partystat:"};
    for (auto p : pre) if (startsWith(c, p)) return true;
    return c == "night" || c == "day" || c == "lit" || c == "room" || c == "stealthy";   // 1.1.7: lit (a light is on); 1.4.0: stealthy, stat checks
}
std::vector<std::string> condFlags(const std::string& cond) {
    std::vector<std::string> out;
    for (auto& part : split(cond, '&')) {
        std::string c = trim(part); if (c.empty()) continue; if (c[0] == '!') c = trim(c.substr(1));
        if (nonFlagTerm(c)) continue;
        size_t ge = c.find(">="); if (ge != std::string::npos) c = trim(c.substr(0, ge));
        if (!c.empty() && std::find(out.begin(), out.end(), c) == out.end()) out.push_back(c);
    }
    return out;
}
// flags a script ("set:a,b=2;dialogue:x;inc:c") or a set= value ("a,b=2") sets
static std::vector<std::string> setList(const std::string& v) { std::vector<std::string> o; for (auto& f : split(v, ',')) { std::string n = trim(split(trim(f), '=')[0]); if (!n.empty()) o.push_back(n); } return o; }
// 1.4.0 (#76): script:<id>,a,b,c sets the flags its arguments name when the script has "set {argN}" lines (the puzzle
// scripts take their "solved" flag as an argument). gFlagScriptText reads a script's text (set while the index is built).
static std::function<std::string(const std::string&)> gFlagScriptText;
static void scriptArgFlags(const std::string& v, std::vector<std::pair<std::string, bool>>& out) {
    if (!gFlagScriptText) return;
    auto p = split(v, ','); std::string id = trim(p[0]); if (id.empty() || p.size() < 2) return;
    std::string text = gFlagScriptText(id);
    for (auto& raw : split(text, '\n')) {
        std::string l = trim(raw); size_t sp = l.find(' '); if (sp == std::string::npos) continue;
        std::string w = toLower(l.substr(0, sp)), rest = trim(l.substr(sp + 1)); bool set = w == "set" || w == "flag" || w == "inc"; if (!set && w != "clear") continue;
        for (auto& part : split(rest, ',')) { std::string f = trim(split(part, '=')[0]);
            if (f.size() == 6 && startsWith(f, "{arg") && f[5] == '}' && isdigit((unsigned char)f[4])) { size_t n = (size_t)(f[4] - '0'); if (n >= 1 && n < p.size() && !trim(p[n]).empty()) out.push_back({trim(p[n]), set}); } }
    }
}
static void scriptFlags(const std::string& script, std::vector<std::pair<std::string, bool>>& out) {   // (flag, true = set / false = cleared)
    for (auto& a : split(script, ';')) { std::string t = trim(a); size_t p = t.find(':'); if (p == std::string::npos) continue; std::string k = trim(t.substr(0, p)), v = trim(t.substr(p + 1));
        if (k == "script") { scriptArgFlags(v, out); continue; }
        if (k == "set" || k == "flag" || k == "inc") for (auto& f : (k == "set" ? setList(v) : std::vector<std::string>{v})) out.push_back({f, true});
        else if (k == "clear") for (auto& f : setList(v)) out.push_back({f, false});
        else if (k == "join") { std::string c = parseJoin(v).cls; if (!c.empty()) out.push_back({"joined_" + c, true}); }   // the NPCs the game has loaded
        else if (k == "win") out.push_back({"game_won", true}); }
}
// "q_main>=2&!item:key" -> "q_main is at least 2 and the party does NOT have Key"
std::string Editor::condEnglish(const std::string& cond) const {
    std::string o;
    for (auto& part : split(cond, '&')) {
        std::string c = trim(part); if (c.empty()) continue; bool neg = c[0] == '!'; if (neg) c = trim(c.substr(1));
        std::string s; auto num = [&](const std::string& t) { return t.substr(t.find(':') + 1); };
        auto itemName = [&](const std::string& id) { const ItemDef* d = gd->item(id); return d ? d->name : id; };
        if (startsWith(c, "item:")) s = std::string(neg ? "the party does NOT have " : "the party has ") + itemName(c.substr(5));
        else if (startsWith(c, "gold:")) s = std::string(neg ? "the party has less than " : "the party has at least ") + num(c) + " gold";
        else if (startsWith(c, "level:")) s = std::string("the hero is ") + (neg ? "below level " : "level ") + num(c) + (neg ? "" : " or higher");
        else if (startsWith(c, "party:")) s = c.substr(6) + (neg ? " is NOT in the party" : " is in the party");
        else if (startsWith(c, "kills:")) { auto p = split(c.substr(6), ':'); s = std::string(neg ? "has NOT killed " : "has killed ") + (p.size() > 1 ? p[1] : "1") + "+ " + p[0]; }
        else if (startsWith(c, "itemn:")) { auto p = split(c.substr(6), ':'); s = std::string(neg ? "has fewer than " : "has ") + (p.size() > 1 ? p[1] : "1") + (neg ? " " : "+ ") + itemName(p[0]); }
        else if (startsWith(c, "approval:")) { auto p = split(c.substr(9), ':'); s = p[0] + " approval is " + (neg ? "below " : "at least ") + (p.size() > 1 ? p[1] : "1"); }
        else if (startsWith(c, "rep:")) { auto p = split(c.substr(4), ':'); s = "reputation with " + p[0] + " is " + (neg ? "below " : "at least ") + (p.size() > 1 ? p[1] : "1"); }
        else if (startsWith(c, "weather:")) s = std::string("the weather is ") + (neg ? "not " : "") + c.substr(8);
        else if (startsWith(c, "mode:")) s = std::string(neg ? "the player is NOT in " : "the player is in ") + c.substr(5);
        else if (startsWith(c, "equipped:")) s = std::string(neg ? "nobody wears " : "someone wears ") + itemName(c.substr(9));
        else if (startsWith(c, "fav:")) s = c.substr(4) == "any" ? std::string(neg ? "the hotbar is empty" : "something is on the hotbar") : std::string(neg ? "NOT on the hotbar: " : "on the hotbar: ") + c.substr(4);
        else if (startsWith(c, "map:")) s = std::string(neg ? "the party is NOT on " : "the party is on ") + c.substr(4);
        else if (c == "night" || c == "day") s = std::string("it is ") + (neg ? "not " : "") + c;
        else if (c == "lit") s = neg ? "the party has no light going" : "the party has a light going";
        else if (c == "stealthy") s = neg ? "the party is NOT sneaking" : "the party is sneaking (Camouflage)";   // 1.4.0
        else if (startsWith(c, "stat:") || startsWith(c, "partystat:")) { bool best = c[0] == 'p'; int st = 0, v = 0; std::string op;   // 1.4.0 (#73)
            if (!parseStatCheck(c.substr(best ? 10 : 5), st, op, v)) s = c + " (not a stat check: stat:cha>=12)";
            else { std::string ops = op == ">=" ? "at least " : op == ">" ? "more than " : op == "<=" ? "at most " : op == "<" ? "less than " : "exactly ";
                s = std::string(best ? "someone in the party has " : "the hero has ") + statName(st) + (neg ? " NOT " : " ") + ops + std::to_string(v); } }
        else if (c.find(">=") != std::string::npos) { auto p = c.find(">="); s = trim(c.substr(0, p)) + (neg ? " is below " : " is at least ") + trim(c.substr(p + 2)); }
        else s = c + (neg ? " is NOT set" : " is set");
        o += (o.empty() ? "" : " and ") + s;
    }
    return o.empty() ? "always (no condition)" : o;
}

// ---------------------------------------------------------------- scanning the campaign
void Editor::scanMaps() {
    mapScan.clear();
    for (auto& f : mapFiles) {
        MapScan ms; ms.id = f;
        if (mapLoaded && em.id == f) { ms.header = em.header; ms.raw = em.objs; for (auto& c : em.cells) ms.kinds.insert(trim(split(c, '|')[0])); }
        else { EdMap m; if (!m.load(readPath("maps/" + f + ".txt"))) continue; ms.header = m.header; ms.raw = m.objs; for (auto& c : m.cells) ms.kinds.insert(trim(split(c, '|')[0])); }
        for (auto& o : ms.raw) ms.objs.push_back(resolvedObj(o));   // 1.1.0: the indexes see template and resource keys too
        for (auto& kv : ms.header) { if (kv.first == "encounter") ms.encounter = atoi(kv.second.c_str()); else if (kv.first == "table") ms.table = kv.second; else if (kv.first == "name") ms.name = kv.second; }
        mapScan.push_back(ms);
    }
}
void Editor::ensureIndex() { if (flagIndexDirty || !indexBuilt) buildFlagIndex(); }

void Editor::buildFlagIndex() {
    gFlagScriptText = [this](const std::string& id) { std::string t; if (id.find('/') == std::string::npos && id.find('\\') == std::string::npos) readFile(readPath("scripts/" + id + ".txt"), t); return t; };   // 1.4.0
    flagIndex.clear(); flagIndexDirty = false; indexBuilt = true;
    scanMaps();
    auto add = [&](const std::string& f, bool set, const std::string& where, const std::string& detail, FlagUse u) { if (f.empty() || f[0] == '_') return; u.set = set; u.where = where; u.detail = detail; flagIndex[f].push_back(u); };
    auto addCond = [&](const std::string& cond, const std::string& where, const std::string& detail, FlagUse u) { for (auto& f : condFlags(cond)) add(f, false, where, detail, u); };
    // ---- dialogue ----
    std::string text;
    for (auto& f : dlgFiles) {
        bool open = !te.path.empty() && fs::path(te.path).stem().string() == f;
        std::vector<std::string> lines; if (open) lines = te.lines; else if (readFile(readPath("dialogue/" + f + ".txt"), text)) lines = split(text, '\n'); else continue;
        std::string node;
        for (size_t i = 0; i < lines.size(); i++) {
            std::string l = trim(lines[i]); if (l.empty() || l[0] == '#') continue;
            if (l[0] == '[' && l.back() == ']') { node = l.substr(1, l.size() - 2); continue; }
            size_t eq = l.find('='); if (eq == std::string::npos) continue;
            std::string k = trim(l.substr(0, eq)), v = trim(l.substr(eq + 1));
            FlagUse u; u.tab = 11; u.file = f; u.line = (int)i;
            std::string where = "dialogue " + f + " [" + node + "] line " + std::to_string(i + 1);
            if (k == "set") for (auto& n : setList(v)) add(n, true, where, "set=" + v, u);
            else if (k == "flag" || k == "inc") add(v, true, where, k + "=" + v, u);
            else if (k == "clear") for (auto& n : setList(v)) add(n, true, where, "clear=" + v + "  (clears it)", u);
            else if (k == "join") { std::string c = joinClassOf(v); if (!c.empty()) add("joined_" + c, true, where, "join=" + v + "  (sets joined_<class>)", u); }
            else if (k == "win") add("game_won", true, where, "win=  (the ending sets game_won)", u);
            else if (k == "option") { auto p = split(v, '|'); if (p.size() > 2) addCond(p[2], where, "option '" + trim(p[0]) + "' shows only if " + condEnglish(p[2]), u); }
        }
    }
    // ---- maps ----
    for (size_t mi = 0; mi < mapScan.size(); mi++) {
        auto& ms = mapScan[mi];
        for (size_t oi = 0; oi < ms.objs.size(); oi++) {
            auto& o = ms.objs[oi]; FlagUse u; u.tab = 0; u.file = ms.id; u.obj = (int)oi;
            std::string what = o.type; if (!o.get("id").empty()) what += " " + o.get("id"); else if (!o.get("name").empty()) what += " " + o.get("name");
            std::string where = "map " + ms.id + ": " + what + " at " + o.get("x") + "," + o.get("y");
            std::string fl = o.get("flag");
            if (!fl.empty()) {
                std::string d = o.type == "chest" ? "set when the chest is opened" : o.type == "enemy" ? "set when this group is defeated (it stays dead)" : o.type == "trigger" ? "set when the trigger fires" :
                                o.type == "door" ? "set when the door is opened (it stays open)" : o.type == "obstacle" ? "set when the obstacle is cleared" : o.type == "lever" ? "toggled by pulling the lever" : "flag=";
                add(fl, true, where, d, u);
            }
            for (const char* ck : {"show", "hide", "moveif", "openif", "require", "if"}) { std::string c = o.get(ck); if (c.empty()) continue; if (std::string(ck) == "require" && o.type == "exit") {}
                std::string d = std::string(ck) == "show" ? "only there while " : std::string(ck) == "hide" ? "gone while " : std::string(ck) == "moveif" ? "moves aside once " : std::string(ck) == "openif" ? "opens by itself once " : std::string(ck) == "if" ? "the tiles change while " : "locked until ";
                addCond(c, where, std::string(ck) + "=: " + d + condEnglish(c), u); }
            for (auto& n : split(o.get("needs"), '|')) if (startsWith(trim(n), "flag:")) addCond(trim(n).substr(5), where, "needs=flag: can be cleared once " + condEnglish(trim(n).substr(5)), u);
            for (const char* sk : {"script", "ondefeat", "onclear", "onuse", "onpress", "onrelease"}) { std::vector<std::pair<std::string, bool>> fs2; scriptFlags(o.get(sk), fs2); for (auto& p : fs2) add(p.first, true, where, std::string(sk) + "=" + o.get(sk) + (p.second ? "" : "  (clears it)"), u); }
        }
    }
    // ---- scripts (1.1.10): set / flag / inc / clear lines, and flag("x") / cond("...") reads ----
    for (auto& sf : scriptFiles) {
        std::string body; if (!ste.path.empty() && fs::path(ste.path).stem().string() == sf) body = ste.text(); else if (!readFile(readPath("scripts/" + sf + ".txt"), body)) continue;
        ScriptProg p = compileScript(sf, body);
        for (auto& r : p.refs) { FlagUse u; u.tab = 25; u.file = sf; u.line = r.line; std::string where = "script " + sf + " line " + std::to_string(r.line + 1); std::string v = trim(r.value);
            if (v.find('{') != std::string::npos) continue;
            if (r.key == "set") for (auto& n : setList(v)) add(n, true, where, "set " + v, u);
            else if (r.key == "flag" || r.key == "inc") add(v, true, where, r.key + " " + v, u);
            else if (r.key == "clear") for (auto& n : setList(v)) add(n, true, where, "clear " + v + "  (clears it)", u); }
        auto ls = split(body, '\n');
        for (size_t li = 0; li < ls.size(); li++) for (const char* fn : {"flag(\"", "cond(\""}) { size_t pos = 0; while ((pos = ls[li].find(fn, pos)) != std::string::npos) { size_t a = pos + strlen(fn), b = ls[li].find('"', a); if (b == std::string::npos) break;
            FlagUse u; u.tab = 25; u.file = sf; u.line = (int)li; std::string arg = ls[li].substr(a, b - a); addCond(arg, "script " + sf + " line " + std::to_string(li + 1), std::string(fn).substr(0, 4) + "(\"" + arg + "\")", u); pos = b; } }
    }
    // ---- quests, dungeons, locations, achievements, banter ----
    for (size_t i = 0; i < gens[6].secs.size(); i++) { auto& q = gens[6].secs[i]; int n = 0; for (auto& kv : q.kv) if (kv.first == "stage") { n++; FlagUse u; u.tab = 9; u.gen = 6; u.sec = (int)i; std::string c = trim(split(kv.second, '|')[0]); addCond(c, "quest " + q.get("id") + fmt(" stage %d", n), "the journal shows this stage when " + condEnglish(c), u); }
        for (auto& kv : q.kv) if (kv.first == "fail") { FlagUse u; u.tab = 9; u.gen = 6; u.sec = (int)i; std::string c = trim(split(kv.second, '|')[0]); addCond(c, "quest " + q.get("id") + " fail", "the quest FAILS when " + condEnglish(c), u); } }
    for (size_t i = 0; i < gens[7].secs.size(); i++) { auto& d = gens[7].secs[i]; FlagUse u; u.tab = 10; u.gen = 7; u.sec = (int)i;
        if (!d.get("bossflag").empty()) add(d.get("bossflag"), true, "dungeon " + d.get("id"), "bossflag: set when the boss " + d.get("boss") + " is defeated", u);
        std::vector<std::pair<std::string, bool>> fs2; scriptFlags(d.get("bossdefeat"), fs2); for (auto& p : fs2) add(p.first, true, "dungeon " + d.get("id"), "bossdefeat=" + d.get("bossdefeat"), u); }
    for (size_t i = 0; i < gens[8].secs.size(); i++) { auto& l = gens[8].secs[i]; FlagUse u; u.tab = 2; u.gen = 8; u.sec = (int)i; if (!l.get("require").empty()) addCond(l.get("require"), "location " + l.get("id"), "the entrance is locked until " + condEnglish(l.get("require")), u); }
    for (size_t i = 0; i < gens[12].secs.size(); i++) { auto& a = gens[12].secs[i]; FlagUse u; u.tab = 17; u.gen = 12; u.sec = (int)i; for (auto& t : split(a.get("cond"), '&')) if (startsWith(trim(t), "flag:")) addCond(trim(trim(t).substr(5)), "achievement " + a.get("id"), "unlocks when " + condEnglish(trim(trim(t).substr(5))), u); }
    for (size_t i = 0; i < gens[11].secs.size(); i++) { auto& b = gens[11].secs[i]; FlagUse u; u.tab = 16; u.gen = 11; u.sec = (int)i; std::string w = "banter " + b.get("class") + " (" + b.get("when") + ")";
        if (!b.get("if").empty()) addCond(b.get("if"), w, "the line can play only if " + condEnglish(b.get("if")), u);
    }
    // ---- set by the game itself ----
    if (flagIndex.count("reinforced3")) { FlagUse u; add("reinforced3", true, "the game (crafting)", "set when an item is reinforced to +3", u); }
    if (flagIndex.count("room")) { FlagUse u; add("room", true, "the game (party)", "a condition, not a flag: true while the party has a free place", u); }
    for (auto& c : gd->classes) if (flagIndex.count("left_" + c.first)) { FlagUse u; add("left_" + c.first, true, "the game (companions)", "set when the companion leaves the party at -40 approval; join= clears it", u); }
    // ---- campaign start ----
    for (auto& f : split(camp.flags, ',')) { std::string n = trim(split(trim(f), '=')[0]); FlagUse u; u.tab = 18; if (!n.empty()) add(n, true, "campaign start", "starting flags - set when a new game begins", u); }
    for (auto& d : flagDocs) if (!flagIndex.count(d.first)) flagIndex[d.first];   // documented but not used yet
    buildUseIndex();
}

// ---------------------------------------------------------------- flags.txt (notes)
void Editor::loadFlagDocs() {
    flagDocs.clear(); flagDocsDirty = false; std::string t;
    if (!readFile(dataRoot + "/flags.txt", t)) return;
    for (auto& sec : parseConfig(t)) if (sec.name == "flag" && !sec.get("id").empty()) flagDocs[sec.get("id")] = sec.get("desc");
}
bool Editor::saveFlagDocs() {
    std::string o = "# Flag notes - written in the Editor's Flags tab. The game does not read this file; it documents what each flag means.\n\n";
    for (auto& d : flagDocs) o += "[flag]\nid=" + d.first + "\ndesc=" + d.second + "\n\n";
    if (!writeFile(dataRoot + "/flags.txt", o)) { say("Could not write flags.txt"); return false; }
    flagDocsDirty = false; say("Saved " + dataRoot + "/flags.txt"); return true;
}

// ---------------------------------------------------------------- jumping to a use
void Editor::jumpToUse(const FlagUse& u) {
    if (u.tab == 25) { openScript(u.file, u.line); return; }   // 1.1.10 scripts
    if (u.tab == 11) {
        for (size_t i = 0; i < dlgFiles.size(); i++) if (dlgFiles[i] == u.file) dlgSel = (int)i;
        auto go = [this, u]() { if (te.path.empty() || fs::path(te.path).stem().string() != u.file) { te.load(readPath("dialogue/" + u.file + ".txt")); te.path = dataRoot + "/dialogue/" + u.file + ".txt"; }
            te.cy = clampi(u.line, 0, (int)te.lines.size() - 1); te.cx = 0; te.ax = te.ay = -1; te.scroll = std::max(0, te.cy - 6); tab = 11; lintShown = false; };
        if (te.dirty && fs::path(te.path).stem().string() != u.file) openConfirm("Discard unsaved changes to the open dialogue?", go); else go();
        return;
    }
    if (u.tab == 0) {
        auto go = [this, u]() { if (!mapLoaded || em.id != u.file) { for (size_t i = 0; i < mapFiles.size(); i++) if (mapFiles[i] == u.file) mapListSel = (int)i; loadMapFile(u.file); }
            tab = 0; if (u.obj >= 0 && u.obj < (int)em.objs.size()) { objSel = u.obj; mapTool = 3; int cell = 12 * zoom; viewX = std::max(0, em.objs[u.obj].geti("x") * cell - 300); viewY = std::max(0, em.objs[u.obj].geti("y") * cell - 220); } };
        if (mapDirty && em.id != u.file) openConfirm("Discard unsaved changes to the current map?", go); else go();
        return;
    }
    if (u.tab == 19) { flagSelName = u.file; tab = 19; return; }
    if (u.gen >= 0 && u.gen < (int)gens.size()) { auto& g = gens[u.gen]; g.sel = u.sec; g.filter.clear(); g.scroll = std::max(0, u.sec - 5); g.fieldScroll = 0; }
    if (u.tab >= 0) tab = u.tab;
}

// a "flag" picker button: choose one of the known flags (or type a new one); onPick gets the name
void Editor::flagPicker(int x, int y, int w, bool appendCond, std::function<void(const std::string&)> onPick, const std::string& label) {
    if (ui.button(x, y, w, 16, !label.empty() ? label : appendCond ? "+ flag" : "flag...")) {
        ensureIndex();
        std::vector<std::string> names, labels; for (auto& f : flagIndex) { int s = 0, c = 0; for (auto& u : f.second) (u.set ? s : c)++; names.push_back(f.first); labels.push_back(f.first + fmt("   (set by %d, checked by %d)", s, c)); }
        names.insert(names.begin(), ""); labels.insert(labels.begin(), "+ a new flag name...");
        // 1.4.0 (#73, #74): conditions that aren't flags - stat checks and sneaking - from the same list
        if (appendCond) { names.insert(names.begin() + 1, {"#stat", "#partystat", "stealthy"}); labels.insert(labels.begin() + 1, {"+ a stat check: the hero's stat (stat:cha>=12)...", "+ a stat check: the best in the party (partystat:str>=14)...", "stealthy   (the party is sneaking: Camouflage)"}); }
        openListPicker(appendCond ? "Flag to add to the condition (& = and)" : "Flag", labels, 0, [this, names, onPick](int i) {
            if (i == 0) openTextPrompt("New flag name (letters, digits, _)", "my_flag", [this, onPick](const std::string& n) { std::string f; for (char c : n) if (isalnum((unsigned char)c) || c == '_') f += c; if (f.empty()) return; if (!flagDocs.count(f)) { flagDocs[f] = ""; flagDocsDirty = true; flagIndexDirty = true; } onPick(f); });
            else if (names[i] == "#stat" || names[i] == "#partystat") {
                std::string pre = names[i].substr(1) + ":";
                std::vector<std::string> st = {"STR (strength)", "DEX (dexterity)", "CON (constitution)", "INT (intelligence)", "WIS (wisdom)", "CHA (charisma)"};
                openListPicker(pre == "stat:" ? "Which of the hero's stats?" : "Which stat (the best in the party counts)?", st, 5, [this, pre, onPick](int s) {
                    std::string sn = toLower(statName(s));
                    openTextPrompt("At least how much " + std::string(statName(s)) + "? (12 is good, 14 is hard; >, <=, < or = in front for other tests)", "12", [pre, sn, onPick](const std::string& n0) {
                        std::string n = trim(n0); if (n.empty()) return; if (isdigit((unsigned char)n[0])) n = ">=" + n;
                        int a, b; std::string op; if (!parseStatCheck(sn + n, a, op, b)) return; onPick(pre + sn + n); }); });
            }
            else onPick(names[i]);
        });
    }
    if (ui.hover(x, y, w, 16)) ui.tip("Pick a flag the campaign already uses (the Flags tab lists where each is set and checked)");
}
// one line: where a flag gets set (for the quest cards and object panel tooltips)
std::string Editor::flagSetSummary(const std::string& f) {
    ensureIndex();
    auto it = flagIndex.find(f); std::vector<const FlagUse*> sets; if (it != flagIndex.end()) for (auto& u : it->second) if (u.set) sets.push_back(&u);
    if (sets.empty()) return "";
    std::string o = sets[0]->where; if (sets.size() > 1) o += fmt("  (+%d more)", (int)sets.size() - 1);
    return o;
}

// ---------------------------------------------------------------- the Flags tab
void Editor::drawFlags(InputState& in) {
    ensureIndex();
    int x = 4, y = TOP + 4, h = H - TOP - BOTTOM - 8, lw = 280;
    ui.panel(x, y, lw, h); listHeader(x, y, lw, fmt("Flags (%d)", (int)flagIndex.size()), IMP_FLAGS); helpBtn(x + lw - 16, y + 1, "tab.flags");
    ui.searchField(x + 4, y + 20, lw - 8, flagFilter, "search flags and notes...");
    std::vector<std::string> names;
    int hiddenN = 0;
    for (auto& kv : flagIndex) {
        if (!edMatch(kv.first + " " + (flagDocs.count(kv.first) ? flagDocs[kv.first] : std::string()), flagFilter)) continue;
        // "story flags": hide the bookkeeping flags of chests / enemy groups / obstacles that nothing ever checks
        if (flagStoryOnly && flagFilter.empty() && !kv.second.empty() && !flagDocs.count(kv.first)) { bool book = true; for (auto& u : kv.second) if (!u.set || u.tab != 0) book = false; if (book) { hiddenN++; continue; } }
        names.push_back(kv.first);
    }
    // problem flags first: checked but never set
    auto counts = [&](const std::string& n, int& s, int& c) { s = c = 0; for (auto& u : flagIndex[n]) (u.set ? s : c)++; };
    std::stable_sort(names.begin(), names.end(), [&](const std::string& a, const std::string& b) { int sa, ca, sb, cb; counts(a, sa, ca); counts(b, sb, cb); return (sa == 0 && ca > 0) > (sb == 0 && cb > 0); });
    int lx = x + 4, ly = y + 40, lh = h - 136, rowH = 15, rows = lh / rowH;
    ui.cv->fillRect(lx, ly, lw - 8, lh, FIELD); ui.cv->rect(lx, ly, lw - 8, lh, BORDER);
    if (ui.hover(lx, ly, lw - 8, lh) && in.wheel) flagScroll -= in.wheel * 3;
    flagScroll = clampi(flagScroll, 0, std::max(0, (int)names.size() - rows));
    ui.cv->setClip({lx + 1, ly + 1, lw - 10, lh - 2});
    for (int r = 0; r < rows && flagScroll + r < (int)names.size(); r++) {
        const std::string& n = names[flagScroll + r]; int ry = ly + 1 + r * rowH; int s, c; counts(n, s, c);
        bool hv = ui.hover(lx, ry, lw - 8, rowH);
        if (n == flagSelName) ui.cv->fillRect(lx + 1, ry, lw - 10, rowH, SEL); else if (hv) ui.cv->fillRect(lx + 1, ry, lw - 10, rowH, HOVER);
        Color8 col = (s == 0 && c > 0) ? RGB(255, 120, 120) : (c == 0 && s > 0) ? DIM : (s == 0 && c == 0) ? RGB(150, 170, 220) : TEXT;
        font->draw(*ui.cv, *ts, lx + 5, ry + 1, n, col);
        std::string cnt = fmt("%d / %d", s, c); font->draw(*ui.cv, *ts, lx + lw - 16 - font->width(cnt), ry + 1, cnt, DIM);
        if (hv && in.mousePressed[0] && !ui.mouseUsed) { ui.mouseUsed = true; flagSelName = n; flagUseScroll = 0; ui.blur(); }
    }
    ui.cv->resetClip();
    int by = ly + lh + 4;
    ui.checkbox(lx, by, fmt("story flags only (%d chest/enemy flags hidden)", hiddenN), flagStoryOnly); by += 16;
    ui.text(lx, by, "set / checked.  red = checked, never set", DIM); by += 13; ui.text(lx, by, "grey = never checked   blue = noted only", DIM); by += 18;
    if (ui.button(lx, by, 80, 20, "New flag")) openTextPrompt("New flag name (letters, digits, _)", "my_flag", [this](const std::string& n) { std::string fl; for (char ch : n) if (isalnum((unsigned char)ch) || ch == '_') fl += ch; if (fl.empty()) return; if (!flagDocs.count(fl)) flagDocs[fl] = ""; flagDocsDirty = true; flagIndex[fl]; flagSelName = fl; });
    if (ui.button(lx + 84, by, 70, 20, "Refresh")) { buildFlagIndex(); say(fmt("Scanned %d maps, %d dialogue files: %d flags.", (int)mapScan.size(), (int)dlgFiles.size(), (int)flagIndex.size())); }
    if (ui.button(lx + 158, by, 110, 20, "Save notes", flagDocsDirty, false, OK) || (in.ctrl() && in.pressed[K_S] && flagDocsDirty && pop == P_NONE)) saveFlagDocs();
    // ---- details ----
    int fx = x + lw + 6, fw = W - fx - 4;
    ui.panel(fx, y, fw, h);
    if (flagSelName.empty() || !flagIndex.count(flagSelName)) {
        ui.header(fx, y, fw, "Flags");
        font->drawWrapped(*ui.cv, *ts, fx + 10, y + 26, fw - 20, "A flag is a named switch (or counter) the story remembers: dialogue sets it (set=met_mara), and doors, quest stages, NPCs, triggers and options check it. Pick a flag on the left to see every place that sets it and every place that checks it - click a line to jump there. Flags only checked and never set are listed first in red: nothing in the game can ever turn them on. Type a note to remember what a flag means; notes are saved in flags.txt.", DIM);
        return;
    }
    auto& uses = flagIndex[flagSelName]; int s, c; counts(flagSelName, s, c);
    ui.header(fx, y, fw, "Flag: " + flagSelName); helpBtn(fx + fw - 16, y + 1, "flags.status", "tab.flags");
    int cy = y + 24;
    std::string status = (s == 0 && c > 0) ? "PROBLEM: checked but never set anywhere - add set=" + flagSelName + " to a dialogue node, trigger script or object, or fix a typo."
                       : (c == 0 && s > 0) ? "Set but never checked - harmless, but nothing reacts to it yet." : (s == 0 && c == 0) ? "Noted only - not used anywhere yet." : fmt("OK - set in %d place(s), checked in %d.", s, c);
    font->drawWrapped(*ui.cv, *ts, fx + 10, cy, fw - 20, status, (s == 0 && c > 0) ? DANGER : (s && c) ? OK : DIM); cy += 18;
    if (ui.button(fx + fw - 150, cy - 18, 140, 16, "Rename everywhere...", s + c > 0)) {
        bool any = mapDirty || te.dirty || camp.dirty || flagDocsDirty; for (int gi : {6, 7, 8, 11, 12}) if (gens[gi].dirty) any = true;
        if (any) say("Save your open changes first (maps, dialogue, quests, dungeons, locations, banter, achievements, campaign, flag notes) - renaming rewrites those files.");
        else { std::string from = flagSelName; openTextPrompt("Rename flag '" + from + "' to", from, [this, from](const std::string& n) { std::string to; for (char ch : n) if (isalnum((unsigned char)ch) || ch == '_') to += ch; if (to.empty() || to == from) return; if (flagIndex.count(to)) { std::string t2 = to; openConfirm("A flag called '" + to + "' already exists - the two will become one flag. Rename anyway?", [this, from, t2]() { renameFlag(from, t2); }); } else renameFlag(from, to); }); }
    }
    if (ui.hover(fx + fw - 150, cy - 18, 140, 16)) ui.tip("Renames the flag in every dialogue, map, quest, dungeon, location, achievement, banter line and the campaign start, and saves them");
    ui.label(fx + 10, cy + 2, "note"); helpBtn(fx + 44, cy + 1, "flags.desc", "tab.flags");
    { std::string d = flagDocs.count(flagSelName) ? flagDocs[flagSelName] : ""; if (ui.textField(fx + 62, cy, fw - 72, d, 300)) { flagDocs[flagSelName] = d; flagDocsDirty = true; } }
    cy += 24;
    // uses
    int ah = y + h - cy - 6; int lineH = 30;
    std::vector<const FlagUse*> list; for (auto& u : uses) if (u.set) list.push_back(&u); size_t nSet = list.size(); for (auto& u : uses) if (!u.set) list.push_back(&u);
    int total = (int)list.size() * lineH + 2 * 22;
    if (ui.hover(fx, cy, fw, ah) && in.wheel) flagUseScroll -= in.wheel * lineH;
    flagUseScroll = clampi(flagUseScroll, 0, std::max(0, total - ah));
    ui.cv->setClip({fx + 1, cy, fw - 2, ah});
    int ry = cy - flagUseScroll;
    for (size_t i = 0; i <= list.size(); i++) {
        if (i == 0 || i == nSet) { bool setHead = i == 0; if (!(i == nSet && i == 0 && false)) { ui.cv->fillRect(fx + 4, ry, fw - 8, 18, PANEL2); font->draw(*ui.cv, *ts, fx + 10, ry + 3, setHead ? fmt("Set by (%d)", (int)nSet) : fmt("Checked by (%d)", (int)(list.size() - nSet)), ACCENT2); ry += 22;
            if (setHead && nSet == 0) { font->draw(*ui.cv, *ts, fx + 14, ry, "nothing sets this flag", s == 0 && c > 0 ? DANGER : DIM); ry += 18; }
            if (setHead && nSet == list.size()) { ui.cv->fillRect(fx + 4, ry, fw - 8, 18, PANEL2); font->draw(*ui.cv, *ts, fx + 10, ry + 3, "Checked by (0)", ACCENT2); ry += 22; font->draw(*ui.cv, *ts, fx + 14, ry, "nothing checks this flag", DIM); ry += 18; } } }
        if (i == list.size()) break;
        const FlagUse& u = *list[i];
        bool vis = ry + lineH > cy && ry < cy + ah;
        bool hv = vis && ui.hover(fx + 4, std::max(ry, cy), fw - 8, lineH - 2);
        if (hv) ui.cv->fillRect(fx + 4, ry, fw - 8, lineH - 2, HOVER);
        font->draw(*ui.cv, *ts, fx + 14, ry + 2, u.where, u.set ? OK : ACCENT);
        std::string det = u.detail; auto wl = font->wrap(det, fw - 40); font->draw(*ui.cv, *ts, fx + 24, ry + 15, wl.empty() ? "" : wl[0], DIM);
        if (hv) { ui.tip("Click to open it"); if (in.mousePressed[0] && !ui.mouseUsed) { ui.mouseUsed = true; FlagUse cp = u; ui.cv->resetClip(); jumpToUse(cp); return; } }
        ry += lineH;
    }
    ui.cv->resetClip();
}

// ---------------------------------------------------------------- quest stages as cards
// a stage line is "condition | text", the text may start with DONE and end with " @location" (a map pin)
struct StageParts { std::string cond, text, pin; bool done = false; };
static StageParts parseStage(const std::string& v) {
    StageParts p; size_t bar = v.find('|'); p.cond = trim(bar == std::string::npos ? std::string() : v.substr(0, bar)); std::string t = trim(bar == std::string::npos ? v : v.substr(bar + 1));
    if (startsWith(t, "DONE")) { p.done = true; t = trim(t.substr(4)); }
    size_t at = t.rfind(" @"); if (at != std::string::npos && t.find(' ', at + 2) == std::string::npos) { p.pin = trim(t.substr(at + 2)); t = trim(t.substr(0, at)); }
    else if (startsWith(t, "@") && t.find(' ') == std::string::npos) { p.pin = t.substr(1); t.clear(); }
    p.text = t; return p;
}
static std::string joinStage(const StageParts& p) { return p.cond + " | " + (p.done ? "DONE " : "") + p.text + (p.pin.empty() ? "" : " @" + p.pin); }

// objective=STAGE | DONE-CONDITION | TEXT
struct ObjParts { int stage = 0; std::string cond, text; };
static ObjParts parseObj(const std::string& v) { ObjParts o; auto p = split(v, '|'); o.stage = atoi(trim(p[0]).c_str()); if (p.size() > 1) o.cond = trim(p[1]); for (size_t i = 2; i < p.size(); i++) o.text += (i > 2 ? "|" : "") + (i == 2 ? trim(p[i]) : p[i]); return o; }
static std::string joinObj(const ObjParts& o) { return std::to_string(o.stage) + " | " + o.cond + " | " + o.text; }

int Editor::drawQuestStages(GenericEditor& g, EdSection& s, int x, int y, int w, int clipTop, int clipBottom, InputState& in) {
    std::vector<size_t> st; for (size_t i = 0; i < s.kv.size(); i++) if (s.kv[i].first == "stage") st.push_back(i);
    int y0 = y; auto vis = [&](int yy, int hh = 16) { return yy >= clipTop && yy + hh <= clipBottom; };
    // explanation
    if (vis(y, 30)) { ui.cv->fillRect(x + 4, y, w - 8, 30, PANEL2); helpBtn(x + 10, y + 2, "quest.stage", "tab.quests");
        ui.text(x + 28, y + 3, fmt("Stages (%d) - the journal shows the LAST stage whose 'Shows when' is true,", (int)st.size()), ACCENT2);
        ui.text(x + 28, y + 16, "so list them from first to last. A stage marked DONE greys the quest out.", DIM); }
    y += 36;
    int moveFrom = -1, moveDir = 0, del = -1; bool any = false;
    auto locIds = [&]() { std::vector<std::string> o{"(no pin)"}; for (auto& l : gens[8].secs) o.push_back(l.get("id")); for (auto& m : mapFiles) if (std::find(o.begin(), o.end(), m) == o.end()) o.push_back(m); return o; };
    for (size_t k = 0; k < st.size(); k++) {
        std::string& v = s.kv[st[k]].second; StageParts p = parseStage(v); bool ch = false;
        auto fl = condFlags(p.cond);
        std::vector<size_t> objs; for (size_t i = 0; i < s.kv.size(); i++) if (s.kv[i].first == "objective" && parseObj(s.kv[i].second).stage == (int)k + 1) objs.push_back(i);
        int cardH = 78 + (int)fl.size() * 13 + (int)objs.size() * 20 + 22;
        if (vis(y, 2)) { ui.cv->fillRect(x + 4, y, w - 8, cardH - 6, RGB(33, 36, 44)); ui.cv->rect(x + 4, y, w - 8, cardH - 6, p.done ? OK : BORDER); }
        int ry = y + 4;
        if (vis(ry)) {
            font->bold = true; ui.text(x + 10, ry + 2, fmt("#%d", (int)k + 1), p.done ? OK : ACCENT2); font->bold = false;
            ui.label(x + 40, ry + 2, "shows when"); helpBtn(x + 112, ry + 1, "quest.stage.when", "quest.stage");
            if (ui.textField(x + 130, ry, w - 470, p.cond, 300)) ch = true;
            { size_t idx = st[k]; EdSection* sp = &s; GenericEditor* gp = &g; flagPicker(x + w - 334, ry, 52, true, [sp, idx, gp](const std::string& f) { if (idx >= sp->kv.size()) return; StageParts q = parseStage(sp->kv[idx].second); q.cond = trim(q.cond).empty() ? f : trim(q.cond) + "&" + f; sp->kv[idx].second = joinStage(q); gp->dirty = true; }); }
            bool d = p.done; if (ui.checkbox(x + w - 274, ry + 2, "DONE", d)) { p.done = d; ch = true; } helpBtn(x + w - 214, ry + 1, "quest.stage.done", "quest.stage");
            ui.label(x + w - 196, ry + 2, "pin"); helpBtn(x + w - 174, ry + 1, "quest.stage.pin", "quest.stage");
            if (ui.choice(x + w - 158, ry, 84, p.pin.empty() ? "(none)" : p.pin)) { auto ids = locIds(); int cur = 0; for (size_t i = 0; i < ids.size(); i++) if (ids[i] == p.pin) cur = (int)i; size_t idx = st[k]; EdSection* sp = &s; GenericEditor* gp = &g;
                openListPicker("Map pin: where the world map (M) marks this stage", ids, cur, [ids, idx, sp, gp](int i) { StageParts q = parseStage(sp->kv[idx].second); q.pin = i == 0 ? "" : ids[i]; sp->kv[idx].second = joinStage(q); gp->dirty = true; }); }
            if (ui.button(x + w - 70, ry, 18, 16, "^", k > 0)) { moveFrom = (int)k; moveDir = -1; }
            if (ui.button(x + w - 50, ry, 18, 16, "v", k + 1 < st.size())) { moveFrom = (int)k; moveDir = 1; }
            if (ui.button(x + w - 28, ry, 18, 16, "x", true, false, DANGER)) del = (int)k;
        } else ui.idCounter += 1;
        ry += 22;
        if (vis(ry)) { ui.label(x + 40, ry + 2, "journal"); helpBtn(x + 112, ry + 1, "quest.stage.text", "quest.stage"); if (ui.textField(x + 130, ry, w - 140, p.text, 400)) ch = true; } else ui.idCounter += 1;
        ry += 20;
        if (vis(ry, 13)) { auto wl = font->wrap("Reads: " + condEnglish(p.cond), w - 150); ui.text(x + 130, ry, wl.empty() ? "" : wl[0], RGB(150, 200, 255)); }
        ry += 14;
        for (auto& f : fl) { if (vis(ry, 13)) { std::string ws = flagSetSummary(f); ui.text(x + 140, ry, f + ":", TEXT); ui.text(x + 146 + font->width(f), ry, ws.empty() ? "never set anywhere!  (Flags tab)" : "set by " + ws, ws.empty() ? DANGER : DIM);
                if (ui.hover(x + 140, ry, w - 150, 13)) { ui.tip("Click to see this flag in the Flags tab"); if (in.mousePressed[0] && !ui.mouseUsed) { ui.mouseUsed = true; flagSelName = f; tab = 19; } } } ry += 13; }
        // checklist lines shown under this stage in the journal
        int objDel = -1;
        for (size_t oi = 0; oi < objs.size(); oi++) {
            std::string& ov = s.kv[objs[oi]].second; ObjParts op = parseObj(ov); bool och = false;
            if (vis(ry)) { ui.text(x + 40, ry + 2, "objective", DIM); helpBtn(x + 112, ry + 1, "quest.objective", "quest.stage");
                ui.text(x + 130, ry + 2, "done when", DIM); if (ui.textField(x + 196, ry, 170, op.cond, 200)) och = true; if (ui.hover(x + 196, ry, 170, 16) && !op.cond.empty()) ui.tip("Reads: " + condEnglish(op.cond));
                if (ui.textField(x + 372, ry, w - 410, op.text, 300)) och = true;
                if (ui.hover(x + 372, ry, w - 410, 16)) ui.tip("{itemn:id} {kills:creature} {flag:name} {gold} show live counts, e.g. Moonleaf ({itemn:herb_moon}/3)");
                if (ui.button(x + w - 28, ry, 18, 16, "x", true, false, DANGER)) objDel = (int)objs[oi]; }
            else ui.idCounter += 2;
            if (och) { ov = joinObj(op); g.dirty = true; }
            ry += 20;
        }
        if (vis(ry)) { if (ui.button(x + 130, ry, 96, 16, "+ objective")) { ObjParts op; op.stage = (int)k + 1; op.cond = "itemn:ITEM:3"; op.text = "Collect 3 things ({itemn:ITEM}/3)"; s.kv.push_back({"objective", joinObj(op)}); g.dirty = true; }
            ui.text(x + 234, ry + 2, "checklist lines the journal shows under this stage", DIM); }
        if (objDel >= 0) { s.kv.erase(s.kv.begin() + objDel); g.dirty = true; return y - y0; }
        if (ch) { v = joinStage(p); g.dirty = true; any = true; }
        y += cardH;
    }
    if (moveFrom >= 0) { std::swap(s.kv[st[moveFrom]], s.kv[st[moveFrom + moveDir]]); int a = moveFrom + 1, b = moveFrom + moveDir + 1;   // objectives follow their stage
        for (auto& kv : s.kv) if (kv.first == "objective") { ObjParts op = parseObj(kv.second); if (op.stage == a) op.stage = b; else if (op.stage == b) op.stage = a; else continue; kv.second = joinObj(op); }
        g.dirty = true; }
    if (del >= 0) { size_t i = st[del]; int num = del + 1; openConfirm(fmt("Delete stage #%d (and its objectives)?", num), [&s, &g, i, num]() { if (i >= s.kv.size()) return; s.kv.erase(s.kv.begin() + i);
            for (size_t j = 0; j < s.kv.size(); j++) if (s.kv[j].first == "objective") { ObjParts op = parseObj(s.kv[j].second); if (op.stage == num) { s.kv.erase(s.kv.begin() + j); j--; continue; } if (op.stage > num) { op.stage--; s.kv[j].second = joinObj(op); } }
            g.dirty = true; }); }
    // ---- failure: fail=COND | why. Ends the quest as FAILED (unless a DONE stage already shows) ----
    { std::vector<size_t> fl; for (size_t i = 0; i < s.kv.size(); i++) if (s.kv[i].first == "fail") fl.push_back(i);
      int dones = 0; for (auto i : st) if (parseStage(s.kv[i].second).done) dones++;
      if (vis(y, 16)) { helpBtn(x + 10, y + 1, "quest.fail", "tab.quests"); ui.text(x + 28, y + 2, dones > 1 ? fmt("Fail conditions   (%d DONE stages = %d possible endings; the last true one shows)", dones, dones) : "Fail conditions   (the quest ends as FAILED when one is true)", ACCENT2); }
      y += 20; int fdel = -1;
      for (auto i : fl) { std::string& fv = s.kv[i].second; auto bar = fv.find('|'); std::string fc = trim(bar == std::string::npos ? fv : fv.substr(0, bar)), ft = bar == std::string::npos ? "" : trim(fv.substr(bar + 1)); bool fch = false;
          if (vis(y)) { ui.text(x + 40, y + 2, "fails when", DANGER); if (ui.textField(x + 130, y, 200, fc, 200)) fch = true; if (ui.hover(x + 130, y, 200, 16) && !fc.empty()) ui.tip("Reads: " + condEnglish(fc));
              { size_t idx = i; EdSection* sp = &s; GenericEditor* gp = &g; flagPicker(x + 336, y, 52, true, [sp, idx, gp](const std::string& f) { if (idx >= sp->kv.size()) return; std::string& v2 = sp->kv[idx].second; auto b2 = v2.find('|'); std::string c2 = trim(b2 == std::string::npos ? v2 : v2.substr(0, b2)), t2 = b2 == std::string::npos ? "" : v2.substr(b2); v2 = (c2.empty() ? f : c2 + "&" + f) + " " + (t2.empty() ? "| The chance has passed." : t2); gp->dirty = true; }); }
              if (ui.textField(x + 394, y, w - 432, ft, 300)) fch = true; if (ui.hover(x + 394, y, w - 432, 16)) ui.tip("What the journal says after 'FAILED:'");
              if (ui.button(x + w - 28, y, 18, 16, "x", true, false, DANGER)) fdel = (int)i; }
          else ui.idCounter += 3;
          if (fch) { fv = fc + " | " + ft; g.dirty = true; }
          y += 20; }
      if (fdel >= 0) { s.kv.erase(s.kv.begin() + fdel); g.dirty = true; return y - y0; }
      if (vis(y)) { if (ui.button(x + 130, y, 120, 16, "+ fail condition")) { s.kv.push_back({"fail", s.get("id") + "_failed | The chance has passed."}); g.dirty = true; } ui.text(x + 258, y + 2, "e.g. a flag set when an NPC dies or a timer runs out", DIM); }
      y += 26; }
    if (vis(y, 20)) { if (ui.button(x + 10, y, 90, 18, "+ stage", true, false, OK)) { std::string prev = st.empty() ? "" : parseStage(s.kv[st.back()].second).cond; s.kv.push_back({"stage", (prev.empty() ? s.get("id") + "_started" : prev) + " | What the journal says now."}); g.dirty = true; }
        ui.text(x + 108, y + 3, "Tip: one counter flag (set=q_x=2) with stages q_x>=1, q_x>=2... keeps a quest in order.", DIM); }
    y += 28;
    // ---- preview ----
    std::vector<std::string> conds; for (size_t k = 0; k < st.size(); k++) conds.push_back(parseStage(s.kv[st[k]].second).cond);
    std::vector<std::pair<std::string, std::string>> fails; for (auto& kv : s.kv) if (kv.first == "fail") { auto b = kv.second.find('|'); fails.push_back({trim(b == std::string::npos ? kv.second : kv.second.substr(0, b)), b == std::string::npos ? "" : trim(kv.second.substr(b + 1))}); conds.push_back(fails.back().first); }
    std::vector<std::string> terms; for (auto& cd : conds) for (auto& part : split(cd, '&')) { std::string c = trim(part); if (!c.empty() && c[0] == '!') c = trim(c.substr(1)); size_t ge = c.find(">="); if (ge != std::string::npos) c = trim(c.substr(0, ge)); if (!c.empty() && std::find(terms.begin(), terms.end(), c) == terms.end()) terms.push_back(c); }
    if (vis(y, 18)) { ui.cv->fillRect(x + 4, y, w - 8, 18, PANEL2); ui.text(x + 28, y + 3, "Preview - set flag values as the player might have them, see which stage the journal shows", ACCENT2); helpBtn(x + 10, y + 2, "quest.preview", "tab.quests"); }
    y += 22;
    int col = 0, px0 = x + 10;
    for (auto& t : terms) {
        int px = px0 + col * 220; int& val = questPreview[t];
        if (vis(y)) { std::string lbl = t.size() > 16 ? t.substr(0, 15) + "." : t; ui.text(px, y + 2, lbl, nonFlagTerm(t) ? DIM : TEXT);
            if (nonFlagTerm(t)) { bool b = val != 0; ui.checkbox(px + 130, y + 2, "true", b); val = b ? 1 : 0; } else ui.intField(px + 120, y, 80, val, 0, 99); }
        else ui.idCounter += 1;
        if (++col == 3) { col = 0; y += 20; }
    }
    if (col) y += 20;
    // evaluate like the game: flags by value (>= n, or set = non-zero), other terms by their checkbox
    auto eval = [&](const std::string& cond) { for (auto& part : split(cond, '&')) { std::string c = trim(part); if (c.empty()) continue; bool neg = c[0] == '!'; if (neg) c = trim(c.substr(1)); bool val;
            size_t ge = c.find(">="); if (ge != std::string::npos && !nonFlagTerm(c)) { std::string n = trim(c.substr(0, ge)); val = questPreview[n] >= atoi(c.substr(ge + 2).c_str()); } else val = questPreview[c] != 0;
            if (neg) val = !val; if (!val) return false; } return true; };
    int showing = -1; for (size_t k = 0; k < st.size(); k++) if (eval(parseStage(s.kv[st[k]].second).cond)) showing = (int)k;
    if (vis(y, 14)) { if (showing < 0) ui.text(x + 10, y, "Journal shows: nothing - the quest is not in the journal yet.", DIM);
        else if (!parseStage(s.kv[st[showing]].second).done && [&]() { for (auto& f : fails) if (!f.first.empty() && eval(f.first)) return true; return false; }()) { std::string why; for (auto& f : fails) if (!f.first.empty() && eval(f.first)) { why = f.second; break; } ui.text(x + 10, y, "Journal shows: FAILED: " + why, DANGER); }
        else { StageParts p = parseStage(s.kv[st[showing]].second); auto wl = font->wrap(fmt("Journal shows #%d: ", showing + 1) + (p.done ? "(done) " : "") + p.text + (p.pin.empty() ? "" : "   [pin: " + p.pin + "]"), w - 30); ui.text(x + 10, y, wl.empty() ? "" : wl[0], p.done ? OK : C::White); } }
    y += 20;
    if (any) {}
    return y - y0;
}

// ---------------------------------------------------------------- encounter tables: where used
Editor::EncUsage Editor::encounterUsage(const std::string& id) {
    ensureIndex();
    EncUsage u; std::string base = id; bool night = false;
    if (base.size() > 6 && base.substr(base.size() - 6) == "_night") { base = base.substr(0, base.size() - 6); night = true; }
    u.night = night; u.base = base;
    for (auto& e : gens[5].secs) { if (e.get("id") == id + "_night") u.hasNight = true; if (night && e.get("id") == base) u.hasDay = true; }
    std::set<std::string> kinds;
    for (auto& k : allTileKinds()) if (k.biome == base) { u.kinds.push_back({k.name, k.encounter}); if (k.encounter > 0) kinds.insert(k.name); }
    bool owFile = false;
    for (auto& ms : mapScan) {
        if (ms.id == "overworld") owFile = true;
        if (ms.table == id || (night && ms.table == base)) u.tableMaps.push_back({ms.id, ms.encounter});
        if (ms.encounter > 0) { int n = 0; for (auto& k : ms.kinds) if (kinds.count(k)) n++; if (n) u.biomeMaps.push_back({ms.id, ms.encounter}); }
    }
    if (!owFile && !kinds.empty()) u.biomeMaps.push_back({"overworld (generated)", 4});
    if (!owFile && base == "plains") u.tableMaps.push_back({"overworld (generated)", 4});
    for (size_t i = 0; i < gens[7].secs.size(); i++) if (gens[7].secs[i].get("table") == id) u.dungeons.push_back(gens[7].secs[i].get("id"));
    return u;
}
bool Editor::encounterUnused(const std::string& id) {
    if (id.empty()) return false;
    auto u = encounterUsage(id);
    bool kindUse = false; for (auto& k : u.kinds) if (k.second > 0) kindUse = true;
    bool mapUse = false; for (auto& m : u.tableMaps) if (m.second > 0) mapUse = true;
    return !kindUse && !mapUse && u.dungeons.empty();
}
void Editor::drawEncounterUsage(int x, int y, int w, int h, InputState& in) {
    GenericEditor& g = gens[5];
    ui.panel(x, y, w, h); ui.header(x, y, w, "Where this table is used"); helpBtn(x + w - 16, y + 1, "table.usedby", "tab.encounters");
    if (g.sel < 0 || g.sel >= (int)g.secs.size()) { font->drawWrapped(*ui.cv, *ts, x + 8, y + 24, w - 16, "Pick a table. Random fights happen when the party steps on a tile whose kind has an encounter weight, on a map with an encounter rate above 0. The table used is the tile kind's biome (at night <biome>_night first), else the map's table=.", DIM); return; }
    std::string id = g.secs[g.sel].get("id"); auto u = encounterUsage(id);
    int cy = y + 22; int lineH = 14;
    struct Row { std::string text; Color8 col; int kind; std::string target; };   // kind: 0 text, 1 tile kind, 2 map, 3 dungeon
    std::vector<Row> rows;
    auto head = [&](const std::string& s) { rows.push_back({"", DIM, 0, ""}); rows.push_back({s, ACCENT2, 0, ""}); };
    if (u.night) rows.push_back({"Night table: replaces '" + u.base + "' between dusk and dawn" + (u.hasDay ? "." : " (no '" + u.base + "' table exists!)"), u.hasDay ? TEXT : DANGER, 0, ""});
    else if (u.hasNight) rows.push_back({"At night '" + id + "_night' is used instead.", DIM, 0, ""});
    head("Tile kinds with biome=" + u.base + ":");
    if (u.kinds.empty()) rows.push_back({"  none - no tile kind rolls this table", DIM, 0, ""});
    for (auto& k : u.kinds) rows.push_back({"  " + k.first + (k.second > 0 ? fmt("   weight %d", k.second) : "   weight 0 - never ambushes"), k.second > 0 ? TEXT : DIM, 1, k.first});
    head("Maps that roll it on those tiles:");
    if (u.biomeMaps.empty()) rows.push_back({"  none (a map needs encounter > 0 and those tiles)", DIM, 0, ""});
    for (auto& m : u.biomeMaps) rows.push_back({"  " + m.first + fmt("   rate %d", m.second), TEXT, 2, m.first});
    head("Maps with table=" + id + " (fallback for tiles without a biome table):");
    if (u.tableMaps.empty()) rows.push_back({"  none", DIM, 0, ""});
    for (auto& m : u.tableMaps) rows.push_back({"  " + m.first + (m.second > 0 ? fmt("   rate %d", m.second) : "   rate 0 - never rolls!"), m.second > 0 ? TEXT : DANGER, 2, m.first});
    head("Dungeons with table=" + id + ":");
    if (u.dungeons.empty()) rows.push_back({"  none", DIM, 0, ""});
    for (auto& d : u.dungeons) rows.push_back({"  " + d, TEXT, 3, d});
    if (encounterUnused(id)) { rows.push_back({"", DIM, 0, ""}); rows.push_back({"UNUSED: nothing ever picks this table. Give a tile kind biome=" + u.base + " (Tiles tab) or set table=" + id + " on a map with an encounter rate.", DANGER, 0, ""}); }
    rows.push_back({"", DIM, 0, ""}); rows.push_back({"Level: monsters get +1 level per 3 hero levels above it.", DIM, 0, ""});
    ui.cv->setClip({x + 1, y + 18, w - 2, h - 20});
    for (auto& r : rows) {
        auto wl = r.kind ? std::vector<std::string>{r.text} : font->wrap(r.text, w - 16);
        for (auto& l : wl) {
            bool hv = r.kind && ui.hover(x + 2, cy, w - 4, lineH);
            if (hv) ui.cv->fillRect(x + 2, cy - 1, w - 4, lineH, HOVER);
            font->draw(*ui.cv, *ts, x + 8, cy, l, hv ? C::White : r.col);
            if (hv) { ui.tip("Click to open it"); if (in.mousePressed[0] && !ui.mouseUsed) { ui.mouseUsed = true; std::string t = r.target; ui.cv->resetClip();
                if (r.kind == 1) jumpToKind(t);
                else if (r.kind == 2 && t.find(' ') == std::string::npos) { FlagUse fu; fu.tab = 0; fu.file = t; jumpToUse(fu); }
                else if (r.kind == 3) { for (size_t i = 0; i < gens[7].secs.size(); i++) if (gens[7].secs[i].get("id") == t) { FlagUse fu; fu.tab = 10; fu.gen = 7; fu.sec = (int)i; jumpToUse(fu); } }
                return; } }
            cy += r.text.empty() ? 6 : lineH;
        }
    }
    ui.cv->resetClip();
}
