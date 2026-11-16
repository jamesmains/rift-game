// The ending: after the final win= cutscene, epilogue slides (what became of the world, the towns and every companion,
// chosen by flags, reputation and approval from assets/data/epilogue.txt), the journey in numbers, the credits
// (assets/data/credits.txt), then "keep exploring" or back to the desktop.
#include "game.h"
#include "../core/audio.h"
#include "../core/version.h"
#include "tiles.h"
#include <algorithm>
#include <cmath>

using namespace UI;

std::string Game::endingText(const std::string& raw) const {
    std::string s = raw; size_t p;
    while ((p = s.find("$hero")) != std::string::npos) s.replace(p, 5, heroName);
    auto rep = [&](const std::string& key, const std::string& val) { size_t q; while ((q = s.find(key)) != std::string::npos) s.replace(q, key.size(), val); };
    rep("{days} days", day == 1 ? std::string("one day") : std::to_string(day) + " days"); rep("{days}", std::to_string(day)); rep("{kills}", std::to_string(stats.kills)); rep("{level}", std::to_string(party.members.empty() ? 1 : party.members[0].level));
    rep("{campaign}", campaign.name);
    return questCounters(s);
}

void Game::startEnding() {
    endSlides.clear(); endPage = 0; endT = 0; endSel = 0; endScroll = 0;
    std::set<std::string> groupsDone;
    for (auto& sec : mergedConfig("epilogue.txt")) {
        if (sec.name != "slide") continue;
        std::string group = sec.get("group");
        if (!group.empty() && groupsDone.count(group)) continue;          // the first slide of a group whose if= holds wins
        if (sec.has("if") && !evalCond(sec.get("if"))) continue;
        EpilogueSlide sl; sl.group = group; sl.title = endingText(sec.get("title")); sl.text = endingText(sec.get("text"));
        if (sec.has("tile")) { sl.tile = parseTile(sec.get("tile")); sl.hasTile = true; }
        if (sec.has("color")) { sl.color = parseColor(sec.get("color")); sl.recolor = true; }
        // a companion's slide shows their portrait as it is in the party
        if (sec.has("member")) for (auto& m : party.members) if (m.classId == sec.get("member")) { sl.tile = m.tile; sl.hasTile = true; }
        if (!group.empty()) groupsDone.insert(group);
        if (!sl.text.empty()) endSlides.push_back(sl);
    }
    creditLines.clear();
    std::string ct; if (readFile(dataPath("credits.txt"), ct)) for (auto& l : split(ct, '\n')) { std::string t = trim(l); if (!t.empty() && t[0] == '#') continue; creditLines.push_back(endingText(t)); }
    if (creditLines.empty()) creditLines = {"= " + campaign.name, "", "Thank you for playing."};
    modeStack.clear(); mode = GMode::ENDING;
}

// pages: 0..n-1 slides, n = the journey in numbers, n+1 = credits, n+2 = keep exploring / desktop
void Game::updateEnding(InputState& in, float dt) {
    endT += dt;
    int n = (int)endSlides.size(), stat = n, credits = n + 1, last = n + 2;
    bool next = actP(in, A_CONFIRM) || actP(in, A_INTERACT) || in.mousePressed[0];
    if (endPage == credits) {
        endScroll += dt * (actD(in, A_DOWN) || actD(in, A_CONFIRM) ? 120.0f : 28.0f);
        float total = SCREEN_H; for (auto& l : creditLines) total += startsWith(l, "= ") ? 30 : 18;
        if (endScroll > total || actP(in, A_CANCEL)) { endPage = last; endT = 0; return; }
        if (next && endT > 1.0f && endScroll > 200) { endPage = last; endT = 0; }
        return;
    }
    if (endPage == last) {
        if (actP(in, A_UP) || actP(in, A_DOWN) || actP(in, A_LEFT) || actP(in, A_RIGHT)) endSel = 1 - endSel;
        int bx = SCREEN_W / 2 - 150, by = 300;
        bool kbNext = actP(in, A_CONFIRM) || actP(in, A_INTERACT); bool clickHit = false;
        for (int i = 0; i < 2; i++) if (in.mouseX >= bx + i * 160 && in.mouseX < bx + i * 160 + 140 && in.mouseY >= by && in.mouseY < by + 26) { if (in.mouseDX || in.mouseDY) endSel = i; if (in.mousePressed[0]) { endSel = i; clickHit = true; } }
        next = kbNext || clickHit;   // a click outside the buttons does nothing
        if (next && endT > 0.5f) {
            if (endSel == 0) {   // keep exploring: back on the map, the world is still open
                mode = GMode::EXPLORE; modeStack.clear(); fadeIn = 1.0f;
                log("The Rift is quiet. The world is still yours to wander - unfinished quests, bounties and all.");
                lastAutosave = -1000; autosave();
            } else requestDesktop = true;
        }
        return;
    }
    if (actP(in, A_CANCEL) && endPage < stat) { endPage = stat; endT = 0; return; }   // skip the slides
    if (next && endT > 0.6f) { endPage++; endT = 0; gAudio.play("ui_confirm"); }
}

void Game::drawEnding(Canvas& cv, InputState& in) {
    cv.clear(RGB(6, 6, 12));
    // a slow starfield: the sky after the Rift
    for (int i = 0; i < 90; i++) {
        uint32_t h = (uint32_t)i * 2654435761u; int sx = (int)(h % SCREEN_W), sy = (int)((h >> 12) % SCREEN_H);
        int y = (int)(sy + ctx.time * (4 + (h >> 24) % 6)) % SCREEN_H; uint8_t b = (uint8_t)(70 + (h >> 20) % 120);
        cv.put(sx, y, RGB(b, b, (uint8_t)std::min(255, b + 30)));
    }
    int n = (int)endSlides.size(), stat = n, credits = n + 1, last = n + 2;
    float fade = std::min(1.0f, endT * 2.5f); (void)fade;
    auto hint = [&](const std::string& s) { if (endT > 0.6f) textCenter(ctx, SCREEN_W / 2, SCREEN_H - 28, s, C::DkGray); };
    if (endPage < n) {
        const EpilogueSlide& sl = endSlides[endPage];
        std::vector<std::vector<std::string>> paras; int hgt = (sl.hasTile ? 64 : 0) + 40;
        int ts = textScale(), lh = ts == 2 ? 26 : 16;
        for (auto& para : split(sl.text, '|')) { paras.push_back(ctx.font->wrap(trim(para), ts == 2 ? 600 : 480, ts)); hgt += (int)paras.back().size() * lh + 8; }
        int y = std::max(40, (SCREEN_H - 70 - hgt) / 2);
        if (sl.hasTile) { ctx.ts->draw(cv, sl.tile, SCREEN_W / 2 - 24, y, 4, sl.recolor ? sl.color : C::White); y += 64; }
        ctx.font->bold = true; textCenter(ctx, SCREEN_W / 2, y, sl.title, C::Gold, 2); ctx.font->bold = false; y += 40;
        for (auto& lines : paras) { for (auto& l : lines) { textCenter(ctx, SCREEN_W / 2, y, l, C::Cream, ts); y += lh; } y += 8; }
        textCenter(ctx, SCREEN_W / 2, SCREEN_H - 48, fmt("%d / %d", endPage + 1, n), RGB(70, 70, 90));
        hint(gPadGlyphs ? "A: next   B: skip to the end" : "[Enter] next   [Esc] skip to the end");
    } else if (endPage == stat) {
        ctx.font->bold = true; textCenter(ctx, SCREEN_W / 2, 60, "The Journey", C::Gold, 2); ctx.font->bold = false;
        int h = playtimeSec / 3600, m = (playtimeSec / 60) % 60;
        int done = 0, failed = 0, total = 0; for (auto& q : quests) { std::string st = questStatus(q); if (st.empty()) continue; total++; if (startsWith(st, "DONE")) done++; else if (startsWith(st, "FAILED")) failed++; }
        std::vector<std::pair<std::string, std::string>> rows = {
            {"Time played", fmt("%d h %02d min", h, m)}, {"Days in Aldmoor", std::to_string(day)},
            {"Level", std::to_string(party.members.empty() ? 1 : party.members[0].level)}, {"Quests finished", fmt("%d of %d begun%s", done, total, failed ? fmt(", %d failed", failed).c_str() : "")},
            {"Foes defeated", std::to_string(stats.kills)}, {"Battles won", std::to_string(stats.fightsWon)}, {"Battles fled", std::to_string(stats.fightsFled)},
            {"Party knock-outs", std::to_string(stats.knockouts)}, {"Damage dealt", std::to_string(stats.damageDealt)}, {"Critical hits", std::to_string(stats.crits)},
            {"Gold earned", std::to_string(stats.goldEarned)}, {"Chests opened", std::to_string(stats.chests)}, {"Items crafted", std::to_string(stats.crafted)},
            {"Bounties paid", std::to_string(stats.bounties)}, {"Achievements", fmt("%d of %d", (int)unlocked.size(), (int)achievements.size())}};
        int y = 110;
        for (auto& r : rows) { textRight(ctx, SCREEN_W / 2 - 10, y, r.first, C::LtGray); UI::text(ctx, SCREEN_W / 2 + 10, y, r.second, C::White); y += 18; }
        int px0 = SCREEN_W / 2 - (int)party.members.size() * 30;
        for (size_t i = 0; i < party.members.size(); i++) drawUnitSprite(cv, party.members[i], px0 + (int)i * 60 + 6, y + 14, false);
        hint(gPadGlyphs ? "A: credits" : "[Enter] credits");
    } else if (endPage == credits) {
        int y = SCREEN_H + 10 - (int)endScroll;
        for (auto& l : creditLines) {
            if (y > -20 && y < SCREEN_H) {
                if (startsWith(l, "= ")) { ctx.font->bold = true; textCenter(ctx, SCREEN_W / 2, y, l.substr(2), C::Gold, 2); ctx.font->bold = false; }
                else if (startsWith(l, "- ")) textCenter(ctx, SCREEN_W / 2, y, l.substr(2), C::Gold);
                else textCenter(ctx, SCREEN_W / 2, y, l, C::Cream);
            }
            y += startsWith(l, "= ") ? 30 : 18;
        }
    } else if (endPage == last) {
        ctx.font->bold = true; textCenter(ctx, SCREEN_W / 2, 150, "The End", C::Gold, 3); ctx.font->bold = false;
        textCenter(ctx, SCREEN_W / 2, 210, "Thank you for playing " + campaign.name + ".", C::Cream);
        textCenter(ctx, SCREEN_W / 2, 230, "Keep exploring: the world stays open (an autosave is made first).", C::LtGray);
        int bx = SCREEN_W / 2 - 150, by = 300;
        button(ctx, in, bx, by, 140, 26, "Keep exploring", true, endSel == 0);
        button(ctx, in, bx + 160, by, 140, 26, "Return to desktop", true, endSel == 1);
        selCursor(ctx, bx + endSel * 160 - 2, by, 26);
    }
    (void)last;
}
