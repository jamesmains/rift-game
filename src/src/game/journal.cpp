// Journal (quests / bestiary / stats / notes / achievements), companion banter & approval, achievements.
#include "game.h"
#include "../core/platform_services.h"
#include "../core/audio.h"
#include "tiles.h"
#include <algorithm>
#include <cmath>

using namespace UI;

// ---------------- data loading ----------------
void Game::loadSystems() {
    loadBoards(); loadHints();
    banters.clear(); achievements.clear(); recipes.clear(); affixes.clear(); factions.clear();
    for (auto& sec : mergedConfig("factions.txt")) { if (sec.name != "faction" || sec.get("id").empty()) continue; FactionDef f{sec.get("id"), sec.get("name", sec.get("id")), sec.get("text")}; bool dup = false; for (auto& o : factions) if (o.id == f.id) { o = f; dup = true; } if (!dup) factions.push_back(f); }
    for (auto& sec : mergedConfig("banter.txt")) {
        if (sec.name != "banter") continue;
        BanterLine b; b.cls = sec.get("class"); b.when = sec.get("when", "idle"); b.map = sec.get("map"); b.flag = sec.get("flag"); b.cond = sec.get("if");
        for (auto& kv : sec.kv) if (kv.first == "text") { BanterLine c = b; c.text = kv.second; banters.push_back(c); }
    }
    for (auto& sec : mergedConfig("achievements.txt")) {
        if (sec.name != "achievement") continue;
        AchievementDef a; a.id = sec.get("id"); a.name = sec.get("name", a.id); a.desc = sec.get("desc"); a.cond = sec.get("cond"); a.tile = parseTile(sec.get("tile", "41,46")); a.hidden = sec.geti("hidden", 0) != 0;
        bool dup = false; for (auto& o : achievements) if (o.id == a.id) { o = a; dup = true; } if (!a.id.empty() && !dup) achievements.push_back(a);
    }
    for (auto& sec : mergedConfig("recipes.txt")) {
        if (sec.name != "recipe") continue;
        Recipe r; r.id = sec.get("id"); r.result = sec.get("result", r.id); r.name = sec.get("name"); r.station = sec.get("station", "any"); r.count = sec.geti("count", 1); r.gold = sec.geti("gold", 0); r.level = sec.geti("level", 1);
        r.difficulty = clampi(sec.geti("difficulty", 1), 1, 6); r.known = sec.geti("known", 1) != 0; r.reveal = sec.get("reveal"); r.category = sec.get("category");
        for (auto& m : split(sec.get("materials"), ',')) { auto p = split(trim(m), ':'); if (trim(p[0]).empty()) continue; r.mats.push_back({trim(p[0]), p.size() > 1 ? atoi(p[1].c_str()) : 1}); }
        if (r.name.empty()) { if (auto* d = gd->item(r.result)) r.name = d->name; else r.name = r.result; }
        for (auto& o : recipes) if (o.id == r.id) { o = r; r.id.clear(); break; }   // a campaign recipe with the same id replaces the base one
        if (!r.id.empty()) recipes.push_back(r);
    }
    for (auto& sec : mergedConfig("affixes.txt")) {
        if (sec.name != "affix") continue;
        AffixDef a; a.id = sec.get("id"); a.name = sec.get("name", a.id); a.prefix = sec.get("pos", "prefix") == "prefix"; a.tier = sec.geti("tier", 1); a.valuePct = sec.geti("value", 160); a.dmgType = sec.get("dmgtype");
        for (auto& ty : split(sec.get("types", "weapon"), ',')) if (!trim(ty).empty()) a.types.push_back(trim(ty));
        for (auto& m : split(sec.get("mods"), ',')) { auto p = split(trim(m), ':'); if (p.size() == 2) a.mods.push_back({trim(p[0]), atoi(p[1].c_str())}); }
        bool dup = false; for (auto& o : affixes) if (o.id == a.id) { o = a; dup = true; } if (!dup) affixes.push_back(a);
    }
    loadLocations();
    loadAchievementState();
}

// ---------------- kills / stats / notes ----------------
void Game::onKill(const Character& victim, const Combatant* killer) {
    if (victim.isPlayerSide || victim.creatureId.empty()) return;
    stats.kills++; bestiary[victim.creatureId]++;
    bountyEvent("kill", victim.creatureId, 1);
    (void)killer;
}
void Game::addNote(const std::string& title, const std::string& text) {
    std::string n = title + ": " + text;
    if (std::find(notes.begin(), notes.end(), n) != notes.end()) return;
    notes.push_back(n); if (notes.size() > 60) notes.erase(notes.begin());
}

// ---------------- companions ----------------
void Game::banterTrigger(const std::string& when) {
    int freq = settings ? settings->banter : 2;
    if (party.members.size() < 2 || banters.empty() || mode != GMode::EXPLORE || freq == 0) return;
    if (when == "idle") { if (banterCooldown-- > 0) return; int lo = freq == 1 ? 260 : freq == 3 ? 45 : 90, hi = freq == 1 ? 520 : freq == 3 ? 110 : 200; banterCooldown = rng.range(lo, hi); if (!rng.chance(freq == 1 ? 35 : 55)) return; }
    else if (freq == 1 && when != "enter" && !rng.chance(40)) return;
    std::vector<std::pair<int, const BanterLine*>> cands;
    bool night = nightAmount() > 0.5f;
    for (size_t mi = 1; mi < party.members.size(); mi++) {
        auto& m = party.members[mi]; if (!m.alive) continue;
        for (auto& b : banters) {
            if (b.cls != m.classId && b.cls != "any") continue;
            std::string w = b.when;
            if (w == when || (when == "idle" && ((w == "night" && night) || (w == "rain" && (weather == "rain" || weather == "storm")) || (w == "dungeon" && map && map->kind == "dungeon") || (w == "town" && map && map->kind == "town") || (w == "overworld" && map && map->kind == "overworld")))) {
                if (!b.map.empty() && b.map != mapId) continue;
                if (!b.cond.empty() && !evalCond(b.cond)) continue;
                if (!b.flag.empty() && hasFlag("_banter_" + b.flag)) continue;
                cands.push_back({(int)mi, &b});
            }
        }
    }
    if (cands.empty()) return;
    auto pick = cands[rng.range(0, (int)cands.size() - 1)];
    banterMember = pick.first; banterText = pick.second->text; banterT = 4.0f + banterText.size() * 0.04f;
    size_t hp; while ((hp = banterText.find("$hero")) != std::string::npos) banterText.replace(hp, 5, heroName);
    if (!pick.second->flag.empty()) setFlag("_banter_" + pick.second->flag);
    log(party.members[banterMember].name + ": \"" + banterText + "\"");
    gAudio.play("banter");
}
void Game::drawBanter(Canvas& cv) {
    if (banterT <= 0 || banterMember < 0 || banterMember >= (int)party.members.size() || mode != GMode::EXPLORE) return;
    int viewH = SCREEN_H - 72;
    // speech bubble above the companion's trail position
    int tx = px, ty = py; if (banterMember - 1 < (int)trail.size()) { tx = trail[banterMember - 1].first; ty = trail[banterMember - 1].second; }
    auto lines = ctx.font->wrap(banterText, 200); int w = 0; for (auto& l : lines) w = std::max(w, ctx.font->width(l)); w += 12;
    int h = (int)lines.size() * 12 + 8;
    int bx = clampi(tx * TILE - camX + TILE / 2 - w / 2, 4, SCREEN_W - w - 4), by = clampi(ty * TILE - camY - h - 10, 4, viewH - h - 4);
    cv.fillRect(bx, by, w, h, RGB(250, 245, 230, 235)); cv.rect(bx, by, w, h, RGB(60, 40, 20));
    int px2 = clampi(tx * TILE - camX + TILE / 2, bx + 6, bx + w - 6); cv.line(px2 - 3, by + h, px2, by + h + 5, RGB(60, 40, 20)); cv.line(px2 + 3, by + h, px2, by + h + 5, RGB(60, 40, 20));
    for (size_t i = 0; i < lines.size(); i++) ctx.font->draw(cv, *ctx.ts, bx + 6, by + 4 + (int)i * 12, lines[i], RGB(30, 20, 10));
}

// ---------------- achievements ----------------
void Game::loadAchievementState() {
    unlocked.clear(); std::string t;
    if (readFile(saveDir + "/achievements.ini", t)) for (auto& sec : parseConfig(t)) if (sec.name == "unlocked") for (auto& kv : sec.kv) if (kv.second != "0") unlocked.insert(kv.first);
}
void Game::saveAchievementState() const {
    std::string s = "# Triffall achievements (global, not per save slot)\n[unlocked]\n"; for (auto& u : unlocked) s += u + "=1\n";
    writeFile(saveDir + "/achievements.ini", s);
}
void Game::unlockAchievement(const std::string& id) {
    if (unlocked.count(id)) return;
    unlocked.insert(id); saveAchievementState(); gPlatform->unlockAchievement(id);
    for (auto& a : achievements) if (a.id == id) { achPopups.push_back({id, 5.0f}); gAudio.play("achievement"); log("Achievement unlocked: " + a.name); }
}
void Game::checkAchievements(bool force) {
    if (!running || !campaign.achievements) return;
    if (!force) { achCheckT += frameDt; if (achCheckT < 0.5f) return; achCheckT = 0; }
    int questsDone = 0; for (auto& q : quests) { std::string st = questStatus(q); if (startsWith(st, "DONE")) questsDone++; } stats.questsDone = questsDone;
    int lvl = party.hero() ? party.hero()->level : 1;
    for (auto& a : achievements) {
        if (unlocked.count(a.id) || a.cond.empty()) continue;
        bool ok = true;
        for (auto& part : split(a.cond, '&')) {
            std::string c = trim(part); if (c.empty()) continue;
            size_t col = c.find(':'); std::string k = col == std::string::npos ? c : c.substr(0, col), v = col == std::string::npos ? "" : c.substr(col + 1); int n = atoi(v.c_str());
            bool r = false;
            if (k == "flag") r = evalCond(v); else if (k == "kills") r = stats.kills >= n; else if (k == "fights") r = stats.fightsWon >= n; else if (k == "gold") r = stats.goldEarned >= n;
            else if (k == "level") r = lvl >= n; else if (k == "chests") r = stats.chests >= n; else if (k == "maps") r = (int)visited.size() >= n; else if (k == "crafted") r = stats.crafted >= n;
            else if (k == "bounties") r = stats.bounties >= n; else if (k == "bestiary") { int kinds = 0; for (auto& b : bestiary) if (b.second > 0) kinds++; r = kinds >= n; } else if (k == "steps") r = stepCounter >= n;
            else if (k == "day") r = day >= n; else if (k == "travels") r = stats.travels >= n; else if (k == "crits") r = stats.crits >= n; else if (k == "boss") { auto it = bestiary.find(v); r = it != bestiary.end() && it->second > 0; }
            else if (k == "party") r = (int)party.members.size() >= n; else if (k == "quests") r = questsDone >= n; else if (k == "spells") r = stats.spellsCast >= n; else if (k == "damage") r = stats.damageDealt >= n;
            else if (k == "knockouts") r = stats.knockouts >= n; else if (k == "fled") r = stats.fightsFled >= n; else if (k == "notes") r = (int)notes.size() >= n; else if (k == "item") r = party.hasItem(v);
            else if (k == "rep") { auto p = split(v, '>'); auto it = reputation.find(trim(p[0])); r = (it != reputation.end() ? it->second : 0) >= (p.size() > 1 ? atoi(p[1].c_str() + (p[1].size() && p[1][0] == '=' ? 1 : 0)) : 1); }
            else if (k == "approval") { auto p = split(v, '>'); auto it = approval.find(trim(p[0])); r = it != approval.end() && it->second >= (p.size() > 1 ? atoi(p[1].c_str() + (p[1].size() && p[1][0] == '=' ? 1 : 0)) : 1); }
            if (!r) { ok = false; break; }
        }
        if (ok) { unlockAchievement(a.id); continue; }
        // a single counter goal ("kills:100"): tell the platform at half way (Steam shows its progress toast once)
        { auto parts = split(a.cond, '&'); if (parts.size() != 1) continue; std::string c = trim(parts[0]); size_t col = c.find(':'); if (col == std::string::npos) continue;
          std::string k = c.substr(0, col); int n = atoi(c.substr(col + 1).c_str()); if (n < 4) continue;
          int cur = k == "kills" ? stats.kills : k == "fights" ? stats.fightsWon : k == "gold" ? stats.goldEarned : k == "chests" ? stats.chests : k == "maps" ? (int)visited.size() : k == "crafted" ? stats.crafted :
                    k == "bounties" ? stats.bounties : k == "steps" ? stepCounter : k == "travels" ? stats.travels : k == "crits" ? stats.crits : k == "spells" ? stats.spellsCast : k == "damage" ? stats.damageDealt : k == "notes" ? (int)notes.size() : -1;
          if (k == "bestiary") { cur = 0; for (auto& b : bestiary) if (b.second > 0) cur++; }
          if (cur < 0) continue;
          std::string key = "achprog_" + a.id;
          if (cur * 2 >= n && !extra.count(key)) { extra[key] = "50"; gPlatform->indicateAchievementProgress(a.id, cur, n); } }
    }
}
void Game::drawAchievementPopups(Canvas& cv, float dt) {
    int y = SCREEN_H - 72 - 36;
    for (size_t i = 0; i < achPopups.size(); i++) {
        auto& p = achPopups[i]; p.second -= dt;
        const AchievementDef* a = nullptr; for (auto& d : achievements) if (d.id == p.first) a = &d;
        if (!a) continue;
        float t = p.second; float slide = t > 4.6f ? (5.0f - t) / 0.4f : t < 0.5f ? t / 0.5f : 1.0f;
        int w = 230, h = 44; int x = SCREEN_W - (int)(w * slide) - 4; int yy = y - (int)i * (h + 4);
        // Win98 "balloon" style
        cv.fillRect(x, yy, w, h, RGB(255, 255, 225)); cv.rect(x, yy, w, h, C::Black);
        cv.fillRect(x + 1, yy + 1, w - 2, 12, RGB(0, 0, 128)); ctx.font->draw(cv, *ctx.ts, x + 5, yy + 1, "Achievement unlocked", C::White);
        cv.fillRect(x + 4, yy + 16, 26, 26, RGB(200, 200, 200)); ctx.ts->draw(cv, a->tile, x + 5, yy + 17, 2);
        ctx.font->bold = true; ctx.font->draw(cv, *ctx.ts, x + 34, yy + 16, a->name, C::Black); ctx.font->bold = false;
        ctx.font->draw(cv, *ctx.ts, x + 34, yy + 29, a->desc.substr(0, 34), RGB(60, 60, 60));
    }
    achPopups.erase(std::remove_if(achPopups.begin(), achPopups.end(), [](const std::pair<std::string, float>& p) { return p.second <= 0; }), achPopups.end());
}

// ---------------- journal ----------------
std::string Game::questText(const QuestDef& q) const { std::string s = questStatus(q); size_t at = s.find(" @"); return at == std::string::npos ? s : s.substr(0, at); }
std::string Game::questPin(const QuestDef& q) const { std::string s = questStatus(q); size_t at = s.find(" @"); return at == std::string::npos ? "" : trim(s.substr(at + 2)); }

static const char* JTABS[] = {"Quests", "Bounties", "Bestiary", "Stats", "Notes", "Achievements"};
static const int JN = 6;

void Game::updateJournal(InputState& in, float dt) {
    (void)dt;
    if (actP(in, A_CANCEL) || actP(in, A_JOURNAL)) { popMode(); return; }
    // achievements are a two-column grid: left/right move across it (and change tab at the edges); elsewhere left/right change tab
    bool grid = jTab == 5 && !achievements.empty(); int na = (int)achievements.size();
    if (grid && actP(in, A_RIGHT) && jSel % 2 == 0 && jSel + 1 < na) jSel++;
    else if (grid && actP(in, A_LEFT) && jSel % 2 == 1) jSel--;
    else if (actP(in, A_PAGEUP) || actP(in, A_LEFT)) { jTab = (jTab + JN - 1) % JN; jSel = 0; jScroll = 0; }
    else if (actP(in, A_PAGEDOWN) || actP(in, A_RIGHT)) { jTab = (jTab + 1) % JN; jSel = 0; jScroll = 0; }
    for (int i = 0; i < JN; i++) { int bx = 14 + i * 100; if (in.mousePressed[0] && in.mouseX >= bx && in.mouseX < bx + 96 && in.mouseY >= 12 && in.mouseY < 32) { jTab = i; jSel = 0; jScroll = 0; in.eatMouse(); } }
    int n = 0;
    if (jTab == 0) { for (auto& q : quests) if (!questStatus(q).empty()) n++; }
    else if (jTab == 1) n = (int)bounties.size();
    else if (jTab == 2) n = (int)bestiary.size();
    else if (jTab == 4) n = (int)notes.size();
    else if (jTab == 5) n = (int)achievements.size();
    if (n > 0 && !grid) { if (actP(in, A_UP)) jSel = (jSel + n - 1) % n; if (actP(in, A_DOWN)) jSel = (jSel + 1) % n; }
    if (grid) { if (actP(in, A_UP) && jSel >= 2) jSel -= 2; if (actP(in, A_DOWN) && jSel + 2 < n) jSel += 2; }
    jSel = clampi(jSel, 0, std::max(0, n - 1));
    if (in.wheel) { if (jTab == 0) jSel = clampi(jSel - in.wheel, 0, std::max(0, n - 1)); else jScroll = clampi(jScroll - in.wheel, 0, std::max(0, n - 1)); }
    if (jTab == 0 && !worldMapSource().empty() && (actP(in, A_CONFIRM) || actP(in, A_WORLDMAP))) { // open the map on the quest's pin
        int k = 0; for (auto& q : quests) { if (questStatus(q).empty()) continue; if (k == jSel) { std::string pin = questPin(q); if (!pin.empty()) { popMode(); openWorldMap(); if (wmSource == "overworld") { for (size_t i = 0; i < locations.size(); i++) if (locations[i].id == pin) wmSel = (int)i; } else { auto pl = worldPlaces(loadMap(wmSource)); std::string pm = pin; for (auto& l : locations) if (l.id == pin) pm = l.mapId; for (size_t i = 0; i < pl.size(); i++) if (pl[i].known && pl[i].mapId == pm) wmSel = (int)i; } } break; } k++; }
    }
}

void Game::drawJournal(Canvas& cv, InputState& in) {
    cv.dim(0, 0, SCREEN_W, SCREEN_H, 160); uiRect(0, 0, SCREEN_W, SCREEN_H);
    panel(ctx, 6, 6, SCREEN_W - 12, SCREEN_H - 12);
    for (int i = 0; i < JN; i++) button(ctx, in, 14 + i * 100, 12, 96, 20, JTABS[i], true, jTab == i);
    int y0 = 44, x0 = 16, w = SCREEN_W - 32;
    if (jTab == 0) {
        // Rows are measured first so the list can scroll to keep the selected (opened) quest fully on screen; every
        // line of text - the quest's, and each checklist line - is wrapped to the row, and the list is clipped to the panel.
        struct JRow { const QuestDef* q; std::string st; bool failed, done; std::vector<std::string> full; std::vector<std::pair<std::vector<std::string>, bool>> objs; int h; };
        std::vector<JRow> rows; const int bottom = SCREEN_H - 34;
        for (auto& q : quests) {
            std::string st = questStatus(q); if (st.empty()) continue;
            JRow r; r.q = &q; r.st = st; r.failed = startsWith(st, "FAILED"); r.done = startsWith(st, "DONE") || r.failed;
            std::string txt = questText(q); if (startsWith(txt, "DONE ")) txt = txt.substr(5); else if (startsWith(txt, "FAILED ")) txt = txt.substr(7);
            r.full = ctx.font->wrap(txt, w - 20);
            bool sel = (int)rows.size() == jSel;
            if (sel && !r.done) { int si = questStageIndex(q); for (auto& o : q.objectives) if (o.stage == 0 || o.stage == si) r.objs.push_back({ctx.font->wrap(questCounters(o.text), w - 40), evalCond(o.cond)}); }
            int objLines = 0; for (auto& ob : r.objs) objLines += (int)std::max<size_t>(1, ob.first.size());
            r.h = sel ? 21 + (int)std::max<size_t>(1, r.full.size()) * 13 + objLines * 13 : 34;
            rows.push_back(r);
        }
        bool any = !rows.empty();
        if (any) {
            jSel = clampi(jSel, 0, (int)rows.size() - 1);
            if (jScroll > jSel) jScroll = jSel;
            for (;;) { int yy = y0; for (int i = jScroll; i < jSel; i++) yy += rows[i].h + 2; if (yy + rows[jSel].h <= bottom || jScroll >= jSel) break; jScroll++; }
        }
        cv.setClip({x0, y0, w, bottom - y0});
        int y = y0;
        for (int k = jScroll; k < (int)rows.size() && y < bottom; k++) {
            auto& r = rows[k]; const QuestDef& q = *r.q; bool failed = r.failed, done = r.done, sel = k == jSel; int rowH = r.h;
            listRow(ctx, in, x0, y, w, rowH, sel);
            int rightX = x0 + w - 8;
            if (failed) { textRight(ctx, rightX, y + 3, "failed", C::Red); rightX -= ctx.font->width("failed") + 10; }
            else if (startsWith(r.st, "DONE")) { textRight(ctx, rightX, y + 3, "done", C::Green); rightX -= ctx.font->width("done") + 10; }
            std::string pin = questPin(q); if (!pin.empty() && !done) { std::string nm = pin; for (auto& l : locations) if (l.id == pin) nm = l.name; textRight(ctx, rightX, y + 3, "> " + nm, C::Cyan); rightX -= ctx.font->width("> " + nm) + 10; }
            { ctx.font->bold = true; std::string nm = q.name; while (nm.size() > 4 && x0 + 8 + ctx.font->width(nm) > rightX) nm = nm.substr(0, nm.size() - 4) + "..";
              UI::text(ctx, x0 + 8, y + 3, nm, failed ? C::Red : done ? C::Gray : C::Gold); ctx.font->bold = false; }
            if (sel) { int ly = y + 17; for (auto& l : r.full) { UI::text(ctx, x0 + 8, ly, l, failed ? RGB(220, 130, 115) : done ? C::DkGray : C::White); ly += 13; } if (r.full.empty()) ly += 13;
                for (auto& ob : r.objs) { cv.rect(x0 + 14, ly + 2, 8, 8, ob.second ? C::Green : C::LtGray); if (ob.second) cv.fillRect(x0 + 16, ly + 4, 4, 4, C::Green);
                    for (auto& l : ob.first) { UI::text(ctx, x0 + 28, ly, l, ob.second ? C::Gray : C::Cream); ly += 13; } if (ob.first.empty()) ly += 13; } }
            else {   // closed rows show the first line, with ".." when there is more
                std::string first = r.full.empty() ? "" : r.full[0];
                if (r.full.size() > 1) { while (first.find(' ') != std::string::npos && ctx.font->width(first + " ..") > w - 16) first = first.substr(0, first.rfind(' ')); first += " .."; }
                UI::text(ctx, x0 + 8, y + 17, first, failed ? RGB(190, 110, 100) : done ? C::DkGray : C::White); }
            y += rowH + 2;
        }
        cv.resetClip();
        if (!any) UI::text(ctx, x0, y0, "No quests yet. Talk to people!", C::Gray);
        if (!worldMapSource().empty()) keyHint(ctx, 16, SCREEN_H - 28, "Enter/" + gKeys.label(A_WORLDMAP), "show on map");
    } else if (jTab == 1) {
        int y = y0;
        if (bounties.empty()) UI::text(ctx, x0, y0, "No bounties taken. Notice boards in towns post them.", C::Gray);
        for (size_t i = 0; i < bounties.size(); i++) {
            auto& b = bounties[i]; if ((int)i < jScroll) continue; if (y > SCREEN_H - 60) break;
            listRow(ctx, in, x0, y, w, 34, (int)i == jSel);
            std::string title = b.kind == "kill" ? fmt("Slay %d %s", b.need, b.targetName.c_str()) : b.kind == "fetch" ? fmt("Recover %d %s", b.need, b.targetName.c_str()) : "Deliver a letter to " + b.targetName;
            ctx.font->bold = true; UI::text(ctx, x0 + 8, y + 3, title, b.done ? C::Green : C::Gold); ctx.font->bold = false;
            textRight(ctx, x0 + w - 8, y + 3, fmt("%d gold, %d XP", b.reward, b.xp), C::Cream);
            UI::text(ctx, x0 + 8, y + 17, b.done ? "Complete - return to the notice board in " + boardName(b.town) + "." : fmt("%s  (%d/%d)  posted in %s", b.whereName.empty() ? "" : ("near " + b.whereName).c_str(), b.have, b.need, boardName(b.town).c_str()), b.done ? C::Green : C::White);
            y += 36;
        }
    } else if (jTab == 2) {
        // bestiary: list on the left, detail on the right
        std::vector<std::string> ids; for (auto& b : bestiary) ids.push_back(b.first);
        int y = y0; int lw = 220;
        if (ids.empty()) UI::text(ctx, x0, y0, "Nothing catalogued yet. Fight something.", C::Gray);
        for (size_t i = 0; i < ids.size(); i++) {
            if ((int)i < jScroll) continue; if (y > SCREEN_H - 50) break;
            const CreatureDef* cd = gd->creature(ids[i]); listRow(ctx, in, x0, y, lw, 20, (int)i == jSel);
            if (cd) { if (cd->recolor) ctx.ts->drawRecolor(cv, cd->tile, x0 + 4, y + 2, 1, cd->color); else ctx.ts->draw(cv, cd->tile, x0 + 4, y + 2, 1); }
            UI::text(ctx, x0 + 22, y + 4, cd ? cd->name : ids[i], bestiary[ids[i]] > 0 ? C::White : C::Gray); textRight(ctx, x0 + lw - 6, y + 4, fmt("x%d", bestiary[ids[i]]), C::Gray);
            y += 21;
        }
        if (jSel >= 0 && jSel < (int)ids.size()) {
            const CreatureDef* cd = gd->creature(ids[jSel]); int kills = bestiary[ids[jSel]];
            int dx = x0 + lw + 16, dy = y0;
            if (cd) {
                panel(ctx, dx - 6, dy - 6, w - lw - 16, SCREEN_H - 100, C::Panel, C::DkGray);
                cv.fillRect(dx, dy, 52, 52, RGB(40, 40, 60)); if (cd->size == 2) { for (int yy = 0; yy < 2; yy++) for (int xx = 0; xx < 2; xx++) ctx.ts->draw(cv, T(cd->tile.c + xx, cd->tile.r + yy), dx + 2 + xx * 24, dy + 2 + yy * 24, 2); } else if (cd->recolor) ctx.ts->drawRecolor(cv, cd->tile, dx + 14, dy + 14, 2, cd->color); else ctx.ts->draw(cv, cd->tile, dx + 14, dy + 14, 2);
                ctx.font->bold = true; UI::text(ctx, dx + 60, dy, cd->name, C::Gold); ctx.font->bold = false;
                UI::text(ctx, dx + 60, dy + 14, fmt("Level %d  %s  %s", cd->level, cd->faction.c_str(), cd->boss ? "BOSS" : ""), C::Gray);
                UI::text(ctx, dx + 60, dy + 28, fmt("Killed: %d", kills), C::White);
                int cy = dy + 62, tw = w - lw - 28;
                // entries flow onto as many lines as they need, never breaking inside an entry
                auto flow = [&](const std::vector<std::string>& parts, Color8 col) {
                    std::string line;
                    for (auto& p : parts) {
                        std::string tryL = line.empty() ? p : line + "   " + p;
                        if (!line.empty() && ctx.font->width(tryL) > tw) { UI::text(ctx, dx, cy, line, col); cy += 13; line = p; } else line = tryL;
                    }
                    if (!line.empty()) { UI::text(ctx, dx, cy, line, col); cy += 13; }
                };
                // more detail unlocks with kills: 1 = stats, 3 = resistances, 5 = loot
                flow({fmt("HP %d", cd->hp), fmt("Attack %d-%d %s", cd->dmgMin, cd->dmgMax, dmgTypeName(cd->dmgType)), fmt("Range %d", cd->range), fmt("Move %d", cd->move)}, kills >= 1 ? C::White : C::DkGray);
                if (kills < 1) { UI::text(ctx, dx, cy, "(defeat one to learn its stats)", C::DkGray); cy += 14; }
                { std::vector<std::string> info = {fmt("XP %d", cd->xp), "AI: " + cd->ai}; if (cd->undead) info.push_back("undead"); if (cd->mechanical) info.push_back("mechanical"); if (cd->darkvision) info.push_back("sees in the dark"); flow(info, kills >= 1 ? C::LtGray : C::DkGray); }
                cy += 5;
                UI::text(ctx, dx, cy, "Resistances:", kills >= 3 ? C::Gold : C::DkGray); cy += 14;
                if (kills >= 3) { std::vector<std::string> rs; for (int i = 0; i < DT_COUNT; i++) if (cd->res[i] != 0) rs.push_back(fmt("%s %+d%%", dmgTypeName(i), cd->res[i])); bool poisonListed = cd->res[DT_POISON] >= 100; if ((cd->undead || cd->mechanical) && !poisonListed) rs.push_back("immune to poison"); if (cd->mechanical) rs.push_back("weak to shock"); if (rs.empty()) rs.push_back("none"); flow(rs, C::White); }
                else { UI::text(ctx, dx, cy, fmt("(revealed after 3 kills - %d more)", 3 - kills), C::DkGray); cy += 13; }
                cy += 5;
                UI::text(ctx, dx, cy, "Loot:", kills >= 5 ? C::Gold : C::DkGray); cy += 14;
                if (kills >= 5) { std::vector<std::string> ls = {"gold " + cd->lootGold}; for (auto& l : cd->loot) if (auto* d = gd->item(l.first)) ls.push_back(fmt("%s %d%%", d->name.c_str(), l.second)); flow(ls, C::White); }
                else { UI::text(ctx, dx, cy, fmt("(revealed after 5 kills - %d more)", 5 - kills), C::DkGray); cy += 13; }
                cy += 5;
                if (!cd->spells.empty() && kills >= 3) { std::vector<std::string> sp = {"Spells:"}; for (auto& s2 : cd->spells) if (auto* d = gd->spell(s2)) sp.push_back(d->name); flow(sp, C::Cyan); }
            }
        }
    } else if (jTab == 3) {
        int kinds = 0; for (auto& b : bestiary) if (b.second > 0) kinds++;
        const Character* h = party.hero();
        std::vector<std::pair<std::string, std::string>> rows = {
            {"Hero", h ? fmt("%s, level %d %s", h->name.c_str(), h->level, gd->cls(h->classId) ? gd->cls(h->classId)->name.c_str() : "") : ""},
            {"Play time", fmt("%dh %02dm", playtimeSec / 3600, (playtimeSec / 60) % 60)}, {"In-game time", clockText()},
            {"Steps taken", std::to_string(stepCounter)}, {"Maps visited", std::to_string(visited.size())}, {"Fast travels", std::to_string(stats.travels)},
            {"Fights won / fled", fmt("%d / %d", stats.fightsWon, stats.fightsFled)}, {"Enemies slain", fmt("%d (%d kinds)", stats.kills, kinds)}, {"Critical hits", std::to_string(stats.crits)},
            {"Damage dealt / taken", fmt("%d / %d", stats.damageDealt, stats.damageTaken)}, {"Spells cast", std::to_string(stats.spellsCast)}, {"Party knockouts", std::to_string(stats.knockouts)},
            {"Gold earned / spent", fmt("%d / %d", stats.goldEarned, stats.goldSpent)}, {"Chests opened", std::to_string(stats.chests)}, {"Items crafted", std::to_string(stats.crafted)},
            {"Bounties completed", std::to_string(stats.bounties)}, {"Quests completed", std::to_string(stats.questsDone)}, {"Achievements", fmt("%d / %d", (int)unlocked.size(), (int)achievements.size())}};
        int y = y0; for (auto& r : rows) { UI::text(ctx, x0 + 8, y, r.first, C::Gray); UI::text(ctx, x0 + 190, y, r.second, C::White); y += 15; }
        // companions & approval
        int cx = x0 + 380; UI::text(ctx, cx, y0, "Companions", C::Gold);
        int cy = y0 + 16; for (size_t i = 1; i < party.members.size(); i++) { auto& m = party.members[i]; int ap = approval.count(m.classId) ? approval[m.classId] : 0; UI::text(ctx, cx, cy, m.name, C::White); bar(ctx, cx + 90, cy + 2, 100, 8, clampi(ap + 50, 0, 100), 100, ap >= 0 ? RGB(80, 200, 100) : RGB(200, 80, 60)); UI::text(ctx, cx + 196, cy, fmt("%+d", ap), ap >= 0 ? C::Green : C::Red); cy += 16; }
        if (party.members.size() < 2) { UI::text(ctx, cx, cy, "none yet", C::Gray); cy += 16; }
        // reputation with towns and groups (only the ones you have dealt with, plus any the campaign lists)
        cy += 10; UI::text(ctx, cx, cy, "Reputation", C::Gold); cy += 16; std::vector<std::string> ids; for (auto& r : reputation) ids.push_back(r.first);
        if (ids.empty()) UI::text(ctx, cx, cy, "nobody knows you yet", C::Gray);
        for (auto& id : ids) { if (cy > SCREEN_H - 50) break; int r = reputation[id]; UI::text(ctx, cx, cy, factionName(id), C::White); bar(ctx, cx + 90, cy + 2, 100, 8, (r + 100) / 2, 100, r >= 0 ? RGB(80, 170, 220) : RGB(200, 80, 60)); UI::text(ctx, cx + 196, cy, fmt("%s %+d", repTier(r), r), r >= 10 ? C::Cyan : r <= -10 ? C::Red : C::LtGray); cy += 16; }
    } else if (jTab == 4) {
        int y = y0; if (notes.empty()) UI::text(ctx, x0, y0, "Signs you read and things you learn are kept here.", C::Gray);
        for (size_t i = 0; i < notes.size(); i++) { if ((int)i < jScroll) continue; if (y > SCREEN_H - 50) break; auto lines = ctx.font->wrap(notes[i], w - 16); listRow(ctx, in, x0, y, w, (int)lines.size() * 12 + 6, (int)i == jSel); for (size_t l = 0; l < lines.size(); l++) UI::text(ctx, x0 + 8, y + 3 + (int)l * 12, lines[l], l == 0 ? C::Cream : C::LtGray); y += (int)lines.size() * 12 + 8; }
    } else if (jTab == 5) {
        int y = y0; int cols = 2; int cw = w / cols;
        for (size_t i = 0; i < achievements.size(); i++) {
            auto& a = achievements[i]; bool got = unlocked.count(a.id) > 0;
            int row = (int)i / cols - jScroll; if (row < 0) continue; int ax = x0 + (int)(i % cols) * cw, ay = y + row * 40; if (ay > SCREEN_H - 60) continue;
            bool sel = (int)i == jSel; listRow(ctx, in, ax, ay, cw - 6, 38, sel);
            cv.fillRect(ax + 4, ay + 5, 28, 28, got ? RGB(60, 50, 20) : RGB(30, 30, 40)); ctx.ts->draw(cv, a.tile, ax + 6, ay + 7, 2, got ? C::White : RGB(90, 90, 90));
            bool hide = a.hidden && !got;
            ctx.font->bold = true; UI::text(ctx, ax + 38, ay + 4, hide ? "???" : a.name, got ? C::Gold : C::Gray); ctx.font->bold = false;
            auto lines = ctx.font->wrap(hide ? "Hidden achievement." : a.desc, cw - 50); UI::text(ctx, ax + 38, ay + 18, lines.empty() ? "" : lines[0], got ? C::White : C::DkGray);
        }
        UI::text(ctx, x0, SCREEN_H - 28, fmt("%d / %d unlocked", (int)unlocked.size(), (int)achievements.size()), C::Gray);
    }
    keyHint(ctx, SCREEN_W - 230, SCREEN_H - 28, "<  >", "tabs"); keyHint(ctx, SCREEN_W - 110, SCREEN_H - 28, "Esc", "close");
}
