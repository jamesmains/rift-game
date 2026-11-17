// Notice boards: procedurally generated bounties (kill / fetch / deliver) per town.
// A board posts up to three offers a day. Taking one moves it to your bounties (at most 3 at a time); only taken
// bounties count kills, and a collected or dropped bounty's slot stays empty until the board refreshes the next day.
// Courier jobs never send you back to the board that sent a letter to here, so two towns can't ping-pong.
#include "game.h"
#include "../core/audio.h"
#include "tiles.h"
#include <algorithm>
#include <optional>
#include <filesystem>

using namespace UI;

static const int MAX_TAKEN = 3;

// ---- boards: bounties.txt, or (without one) every board= the maps and dialogue mention, with default settings ----
static std::vector<std::string> csvList(const std::string& v) { std::vector<std::string> o; for (auto& p : split(v, ',')) if (!trim(p).empty()) o.push_back(trim(p)); return o; }
void Game::loadBoards() {
    boards.clear(); fixedBounties.clear();
    for (auto& sec : mergedConfig("bounties.txt")) {
        if (sec.name == "board" && !sec.get("id").empty()) {
            BoardDef d; d.fromFile = true; d.id = sec.get("id"); d.name = sec.get("name"); d.map = sec.get("map", d.id); d.location = sec.get("location");
            d.offers = clampi(sec.geti("offers", 3), 0, 6); d.reward = std::max(1, sec.geti("reward", 100)); d.rep = sec.geti("rep", 3);
            if (sec.has("kinds")) d.kinds = csvList(sec.get("kinds"));
            d.kill = csvList(sec.get("kill")); d.creatures = csvList(sec.get("creatures")); d.fetch = csvList(sec.get("fetch")); d.deliver = csvList(sec.get("deliver"));
            bool dup = false; for (auto& o : boards) if (o.id == d.id) { o = d; dup = true; } if (!dup) boards.push_back(d);
        } else if (sec.name == "bounty" && !sec.get("id").empty()) {
            FixedBountyDef f; f.id = sec.get("id"); f.board = sec.get("board"); f.kind = sec.get("kind", "kill"); f.target = sec.get("target"); f.where = sec.get("where");
            f.title = sec.get("title"); f.text = sec.get("text"); f.require = sec.get("require"); f.need = std::max(1, sec.geti("need", 1)); f.reward = sec.geti("reward", 30); f.xp = sec.geti("xp", 20); f.repeat = sec.geti("repeat", 0) != 0;
            fixedBounties.push_back(f);
        }
    }
    if (!boards.empty()) return;
    // no bounties.txt: find the boards the campaign actually has (NPCs with board=, dialogue board=)
    std::vector<std::string> dirs; if (!campaignDir.empty()) dirs.push_back(campaignDir); if (campaignDir.empty() || campaign.inherit) dirs.push_back(assetDir + "/data");
    std::error_code ec;
    for (auto& d : dirs) for (const char* sub : {"/maps", "/dialogue"}) for (auto& e : std::filesystem::directory_iterator(d + sub, ec)) {
        std::string t; if (e.path().extension() != ".txt" || !readFile(e.path().string(), t)) continue;
        std::string map = e.path().stem().string(); bool isMap = std::string(sub) == "/maps";
        for (auto& raw : split(t, '\n')) { std::string l = trim(raw); if (!startsWith(l, "board=")) continue; std::string id = trim(l.substr(6)); if (id.empty()) continue;
            bool have = false; for (auto& b : boards) if (b.id == id) { have = true; if (isMap) b.map = map; } if (have) continue;
            BoardDef b; b.id = id; b.map = isMap ? map : id; boards.push_back(b); }
    }
}
const BoardDef* Game::board(const std::string& id) const { for (auto& b : boards) if (b.id == id) return &b; return nullptr; }
std::string Game::boardName(const std::string& id) const {
    const BoardDef* b = board(id); if (b && !b->name.empty()) return b->name;
    std::string loc = b ? b->location : ""; for (auto& l : locations) if ((!loc.empty() && l.id == loc) || l.mapId == (b ? b->map : id) || l.id == id) return l.name;
    return id;
}

void Game::generateBounties(const std::string& town) {
    // a board posts once per day: it only fills empty slots when the day has changed since its last posting
    std::string dayKey = "board_day_" + town;
    auto dk = extra.find(dayKey);
    int lastDay = dk == extra.end() ? -1 : atoi(dk->second.c_str());
    if (lastDay == day) return;
    extra[dayKey] = std::to_string(day);
    // yesterday's untaken offers are taken down
    bountyOffers.erase(std::remove_if(bountyOffers.begin(), bountyOffers.end(), [&](const Bounty& b) { return b.town == town; }), bountyOffers.end());
    BoardDef def; if (const BoardDef* bd = board(town)) def = *bd; else { def.id = town; def.map = town; }
    auto locOf = [&](const BoardDef& d) -> const WorldLocation* { for (auto& l : locations) if (!d.location.empty() && l.id == d.location) return &l; for (auto& l : locations) if (l.mapId == d.map || l.id == d.id) return &l; return nullptr; };
    auto boardName = [&](const BoardDef& d) { if (!d.name.empty()) return d.name; if (auto* l = locOf(d)) return l->name; return d.id; };
    auto has = [&](const char* k) { return std::find(def.kinds.begin(), def.kinds.end(), k) != def.kinds.end(); };
    int have = 0;
    int lvl = party.hero() ? party.hero()->level : 1;
    int tx = 0, ty = 0; if (auto* l = locOf(def)) { tx = l->x; ty = l->y; }
    // ---- hand-written notices first ----
    for (auto& f : fixedBounties) {
        if (f.board != town || have >= 3 || f.kind == "note") continue;   // at most three hand-written notices up at once (1.5.0: notes go up in postBoardNotes)
        if (!f.repeat && extra.count("bounty_done_" + f.id)) continue;
        if (!f.require.empty() && !evalCond(f.require)) continue;
        bool busy = false; for (auto& o : bounties) if (o.id == "fix:" + f.id) busy = true; if (busy) continue;
        Bounty b; b.id = "fix:" + f.id; b.town = town; b.kind = f.kind; b.target = f.target; b.need = f.need; b.reward = f.reward; b.xp = f.xp; b.posted = day; b.title = f.title; b.text = f.text;
        if (f.kind == "kill") { const CreatureDef* cd = gd->creature(f.target); if (!cd) continue; b.targetName = cd->name; }
        else if (f.kind == "fetch") { const ItemDef* d = gd->item(f.target); if (!d) continue; b.targetName = d->name; }
        else if (f.kind == "deliver") { const BoardDef* to = board(f.target); if (!to || to->id == town) continue; b.need = 1; b.targetName = "the notice board in " + boardName(*to); b.whereName = boardName(*to); if (auto* l = locOf(*to)) b.where = l->id; }
        else continue;
        if (!f.where.empty() && f.kind != "deliver") { b.where = f.where; b.whereName = f.where; for (auto& l : locations) if (l.id == f.where || l.mapId == f.where) { b.where = l.id; b.whereName = l.name; } for (auto& sec : mergedConfig("dungeons.txt")) if (sec.name == "dungeon" && sec.get("id") == f.where) b.whereName = sec.get("name", f.where); }
        bountyOffers.push_back(b); have++;
    }
    // ---- kill jobs: creatures from the listed dungeons (default: the 4 nearest within the hero's reach) ----
    struct D { std::string id, name; std::vector<std::string> pool; int dist; };
    std::vector<D> ds;
    for (auto& sec : mergedConfig("dungeons.txt")) {
        if (sec.name != "dungeon") continue;
        D d; d.id = sec.get("id"); d.name = sec.get("name", d.id); for (auto& m : split(sec.get("monsters"), ',')) if (gd->creature(trim(m))) d.pool.push_back(trim(m));
        d.dist = 999; for (auto& l : locations) if (l.mapId == d.id || l.id == d.id) d.dist = chebyshev(l.x, l.y, tx, ty);
        bool listed = std::find(def.kill.begin(), def.kill.end(), d.id) != def.kill.end();
        if (!d.pool.empty() && (listed || (def.kill.empty() && sec.geti("level", 1) <= lvl + 3))) ds.push_back(d);
    }
    std::sort(ds.begin(), ds.end(), [](const D& a, const D& b) { return a.dist < b.dist; });
    if (def.kill.empty() && ds.size() > 4) ds.resize(4);
    if (!def.creatures.empty()) { D d; d.id = def.kill.empty() ? "" : def.kill[0]; d.name = d.id; for (auto& l : locations) if (l.id == d.id || l.mapId == d.id) d.name = l.name; for (auto& c : def.creatures) if (gd->creature(c)) d.pool.push_back(c); ds.clear(); if (!d.pool.empty()) ds.push_back(d); }
    // ---- courier destinations: other boards whose town you have visited, never back to a board that sent a letter here ----
    std::vector<const BoardDef*> dests;
    for (auto& o : boards) {
        if (o.id == town) continue;
        if (!def.deliver.empty() && std::find(def.deliver.begin(), def.deliver.end(), o.id) == def.deliver.end()) continue;
        if (!visited.count(o.map)) continue;
        bool reverse = false;
        for (auto* lst : {&bounties, &bountyOffers}) for (auto& b : *lst) if (b.kind == "deliver" && ((b.town == o.id && b.target == town) || (b.town == town && b.target == o.id))) reverse = true;
        if (!reverse) dests.push_back(&o);
    }
    // ---- fetch jobs: the listed items (default: cheap crafting materials) ----
    std::vector<std::string> mats;
    if (!def.fetch.empty()) { for (auto& f : def.fetch) if (gd->item(f)) mats.push_back(f); }
    else for (auto& it : gd->items) if (it.second.type == IT_MATERIAL && it.second.value <= 40 && it.first.find('@') == std::string::npos) mats.push_back(it.first);
    int guard = 0, fixedN = have;
    while (have - fixedN < def.offers && guard++ < 40) {
        Bounty b; b.town = town; b.id = fmt("%s_%d_%d", town.c_str(), day, rng.range(1000, 9999)); b.posted = day;
        int kind = rng.range(0, 99);
        if (kind < 55 && has("kill") && !ds.empty()) {
            const D& d = ds[rng.range(0, (int)ds.size() - 1)]; std::string cr = d.pool[rng.range(0, (int)d.pool.size() - 1)];
            const CreatureDef* cd = gd->creature(cr); if (!cd || cd->boss) continue;
            b.kind = "kill"; b.target = cr; b.targetName = cd->name; b.need = rng.range(3, 6); b.where = d.id; b.whereName = d.name;
            b.reward = (cd->xp * b.need) / 3 + 15 + lvl * 5; b.xp = cd->xp * b.need / 3 + 10;
        } else if (kind < 80 && has("fetch") && !mats.empty()) {
            std::string it = mats[rng.range(0, (int)mats.size() - 1)]; const ItemDef* d = gd->item(it);
            b.kind = "fetch"; b.target = it; b.targetName = d->name; b.need = rng.range(2, 4); b.reward = std::max(12, d->value * b.need) + lvl * 4; b.xp = 15 + lvl * 6;
            if (!ds.empty() && def.fetch.empty()) { const D& dd = ds[rng.range(0, (int)ds.size() - 1)]; b.where = dd.id; b.whereName = dd.name; }
        } else if (has("deliver") && !dests.empty()) {
            const BoardDef* dest = dests[rng.range(0, (int)dests.size() - 1)]; const WorldLocation* dl = locOf(*dest);
            b.kind = "deliver"; b.target = dest->id; b.whereName = boardName(*dest); b.targetName = "the notice board in " + b.whereName; b.need = 1; b.where = dl ? dl->id : dest->map;
            b.reward = 15 + (dl ? chebyshev(tx, ty, dl->x, dl->y) / 2 : 10) + lvl * 3; b.xp = 12 + lvl * 5;
        } else if (!(has("kill") && !ds.empty()) && !(has("fetch") && !mats.empty()) && !(has("deliver") && !dests.empty())) break;   // nothing this board can post
        else continue;
        b.reward = b.reward * def.reward / 100;
        bool dup = false;
        for (auto* lst : {&bounties, &bountyOffers}) for (auto& o : *lst) if (o.town == town && o.kind == b.kind && o.target == b.target) dup = true;
        if (dup) continue;
        bountyOffers.push_back(b); have++;
    }
}

void Game::bountyEvent(const std::string& kind, const std::string& id, int n) {
    for (auto& b : bounties) {
        if (b.done) continue;
        if (b.kind == "kill" && kind == "kill" && b.target == id) { b.have = std::min(b.need, b.have + n); if (b.have >= b.need) { b.done = true; notify("Bounty complete: " + b.targetName + ". Return to " + boardName(b.town) + "."); gAudio.play("bounty"); } else notify(fmt("Bounty: %s %d/%d", b.targetName.c_str(), b.have, b.need)); }
        if (b.kind == "fetch" && (kind == "item" || kind == "craft")) { b.have = std::min(b.need, party.countItem(b.target)); if (b.have >= b.need && !b.done) { b.done = true; notify("Bounty: you have the " + b.targetName + ". Return to " + boardName(b.town) + "."); gAudio.play("bounty"); } }
        if (b.kind == "deliver" && kind == "board" && id == b.target) {
            if (!party.hasItem("courier_letter")) { notify("You've lost the letter for " + b.whereName + "."); continue; }
            party.removeItem("courier_letter", 1); b.done = true; notify("Letter delivered. Collect the reward in " + boardName(b.town) + "."); gAudio.play("bounty");
        }
    }
}

void Game::openBoard(const std::string& town) {
    boardTown = town; bountySel = 0; boardPage = 0; standSel = 0;
    bountyEvent("board", town);
    generateBounties(town);
    postBoardNotes(town);
    pushMode(GMode::BOUNTY);
}

// 1.5.0 (#98): hand-written notes (bounties.txt kind=note) - news pinned to the board while their require= holds, no job
// to take. Put up again every time the board is read (at most two, first in the list), so they follow the story at once.
void Game::postBoardNotes(const std::string& town) {
    bountyOffers.erase(std::remove_if(bountyOffers.begin(), bountyOffers.end(), [&](const Bounty& b) { return b.town == town && b.kind == "note"; }), bountyOffers.end());
    std::vector<Bounty> notes;
    for (auto& f : fixedBounties) {
        if (f.board != town || f.kind != "note" || notes.size() >= 2) continue;
        if (!f.require.empty() && !evalCond(f.require)) continue;
        Bounty b; b.id = "note:" + f.id; b.town = town; b.kind = "note"; b.need = 0; b.reward = 0; b.xp = 0; b.posted = day; b.title = f.title.empty() ? "NOTICE" : f.title; b.text = f.text;
        notes.push_back(b);
    }
    bountyOffers.insert(bountyOffers.begin(), notes.begin(), notes.end());
}

// rows on a board: bounties from this town you've taken (in progress / complete), then today's offers
static void boardRows(const Game& g, std::vector<std::pair<int, bool>>& rows) {   // (index, isOffer)
    rows.clear();
    for (size_t i = 0; i < g.bounties.size(); i++) if (g.bounties[i].town == g.boardTown) rows.push_back({(int)i, false});
    if (g.repOf(g.boardTown) > -10)   // Disliked: nobody here will hire you
        for (size_t i = 0; i < g.bountyOffers.size(); i++) if (g.bountyOffers[i].town == g.boardTown) rows.push_back({(int)i, true});
}

void Game::updateBounty(InputState& in, float dt) {
    (void)dt;
    if (actP(in, A_CANCEL)) { popMode(); return; }
    if (actP(in, A_JOURNAL)) { jTab = 1; jSel = 0; jScroll = 0; pushMode(GMode::JOURNAL); return; }   // 1.8.4 (#193): the hint's "J journal" (its Bounties page; Esc comes back here)
    // two pages: today's notices and the town's standing (favours, civic projects, amends)
    if (actP(in, A_PAGEUP) || actP(in, A_PAGEDOWN) || actP(in, A_MENU)) { boardPage = 1 - boardPage; gAudio.play("ui_move"); return; }
    if (in.mousePressed[0] && in.mouseY >= 24 && in.mouseY < 38) { if (in.mouseX >= 30 && in.mouseX < 100) { boardPage = 0; return; } if (in.mouseX >= 104 && in.mouseX < 180) { boardPage = 1; return; } }
    if (boardPage == 1) { updateStanding(in); return; }
    std::vector<std::pair<int, bool>> rows; boardRows(*this, rows);
    int n = (int)rows.size();
    if (n > 0) { if (actP(in, A_UP)) bountySel = (bountySel + n - 1) % n; if (actP(in, A_DOWN)) bountySel = (bountySel + 1) % n; }
    bountySel = clampi(bountySel, 0, std::max(0, n - 1));
    bool click = false;
    for (int i = 0; i < n; i++) if (in.mouseX >= 30 && in.mouseX < SCREEN_W - 30 && in.mouseY >= 56 + i * 58 && in.mouseY < 110 + i * 58) { if (in.mouseDX || in.mouseDY) bountySel = i; if (in.mousePressed[0]) { bountySel = i; click = true; } }
    if (n == 0) return;
    auto row = rows[bountySel];
    if (actP(in, A_INTERACT) && !row.second) {   // drop a taken bounty
        Bounty& b = bounties[row.first];
        if (b.kind == "deliver") party.removeItem("courier_letter", 1);
        log("You take the notice down: " + b.targetName + ". Word gets around that you didn't keep your word."); std::string bt = b.town; bounties.erase(bounties.begin() + row.first); gAudio.play("ui_close");
        if (board(bt)) changeRep(bt, -2);
        return;
    }
    if (!(actP(in, A_CONFIRM) || click)) return;
    if (row.second && bountyOffers[row.first].kind == "note") { log(bountyOffers[row.first].text); return; }   // 1.5.0 (#98): just news
    if (row.second) {
        int taken = 0; for (auto& b : bounties) if (!b.done) taken++;
        if (taken >= MAX_TAKEN) { log(fmt("You already carry %d bounties. Finish or drop one first.", MAX_TAKEN)); gAudio.play("ui_error"); return; }
        Bounty b = bountyOffers[row.first]; b.taken = true;
        bountyOffers.erase(bountyOffers.begin() + row.first);
        if (b.kind == "deliver") { giveItem("courier_letter", 1, true); log("You take the sealed letter for " + b.whereName + "."); }
        if (b.kind == "fetch") { b.have = std::min(b.need, party.countItem(b.target)); if (b.have >= b.need) b.done = true; }
        bounties.push_back(b); gAudio.play("quest");
        log("Bounty taken. Progress shows in the Journal (" + gKeys.label(A_JOURNAL) + ").");
        return;
    }
    Bounty& b = bounties[row.first];
    if (b.done) {
        if (b.kind == "fetch") { if (party.countItem(b.target) < b.need) { b.done = false; b.have = party.countItem(b.target); log("You no longer have the items."); return; } party.removeItem(b.target, b.need); }
        party.gold += b.reward; stats.goldEarned += b.reward; stats.bounties++; giveXP(b.xp);
        if (startsWith(b.id, "fix:")) extra["bounty_done_" + b.id.substr(4)] = std::to_string(day);
        log(fmt("Bounty paid: %d gold.", b.reward)); gAudio.play("bounty_paid");
        if (const BoardDef* bd = board(b.town)) { int now = repOf(bd->id), d = std::min(bd->rep, 30 - now); if (d > 0) changeRep(bd->id, d); }   // the town remembers who helps it - up to Friendly
        bounties.erase(bounties.begin() + row.first);
    } else if (b.kind == "fetch") { b.have = std::min(b.need, party.countItem(b.target)); if (b.have >= b.need) { b.done = true; log("You already have what they need."); } else log(fmt("Bring %d %s (you have %d).", b.need, b.targetName.c_str(), b.have)); }
    else log("Still open. Progress shows in the Journal (" + gKeys.label(A_JOURNAL) + ").");
}

void Game::drawBounty(Canvas& cv, InputState& in) {
    cv.dim(0, 0, SCREEN_W, SCREEN_H, 150); uiRect(0, 0, SCREEN_W, SCREEN_H);
    std::string tn = boardTown; for (auto& l : locations) if (l.mapId == boardTown || l.id == boardTown) tn = l.name; if (const BoardDef* bd = board(boardTown)) if (!bd->name.empty()) tn = bd->name;
    // 1.8.4 (#194): the board in the theme's look (frame: the cork board; native / one-bit: their own panel and cards)
    const BoardLook L = boardLook();
    std::optional<Theme::NoMap> exact; if (!L.cork) exact.emplace();   // the theme colours below are meant as they are
    boardPanel(ctx, 20, 20, SCREEN_W - 40, SCREEN_H - 40);
    ctx.font->bold = true; textCenter(ctx, SCREEN_W / 2, 28, "NOTICE BOARD - " + tn, L.head); ctx.font->bold = false;
    if (L.cork) for (int t = 0; t < 2; t++) { int tx = t == 0 ? 30 : 104, tw = t == 0 ? 70 : 76; bool on = boardPage == t; cv.fillRect(tx, 24, tw, 14, on ? RGB(160, 120, 60) : RGB(90, 65, 40)); cv.rect(tx, 24, tw, 14, on ? C::Gold : RGB(120, 90, 50)); textCenter(ctx, tx + tw / 2, 26, t == 0 ? "Notices" : "Standing", on ? C::Cream : RGB(200, 180, 140)); }
    else tabRow(ctx, {{30, 70, "Notices"}, {104, 76, "Standing"}}, 23, 16, boardPage);
    if (boardPage == 1) { drawStanding(cv, in); return; }
    int taken = 0; for (auto& b : bounties) if (!b.done) taken++;
    textCenter(ctx, SCREEN_W / 2, 41, fmt("Day %d. You carry %d/%d bounties. New notices go up each morning.", day, taken, MAX_TAKEN), L.sub);
    std::vector<std::pair<int, bool>> rows; boardRows(*this, rows);
    if (repOf(boardTown) <= -10) textCenter(ctx, SCREEN_W / 2, rows.empty() ? 80 : SCREEN_H - 56, "Nobody here will hire you while they dislike you. (Tab: Standing - make amends.)", L.cork ? RGB(230, 150, 120) : L.bad);
    if (rows.empty()) textCenter(ctx, SCREEN_W / 2, 100, "Nothing posted today. Check back tomorrow.", L.cork ? C::Gray : L.soft);
    for (size_t i = 0; i < rows.size() && i < 6; i++) {
        const Bounty& b = rows[i].second ? bountyOffers[rows[i].first] : bounties[rows[i].first];
        int y = 56 + (int)i * 58; bool sel = (int)i == bountySel;
        if (L.cork) { Color8 paper = rows[i].second ? RGB(225, 215, 180) : RGB(205, 225, 190); cv.fillRect(30, y, SCREEN_W - 60, 54, sel ? RGB(250, 240, 205) : paper); cv.rect(30, y, SCREEN_W - 60, 54, sel ? C::Gold : RGB(120, 100, 60)); cv.fillRect(SCREEN_W / 2 - 3, y - 3, 6, 6, RGB(180, 40, 40)); }   // pin
        else boardCard(ctx, 30, y, SCREEN_W - 60, 54, sel, true);
        auto ink = [&](Color8 c) { return boardInk(L, sel, c); };
        std::string title = b.kind == "kill" ? fmt("WANTED: %d %s", b.need, b.targetName.c_str()) : b.kind == "fetch" ? fmt("NEEDED: %d %s", b.need, b.targetName.c_str()) : "COURIER: carry a letter to " + b.whereName;
        if (!b.title.empty()) title = b.title;
        ctx.font->bold = true; UI::text(ctx, 40, y + 5, title, ink(L.ink)); ctx.font->bold = false;
        std::string body = b.kind == "kill" ? "They have been seen around " + b.whereName + "." : b.kind == "fetch" ? (b.whereName.empty() ? "Any source will do." : "Scavengers report some in " + b.whereName + ".") : "Pin it to the notice board there, then come back for the pay.";
        if (!b.text.empty()) body = b.text;
        if (b.kind != "note") UI::text(ctx, 40, y + 20, Theme::fit(Theme::Pen{&cv, ctx.font, ctx.ts}, body, SCREEN_W - 80), ink(L.body));   // 1.8.4: kept inside the card
        if (b.kind == "note") {   // 1.5.0 (#98): news, wrapped over two lines, nothing to take
            auto ls = ctx.font->wrap(b.text, SCREEN_W - 80); for (size_t k = 0; k < ls.size() && k < 2; k++) UI::text(ctx, 40, y + 20 + (int)k * 14, ls[k], ink(L.body));
            continue; }
        std::string st = rows[i].second ? "OFFER - Enter to take it" : b.done ? "COMPLETE - Enter to collect" : b.kind == "kill" ? fmt("taken - progress %d/%d", b.have, b.need) : b.kind == "fetch" ? fmt("taken - you carry %d/%d", party.countItem(b.target), b.need) : "taken - in progress";
        UI::text(ctx, 40, y + 36, st, ink(b.done ? L.good : rows[i].second ? L.warn : (L.cork ? RGB(40, 90, 60) : L.good)));
        textRight(ctx, SCREEN_W - 40, y + 36, fmt("Reward: %d gold, %d XP", b.reward, b.xp), ink(L.reward));
    }
    keyHint(ctx, 30, SCREEN_H - 40, "Enter", "take / collect"); keyHint(ctx, 170, SCREEN_H - 40, gKeys.label(A_INTERACT), "drop taken"); keyHint(ctx, 310, SCREEN_H - 40, "Esc", "leave"); keyHint(ctx, 400, SCREEN_H - 40, gKeys.label(A_JOURNAL), "journal"); keyHint(ctx, 500, SCREEN_H - 40, "Tab", "standing");
    (void)in;
}
