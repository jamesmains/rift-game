// Developer console (~): the headless test commands, available in the running game.
#include "game.h"
#include "../core/audio.h"
#include "tiles.h"
#include "../core/crash.h"
#include <algorithm>

using namespace UI;

void Game::runConsole(const std::string& line) {
    std::string cmd = trim(line); if (cmd.empty()) return;
    consoleHist.push_back(cmd); if (consoleHist.size() > 40) consoleHist.erase(consoleHist.begin()); consoleHistPos = -1;
    consoleOut.push_back("> " + cmd);
    auto out = [&](const std::string& s) { consoleOut.push_back(s); };
    auto p = split(cmd, ' '); std::string c = toLower(p[0]);
    auto arg = [&](size_t i, const std::string& d = "") { return i < p.size() ? p[i] : d; };
    auto argi = [&](size_t i, int d = 0) { return i < p.size() ? atoi(p[i].c_str()) : d; };
    // 1.4.0 (kept out of the long else-if chain below): stealth [N] = sneaking steps (as Camouflage); setstat STAT N [MEMBER] =
    // a party member's base stat (stat checks in dialogue); spellsof [MEMBER] = what they know
    if (c == "light") { lightSteps = std::max(0, argi(1, 200)); if (lightSteps > 0) lightPower = std::max(lightPower, 5); revealAround(); out(fmt("light = %d steps", lightSteps)); return; }   // 1.5.0 (#99): a torch without the item
    if (c == "stealth") { stealthSteps = std::max(0, argi(1, 200)); out(fmt("stealth = %d steps", stealthSteps)); return; }
    if (c == "setstat") { if (party.members.empty()) return; int mi = clampi(argi(3, 0), 0, (int)party.members.size() - 1); std::string nm = toUpper(arg(1)); int st = -1; for (int i = 0; i < ST_COUNT; i++) if (nm == statName(i)) st = i;
        if (st < 0) { out("usage: setstat str|dex|con|int|wis|cha N [MEMBER]"); return; } party.members[mi].baseStats[st] = argi(2, 10); out(fmt("%s %s = %d (with gear %d)", party.members[mi].name.c_str(), statName(st), party.members[mi].baseStats[st], party.members[mi].stat(st, *gd))); return; }
    if (c == "spellsof") { if (party.members.empty()) return; int mi = clampi(argi(1, 0), 0, (int)party.members.size() - 1); std::string s; for (auto& sp : party.members[mi].spells) s += sp + " "; out(party.members[mi].name + ": " + (s.empty() ? "(none)" : s)); return; }
    if (c == "help") {   // one topic per line; drawConsole wraps anything wider than the screen
        out("world: teleport MAP [X Y] | maps | pos | reveal | worldmap MODE | sky [0|1] | time HH:MM | day N | weather clear|rain|storm|fog|snow");
        out("party: give ITEM [N] | gold N | xp N | level N | heal | hurt N [poison] | god | auto | spells | learn SPELL [MEMBER] | join CLASS | item?");
        out("1.4.0: stealth [N] | setstat STAT N [MEMBER] | spellsof [MEMBER] | run teach:bram:SPELL");
        out("story: flag NAME [V] | unflag NAME | flags | approve CLASS N | rep [FACTION N] | questfail QUEST COND|TEXT | achieve ID | reset_achievements");
        out("fights: spawn CREATURE[,CREATURE] | kill | touch [0|1] | combatmusic [ID|-] | cast SPELL [DX DY] | bestiary CREATURE N");
        out("screens: bounty [TOWN] | bounties | craft [STATION] | cskill STATION XP | shop ID | dialogue ID [NODE] | run SCRIPT");
        out("misc: stats | hint | doors | demo 0|1 | autosave | crashtest | reload | clear");
    }
    else if (c == "teleport" || c == "tp") { if (p.size() < 2) { out("usage: teleport MAP [X Y]"); return; } modeStack.clear(); mode = GMode::EXPLORE; enterMap(arg(1), argi(2), argi(3), true); out("-> " + mapId + fmt(" %d,%d", px, py)); pushMode(GMode::CONSOLE); }
    else if (c == "give") { std::string id = arg(1); if (!gd->item(id)) { out("unknown item '" + id + "' (ids: see assets/data/items.txt)"); return; } giveItem(id, argi(2, 1)); out("ok"); }
    else if (c == "gold") { party.gold = std::max(0, party.gold + argi(1)); out(fmt("gold = %d", party.gold)); }
    else if (c == "xp") { giveXP(argi(1)); out("ok"); }
    else if (c == "level") { int target = argi(1, 1); for (auto& m : party.members) while (m.level < target && m.level < 30) { m.xp += m.xpToNext(); checkLevelUps(); } out("ok (level-up screens follow)"); }
    else if (c == "flag") { setFlag(arg(1), argi(2, 1)); out("ok"); }
    else if (c == "unflag") { flags.erase(arg(1)); out("ok"); }
    else if (c == "flags") { std::string s; for (auto& f : flags) s += f.first + "=" + std::to_string(f.second) + " "; out(s.empty() ? "(none)" : s); }
    else if (c == "god") { godMode = !godMode; out(godMode ? "god mode ON" : "god mode off"); }
    else if (c == "auto") { autoCombat = !autoCombat; out(autoCombat ? "the party fights on its own" : "manual combat"); }
    else if (c == "heal") { for (auto& m : party.members) m.fullHeal(*gd); out("party healed"); }
    else if (c == "time") { auto t = split(arg(1, "8:00"), ':'); clockMin = clampi(atoi(t[0].c_str()), 0, 23) * 60 + (t.size() > 1 ? clampi(atoi(t[1].c_str()), 0, 59) : 0); out(clockText()); }
    else if (c == "day") { day = std::max(1, argi(1, 1)); out(clockText()); }
    else if (c == "weather") { weather = arg(1, "clear"); weatherSteps = 9999; precip.clear(); out("weather = " + weather); }
    else if (c == "spawn") { if (!map) return; std::vector<std::string> cs; for (auto& s : split(arg(1), ',')) if (gd->creature(trim(s))) cs.push_back(trim(s)); if (cs.empty()) { out("unknown creature"); return; } popMode(); startCombat(cs, px + 2, py, argi(2, 0), {}, ""); out("fight!"); }
    else if (c == "kill") { if (mode == GMode::COMBAT || (!modeStack.empty() && modeStack.back() == GMode::COMBAT)) { std::string ds; for (auto& u : cb.units) if (!u.friendly() && u.ch.alive) { u.ch.hp = 0; u.ch.alive = false; onKill(u.ch, nullptr); if (ds.empty()) ds = deathSound(u.ch, false); } if (!ds.empty()) gAudio.play(ds); out("enemies slain"); } else out("not in combat"); }
    else if (c == "spells") { std::string s; for (auto& sp : gd->spells) s += sp.first + " "; out(s); }
    else if (c == "testspell") { if (p.size() < 3) { out("testspell ID UTILITY [COST] [MEMBER]: a made-up spell with that utility= (tests)"); return; } SpellDef d; d.id = arg(1); d.name = arg(1); d.utility = arg(2); d.cost = argi(3, 3); d.target = "self"; d.dmgMax = 0; gd->spells[d.id] = d; int mi = clampi(argi(4, 0), 0, (int)party.members.size() - 1); party.members[mi].spells.push_back(d.id); out("ok"); }   // 1.4.0: test-only utility spells
    else if (c == "clearmap") { Map* cm = p.size() > 1 ? loadMap(arg(1)) : map; int n = 0; if (cm) for (auto& e : cm->enemies) if (!e.defeated && (p.size() < 3 || e.flag != arg(2))) { e.defeated = true; n++; } out(fmt("%d groups removed (this visit)", n)); }   // clearmap [MAP] [KEEPFLAG]: before teleporting there (1.5.0: the group with flag KEEPFLAG stays)   // 1.4.0: tests walk dungeons without fights
    else if (c == "castout") { int mi = clampi(argi(2, 0), 0, (int)party.members.size() - 1); if (!gd->spell(arg(1))) { out("unknown spell"); return; } popMode(); out(castUtility(mi, arg(1), mi) ? "cast" : "nothing happened"); }   // 1.4.0: a spell cast from the menu outside a fight
    else if (c == "learn") { if (!gd->spell(arg(1))) { out("unknown spell"); return; } int mi = clampi(argi(2, 0), 0, (int)party.members.size() - 1); party.members[mi].spells.push_back(arg(1)); out("ok"); }
    else if (c == "join") { runAction("join", arg(1)); out("ok"); }
    else if (c == "questfail") {   // questfail QUEST COND | TEXT  - add a fail condition for this session (testing branches)
        std::string rest; for (size_t i = 2; i < p.size(); i++) rest += (rest.empty() ? "" : " ") + p[i]; auto fp = split(rest, '|'); bool ok = false;
        for (auto& q : quests) if (q.id == arg(1) && !trim(fp[0]).empty()) { q.fails.push_back({trim(fp[0]), fp.size() > 1 ? trim(fp[1]) : "The chance has passed."}); ok = true; }
        out(ok ? "fail condition added: " + questStatus(*std::find_if(quests.begin(), quests.end(), [&](const QuestDef& q) { return q.id == arg(1); })) : "usage: questfail QUEST COND | TEXT"); }
    else if (c == "cast") {   // cast SPELL [DX DY]: in a fight, whoever's turn it is casts at a tile relative to itself
        if (!inCombat() || cb.selectedUnit < 0) { out("only in combat"); return; } auto& u = cb.units[cb.selectedUnit]; if (!gd->spell(arg(1))) { out("unknown spell"); return; }
        combatCast(cb.selectedUnit, arg(1), u.x + argi(2), u.y + argi(3)); out("cast " + arg(1)); }
    else if (c == "hint") { int h = currentHint(); out(h >= 0 ? fmt("hint %d/%d: %s", h + 1, (int)hintDefs.size(), keyText(hintDefs[h].text).c_str()) : std::string("no hint showing")); }
    else if (c == "rep") { if (arg(1).empty()) { for (auto& r : reputation) out(fmt("%s = %d (%s)", r.first.c_str(), r.second, repTier(r.second))); } else { changeRep(arg(1), argi(2, 10)); out(fmt("%s reputation = %d", arg(1).c_str(), reputation[arg(1)])); } }
    else if (c == "approve") { approval[arg(1)] += argi(2, 1); out(fmt("%s approval = %d", arg(1).c_str(), approval[arg(1)])); }
    else if (c == "hurt") { int n = atoi(arg(1).c_str()); for (auto& m : party.members) { m.hp = std::max(1, m.hp - n); if (arg(2) == "poison") m.addEffect("poison", 5, 2); } out("ouch"); }
    else if (c == "doors") { if (map) { const auto& k = allTileKinds(); for (int y = 0; y < map->h; y++) for (int x = 0; x < map->w; x++) { const Cell& ce = map->at(x, y); if (ce.kind < k.size() && startsWith(k[ce.kind].name, "door")) { const auto& d = doorShop(x, y); std::string h; bool open = d.first.empty() || shopOpen(d.first, h); out(fmt("%d,%d %s %s (%d cells)", x, y, d.first.empty() ? "-" : d.first.c_str(), d.first.empty() ? "" : (open ? "open" : "LOCKED"), (int)d.second.size())); } } } }
    else if (c == "bestiary") { if (gd->creature(arg(1))) { bestiary[arg(1)] = std::max(1, atoi(arg(2).c_str())); out("ok"); } else out("unknown creature"); }
    else if (c == "bounties") { out(fmt("%d boards:", (int)boards.size())); for (auto& b : boards) out("  board " + b.id + " (map " + b.map + ")"); for (auto& b : bountyOffers) out("  offer " + b.town + " " + b.kind + " " + b.target + (b.title.empty() ? "" : " '" + b.title + "'")); for (auto& b : bounties) out("  taken " + b.town + " " + b.kind + " " + b.target); }
    else if (c == "combatmusic") { if (!map) return; if (!arg(1).empty()) map->combatMusic = arg(1) == "-" ? "" : arg(1); out("combatmusic = " + (map->combatMusic.empty() ? std::string("(campaign: ") + (campaign.combatMusic.empty() ? "combat" : campaign.combatMusic) + ")" : map->combatMusic) + " - this visit; map header combatmusic="); }   // 1.3.1 (#47)
    else if (c == "sky") { if (!map) return; if (!arg(1).empty()) { map->sky = argi(1) != 0 ? 1 : 0; computeLight(); } out(std::string("sky = ") + (map->skyOpen() ? "1 (open: outside the light keeps the night tint)" : "0 (enclosed: outside the light is pitch black)") + " - this visit; map header sky=1|0"); }   // 1.8.1 (#166)
    else if (c == "touch") { if (!map) return; if (!arg(1).empty()) map->touchCombat = argi(1) != 0; out(std::string("touch = ") + (map->touchCombat ? "1 (fights start when a group touches the party)" : "0 (groups attack within aggro range)") + " - this visit; map header touch=1"); }
    else if (c == "worldmap") { if (!arg(1).empty()) campaign.worldMap = arg(1); out("world map = " + campaign.worldMap + " (overworld | off | current | MAP_ID; campaign.txt worldmap=)"); }
    else if (c == "reveal") { if (map) { for (auto& cell : map->cells) cell.explored = true; } out("map revealed"); }
    else if (c == "achieve") { unlockAchievement(arg(1)); out("ok"); }
    else if (c == "reset_achievements") { unlocked.clear(); saveAchievementState(); out("achievements reset"); }
    else if (c == "reset_fieldnotes") { fieldNotes.clear(); saveFieldNotes(); out("field notes cleared"); }   // 1.5.0 (#96)
    else if (c == "reset_files") { foundFiles.clear(); writeFile(saveDir + "/files.ini", "[found]\n"); out("desktop files locked again"); }   // 1.4.0 (#77)
    else if (c == "bounty") { popMode(); openBoard(arg(1, lastTownMap.empty() ? "hollowmere" : lastTownMap)); }
    else if (c == "craft") { popMode(); openCraft(arg(1, "all")); }
    else if (c == "shop") { popMode(); runAction("shop", arg(1, "hollowmere_general")); }
    else if (c == "dialogue") { popMode(); startDialogue(arg(1), nullptr, arg(2, "start")); }
    else if (c == "run") { popMode(); runScript(cmd.substr(cmd.find(' ') + 1)); }
    // 1.1.10: script CODE - a script on one line, \n between statements (tests); scripts - what is running; var NAME - a kept variable
    else if (c == "script") { std::string code = cmd.find(' ') == std::string::npos ? "" : cmd.substr(cmd.find(' ') + 1); size_t pos; while ((pos = code.find("\\n")) != std::string::npos) code.replace(pos, 2, "\n"); popMode(); startScriptText(code); }
    else if (c == "scripts") { std::string s; for (auto& r : scriptRuns) if (!r->done && !r->frames.empty()) s += r->frames.back().prog->id + fmt("(wait %d) ", r->wait); out(s.empty() ? "(none running)" : s); if (!lastScriptError.empty()) out("last error: " + lastScriptError); }
    else if (c == "maps") { std::string s; for (auto& l : locations) s += l.mapId + " "; out(s); }
    else if (c == "stats") { out(fmt("kills %d fights %d gold+ %d chests %d crits %d crafted %d bounties %d travels %d steps %d", stats.kills, stats.fightsWon, stats.goldEarned, stats.chests, stats.crits, stats.crafted, stats.bounties, stats.travels, stepCounter)); }
    else if (c == "reload") { reloadContent(); out("content reloaded"); pushMode(GMode::CONSOLE); }
    else if (c == "clear") consoleOut.clear();
    else if (c == "cskill") { if (arg(1).empty()) { for (const char* st : {"field", "forge", "alchemy", "workbench"}) out(fmt("%s skill %d (%d xp)", st, craftSkill(std::string(st) == "field" ? "any" : st), craftXP(st))); } else { addCraftXP(arg(1), argi(2, 100)); out(fmt("%s skill %d", arg(1).c_str(), craftSkill(arg(1) == "field" ? "any" : arg(1)))); } }
    else if (c == "demo") { demoMode = argi(1, 1) != 0; out(demoMode ? "demo mode: the base campaign is limited to demo.txt" : "full game"); }
    else if (c == "crashtest") { std::string path = crashWriteReport("crash test from the dev console (the game is fine)"); emergencySave(); out("wrote " + path + " and crash_recovery.sav"); }
    else if (c == "autosave") { lastAutosave = -1000; autosave(); out("autosaved"); }
    else if (c == "pos") out(fmt("%s %d,%d", mapId.c_str(), px, py));
    else out("unknown command (try: help)");
    if (consoleOut.size() > 60) consoleOut.erase(consoleOut.begin(), consoleOut.begin() + (consoleOut.size() - 60));
}

void Game::updateConsole(InputState& in, float dt) {
    (void)dt;
    if (actP(in, A_CONSOLE) || in.pressed[K_ESCAPE]) { popMode(); in.eatKeys(); return; }
    for (char ch : in.typed) if (ch >= 32 && ch < 127 && ch != '`' && ch != '~' && consoleBuf.size() < 120) consoleBuf += ch;
    if (in.pressed[K_BACKSPACE] && !consoleBuf.empty()) { if (in.ctrl()) { size_t n = consoleBuf.size(); while (n > 0 && consoleBuf[n - 1] == ' ') n--; while (n > 0 && consoleBuf[n - 1] != ' ') n--; consoleBuf.resize(n); } else consoleBuf.pop_back(); }   // 1.1.11: Ctrl+Backspace = a word
    if (in.ctrl() && in.pressed[K_V]) for (char ch : in.clipboard) if (ch >= 32 && ch < 127 && consoleBuf.size() < 120) consoleBuf += ch;
    if (in.pressed[K_UP] && !consoleHist.empty()) { if (consoleHistPos < 0) consoleHistPos = (int)consoleHist.size() - 1; else consoleHistPos = std::max(0, consoleHistPos - 1); consoleBuf = consoleHist[consoleHistPos]; }
    if (in.pressed[K_DOWN] && !consoleHist.empty()) { if (consoleHistPos >= 0 && consoleHistPos < (int)consoleHist.size() - 1) { consoleHistPos++; consoleBuf = consoleHist[consoleHistPos]; } else { consoleHistPos = -1; consoleBuf.clear(); } }
    if (in.pressed[K_ENTER]) { std::string c = consoleBuf; consoleBuf.clear(); runConsole(c); }
    in.eatKeys();
}
void Game::drawConsole(Canvas& cv, InputState& in) {
    (void)in;
    int h = 200; cv.fillRect(0, 0, SCREEN_W, h, RGB(10, 10, 14, 230)); cv.hline(0, h, SCREEN_W, C::Gold); uiRect(0, 0, SCREEN_W, h + 1);
    // long output lines wrap to the screen width (the newest lines stay at the bottom)
    std::vector<std::pair<std::string, bool>> rows;
    for (auto& o : consoleOut) { bool cmd = startsWith(o, "> "); auto ws = ctx.font->wrap(o, SCREEN_W - 24); if (ws.empty()) ws.push_back(""); for (size_t k = 0; k < ws.size(); k++) rows.push_back({(k ? "  " : "") + ws[k], cmd}); }
    int lines = (h - 24) / 12; int start = std::max(0, (int)rows.size() - lines);
    for (int i = start; i < (int)rows.size(); i++) UI::text(ctx, 6, 4 + (i - start) * 12, rows[i].first, rows[i].second ? C::Yellow : C::LtGray);
    UI::text(ctx, 6, h - 16, "> " + consoleBuf + (((ctx.frame / 20) % 2) ? "_" : ""), C::White);
    textRight(ctx, SCREEN_W - 6, h - 16, "dev console - 'help' for commands, ~ or Esc to close", C::DkGray);
}
