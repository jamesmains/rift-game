// Editor extras: undo/redo for the form tabs, "Used by" for items / creatures / spells / shops / dialogues,
// renaming a flag everywhere, and the whole-campaign check.
#include "editor.h"
#include <filesystem>
#include <algorithm>

namespace fs = std::filesystem;
using namespace ED;

std::vector<std::string> condFlags(const std::string& cond);   // editor_flags.cpp

// ---------------------------------------------------------------- undo / redo (form tabs)
static bool sameSecs(const std::vector<EdSection>& a, const std::vector<EdSection>& b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); i++) if (a[i].name != b[i].name || a[i].kv != b[i].kv) return false;
    return true;
}
// called at the top of drawGeneric: anything that changed since last frame becomes an undo step (typing in one field
// within a second of the previous change is merged into the same step), then Ctrl+Z / Ctrl+Y are handled
void Editor::genUndo(GenericEditor& g, InputState& in) {
    if (!g.shadowInit) { g.shadow = g.secs; g.shadowInit = true; }
    if (!sameSecs(g.secs, g.shadow)) {
        if (ui.time - g.lastChangeT > 1.0f || g.undo.empty()) { g.undo.push_back(g.shadow); if (g.undo.size() > 80) g.undo.erase(g.undo.begin()); }
        g.redo.clear(); g.lastChangeT = ui.time; g.shadow = g.secs;
    }
    if (pop == P_NONE && in.ctrl() && in.pressed[K_Z]) { genStep(g, !in.shift()); in.pressed[K_Z] = false; }
    if (pop == P_NONE && in.ctrl() && in.pressed[K_Y]) { genStep(g, false); in.pressed[K_Y] = false; }
}
void Editor::genStep(GenericEditor& g, bool undo) {
    auto& from = undo ? g.undo : g.redo; auto& to = undo ? g.redo : g.undo;
    if (from.empty()) { say(undo ? "Nothing to undo." : "Nothing to redo."); return; }
    to.push_back(g.secs); g.secs = from.back(); from.pop_back(); g.shadow = g.secs; g.lastChangeT = -10;
    g.sel = std::min(g.sel, (int)g.secs.size() - 1); g.dirty = true; ui.blur();
    say(undo ? "Undo." : "Redo.");
}

// ---------------------------------------------------------------- where ids are used
void Editor::buildUseIndex() {
    useIndex.clear();
    auto add = [&](const std::string& kind, const std::string& id, const std::string& where, const std::string& detail, FlagUse u) { std::string i = trim(id); if (i.empty()) return; u.where = where; u.detail = detail; useIndex[kind + ":" + i].push_back(u); };
    auto idList = [](const std::string& v) { std::vector<std::string> o; for (auto& p : split(v, ',')) { std::string t = trim(split(trim(p), ':')[0]); if (!t.empty()) o.push_back(t); } return o; };
    auto condRefs = [&](const std::string& cond, const std::string& where, const std::string& detail, FlagUse u) {
        for (auto& part : split(cond, '&')) { std::string c = trim(part); if (!c.empty() && c[0] == '!') c = c.substr(1);
            if (startsWith(c, "item:")) add("item", c.substr(5), where, detail, u); else if (startsWith(c, "itemn:")) add("item", split(c.substr(6), ':')[0], where, detail, u);
            else if (startsWith(c, "kills:")) add("creature", split(c.substr(6), ':')[0], where, detail, u);
            else if (startsWith(c, "rep:")) add("faction", split(c.substr(4), ':')[0], where, detail + " (" + c + ")", u); }
    };
    auto scriptRefs = [&](const std::string& script, const std::string& where, FlagUse u) {
        for (auto& a : split(script, ';')) { std::string t = trim(a); size_t p = t.find(':'); if (p == std::string::npos) continue; std::string k = trim(t.substr(0, p)), v = trim(t.substr(p + 1));
            if (k == "dialogue") add("dialogue", v, where, "script " + t, u); else if (k == "combat") for (auto& c : idList(v)) add("creature", c, where, "script " + t, u);
            else if (k == "item" || k == "give" || k == "take") for (auto& c : idList(v)) add("item", c, where, "script " + t, u); else if (k == "shop") add("shop", v, where, "script " + t, u); }
    };
    // dialogue
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
            FlagUse u; u.tab = 11; u.file = f; u.line = (int)i; std::string where = "dialogue " + f + " [" + node + "] line " + std::to_string(i + 1);
            if (k == "give" || k == "take" || k == "item") for (auto& c : idList(v)) add("item", c, where, k + "=" + v, u);
            else if (k == "combat") for (auto& c : idList(v)) add("creature", c, where, k + "=" + v, u);
            else if (k == "spawn") add("creature", split(v, ',')[0], where, k + "=" + v, u);
            else if (k == "shop") add("shop", v, where, k + "=" + v, u);
            else if (k == "dialogue") add("dialogue", v, where, k + "=" + v, u);
            else if (k == "board") add("board", v, where, "board=" + v + " opens the notice board", u);
            else if (k == "rep") for (auto& part : split(v, ',')) { auto p = split(trim(part), ':'); add("faction", p[0], where, "rep=" + trim(part), u); }
            else if (k == "option") { auto p = split(v, '|'); if (p.size() > 2) condRefs(p[2], where, "option '" + trim(p[0]) + "' condition", u); }
        }
    }
    // maps
    for (auto& ms : mapScan) for (size_t oi = 0; oi < ms.objs.size(); oi++) {
        auto& o = ms.objs[oi]; FlagUse u; u.tab = 0; u.file = ms.id; u.obj = (int)oi;
        std::string where = "map " + ms.id + ": " + o.type + (o.get("id").empty() ? "" : " " + o.get("id")) + " at " + o.get("x") + "," + o.get("y");
        if (o.type == "chest") { for (auto& c : idList(o.get("items"))) add("item", c, where, "in the chest", u); add("item", o.get("locked"), where, "key that opens it", u); }
        if (o.type == "gather") { add("item", o.get("item"), where, "gathered here", u); add("item", o.get("tool"), where, "tool needed", u); }
        if (o.type == "door") add("item", o.get("key"), where, "key", u);
        if (o.type == "exit") add("item", o.get("item"), where, "needed to pass", u);
        if (o.type == "enemy") for (auto& c : idList(o.get("group"))) add("creature", c, where, "in the group", u);
        if (o.type == "npc") { add("board", o.get("board"), where, "is the notice board", u); add("dialogue", o.get("dialogue"), where, "talks with it", u); add("shop", o.get("shop"), where, "sells from it", u); for (auto& c : idList(o.get("hostile"))) add("creature", c, where, "turns into it when attacked", u); }
        for (auto& n : split(o.get("needs"), '|')) if (startsWith(trim(n), "item:")) add("item", trim(n).substr(5), where, "clears the obstacle", u);
        for (const char* ck : {"show", "hide", "moveif", "openif", "require", "if"}) condRefs(o.get(ck), where, std::string(ck) + "= condition", u);
        for (const char* sk : {"script", "ondefeat", "onclear", "onuse"}) scriptRefs(o.get(sk), where, u);
    }
    auto gu = [](int gen, int sec, int tab) { FlagUse u; u.gen = gen; u.sec = sec; u.tab = tab; return u; };
    // data files
    for (size_t i = 0; i < gens[2].secs.size(); i++) { std::string sm = gens[2].secs[i].get("summon"); if (!sm.empty() && sm != "0" && sm != "1") add("creature", split(sm, ':')[0], "spell " + gens[2].secs[i].get("id"), "summoned by it", gu(2, (int)i, 5)); }
    for (size_t i = 0; i < gens[4].secs.size(); i++) add("faction", gens[4].secs[i].get("faction"), "shop " + gens[4].secs[i].get("id"), "prices follow this reputation", gu(4, (int)i, 7));
    for (size_t i = 0; i < gens[13].secs.size(); i++) if (gens[13].secs[i].name == "board") add("faction", gens[13].secs[i].get("id"), "board " + gens[13].secs[i].get("id"), "paid bounties add rep=" + gens[13].secs[i].get("rep", "3"), gu(13, (int)i, 20));
    for (size_t i = 0; i < gens[12].secs.size(); i++) for (auto& part : split(gens[12].secs[i].get("cond"), '&')) { std::string c = trim(part); if (startsWith(c, "rep:")) add("faction", trim(split(c.substr(4), '>')[0]), "achievement " + gens[12].secs[i].get("id"), c, gu(12, (int)i, 17)); }
    for (size_t i = 0; i < gens[6].secs.size(); i++) for (auto& kv : gens[6].secs[i].kv) if (kv.first == "stage" || kv.first == "fail" || kv.first == "objective") for (auto& part : split(kv.second, '&')) { std::string c = trim(part); if (!c.empty() && c[0] == '!') c = c.substr(1); if (startsWith(c, "rep:")) add("faction", split(c.substr(4), ':')[0], "quest " + gens[6].secs[i].get("id"), kv.first + " " + trim(split(c, '|')[0]), gu(6, (int)i, 9)); }
    for (size_t i = 0; i < gens[4].secs.size(); i++) { auto& s = gens[4].secs[i]; for (auto& kv : s.kv) if (kv.first == "item") add("item", split(kv.second, ':')[0], "shop " + s.get("id"), "sells it", gu(4, (int)i, 7)); }
    for (size_t i = 0; i < gens[3].secs.size(); i++) { auto& s = gens[3].secs[i]; std::string w = "class " + s.get("id");
        add("item", s.get("weapon"), w, "starting weapon", gu(3, (int)i, 6)); add("item", s.get("armor"), w, "starting armour", gu(3, (int)i, 6));
        for (auto& c : idList(s.get("items"))) add("item", c, w, "starting item", gu(3, (int)i, 6)); for (auto& c : idList(s.get("spells"))) add("spell", c, w, "starting spell", gu(3, (int)i, 6));
        for (auto& p : split(s.get("learn"), ',')) { auto q = split(trim(p), ':'); if (q.size() == 2) add("spell", q[1], w, "learned at rank " + trim(q[0]), gu(3, (int)i, 6)); } }
    for (size_t i = 0; i < gens[1].secs.size(); i++) { auto& s = gens[1].secs[i]; std::string w = "creature " + s.get("id");
        for (auto& c : idList(s.get("loot"))) add("item", c, w, "drops it", gu(1, (int)i, 4)); for (auto& c : idList(s.get("spells"))) add("spell", c, w, "casts it", gu(1, (int)i, 4)); }
    for (size_t i = 0; i < gens[0].secs.size(); i++) { auto& s = gens[0].secs[i]; if (!s.get("spell").empty()) add("spell", s.get("spell"), "item " + s.get("id"), "using the item casts it", gu(0, (int)i, 3)); if (!s.get("ammo").empty()) add("item", s.get("ammo"), "item " + s.get("id"), "ammunition for it", gu(0, (int)i, 3));
        for (auto& c : idList(s.get("salvage"))) add("item", c, "item " + s.get("id"), "salvaging it gives this", gu(0, (int)i, 3)); }
    for (size_t i = 0; i < gens[9].secs.size(); i++) { auto& s = gens[9].secs[i]; std::string w = "recipe " + s.get("id");
        add("item", s.get("result"), w, "crafted by it", gu(9, (int)i, 14)); for (auto& c : idList(s.get("materials"))) add("item", c, w, "material", gu(9, (int)i, 14)); add("item", s.get("reveal"), w, "reveals the recipe", gu(9, (int)i, 14)); }
    for (size_t i = 0; i < gens[5].secs.size(); i++) { auto& s = gens[5].secs[i]; for (auto& kv : s.kv) if (kv.first == "group") for (auto& c : idList(kv.second)) add("creature", c, "encounter table " + s.get("id"), "random fights: " + kv.second, gu(5, (int)i, 8)); }
    for (size_t i = 0; i < gens[7].secs.size(); i++) { auto& s = gens[7].secs[i]; std::string w = "dungeon " + s.get("id");
        for (auto& c : idList(s.get("monsters"))) add("creature", c, w, "monster pool", gu(7, (int)i, 10)); add("creature", s.get("boss"), w, "boss", gu(7, (int)i, 10)); scriptRefs(s.get("bossdefeat"), w, gu(7, (int)i, 10)); }
    for (size_t i = 0; i < gens[6].secs.size(); i++) { auto& s = gens[6].secs[i]; int n = 0; for (auto& kv : s.kv) if (kv.first == "stage" || kv.first == "objective" || kv.first == "fail") { n++; auto p = split(kv.second, '|'); condRefs(kv.first != "objective" ? p[0] : (p.size() > 1 ? p[1] : ""), "quest " + s.get("id"), kv.first + " condition", gu(6, (int)i, 9)); } }
    for (size_t i = 0; i < gens[12].secs.size(); i++) { auto& s = gens[12].secs[i]; for (auto& t : split(s.get("cond"), '&')) { std::string c = trim(t); if (startsWith(c, "item:")) add("item", c.substr(5), "achievement " + s.get("id"), "condition", gu(12, (int)i, 17)); if (startsWith(c, "boss:")) add("creature", c.substr(5), "achievement " + s.get("id"), "condition", gu(12, (int)i, 17)); if (startsWith(c, "flag:")) condRefs(c.substr(5), "achievement " + s.get("id"), "condition", gu(12, (int)i, 17)); } }
    for (size_t i = 0; i < gens[11].secs.size(); i++) condRefs(gens[11].secs[i].get("if"), "banter " + gens[11].secs[i].get("class"), "if= condition", gu(11, (int)i, 16));
    // notice boards
    for (size_t i = 0; i < gens[13].secs.size(); i++) { auto& s = gens[13].secs[i]; FlagUse u = gu(13, (int)i, 20); std::string w = (s.name == "board" ? "board " : "notice ") + s.get("id");
        if (s.name == "board") { for (auto& c : idList(s.get("creatures"))) add("creature", c, w, "bounty targets", u); for (auto& c : idList(s.get("fetch"))) add("item", c, w, "bounties ask for it", u); }
        else { std::string kd = s.get("kind", "kill"); if (kd == "kill") add("creature", s.get("target"), w, "bounty target", u); else if (kd == "fetch") add("item", s.get("target"), w, "bounty asks for it", u); condRefs(s.get("require"), w, "require= condition", u); } }
    // campaign start
    { FlagUse u; u.tab = 18; for (auto& c : idList(camp.items)) add("item", c, "campaign start", "starting item", u); add("dialogue", camp.dialogue, "campaign start", "opens after the intro", u); }
}

void Editor::drawUsedBy(const std::string& key, int x, int y, int w, int h, InputState& in) {
    ensureIndex();
    auto it = useIndex.find(key); static const std::vector<FlagUse> none; const auto& uses = it == useIndex.end() ? none : it->second;
    std::string id = key.substr(key.find(':') + 1);
    ui.cv->fillRect(x, y, w, h, FIELD); ui.cv->rect(x, y, w, h, BORDER);
    ui.cv->fillRect(x + 1, y + 1, w - 2, 16, PANEL2);
    font->draw(*ui.cv, *ts, x + 6, y + 3, fmt("Used by (%d)", (int)uses.size()), ACCENT2);
    { std::string hint = uses.empty() ? "nothing in the campaign uses '" + id + "' yet" : "click a line to open it"; if (x + 100 + font->width(hint) < x + w - 70) font->draw(*ui.cv, *ts, x + 100, y + 3, hint, DIM); else if (uses.empty()) font->draw(*ui.cv, *ts, x + 8, y + 22, "nothing uses it yet", DIM); }
    if (ui.button(x + w - 64, y + 1, 60, 15, "Refresh")) buildFlagIndex();
    int rowH = 14, rows = (h - 20) / rowH;
    if (ui.hover(x, y, w, h) && in.wheel) useScroll -= in.wheel * 2;
    useScroll = clampi(useScroll, 0, std::max(0, (int)uses.size() - rows));
    ui.cv->setClip({x + 1, y + 18, w - 2, h - 19});
    for (int r = 0; r < rows && useScroll + r < (int)uses.size(); r++) {
        const FlagUse& u = uses[useScroll + r]; int ry = y + 19 + r * rowH;
        bool hv = ui.hover(x + 2, ry, w - 4, rowH);
        if (hv) ui.cv->fillRect(x + 2, ry, w - 4, rowH, HOVER);
        font->draw(*ui.cv, *ts, x + 8, ry + 1, u.where, TEXT); font->draw(*ui.cv, *ts, x + 16 + font->width(u.where), ry + 1, u.detail, DIM);
        if (hv && in.mousePressed[0] && !ui.mouseUsed) { ui.mouseUsed = true; FlagUse cp = u; ui.cv->resetClip(); jumpToUse(cp); return; }
    }
    ui.cv->resetClip();
}

// ---------------------------------------------------------------- rename a flag everywhere
// whole-word replace of a flag inside a condition or script; "item:x" / "kills:x" style ids are left alone
static std::string renameWord(const std::string& s, const std::string& from, const std::string& to) {
    std::string o; size_t i = 0;
    auto isw = [](char c) { return isalnum((unsigned char)c) || c == '_'; };
    while (i < s.size()) {
        if (!isw(s[i])) { o += s[i++]; continue; }
        size_t j = i; while (j < s.size() && isw(s[j])) j++;
        std::string w = s.substr(i, j - i);
        bool ok = w == from;
        if (ok && i > 0 && s[i - 1] == ':') { size_t k = i - 1; size_t b = k; while (b > 0 && isw(s[b - 1])) b--; std::string pre = s.substr(b, k - b); ok = pre == "flag" || pre == "set" || pre == "inc" || pre == "clear"; }
        o += ok ? to : w; i = j;
    }
    return o;
}
void Editor::renameFlag(const std::string& from, const std::string& to) {
    int n = 0; std::string text;
    // dialogue files
    for (auto& f : dlgFiles) {
        if (!readFile(readPath("dialogue/" + f + ".txt"), text)) continue;
        auto lines = split(text, '\n'); bool ch = false;
        for (auto& l : lines) { std::string t = trim(l); size_t eq = t.find('='); if (eq == std::string::npos || t[0] == '#') continue; std::string k = trim(t.substr(0, eq)), v = t.substr(eq + 1), nv = v;
            if (k == "set" || k == "flag" || k == "inc" || k == "clear") nv = renameWord(v, from, to);
            else if (k == "option") { auto p = split(v, '|'); if (p.size() > 2) { p[2] = renameWord(p[2], from, to); nv.clear(); for (size_t i = 0; i < p.size(); i++) nv += (i ? "|" : "") + p[i]; } }
            if (nv != v) { size_t lead = l.find_first_not_of(" \t"); l = l.substr(0, lead == std::string::npos ? 0 : lead) + k + "=" + nv; ch = true; n++; } }
        if (ch) { std::string o; for (size_t i = 0; i < lines.size(); i++) o += lines[i] + (i + 1 < lines.size() ? "\n" : ""); writeFile(dataRoot + "/dialogue/" + f + ".txt", o); }
    }
    // maps
    for (auto& f : mapFiles) {
        EdMap m; if (!m.load(readPath("maps/" + f + ".txt"))) continue; bool ch = false;
        for (auto& o : m.objs) for (auto& kv : o.kv) { if (kv.first == "flag" || kv.first == "show" || kv.first == "hide" || kv.first == "moveif" || kv.first == "openif" || kv.first == "require" || kv.first == "if" || kv.first == "needs" || kv.first == "script" || kv.first == "ondefeat" || kv.first == "onclear" || kv.first == "onuse") {
            std::string nv = kv.first == "flag" ? (kv.second == from ? to : kv.second) : renameWord(kv.second, from, to); if (nv != kv.second) { kv.second = nv; ch = true; n++; } } }
        if (ch) { m.path = dataRoot + "/maps/" + f + ".txt"; m.save(); }
    }
    // data files
    auto edit = [&](int gi, std::function<bool(EdSection&)> fn) { bool ch = false; for (auto& s : gens[gi].secs) if (fn(s)) ch = true; if (ch) gens[gi].save(); };
    auto rk = [&](EdSection& s, const char* key, bool cond) { bool ch = false; for (auto& kv : s.kv) if (kv.first == key) { std::string nv = cond ? renameWord(kv.second, from, to) : (kv.second == from ? to : kv.second); if (nv != kv.second) { kv.second = nv; ch = true; n++; } } return ch; };
    edit(6, [&](EdSection& s) { bool ch = false; for (auto& kv : s.kv) if (kv.first == "stage" || kv.first == "objective" || kv.first == "fail") { auto p = split(kv.second, '|'); size_t ci = kv.first == "objective" ? 1 : 0; if (p.size() > ci) { std::string nc = renameWord(p[ci], from, to); if (nc != p[ci]) { p[ci] = nc; std::string nv; for (size_t i = 0; i < p.size(); i++) nv += (i ? "|" : "") + p[i]; kv.second = nv; ch = true; n++; } } } return ch; });
    edit(7, [&](EdSection& s) { bool a = rk(s, "bossflag", false); bool b = rk(s, "bossdefeat", true); return a || b; });
    edit(8, [&](EdSection& s) { return rk(s, "require", true); });
    edit(12, [&](EdSection& s) { return rk(s, "cond", true); });
    edit(11, [&](EdSection& s) { return rk(s, "if", true); });
    { std::string nf = renameWord(camp.flags, from, to); if (nf != camp.flags) { camp.flags = nf; camp.dirty = true; saveCampaignForm(); n++; } }
    if (flagDocs.count(from)) { if (!flagDocs.count(to)) flagDocs[to] = flagDocs[from]; flagDocs.erase(from); saveFlagDocs(); }
    // reload what is open
    if (mapLoaded) { std::string id = em.id; loadMapFile(id); }
    if (!te.path.empty()) { std::string p = te.path; std::string stem = fs::path(p).stem().string(); te.load(readPath("dialogue/" + stem + ".txt")); te.path = p; }
    changed = true; flagIndexDirty = true; flagSelName = to;
    say(fmt("Renamed flag '%s' to '%s' in %d place(s) - all files saved.", from.c_str(), to.c_str(), n));
}

// ---------------------------------------------------------------- whole-campaign check
void Editor::runCampaignCheck() {
    report.clear(); reportScroll = 0;
    buildFlagIndex();
    auto addF = [&](bool err, const std::string& t, FlagUse u) { Finding f; f.err = err; f.text = t; f.jump = u; report.push_back(f); };
    auto gu = [](int gen, int sec, int tab) { FlagUse u; u.gen = gen; u.sec = sec; u.tab = tab; return u; };
    auto isMap = [&](const std::string& m) { if (m == "overworld" || std::find(mapFiles.begin(), mapFiles.end(), m) != mapFiles.end()) return true; for (auto& d : gens[7].secs) if (d.get("id") == m) return true; return false; };
    auto item = [&](const std::string& id) { return id.empty() || gd->item(id) != nullptr; };
    auto crea = [&](const std::string& id) { return id.empty() || gd->creature(id) != nullptr; };
    auto ids = [](const std::string& v) { std::vector<std::string> o; for (auto& p : split(v, ',')) { std::string t = trim(split(trim(p), ':')[0]); if (!t.empty()) o.push_back(t); } return o; };
    // ---- campaign ----
    { FlagUse u; u.tab = 18;
      if (camp.startMap.empty() || !isMap(camp.startMap)) addF(true, "Campaign: the start map '" + camp.startMap + "' does not exist", u);
      if (!camp.dialogue.empty() && std::find(dlgFiles.begin(), dlgFiles.end(), camp.dialogue) == dlgFiles.end()) addF(true, "Campaign: the opening dialogue '" + camp.dialogue + "' does not exist", u);
      std::string wm = toLower(camp.worldMap); if (!wm.empty() && wm != "overworld" && wm != "off" && wm != "current" && !isMap(camp.worldMap)) addF(true, "Campaign: worldmap= names a map that does not exist ('" + camp.worldMap + "')", u);
      for (auto& c : ids(camp.items)) if (!item(c)) addF(true, "Campaign: starting item '" + c + "' does not exist", u); }
    // ---- NPCs (1.1.9) ----
    for (size_t i = 0; i < gens[17].secs.size(); i++) { auto& sc = gens[17].secs[i]; if (sc.name != "npc") continue;
        std::vector<std::pair<bool, std::string>> pr; npcProblems(sc, pr);
        for (auto& p : pr) { FlagUse u; u.gen = 17; u.sec = (int)i; u.tab = 24; addF(p.first, "NPC " + sc.get("id") + ": " + p.second, u); } }
    // ---- maps ----
    for (auto& f : mapFiles) {
        EdMap m; if (!m.load(readPath("maps/" + f + ".txt"))) continue;
        auto walk = [&](int cx, int cy) { if (cx < 0 || cy < 0 || cx >= m.w || cy >= m.h) return false; const TileKind* k = tileKind(trim(split(m.at(cx, cy), '|')[0])); return k && k->walkable; };
        // 1.1.7: a trigger or exit area is fine while any of its cells can be walked on (a whole-map trigger starts on a tree)
        auto areaWalk = [&](const EdObj& ob, int x0, int y0) { int w = std::max(1, ob.geti("w")), h = std::max(1, ob.geti("h")); for (int yy = y0; yy < y0 + h; yy++) for (int xx = x0; xx < x0 + w; xx++) if (walk(xx, yy)) return true; return false; };
        for (size_t oi = 0; oi < m.objs.size(); oi++) { const EdObj& raw = m.objs[oi]; EdObj o = resolvedObj(raw); FlagUse u; u.tab = 0; u.file = f; u.obj = (int)oi; int ox = o.geti("x"), oy = o.geti("y");
            std::string at = fmt("map %s: %s at %d,%d", f.c_str(), o.type.c_str(), ox, oy);
            if (!raw.get("template").empty() && !templateSec(raw.get("template"))) addF(true, at + " uses unknown template '" + raw.get("template") + "' (Objects tab)", u);
            if (o.type == "gather" && !o.get("node").empty() && !resourceSec(o.get("node"), "")) addF(true, at + " uses unknown resource node type '" + o.get("node") + "' (Resources tab)", u);
            if (o.type == "npc" && !trim(o.get("npc")).empty() && !npcSec(o.get("npc"))) addF(true, at + " shows unknown NPC '" + o.get("npc") + "' (NPCs tab)", u);
            if (o.type == "npc" && trim(o.get("npc")).empty()) addF(false, at + " is not linked to an NPC (select it in the Maps tab > make NPC)", u);
            if (ox < 0 || oy < 0 || ox >= m.w || oy >= m.h) addF(true, at + " is outside the map", u);
            else if (o.type != "door" && o.type != "sign" && o.type != "lever" && o.type != "obstacle" && o.type != "change" && o.type != "gather" && !((o.type == "trigger" || o.type == "exit") ? areaWalk(o, ox, oy) : walk(ox, oy))) addF(false, at + ((o.type == "trigger" || o.type == "exit") && (o.geti("w") > 1 || o.geti("h") > 1) ? " covers only blocked tiles" : " stands on a blocked tile"), u);
            if (o.type == "npc" && !o.get("dialogue").empty() && std::find(dlgFiles.begin(), dlgFiles.end(), o.get("dialogue")) == dlgFiles.end()) addF(true, at + " uses missing dialogue '" + o.get("dialogue") + "'", u);
            if (o.type == "npc" && !o.get("shop").empty()) { bool ok = false; for (auto& s : gens[4].secs) if (s.get("id") == o.get("shop")) ok = true; if (!ok) addF(true, at + " uses unknown shop '" + o.get("shop") + "'", u); }
            if (o.type == "exit" && !isMap(o.get("to"))) addF(true, at + " leads to unknown map '" + o.get("to") + "'", u);
            if (o.type == "enemy") for (auto& c : ids(o.get("group"))) if (!crea(c)) addF(true, at + " has unknown creature '" + c + "'", u);
            if (o.type == "chest") for (auto& c : ids(o.get("items"))) if (!item(c)) addF(true, at + " holds unknown item '" + c + "'", u);
            if (o.type == "gather" && (o.get("item").empty() || !item(o.get("item")))) addF(true, at + " gathers unknown item '" + o.get("item") + "'", u);
            if (o.type == "gather" && !item(o.get("tool"))) addF(true, at + " needs unknown tool '" + o.get("tool") + "'", u);
            if (o.type == "door" && !item(o.get("key"))) addF(true, at + " needs unknown key '" + o.get("key") + "'", u);
        }
        FlagUse u; u.tab = 0; u.file = f;
        auto sp = split(m.hget("spawn"), ','); if (sp.size() == 2 && !walk(atoi(sp[0].c_str()), atoi(sp[1].c_str()))) addF(false, "map " + f + ": the spawn point is on a blocked tile", u);
        for (auto& c : m.cells) if (!tileKind(trim(split(c, '|')[0]))) { addF(true, "map " + f + ": unknown tile kind '" + c + "'", u); break; }
        if (!m.hget("table").empty()) { bool ok = false; for (auto& e : gens[5].secs) if (e.get("id") == m.hget("table")) ok = true; if (!ok) addF(true, "map " + f + ": table= names a missing encounter table '" + m.hget("table") + "'", u); }
        std::string wm = toLower(m.hget("worldmap")); if (!wm.empty() && wm != "self" && wm != "current" && wm != "off" && wm != "overworld" && !isMap(m.hget("worldmap"))) addF(true, "map " + f + ": worldmap= names a missing map", u);
    }
    // ---- dialogue ----
    std::string txt;
    for (auto& f : dlgFiles) if (readFile(readPath("dialogue/" + f + ".txt"), txt)) for (auto& it : lintDialogue(txt, f)) { FlagUse u; u.tab = 11; u.file = f; u.line = std::max(0, it.line); addF(!it.warn, "dialogue " + f + (it.line >= 0 ? fmt(" line %d: ", it.line + 1) : ": ") + it.msg, u); }
    // ---- flags ----
    for (auto& kv : flagIndex) { int s = 0, c = 0; for (auto& u : kv.second) (u.set ? s : c)++; if (s == 0 && c > 0) { FlagUse u; u.tab = 19; u.file = kv.first; addF(true, "flag '" + kv.first + "' is checked but nothing ever sets it", u); } }
    // ---- data files ----
    for (size_t i = 0; i < gens[15].secs.size(); i++) { auto& s = gens[15].secs[i]; auto& ty = edObjTypes(); std::string tt = s.name == "type" ? s.get("id") : s.get("type");
        if (std::find(ty.begin(), ty.end(), tt) == ty.end()) addF(true, (s.name == "type" ? "object type entry '" : "object template " + s.get("id") + ": type '") + tt + "' is not an object type", gu(15, (int)i, 22));
        for (const char* tk : {"sprite", "opensprite", "onsprite"}) if (!trim(s.get(tk)).empty() && !parseTile(trim(s.get(tk))).valid()) addF(true, (s.name == "type" ? "object type " : "object template ") + s.get("id") + ": " + tk + "=" + s.get(tk) + " is not a tile (col,row)", gu(15, (int)i, 22)); }
    for (size_t i = 0; i < gens[16].secs.size(); i++) { auto& s = gens[16].secs[i]; std::string w = "resource node " + s.get("id");
        if (s.get("item").empty() || !item(s.get("item"))) addF(true, w + ": unknown item '" + s.get("item") + "'", gu(16, (int)i, 23));
        if (!item(s.get("tool"))) addF(true, w + ": unknown tool '" + s.get("tool") + "'", gu(16, (int)i, 23));
        if (!s.get("kind").empty() && !tileKind(s.get("kind"))) addF(true, w + ": unknown tile kind '" + s.get("kind") + "'", gu(16, (int)i, 23)); }
    for (size_t i = 0; i < gens[4].secs.size(); i++) for (auto& kv : gens[4].secs[i].kv) if (kv.first == "item" && !item(split(kv.second, ':')[0])) addF(true, "shop " + gens[4].secs[i].get("id") + " sells unknown item '" + kv.second + "'", gu(4, (int)i, 7));
    for (size_t i = 0; i < gens[3].secs.size(); i++) { auto& s = gens[3].secs[i]; for (auto& c : {s.get("weapon"), s.get("armor")}) if (!item(c)) addF(true, "class " + s.get("id") + ": unknown item '" + c + "'", gu(3, (int)i, 6)); for (auto& c : ids(s.get("items"))) if (!item(c)) addF(true, "class " + s.get("id") + ": unknown item '" + c + "'", gu(3, (int)i, 6)); for (auto& c : ids(s.get("spells"))) if (!gd->spell(c)) addF(true, "class " + s.get("id") + ": unknown spell '" + c + "'", gu(3, (int)i, 6)); }
    for (size_t i = 0; i < gens[2].secs.size(); i++) { auto& s = gens[2].secs[i]; std::string sm = s.get("summon"); if (sm.empty() || sm == "0") continue; if (sm == "1") addF(false, "spell " + s.get("id") + ": summon=1 is the old placeholder; name a creature (summon=wolf or wolf:2)", gu(2, (int)i, 5)); else { if (!gd->creature(trim(split(sm, ':')[0]))) addF(true, "spell " + s.get("id") + " summons unknown creature '" + sm + "'", gu(2, (int)i, 5)); if (s.get("target") != "ground") addF(false, "spell " + s.get("id") + " summons: target=ground lets the caster pick the tile", gu(2, (int)i, 5)); } }
    for (size_t i = 0; i < gens[4].secs.size(); i++) { auto& s = gens[4].secs[i]; std::string f = s.get("faction"); if (!f.empty() && !knownFaction(f)) addF(false, "shop " + s.get("id") + ": faction '" + f + "' is not in the Factions tab or a board id", gu(4, (int)i, 7)); }
    for (size_t i = 0; i < gens[1].secs.size(); i++) { auto& s = gens[1].secs[i]; for (auto& c : ids(s.get("loot"))) if (!item(c)) addF(true, "creature " + s.get("id") + " drops unknown item '" + c + "'", gu(1, (int)i, 4)); for (auto& c : ids(s.get("spells"))) if (!gd->spell(c)) addF(true, "creature " + s.get("id") + " casts unknown spell '" + c + "'", gu(1, (int)i, 4)); }
    for (size_t i = 0; i < gens[9].secs.size(); i++) { auto& s = gens[9].secs[i]; if (!item(s.get("result"))) addF(true, "recipe " + s.get("id") + " makes unknown item '" + s.get("result") + "'", gu(9, (int)i, 14)); for (auto& c : ids(s.get("materials"))) if (!item(c)) addF(true, "recipe " + s.get("id") + " needs unknown item '" + c + "'", gu(9, (int)i, 14)); }
    for (size_t i = 0; i < gens[5].secs.size(); i++) { auto& s = gens[5].secs[i]; for (auto& kv : s.kv) if (kv.first == "group") for (auto& c : ids(kv.second)) if (!crea(c)) addF(true, "encounter table " + s.get("id") + ": unknown creature '" + c + "'", gu(5, (int)i, 8)); if (encounterUnused(s.get("id"))) addF(false, "encounter table " + s.get("id") + " is never used", gu(5, (int)i, 8)); }
    for (size_t i = 0; i < gens[7].secs.size(); i++) { auto& s = gens[7].secs[i]; for (auto& c : ids(s.get("monsters"))) if (!crea(c)) addF(true, "dungeon " + s.get("id") + ": unknown creature '" + c + "'", gu(7, (int)i, 10)); if (!crea(s.get("boss"))) addF(true, "dungeon " + s.get("id") + ": unknown boss '" + s.get("boss") + "'", gu(7, (int)i, 10)); }
    for (size_t i = 0; i < gens[8].secs.size(); i++) { auto& s = gens[8].secs[i]; std::string mp = s.get("map", s.get("id")); if (!isMap(mp)) addF(true, "location " + s.get("id") + " enters unknown map '" + mp + "'", gu(8, (int)i, 2)); }
    for (size_t i = 0; i < gens[6].secs.size(); i++) { auto& s = gens[6].secs[i]; int stages = 0; for (auto& kv : s.kv) if (kv.first == "stage") { stages++; std::string t = split(kv.second, '|').size() > 1 ? split(kv.second, '|')[1] : ""; size_t at = t.rfind(" @"); if (at != std::string::npos) { std::string pin = trim(t.substr(at + 2)); bool ok = isMap(pin); for (auto& l : gens[8].secs) if (l.get("id") == pin) ok = true; if (!ok) addF(false, "quest " + s.get("id") + fmt(" stage %d pins unknown place '", stages) + pin + "'", gu(6, (int)i, 9)); } }
        if (!stages) addF(false, "quest " + s.get("id") + " has no stages", gu(6, (int)i, 9));
        for (auto& kv : s.kv) if (kv.first == "objective") { int st = atoi(trim(split(kv.second, '|')[0]).c_str()); if (st < 0 || st > stages) addF(true, "quest " + s.get("id") + fmt(": an objective belongs to stage %d, which does not exist", st), gu(6, (int)i, 9)); } }
    for (size_t i = 0; i < gens[0].secs.size(); i++) { auto& s = gens[0].secs[i]; std::string e = s.get("effect"); if (startsWith(e, "revealmap:")) { std::string m = trim(split(e.substr(10), ',')[0]); if (!m.empty() && m != "here" && !isMap(m)) addF(true, "item " + s.get("id") + " reveals unknown map '" + m + "'", gu(0, (int)i, 3)); } }
    for (size_t i = 0; i < gens[13].secs.size(); i++) { std::vector<std::pair<bool, std::string>> pr; boardProblems(gens[13].secs[i], pr); for (auto& p : pr) addF(p.first, (gens[13].secs[i].name == "board" ? "board " : "notice ") + gens[13].secs[i].get("id") + ": " + p.second, gu(13, (int)i, 20)); }
    std::stable_sort(report.begin(), report.end(), [](const Finding& a, const Finding& b) { return a.err > b.err; });
}

void Editor::drawReport(InputState& in, bool& close) {
    Canvas& cv = *ui.cv;
    int x = 30, y = 24, w = W - 60, h = H - 48;
    int errs = 0; for (auto& f : report) if (f.err) errs++;
    ui.panel(x, y, w, h); ui.header(x, y, w, fmt("Campaign check - %d error(s), %d warning(s)", errs, (int)report.size() - errs));
    helpBtn(x + w - 16, y + 1, "editor.check", "start");
    std::vector<const Finding*> shown; for (auto& f : report) if (!reportErrOnly || f.err) shown.push_back(&f);
    int lx = x + 6, ly = y + 22, lw = w - 12, lh = h - 56, rowH = 16, rows = lh / rowH;
    cv.fillRect(lx, ly, lw, lh, FIELD); cv.rect(lx, ly, lw, lh, BORDER);
    if (ui.hover(lx, ly, lw, lh) && in.wheel) reportScroll -= in.wheel * 3;
    if (!ui.typing()) { if (in.pressed[K_PAGEDOWN]) reportScroll += rows; if (in.pressed[K_PAGEUP]) reportScroll -= rows; }
    reportScroll = clampi(reportScroll, 0, std::max(0, (int)shown.size() - rows));
    if (shown.empty()) ui.text(lx + 10, ly + 10, report.empty() ? "No problems found. Everything points at something that exists." : "No errors (warnings are hidden).", OK);
    cv.setClip({lx + 1, ly + 1, lw - 2, lh - 2});
    for (int r = 0; r < rows && reportScroll + r < (int)shown.size(); r++) {
        const Finding& f = *shown[reportScroll + r]; int ry = ly + 1 + r * rowH;
        bool hv = ui.hover(lx, ry, lw, rowH); if (hv) cv.fillRect(lx + 1, ry, lw - 2, rowH, HOVER);
        font->draw(cv, *ts, lx + 6, ry + 2, f.err ? "ERROR" : "warn", f.err ? DANGER : ACCENT2);
        font->draw(cv, *ts, lx + 52, ry + 2, f.text, TEXT);
        if (hv && in.mousePressed[0] && !ui.mouseUsed) { ui.mouseUsed = true; FlagUse u = f.jump; cv.resetClip(); close = true;
            if (u.tab == 19) { flagSelName = u.file; tab = 19; } else jumpToUse(u); return; }
    }
    cv.resetClip();
    int by = y + h - 28;
    ui.checkbox(lx, by + 4, "errors only", reportErrOnly);
    if (ui.button(lx + 120, by, 80, 20, "Run again")) runCampaignCheck();
    ui.text(lx + 210, by + 4, "click a line to open it", DIM);
    if (ui.button(x + w - 80, by, 70, 20, "Close") || in.pressed[K_ESCAPE]) close = true;
}

// ---------------------------------------------------------------- notice boards (Bounties tab)
std::vector<std::string> Editor::boardIds() {
    std::vector<std::string> o; for (auto& s : gens[13].secs) if (s.name == "board" && !s.get("id").empty()) o.push_back(s.get("id"));
    if (o.empty()) { ensureIndex(); for (auto& kv : useIndex) if (startsWith(kv.first, "board:")) o.push_back(kv.first.substr(6)); }
    return o;
}
void Editor::boardProblems(const EdSection& s, std::vector<std::pair<bool, std::string>>& out) {
    ensureIndex();
    auto boards = boardIds(); auto isBoard = [&](const std::string& b) { return std::find(boards.begin(), boards.end(), b) != boards.end(); };
    auto ids = [](const std::string& v) { std::vector<std::string> o; for (auto& p : split(v, ',')) if (!trim(p).empty()) o.push_back(trim(p)); return o; };
    auto isDungeon = [&](const std::string& d) { for (auto& x : gens[7].secs) if (x.get("id") == d) return true; return false; };
    auto isLoc = [&](const std::string& d) { for (auto& x : gens[8].secs) if (x.get("id") == d) return true; return false; };
    std::string id = s.get("id");
    if (s.name == "board") {
        auto it = useIndex.find("board:" + id);
        if (it == useIndex.end()) out.push_back({true, "Nothing opens this board: no NPC has board=" + id + " and no dialogue says board=" + id + ". Players can't read it, and letters sent here can never be delivered."});
        else { std::string map = s.get("map", id); bool onMap = false; for (auto& u : it->second) if (u.tab == 0 && u.file == map) onMap = true; bool anyMap = false; for (auto& u : it->second) if (u.tab == 0) anyMap = true;
            if (anyMap && !onMap) out.push_back({false, "map=" + map + " but the board NPC stands on another map - letters only go to boards whose map= the party has visited."}); }
        if (std::find(mapFiles.begin(), mapFiles.end(), s.get("map", id)) == mapFiles.end()) out.push_back({true, "map '" + s.get("map", id) + "' does not exist"});
        if (!s.get("location").empty() && !isLoc(s.get("location"))) out.push_back({false, "location '" + s.get("location") + "' is not an overworld location"});
        for (auto& k : ids(s.get("kinds"))) if (k != "kill" && k != "fetch" && k != "deliver") out.push_back({true, "kinds: unknown kind '" + k + "' (kill, fetch, deliver)"});
        for (auto& d : ids(s.get("kill"))) if (!isDungeon(d)) out.push_back({true, "kill: '" + d + "' is not a dungeon"});
        for (auto& c : ids(s.get("creatures"))) if (!gd->creature(c)) out.push_back({true, "creatures: unknown creature '" + c + "'"});
        for (auto& c : ids(s.get("fetch"))) if (!gd->item(c)) out.push_back({true, "fetch: unknown item '" + c + "'"});
        for (auto& b : ids(s.get("deliver"))) { if (b == id) out.push_back({true, "deliver: a board can't send letters to itself"}); else if (!isBoard(b)) out.push_back({true, "deliver: '" + b + "' is not a notice board - letters there could never be delivered"}); }
        bool del = !s.has("kinds") || s.get("kinds").find("deliver") != std::string::npos;
        if (del) { int n = 0; for (auto& b : boards) if (b != id && (ids(s.get("deliver")).empty() || std::find(ids(s.get("deliver")).begin(), ids(s.get("deliver")).end(), b) != ids(s.get("deliver")).end())) n++; if (!n) out.push_back({false, "posts courier jobs but there is no other board to send letters to"}); }
    } else {
        std::string kd = s.get("kind", "kill"), t = s.get("target");
        if (!isBoard(s.get("board"))) out.push_back({true, "board '" + s.get("board") + "' is not a notice board"});
        if (kd == "kill" && !gd->creature(t)) out.push_back({true, "target: unknown creature '" + t + "'"});
        if (kd == "fetch" && !gd->item(t)) out.push_back({true, "target: unknown item '" + t + "'"});
        if (kd == "deliver" && !isBoard(t)) out.push_back({true, "target: '" + t + "' is not a notice board - the letter could never be delivered"});
        if (kd == "deliver" && t == s.get("board")) out.push_back({true, "a letter to the board that posts it"});
        if (kd != "kill" && kd != "fetch" && kd != "deliver") out.push_back({true, "kind must be kill, fetch or deliver"});
        if (!s.get("where").empty() && !isDungeon(s.get("where")) && !isLoc(s.get("where"))) out.push_back({false, "where '" + s.get("where") + "' is not a dungeon or location"});
    }
}
bool Editor::knownFaction(const std::string& id) {
    for (auto& s : gens[14].secs) if (s.get("id") == id) return true;
    for (auto& b : boardIds()) if (b == id) return true;
    return false;
}
void Editor::drawFactionInfo(int x, int y, int w, int h, InputState& in) {
    GenericEditor& g = gens[14];
    ui.panel(x, y, w, h); ui.header(x, y, w, "Reputation"); helpBtn(x + w - 16, y + 1, "tab.factions", "start");
    std::vector<std::pair<std::string, Color8>> rows;
    rows.push_back({"Reputation runs from -100 to 100 per faction, starting at 0. Dialogue changes it with rep=FACTION:N; conditions test it with rep:FACTION:N (at least N).", DIM});
    rows.push_back({"Shops with faction=ID price by it (up to 15% off when Honoured, 25% more when Hated). Paying a bounty at a board adds that board's rep= (default 3) to the faction with the board's id.", DIM});
    rows.push_back({"Tiers: Hated -50, Disliked -10, Neutral, Liked 10, Friendly 30, Honoured 60.", DIM});
    std::string id = g.sel >= 0 && g.sel < (int)g.secs.size() ? g.secs[g.sel].get("id") : "";
    if (!id.empty()) { bool isBoard = false; for (auto& b : boardIds()) if (b == id) isBoard = true; rows.push_back({isBoard ? "A notice board has this id, so paid bounties there raise it." : "No notice board has this id.", isBoard ? OK : DIM}); }
    int cy = y + 22; ui.cv->setClip({x + 1, y + 18, w - 2, h - 160});
    for (auto& r : rows) for (auto& l : font->wrap(r.first, w - 16)) { font->draw(*ui.cv, *ts, x + 8, cy, l, r.second); cy += 13; }
    ui.cv->resetClip();
    if (!id.empty()) drawUsedBy("faction:" + id, x + 4, y + h - 150, w - 8, 146, in);
}
void Editor::drawBoardInfo(int x, int y, int w, int h, InputState& in) {
    GenericEditor& g = gens[13];
    ui.panel(x, y, w, h); ui.header(x, y, w, "What this board does"); helpBtn(x + w - 16, y + 1, "tab.bounties", "start");
    struct Row { std::string text; Color8 col; };
    std::vector<Row> rows;
    if (g.secs.empty() || g.sel < 0 || g.sel >= (int)g.secs.size()) {
        rows.push_back({"Notice boards post bounties: kill jobs, fetch jobs and courier letters to other boards. A board is an NPC with board=<id> (or a dialogue line board=<id>); its [board] entry here says what it posts.", DIM});
        if (g.secs.empty()) rows.push_back({"This campaign has no bounties.txt yet, so every board= in the maps uses the default settings. Press New to describe a board.", ACCENT2});
        auto b = boardIds(); std::string l; for (auto& s : b) l += (l.empty() ? "" : ", ") + s; rows.push_back({"Boards found: " + (l.empty() ? std::string("none") : l), TEXT});
    } else {
        EdSection& s = g.secs[g.sel]; std::vector<std::pair<bool, std::string>> probs; boardProblems(s, probs);
        if (probs.empty()) rows.push_back({"No problems found.", OK});
        for (auto& p : probs) rows.push_back({(p.first ? "ERROR: " : "warning: ") + p.second, p.first ? DANGER : ACCENT2});
        rows.push_back({"", DIM});
        auto ids = [](const std::string& v) { std::vector<std::string> o; for (auto& p : split(v, ',')) if (!trim(p).empty()) o.push_back(trim(p)); return o; };
        if (s.name == "board") {
            std::string id = s.get("id");
            std::string kinds = s.has("kinds") ? s.get("kinds") : "kill, fetch, deliver";
            rows.push_back({fmt("Posts %d random notice(s) a day (%s), reward %s%%.", atoi(s.get("offers", "3").c_str()), kinds.c_str(), s.get("reward", "100").c_str()), TEXT});
            rows.push_back({"Kill jobs: " + (!s.get("creatures").empty() ? "these creatures: " + s.get("creatures") : !s.get("kill").empty() ? "monsters of " + s.get("kill") : std::string("monsters of the 4 nearest dungeons the hero can handle")), TEXT});
            rows.push_back({"Fetch jobs: " + (s.get("fetch").empty() ? std::string("random cheap crafting materials") : s.get("fetch")), TEXT});
            std::string dl; for (auto& b : boardIds()) if (b != id && (ids(s.get("deliver")).empty() || std::find(ids(s.get("deliver")).begin(), ids(s.get("deliver")).end(), b) != ids(s.get("deliver")).end())) dl += (dl.empty() ? "" : ", ") + b;
            rows.push_back({"Letters go to: " + (dl.empty() ? std::string("nowhere") : dl) + " (once the party has visited that board's map)", TEXT});
            int nf = 0; for (auto& o : g.secs) if (o.name == "bounty" && o.get("board") == id) nf++;
            rows.push_back({fmt("Hand-written notices: %d", nf), TEXT});
            rows.push_back({"", DIM});
            drawUsedBy("board:" + id, x + 4, y + h - 130, w - 8, 126, in);
        } else {
            rows.push_back({"A hand-written notice: the board '" + s.get("board") + "' offers it every day until it has been paid" + (s.get("repeat") == "1" ? " (repeat=1: then it comes back)." : "."), TEXT});
            if (!s.get("require").empty()) rows.push_back({"Only while " + condEnglish(s.get("require")) + ".", TEXT});
        }
    }
    int cy = y + 22; int bottom = y + h - (g.sel >= 0 && g.sel < (int)g.secs.size() && g.secs[g.sel].name == "board" ? 136 : 6);
    ui.cv->setClip({x + 1, y + 18, w - 2, bottom - y - 18});
    for (auto& r : rows) { if (r.text.empty()) { cy += 6; continue; } for (auto& l : font->wrap(r.text, w - 16)) { font->draw(*ui.cv, *ts, x + 8, cy, l, r.col); cy += 13; } }
    ui.cv->resetClip();
}
