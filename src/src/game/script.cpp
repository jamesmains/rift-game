// Triffall scripts: running them (1.1.10, owner's #34). The compiler is script_compile.cpp; the language is in script.h.
// A run executes statements until one has to wait: say / ask (the talk box, resumed by scriptDialogueDone), pick (the
// world map), wait (seconds of play on the map) or an action that opens a screen (fight, shop, teleport...: resumed once
// the party is back on the map, in tickScripts). Runs are not saved: a save loaded mid-script starts with none.
#include "game.h"
#include "../core/audio.h"
#include <algorithm>

static const char* SCRIPT_DLG = "__script";
static const int SCRIPT_STEP_LIMIT = 200000;   // statements per resume before a run is stopped as an endless loop
static const int SCRIPT_CALL_LIMIT = 40;

std::shared_ptr<ScriptProg> Game::scriptProg(const std::string& id0) {
    std::string id = trim(id0);
    auto it = scriptCache.find(id); if (it != scriptCache.end()) return it->second;
    std::string t; std::shared_ptr<ScriptProg> p;
    if (!id.empty() && id.find('/') == std::string::npos && id.find('\\') == std::string::npos && readFile(dataPath("scripts/" + id + ".txt"), t)) p = std::make_shared<ScriptProg>(compileScript(id, t));
    scriptCache[id] = p;
    return p;
}

static void sweepRuns(std::vector<std::shared_ptr<ScriptRun>>& runs) { runs.erase(std::remove_if(runs.begin(), runs.end(), [](const std::shared_ptr<ScriptRun>& r) { return r->done; }), runs.end()); }

static std::shared_ptr<ScriptRun> newRun(Game& g, std::shared_ptr<ScriptProg> p, const std::vector<SVal>& args, NPC* npc, const std::map<std::string, SVal>& locals) {
    auto r = std::make_shared<ScriptRun>(); r->id = g.scriptNextId++;
    ScriptFrame f; f.prog = p; f.vars = locals;
    for (size_t i = 0; i < args.size(); i++) f.vars["arg" + std::to_string(i + 1)] = args[i];
    f.vars["args"] = SVal::num((long long)args.size());
    r->frames.push_back(f);
    if (!npc && g.mode == GMode::DIALOGUE && g.dlgId != SCRIPT_DLG) npc = g.dlgNpc;   // started from a dialogue's action: that NPC
    if (npc) { r->npcMap = g.mapId; r->npcId = npc->id; }
    r->inDialogue = g.mode == GMode::DIALOGUE && g.dlgId != SCRIPT_DLG;
    return r;
}

bool Game::startScript(const std::string& id0, const std::vector<SVal>& args, NPC* npc, const std::map<std::string, SVal>& locals) {
    std::string id = trim(id0);
    auto p = scriptProg(id);
    if (!p) { log("(missing script " + id + ")"); lastScriptError = "missing script " + id; return false; }
    if (!p->ok()) { lastScriptError = fmt("script %s line %d: %s", id.c_str(), p->errors[0].line + 1, p->errors[0].msg.c_str()); log("(" + lastScriptError + ")"); return false; }
    sweepRuns(scriptRuns);
    if (scriptRuns.size() >= 64) { log("(too many scripts running - " + id + " not started)"); return false; }
    auto r = newRun(*this, p, args, npc, locals);
    scriptRuns.push_back(r);
    resumeScript(r);
    return true;
}
// the dev console's "script" command: a script written on one line, statements separated by \n
bool Game::startScriptText(const std::string& code, const std::string& name) {
    auto p = std::make_shared<ScriptProg>(compileScript(name, code));
    if (!p->ok()) { lastScriptError = fmt("%s line %d: %s", name.c_str(), p->errors[0].line + 1, p->errors[0].msg.c_str()); log("(" + lastScriptError + ")"); return false; }
    auto r = newRun(*this, p, {}, nullptr, {});
    scriptRuns.push_back(r);
    resumeScript(r);
    return true;
}

void Game::scriptFail(ScriptRun& r, const std::string& msg) {
    std::string where = r.frames.empty() ? std::string("?") : r.frames.back().prog->id;
    int line = 0; if (!r.frames.empty()) { auto& f = r.frames.back(); int pc = clampi(f.pc, 0, std::max(0, (int)f.prog->code.size() - 1)); if (!f.prog->code.empty()) line = f.prog->code[pc].line + 1; }
    lastScriptError = fmt("script %s line %d: %s", where.c_str(), line, msg.c_str());
    log("(" + lastScriptError + ")");
    r.done = true;
}

NPC* Game::scriptNpc(const ScriptRun& r) {
    if (!map || r.npcId.empty() || r.npcMap != mapId) return nullptr;
    for (auto& n : map->npcs) if (n.id == r.npcId) return &n;
    return nullptr;
}
const WorldLocation* Game::scriptPlace(const std::string& id) const {
    for (auto& l : locations) if (l.id == id) return &l;
    for (auto& l : locations) if (l.mapId == id) return &l;
    return nullptr;
}

void Game::scriptAssign(ScriptRun& r, const std::string& name, const SVal& v) {
    if (r.frames.empty()) return;
    auto& f = r.frames.back();
    if (f.vars.count(name) || !extra.count("var_" + name)) f.vars[name] = v; else extra["var_" + name] = v.str();
}

// ------------------------------------------------------------------ values
SVal Game::scriptValue(ScriptRun& r, const std::string& n) {
    if (!r.frames.empty()) { auto& f = r.frames.back(); auto it = f.vars.find(n); if (it != f.vars.end()) return it->second; }
    { auto it = extra.find("var_" + n); if (it != extra.end()) return SVal::parse(it->second); }
    if (n == "gold") return SVal::num(party.gold);
    if (n == "day") return SVal::num(day);
    if (n == "hour") return SVal::num(clockMin / 60);
    if (n == "minute") return SVal::num(clockMin % 60);
    if (n == "here") return SVal::text(mapId);
    if (n == "herename" || n == "hero") return SVal::text(heroName);
    if (n == "x") return SVal::num(px);
    if (n == "y") return SVal::num(py);
    if (n == "level") return SVal::num(party.members.empty() ? 1 : party.members[0].level);
    if (n == "partysize") return SVal::num((long long)party.members.size());
    if (n == "night") return SVal::num(nightAmount() > 0.5f ? 1 : 0);
    if (n == "stealth") return SVal::num(stealthSteps > 0 ? 1 : 0);
    if (n == "weather") return SVal::text(weather);
    if (n == "npc") return SVal::text(r.npcId);
    if (n == "npcname") { NPC* np = scriptNpc(r); return SVal::text(np ? np->name : ""); }
    if (n == "won") return SVal::num(lastFightWon ? 1 : 0);
    if (n == "true") return SVal::num(1);
    if (n == "false") return SVal::num(0);
    auto fl = flags.find(n); if (fl != flags.end()) return SVal::num(fl->second);   // any other name: a flag (0 when not set)
    return SVal::num(0);
}

static std::vector<std::string> listItems(const std::string& s) { std::vector<std::string> o; for (auto& p : split(s, ',')) { std::string t = trim(p); if (!t.empty()) o.push_back(t); } return o; }
static std::string joinList(const std::vector<std::string>& v) { std::string o; for (auto& s : v) o += (o.empty() ? "" : ",") + s; return o; }

SVal Game::scriptFn(ScriptRun& r, const std::string& n, const std::vector<SVal>& a, std::string& err) {
    (void)r;
    auto s0 = [&]() { return a.empty() ? std::string() : trim(a[0].str()); };
    auto n0 = [&]() { return a.empty() ? 0LL : a[0].toNum(); };
    auto n1 = [&]() { return a.size() < 2 ? 0LL : a[1].toNum(); };
    if (n == "flag") { auto it = flags.find(s0()); return SVal::num(it == flags.end() ? 0 : it->second); }
    if (n == "cond") return SVal::num(evalCond(s0()) ? 1 : 0);
    if (n == "has") { int c = party.countItem(s0()); for (auto& m : party.members) for (int sl = 0; sl < SL_COUNT; sl++) if (m.equip[sl] == s0()) c++; return SVal::num(c); }
    if (n == "visited") return SVal::num(visited.count(s0()) ? 1 : 0);
    if (n == "inparty") return SVal::num(evalCond("party:" + s0()) ? 1 : 0);
    if (n == "rep") { auto it = reputation.find(s0()); return SVal::num(it == reputation.end() ? 0 : it->second); }
    if (n == "approval") return SVal::num(approvalOf(s0()));
    if (n == "kills") { auto it = bestiary.find(s0()); return SVal::num(it == bestiary.end() ? 0 : it->second); }
    if (n == "random") { long long lo = a.size() > 1 ? n0() : 1, hi = a.size() > 1 ? n1() : n0(); if (hi < lo) std::swap(lo, hi); return SVal::num(rng.range((int)lo, (int)hi)); }
    if (n == "chance") return SVal::num(rng.chance((int)n0()) ? 1 : 0);
    if (n == "places") { std::vector<std::string> o; for (auto& l : locations) if (visited.count(l.mapId)) o.push_back(l.id); return SVal::text(joinList(o)); }
    if (n == "placename" || n == "placemap" || n == "placex" || n == "placey" || n == "distance" || n == "leagues") {
        const WorldLocation* l = scriptPlace(s0());
        if (!l) { if (n == "placename" || n == "placemap") return SVal::text(""); return SVal::num(0); }
        if (n == "placename") return SVal::text(l->name);
        if (n == "placemap") return SVal::text(l->mapId);
        if (n == "placex") return SVal::num(l->x);
        if (n == "placey") return SVal::num(l->y);
        int fx = px, fy = py; if (!map || map->kind != "overworld") for (auto& o : locations) if (o.mapId == mapId) { fx = o.x; fy = o.y; }   // where the party is on the overworld
        int d = chebyshev(fx, fy, l->x, l->y);
        return SVal::num(n == "distance" ? d : d / 10 + 1);
    }
    if (n == "mapname") return SVal::text(mapDisplayName(s0()));
    if (n == "itemname") { const ItemDef* d = gd->item(s0()); return SVal::text(d ? d->name : s0()); }
    if (n == "count") return SVal::num((long long)listItems(a[0].str()).size());
    if (n == "at") { auto v = listItems(a[0].str()); long long i = n1(); return i >= 1 && i <= (long long)v.size() ? SVal::parse(v[(size_t)i - 1]) : SVal::text(""); }
    if (n == "contains") { std::string want = toLower(trim(a[1].str())); for (auto& x : listItems(a[0].str())) if (toLower(x) == want) return SVal::num(1); return SVal::num(0); }
    if (n == "add") { auto v = listItems(a[0].str()); v.push_back(trim(a[1].str())); return SVal::text(joinList(v)); }
    if (n == "len") return SVal::num((long long)a[0].str().size());
    if (n == "upper") return SVal::text(toUpper(a[0].str()));
    if (n == "lower") return SVal::text(toLower(a[0].str()));
    if (n == "find") { size_t p = toLower(a[0].str()).find(toLower(a[1].str())); return SVal::num(p == std::string::npos ? 0 : (long long)p + 1); }
    if (n == "min") return SVal::num(std::min(n0(), n1()));
    if (n == "max") return SVal::num(std::max(n0(), n1()));
    if (n == "abs") return SVal::num(n0() < 0 ? -n0() : n0());
    if (n == "var") return SVal::parse(keptVar(s0()));
    // 1.4.0 (#76/#77)
    if (n == "mid") { std::string t = a[0].str(); long long st = n1(); long long ln = a.size() > 2 ? a[2].toNum() : (long long)t.size(); if (st < 1) { ln += st - 1; st = 1; } if (ln <= 0 || st > (long long)t.size()) return SVal::text(""); return SVal::text(t.substr((size_t)st - 1, (size_t)ln)); }
    if (n == "squash") { std::string o; for (char c : a[0].str()) if (isalnum((unsigned char)c)) o += (char)toupper((unsigned char)c); return SVal::text(o); }   // codes: case, spaces and dashes don't matter
    if (n == "plates") return SVal::num(platesIn(s0(), false));
    if (n == "platesdown") return SVal::num(platesIn(s0(), true));
    if (n == "crateat") return SVal::num(map && map->crateAt((int)n0(), (int)n1()) ? 1 : 0);
    if (n == "lorefile") return SVal::num(foundFiles.count(s0()) ? 1 : 0);
    // 1.4.0 (#73): stat("cha") = the hero's stat with gear, partystat("str") = the best living member's (as the conditions)
    if (n == "stat" || n == "partystat") { std::string nm = toUpper(s0()); int st = -1; for (int i = 0; i < ST_COUNT; i++) if (nm == statName(i)) st = i;
        if (st < 0) { err = n + "() needs str, dex, con, int, wis or cha (got '" + s0() + "')"; return SVal::num(0); }
        if (party.members.empty()) return SVal::num(0);
        int have = party.members[0].stat(st, *gd); if (n == "partystat") for (auto& m : party.members) if (m.alive) have = std::max(have, m.stat(st, *gd));
        return SVal::num(have); }
    err = "unknown function " + n + "()";
    return SVal::num(0);
}

SVal Game::scriptEval(ScriptRun& r, const SExpr& e, std::string& err) {
    if (!err.empty()) return SVal::num(0);
    switch (e.kind) {
        case SX_NUM: return SVal::num(e.n);
        case SX_STR: return SVal::text(e.s);
        case SX_INTERP: { std::string o; for (auto& k : e.kids) o += scriptEval(r, k, err).str(); return SVal::text(o); }
        case SX_VAR: return scriptValue(r, e.s);
        case SX_CALL: { std::vector<SVal> a; for (auto& k : e.kids) a.push_back(scriptEval(r, k, err)); if (!err.empty()) return SVal::num(0); return scriptFn(r, e.s, a, err); }
        case SX_NOT: return SVal::num(scriptEval(r, e.kids[0], err).truthy() ? 0 : 1);
        case SX_NEG: { SVal v = scriptEval(r, e.kids[0], err); long long n = 0; if (!v.numeric(&n)) { err = "can't make text negative ('" + v.str() + "')"; return SVal::num(0); } return SVal::num(-n); }
        case SX_BIN: {
            const std::string& op = e.s;
            if (op == "and") { if (!scriptEval(r, e.kids[0], err).truthy()) return SVal::num(0); return SVal::num(scriptEval(r, e.kids[1], err).truthy() ? 1 : 0); }
            if (op == "or") { if (scriptEval(r, e.kids[0], err).truthy()) return SVal::num(1); return SVal::num(scriptEval(r, e.kids[1], err).truthy() ? 1 : 0); }
            SVal a = scriptEval(r, e.kids[0], err), b = scriptEval(r, e.kids[1], err);
            if (!err.empty()) return SVal::num(0);
            long long x = 0, y = 0; bool nums = a.numeric(&x) && b.numeric(&y);
            if (op == "+") return nums ? SVal::num(x + y) : SVal::text(a.str() + b.str());
            if (op == "==" || op == "!=") { bool eq = nums ? x == y : toLower(a.str()) == toLower(b.str()); return SVal::num((op == "==") == eq ? 1 : 0); }
            if (op == "<" || op == "<=" || op == ">" || op == ">=") {
                int c = nums ? (x < y ? -1 : x > y ? 1 : 0) : toLower(a.str()).compare(toLower(b.str()));
                bool v = op == "<" ? c < 0 : op == "<=" ? c <= 0 : op == ">" ? c > 0 : c >= 0; return SVal::num(v ? 1 : 0);
            }
            if (!nums) { err = "can't do " + op + " with text ('" + (a.numeric() ? b.str() : a.str()) + "')"; return SVal::num(0); }
            if (op == "-") return SVal::num(x - y);
            if (op == "*") return SVal::num(x * y);
            if (op == "/" || op == "%") { if (y == 0) { err = "division by zero"; return SVal::num(0); } return SVal::num(op == "/" ? x / y : x % y); }
            err = "unknown operator " + op; return SVal::num(0);
        }
    }
    return SVal::num(0);
}

// ------------------------------------------------------------------ the talk box
// say / ask show a node of the hidden dialogue "__script". A run may use the box when the party is on the map, when it
// already owns the box, or when it was started from a dialogue (it takes that dialogue's box over).
bool Game::scriptShowTalk(ScriptRun& r, const std::string& text, bool withOptions) {
    bool can = mode == GMode::EXPLORE || (mode == GMode::DIALOGUE && (dlgId == SCRIPT_DLG ? scriptDlgOwner == r.id : r.inDialogue));
    if (!can) return false;
    NPC* npc = scriptNpc(r);
    Dialogue& d = dialogues[SCRIPT_DLG]; d.id = SCRIPT_DLG; d.nodes.clear();
    DialogueNode node; node.id = "s"; node.text = text;
    if (withOptions) for (size_t i = 0; i < r.options.size(); i++) { DialogueOption o; o.text = r.options[i].first; o.target = "#" + std::to_string(i); node.options.push_back(o); }
    d.nodes["s"] = node;
    dlgId = SCRIPT_DLG; dlgNpc = npc;
    std::string who = r.speakerSet ? r.speaker : npc ? npc->name : "";
    dlgSpeakerOverride = who;
    if (npc && who == npc->name) { dlgPortrait = npc->tile; dlgPortraitRecolor = npc->recolor; dlgPortraitColor = npc->color; } else dlgPortrait = Tile{};
    if (mode != GMode::DIALOGUE) pushMode(GMode::DIALOGUE);
    showNode("s");
    scriptDlgOwner = r.id; scriptDlgShown = true; r.inDialogue = false;
    return true;
}

void Game::scriptDialogueDone(int option) {
    std::shared_ptr<ScriptRun> r;
    for (auto& x : scriptRuns) if (!x->done && x->id == scriptDlgOwner && x->wait == SW_DIALOGUE) r = x;
    scriptDlgShown = false;
    if (r) {
        r->wait = SW_NONE;
        if (r->pendingKind == SP_ASK) { scriptAssign(*r, r->pendingVar, option >= 0 && option < (int)r->options.size() ? r->options[option].second : SVal::text("")); r->options.clear(); r->pendingVar.clear(); r->pendingKind = SP_NONE; }
        resumeScript(r);
    }
    if (!scriptDlgShown && mode == GMode::DIALOGUE && dlgId == SCRIPT_DLG) popMode();
}

// ------------------------------------------------------------------ running
void Game::resumeScript(std::shared_ptr<ScriptRun> rp) {
    ScriptRun& r = *rp;
    int budget = SCRIPT_STEP_LIMIT;
    while (!r.done && r.wait == SW_NONE) {
        if (--budget <= 0) { scriptFail(r, fmt("ran %d steps without stopping - an endless loop?", SCRIPT_STEP_LIMIT)); break; }
        if (!scriptExec(r)) break;
    }
}

bool Game::scriptExec(ScriptRun& r) {
    if (r.frames.empty()) { r.done = true; return false; }
    ScriptFrame* f = &r.frames.back();
    if (f->pc >= (int)f->prog->code.size()) {   // the end of a script: back to the one that called it
        r.frames.pop_back();
        if (r.frames.empty()) { r.done = true; return false; }
        return true;
    }
    const SStmt& s = f->prog->code[f->pc];
    std::string err;
    auto val = [&](const SExpr& e) { return scriptEval(r, e, err); };
    switch (s.op) {
        case SO_LET: { SVal v = val(s.a); if (!err.empty()) break; f->vars[s.name] = v; f->pc++; return true; }
        case SO_KEEP: { SVal v = val(s.a); if (!err.empty()) break; extra["var_" + s.name] = v.str(); f->vars.erase(s.name); f->pc++; return true; }
        case SO_SET: {
            SVal v = val(s.a); if (!err.empty()) break;
            if (s.key != "=") { SVal c = scriptValue(r, s.name); long long x = 0, y = 0;
                if (s.key == "+=" && !(c.numeric(&x) && v.numeric(&y))) v = SVal::text(c.str() + v.str());
                else { if (!c.numeric(&x) || !v.numeric(&y)) { err = "can't do " + s.key + " with text"; break; } v = SVal::num(s.key == "+=" ? x + y : x - y); } }
            scriptAssign(r, s.name, v); f->pc++; return true;
        }
        case SO_JF: { bool t = val(s.a).truthy(); if (!err.empty()) break; f->pc = t ? f->pc + 1 : s.target; return true; }
        case SO_JMP: f->pc = s.target; return true;
        case SO_FORINIT: {
            SVal v = val(s.a); if (!err.empty()) break;
            ScriptFrame::Loop L; long long n = 0;
            if (v.numeric(&n)) { if (n > 100000) { err = "for ... in a number counts to 100000 at most"; break; } for (long long i = 1; i <= n; i++) L.items.push_back(std::to_string(i)); }
            else L.items = listItems(v.s);
            f->loops[s.slot] = L; f->pc++; return true;
        }
        case SO_FORNEXT: {
            auto& L = f->loops[s.slot];
            if (L.i < L.items.size()) { f->vars[s.name] = SVal::parse(L.items[L.i++]); f->pc++; } else f->pc = s.target;
            return true;
        }
        case SO_CALL: {
            std::vector<SVal> a; for (auto& e : s.args) a.push_back(val(e)); if (!err.empty()) break;
            auto p = scriptProg(s.name);
            if (!p) { err = "there is no script '" + s.name + "'"; break; }
            if (!p->ok()) { err = fmt("script %s has an error on line %d: %s", s.name.c_str(), p->errors[0].line + 1, p->errors[0].msg.c_str()); break; }
            if ((int)r.frames.size() >= SCRIPT_CALL_LIMIT) { err = "scripts call each other too deep (a script calling itself forever?)"; break; }
            f->pc++;
            ScriptFrame nf; nf.prog = p; for (size_t i = 0; i < a.size(); i++) nf.vars["arg" + std::to_string(i + 1)] = a[i]; nf.vars["args"] = SVal::num((long long)a.size());
            r.frames.push_back(nf);
            return true;
        }
        case SO_START: {
            std::vector<SVal> a; for (auto& e : s.args) a.push_back(val(e)); if (!err.empty()) break;
            f->pc++;
            std::string id = s.name; NPC* np = scriptNpc(r);
            startScript(id, a, np);   // runs alongside; may run right now until it waits
            return true;
        }
        case SO_RETURN: {
            SVal v = s.hasA ? val(s.a) : SVal::text(""); if (!err.empty()) break;
            r.frames.pop_back();
            if (r.frames.empty()) { r.done = true; return false; }
            r.frames.back().vars["result"] = v;
            return true;
        }
        case SO_STOP: r.done = true; return false;
        case SO_SPEAKER: { SVal v = val(s.a); if (!err.empty()) break; r.speaker = v.str(); r.speakerSet = true; f->pc++; return true; }
        case SO_OPTION: {
            SVal l = val(s.a); SVal v = s.hasB ? val(s.b) : l; if (!err.empty()) break;
            if (r.options.size() >= 40) { err = "more than 40 options for one ask"; break; }
            r.options.push_back({l.str(), v}); f->pc++; return true;
        }
        case SO_SAY: case SO_ASK: {
            if (s.op == SO_ASK && r.options.empty()) { err = "ask without any option lines before it"; break; }
            std::string text = s.hasA ? val(s.a).str() : std::string(); if (!err.empty()) break;
            if (!scriptShowTalk(r, text, s.op == SO_ASK)) { r.wait = SW_IDLE; return false; }   // the box is busy: try again once the party is back on the map
            f->pc++;
            if (s.op == SO_ASK) { r.pendingVar = s.name; r.pendingKind = SP_ASK; }
            r.wait = SW_DIALOGUE; return false;
        }
        case SO_PICK: {
            std::string prompt = s.hasA ? val(s.a).str() : std::string(); if (!err.empty()) break;
            if (mode == GMode::DIALOGUE && dlgId == SCRIPT_DLG && scriptDlgOwner == r.id) popMode();
            if (mode != GMode::EXPLORE) { r.wait = SW_IDLE; return false; }
            f->pc++;
            wmPickResult.clear(); wmPickPrompt = prompt;
            openWorldMap(true, true);
            if (mode != GMode::WORLDMAP) { scriptAssign(r, s.name, SVal::text("")); return true; }
            r.pendingVar = s.name; r.pendingKind = SP_PICK; r.wait = SW_IDLE; return false;
        }
        case SO_INPUT: {   // 1.4.0 (#76/#77): the text box; resumed by textInputDone
            std::string prompt = s.hasA ? val(s.a).str() : std::string(); if (!err.empty()) break;
            if (mode == GMode::DIALOGUE && dlgId == SCRIPT_DLG && scriptDlgOwner == r.id) popMode();
            if (mode != GMode::EXPLORE) { r.wait = SW_IDLE; return false; }
            f->pc++;
            NPC* np = scriptNpc(r);
            openTextInput(r.speakerSet ? r.speaker : np ? np->name : std::string(), prompt, r.id);
            r.pendingVar = s.name; r.pendingKind = SP_INPUT; r.wait = SW_INPUT; return false;
        }
        case SO_WAIT: {
            SVal v = val(s.a); if (!err.empty()) break;
            long long ms = s.name == "ms" ? v.toNum() : v.toNum() * 1000;
            f->pc++;
            if (mode == GMode::DIALOGUE && dlgId == SCRIPT_DLG && scriptDlgOwner == r.id) popMode();   // the map shows while it waits
            r.timer = (float)ms / 1000.0f; r.wait = SW_TIMER; return false;
        }
        case SO_ACTION: {
            std::string v = trim(val(s.a).str()); if (!err.empty()) break;
            bool blocking = isScriptBlockingAction(s.key);
            if (blocking && mode == GMode::DIALOGUE && dlgId == SCRIPT_DLG && scriptDlgOwner == r.id) popMode();   // close the script's talk box first
            f->pc++;
            NPC* np = scriptNpc(r); if (np || !r.inDialogue) dlgNpc = np;   // the actions see the script's NPC (join=, combat=, dialogue=)
            runAction(s.key, v);
            if (blocking && mode != GMode::EXPLORE) { r.wait = SW_IDLE; return false; }
            return true;
        }
    }
    if (!err.empty()) { scriptFail(r, err); return false; }
    return true;
}

// once a frame (Game::update): runs waiting for the map carry on when the party is back on it
void Game::tickScripts(float dt) {
    if (scriptRuns.empty()) return;
    auto runs = scriptRuns;
    for (auto& r : runs) {
        if (r->done) continue;
        if (mode != GMode::EXPLORE) break;
        if (r->wait == SW_TIMER) { r->timer -= dt; if (r->timer > 0) continue; r->wait = SW_NONE; }
        else if (r->wait == SW_IDLE) r->wait = SW_NONE;
        else if (r->wait == SW_INPUT) {   // its text box went away some other way: as if cancelled
            r->wait = SW_NONE; if (r->pendingKind == SP_INPUT) scriptAssign(*r, r->pendingVar, SVal::text(""));
            r->pendingKind = SP_NONE; r->pendingVar.clear();
        }
        else if (r->wait == SW_DIALOGUE) {   // its box was closed some other way: carry on as if answered with nothing
            r->wait = SW_NONE; if (r->pendingKind == SP_ASK) { scriptAssign(*r, r->pendingVar, SVal::text("")); r->options.clear(); }
            r->pendingKind = SP_NONE; r->pendingVar.clear();
        } else continue;
        if (r->pendingKind == SP_PICK) { scriptAssign(*r, r->pendingVar, SVal::text(wmPickResult)); r->pendingKind = SP_NONE; r->pendingVar.clear(); }
        resumeScript(r);
    }
    sweepRuns(scriptRuns);
}

// ------------------------------------------------------------------ 1.4.0 (#76/#77): the text box
// GMode::NAME (unused since the new-game name moved to the desktop) is the box: menus.cpp draws it and reads the keys.
void Game::openTextInput(const std::string& title, const std::string& prompt, int owner) {
    inputBuf.clear(); inputTitle = title; inputPrompt = prompt; inputOwner = owner;
    pushMode(GMode::NAME);
}
void Game::textInputDone(bool ok) {
    std::string text = ok ? trim(inputBuf) : std::string();
    if (mode == GMode::NAME) popMode();
    std::shared_ptr<ScriptRun> r;
    for (auto& x : scriptRuns) if (!x->done && x->id == inputOwner && x->wait == SW_INPUT) r = x;
    inputOwner = 0;
    if (!r) return;
    r->wait = SW_NONE;
    if (r->pendingKind == SP_INPUT) scriptAssign(*r, r->pendingVar, SVal::text(text));
    r->pendingKind = SP_NONE; r->pendingVar.clear();
    resumeScript(r);
}
