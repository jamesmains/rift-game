// 1.5.0 (#89): story contradictions - flag combinations that must never all be true at once, listed in
// data/contradictions.txt (a campaign's own file is added to the base game's, like the other data files):
//     goblin_chief_spared & goblin_chief_dead | Grakk can't be spared and killed
// Each line names two or more conditions joined by '&' (plain flags, !flag or flag>=N); text after '|' is a note.
// The game warns once per line (message log + lastScriptError) the moment a flag change makes the whole line true.
// The Editor's Check all looks for routes in the content that could do it (editor_contradictions.cpp).
#include "game.h"
#include <algorithm>

std::vector<Game::Contradiction> Game::parseContradictions(const std::string& text) {
    std::vector<Contradiction> out;
    for (auto& raw : split(text, '\n')) {
        std::string l = trim(raw); if (l.empty() || l[0] == '#') continue;
        Contradiction c; size_t bar = l.find('|');
        if (bar != std::string::npos) { c.note = trim(l.substr(bar + 1)); l = trim(l.substr(0, bar)); }
        for (auto& t : split(l, '&')) if (!trim(t).empty()) c.terms.push_back(trim(t));
        if (c.terms.size() >= 2) out.push_back(c);
    }
    return out;
}

// the flag a term reads: "!a" -> a, "a>=2" -> a
std::string Game::contradictionFlag(const std::string& term) {
    std::string t = trim(term); if (!t.empty() && t[0] == '!') t = trim(t.substr(1));
    size_t ge = t.find(">="); if (ge != std::string::npos) t = trim(t.substr(0, ge));
    return t;
}

void Game::loadContradictions() {
    contradictions.clear(); contradictionsWarned.clear();
    std::string t; bool base = campaignDir.empty() || campaign.inherit;
    if (base && readFile(assetDir + "/data/contradictions.txt", t)) contradictions = parseContradictions(t);
    if (!campaignDir.empty() && readFile(campaignDir + "/contradictions.txt", t)) { auto more = parseContradictions(t); contradictions.insert(contradictions.end(), more.begin(), more.end()); }
}

void Game::checkContradictions(const std::string& changed) {
    for (auto& c : contradictions) {
        bool mine = false; for (auto& t : c.terms) if (contradictionFlag(t) == changed) mine = true;
        if (!mine) continue;
        std::string all; for (auto& t : c.terms) all += (all.empty() ? "" : " & ") + t;
        if (contradictionsWarned.count(all)) continue;
        bool hit = true; for (auto& t : c.terms) if (!evalCond(t)) { hit = false; break; }
        if (!hit) continue;
        contradictionsWarned.insert(all);
        lastScriptError = "story contradiction: " + all + " are all true" + (c.note.empty() ? std::string() : " (" + c.note + ")");
        log("(" + lastScriptError + " - contradictions.txt)");
    }
}
