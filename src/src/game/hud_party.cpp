// 1.8.3 (#173): the Party tab in the theme's own layout (the frame's here; native / onebit in their own files).
#include "game.h"
#include "tiles.h"
#include <algorithm>

using namespace UI;
using Theme::Pen;

static std::string bindName(int a) { return gKeys.key1[a] ? keyName(gKeys.key1[a]) : gKeys.key2[a] ? keyName(gKeys.key2[a]) : ""; }
static std::string classLine(const Character& m, const GameData& gd) {
    const ClassDef* c = gd.cls(m.classId);
    std::string n = c ? c->name : m.classId; if (!n.empty()) n[0] = (char)toupper((unsigned char)n[0]);
    int r = m.rank(m.classId); return n + " " + std::to_string(r > 0 ? r : m.level);
}

std::vector<std::string> Game::equipCandidates(int slot) const {
    std::vector<std::string> c; for (auto& it : party.inventory) { auto* d = gd->item(it.id); if (d && d->slot() == slot) c.push_back(it.id); }
    return c;
}
bool nativePartyFits(const Rect& a);   // hud_native.cpp
bool oneBitPartyFits(const Rect& a);   // hud_onebit.cpp
bool Game::themedParty() const {
    if (mode != GMode::MENU || menuTab != 0 || menuPrompt == "respec" || party.members.empty()) return false;
    Rect a = menuArea();
    if (Theme::native()) return nativePartyFits(a);
    if (Theme::onebit()) return oneBitPartyFits(a);
    return a.w >= 700 && a.h >= 520;
}
void Game::drawThemedParty(Canvas& cv, InputState& in) {
    if (Theme::native()) drawNativeParty(cv, in);
    else if (Theme::onebit()) drawOneBitParty(cv, in);
    else drawFrameParty(cv, in);
}

const char* statLine(int s) {
    static const char* L[] = {"Melee damage, carry weight", "Hit chance, evasion", "Hit points", "Spell power", "Mana, resist magic", "Prices, talk checks"};
    return s >= 0 && s < 6 ? L[s] : "";
}
std::vector<std::string> classOrder(const GameData& gd) {
    std::vector<std::string> ids; for (const char* c : {"warrior", "ranger", "mage", "scavenger", "cleric"}) if (gd.cls(c)) ids.push_back(c);
    for (auto& kv : gd.classes) if (std::find(ids.begin(), ids.end(), kv.first) == ids.end()) ids.push_back(kv.first);
    return ids;
}

// #173: a full-height carved panel over the viewport: tabs, the paper doll with six slots, attributes, class ranks,
// resists and notes, and key hints along the bottom. The column keeps the plaque and The Party; its last panel compares.
void Game::drawFrameParty(Canvas& cv, InputState& in) {
    Theme::NoMap nm; const UiTheme& t = Theme::th(); Pen p{&cv, ctx.font, ctx.ts};
    Rect a = menuArea();
    uiRect(a.x, a.y, a.w, a.h);
    Color8 deep{20, 22, 31, 255}; Theme::well(cv, a.x, a.y, a.w, a.h, &deep);
    const int X = a.x + 2, Y = a.y + 2, Wd = a.w - 4, Ht = a.h - 4;
    Theme::panel(cv, X, Y, Wd, Ht);
    const int mi = clampi(menuMember, 0, (int)party.members.size() - 1);
    Character& m = party.members[mi];
    // tabs
    static const char* TABN[] = {"Party", "Items", "Spells", "Quests"};
    static const int TABK[] = {A_MENU, A_ITEMS, A_SPELLS, A_JOURNAL};
    for (int i = 0; i < 4; i++) {
        int tx = X + 12 + i * 112; bool on = menuTab == i;
        bool hover = in.mouseX >= tx && in.mouseX < tx + 106 && in.mouseY >= Y + 8 && in.mouseY < Y + 32;
        Theme::button(p, tx, Y + 8, 106, 24, TABN[i], on ? Theme::BS_SELECTED : Theme::BS_NORMAL, hover, bindName(TABK[i]));
        hudHit(tx, Y + 8, 106, 24, HH_PTAB, i);
    }
    Theme::textRight(p, X + Wd - 12, Y + 14, fmt("%d gold", party.gold), t.gold);
    Theme::rule(cv, X + 10, Y + 36, Wd - 20);
    // the paper doll
    int ox = X + 10, oy = Y + 46;
    Theme::title(p, ox + 12, oy + 4, m.name);
    std::string sub = classLine(m, *gd) + (leader == mi ? "  -  leading the party" : "");
    Theme::text(p, ox + 22 + Theme::titleW(p, m.name), oy + 5, sub, t.dim);
    Color8 dark{11, 12, 16, 255}; Theme::well(cv, ox + 138, oy + 32, 120, 150, &dark);
    if (m.recolor) ctx.ts->drawRecolor(cv, m.tile, ox + 162, oy + 56, 6, m.color); else ctx.ts->draw(cv, m.tile, ox + 162, oy + 56, 6);
    struct SlotPos { int slot, x, y; };
    const SlotPos sp[] = {{SL_HEAD, ox + 22, oy + 36}, {SL_BODY, ox + 22, oy + 90}, {SL_FEET, ox + 22, oy + 144}, {SL_WEAPON, ox + 270, oy + 36}, {SL_OFFHAND, ox + 270, oy + 90}, {SL_ACCESSORY, ox + 270, oy + 144}};
    for (auto& s : sp) {
        bool sel = menuSubFocus && menuSub == s.slot;
        bool hover = in.mouseX >= s.x && in.mouseX < s.x + 110 && in.mouseY >= s.y && in.mouseY < s.y + 44;
        Color8 wf = sel ? t.rowSel : t.well; Theme::well(cv, s.x, s.y, 110, 44, &wf);
        if (sel || hover) cv.rect(s.x, s.y, 110, 44, sel ? t.accent : t.hi2);
        Theme::small(p, s.x + 5, s.y + 4, slotName(s.slot), t.dim);
        const ItemDef* it = m.equip[s.slot].empty() ? nullptr : gd->item(m.equip[s.slot]);
        if (it) { drawItemTile(*ctx.ts, cv, *it, s.x + 3, s.y + 17, 2); Theme::text(p, s.x + 29, s.y + 21, Theme::fit(p, it->name, 78), sel ? t.accent : t.text); }
        else Theme::text(p, s.x + 6, s.y + 21, "empty", t.hi);
        hudHit(s.x, s.y, 110, 44, HH_SLOT, s.slot);
    }
    Theme::small(p, ox + 28, oy + 200, "XP", t.dim);
    Theme::bar(cv, ox + 44, oy + 201, 288, 5, m.xp, m.xpToNext(), t.accent);
    Theme::smallRight(p, ox + 340, oy + 200, fmt("%d / %d", m.xp, m.xpToNext()), t.text);
    // attributes
    int axp = X + 402, ayp = Y + 46, aw = Wd - 414;
    Theme::panel(cv, axp, ayp, aw, 222); Theme::title(p, axp + 12, ayp + 10, "Attributes");
    int hoverStat = -1;
    for (int s = 0; s < ST_COUNT && s < 6; s++) {
        int ry = ayp + 32 + s * 22; int v = m.stat(s, *gd), b = m.baseStats[s];
        Color8 wf = t.well; Theme::well(cv, axp + 10, ry, 40, 18, &wf);
        Theme::text(p, axp + 16, ry + 3, statName(s), t.accent);
        Theme::text(p, axp + 64, ry + 3, std::to_string(v), v > b ? t.good : v < b ? Theme::foe() : t.text);
        Theme::small(p, axp + 88, ry + 5, statLine(s), t.dim);
        if (in.mouseX >= axp + 8 && in.mouseX < axp + aw - 8 && in.mouseY >= ry && in.mouseY < ry + 18) hoverStat = s;
    }
    Theme::rule(cv, axp + 10, ayp + 166, aw - 20);
    int lo, hi, ty; m.attackDamage(*gd, lo, hi, ty);
    Theme::text(p, axp + 10, ayp + 174, fmt("HP %d/%d", m.hp, m.maxHP(*gd)), Theme::hp());
    Theme::text(p, axp + 96, ayp + 174, fmt("MP %d/%d", m.mp, m.maxMP(*gd)), Theme::mp());
    Theme::text(p, axp + 182, ayp + 174, fmt("AP %d  Move %d", m.maxAP(*gd), m.maxMove(*gd)), t.accent);
    Theme::text(p, axp + 10, ayp + 190, fmt("Damage %d-%d %s  Range %d", lo, hi, dmgTypeName(ty), m.attackRange(*gd)), t.text);
    Theme::text(p, axp + 10, ayp + 204, fmt("Defense %d   Evasion %d", m.defense(*gd), m.evasion(*gd)), t.text);
    // class ranks
    int cxp = X + 12, cyp = Y + 276;
    Theme::panel(cv, cxp, cyp, 380, 200); Theme::title(p, cxp + 12, cyp + 10, "Class ranks");
    Theme::textRight(p, cxp + 368, cyp + 11, m.rankPoints > 0 ? fmt("%d rank point%s free", m.rankPoints, m.rankPoints == 1 ? "" : "s") : "ranks rise on level-up", m.rankPoints > 0 ? t.accent : t.dim);
    auto cls = classOrder(*gd);
    for (size_t i = 0; i < cls.size() && i < 5; i++) {
        int ry = cyp + 32 + (int)i * 32; int r = m.rank(cls[i]);
        Color8 wf = t.well; Theme::well(cv, cxp + 10, ry, 360, 28, &wf);
        const ClassDef* cd = gd->cls(cls[i]); std::string nm2 = cd ? cd->name : cls[i];
        Theme::text(p, cxp + 18, ry + 8, nm2, r > 0 ? t.accent : t.text);
        for (int k = 0; k < 10; k++) cv.fillRect(cxp + 136 + k * 18, ry + 10, 14, 8, k < r ? t.accent : t.pipOff);
        bool can = m.rankPoints > 0 && !levelUpQueue.empty();
        bool hover = can && in.mouseX >= cxp + 320 && in.mouseX < cxp + 364 && in.mouseY >= ry + 4 && in.mouseY < ry + 24;
        Theme::button(p, cxp + 320, ry + 4, 44, 20, "+1", can ? Theme::BS_NORMAL : Theme::BS_DISABLED, hover);
        if (can) hudHit(cxp + 320, ry + 4, 44, 20, HH_RANKUP, (int)i);
    }
    // resists and notes
    int rxp = axp, ryp = cyp;
    Theme::panel(cv, rxp, ryp, aw, 200); Theme::title(p, rxp + 12, ryp + 10, "Resists & notes");
    std::string res; for (int d = 1; d < DT_COUNT; d++) { int r = m.resist(d, *gd); if (r) res += std::string(res.empty() ? "" : ", ") + dmgTypeName(d) + fmt(" %d%%", r); }
    int ny = ryp + 32;
    Theme::text(p, rxp + 12, ny, Theme::fit(p, "Resist: " + (res.empty() ? std::string("none yet") : res), aw - 24), res.empty() ? t.dim : t.text); ny += 18;
    std::string note = hoverStat >= 0 ? std::string(statName(hoverStat)) + ": " + statHelp(hoverStat) : "Point at a stat to see what it does. Pick a slot to see what fits from the pack.";
    ny += ctx.font->drawWrapped(cv, *ctx.ts, rxp + 12, ny, aw - 24, note, hoverStat >= 0 ? t.text : t.dim) + 6;
    if (m.wounds > 0) ny += ctx.font->drawWrapped(cv, *ctx.ts, rxp + 12, ny, aw - 24, fmt("Wounded x%d: -%d%% max HP, -%d to hit. An inn or a healer cures it.", m.wounds, 15 * m.wounds, 5 * m.wounds), Theme::foe()) + 4;
    for (int s = 0; s < SL_COUNT && ny < ryp + 140; s++) if (!m.equip[s].empty()) if (const ItemDef* it = gd->item(m.equip[s])) { std::string wv = gearWarning(*it, m); if (!wv.empty()) ny += ctx.font->drawWrapped(cv, *ctx.ts, rxp + 12, ny, aw - 24, it->name + ": " + wv, t.accentHi) + 2; }
    Theme::text(p, rxp + 12, ryp + 156, Theme::fit(p, "Leader: " + party.members[clampi(leader, 0, (int)party.members.size() - 1)].name, aw - 24), t.text);
    { std::string order; for (auto& mm : party.members) order += (order.empty() ? "" : ", ") + mm.name; Theme::text(p, rxp + 12, ryp + 172, Theme::fit(p, "Order: " + order, aw - 24), t.text); }
    // key hints
    Theme::rule(cv, X + 10, Y + Ht - 52, Wd - 20);
    struct KH { std::string k, l; int kind; };
    std::vector<KH> hints;
    if (menuSubFocus) hints = {{"Up/Down", "slot", 0}, {"Enter", "change", 0}, {bindName(A_ITEMS), "unequip", 0}, {"Left", "members", 0}, {"Esc", "back", 0}};
    else hints = {{"Up/Down", "member", 0}, {"Right", "equipment", 0}, {bindName(A_INTERACT), leader == mi ? "leading the party" : "lead the party", HH_LEAD}, {bindName(A_PAGEDOWN), "next tab", 0}, {"Esc", "back", 0}};
    int hx = X + 22;
    for (auto& h : hints) {
        int kw = Theme::textW(p, h.k) + 12;
        Theme::button(p, hx, Y + Ht - 40, kw, 20, h.k, Theme::BS_NORMAL);
        Theme::text(p, hx + kw + 6, Y + Ht - 36, h.l, t.dim);
        if (h.kind) hudHit(hx, Y + Ht - 40, kw + 6 + Theme::textW(p, h.l), 20, h.kind, 0);
        hx += std::max(138, kw + 18 + Theme::textW(p, h.l));
    }
    // the equip picker, over the attributes (the column's Compare panel shows the pointed-at item)
    if (menuPrompt == "equip") {
        auto cands = equipCandidates(menuSub);
        int n = (int)cands.size() + 1, vis = std::min(n, 12);
        int px0 = axp, py0 = ayp, pw = aw, ph = 34 + vis * 18 + 8;
        Theme::panel(cv, px0, py0, pw, ph); Theme::title(p, px0 + 12, py0 + 10, std::string("Equip ") + slotName(menuSub));
        int first = clampi(menuSel - vis + 1, 0, std::max(0, n - vis));
        for (int k = 0; k < vis; k++) {
            int i = first + k, ry = py0 + 30 + k * 18; bool sel = i == menuSel;
            if (sel) Theme::listRow(p, px0 + 8, ry - 1, pw - 16, 17, Theme::RS_SELECTED);
            if (i == 0) Theme::text(p, px0 + 14, ry + 2, "(remove)", sel ? t.accent : t.dim);
            else if (const ItemDef* d = gd->item(cands[i - 1])) { drawItemTile(*ctx.ts, cv, *d, px0 + 12, ry + 2, 1); Theme::text(p, px0 + 30, ry + 2, Theme::fit(p, d->name, pw - 50), sel ? t.accent : t.text); }
            hudHit(px0 + 8, ry - 1, pw - 16, 17, HH_PICK, i);
        }
    }
}

// the item the party screen points at (the frame column's Compare panel)
const ItemDef* Game::partyPointedItem() const {
    if (mode != GMode::MENU || menuTab != 0 || party.members.empty()) return nullptr;
    const Character& m = party.members[clampi(menuMember, 0, (int)party.members.size() - 1)];
    if (menuPrompt == "equip") { auto c = equipCandidates(menuSub); if (menuSel > 0 && menuSel - 1 < (int)c.size()) return gd->item(c[menuSel - 1]); }
    if (menuSubFocus && !m.equip[menuSub].empty()) return gd->item(m.equip[menuSub]);
    if (!m.equip[SL_WEAPON].empty()) return gd->item(m.equip[SL_WEAPON]);
    return nullptr;
}
