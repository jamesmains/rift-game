// 1.1.2 standing: what companions think of you (approval, bonds, leaving) and what towns think of you (favours, civic
// projects, amends, refusals). Both only move through things the player chooses to do - never with time or by chatting.
//   Companions: approval changes only for those in the party (they have to witness it), from story choices, their own
//   quest, and one-off gifts. Strained (<= -15): -10 hit / -5 evasion. At -40 they leave and go home; making amends
//   there (gold, in their dialogue: amends=CLASS:N) brings them back. Loyal (flag loyal_CLASS, set in their camp talk
//   once approval is 30+ and their quest is done; holds while approval stays 10+): +10 hit / +5 evasion.
//   Towns: odd jobs (bounties) raise standing only up to Friendly (30). Past that it takes civic projects (deliveries
//   listed in factions.txt, one after another) and story choices. Favours (reward=) are claimed once each at the
//   board. Disliked (<= -10): the board posts no work. Hated (<= -50): the town's shops won't trade. Below 0 you can pay
//   to make amends (back to Neutral, never higher - gold can't buy honour). Dropping a taken notice costs 2.
#include "game.h"
#include "../core/audio.h"
#include "tiles.h"
#include <algorithm>

using namespace UI;

// ---------------- companions ----------------
const char* Game::approvalTier(int a) { return a <= -30 ? "Resentful" : a <= -15 ? "Strained" : a < 10 ? "Neutral" : a < 30 ? "Warm" : "Trusting"; }
int Game::approvalOf(const std::string& cls) const { auto it = approval.find(cls); return it == approval.end() ? 0 : it->second; }
bool Game::companionLoyal(const std::string& cls) const { return hasFlag("loyal_" + cls) && approvalOf(cls) >= 10; }

void Game::syncBonds() {
    for (size_t i = 0; i < party.members.size(); i++) {
        Character& m = party.members[i];
        if (i == 0) { m.bond = 0; continue; }
        int a = approvalOf(m.classId);
        m.bond = companionLoyal(m.classId) ? 1 : a <= -15 ? -1 : 0;
    }
    applyTaught("", true);   // 1.4.0 (#81): lessons from their personal quests (the Loyal version while Loyal)
}

// ---------------- taught abilities (1.4.0, #81) ----------------
// teach=WHO:SPELL[|LOYAL_SPELL][,WHO:SPELL...] - WHO is a companion NPC id (bram), a class id (warrior), a party member's
// name, or hero. The hero learns SPELL at once. A companion's lesson is remembered in extra["taught_<class>"] (saved),
// so it also reaches a companion who is away and arrives with them when they join again; applyTaught swaps in the
// LOYAL_SPELL while they are Loyal (flag loyal_<class> and approval 10+) and back if their loyalty breaks.
void Game::teachAction(const std::string& v) {
    for (auto& part : split(v, ',')) {
        std::string t = trim(part); size_t col = t.find(':'); if (col == std::string::npos) continue;
        std::string who = trim(t.substr(0, col)); auto alts = split(t.substr(col + 1), '|');
        std::string base = trim(alts[0]), loyal = alts.size() > 1 ? trim(alts[1]) : "";
        const SpellDef* sd = gd->spell(base); if (!sd) { log("(teach: unknown spell '" + base + "')"); continue; }
        if (!loyal.empty() && !gd->spell(loyal)) loyal.clear();
        bool hero = toLower(who) == "hero"; std::string cls;
        if (!hero) for (size_t i = 0; i < party.members.size(); i++) if (toLower(party.members[i].name) == toLower(who)) { if (i == 0) hero = true; else cls = party.members[i].classId; break; }
        if (!hero && cls.empty()) cls = gd->cls(who) ? who : parseJoin(who).cls;   // a class id, else a companion NPC's id
        if (hero) {
            if (party.members.empty()) continue; Character& m = party.members[0];
            if (std::find(m.spells.begin(), m.spells.end(), base) == m.spells.end()) { m.spells.push_back(base); log(m.name + " learns " + sd->name + "."); notify(m.name + " learns " + sd->name + "!"); gAudio.play("ability_learned"); }
            continue;
        }
        if (cls.empty() || !gd->cls(cls)) { log("(teach: nobody called '" + who + "')"); continue; }
        std::string& list = extra["taught_" + cls]; std::string entry = base + (loyal.empty() ? "" : "|" + loyal);
        bool have = false; for (auto& e : split(list, ',')) if (trim(e) == entry) have = true;
        if (!have) list += (list.empty() ? "" : ",") + entry;
        bool present = false; for (size_t i = 1; i < party.members.size(); i++) if (party.members[i].classId == cls) present = true;
        if (present) applyTaught(cls, true);
        else log("They will remember " + sd->name + " when they travel with you again.");
    }
}

void Game::applyTaught(const std::string& onlyCls, bool announce) {
    for (size_t i = 1; i < party.members.size(); i++) {
        Character& m = party.members[i]; if (!onlyCls.empty() && m.classId != onlyCls) continue;
        auto it = extra.find("taught_" + m.classId); if (it == extra.end()) continue;
        bool loyal = companionLoyal(m.classId);
        auto has = [&m](const std::string& id) { return std::find(m.spells.begin(), m.spells.end(), id) != m.spells.end(); };
        for (auto& e : split(it->second, ',')) {
            auto alts = split(trim(e), '|'); std::string base = trim(alts[0]), up = alts.size() > 1 ? trim(alts[1]) : "";
            if (!gd->spell(up)) up.clear();
            std::string want = loyal && !up.empty() ? up : base, other = want == base ? up : base;
            const SpellDef* wd = gd->spell(want); if (!wd) continue;
            bool swapped = !other.empty() && has(other);
            if (swapped) { m.spells.erase(std::remove(m.spells.begin(), m.spells.end(), other), m.spells.end()); for (auto& f : m.favs) if (f == "spell:" + other) f = "spell:" + want; }
            if (has(want)) continue;
            m.spells.push_back(want);
            if (!announce) continue;
            const SpellDef* od = swapped ? gd->spell(other) : nullptr;
            if (od && want == up) { log(m.name + " is Loyal: " + od->name + " becomes " + wd->name + "."); notify(m.name + ": " + wd->name + "!"); }
            else if (od) log(m.name + "'s loyalty is shaken: " + od->name + " is only " + wd->name + " again.");
            else { log(m.name + " learns " + wd->name + "."); notify(m.name + " learns " + wd->name + "!"); gAudio.play("ability_learned"); }
        }
    }
}

void Game::changeApproval(const std::string& cls, int d) {
    if (d == 0) return;
    int before = approvalOf(cls); int& a = approval[cls]; a = clampi(before + d, -100, 100);
    std::string name = cls; for (size_t i = 1; i < party.members.size(); i++) if (party.members[i].classId == cls) name = party.members[i].name;
    notify(fmt("%s %s (%+d)", name.c_str(), d >= 0 ? "approves" : "disapproves", d));
    if (before > -15 && a <= -15) log(name + " is unhappy with you: Strained, and worse in a fight for it. At -40 they will leave the party.");
    else if (before > -30 && a <= -30) log(name + " barely speaks to you now. Any more and they will walk away.");
    else if (before <= -15 && a > -15) log(name + " is willing to give you another chance.");
    if (before < 30 && a >= 30 && !hasFlag("loyal_" + cls)) log(name + " trusts you. Talk to them in the party menu (T) when their own business is settled.");
    if (hasFlag("loyal_" + cls) && before >= 10 && a < 10) log(name + "'s loyalty is shaken. It holds again at 10.");
    syncBonds();
}

// explore mode, every frame: a companion pushed to -40 leaves at the next calm moment
void Game::checkCompanionMoods() {
    for (size_t i = party.members.size(); i-- > 1;) if (approvalOf(party.members[i].classId) <= -40) { companionLeaves(i); return; }
}

// 1.5.0: a member left the party - level-ups waiting for them (or pointing past them) must not point at someone else
void Game::dropMemberRefs(size_t idx) {
    levelUpQueue.erase(std::remove(levelUpQueue.begin(), levelUpQueue.end(), (int)idx), levelUpQueue.end());
    for (auto& q : levelUpQueue) if (q > (int)idx) q--;
    if (banterMember == (int)idx) banterT = 0; else if (banterMember > (int)idx) banterMember--;
}

int Game::memberOfClass(const std::string& cls) const { for (size_t i = 1; i < party.members.size(); i++) if (party.members[i].classId == cls) return (int)i; return -1; }

// 1.5.0 (#87/#88): a companion's red line crossed and not talked round - fell=CLASS|text. They are gone for good (off
// screen): gear goes to the pack, joined_<class> is cleared, fell_<class> is set, and their NPC stays hidden at home.
void Game::companionFalls(size_t idx, const std::string& why) {
    if (idx == 0 || idx >= party.members.size()) return;
    Character m = party.members[idx]; std::string cls = m.classId, kept;
    for (int s = 0; s < SL_COUNT; s++) if (!m.equip[s].empty() && gd->item(m.equip[s])) { party.addItem(m.equip[s], 1, *gd); if (auto* d = gd->item(m.equip[s])) kept += (kept.empty() ? "" : ", ") + d->name; }
    party.members.erase(party.members.begin() + idx);
    if (leader == (int)idx) leader = 0; else if (leader > (int)idx) leader--;
    dropMemberRefs(idx);
    flags.erase("joined_" + cls); setFlag("fell_" + cls);
    log(m.name + " is gone.");
    gAudio.play("death_party");
    std::string body = (trim(why).empty() ? m.name + " is gone, and will not be coming back." : trim(why)) + (kept.empty() ? std::string() : " Their gear stays with you: " + kept + ".");
    if (mode == GMode::DIALOGUE) { notify(m.name + " is gone."); log(body); } else showMessage(m.name, body);   // in a talk the scene itself tells it
}

void Game::companionLeaves(size_t idx, const std::string& why) {
    if (idx == 0 || idx >= party.members.size()) return;
    Character m = party.members[idx]; std::string cls = m.classId;
    std::string kept;
    for (int s = 0; s < SL_COUNT; s++) if (!m.equip[s].empty() && gd->item(m.equip[s])) { party.addItem(m.equip[s], 1, *gd); if (auto* d = gd->item(m.equip[s])) kept += (kept.empty() ? "" : ", ") + d->name; }
    party.members.erase(party.members.begin() + idx);
    if (leader == (int)idx) leader = 0; else if (leader > (int)idx) leader--;
    dropMemberRefs(idx);
    flags.erase("joined_" + cls); setFlag("left_" + cls);
    // they walk back to where you met them
    auto h = extra.find("home_" + cls);
    if (h != extra.end()) extra.erase(h->second);
    else { auto dl = extra.find("dlg_" + cls); std::string did = dl == extra.end() ? "" : dl->second;
        if (!did.empty()) for (auto it = extra.begin(); it != extra.end();) { if (startsWith(it->first, "hidenpc_") && it->first.size() > did.size() && it->first.compare(it->first.size() - did.size() - 1, did.size() + 1, "_" + did) == 0) it = extra.erase(it); else ++it; } }
    log(m.name + " has left the party.");
    gAudio.play("ui_close");
    std::string body = (trim(why).empty() ? m.name + " has had enough of the way you do things, and goes back to where you first met." : trim(why)) +
        (kept.empty() ? std::string() : " Their gear stays with you: " + kept + ".") + " You could try to make amends there.";
    if (mode == GMode::DIALOGUE) { notify(m.name + " leaves the party."); log(body); } else showMessage(m.name + " leaves", body);   // 1.5.0: in a talk (leave=) the scene tells it
}

// ---------------- towns ----------------
int Game::repOf(const std::string& id) const { auto it = reputation.find(id); return it == reputation.end() ? 0 : it->second; }
const FactionDef* Game::faction(const std::string& id) const { for (auto& f : factions) if (f.id == id) return &f; return nullptr; }
int Game::projectsDone(const std::string& id) const { auto it = extra.find("project_" + id); return it == extra.end() ? 0 : atoi(it->second.c_str()); }
int Game::amendsCost(const std::string& id) const { int r = repOf(id); int lvl = party.members.empty() ? 1 : party.members[0].level; return r >= 0 ? 0 : -r * (6 + 2 * lvl); }

// Hated: the shops of that town turn you away
bool Game::shopRefuses(const std::string& shop) {
    std::string f = shopFactionOf(shop); if (f.empty() || repOf(f) > -50) return false;
    if (mode == GMode::DIALOGUE) popMode();
    showMessage("Not welcome", "\"We don't trade with your kind here.\" " + factionName(f) + " hates you. Make amends at the town's notice board first.");
    gAudio.play("ui_error");
    return true;
}

// rows of the standing page: -1 = amends, -2 = the current civic project, 0.. = favours
static std::vector<int> standingRows(const Game& g) {
    std::vector<int> rows; const FactionDef* f = g.faction(g.boardTown);
    if (g.repOf(g.boardTown) < 0) rows.push_back(-1);
    if (f && !f->projects.empty()) rows.push_back(-2);
    if (f) for (size_t i = 0; i < f->rewards.size(); i++) rows.push_back((int)i);
    return rows;
}
static std::string itemsText(const Game& g, const std::string& csv) {
    std::string s;
    for (auto& p : split(csv, ',')) { auto kv = split(trim(p), ':'); std::string id = trim(kv[0]); int n = kv.size() > 1 ? atoi(kv[1].c_str()) : 1; if (id.empty()) continue;
        std::string part = id == "gold" ? fmt("%d gold", n) : g.gd->item(id) ? itemLabel(*g.gd->item(id), n) : id; s += (s.empty() ? "" : ", ") + part; }
    return s;
}

void Game::updateStanding(InputState& in) {
    auto rows = standingRows(*this); int n = (int)rows.size();
    if (n > 0) { if (actP(in, A_UP)) standSel = (standSel + n - 1) % n; if (actP(in, A_DOWN)) standSel = (standSel + 1) % n; }
    standSel = clampi(standSel, 0, std::max(0, n - 1));
    bool click = false;
    for (int i = 0; i < n; i++) if (in.mouseX >= 30 && in.mouseX < SCREEN_W - 30 && in.mouseY >= 150 + i * 46 && in.mouseY < 192 + i * 46) { if (in.mouseDX || in.mouseDY) standSel = i; if (in.mousePressed[0]) { standSel = i; click = true; } }
    if (n == 0 || !(actP(in, A_CONFIRM) || click)) return;
    const FactionDef* f = faction(boardTown); int r = repOf(boardTown); int row = rows[standSel];
    if (row == -1) {
        int cost = amendsCost(boardTown);
        if (party.gold < cost) { log(fmt("Making amends costs %d gold.", cost)); gAudio.play("ui_error"); return; }
        party.gold -= cost; stats.goldSpent += cost; changeRep(boardTown, -r);
        log(fmt("You pay %d gold in fines and damages. %s will deal with you again.", cost, factionName(boardTown).c_str())); gAudio.play("gold");
    } else if (row == -2 && f) {
        int done = projectsDone(boardTown); if (done >= (int)f->projects.size()) { log("Every civic project here is finished."); return; }
        const FactionProject& p = f->projects[done];
        if (r <= -10) { log("They won't take your help while they dislike you. Make amends first."); gAudio.play("ui_error"); return; }
        for (auto& m : p.mats) if (party.countItem(m.first) < m.second) { const ItemDef* d = gd->item(m.first); log(fmt("The project needs %d %s (you have %d).", m.second, d ? d->name.c_str() : m.first.c_str(), party.countItem(m.first))); gAudio.play("ui_error"); return; }
        for (auto& m : p.mats) party.removeItem(m.first, m.second);
        extra["project_" + boardTown] = std::to_string(done + 1);
        if (p.gold > 0) { party.gold += p.gold; stats.goldEarned += p.gold; }
        giveXP(p.xp); changeRep(boardTown, p.rep);
        log("Project complete: " + p.title + (p.gold > 0 ? fmt(". The town pays you %d gold.", p.gold) : ".")); gAudio.play("town_reward");
        setFlag("project_" + boardTown + "_" + std::to_string(done + 1));
    } else if (row >= 0 && f && row < (int)f->rewards.size()) {
        const FactionReward& w = f->rewards[row]; std::string key = fmt("favour_%s_%d", boardTown.c_str(), row);
        if (extra.count(key)) { log("You already have that."); return; }
        if (r < w.rep) { log(fmt("Needs %s standing (%d). You have %d.", repTier(w.rep), w.rep, r)); gAudio.play("ui_error"); return; }
        extra[key] = "1";
        for (auto& p : split(w.items, ',')) { auto kv = split(trim(p), ':'); std::string id = trim(kv[0]); int cnt = kv.size() > 1 ? atoi(kv[1].c_str()) : 1; if (id.empty()) continue;
            if (id == "gold") { party.gold += cnt; stats.goldEarned += cnt; } else if (gd->item(id)) giveItem(id, cnt); }
        log((w.title.empty() ? std::string("A favour from ") + factionName(boardTown) : w.title) + ": " + itemsText(*this, w.items) + "."); gAudio.play("town_reward");
    }
}

void Game::drawStanding(Canvas& cv, InputState& in) {
    const FactionDef* f = faction(boardTown); int r = repOf(boardTown);
    Color8 ink = RGB(40, 25, 10), soft = RGB(70, 50, 30);
    std::string name = factionName(boardTown);
    // the header sits on the dark board; the rows below are paper notices
    Color8 hd = RGB(220, 200, 160), good = RGB(130, 220, 130), bad = RGB(240, 130, 110);
    ctx.font->bold = true; UI::text(ctx, 34, 56, name + ": " + repTier(r) + fmt(" (%+d)", r), C::Cream); ctx.font->bold = false;
    cv.fillRect(SCREEN_W - 236, 58, 200, 8, RGB(120, 100, 70)); cv.fillRect(SCREEN_W - 236, 58, clampi((r + 100), 0, 200), 8, r >= 0 ? RGB(60, 120, 170) : RGB(170, 60, 50));
    int buy = clampi(-r / 4, -15, 25);
    std::vector<std::pair<std::string, Color8>> lines = {
        {buy < 0 ? fmt("Shops give you %d%% off.", -buy) : buy > 0 ? fmt("Shops charge you %d%% more.", buy) : "Shops charge the usual prices.", buy <= 0 ? good : bad},
        {r <= -50 ? "Hated: the shops here won't trade with you at all." : r <= -10 ? "Disliked: nobody here will hire you - the board posts no work." : r >= 30 ? "Friendly: some shops show you stock they keep for friends." : "The board posts work every morning.", r <= -10 ? bad : hd},
        {r >= 30 ? "Odd jobs no longer raise your standing here: civic projects and your choices do." : "Odd jobs raise your standing up to Friendly (30); past that it takes civic projects.", hd}};
    int y = 74; for (auto& l : lines) { UI::text(ctx, 34, y, l.first, l.second); y += 13; }
    if (f && !f->text.empty()) { wrapped(ctx, 34, y + 2, SCREEN_W - 68, f->text, RGB(170, 150, 115)); }
    auto rows = standingRows(*this);
    if (rows.empty()) textCenter(ctx, SCREEN_W / 2, 170, "This town asks nothing of you, and offers nothing in return.", hd);
    for (size_t i = 0; i < rows.size() && i < 6; i++) {
        int yy = 150 + (int)i * 46; bool sel = (int)i == standSel; int row = rows[i];
        std::string title, body, status, right; Color8 stc = soft; bool open = true;
        if (row == -1) { int cost = amendsCost(boardTown); title = "MAKE AMENDS"; body = "Pay fines and damages, and the town treats you as a stranger again (Neutral, 0)."; status = party.gold >= cost ? "Enter to pay" : "You can't afford it yet"; right = fmt("Cost: %d gold", cost); stc = party.gold >= cost ? RGB(20, 120, 40) : RGB(150, 40, 30); }
        else if (row == -2) {
            int done = projectsDone(boardTown);
            if (!f || done >= (int)f->projects.size()) { title = "CIVIC PROJECTS - ALL DONE"; body = "The town remembers what you built for it."; status = fmt("%d finished", done); open = false; }
            else { const FactionProject& p = f->projects[done]; title = fmt("CIVIC PROJECT %d/%d: ", done + 1, (int)f->projects.size()) + p.title; body = p.text;
                std::string need; bool all = true; for (auto& m : p.mats) { const ItemDef* d = gd->item(m.first); int have = party.countItem(m.first); if (have < m.second) all = false; need += (need.empty() ? "" : ", ") + fmt("%s %d/%d", d ? d->name.c_str() : m.first.c_str(), std::min(have, m.second), m.second); }
                status = "Needs: " + need; stc = all && r > -10 ? RGB(20, 120, 40) : RGB(120, 60, 20); right = fmt("+%d standing", p.rep) + (p.gold > 0 ? fmt(", %d gold", p.gold) : ""); }
        } else if (f) {
            const FactionReward& w = f->rewards[row]; bool got = extra.count(fmt("favour_%s_%d", boardTown.c_str(), row)) > 0;
            title = std::string("FAVOUR (") + repTier(w.rep) + fmt(" %d): ", w.rep) + (w.title.empty() ? "A gift" : w.title); body = itemsText(*this, w.items);
            status = got ? "Claimed" : r >= w.rep ? "Enter to claim it" : fmt("Locked - needs %d standing", w.rep); stc = got ? RGB(110, 100, 80) : r >= w.rep ? RGB(20, 120, 40) : RGB(150, 40, 30); open = !got;
        }
        Color8 paper = open ? RGB(225, 215, 180) : RGB(195, 190, 170);
        cv.fillRect(30, yy, SCREEN_W - 60, 42, sel ? RGB(250, 240, 205) : paper); cv.rect(30, yy, SCREEN_W - 60, 42, sel ? C::Gold : RGB(120, 100, 60));
        ctx.font->bold = true; UI::text(ctx, 40, yy + 3, title, ink); ctx.font->bold = false;
        UI::text(ctx, 40, yy + 15, body.size() > 96 ? body.substr(0, 93) + "..." : body, RGB(60, 45, 25));
        UI::text(ctx, 40, yy + 27, status, stc); if (!right.empty()) textRight(ctx, SCREEN_W - 40, yy + 27, right, RGB(120, 80, 10));
    }
    keyHint(ctx, 30, SCREEN_H - 40, "Enter", "pay / deliver / claim"); keyHint(ctx, 230, SCREEN_H - 40, "Tab", "notices"); keyHint(ctx, 340, SCREEN_H - 40, "Esc", "leave");
    (void)in;
}
