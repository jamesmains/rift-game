// 1.5.0 (#89): Check all - story contradictions. data/contradictions.txt lists flag pairs that must never both be true
// (goblin_chief_spared & goblin_chief_dead). This looks for a route in the campaign's content that could set both:
//   * every place that sets a flag is a "setter" with the conditions that must hold for it to happen (its guard):
//     dialogue nodes (option conditions along the way, and the show=/hide=/require= of whatever starts the talk),
//     map enemy groups (flag=, ondefeat=; guard = show= and the opposite of a one-term hide=), triggers (require=),
//     chests, doors, levers, obstacles, signs, plates, scripts, dungeons.txt bosses, and the game's own rules
//     (a companion at -40 approval leaves at any time).
//   * A then B is possible unless B's setter clears A, or B's guard says !A, or B's guard says !X for a flag X that the
//     setter of A sets (or needs set) and that nothing ever clears again. Both orders are tried.
// It errs on the side of reporting: a guard it can't read (a hide= with several terms, a dialogue started from a script)
// counts as no guard. Lines with !flag / flag>=N terms or more than two flags are only checked while playing.
#include "editor.h"
#include <algorithm>
#include <set>
#include <map>
#include <functional>

namespace {
using Terms = std::set<std::string>;

Terms condTerms(const std::string& cond) { Terms t; for (auto& p : split(cond, '&')) { std::string s = trim(p); if (!s.empty()) t.insert(s); } return t; }
// the guard a one-term hide= gives (hide=a -> !a, hide=!a -> a); several terms can't be turned into an "and"
Terms hideGuard(const std::string& hide) {
    Terms t = condTerms(hide); if (t.size() != 1) return {};
    std::string s = *t.begin(); return {s[0] == '!' ? trim(s.substr(1)) : "!" + s};
}
Terms objGuard(const EdObj& o) {
    Terms g = condTerms(o.get("show")); for (auto& h : hideGuard(o.get("hide"))) g.insert(h);
    if (o.type == "trigger" || o.type == "door") for (auto& r : condTerms(o.get("require"))) g.insert(r);
    return g;
}
// set:/flag:/inc:/clear:/join:/leave:/fell: in an action list ("set:a,b;notify:x") or one dialogue action
void actionFlags(const std::string& k, const std::string& v, Terms& sets, Terms& clears, std::vector<std::string>& fights, std::vector<std::string>& dialogues, const std::function<std::string(const std::string&)>& joinCls) {
    auto names = [](const std::string& s) { std::vector<std::string> o; for (auto& f : split(s, ',')) { std::string n = trim(split(trim(f), '=')[0]); if (!n.empty()) o.push_back(n); } return o; };
    if (k == "set") for (auto& n : names(v)) sets.insert(n);
    else if (k == "flag" || k == "inc") { if (!trim(v).empty()) sets.insert(trim(v)); }
    else if (k == "clear") for (auto& n : names(v)) clears.insert(n);
    else if (k == "join") { std::string c = joinCls(v); if (!c.empty()) { sets.insert("joined_" + c); clears.insert("left_" + c); } }
    else if (k == "leave" || k == "fell") { std::string c = trim(split(v, '|')[0]); if (!c.empty()) { sets.insert((k == "leave" ? "left_" : "fell_") + c); clears.insert("joined_" + c); } }
    else if (k == "fight") { if (!trim(v).empty()) fights.push_back(trim(v)); }
    else if (k == "dialogue") { if (!trim(v).empty()) dialogues.push_back(trim(v)); }
    else if (k == "win") sets.insert("game_won");
}
void actionList(const std::string& acts, Terms& sets, Terms& clears, std::vector<std::string>& fights, std::vector<std::string>& dialogues, const std::function<std::string(const std::string&)>& joinCls) {
    for (auto& a : split(acts, ';')) { std::string t = trim(a); size_t c = t.find(':'); if (c == std::string::npos) continue; actionFlags(trim(t.substr(0, c)), trim(t.substr(c + 1)), sets, clears, fights, dialogues, joinCls); }
}
struct DNode { std::vector<std::pair<std::string, std::string>> acts; std::vector<std::pair<std::string, Terms>> edges; int line = 0; };
}

void Editor::contradictionCheck() {
    struct CSetter { std::set<std::string> sets, clears, guard; std::string where; FlagUse jump; };
    std::string text;
    std::vector<Game::Contradiction> pairs;
    if (readFile(readPath("contradictions.txt"), text)) pairs = Game::parseContradictions(text);
    if (dataRoot != baseRoot && campaignInherit && fileExists(dataRoot + "/contradictions.txt") && readFile(baseRoot + "/contradictions.txt", text)) { auto b = Game::parseContradictions(text); pairs.insert(pairs.begin(), b.begin(), b.end()); }   // base + the campaign's own
    if (pairs.empty()) return;
    auto joinCls = [this](const std::string& v) { std::string jv = trim(v); return jv.empty() ? std::string() : joinClassOf(jv); };
    std::vector<CSetter> setters;
    // ---- dialogue starters: who opens which file, and under what conditions ----
    std::map<std::string, std::vector<Terms>> starters; std::set<std::string> openStart;   // openStart: started from somewhere we can't read
    auto addStarter = [&](const std::string& dlg, const Terms& g) { starters[dlg].push_back(g); };
    auto scanActs = [&](const std::string& acts, const Terms& g) { for (auto& a : split(acts, ';')) { std::string t = trim(a); if (startsWith(t, "dialogue:")) addStarter(trim(t.substr(9)), g); } };
    // ---- map objects ----
    std::map<std::string, std::pair<Terms, Terms>> groupFlags;   // enemy flag= -> (sets, clears) when beaten (for fight=)
    std::map<std::string, std::string> bossDefeat;                // dungeons.txt bossflag -> bossdefeat
    for (auto& d : gens[7].secs) if (!d.get("bossflag").empty()) bossDefeat[d.get("bossflag")] = d.get("bossdefeat");
    std::set<std::string> bossOnMap;
    for (auto& ms : mapScan) for (size_t oi = 0; oi < ms.objs.size(); oi++) {
        const EdObj& o = ms.objs[oi]; Terms g = objGuard(o);
        CSetter s; s.guard = g; s.jump.tab = 0; s.jump.file = ms.id; s.jump.obj = (int)oi;
        s.where = "map " + ms.id + ": " + o.type + (o.get("id").empty() ? "" : " " + o.get("id")) + " at " + o.get("x") + "," + o.get("y");
        std::vector<std::string> fights, dlgs;
        if (o.type == "npc") { if (!o.get("dialogue").empty()) addStarter(o.get("dialogue"), g); continue; }
        if (o.type == "enemy") {
            if (!o.get("flag").empty()) s.sets.insert(o.get("flag"));
            actionList(o.get("ondefeat"), s.sets, s.clears, fights, dlgs, joinCls);
            auto bd = bossDefeat.find(o.get("flag")); if (bd != bossDefeat.end()) { actionList(bd->second, s.sets, s.clears, fights, dlgs, joinCls); bossOnMap.insert(bd->first); }
            for (const char* k : {"ongoal", "ongoalfail"}) actionList(o.get(k), s.sets, s.clears, fights, dlgs, joinCls);
            scanActs(o.get("ondefeat"), g); if (bd != bossDefeat.end()) scanActs(bd->second, g);
            if (!o.get("flag").empty()) groupFlags[o.get("flag")] = {s.sets, s.clears};
        } else {
            if (!o.get("flag").empty() && o.type != "exit" && o.type != "npc") s.sets.insert(o.get("flag"));
            if (o.type == "door" && !o.get("openif").empty()) for (auto& t : condTerms(o.get("openif"))) s.guard.insert(t);
            for (const char* k : {"script", "onuse", "onclear", "onpress", "onrelease"}) { actionList(o.get(k), s.sets, s.clears, fights, dlgs, joinCls); scanActs(o.get(k), o.type == "trigger" ? g : Terms()); }
        }
        if (!s.sets.empty() || !s.clears.empty()) setters.push_back(s);
    }
    for (auto& ms : mapScan) { std::string oe; for (auto& kv : ms.header) if (kv.first == "onenter") oe = kv.second;
        if (oe.empty()) continue; CSetter s; std::vector<std::string> f, d; actionList(oe, s.sets, s.clears, f, d, joinCls); scanActs(oe, {}); s.where = "map " + ms.id + ": onenter="; s.jump.tab = 0; s.jump.file = ms.id; if (!s.sets.empty() || !s.clears.empty()) setters.push_back(s); }
    // generated dungeons (no map file): the boss group, with nothing guarding it
    for (auto& d : gens[7].secs) { std::string bf = d.get("bossflag"); if (bf.empty() || bossOnMap.count(bf)) continue;
        CSetter s; s.sets.insert(bf); std::vector<std::string> f, dl; actionList(d.get("bossdefeat"), s.sets, s.clears, f, dl, joinCls); scanActs(d.get("bossdefeat"), {}); s.where = "dungeon " + d.get("id") + ": the boss"; s.jump.tab = 10; groupFlags[bf] = {s.sets, s.clears}; setters.push_back(s); }
    { std::string ct; if (readFile(readPath("campaign.txt"), ct)) for (auto& raw : split(ct, '\n')) { std::string l = trim(raw); if (startsWith(l, "dialogue=")) openStart.insert(trim(l.substr(9))); size_t at = 0; while ((at = l.find("dialogue:", at)) != std::string::npos) { size_t e = at + 9; while (e < l.size() && (isalnum((unsigned char)l[e]) || l[e] == '_')) e++; openStart.insert(l.substr(at + 9, e - at - 9)); at = e; } } }
    // ---- scripts: their set/clear lines happen whenever the script runs; dialogues they open start unguarded ----
    for (auto& sf : scriptFiles) { std::string body; if (!readFile(readPath("scripts/" + sf + ".txt"), body)) continue;
        CSetter s; s.where = "script " + sf; s.jump.tab = 25; s.jump.file = sf;
        for (auto& raw : split(body, '\n')) { std::string l = trim(raw); size_t sp = l.find_first_of(" =:"); if (sp == std::string::npos) continue; std::string w = toLower(l.substr(0, sp)), rest = trim(l.substr(sp + 1));
            if (w == "dialogue") { std::string id = trim(split(rest, ' ')[0]); if (!id.empty()) openStart.insert(id); continue; }
            if (rest.find('{') != std::string::npos) continue;   // set {arg1}: the flag comes from the caller
            std::vector<std::string> f, d; actionFlags(w, rest, s.sets, s.clears, f, d, joinCls); }
        if (!s.sets.empty() || !s.clears.empty()) setters.push_back(s); }
    // ---- the game's own rules: a companion pushed to -40 approval walks out at any time ----
    for (auto& c : gd->classes) { CSetter s; s.sets.insert("left_" + c.first); s.clears.insert("joined_" + c.first); s.where = "the game (a companion at -40 approval leaves)"; setters.push_back(s); }
    // ---- dialogue files: nodes, edges, then the guard of each node (what must hold on every way to it) ----
    std::map<std::string, std::map<std::string, DNode>> files;
    for (auto& f : dlgFiles) {
        if (!readFile(readPath("dialogue/" + f + ".txt"), text)) continue;
        auto& nodes = files[f]; std::string cur; int ln = 0;
        for (auto& raw : split(text, '\n')) { ln++; std::string l = trim(raw); if (l.empty() || l[0] == '#') continue;
            if (l[0] == '[') { cur = trim(l.substr(1, l.find(']') - 1)); nodes[cur].line = ln - 1; continue; }
            if (cur.empty()) continue; size_t eq = l.find('='); if (eq == std::string::npos) continue;
            std::string k = trim(l.substr(0, eq)), v = trim(l.substr(eq + 1));
            if (k == "option") { auto p = split(v, '|'); std::string tgt = p.size() > 1 ? trim(p[1]) : "end"; Terms c; for (size_t i = 2; i < p.size(); i++) for (auto& t : condTerms(p[i])) c.insert(t); nodes[cur].edges.push_back({tgt, c}); }
            else if (k == "next") nodes[cur].edges.push_back({v, {}});
            else if (k != "speaker" && k != "text") { nodes[cur].acts.push_back({k, v}); if (k == "dialogue") openStart.insert(v); }   // a talk opened from another talk: no guard known
        }
    }
    for (auto& fk : files) {
        auto& nodes = fk.second; std::map<std::string, Terms> guard; std::set<std::string> known;
        if (nodes.count("start")) {
            Terms g; bool first = true;
            if (!openStart.count(fk.first) && starters.count(fk.first)) for (auto& s : starters[fk.first]) { if (first) { g = s; first = false; } else { Terms k; for (auto& t : g) if (s.count(t)) k.insert(t); g = k; } }
            guard["start"] = g; known.insert("start");
        }
        for (auto& n : nodes) if (!known.count(n.first) && n.first != "start") { bool target = false; for (auto& m : nodes) for (auto& e : m.second.edges) if (e.first == n.first) target = true; if (!target) { guard[n.first] = {}; known.insert(n.first); } }   // [camp] and other entry points
        for (bool changed = true; changed;) { changed = false;
            for (auto& n : nodes) { if (!known.count(n.first)) continue;
                for (auto& e : n.second.edges) { if (e.first == "end" || !nodes.count(e.first)) continue;
                    Terms cand = guard[n.first]; for (auto& t : e.second) cand.insert(t);
                    if (!known.count(e.first)) { guard[e.first] = cand; known.insert(e.first); changed = true; }
                    else { Terms k; for (auto& t : guard[e.first]) if (cand.count(t)) k.insert(t); if (k != guard[e.first]) { guard[e.first] = k; changed = true; } } } } }
        for (auto& n : nodes) {
            CSetter s; s.guard = guard[n.first]; std::vector<std::string> fights, dlgs;
            for (auto& a : n.second.acts) actionFlags(a.first, a.second, s.sets, s.clears, fights, dlgs, joinCls);
            for (auto& fl : fights) { auto gf = groupFlags.find(fl); if (gf != groupFlags.end()) { for (auto& x : gf->second.first) s.sets.insert(x); for (auto& x : gf->second.second) s.clears.insert(x); } else s.sets.insert(fl); }
            for (auto& x : s.clears) s.sets.erase(x);   // clear= then set= of the same flag in one node is unusual; take the clear
            if (s.sets.empty() && s.clears.empty()) continue;
            s.where = "dialogue " + fk.first + " [" + n.first + "]"; s.jump.tab = 11; s.jump.file = fk.first; s.jump.line = n.second.line;
            setters.push_back(s);
        }
    }
    // ---- check each pair ----
    std::set<std::string> cleared; for (auto& s : setters) for (auto& c : s.clears) cleared.insert(c);
    auto blocked = [&](const CSetter& sa, const std::string& a, const CSetter& sb) {
        if (sb.clears.count(a)) return true;
        if (sb.guard.count("!" + a)) return true;
        for (auto& t : sb.guard) { if (t.empty() || t[0] != '!') continue; std::string x = trim(t.substr(1));
            if (cleared.count(x)) continue;   // could be undone again later
            if (sa.sets.count(x) || sa.guard.count(x)) return true; }
        return false;
    };
    for (auto& c : pairs) {
        if (c.terms.size() != 2) continue;
        bool plain = true; for (auto& t : c.terms) if (t.empty() || t[0] == '!' || t.find_first_of("<>=:") != std::string::npos) plain = false;
        if (!plain) continue;
        std::string a = c.terms[0], b = c.terms[1]; int routes = 0; std::string first; FlagUse jump;
        for (int order = 0; order < 2; order++) {
            const std::string& x = order ? b : a; const std::string& y = order ? a : b;
            for (auto& sa : setters) { if (!sa.sets.count(x)) continue;
                for (auto& sb : setters) { if (!sb.sets.count(y)) continue;
                    if (&sa == &sb) { if (!routes++) { first = sa.where + " sets both " + x + " and " + y; jump = sa.jump; } continue; }
                    if (blocked(sa, x, sb)) continue;
                    if (!routes++) { first = sa.where + " sets " + x + ", and then " + sb.where + " can still set " + y + " (nothing in its conditions rules out " + x + ")"; jump = sb.jump; } } }
        }
        if (!routes) continue;
        Finding f; f.err = true; f.jump = jump;
        f.text = "Contradiction: " + a + " and " + b + " can both end up true" + (c.note.empty() ? "" : " (" + c.note + ")") + ": " + first + (routes > 1 ? fmt(" - and %d more way(s)", routes - 1) : std::string()) + ". Add a condition (show=/hide=/require=/option) or clear the other flag.";
        report.push_back(f);
    }
}
