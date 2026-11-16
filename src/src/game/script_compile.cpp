// Triffall scripts: the compiler (text -> statements). No game state here, so the Editor's Check uses it as is.
// See script.h for the language; the Editor guide (guide topics script.*) is the owner's manual.
#include "script.h"
#include "../core/util.h"
#include <algorithm>
#include <cctype>
#include <cstdlib>

SVal SVal::parse(const std::string& t) { SVal v = text(t); long long n = 0; if (v.numeric(&n)) return num(n); return v; }
bool SVal::numeric(long long* out) const {
    if (isNum) { if (out) *out = n; return true; }
    std::string t = trim(s); if (t.empty() || t.size() > 18) return false;
    size_t i = (t[0] == '-' || t[0] == '+') ? 1 : 0; if (i >= t.size()) return false;
    for (size_t j = i; j < t.size(); j++) if (!isdigit((unsigned char)t[j])) return false;
    if (out) *out = atoll(t.c_str());
    return true;
}

const std::vector<std::string>& scriptActionKeys() {
    static const std::vector<std::string> k = {
        // the dialogue actions (Game::runActionInner)
        "set", "flag", "clear", "inc", "give", "item", "take", "gold", "xp", "heal", "rest", "shop", "join", "leave", "combat",
        "teleport", "message", "dialogue", "cutscene", "log", "notify", "hidenpc", "shownpc", "spawn", "quest", "craft", "board",
        "travel", "revealmap", "rep", "approve", "amends", "craftxp", "note", "win", "savepoint", "finish", "respec", "healer",
        "healsvc", "obstacle", "npcmove", "mapset",
        // 1.1.10, for scripts and dialogue alike
        "time", "sound", "encounter", "weather", "stat", "script",
        // 1.4.0 (#76/#77)
        "resetcrates", "lorefile",
        // 1.4.0
        "teach",
        // 1.5.0 (#86)
        "recipe"};
    return k;
}
bool isScriptAction(const std::string& key) { auto& k = scriptActionKeys(); return std::find(k.begin(), k.end(), key) != k.end(); }
bool isScriptBlockingAction(const std::string& key) {
    static const char* b[] = {"dialogue", "shop", "combat", "teleport", "message", "cutscene", "craft", "board", "travel", "healer", "healsvc", "respec", "win", "finish", "obstacle", "encounter"};
    for (auto s : b) if (key == s) return true;
    return false;
}
const std::vector<std::string>& scriptBuiltins() {
    static const std::vector<std::string> k = {"gold", "day", "hour", "minute", "here", "herename", "hero", "x", "y", "level", "partysize", "night", "stealth", "weather", "npc", "npcname", "won", "true", "false"};
    return k;
}
// name -> min, max arguments
static const std::map<std::string, std::pair<int, int>>& fnTable() {
    static const std::map<std::string, std::pair<int, int>> t = {
        {"flag", {1, 1}}, {"cond", {1, 1}}, {"has", {1, 1}}, {"visited", {1, 1}}, {"inparty", {1, 1}}, {"rep", {1, 1}}, {"approval", {1, 1}}, {"kills", {1, 1}},
        {"random", {1, 2}}, {"chance", {1, 1}}, {"places", {0, 0}}, {"placename", {1, 1}}, {"placemap", {1, 1}}, {"placex", {1, 1}}, {"placey", {1, 1}},
        {"distance", {1, 1}}, {"leagues", {1, 1}}, {"mapname", {1, 1}}, {"itemname", {1, 1}}, {"count", {1, 1}}, {"at", {2, 2}}, {"contains", {2, 2}},
        {"add", {2, 2}}, {"len", {1, 1}}, {"upper", {1, 1}}, {"lower", {1, 1}}, {"find", {2, 2}}, {"min", {2, 2}}, {"max", {2, 2}}, {"abs", {1, 1}}, {"var", {1, 1}},
        {"mid", {2, 3}}, {"squash", {1, 1}}, {"plates", {0, 1}}, {"platesdown", {0, 1}}, {"crateat", {2, 2}}, {"lorefile", {1, 1}},   // 1.4.0
        {"stat", {1, 1}}, {"partystat", {1, 1}}};   // 1.4.0 (#73)
    return t;
}
const std::vector<std::string>& scriptFunctions() { static std::vector<std::string> k; if (k.empty()) for (auto& f : fnTable()) k.push_back(f.first); return k; }
const std::vector<std::string>& scriptKeywords() {
    static const std::vector<std::string> k = {"let", "keep", "if", "elif", "elseif", "else", "end", "endif", "endwhile", "endfor", "while", "for", "break", "continue", "stop", "return", "call", "start", "say", "speaker", "option", "ask", "pick", "wait", "and", "or", "not", "in", "input"};
    return k;
}
static bool isKeyword(const std::string& w) { auto& k = scriptKeywords(); return std::find(k.begin(), k.end(), w) != k.end(); }
static bool isBuiltin(const std::string& w) { auto& k = scriptBuiltins(); return std::find(k.begin(), k.end(), w) != k.end(); }
static bool identStart(char c) { return isalpha((unsigned char)c) || c == '_'; }
static bool identChar(char c) { return isalnum((unsigned char)c) || c == '_'; }

// ------------------------------------------------------------------ expressions
namespace {
struct Tok { int t = 0; std::string s; long long n = 0; };   // t: 0 end, 1 number, 2 text, 3 name, 4 operator
bool tokenize(const std::string& src, std::vector<Tok>& out, std::string& err) {
    size_t i = 0;
    while (i < src.size()) {
        char c = src[i];
        if (isspace((unsigned char)c)) { i++; continue; }
        if (isdigit((unsigned char)c)) { size_t j = i; while (j < src.size() && isdigit((unsigned char)src[j])) j++;
            if (j < src.size() && src[j] == '.') { err = "whole numbers only (" + src.substr(i, j - i + 2) + "...)"; return false; }
            Tok t; t.t = 1; t.n = atoll(src.substr(i, j - i).c_str()); out.push_back(t); i = j; continue; }
        if (c == '"' || c == '\'') { char q = c; std::string s; size_t j = i + 1; bool closed = false;
            while (j < src.size()) { char d = src[j]; if (d == '\\' && j + 1 < src.size()) { char e = src[j + 1]; s += e == 'n' ? '\n' : e; j += 2; continue; } if (d == q) { closed = true; j++; break; } s += d; j++; }
            if (!closed) { err = "text in quotes isn't closed"; return false; }
            Tok t; t.t = 2; t.s = s; out.push_back(t); i = j; continue; }
        if (identStart(c)) { size_t j = i; while (j < src.size() && identChar(src[j])) j++; Tok t; t.t = 3; t.s = toLower(src.substr(i, j - i)); out.push_back(t); i = j; continue; }
        static const char* two[] = {"==", "!=", "<=", ">=", "&&", "||", "<>"};
        bool got = false;
        for (auto op : two) if (src.compare(i, 2, op) == 0) { Tok t; t.t = 4; t.s = op; if (t.s == "<>") t.s = "!="; out.push_back(t); i += 2; got = true; break; }
        if (got) continue;
        if (std::string("+-*/%()<>=!,").find(c) != std::string::npos) { Tok t; t.t = 4; t.s = std::string(1, c); out.push_back(t); i++; continue; }
        err = std::string("unexpected '") + c + "'"; return false;
    }
    out.push_back(Tok());
    return true;
}

SExpr parseInterp(const std::string& raw, std::string& err);

struct Parser {
    std::vector<Tok> t; size_t p = 0; std::string err;
    const Tok& cur() const { return t[p]; }
    bool isOp(const char* s) const { return cur().t == 4 && cur().s == s; }
    bool isWord(const char* s) const { return cur().t == 3 && cur().s == s; }
    void fail(const std::string& m) { if (err.empty()) err = m; }
    SExpr bin(const std::string& op, SExpr a, SExpr b) { SExpr e; e.kind = SX_BIN; e.s = op; e.kids.push_back(std::move(a)); e.kids.push_back(std::move(b)); return e; }
    SExpr parseOr() { SExpr a = parseAnd(); while (err.empty() && (isWord("or") || isOp("||"))) { p++; a = bin("or", std::move(a), parseAnd()); } return a; }
    SExpr parseAnd() { SExpr a = parseNot(); while (err.empty() && (isWord("and") || isOp("&&"))) { p++; a = bin("and", std::move(a), parseNot()); } return a; }
    SExpr parseNot() { if (isWord("not") || isOp("!")) { p++; SExpr e; e.kind = SX_NOT; e.kids.push_back(parseNot()); return e; } return parseCmp(); }
    SExpr parseCmp() {
        SExpr a = parseAdd();
        while (err.empty() && cur().t == 4 && (cur().s == "==" || cur().s == "=" || cur().s == "!=" || cur().s == "<" || cur().s == "<=" || cur().s == ">" || cur().s == ">=")) {
            std::string op = cur().s == "=" ? "==" : cur().s; p++; a = bin(op, std::move(a), parseAdd()); }
        return a;
    }
    SExpr parseAdd() { SExpr a = parseMul(); while (err.empty() && (isOp("+") || isOp("-"))) { std::string op = cur().s; p++; a = bin(op, std::move(a), parseMul()); } return a; }
    SExpr parseMul() { SExpr a = parseUnary(); while (err.empty() && (isOp("*") || isOp("/") || isOp("%"))) { std::string op = cur().s; p++; a = bin(op, std::move(a), parseUnary()); } return a; }
    SExpr parseUnary() { if (isOp("-")) { p++; SExpr e; e.kind = SX_NEG; e.kids.push_back(parseUnary()); return e; } if (isOp("+")) { p++; return parseUnary(); } return parsePrimary(); }
    SExpr parsePrimary() {
        SExpr e; const Tok& k = cur();
        if (k.t == 1) { e.kind = SX_NUM; e.n = k.n; p++; return e; }
        if (k.t == 2) { std::string ie; e = parseInterp(k.s, ie); if (!ie.empty()) fail(ie); p++; return e; }
        if (k.t == 3) {
            std::string name = k.s; p++;
            if (isOp("(")) {
                p++; e.kind = SX_CALL; e.s = name;
                if (!isOp(")")) for (;;) { e.kids.push_back(parseOr()); if (!err.empty()) return e; if (isOp(",")) { p++; continue; } break; }
                if (!isOp(")")) { fail("missing ) after the values of " + name + "("); return e; }
                p++;
                auto it = fnTable().find(name);
                if (it == fnTable().end()) fail("unknown function " + name + "()");
                else if ((int)e.kids.size() < it->second.first || (int)e.kids.size() > it->second.second) fail(fmt("%s() takes %d value%s", name.c_str(), it->second.second, it->second.second == 1 ? "" : "s"));
                return e;
            }
            if (name == "and" || name == "or" || name == "not" || name == "in") { fail("'" + name + "' needs something before or after it"); return e; }
            e.kind = SX_VAR; e.s = name; return e;
        }
        if (k.t == 4 && k.s == "(") { p++; e = parseOr(); if (!isOp(")")) fail("missing )"); else p++; return e; }
        if (k.t == 0) fail("something is missing at the end"); else fail("unexpected '" + k.s + "'");
        return e;
    }
};
bool parseExprText(const std::string& src, SExpr& out, std::string& err) {
    Parser ps; if (!tokenize(src, ps.t, err)) return false;
    if (ps.t.size() == 1) { err = "a value is missing"; return false; }
    out = ps.parseOr();
    if (ps.err.empty() && ps.cur().t != 0) ps.fail("unexpected '" + ps.cur().s + "' (an operator missing?)");
    if (!ps.err.empty()) { err = ps.err; return false; }
    return true;
}
bool parseExprList(const std::string& src, std::vector<SExpr>& out, std::string& err) {
    Parser ps; if (!tokenize(src, ps.t, err)) return false;
    if (ps.t.size() == 1) return true;
    for (;;) { out.push_back(ps.parseOr()); if (!ps.err.empty()) break; if (ps.isOp(",")) { ps.p++; continue; } break; }
    if (ps.err.empty() && ps.cur().t != 0) ps.fail("unexpected '" + ps.cur().s + "'");
    if (!ps.err.empty()) { err = ps.err; return false; }
    return true;
}
// text with {expression} parts ({{ and }} are plain braces)
SExpr parseInterp(const std::string& raw, std::string& err) {
    SExpr e; e.kind = SX_INTERP; std::string lit;
    auto flush = [&]() { if (!lit.empty()) { SExpr s; s.kind = SX_STR; s.s = lit; e.kids.push_back(s); lit.clear(); } };
    for (size_t i = 0; i < raw.size(); i++) {
        char c = raw[i];
        if (c == '{' && i + 1 < raw.size() && raw[i + 1] == '{') { lit += '{'; i++; continue; }
        if (c == '}' && i + 1 < raw.size() && raw[i + 1] == '}') { lit += '}'; i++; continue; }
        if (c == '{') {
            size_t j = i + 1; char q = 0; while (j < raw.size() && (q || raw[j] != '}')) { if (q) { if (raw[j] == q) q = 0; } else if (raw[j] == '"' || raw[j] == '\'') q = raw[j]; j++; }
            if (j >= raw.size()) { err = "a { has no matching }"; return e; }
            flush(); SExpr x; std::string xe; if (!parseExprText(raw.substr(i + 1, j - i - 1), x, xe)) { err = "in {" + raw.substr(i + 1, j - i - 1) + "}: " + xe; return e; }
            e.kids.push_back(x); i = j; continue;
        }
        lit += c;
    }
    flush();
    if (e.kids.empty()) { SExpr s; s.kind = SX_STR; return s; }
    if (e.kids.size() == 1 && e.kids[0].kind == SX_STR) return e.kids[0];
    return e;
}
// the text after say / speaker / option: "quoted" = an expression, anything else = plain text with {expressions}
bool parseTextArg(const std::string& rest, SExpr& out, std::string& err) {
    std::string r = trim(rest);
    if (!r.empty() && (r[0] == '"' || r[0] == '\'')) return parseExprText(r, out, err);
    out = parseInterp(r, err); return err.empty();
}
// where " -> " splits an option line (outside quotes and braces)
size_t arrowPos(const std::string& s) {
    char q = 0; int brace = 0; size_t found = std::string::npos;
    for (size_t i = 0; i + 1 < s.size(); i++) {
        char c = s[i];
        if (q) { if (c == q) q = 0; continue; }
        if (c == '"' || c == '\'') { if (i == 0 || brace) q = c; continue; }   // quotes count only around a whole value or inside {}
        if (c == '{') brace++; else if (c == '}') brace = std::max(0, brace - 1);
        else if (!brace && c == '-' && s[i + 1] == '>') found = i;
    }
    return found;
}
std::string stripComment(const std::string& l) {
    char q = 0; int brace = 0;   // only "double quotes" hide a #: a lone ' in plain text (I'm) must not
    for (size_t i = 0; i < l.size(); i++) {
        char c = l[i];
        if (q) { if (c == q) q = 0; continue; }
        if (c == '"') q = c;
        else if (c == '{') brace++; else if (c == '}') brace = std::max(0, brace - 1);
        else if (c == '#' && !brace && (i == 0 || isspace((unsigned char)l[i - 1]))) return l.substr(0, i);
    }
    return l;
}
}

// ------------------------------------------------------------------ statements
ScriptProg compileScript(const std::string& id, const std::string& text) {
    ScriptProg P; P.id = id;
    struct Block { int kind; int line; int pendingJF = -1; std::vector<int> endJumps, breaks; int start = 0, forStmt = -1; bool hasElse = false; };   // kind 0 if, 1 while, 2 for
    std::vector<Block> stack;
    auto lines = split(text, '\n');
    auto err = [&](int ln, const std::string& m) { P.errors.push_back({ln, m}); };
    auto emit = [&](SStmt s) { P.code.push_back(std::move(s)); return (int)P.code.size() - 1; };
    auto nameOk = [&](int ln, const std::string& n, bool forLet) -> bool {
        if (n.empty() || !identStart(n[0])) { err(ln, "a variable name is missing (letters, digits and _)"); return false; }
        for (char c : n) if (!identChar(c)) { err(ln, "'" + n + "' isn't a name (letters, digits and _ only)"); return false; }
        if (isKeyword(n)) { err(ln, "'" + n + "' is a script word; pick another name"); return false; }
        if (forLet && isBuiltin(n)) { err(ln, "'" + n + "' is a built-in value (read only); pick another name"); return false; }
        if (forLet && isScriptAction(n)) { err(ln, "'" + n + "' is the name of an action; pick another name"); return false; }
        return true;
    };
    for (size_t li = 0; li < lines.size(); li++) {
        int ln = (int)li;
        std::string l = trim(stripComment(lines[li]));
        if (l.empty()) continue;
        size_t w = 0; while (w < l.size() && (identChar(l[w]) || l[w] == '-')) w++;
        std::string word = toLower(l.substr(0, w)), rest = trim(l.substr(w));
        if (word.empty()) { err(ln, "a line should start with a command, an action or a variable name"); continue; }
        SStmt s; s.line = ln;
        std::string e;
        auto expr = [&](const std::string& src, SExpr& out) { std::string x; if (!parseExprText(src, out, x)) { err(ln, x); return false; } return true; };
        if (word == "let" || word == "keep") {
            size_t eq = rest.find('='); std::string n = toLower(trim(rest.substr(0, eq)));
            if (eq == std::string::npos) { err(ln, word + " needs a value: " + word + " name = value"); continue; }
            if (!nameOk(ln, n, true)) continue;
            s.op = word == "let" ? SO_LET : SO_KEEP; s.name = n; if (!expr(rest.substr(eq + 1), s.a)) continue; s.hasA = true; emit(s);
        } else if (word == "if" || word == "while") {
            if (rest.empty()) { err(ln, word + " needs a condition"); continue; }
            Block b; b.kind = word == "if" ? 0 : 1; b.line = ln; b.start = (int)P.code.size();
            s.op = SO_JF; if (!expr(rest, s.a)) { s.a = SExpr(); } s.hasA = true; b.pendingJF = emit(s); stack.push_back(b);
        } else if (word == "elif" || word == "elseif") {
            if (stack.empty() || stack.back().kind != 0) { err(ln, word + " without an if"); continue; }
            Block& b = stack.back(); if (b.hasElse) { err(ln, word + " after else"); continue; }
            SStmt j; j.op = SO_JMP; j.line = ln; b.endJumps.push_back(emit(j));
            if (b.pendingJF >= 0) P.code[b.pendingJF].target = (int)P.code.size();
            s.op = SO_JF; if (!expr(rest, s.a)) s.a = SExpr(); s.hasA = true; b.pendingJF = emit(s);
        } else if (word == "else") {
            if (stack.empty() || stack.back().kind != 0) { err(ln, "else without an if"); continue; }
            Block& b = stack.back(); if (b.hasElse) { err(ln, "two elses in one if"); continue; }
            SStmt j; j.op = SO_JMP; j.line = ln; b.endJumps.push_back(emit(j));
            if (b.pendingJF >= 0) P.code[b.pendingJF].target = (int)P.code.size();
            b.pendingJF = -1; b.hasElse = true;
        } else if (word == "end" || word == "endif" || word == "endwhile" || word == "endfor") {
            if (stack.empty()) { err(ln, "end without an if, while or for"); continue; }
            Block b = stack.back(); stack.pop_back();
            if (b.kind != 0) { SStmt j; j.op = SO_JMP; j.line = ln; j.target = b.start; emit(j); }
            int here = (int)P.code.size();
            if (b.pendingJF >= 0) P.code[b.pendingJF].target = here;
            if (b.forStmt >= 0) P.code[b.forStmt].target = here;
            for (int j : b.endJumps) P.code[j].target = here;
            for (int j : b.breaks) P.code[j].target = here;
        } else if (word == "for") {
            // for NAME in EXPR
            size_t in = std::string::npos; { std::string lr = toLower(rest); size_t pos = 0; while ((pos = lr.find(" in ", pos)) != std::string::npos) { in = pos; break; } }
            if (in == std::string::npos) { err(ln, "for needs: for name in list"); continue; }
            std::string n = toLower(trim(rest.substr(0, in))); if (!nameOk(ln, n, true)) continue;
            SStmt init; init.op = SO_FORINIT; init.line = ln; init.slot = P.loops++; if (!expr(rest.substr(in + 4), init.a)) continue; init.hasA = true; emit(init);
            Block b; b.kind = 2; b.line = ln; b.start = (int)P.code.size();
            SStmt nx; nx.op = SO_FORNEXT; nx.line = ln; nx.slot = init.slot; nx.name = n; b.forStmt = emit(nx); stack.push_back(b);
        } else if (word == "break" || word == "continue") {
            int bi = -1; for (int i = (int)stack.size() - 1; i >= 0; i--) if (stack[i].kind != 0) { bi = i; break; }
            if (bi < 0) { err(ln, word + " outside a while or for"); continue; }
            SStmt j; j.op = SO_JMP; j.line = ln; if (word == "continue") j.target = stack[bi].start; int at = emit(j); if (word == "break") stack[bi].breaks.push_back(at);
        } else if (word == "stop") { s.op = SO_STOP; emit(s);
        } else if (word == "return") { s.op = SO_RETURN; if (!rest.empty()) { if (!expr(rest, s.a)) continue; s.hasA = true; } emit(s);
        } else if (word == "call" || word == "start") {
            size_t k = 0; while (k < rest.size() && (identChar(rest[k]) || rest[k] == '-')) k++;
            std::string sid = rest.substr(0, k); if (sid.empty()) { err(ln, word + " needs the id of a script"); continue; }
            std::string a = trim(rest.substr(k)); if (!a.empty() && a[0] == ',') a = a.substr(1);
            s.op = word == "call" ? SO_CALL : SO_START; s.name = sid;
            { std::string x; if (!parseExprList(a, s.args, x)) { err(ln, x); continue; } }
            P.refs.push_back({ln, "call", sid}); emit(s);
        } else if (word == "say" || word == "speaker") {
            if (word == "say" && rest.empty()) { err(ln, "say needs something to say"); continue; }
            s.op = word == "say" ? SO_SAY : SO_SPEAKER; std::string x; if (!parseTextArg(rest, s.a, x)) { err(ln, x); continue; } s.hasA = true; emit(s);
        } else if (word == "option") {
            size_t ar = arrowPos(rest); std::string lab = ar == std::string::npos ? rest : trim(rest.substr(0, ar));
            if (lab.empty()) { err(ln, "option needs the text of the answer"); continue; }
            s.op = SO_OPTION; std::string x; if (!parseTextArg(lab, s.a, x)) { err(ln, x); continue; } s.hasA = true;
            if (ar != std::string::npos) { if (!expr(rest.substr(ar + 2), s.b)) continue; s.hasB = true; }
            emit(s);
        } else if (word == "ask" || word == "pick" || word == "input") {
            size_t k = 0; while (k < rest.size() && identChar(rest[k])) k++;
            std::string n = toLower(rest.substr(0, k)); if (!nameOk(ln, n, true)) continue;
            s.op = word == "ask" ? SO_ASK : word == "input" ? SO_INPUT : SO_PICK; s.name = n;
            std::string pr = trim(rest.substr(k)); if (!pr.empty()) { std::string x; if (!parseTextArg(pr, s.a, x)) { err(ln, x); continue; } s.hasA = true; }
            emit(s);
        } else if (word == "wait") {
            if (rest.empty()) { err(ln, "wait needs a number of seconds"); continue; }
            s.op = SO_WAIT;
            char* endp = nullptr; double secs = strtod(rest.c_str(), &endp);
            if (endp && trim(endp).empty()) { s.a.kind = SX_NUM; s.a.n = (long long)(secs * 1000 + 0.5); s.name = "ms"; }
            else if (!expr(rest, s.a)) continue;
            s.hasA = true; emit(s);
        } else if (isScriptAction(word)) {
            std::string v = rest; if (!v.empty() && (v[0] == '=' || v[0] == ':')) v = trim(v.substr(1));
            s.op = SO_ACTION; s.key = word; std::string x; s.a = parseInterp(v, x); if (!x.empty()) { err(ln, x); continue; } s.hasA = true;
            if (word == "script" && !v.empty()) { std::string sid = trim(split(v, ',')[0]); if (sid.find('{') == std::string::npos) P.refs.push_back({ln, "call", sid}); }
            P.refs.push_back({ln, word, v}); emit(s);
        } else if (!rest.empty() && (rest[0] == '=' || startsWith(rest, "+=") || startsWith(rest, "-=")) && !startsWith(rest, "==")) {
            std::string n = word; if (!nameOk(ln, n, true)) continue;
            s.op = SO_SET; s.name = n; s.key = rest[0] == '=' ? "=" : rest.substr(0, 2);
            if (!expr(rest.substr(s.key.size()), s.a)) continue; s.hasA = true; emit(s);
        } else if (word == "then" || word == "do") {
            err(ln, "no 'then' / 'do' needed: the block simply runs to its end");
        } else {
            err(ln, "unknown command '" + word + "' (not a script command, a game action or name = value)");
        }
    }
    for (auto& b : stack) err(b.line, std::string(b.kind == 0 ? "if" : b.kind == 1 ? "while" : "for") + " without an end");
    return P;
}
