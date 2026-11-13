// Editor guide: assets/editor_guide.txt shown in a browsable help window. Small "?" buttons all over the editor open
// it on the topic for that tab or field; F1 opens it on the current tab.
//
// File format:  "== topic.id | Title"  starts a topic, "see: a, b" lists related topics, the rest is the body
// (blank line = paragraph, "- " = bullet, two leading spaces = example line shown as-is, "#" at column 0 = comment).
#include "editor.h"
#include <algorithm>

using namespace ED;

static const char* TAB_TOPICS[] = {"tab.maps", "tab.tiles", "tab.world", "tab.items", "tab.creatures", "tab.spells", "tab.classes", "tab.shops", "tab.encounters", "tab.quests",
                                   "tab.dungeons", "tab.dialogue", "tab.sounds", "tab.tileset", "tab.recipes", "tab.affixes", "tab.banter", "tab.achievements", "tab.campaign", "tab.flags", "tab.bounties", "tab.factions", "tab.objects", "tab.resources", "tab.npcs", "tab.scripts"};

void Editor::loadGuide() {
    guide.clear(); guideIdx.clear();
    std::string text; if (!readFile(assetDir + "/editor_guide.txt", text)) return;
    for (auto& raw : split(text, '\n')) {
        std::string l = raw; while (!l.empty() && (l.back() == '\r' || l.back() == ' ')) l.pop_back();
        if (!l.empty() && l[0] == '#') continue;
        if (startsWith(l, "== ")) {
            GuideTopic t; std::string rest = l.substr(3); size_t bar = rest.find('|');
            t.id = trim(bar == std::string::npos ? rest : rest.substr(0, bar)); t.title = trim(bar == std::string::npos ? rest : rest.substr(bar + 1));
            if (t.id.empty()) continue;
            guideIdx[t.id] = (int)guide.size(); guide.push_back(t); continue;
        }
        if (guide.empty()) continue;
        GuideTopic& t = guide.back();
        if (t.body.empty() && t.see.empty() && startsWith(l, "see:")) { for (auto& s : split(l.substr(4), ',')) if (!trim(s).empty()) t.see.push_back(trim(s)); continue; }
        if (t.body.empty() && trim(l).empty()) continue;
        t.body.push_back(l);
    }
    for (auto& t : guide) while (!t.body.empty() && trim(t.body.back()).empty()) t.body.pop_back();
}

std::string Editor::helpTopicFor(const std::string& topic) const {
    std::string t = topic;
    while (!t.empty()) { if (guideIdx.count(t)) return t; size_t dot = t.rfind('.'); if (dot == std::string::npos) break; t = t.substr(0, dot); }
    return "";
}
std::string Editor::tabTopic() const { return tab >= 0 && tab < (int)(sizeof(TAB_TOPICS) / sizeof(TAB_TOPICS[0])) ? TAB_TOPICS[tab] : "start"; }

// a small "?" that opens the guide on 'topic' (or the closest parent topic, else 'fallback'); nothing is drawn if neither exists
bool Editor::helpBtn(int x, int y, const std::string& topic, const std::string& fallback) {
    std::string t = helpTopicFor(topic); if (t.empty() && !fallback.empty()) t = helpTopicFor(fallback);
    if (t.empty()) return false;
    bool hv = ui.hover(x, y, 13, 13);
    ui.cv->fillRect(x, y, 13, 13, hv ? ACCENT : PANEL2); ui.cv->rect(x, y, 13, 13, hv ? C::White : ACCENT);
    font->draw(*ui.cv, *ts, x + (13 - font->width("?")) / 2, y + 1, "?", hv ? C::White : ACCENT);
    if (hv) ui.tip("Guide: " + guide[guideIdx.at(t)].title + (t == topic || fallback.empty() ? "" : "  (general)"));
    if (ui.clicked(x, y, 13, 13)) { openGuide(t); return true; }
    return true;
}

void Editor::openGuide(const std::string& topic) {
    if (guide.empty()) loadGuide();
    if (guide.empty()) { say("No guide found (assets/editor_guide.txt is missing)."); return; }
    std::string t = helpTopicFor(topic); if (t.empty()) t = helpTopicFor("start"); int idx = t.empty() ? 0 : guideIdx[t];
    if (pop == P_HELP) { if (idx != guideCur) { guideHist.push_back(guideCur); guideCur = idx; guideScroll = 0; } return; }
    guideHist.clear(); guideCur = idx; guideScroll = 0; guideFilter.clear(); guideTocScroll = std::max(0, idx - 8);
    popOpenedFrame = frameNo; pop = P_HELP; ui.modal = true; ui.blur();
}

// lays out the body of a topic into drawable lines: {text, kind} kind 0 = prose, 1 = bullet, 2 = example, 3 = gap
static std::vector<std::pair<std::string, int>> layoutBody(const Font& f, const std::vector<std::string>& body, int w) {
    std::vector<std::pair<std::string, int>> out; std::string para;
    auto flush = [&]() { if (para.empty()) return; for (auto& l : f.wrap(para, w)) out.push_back({l, 0}); para.clear(); };
    for (auto& l : body) {
        if (trim(l).empty()) { flush(); if (!out.empty() && out.back().second != 3) out.push_back({"", 3}); continue; }
        if (startsWith(l, "  ")) { flush(); out.push_back({l.substr(2), 2}); continue; }
        if (startsWith(l, "- ")) { flush(); auto ws = f.wrap(l.substr(2), w - 14); for (size_t i = 0; i < ws.size(); i++) out.push_back({ws[i], i == 0 ? 1 : 4}); continue; }
        para += (para.empty() ? "" : " ") + trim(l);
    }
    flush();
    return out;
}

void Editor::drawGuide(InputState& in, bool& close) {
    Canvas& cv = *ui.cv;
    int x = 30, y = 24, w = W - 60, h = H - 48;
    ui.panel(x, y, w, h); ui.header(x, y, w, "Editor guide");
    ui.text(x + w - 250, y + 2, "F1 / Esc: close   Backspace: back", DIM);
    if (guide.empty()) { ui.text(x + 10, y + 30, "assets/editor_guide.txt is missing.", DANGER); if (ui.button(x + w - 80, y + h - 28, 70, 20, "Close") || in.pressed[K_ESCAPE]) close = true; return; }
    guideCur = clampi(guideCur, 0, (int)guide.size() - 1);
    // ---- topic list ----
    int lw = 250, lx = x + 6, ly = y + 22;
    ui.label(lx, ly + 2, "find:"); ui.idCounter = 7000; ui.searchField(lx + 34, ly, lw - 34, guideFilter, "search the guide...", 40);
    std::vector<int> idx; std::vector<std::string> labels; std::string f = toLower(guideFilter);
    for (size_t i = 0; i < guide.size(); i++) {
        auto& t = guide[i]; bool hit = f.empty() || toLower(t.title).find(f) != std::string::npos || toLower(t.id).find(f) != std::string::npos;
        if (!hit && f.size() >= 3) for (auto& b : t.body) if (toLower(b).find(f) != std::string::npos) { hit = true; break; }
        if (!hit) continue;
        int depth = (int)std::count(t.id.begin(), t.id.end(), '.'); if (startsWith(t.id, "tab.") || startsWith(t.id, "concepts.") || startsWith(t.id, "editor.")) depth = 0;
        if (f.empty() && depth >= 2) continue;   // the deepest topics (single object keys) are reached from their parent or by searching
        idx.push_back((int)i); labels.push_back(std::string(depth * 2, ' ') + t.title);
    }
    int lsel = -1; for (size_t i = 0; i < idx.size(); i++) if (idx[i] == guideCur) lsel = (int)i;
    int hit = ui.listBox(lx, ly + 22, lw, h - 74, labels, lsel, guideTocScroll, 14);
    if (hit >= 0 && idx[hit] != guideCur) { guideHist.push_back(guideCur); guideCur = idx[hit]; guideScroll = 0; }
    // ---- the topic ----
    int cx = lx + lw + 10, cy = y + 22, cw = x + w - cx - 10, ch = h - 74;
    const GuideTopic& t = guide[guideCur];
    cv.fillRect(cx, cy, cw, ch, FIELD); cv.rect(cx, cy, cw, ch, BORDER);
    auto lines = layoutBody(*font, t.body, cw - 24);
    int lineH = 13, headH = 34;
    // see-also buttons flow after the body
    std::vector<std::pair<int, std::string>> see; for (auto& s : t.see) { std::string r = helpTopicFor(s); if (!r.empty()) see.push_back({guideIdx[r], guide[guideIdx[r]].title}); }
    // children: topics whose id starts with this id + "." and is one level deeper (field topics of a section)
    std::vector<std::pair<int, std::string>> kids;
    for (size_t i = 0; i < guide.size(); i++) { auto& o = guide[i]; if (startsWith(o.id, t.id + ".") && o.id.find('.', t.id.size() + 1) == std::string::npos) kids.push_back({(int)i, o.title}); }
    // section topics for a tab: "tab.items" lists the "item.*" field topics
    if (startsWith(t.id, "tab.")) {
        static const std::map<std::string, std::string> sec = {{"tab.items", "item"}, {"tab.creatures", "creature"}, {"tab.spells", "spell"}, {"tab.classes", "class"}, {"tab.shops", "shop"}, {"tab.encounters", "table"}, {"tab.quests", "quest"},
            {"tab.dungeons", "dungeon"}, {"tab.world", "location"}, {"tab.recipes", "recipe"}, {"tab.affixes", "affix"}, {"tab.banter", "banter"}, {"tab.achievements", "achievement"}, {"tab.tiles", "tilekind"}, {"tab.sounds", "sound"},
            {"tab.campaign", "campaign"}, {"tab.dialogue", "dlg"}, {"tab.maps", "maps"}, {"tab.flags", "flags"}, {"tab.bounties", "board"}};
        auto it = sec.find(t.id);
        if (it != sec.end()) for (size_t i = 0; i < guide.size(); i++) { auto& o = guide[i]; if (startsWith(o.id, it->second + ".") && o.id.find('.', it->second.size() + 1) == std::string::npos) kids.push_back({(int)i, o.title}); }
        if (t.id == "tab.maps") for (size_t i = 0; i < guide.size(); i++) { auto& o = guide[i]; if ((startsWith(o.id, "map.") || startsWith(o.id, "obj.")) && std::count(o.id.begin(), o.id.end(), '.') == 1) kids.push_back({(int)i, o.title}); }
    }
    int extraRows = 0; auto countRows = [&](const std::vector<std::pair<int, std::string>>& v) { if (v.empty()) return 0; int rx = 0, rows = 1; for (auto& s : v) { int bw = font->width(s.second) + 12; if (rx + bw > cw - 24) { rows++; rx = 0; } rx += bw + 4; } return rows; };
    extraRows = (see.empty() ? 0 : 1 + countRows(see)) + (kids.empty() ? 0 : 1 + countRows(kids));
    int totalH = headH + (int)lines.size() * lineH + (extraRows ? 10 + extraRows * 20 : 0);
    if (ui.hover(cx, cy, cw, ch) && in.wheel) guideScroll -= in.wheel * 3 * lineH;
    if (!ui.typing()) { if (in.pressed[K_PAGEDOWN] || in.pressed[K_DOWN]) guideScroll += in.pressed[K_DOWN] ? lineH * 2 : ch - 40; if (in.pressed[K_PAGEUP] || in.pressed[K_UP]) guideScroll -= in.pressed[K_UP] ? lineH * 2 : ch - 40; }
    guideScroll = clampi(guideScroll, 0, std::max(0, totalH - ch + 10));
    cv.setClip({cx + 1, cy + 1, cw - 2, ch - 2});
    int ty = cy + 8 - guideScroll;
    font->bold = true; font->draw(cv, *ts, cx + 12, ty, t.title, ACCENT2); font->bold = false;
    font->draw(cv, *ts, cx + cw - 12 - font->width(t.id), ty, t.id, DIM);
    ty += headH - 8;
    for (auto& l : lines) {
        if (ty > cy - lineH && ty < cy + ch) {
            if (l.second == 1) { cv.fillRect(cx + 14, ty + 5, 3, 3, ACCENT); font->draw(cv, *ts, cx + 24, ty, l.first, TEXT); }
            else if (l.second == 4) font->draw(cv, *ts, cx + 24, ty, l.first, TEXT);
            else if (l.second == 2) { cv.fillRect(cx + 10, ty - 1, cw - 20, lineH, RGB(30, 34, 44)); font->draw(cv, *ts, cx + 16, ty, l.first, RGB(150, 220, 150)); }
            else if (l.second == 0) font->draw(cv, *ts, cx + 12, ty, l.first, TEXT);
        }
        ty += l.second == 3 ? lineH / 2 + 2 : lineH;
    }
    auto linkRow = [&](const char* head, const std::vector<std::pair<int, std::string>>& v) {
        if (v.empty()) return;
        ty += 6; font->draw(cv, *ts, cx + 12, ty, head, DIM); ty += 16; int rx = cx + 12;
        for (auto& s : v) { int bw = font->width(s.second) + 12; if (rx + bw > cx + cw - 12) { rx = cx + 12; ty += 20; }
            bool vis = ty > cy && ty + 18 < cy + ch;
            if (vis && ui.button(rx, ty, bw, 18, s.second, true, false, ACCENT)) { guideHist.push_back(guideCur); guideCur = s.first; guideScroll = 0; }
            rx += bw + 4; }
        ty += 20;
    };
    linkRow(startsWith(t.id, "tab.") ? "Fields and parts of this tab:" : "In this topic:", kids);
    linkRow("See also:", see);
    cv.resetClip();
    if (totalH > ch) { int th = std::max(16, ch * ch / totalH), tyy = cy + (ch - th) * guideScroll / std::max(1, totalH - ch + 10); cv.fillRect(cx + cw - 6, tyy, 4, th, DIM); }
    // ---- buttons ----
    int by = y + h - 28;
    if (ui.button(lx, by, 70, 20, "< Back", !guideHist.empty()) || (!ui.typing() && in.pressed[K_BACKSPACE] && !guideHist.empty())) { guideCur = guideHist.back(); guideHist.pop_back(); guideScroll = 0; }
    if (ui.button(lx + 76, by, 90, 20, "Contents")) { if (guideCur != 0) guideHist.push_back(guideCur); guideCur = guideIdx.count("start") ? guideIdx["start"] : 0; guideScroll = 0; guideFilter.clear(); }
    ui.text(cx, by + 4, "Every tab and most fields have a ? that opens this guide on the right page.", DIM);
    if (ui.button(x + w - 80, by, 70, 20, "Close") || in.pressed[K_ESCAPE] || (in.pressed[K_F1] && popOpenedFrame != frameNo)) close = true;
}
