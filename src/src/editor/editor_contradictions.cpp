// 1.5.0 (#89): Check all - story contradictions. data/contradictions.txt lists flag pairs that must never both be true
// (goblin_chief_spared & goblin_chief_dead). This looks for a route in the campaign's content that could set both:
//   * every place that sets a flag is a "setter" with the conditions that must hold for it to happen (its guard):
//     dialogue nodes (option conditions along the way, and the show=/hide=/require= of whatever starts the talk),
//     map enemy groups (flag=, ondefeat=; guard = show=, the opposite of a one-term hide=, and not beaten yet),
//     triggers (require=), chests, doors, levers, obstacles, signs, plates, map onenter=, scripts, dungeons.txt
//     bosses, and the game's own rule that a companion at -40 approval walks out at any time.
//   * A then B is possible unless B's setter clears A, or B's guard says !A, or B's guard says !X (or !X>=N) for a flag
//     X that the setter of A sets (or needs set) and that nothing ever clears again. Counters (set=q_x=2) are taken to
//     only go up. Both orders are tried.
//   * A talk opened by a one-time event (a beaten group's ondefeat=, a trigger or chest with flag=) belongs to that
//     event: its answers are alternatives, unless one node leads to the other.
// It errs on the side of reporting: a guard it can't read (a hide= with several terms, a dialogue started from a script
// or another dialogue) counts as no guard. Lines with !flag / flag>=N terms or more than two flags are only checked
// while playing (Game::checkContradictions).
#include "editor.h"
#include <algorithm>
#include <set>
#include <map>
#include <functional>

namespace {
using Terms = std::set<std::string>;
struct Eff { Terms sets, clears; std::map<std::string, int> vals; std::vector<std::string> fights, dialogues; };

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
// set/flag/inc/clear/join/leave/fell/fight/dialogue/win: one dialogue action, or one entry of an action list
void actionFlags(const std::string& k, const std::string& v, Eff& e, const std::function<std::string(const std::string&)>& joinCls) {
    if (k == "set") { for (auto& f : split(v, ',')) { auto p = split(trim(f), '='); std::string n = trim(p[0]); if (n.empty()) continue; e.sets.insert(n); e.vals[n] = p.size() > 1 ? atoi(p[1].c_str()) : 1; } }
    else if (k == "flag") { if (!trim(v).empty()) { e.sets.insert(trim(v)); e.vals[trim(v)] = 1; } }
    else if (k == "inc") { if (!trim(v).empty()) e.sets.insert(trim(v)); }
    else if (k == "clear") { for (auto& f : split(v, ',')) { std::string n = trim(split(trim(f), '=')[0]); if (!n.empty()) e.clears.insert(n); } }
    else if (k == "join") { std::string c = joinCls(v); if (!c.empty()) { e.sets.insert("joined_" + c); e.clears.insert("left_" + c); } }
    else if (k == "leave" || k == "fell") { std::string c = trim(split(v, '|')[0]); if (!c.empty()) { e.sets.insert((k == "leave" ? "left_" : "fell_") + c); e.clears.insert("joined_" + c); } }
    else if (k == "fight") { if (!trim(v).empty()) e.fights.push_back(trim(v)); }
    else if (k == "dialogue") { if (!trim(v).empty()) e.dialogues.push_back(trim(split(trim(v), ',')[0])); }
    else if (k == "win") e.sets.insert("game_won");
}
void actionList(const std::string& acts, Eff& e, const std::function<std::string(const std::string&)>& joinCls) {
    for (auto& a : split(acts, ';')) { std::string t = trim(a); size_t c = t.find(':'); if (c == std::string::npos) continue; actionFlags(trim(t.substr(0, c)), trim(t.substr(c + 1)), e, joinCls); }
}
struct DNode { std::vector<std::pair<std::string, std::string>> acts; std::vector<std::pair<std::string, Terms>> edges; int line = 0; };
struct Starter { Terms guard; Eff event; bool oneShot = false; };
Terms intersect(const Terms& a, const Terms& b) { Terms k; for (auto& t : a) if (b.count(t)) k.insert(t); return k; }
}

void Editor::contradictionCheck() {
    struct CSetter { Eff eff; Terms guard; std::string where, file, node; FlagUse jump; };
    std::string text;
    std::vector<Game::Contradiction> pairs;
    if (readFile(readPath("contradictions.txt"), text)) pairs = Game::parseContradictions(text);
    if (dataRoot != baseRoot && campaignInherit && fileExists(dataRoot + "/contradictions.txt") && readFile(baseRoot + "/contradictions.txt", text)) { auto b = Game::parseContradictions(text); pairs.insert(pairs.begin(), b.begin(), b.end()); }   // base + the campaign's own
    if (pairs.empty()) return;
    auto joinCls = [this](const std::string& v) { std::string jv = trim(v); return jv.empty() ? std::string() : joinClassOf(jv); };
    std::vector<CSetter> setters;
    // ---- who opens which dialogue, under what conditions ----
    std::map<std::string, std::vector<Starter>> starters; std::set<std::string> openStart;   // openStart: opened from somewhere we can't read
    auto starts = [&](const Eff& e, const Terms& g, const Eff& event, bool oneShot) { for (auto& d : std::set<std::string>(e.dialogues.begin(), e.dialogues.end())) { Starter s; s.guard = g; s.event = event; s.oneShot = oneShot; starters[d].push_back(s); } };
    // ---- map objects ----
    std::map<std::string, Eff> groupEff;                          // enemy flag= -> what beating it sets (for fight=)
    std::map<std::string, std::string> bossDefeat;                // dungeons.txt bossflag -> bossdefeat
    for (auto& d : gens[7].secs) if (!d.get("bossflag").empty()) bossDefeat[d.get("bossflag")] = d.get("bossdefeat");
    std::set<std::string> bossOnMap;
    for (auto& ms : mapScan) for (size_t oi = 0; oi < ms.objs.size(); oi++) {
        const EdObj& o = ms.objs[oi]; Terms g = objGuard(o);
        CSetter s; s.guard = g; s.jump.tab = 0; s.jump.file = ms.id; s.jump.obj = (int)oi;
        s.where = "map " + ms.id + ": " + o.type + (o.get("id").empty() ? "" : " " + o.get("id")) + " at " + o.get("x") + "," + o.get("y");
        std::string fl = o.get("flag");
        if (o.type == "npc") { if (!o.get("dialogue").empty()) { Starter st; st.guard = g; starters[o.get("dialogue")].push_back(st); } continue; }
        if (o.type == "exit") continue;
        if (!fl.empty()) { s.eff.sets.insert(fl); s.eff.vals[fl] = 1; }
        bool oneShot = !fl.empty() && o.type != "lever";   // a beaten group, a fired trigger, an opened chest: once
        if (oneShot) s.guard.insert("!" + fl);
        Eff acts;
        if (o.type == "enemy") {
            actionList(o.get("ondefeat"), acts, joinCls);
            auto bd = bossDefeat.find(fl); if (bd != bossDefeat.end()) { actionList(bd->second, acts, joinCls); bossOnMap.insert(bd->first); }
            for (const char* k : {"ongoal", "ongoalfail"}) actionList(o.get(k), acts, joinCls);
        } else {
            if (o.type == "door" && !o.get("openif").empty()) for (auto& t : condTerms(o.get("openif"))) s.guard.insert(t);
            for (const char* k : {"script", "onuse", "onclear", "onpress", "onrelease"}) actionList(o.get(k), acts, joinCls);
        }
        for (auto& x : acts.sets) s.eff.sets.insert(x); for (auto& x : acts.clears) s.eff.clears.insert(x); for (auto& kv : acts.vals) s.eff.vals[kv.first] = kv.second;
        s.eff.fights = acts.fights; s.eff.dialogues = acts.dialogues;
        starts(acts, s.guard, s.eff, oneShot);
        if (o.type == "enemy" && !fl.empty()) groupEff[fl] = s.eff;
        if (!s.eff.sets.empty() || !s.eff.clears.empty()) setters.push_back(s);
    }
    for (auto& ms : mapScan) { std::string oe; for (auto& kv : ms.header) if (kv.first == "onenter") oe = kv.second;
        if (oe.empty()) continue; CSetter s; actionList(oe, s.eff, joinCls); for (auto& d : s.eff.dialogues) openStart.insert(d);
        s.where = "map " + ms.id + ": onenter="; s.jump.tab = 0; s.jump.file = ms.id; if (!s.eff.sets.empty() || !s.eff.clears.empty()) setters.push_back(s); }
    // generated dungeons (no map file): the boss group
    for (auto& d : gens[7].secs) { std::string bf = d.get("bossflag"); if (bf.empty() || bossOnMap.count(bf)) continue;
        CSetter s; s.eff.sets.insert(bf); s.eff.vals[bf] = 1; s.guard.insert("!" + bf); actionList(d.get("bossdefeat"), s.eff, joinCls); starts(s.eff, {}, s.eff, true);
        s.where = "dungeon " + d.get("id") + ": the boss"; s.jump.tab = 10; groupEff[bf] = s.eff; setters.push_back(s); }
    { std::string ct; if (readFile(readPath("campaign.txt"), ct)) for (auto& raw : split(ct, '\n')) { std::string l = trim(raw); if (startsWith(l, "dialogue=")) openStart.insert(trim(l.substr(9))); size_t at = 0; while ((at = l.find("dialogue:", at)) != std::string::npos) { size_t e = at + 9; while (e < l.size() && (isalnum((unsigned char)l[e]) || l[e] == '_')) e++; openStart.insert(l.substr(at + 9, e - at - 9)); at = e; } } }
    // ---- scripts: their set/clear lines can happen whenever the script runs; dialogues they open start unguarded ----
    for (auto& sf : scriptFiles) { std::string body; if (!readFile(readPath("scripts/" + sf + ".txt"), body)) continue;
        CSetter s; s.where = "script " + sf; s.jump.tab = 25; s.jump.file = sf;
        for (auto& raw : split(body, '\n')) { std::string l = trim(raw); size_t sp = l.find_first_of(" =:"); if (sp == std::string::npos) continue; std::string w = toLower(l.substr(0, sp)), rest = trim(l.substr(sp + 1));
            if (w == "dialogue") { std::string id = trim(split(rest, ' ')[0]); if (!id.empty()) openStart.insert(id); continue; }
            if (rest.find('{') != std::string::npos) continue;   // set {arg1}: the flag comes from the caller
            actionFlags(w, rest, s.eff, joinCls); }
        if (!s.eff.sets.empty() || !s.eff.clears.empty()) setters.push_back(s); }
    // ---- the game's own rule: a companion pushed to -40 approval walks out at any time ----
    for (auto& c : gd->classes) { CSetter s; s.eff.sets.insert("left_" + c.first); s.eff.clears.insert("joined_" + c.first); s.where = "the game (a companion at -40 approval leaves)"; setters.push_back(s); }
    // ---- dialogue files: nodes and edges, then each node's guard (what holds on every way to it) ----
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
            else if (k != "speaker" && k != "text") { nodes[cur].acts.push_back({k, v}); if (k == "dialogue") openStart.insert(v); }
        }
    }
    std::map<std::string, std::map<std::string, std::set<std::string>>> reach;   // file -> node -> nodes reachable from it
    for (auto& fk : files) {
        auto& nodes = fk.second; std::map<std::string, Terms> guard; std::set<std::string> known, fromStart;
        bool oneShot = false; Eff event;
        if (nodes.count("start")) {
            Terms g; bool first = true; auto& st = starters[fk.first];
            if (!openStart.count(fk.first) && !st.empty()) {
                oneShot = true; for (auto& s : st) if (!s.oneShot) oneShot = false;
                for (auto& s : st) { if (first) { g = s.guard; first = false; } else g = intersect(g, s.guard); }
                if (oneShot && st.size() == 1) { event = st[0].event; g = intersect(g, st[0].guard); } else oneShot = false;
            }
            guard["start"] = g; known.insert("start");
        }
        for (auto& n : nodes) if (!known.count(n.first) && n.first != "start") { bool target = false; for (auto& m : nodes) for (auto& e : m.second.edges) if (e.first == n.first) target = true; if (!target) { guard[n.first] = {}; known.insert(n.first); } }   // [camp] and other entry points
        for (bool changed = true; changed;) { changed = false;
            for (auto& n : nodes) { if (!known.count(n.first)) continue;
                for (auto& e : n.second.edges) { if (e.first == "end" || !nodes.count(e.first)) continue;
                    Terms cand = guard[n.first]; for (auto& t : e.second) cand.insert(t);
                    if (!known.count(e.first)) { guard[e.first] = cand; known.insert(e.first); changed = true; }
                    else { Terms k = intersect(guard[e.first], cand); if (k != guard[e.first]) { guard[e.first] = k; changed = true; } } } } }
        // reachability inside the file (a node that leads to another can set both in one talk)
        for (auto& n : nodes) { std::set<std::string> seen; std::vector<std::string> q{n.first};
            while (!q.empty()) { std::string c = q.back(); q.pop_back(); auto it = nodes.find(c); if (it == nodes.end()) continue; for (auto& e : it->second.edges) if (!seen.count(e.first)) { seen.insert(e.first); q.push_back(e.first); } }
            reach[fk.first][n.first] = seen; }
        if (nodes.count("start")) { fromStart = reach[fk.first]["start"]; fromStart.insert("start"); }
        for (auto& n : nodes) {
            CSetter s; s.guard = guard[n.first];
            for (auto& a : n.second.acts) actionFlags(a.first, a.second, s.eff, joinCls);
            for (auto& fl : s.eff.fights) { auto gf = groupEff.find(fl); if (gf != groupEff.end()) { for (auto& x : gf->second.sets) s.eff.sets.insert(x); for (auto& x : gf->second.clears) s.eff.clears.insert(x); for (auto& kv : gf->second.vals) s.eff.vals.insert(kv); } else s.eff.sets.insert(fl); }
            for (auto& x : s.eff.clears) s.eff.sets.erase(x);   // clear= and set= of one flag in one node: take the clear
            if (s.eff.sets.empty() && s.eff.clears.empty()) continue;
            if (oneShot && fromStart.count(n.first)) { for (auto& x : event.sets) if (!s.eff.clears.count(x)) s.eff.sets.insert(x); for (auto& kv : event.vals) s.eff.vals.insert(kv); }   // part of the event that opened the talk
            s.where = "dialogue " + fk.first + " [" + n.first + "]"; s.file = fk.first; s.node = n.first; s.jump.tab = 11; s.jump.file = fk.first; s.jump.line = n.second.line;
            setters.push_back(s);
        }
    }
    // ---- check each pair ----
    std::set<std::string> cleared; for (auto& s : setters) for (auto& c : s.eff.clears) cleared.insert(c);
    auto blocked = [&](const CSetter& sa, const std::string& a, const CSetter& sb) {
        if (sb.eff.clears.count(a)) return true;
        if (sb.guard.count("!" + a)) return true;
        if (!sa.file.empty() && sa.file == sb.file && reach[sa.file][sa.node].count(sb.node)) return false;   // one leads to the other in the same talk
        for (auto& t : sb.guard) { if (t.empty() || t[0] != '!') continue; std::string x = trim(t.substr(1)); int need = 1;
            size_t ge = x.find(">="); if (ge != std::string::npos) { need = atoi(x.substr(ge + 2).c_str()); x = trim(x.substr(0, ge)); }
            if (x.find(':') != std::string::npos || cleared.count(x)) continue;   // not a flag, or it could be undone again later
            auto v = sa.eff.vals.find(x); if (v != sa.eff.vals.end() && v->second >= need) return true;
            if (need <= 1 && (sa.eff.sets.count(x) || sa.guard.count(x))) return true; }
        return false;
    };
    for (auto& c : pairs) {
        if (c.terms.size() != 2) continue;
        bool plain = true; for (auto& t : c.terms) if (t.empty() || t[0] == '!' || t.find_first_of("<>=:") != std::string::npos) plain = false;
        if (!plain) continue;
        std::string a = c.terms[0], b = c.terms[1]; int routes = 0; std::string first; FlagUse jump;
        for (int order = 0; order < 2; order++) {
            const std::string& x = order ? b : a; const std::string& y = order ? a : b;
            for (auto& sa : setters) { if (!sa.eff.sets.count(x)) continue;
                for (auto& sb : setters) { if (!sb.eff.sets.count(y)) continue;
                    if (&sa == &sb) { if (order == 0 && !routes++) { first = sa.where + " sets both " + x + " and " + y; jump = sa.jump; } continue; }
                    if (blocked(sa, x, sb)) continue;
                    if (!routes++) { first = sa.where + " sets " + x + ", and then " + sb.where + " can still set " + y + " (nothing in its conditions rules out " + x + ")"; jump = sb.jump; } } }
        }
        if (!routes) continue;
        Finding f; f.err = true; f.jump = jump;
        f.text = "Contradiction: " + a + " and " + b + " can both end up true" + (c.note.empty() ? "" : " (" + c.note + ")") + ": " + first + (routes > 1 ? fmt(" - and %d more way(s)", routes - 1) : std::string()) + ". Add a condition (show=/hide=/require=/option) or clear the other flag.";
        report.push_back(f);
    }
}
