// Triffall scripts (1.1.10, owner's #34: "something for scripting ... robust ... not limited to NPCs").
// A script is a text file in data/scripts/<id>.txt (a campaign's own scripts/ folder first). One statement per line:
//   let x = 5 / keep visits = visits + 1 / x += 1        variables (keep = saved with the game)
//   if / elif / else / end, while ... end, for p in places() ... end, break, continue
//   say <text>, speaker <name>, option <text> -> <value>, ask <var> <prompt>, pick <var> <prompt>, wait <seconds>
//   call <id> [args], start <id> [args], return [value], stop
//   any dialogue action: give potion:2, gold -{fare}, set met_mara, teleport overworld,10,20, dialogue elder ...
// Text in quotes or after say/option/actions can hold {expressions}. Expressions: numbers, "text", variables, flags,
// built-in values (gold, day, hour, here...), functions (cond("item:key"), random(1,6), placename(p), 1.4.0: stat("cha"),
// partystat("str")...), + - * / %,
// == != < <= > >=, and / or / not.
// compileScript turns the text into a flat list of statements with jumps (the Editor's Check uses it too); the game
// runs them in Game::resumeScript (script.cpp), pausing on say / ask / pick / wait and on actions that open a screen.
#pragma once
#include <string>
#include <vector>
#include <map>
#include <memory>

struct SVal {
    bool isNum = true; long long n = 0; std::string s;
    static SVal num(long long v) { SVal r; r.isNum = true; r.n = v; return r; }
    static SVal text(const std::string& t) { SVal r; r.isNum = false; r.s = t; return r; }
    static SVal parse(const std::string& t);          // text that is a whole number becomes a number
    std::string str() const { return isNum ? std::to_string(n) : s; }
    bool numeric(long long* out = nullptr) const;      // a number, or text that is one
    long long toNum() const { long long v = 0; numeric(&v); return v; }
    bool truthy() const { return isNum ? n != 0 : (!s.empty() && s != "0"); }
};

enum { SX_NUM, SX_STR, SX_INTERP, SX_VAR, SX_CALL, SX_NOT, SX_NEG, SX_BIN };
struct SExpr { int kind = SX_NUM; long long n = 0; std::string s; std::vector<SExpr> kids; };

enum { SO_LET, SO_KEEP, SO_SET, SO_JF, SO_JMP, SO_FORINIT, SO_FORNEXT, SO_CALL, SO_START, SO_RETURN, SO_STOP,
       SO_SAY, SO_SPEAKER, SO_OPTION, SO_ASK, SO_PICK, SO_WAIT, SO_ACTION };
struct SStmt {
    int op = 0, line = 0, target = -1, slot = -1;
    std::string name, key;           // variable / script id / action key; key also holds = += -= for SO_SET
    SExpr a, b; bool hasA = false, hasB = false;
    std::vector<SExpr> args;         // call / start arguments
};

struct ScriptMsg { int line = 0; std::string msg; };
struct ScriptProg {
    std::string id; std::vector<SStmt> code; int loops = 0;
    std::vector<ScriptMsg> errors, warnings;
    struct Ref { int line = 0; std::string key, value; };   // action lines (key, value as written) and call/start ("call", id) for the Editor
    std::vector<Ref> refs;
    bool ok() const { return errors.empty(); }
};

ScriptProg compileScript(const std::string& id, const std::string& text);
const std::vector<std::string>& scriptActionKeys();     // the actions a script line can start with (the dialogue actions + a few)
bool isScriptAction(const std::string& key);
bool isScriptBlockingAction(const std::string& key);    // opens a screen: the script waits until the party is back on the map
const std::vector<std::string>& scriptBuiltins();       // read-only values
const std::vector<std::string>& scriptFunctions();
const std::vector<std::string>& scriptKeywords();

// a running script (Game::scriptRuns)
struct ScriptFrame {
    std::shared_ptr<ScriptProg> prog; int pc = 0;
    std::map<std::string, SVal> vars;
    struct Loop { std::vector<std::string> items; size_t i = 0; };
    std::map<int, Loop> loops;
};
enum { SW_NONE, SW_IDLE, SW_DIALOGUE, SW_TIMER };
enum { SP_NONE, SP_ASK, SP_PICK };
struct ScriptRun {
    int id = 0; std::vector<ScriptFrame> frames;
    int wait = SW_NONE; float timer = 0;
    std::string pendingVar; int pendingKind = SP_NONE;
    std::string npcMap, npcId;                         // the NPC it was started from (talk / dialogue), found again by id
    std::string speaker; bool speakerSet = false;
    std::vector<std::pair<std::string, SVal>> options; // option lines waiting for the next ask
    bool done = false, inDialogue = false;             // inDialogue: started from a dialogue's action (say takes over its box)
};
