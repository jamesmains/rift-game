// In-game menus: party/inventory/spells/quests, shops, level-ups.
#include "game.h"
#include "../core/audio.h"
#include "tiles.h"
#include <algorithm>

using namespace UI;

static const char* TABS[] = {"Party", "Items", "Spells", "Quests"};
static const int NTABS = 4;
static const int INV_ACT_Y = 242, INV_WHO_Y = 324;   // pack tab: the item actions list and the "Who?" list under the details panel (1.1.4: 26 px lower)

// ---------------- item categories, filters and sorting (inventory + shop) ----------------
static const char* SORT_NAMES[] = {"as found", "name", "value", "category"};
std::vector<std::pair<std::string, int>> Game::itemCats(const std::vector<std::string>& ids) const {
    std::map<std::string, int> n; for (auto& id : ids) if (const ItemDef* d = gd->item(id)) n[itemCategory(*d)]++;
    std::vector<std::pair<std::string, int>> out(n.begin(), n.end());
    std::stable_sort(out.begin(), out.end(), [](const std::pair<std::string, int>& a, const std::pair<std::string, int>& b) { int oa = itemCategoryOrder(a.first), ob = itemCategoryOrder(b.first); return oa != ob ? oa < ob : a.first < b.first; });
    return out;
}
std::vector<int> Game::itemView(const std::vector<std::string>& ids, const std::string& cat, int sort, bool buying) const {
    std::vector<int> v;
    for (size_t i = 0; i < ids.size(); i++) { const ItemDef* d = gd->item(ids[i]); if (!d) continue; if (cat == "All" || itemCategory(*d) == cat) v.push_back((int)i); }
    auto def = [&](int i) { return gd->item(ids[i]); };
    if (sort == 1) std::stable_sort(v.begin(), v.end(), [&](int a, int b) { return def(a)->name < def(b)->name; });
    else if (sort == 2) std::stable_sort(v.begin(), v.end(), [&](int a, int b) { int pa = buying ? itemBuyPrice(*def(a)) : def(a)->value, pb = buying ? itemBuyPrice(*def(b)) : def(b)->value; return pa > pb; });
    else if (sort == 3) std::stable_sort(v.begin(), v.end(), [&](int a, int b) { std::string ca = itemCategory(*def(a)), cb = itemCategory(*def(b)); int oa = itemCategoryOrder(ca), ob = itemCategoryOrder(cb); if (oa != ob) return oa < ob; if (ca != cb) return ca < cb; return def(a)->name < def(b)->name; });
    return v;
}
std::vector<int> Game::invView() const { std::vector<std::string> ids; for (auto& s : party.inventory) ids.push_back(s.id); return itemView(ids, invCat, invSort, false); }
std::vector<int> Game::shopView() const {
    std::vector<std::string> ids;
    if (shopTab == 0) { auto stock = const_cast<Game*>(this)->shopItems(shopId); for (auto& s : stock) ids.push_back(s.first); } else for (auto& s : party.inventory) ids.push_back(s.id);
    return itemView(ids, shopCat, shopSort, shopTab == 0);
}
// the category chips over an item list: "All" + every category present, then the sort button at the right end
struct CatChip { std::string cat, label; int x, w; };
static std::vector<CatChip> catChips(const Ctx& ctx, const std::vector<std::pair<std::string, int>>& cats, int total, int x, int maxW) {
    std::vector<CatChip> out; int cx = x;
    auto add = [&](const std::string& c, int n) { std::string l = c + " " + std::to_string(n); int w = ctx.font->width(l) + 10; if (cx + w > x + maxW) return; out.push_back({c, l, cx, w}); cx += w + 3; };
    add("All", total); for (auto& c : cats) add(c.first, c.second);
    return out;
}
// mouse + keys for a chip bar; returns true when the category or sort changed
static bool catBarInput(const Ctx& ctx, const InputState& in, const std::vector<std::pair<std::string, int>>& cats, int total, int x, int y, int maxW, std::string& cat, int& sort) {
    bool ch = false;
    std::vector<std::string> order{"All"}; for (auto& c : cats) order.push_back(c.first);
    int cur = 0; for (size_t i = 0; i < order.size(); i++) if (order[i] == cat) cur = (int)i;
    // keyboard: T next category (shift+T back), R sort.  Pad: LT / RT change category, Y sorts (LB/RB already change tabs)
    // rebindable (Settings > Controls): next / previous category, sort.  Shift + next also goes back.
    int n = (int)order.size();
    if (actP(in, A_CATNEXT) && !in.shift()) { cat = order[(cur + 1) % n]; ch = true; }
    else if (actP(in, A_CATPREV) || (actP(in, A_CATNEXT) && in.shift())) { cat = order[(cur + n - 1) % n]; ch = true; }
    if (actP(in, A_SORT)) { sort = (sort + 1) % 4; ch = true; }
    if (in.mousePressed[0] && in.mouseY >= y && in.mouseY < y + 13) {
        for (auto& c : catChips(ctx, cats, total, x, maxW - 116)) if (in.mouseX >= c.x && in.mouseX < c.x + c.w) { cat = c.cat; ch = true; }
        if (in.mouseX >= x + maxW - 112 && in.mouseX < x + maxW) { sort = (sort + 1) % 4; ch = true; }
    }
    return ch;
}
static void catBarDraw(Ctx& ctx, Canvas& cv, const std::vector<std::pair<std::string, int>>& cats, int total, int x, int y, int maxW, const std::string& cat, int sort) {
    for (auto& c : catChips(ctx, cats, total, x, maxW - 116)) UI::chip(ctx, c.x, y, c.w, 13, c.label, c.cat == cat);   // 1.8.4 (#189)
    std::string sl = std::string("sort: ") + SORT_NAMES[clampi(sort, 0, 3)];
    if (Theme::native()) { Theme::NoMap nm; Theme::Pen p{&cv, ctx.font, ctx.ts}; Color8 f = Theme::th().face; Theme::bevel(cv, x + maxW - 112, y, 92, 13, Theme::th().hi, Theme::th().lo, &f); Theme::text(p, x + maxW - 108, y, sl, Theme::th().text); }
    else { cv.fillRect(x + maxW - 112, y, 92, 13, RGB(40, 40, 54)); cv.rect(x + maxW - 112, y, 92, 13, C::DkGray); UI::text(ctx, x + maxW - 108, y + 1, sl, C::LtGray); }
}


void Game::drawCharacterSheet(Canvas& cv, InputState& in, int mi, int x, int y, int w, int h) {
    (void)in; (void)h;
    Character& m = party.members[mi];
    cv.fillRect(x + 8, y + 8, 52, 52, RGB(40, 40, 60)); cv.rect(x + 8, y + 8, 52, 52, C::Gold);
    if (m.recolor) ctx.ts->drawRecolor(cv, m.tile, x + 10, y + 10, 4, m.color); else ctx.ts->draw(cv, m.tile, x + 10, y + 10, 4);
    ctx.font->bold = true; UI::text(ctx, x + 70, y + 8, m.name, C::Gold); ctx.font->bold = false;
    const ClassDef* c = gd->cls(m.classId);
    UI::text(ctx, x + 70, y + 22, fmt("Level %d  ", m.level) + m.classSummary(*gd), C::LtGray); (void)c;
    UI::text(ctx, x + 70, y + 36, fmt("XP %d / %d", m.xp, m.xpToNext()), C::Gray);
    bar(ctx, x + 70, y + 50, 120, 6, m.xp, m.xpToNext(), C::Purple);
    int sy = y + 70;
    UI::text(ctx, x + 8, sy, fmt("HP  %d/%d", m.hp, m.maxHP(*gd)), C::Green);
    UI::text(ctx, x + 130, sy, fmt("MP  %d/%d", m.mp, m.maxMP(*gd)), RGB(120, 150, 255)); sy += 14;
    UI::text(ctx, x + 8, sy, fmt("AP %d   Move %d", m.maxAP(*gd), m.maxMove(*gd)), C::Yellow); sy += 14;
    int lo, hi, ty; m.attackDamage(*gd, lo, hi, ty);
    UI::text(ctx, x + 8, sy, fmt("Damage %d-%d %s  Range %d", lo, hi, dmgTypeName(ty), m.attackRange(*gd)), C::White); sy += 14;
    UI::text(ctx, x + 8, sy, fmt("Defense %d   Evasion %d", m.defense(*gd), m.evasion(*gd)), C::White); sy += 18;
    int hoverStat = -1;
    for (int s = 0; s < ST_COUNT; s++) {
        int v = m.stat(s, *gd), b = m.baseStats[s];
        int sx2 = x + 8 + (s % 3) * 76, sy2 = sy + (s / 3) * 14;
        if (in.mouseX >= sx2 && in.mouseX < sx2 + 70 && in.mouseY >= sy2 && in.mouseY < sy2 + 13) { hoverStat = s; cv.fillRect(sx2 - 2, sy2 - 1, 72, 13, RGB(60, 50, 20)); }
        UI::text(ctx, sx2, sy2, fmt("%s %d", statName(s), v), v > b ? C::Cyan : v < b ? C::Red : C::White);
    }
    sy += 30;
    UI::text(ctx, x + 8, sy, hoverStat >= 0 ? statHelp(hoverStat) : "Point at a stat to see what it does.", hoverStat >= 0 ? C::Cream : C::DkGray);
    sy += 16;
    UI::text(ctx, x + 8, sy, "Resist:", C::Gray);
    int rx = x + 60; for (int d = 1; d < DT_COUNT; d++) { int r = m.resist(d, *gd); if (r != 0) { UI::text(ctx, rx, sy, fmt("%s %d%%", dmgTypeName(d), r), dmgColor(d)); rx += 66; } }
    sy += 18;
    UI::text(ctx, x + 8, sy, "Equipment", C::Gold); sy += 14;
    sheetEquipY = sy;
    for (int s = 0; s < SL_COUNT; s++) {
        const ItemDef* it = m.equip[s].empty() ? nullptr : gd->item(m.equip[s]);
        bool sel = menuSubFocus && menuSub == s;
        if (sel) { cv.fillRect(x + 6, sy - 1, w - 12, 14, RGB(90, 70, 20)); cv.rect(x + 6, sy - 1, w - 12, 14, C::Gold); UI::selCursor(ctx, x + 6, sy - 1, 14); }
        UI::text(ctx, x + 8, sy, slotName(s), C::Gray);
        if (it) { drawItemTile(*ctx.ts, cv, *it, x + 78, sy - 1, 1); UI::text(ctx, x + 94, sy, it->name, sel ? C::Yellow : C::White); }
        else UI::text(ctx, x + 94, sy, "-", C::DkGray);
        sy += 14;
    }
    // 1.5.0 (#94 #93): Wounded, and gear the member doesn't use well
    if (m.wounds > 0) { UI::text(ctx, x + 8, sy + 4, fmt("WOUNDED x%d: -%d%% max HP, -%d to hit. An inn night or a healer cures it.", m.wounds, 15 * m.wounds, 5 * m.wounds), RGB(235, 105, 95)); sy += 14; }
    for (int s = 0; s < SL_COUNT; s++) if (!m.equip[s].empty()) if (const ItemDef* it = gd->item(m.equip[s])) { std::string wv = gearWarning(*it, m); if (!wv.empty()) { UI::text(ctx, x + 8, sy + 4, it->name + ": " + wv, RGB(235, 150, 95)); sy += 14; } }
    if (m.statPoints > 0 || m.rankPoints > 0) UI::text(ctx, x + 8, sy + 4, fmt("%d stat points and %d class rank%s to assign (Level Up)", m.statPoints, m.rankPoints, m.rankPoints == 1 ? "" : "s"), C::Cyan);
}

int Game::respecCost(int mi) const { if (respecFree || mi < 0 || mi >= (int)party.members.size()) return 0; return 40 + 20 * party.members[mi].level; }
void Game::updateMenu(InputState& in, float dt) {
    (void)dt;
    if (menuPrompt == "respec") {   // Rite of Renewal: pick who to retrain
        int nm = (int)party.members.size();
        if (actP(in, A_CANCEL)) { menuPrompt.clear(); respecFree = false; popMode(); return; }
        if (actP(in, A_UP)) menuMember = (menuMember + nm - 1) % nm;
        if (actP(in, A_DOWN)) menuMember = (menuMember + 1) % nm;
        for (int i = 0; i < nm; i++) if (in.mousePressed[0] && in.mouseX >= 20 && in.mouseX < 150 && in.mouseY >= 50 + i * 40 && in.mouseY < 88 + i * 40) { menuMember = i; in.press(K_ENTER); }
        if (actP(in, A_CONFIRM) || in.pressed[K_ENTER]) {
            int cost = respecCost(menuMember);
            if (party.members[menuMember].level <= 1) { log(party.members[menuMember].name + " has nothing to unlearn yet."); return; }
            if (party.gold < cost) { log(fmt("The rite costs %d gold.", cost)); gAudio.play("ui_error"); return; }
            party.gold -= cost; stats.goldSpent += cost;
            if (respecFree && respecItem.size()) party.removeItem(respecItem, 1);
            respecMember(menuMember); menuPrompt.clear(); respecFree = false; respecItem.clear(); popMode(); gAudio.play("respec");
        }
        return;
    }
    // 1.8.3 (#173 #177 #181): the themed Party tab is clicked through HUD hot spots
    const bool themed = themedParty();
    if (themed && (in.mousePressed[0] || in.mouseDX || in.mouseDY)) if (const HudHit* h = hudHitAt(in.mouseX, in.mouseY)) {
        bool click = in.mousePressed[0];
        if (h->kind == HH_PICK && (in.mouseDX || in.mouseDY)) menuSel = h->arg;
        if (click) {
            in.eatMouse();
            switch (h->kind) {
                case HH_PTAB: menuTab = clampi(h->arg, 0, NTABS - 1); menuSel = 0; spellCol = 0; menuScroll = 0; menuSubFocus = false; menuPrompt.clear(); return;
                case HH_MEMBER: menuMember = clampi(h->arg, 0, (int)party.members.size() - 1); menuSubFocus = false; menuPrompt.clear(); return;
                case HH_SLOT: if (menuSubFocus && menuSub == h->arg && menuPrompt.empty()) { menuPrompt = "equip"; menuSel = 0; menuScroll = 0; } else { menuSubFocus = true; menuSub = clampi(h->arg, 0, SL_COUNT - 1); menuPrompt.clear(); } return;
                case HH_PICK: { auto cands = equipCandidates(menuSub); menuSel = h->arg; if (menuSel == 0) unequip(menuMember, menuSub); else if (menuSel - 1 < (int)cands.size()) equipItem(menuMember, cands[menuSel - 1]); menuPrompt.clear(); return; }
                case HH_LEAD: if (party.members[menuMember].alive) { leader = menuMember; notify(party.members[leader].name + " takes the lead."); } return;
                case HH_RANKUP: if (!levelUpQueue.empty()) pushMode(GMode::LEVELUP); return;
                default: return;
            }
        }
    }
    if (menuTab == 2 && !menuSubFocus && spellCol == 1 && actP(in, A_CANCEL)) { spellCol = 0; return; }   // back from the spell list to the member list
    if (actP(in, A_CANCEL) || (actP(in, A_MENU) && !menuSubFocus)) { if (menuSubFocus && !menuPrompt.empty()) { menuPrompt.clear(); /* one step back: item picker -> slot (or action) list */ } else if (menuSubFocus) menuSubFocus = false; else popMode(); return; }
    if (actP(in, A_PAGEUP) || (actP(in, A_LEFT) && !menuSubFocus && menuTab != 0 && menuTab != 2)) { menuTab = (menuTab + NTABS - 1) % NTABS; menuSel = 0; spellCol = 0; menuScroll = 0; menuSubFocus = false; menuPrompt.clear(); }
    if (actP(in, A_PAGEDOWN) || (actP(in, A_RIGHT) && !menuSubFocus && menuTab != 0 && menuTab != 2)) { menuTab = (menuTab + 1) % NTABS; menuSel = 0; spellCol = 0; menuScroll = 0; menuSubFocus = false; menuPrompt.clear(); }
    // 1.8.4 (#193): the keys shown on the tabs pick them (Spells B, Items Q, Quests J); J on the Quests tab opens the
    // Journal over this screen. Q on the Party tab still talks to the chosen companion (below).
    // 1.8.5 (#200): only the keyboard's: on a controller LB is also "previous tab" and LT "previous category", and
    // taking them as Spells / Items left LB stuck on the Spells tab
    auto keyP = [&](int a) { return (gKeys.key1[a] && in.pressed[gKeys.key1[a]]) || (gKeys.key2[a] && in.pressed[gKeys.key2[a]]); };
    if (menuPrompt.empty() && !menuSubFocus) {
        int want = -1;
        if (actP(in, A_JOURNAL)) { if (menuTab == 3) { jTab = 0; jSel = 0; jScroll = 0; pushMode(GMode::JOURNAL); return; } want = 3; }
        else if (keyP(A_SPELLS) && menuTab != 2) want = 2;
        else if (keyP(A_ITEMS) && menuTab != 1 && !(menuTab == 0 && menuMember > 0)) want = 1;
        if (want >= 0) { menuTab = want; menuSel = 0; spellCol = 0; menuScroll = 0; menuSubFocus = false; gAudio.play("ui_move"); return; }
    }
    // tab clicks
    for (int i = 0; i < NTABS && !themed && !menuHostedNow; i++) { int tx = 20 + i * 110; if (in.mousePressed[0] && in.mouseX >= tx && in.mouseX < tx + 100 && in.mouseY >= 14 && in.mouseY < 34) { menuTab = i; menuSel = 0; spellCol = 0; menuScroll = 0; menuSubFocus = false; menuPrompt.clear(); } }
    int nMembers = (int)party.members.size();
    if (menuTab == 0 && menuPrompt.empty() && !menuSubFocus && (in.pressed[K_L] || actP(in, A_INTERACT)) && party.members[menuMember].alive) { leader = menuMember; notify(party.members[leader].name + " takes the lead."); }
    if (menuTab == 0 && menuPrompt.empty() && !menuSubFocus && (in.pressed[K_T] || actP(in, A_ITEMS)) && menuMember > 0 && menuMember < nMembers) { std::string cls = party.members[menuMember].classId; auto it = extra.find("dlg_" + cls); std::string did = it != extra.end() ? it->second : "";
        if (did.empty()) if (const NpcDef* nd = companionDef(cls)) { std::string cd = trim(nd->get("dialogue")); if (dialogues.count(cd) && dialogues[cd].nodes.count("camp")) { did = cd; extra["dlg_" + cls] = did; } }   // 1.1.9: the companion NPC's own dialogue
        if (did.empty()) for (auto& d : dialogues) { for (auto& n : d.second.nodes) for (auto& a : n.second.actions) if (a.first == "join" && parseJoin(a.second).cls == cls) did = d.first; if (!did.empty()) { extra["dlg_" + cls] = did; break; } }
        if (!did.empty() && dialogues.count(did) && dialogues[did].nodes.count("camp")) { popMode(); startDialogue(did, nullptr, "camp"); return; } else notify(party.members[menuMember].name + " has nothing to say right now."); }
    if (menuTab == 1 && menuPrompt.empty() && !menuSubFocus && (in.pressed[K_C] || (gKeys.pad[A_INTERACT] && in.pressed[gKeys.pad[A_INTERACT]]))) { popMode(); openCraft("any"); return; }   // 1.1.7: pad X too (free on this tab)
    if (menuTab == 0) {
        if (!menuSubFocus) {
            // the member list runs top to bottom: up/down picks a member, right (or confirm) steps into their equipment
            if (actP(in, A_UP)) menuMember = (menuMember + nMembers - 1) % nMembers;
            if (actP(in, A_DOWN)) menuMember = (menuMember + 1) % nMembers;
            for (int i = 0; i < nMembers && !themed; i++) if (in.mousePressed[0] && in.mouseX >= 20 && in.mouseX < 150 && in.mouseY >= 50 + i * 40 && in.mouseY < 88 + i * 40) menuMember = i;
            if (actP(in, A_CONFIRM) || actP(in, A_RIGHT)) { menuSubFocus = true; menuSub = 0; }
            if (!themed && in.mousePressed[0] && in.mouseX > 160 && in.mouseY > sheetEquipY && in.mouseY < sheetEquipY + SL_COUNT * 14) { menuSubFocus = true; menuSub = clampi((in.mouseY - sheetEquipY) / 14, 0, SL_COUNT - 1); menuPrompt.clear(); }
        } else if (menuPrompt.empty()) {
            if (actP(in, A_UP)) menuSub = std::max(0, menuSub - 1);
            if (actP(in, A_DOWN)) menuSub = std::min(SL_COUNT - 1, menuSub + 1);
            if (actP(in, A_LEFT)) { menuSubFocus = false; }
            if (!themed && in.mouseDY && in.mouseX > 160 && in.mouseY > sheetEquipY && in.mouseY < sheetEquipY + SL_COUNT * 14) menuSub = clampi((in.mouseY - sheetEquipY) / 14, 0, SL_COUNT - 1);
            if (actP(in, A_CONFIRM) || (!themed && in.mousePressed[0])) { menuPrompt = "equip"; menuSel = 0; menuScroll = 0; }
            if (in.pressed[K_DELETE] || actP(in, A_ITEMS)) unequip(menuMember, menuSub);
        } else {
            // choose item for slot
            std::vector<std::string> cands;
            for (auto& it : party.inventory) { auto* d = gd->item(it.id); if (d && d->slot() == menuSub) cands.push_back(it.id); }
            int n = (int)cands.size() + 1; // + unequip
            if (actP(in, A_UP)) menuSel = (menuSel + n - 1) % n;
            if (actP(in, A_DOWN)) menuSel = (menuSel + 1) % n;
            int lx = 330, ly = 80;
            for (int i = 0; i < n && i < 14 && !themed; i++) if (in.mouseX >= lx && in.mouseX < lx + 280 && in.mouseY >= ly + 24 + i * 16 && in.mouseY < ly + 40 + i * 16) { if (in.mouseDX || in.mouseDY) menuSel = i; }
            if (actP(in, A_CONFIRM) || (!themed && in.mousePressed[0] && in.mouseX >= lx && in.mouseX < lx + 280 && in.mouseY >= ly + 24 && in.mouseY < ly + 24 + n * 16)) {
                if (menuSel == 0) unequip(menuMember, menuSub); else equipItem(menuMember, cands[menuSel - 1]);
                menuPrompt.clear();
            }
        }
    } else if (menuTab == 1) {
        { std::vector<std::string> ids; for (auto& st : party.inventory) ids.push_back(st.id); auto cats = itemCats(ids);
          bool known = invCat == "All"; for (auto& c : cats) if (c.first == invCat) known = true; if (!known) invCat = "All";
          if (!menuSubFocus && catBarInput(ctx, in, cats, (int)ids.size(), 20, 36, 600, invCat, invSort)) { menuSel = 0; menuScroll = 0; gAudio.play("ui_move"); } }
        auto view = invView();
        int n = (int)view.size();
        if (!menuSubFocus) {
            if (n > 0) { if (actP(in, A_UP)) menuSel = (menuSel + n - 1) % n; if (actP(in, A_DOWN)) menuSel = (menuSel + 1) % n; }
            menuSel = clampi(menuSel, 0, std::max(0, n - 1));
            const int rowsN = invRows();
            if (in.wheel) menuScroll = clampi(menuScroll - in.wheel, 0, std::max(0, n - rowsN));
            if (menuSel < menuScroll) menuScroll = menuSel; if (menuSel >= menuScroll + rowsN) menuScroll = menuSel - rowsN + 1;
            for (int i = 0; i < rowsN; i++) { int idx = menuScroll + i; if (idx >= n) break; if (in.mouseX >= 20 && in.mouseX < 300 && in.mouseY >= 50 + i * 16 && in.mouseY < 66 + i * 16) { if (in.mouseDX || in.mouseDY) menuSel = idx; if (in.mousePressed[0]) { menuSel = idx; menuSubFocus = true; menuSub = 0; } } }
            for (int f = 0; f < 8; f++) if (actP(in, A_FAV1 + f) && n > 0) { const ItemDef* d = gd->item(party.inventory[view[menuSel]].id); if (d && d->type == IT_CONSUMABLE) { party.members[menuMember].favs[f] = "item:" + d->id; log(fmt("%s set as favorite %d for %s.", d->name.c_str(), f + 1, party.members[menuMember].name.c_str())); } else log("Only consumables can be favorites."); }
            if (actP(in, A_CONFIRM) && n > 0) { menuSubFocus = true; menuSub = 0; }
        } else {
            if (n == 0) { menuSubFocus = false; return; }
            menuSel = clampi(menuSel, 0, n - 1);
            const ItemDef* d = gd->item(party.inventory[view[menuSel]].id);
            if (!d) { menuSubFocus = false; return; }
            // actions: Use / Equip (choose member), Drop
            std::vector<std::string> acts;
            if (d->effect == "lure" || startsWith(d->effect, "lure:")) acts.push_back("Throw...");   // 1.5.0 (#99): a noisemaker is thrown, not used on someone
            else if (d->type == IT_CONSUMABLE && d->dmgMax == 0 && d->useSpell.empty()) acts.push_back("Use on...");
            if (d->equippable()) acts.push_back("Equip on...");
            acts.push_back(d->quest ? "Quest item - can't drop" : "Drop");
            if (menuPrompt.empty()) {
                int na = (int)acts.size();
                if (actP(in, A_UP)) menuSub = (menuSub + na - 1) % na;
                if (actP(in, A_DOWN)) menuSub = (menuSub + 1) % na;
                for (int i = 0; i < na; i++) if (in.mouseX >= 330 && in.mouseX < 600 && in.mouseY >= INV_ACT_Y + i * 18 && in.mouseY < INV_ACT_Y + 18 + i * 18) { if (in.mouseDX || in.mouseDY) menuSub = i; }
                if (actP(in, A_CONFIRM) || (in.mousePressed[0] && in.mouseX >= 330 && in.mouseX < 600 && in.mouseY >= INV_ACT_Y && in.mouseY < INV_ACT_Y + na * 18)) {
                    std::string a = acts[clampi(menuSub, 0, na - 1)];
                    if (a == "Quest item - can't drop") { log(d->name + " is needed for a quest. You keep it."); gAudio.play("ui_error"); }
                    else if (a == "Throw...") { menuSubFocus = false; useItemOn(d->id, party.members[0], false); return; }
                    else if (a == "Drop") { log("Dropped " + d->name + "."); party.removeItem(d->id, 1); menuSubFocus = false; menuSel = clampi(menuSel, 0, std::max(0, (int)invView().size() - 1)); }
                    else { menuPrompt = a == "Use on..." ? "use" : "equipitem"; menuMember = 0; }
                }
            } else {
                if (actP(in, A_UP)) menuMember = (menuMember + nMembers - 1) % nMembers;
                if (actP(in, A_DOWN)) menuMember = (menuMember + 1) % nMembers;
                for (int i = 0; i < nMembers; i++) if (in.mouseX >= 330 && in.mouseX < 600 && in.mouseY >= INV_WHO_Y + 18 + i * 18 && in.mouseY < INV_WHO_Y + 36 + i * 18) { if (in.mouseDX || in.mouseDY) menuMember = i; }
                if (actP(in, A_CONFIRM) || (in.mousePressed[0] && in.mouseX >= 330 && in.mouseX < 600 && in.mouseY >= INV_WHO_Y + 18 && in.mouseY < INV_WHO_Y + 18 + nMembers * 18)) {
                    std::string id = d->id;
                    if (menuPrompt == "use") useItemOn(id, party.members[menuMember], false); else equipItem(menuMember, id);
                    menuPrompt.clear(); menuSubFocus = false; menuSel = clampi(menuSel, 0, std::max(0, (int)invView().size() - 1));
                }
            }
        }
    } else if (menuTab == 2) {
        if (!menuSubFocus) {
            // two columns like the party tab: the member list (up/down, right or confirm to enter) and that member's spells
            auto& sp = party.members[menuMember].spells; int n = (int)sp.size();
            for (int i = 0; i < nMembers; i++) if (in.mousePressed[0] && in.mouseX >= 20 && in.mouseX < 150 && in.mouseY >= 50 + i * 40 && in.mouseY < 88 + i * 40) { if (menuMember != i) menuSel = 0; menuMember = i; spellCol = 0; }
            if (spellCol == 0) {
                int was = menuMember;
                if (actP(in, A_UP)) menuMember = (menuMember + nMembers - 1) % nMembers;
                if (actP(in, A_DOWN)) menuMember = (menuMember + 1) % nMembers;
                if (was != menuMember) menuSel = 0;
                if ((actP(in, A_RIGHT) || actP(in, A_CONFIRM)) && !party.members[menuMember].spells.empty()) { spellCol = 1; menuSel = clampi(menuSel, 0, (int)party.members[menuMember].spells.size() - 1); }
            } else {
                if (n == 0 || actP(in, A_LEFT)) { spellCol = 0; return; }
                if (actP(in, A_UP)) menuSel = (menuSel + n - 1) % n; if (actP(in, A_DOWN)) menuSel = (menuSel + 1) % n;
            }
            menuSel = clampi(menuSel, 0, std::max(0, n - 1));
            for (int i = 0; i < n; i++) if (in.mouseX >= 170 && in.mouseX < 450 && in.mouseY >= 50 + i * 18 && in.mouseY < 68 + i * 18) { if (in.mouseDX || in.mouseDY) { menuSel = i; spellCol = 1; } if (in.mousePressed[0]) { menuSel = i; spellCol = 1; in.pressed[K_ENTER] = true; } }
            if (spellCol == 1) for (int f = 0; f < 8; f++) if (actP(in, A_FAV1 + f) && n > 0) { party.members[menuMember].favs[f] = "spell:" + sp[menuSel]; log(fmt("%s set as favorite %d for %s.", gd->spell(sp[menuSel]) ? gd->spell(sp[menuSel])->name.c_str() : "?", f + 1, party.members[menuMember].name.c_str())); }
            if (spellCol == 1 && (actP(in, A_CONFIRM) || in.pressed[K_ENTER]) && n > 0) { const SpellDef* s = gd->spell(sp[menuSel]); if (s && spellUsableOutside(*s)) { if (!s->utility.empty() || s->target == "self") { castUtility(menuMember, sp[menuSel], menuMember); } else { menuSubFocus = true; menuSub = 0; } } else log("That spell can only be used in combat."); }
        } else {
            if (actP(in, A_UP)) menuSub = (menuSub + nMembers - 1) % nMembers;
            if (actP(in, A_DOWN)) menuSub = (menuSub + 1) % nMembers;
            for (int i = 0; i < nMembers; i++) if (in.mouseX >= 330 && in.mouseX < 600 && in.mouseY >= 300 + i * 18 && in.mouseY < 318 + i * 18) { if (in.mouseDX || in.mouseDY) menuSub = i; }
            if (actP(in, A_CONFIRM) || (in.mousePressed[0] && in.mouseX >= 330 && in.mouseX < 600 && in.mouseY >= 300 && in.mouseY < 300 + nMembers * 18)) {
                Character& caster = party.members[menuMember];
                if (menuSel < (int)caster.spells.size()) castUtility(menuMember, caster.spells[menuSel], menuSub);
                menuSubFocus = false;
            }
        }
    } else if (menuTab == 3) {
        // only the quests the party knows about are listed, so the selection walks over those
        int n = 0; for (auto& q : quests) if (!questText(q).empty()) n++;
        if (n > 0) { if (actP(in, A_UP)) menuSel = (menuSel + n - 1) % n; if (actP(in, A_DOWN)) menuSel = (menuSel + 1) % n; }
        if (in.wheel) menuSel = clampi(menuSel - in.wheel, 0, std::max(0, n - 1));
        menuSel = clampi(menuSel, 0, std::max(0, n - 1));
    }
}

void Game::drawMenu(Canvas& cv, InputState& in) {
    uiRect(0, 0, SCREEN_W, SCREEN_H);
    if (!menuHostedNow) {   // 1.8.5 (#199): hosted, the theme's window and the Party tabs are drawn around this (hud.cpp)
        cv.dim(0, 0, SCREEN_W, SCREEN_H, 150);
        panel(ctx, 8, 8, SCREEN_W - 16, SCREEN_H - 16);
        { std::vector<TabSpec> ts; for (int i = 0; i < NTABS; i++) ts.push_back({20 + i * 110, 100, TABS[i]}); tabRow(ctx, ts, 14, 20, menuTab, 12, SCREEN_W - 12); }   // 1.8.4 (#189)
        textRight(ctx, SCREEN_W - 20, 18, fmt("%d gold", party.gold), C::Gold);
    }
    int nMembers = (int)party.members.size();
    if (menuTab == 0 || menuTab == 2) {
        for (int i = 0; i < nMembers; i++) {
            auto& m = party.members[i]; bool sel = i == menuMember;
            bool away = (menuTab == 0 && menuSubFocus) || (menuTab == 2 && (spellCol == 1 || menuSubFocus));
            listRow(ctx, in, 20, 50 + i * 40, 130, 38, sel && !away);
            if (sel && away) cv.rect(20, 50 + i * 40, 130, 38, C::Gold);
            if (m.recolor) ctx.ts->drawRecolor(cv, m.tile, 26, 56 + i * 40, 2, m.color); else ctx.ts->draw(cv, m.tile, 26, 56 + i * 40, 2);
            UI::text(ctx, 56, 54 + i * 40, m.name.substr(0, 11), m.alive ? C::White : C::Red);
            UI::text(ctx, 56, 68 + i * 40, fmt("L%d %d/%d", m.level, m.hp, m.maxHP(*gd)), C::Gray);
            if (m.wounds > 0) UI::text(ctx, 128, 68 + i * 40, fmt("W%d", m.wounds), RGB(235, 105, 95));   // 1.5.0 (#94)
        }
    }
    if (menuTab == 0) {
        drawCharacterSheet(cv, in, menuMember, 160, 44, 460, 400);
        if (menuPrompt == "respec") {
            int bx = 330, by = 120, bw = 280;
            panelTitle(ctx, bx, by, bw, 150, !respecItem.empty() ? "Draught of Forgetting" : respecFree ? "Sir Vael's Drill" : "Rite of Renewal");
            int cy = by + 24 + wrapped(ctx, bx + 12, by + 24, bw - 24, "Choose who to retrain. All stat points and class ranks are returned and chosen again; class spells are relearned through the new ranks.", C::LtGray);
            UI::text(ctx, bx + 12, cy + 6, party.members[menuMember].name, C::Gold);
            int cost = respecCost(menuMember);
            UI::text(ctx, bx + 12, cy + 20, cost ? fmt("Cost: %d gold", cost) : "Free", party.gold >= cost ? C::Cream : C::Red);
            keyHint(ctx, bx + 12, by + 128, "Enter", "retrain"); keyHint(ctx, bx + 130, by + 128, "Esc", "cancel");
        } else if (menuPrompt == "equip") {
            std::vector<std::string> cands; for (auto& it : party.inventory) { auto* d = gd->item(it.id); if (d && d->slot() == menuSub) cands.push_back(it.id); }
            int lx = 330, ly = 80, n = (int)cands.size() + 1;
            const int detH = std::min(104, SCREEN_H - 76 - (ly + 34 + std::min(14, n) * 16));   // room for the item details under the list
            panelTitle(ctx, lx, ly, 280, 34 + std::min(14, n) * 16 + detH, std::string("Equip ") + slotName(menuSub));
            for (int i = 0; i < n && i < 14; i++) {
                bool sel = menuSel == i; if (sel) cv.fillRect(lx + 6, ly + 24 + i * 16, 268, 15, RGB(90, 70, 20));
                if (i == 0) UI::text(ctx, lx + 12, ly + 26, "(remove)", sel ? C::Yellow : C::Gray);
                else { const ItemDef* d = gd->item(cands[i - 1]); drawItemTile(*ctx.ts, cv, *d, lx + 10, ly + 25 + i * 16, 1); UI::text(ctx, lx + 26, ly + 26 + i * 16, d->name, sel ? C::Yellow : C::White); }
            }
            if (menuSel > 0 && menuSel - 1 < (int)cands.size()) { const ItemDef* d = gd->item(cands[menuSel - 1]); drawItemDetails(cv, *d, lx + 10, ly + 28 + std::min(14, n) * 16, 260, ly + 26 + std::min(14, n) * 16 + detH, &party.members[menuMember]); }   // 1.1.5 (#26): 8 px above the panel bottom
        } else {
            // 1.8.5 (#200): each hint starts where the last one ended (a controller's names are longer than the keys')
            int hx = 170;
            if (menuSubFocus) { hx = keyHint(ctx, hx, SCREEN_H - 30, "Up/Down", "slot"); hx = keyHint(ctx, hx, SCREEN_H - 30, "Enter", "change"); hx = keyHint(ctx, hx, SCREEN_H - 30, gKeys.label(A_ITEMS), "unequip"); keyHint(ctx, hx, SCREEN_H - 30, "Left", "back"); }
            else { hx = keyHint(ctx, hx, SCREEN_H - 30, "Up/Down", "member"); hx = keyHint(ctx, hx, SCREEN_H - 30, "Right", "equipment"); hx = keyHint(ctx, hx, SCREEN_H - 30, gKeys.label(A_PAGEDOWN), "next tab"); keyHint(ctx, hx, SCREEN_H - 30, "Esc", "back"); }
            if (party.members.size() > 1 && !menuSubFocus) { hx = keyHint(ctx, 170, SCREEN_H - 44, gKeys.label(A_INTERACT), leader == menuMember ? "leading the party" : "make party leader"); if (menuMember > 0) keyHint(ctx, hx, SCREEN_H - 44, gKeys.label(A_ITEMS), "talk to " + party.members[menuMember].name); }
        }
    } else if (menuTab == 1) {
        { std::vector<std::string> ids; for (auto& st : party.inventory) ids.push_back(st.id); catBarDraw(ctx, cv, itemCats(ids), (int)ids.size(), 20, 36, 600, invCat, invSort); }
        auto view = invView();
        int n = (int)view.size();
        if (n == 0) UI::text(ctx, 24, 54, party.inventory.empty() ? "Your pack is empty." : "Nothing in this category.", C::Gray);
        const int rowsN = invRows();
        for (int i = 0; i < rowsN; i++) {
            int idx = menuScroll + i; if (idx >= n) break;
            auto& st = party.inventory[view[idx]]; const ItemDef* d = gd->item(st.id); if (!d) continue;
            bool sel = idx == menuSel;
            listRow(ctx, in, 20, 50 + i * 16, 280, 16, sel);
            drawItemTile(*ctx.ts, cv, *d, 24, 52 + i * 16, 1);
            UI::text(ctx, 42, 52 + i * 16, itemLabel(*d, st.count), sel ? C::Yellow : d->quest ? C::Cream : C::White);
            textRight(ctx, 296, 52 + i * 16, d->quest ? "quest" : fmt("%d g", itemSellPrice(*d)), d->quest ? C::Gold : C::DkGray);
            for (int f = 0; f < 8; f++) if (party.members[menuMember].favs[f] == "item:" + d->id) UI::text(ctx, 32, 52 + i * 16, std::to_string(f + 1), C::Cyan);
        }
        if (n > rowsN && n - rowsN - menuScroll > 0) UI::text(ctx, 20, 50 + rowsN * 16, fmt("(%d more - scroll)", n - rowsN - menuScroll), C::Gray);
        if (n > 0 && menuSel < n) {
            const ItemDef* d = gd->item(party.inventory[view[menuSel]].id);
            panel(ctx, 320, 60, 300, INV_ACT_Y - 66, RGB(30, 30, 44));   // 1.1.4: 26 px taller, room for the grouped details
            drawItemTile(*ctx.ts, cv, *d, 330, 70, 2);
            ctx.font->bold = true; UI::text(ctx, 362, 70, d->name, C::Gold); ctx.font->bold = false;
            UI::text(ctx, 362, 83, itemCategory(*d), C::Gray);   // 1.1.5 (#26): the item type sits under the name, not on the panel's bottom edge
            drawItemDetails(cv, *d, 330, 98, 280, INV_ACT_Y - 14, d->equippable() && menuMember < (int)party.members.size() ? &party.members[menuMember] : nullptr);   // panel bottom (INV_ACT_Y - 6) less 8 px of padding
            if (menuSubFocus) {
                std::vector<std::string> acts;
                if (d->effect == "lure" || startsWith(d->effect, "lure:")) acts.push_back("Throw...");   // 1.5.0 (#99)
                else if (d->type == IT_CONSUMABLE && d->dmgMax == 0 && d->useSpell.empty()) acts.push_back("Use on...");
                if (d->equippable()) acts.push_back("Equip on...");
                acts.push_back(d->quest ? "Quest item - can't drop" : "Drop");
                for (size_t i = 0; i < acts.size(); i++) { bool sel = (int)i == menuSub && menuPrompt.empty(); listRow(ctx, in, 330, INV_ACT_Y + (int)i * 18, 270, 18, sel); UI::text(ctx, 340, INV_ACT_Y + 3 + (int)i * 18, acts[i], sel ? C::Yellow : C::White); }
                if (!menuPrompt.empty()) {
                    UI::text(ctx, 330, INV_WHO_Y, "Who?", C::Gold);
                    for (int i = 0; i < nMembers; i++) { bool sel = i == menuMember; listRow(ctx, in, 330, INV_WHO_Y + 18 + i * 18, 270, 18, sel); auto& m = party.members[i]; UI::text(ctx, 340, INV_WHO_Y + 21 + i * 18, fmt("%s  %d/%d", m.name.c_str(), m.hp, m.maxHP(*gd)), sel ? C::Yellow : (m.alive ? C::White : C::Red)); }
                }
            }
        }
        { int hx = keyHint(ctx, 20, SCREEN_H - 30, "Enter", "actions"); hx = keyHint(ctx, hx, SCREEN_H - 30, "1-8", "set favorite"); hx = keyHint(ctx, hx, SCREEN_H - 30, "Esc", "back");
          keyHint(ctx, std::max(hx, 330), SCREEN_H - 30, gPadGlyphs && gKeys.pad[A_INTERACT] ? keyName(gKeys.pad[A_INTERACT]) : "C", "field crafting"); }
        { int hx = keyHint(ctx, 20, SCREEN_H - 44, gKeys.label(A_CATPREV) + "/" + gKeys.label(A_CATNEXT), "category"); keyHint(ctx, hx, SCREEN_H - 44, gKeys.label(A_SORT), "sort"); }
    } else if (menuTab == 2) {
        auto& m = party.members[menuMember];
        UI::text(ctx, 170, 38, fmt("%s's spells  (MP %d/%d)", m.name.c_str(), m.mp, m.maxMP(*gd)), C::Gray);
        if (m.spells.empty()) UI::text(ctx, 170, 60, "Knows no spells.", C::DkGray);
        for (size_t i = 0; i < m.spells.size(); i++) {
            const SpellDef* s = gd->spell(m.spells[i]); if (!s) continue;
            bool sel = (int)i == menuSel && spellCol == 1; listRow(ctx, in, 170, 50 + (int)i * 18, 280, 18, sel);
            ctx.ts->draw(cv, s->tile, 174, 52 + (int)i * 18, 1);
            bool usable = spellUsableOutside(*s);
            UI::text(ctx, 192, 53 + (int)i * 18, s->name, sel ? C::Yellow : (usable ? C::White : C::Gray));
            textRight(ctx, 444, 53 + (int)i * 18, (party.members[menuMember].level < s->level ? fmt("needs Lv %d", s->level) : fmt("%d MP%s", s->cost, usable ? "" : " (combat)")), sel ? C::Cream : usable ? C::LtGray : C::DkGray);
            for (int f = 0; f < 8; f++) if (m.favs[f] == "spell:" + m.spells[i]) UI::text(ctx, 178, 53 + (int)i * 18, std::to_string(f + 1), C::Cyan);
        }
        if (!m.spells.empty() && menuSel < (int)m.spells.size()) {
            const SpellDef* s = gd->spell(m.spells[menuSel]);
            if (s) { panel(ctx, 460, 44, 160, 130, RGB(30, 30, 44)); std::string d = s->desc; if (s->dmgMax) d = fmt("%d-%d %s dmg. ", s->dmgMin, s->dmgMax, dmgTypeName(s->dmgType)) + d; if (s->heal) d = fmt("Heals %d. ", s->heal) + d; d += fmt(" Range %d, %d AP.", s->range, s->ap); if (s->area) d += fmt(" Area %d.", s->area); ctx.font->drawWrapped(cv, *ctx.ts, 468, 52, 146, d, C::LtGray); }
            if (menuSubFocus) { UI::text(ctx, 330, 282, "Cast on whom?", C::Gold); for (int i = 0; i < nMembers; i++) { bool sel = i == menuSub; listRow(ctx, in, 330, 300 + i * 18, 270, 18, sel); auto& t = party.members[i]; UI::text(ctx, 340, 303 + i * 18, fmt("%s  %d/%d", t.name.c_str(), t.hp, t.maxHP(*gd)), sel ? C::Yellow : (t.alive ? C::White : C::Red)); } }
        }
        { int hx = 170;
          if (spellCol == 0) { hx = keyHint(ctx, hx, SCREEN_H - 30, "Up/Down", "member"); hx = keyHint(ctx, hx, SCREEN_H - 30, "Right", "spells"); hx = keyHint(ctx, hx, SCREEN_H - 30, gKeys.label(A_PAGEDOWN), "next tab"); keyHint(ctx, hx, SCREEN_H - 30, "Esc", "back"); }
          else { hx = keyHint(ctx, hx, SCREEN_H - 30, "Enter", "cast"); hx = keyHint(ctx, hx, SCREEN_H - 30, "1-8", "set favorite"); hx = keyHint(ctx, hx, SCREEN_H - 30, "Left", "members"); keyHint(ctx, hx, SCREEN_H - 30, "Esc", "back"); } }
    } else if (menuTab == 3) {
        // Each quest's text wraps to the width of the list, and the row grows to fit it. The list scrolls to keep the
        // selected quest on screen.
        const int top = 46, bottom = SCREEN_H - 40, rowX = 20, rowW = SCREEN_W - 40, textW = rowW - 18;
        struct QRow { size_t qi; std::vector<std::string> lines; bool done, failed; int h; };
        std::vector<QRow> rows;
        for (size_t i = 0; i < quests.size(); i++) {
            std::string st = questText(quests[i]); if (st.empty()) continue;
            QRow r; r.qi = i; r.failed = startsWith(st, "FAILED"); r.done = startsWith(st, "DONE") || r.failed;
            if (startsWith(st, "DONE ")) st = st.substr(5); else if (startsWith(st, "FAILED ")) st = st.substr(7);
            r.lines = ctx.font->wrap(st, textW); r.h = 18 + (int)r.lines.size() * 12 + 4; rows.push_back(r);
        }
        int sel = clampi(menuSel, 0, std::max(0, (int)rows.size() - 1));
        // first row to draw: walk back from the selection while it still fits
        int first = sel, used = sel < (int)rows.size() ? rows[sel].h + 2 : 0;
        while (first > 0 && used + rows[first - 1].h + 2 <= bottom - top) { first--; used += rows[first].h + 2; }
        if (first > 0 && sel < (int)rows.size()) {   // fill from the top instead when everything up to the selection fits
            int all = 0; for (int i = 0; i <= sel; i++) all += rows[i].h + 2; if (all <= bottom - top) first = 0;
        }
        cv.setClip({rowX, top, rowW, bottom - top});
        int y = top, lastFull = first - 1;
        for (int k = first; k < (int)rows.size() && y < bottom; k++) {
            auto& r = rows[k]; const QuestDef& q = quests[r.qi]; if (y + r.h <= bottom) lastFull = k;
            listRow(ctx, in, rowX, y, rowW, r.h, k == sel);
            ctx.font->bold = true; UI::text(ctx, rowX + 8, y + 3, q.name, r.failed ? C::Red : r.done ? C::Gray : C::Gold); ctx.font->bold = false;
            if (r.failed) textRight(ctx, rowX + rowW - 8, y + 3, "failed", C::Red); else if (r.done) textRight(ctx, rowX + rowW - 8, y + 3, "done", C::Green);
            for (size_t l = 0; l < r.lines.size(); l++) UI::text(ctx, rowX + 8, y + 18 + (int)l * 12, r.lines[l], r.done ? C::DkGray : C::White);
            y += r.h + 2;
        }
        cv.resetClip();
        if (first > 0) UI::text(ctx, SCREEN_W - 34, top - 10, "^", C::Gold);
        if (lastFull < (int)rows.size() - 1) UI::text(ctx, SCREEN_W - 34, bottom + 1, "v", C::Gold);
        if (rows.empty()) UI::text(ctx, 20, 50, "No quests yet. Talk to people!", C::Gray);
        keyHint(ctx, 20, SCREEN_H - 30, "Up/Down", "select"); keyHint(ctx, 150, SCREEN_H - 30, gKeys.label(A_JOURNAL), "journal (checklists, map pins)");
    }
}

// ---------------- shop ----------------
// Buying and selling in stacks: Left/Right (or the [-]/[+] buttons) set the quantity, Enter buys/sells that many,
// Interact (E / pad X) does the most you can. LB/RB, [ ] or Tab switch between Buy and Sell.
// Weapons and armour are in limited stock and restock every few days; consumables and ammo never run out.
int Game::shopStockLeft(const std::string& shop, const std::string& item, int listed) const {
    const ItemDef* d = gd->item(item); if (!d) return 0;
    int base = listed;
    if (base < 0) base = d->equippable() ? (d->value < 150 ? 2 : 1) : -1;
    if (base < 0) return -1;
    auto it = extra.find("sold_" + shop + "_" + item);
    return std::max(0, base - (it == extra.end() ? 0 : atoi(it->second.c_str())));
}
void Game::shopRestockCheck(const std::string& shop) {
    std::string key = "restock_" + shop; auto it = extra.find(key);
    int last = it == extra.end() ? -1 : atoi(it->second.c_str());
    if (last >= 0 && day - last < 3) return;
    std::vector<std::string> drop; std::string pre = "sold_" + shop + "_";
    for (auto& kv : extra) if (startsWith(kv.first, pre)) drop.push_back(kv.first);
    for (auto& k : drop) extra.erase(k);
    extra[key] = std::to_string(day);
}

static int shopDefaultQty(const ItemDef& d) { return d.type == IT_AMMO ? 20 : 1; }
int Game::shopMaxQty(int tab, int sel) {
    auto stock = shopItems(shopId);
    if (tab == 0) {
        if (sel < 0 || sel >= (int)stock.size()) return 0;
        const ItemDef* d = gd->item(stock[sel].first); if (!d) return 0;
        int price = itemBuyPrice(*d); int n = price > 0 ? party.gold / price : 99;
        int left = shopStockLeft(shopId, d->id, stock[sel].second); if (left >= 0) n = std::min(n, left);
        if (!d->stackable()) n = std::min(n, left < 0 ? 5 : left);
        return clampi(n, 0, 999);
    }
    if (sel < 0 || sel >= (int)party.inventory.size()) return 0;
    const ItemDef* d = gd->item(party.inventory[sel].id); if (!d || d->quest) return 0;
    return party.inventory[sel].count;
}
void Game::shopTransact(int qty) {
    auto stock = shopItems(shopId);
    auto view = shopView(); if (shopSel < 0 || shopSel >= (int)view.size()) return;
    int shopSel = view[this->shopSel];   // the stock row / inventory slot behind the selected list row
    if (shopTab == 0) {
        if (shopSel >= (int)stock.size()) return;
        const ItemDef* d = gd->item(stock[shopSel].first); if (!d) return;
        int price = itemBuyPrice(*d);
        int left = shopStockLeft(shopId, d->id, stock[shopSel].second);
        if (left == 0) { log("Sold out - come back in a few days."); gAudio.play("ui_error"); return; }
        if (left > 0) qty = std::min(qty, left);
        if (qty <= 0 || party.gold < price) { log("Not enough gold."); gAudio.play("ui_error"); return; }
        qty = std::min(qty, party.gold / price);
        party.gold -= price * qty; stats.goldSpent += price * qty; party.addItem(d->id, qty, *gd); bountyEvent("item", d->id, qty);
        if (left > 0) extra["sold_" + shopId + "_" + d->id] = std::to_string(atoi(extra["sold_" + shopId + "_" + d->id].c_str()) + qty);
        log(qty > 1 ? fmt("Bought %d %s for %d gold.", qty, d->name.c_str(), price * qty) : fmt("Bought %s for %d gold.", d->name.c_str(), price)); gAudio.play("shop_buy");
    } else {
        if (shopSel >= (int)party.inventory.size()) return;
        const ItemDef* d = gd->item(party.inventory[shopSel].id); if (!d) return;
        if (d->quest) { log("You shouldn't sell that."); return; }
        qty = std::min(qty, party.inventory[shopSel].count); if (qty <= 0) return;
        int got = itemSellPrice(*d) * qty;
        party.gold += got; stats.goldEarned += got; party.removeItem(d->id, qty);
        log(qty > 1 ? fmt("Sold %d %s for %d gold.", qty, d->name.c_str(), got) : fmt("Sold %s for %d gold.", d->name.c_str(), got)); gAudio.play("shop_sell");
        this->shopSel = clampi(this->shopSel, 0, std::max(0, (int)shopView().size() - 1));
    }
    shopQty = -1;
}
void Game::updateShop(InputState& in, float dt) {
    if (actP(in, A_CANCEL)) { popMode(); return; }
    auto switchTab = [&](int t) { shopTab = t; shopSel = 0; shopScroll = 0; shopQty = -1; };
    if (actP(in, A_PAGEUP) || actP(in, A_PAGEDOWN) || actP(in, A_MENU)) switchTab(1 - shopTab);
    for (int i = 0; i < 2; i++) if (in.mousePressed[0] && in.mouseX >= 20 + i * 110 && in.mouseX < 120 + i * 110 && in.mouseY >= 14 && in.mouseY < 34) switchTab(i);
    auto stock = shopItems(shopId);
    { std::vector<std::string> ids; if (shopTab == 0) for (auto& s2 : stock) ids.push_back(s2.first); else for (auto& s2 : party.inventory) ids.push_back(s2.id); auto cats = itemCats(ids);
      bool known = shopCat == "All"; for (auto& c : cats) if (c.first == shopCat) known = true; if (!known) shopCat = "All";
      if (catBarInput(ctx, in, cats, (int)ids.size(), 20, 36, 600, shopCat, shopSort)) { shopSel = 0; shopScroll = 0; shopQty = -1; gAudio.play("ui_move"); } }
    auto view = shopView();
    int n = (int)view.size();
    int before = shopSel;
    if (n > 0) { if (actP(in, A_UP)) shopSel = (shopSel + n - 1) % n; if (actP(in, A_DOWN)) shopSel = (shopSel + 1) % n; }
    shopSel = clampi(shopSel, 0, std::max(0, n - 1));
    const int rowsN = shopRows();
    if (in.wheel) shopScroll = clampi(shopScroll - in.wheel, 0, std::max(0, n - rowsN));
    if (shopSel < shopScroll) shopScroll = shopSel; if (shopSel >= shopScroll + rowsN) shopScroll = shopSel - rowsN + 1;
    bool clicked = false;
    for (int i = 0; i < rowsN; i++) { int idx = shopScroll + i; if (idx >= n) break; if (in.mouseX >= 20 && in.mouseX < 330 && in.mouseY >= 50 + i * 16 && in.mouseY < 66 + i * 16 && in.mousePressed[0]) { shopSel = idx; clicked = true; } }   // only a click selects: moving the mouse to the [-]/[+] buttons must not change the item
    if (shopSel != before) shopQty = -1;
    if (n == 0) return;
    int real = view[shopSel];
    const ItemDef* cur = gd->item(shopTab == 0 ? stock[real].first : party.inventory[real].id); if (!cur) return;
    int maxQ = shopMaxQty(shopTab, real);
    if (shopQty < 0) shopQty = shopTab == 0 ? std::min(shopDefaultQty(*cur), std::max(1, maxQ)) : 1;
    // quantity: tap left/right for one, hold to run (faster after a second)
    int dq = 0;
    if (actP(in, A_LEFT)) { dq = -1; shopQtyHold = 0; } else if (actP(in, A_RIGHT)) { dq = 1; shopQtyHold = 0; }
    else if (actD(in, A_LEFT) || actD(in, A_RIGHT)) { shopQtyHold += dt; float rep = shopQtyHold > 1.2f ? 0.03f : 0.08f; if (shopQtyHold > 0.35f) { shopQtyRep -= dt; if (shopQtyRep <= 0) { shopQtyRep = rep; dq = actD(in, A_LEFT) ? -1 : 1; if (shopQtyHold > 2.0f) dq *= 5; } } }
    else shopQtyHold = 0;
    // mouse: [-] [+] [max] and the Buy/Sell button in the detail panel
    int qx = 360, qy = 248;
    if (in.mousePressed[0] && in.mouseY >= qy && in.mouseY < qy + 18) {
        if (in.mouseX >= qx && in.mouseX < qx + 22) dq = in.down[K_SHIFT] ? -10 : -1;
        else if (in.mouseX >= qx + 86 && in.mouseX < qx + 108) dq = in.down[K_SHIFT] ? 10 : 1;
        else if (in.mouseX >= qx + 114 && in.mouseX < qx + 152) shopQty = std::max(1, maxQ);
    }
    if (dq) { shopQty = clampi(shopQty + dq, 1, std::max(1, maxQ)); if (cur->stackable() || shopTab == 0) gAudio.play("ui_move"); }
    shopQty = clampi(shopQty, 1, std::max(1, maxQ));
    bool button = in.mousePressed[0] && in.mouseX >= qx && in.mouseX < qx + 150 && in.mouseY >= qy + 24 && in.mouseY < qy + 46;
    if (actP(in, A_INTERACT)) { if (maxQ > 0) shopTransact(maxQ); else shopTransact(1); return; }
    if (shopTab == 1 && cur->quest && (actP(in, A_CONFIRM) || button)) { log(cur->name + " is needed for a quest - you won't sell it."); gAudio.play("ui_error"); return; }
    if (actP(in, A_CONFIRM) || button || (clicked && in.down[K_SHIFT])) shopTransact(shopQty);
}

void Game::drawShop(Canvas& cv, InputState& in) {
    uiRect(0, 0, SCREEN_W, SCREEN_H);
    if (!menuHostedNow) { cv.dim(0, 0, SCREEN_W, SCREEN_H, 150); panel(ctx, 8, 8, SCREEN_W - 16, SCREEN_H - 16); }   // 1.8.5: hosted, the theme's window is round it
    const char* tabs[2] = {"Buy", "Sell"};
    { std::vector<TabSpec> ts; for (int i = 0; i < 2; i++) ts.push_back({20 + i * 110, 100, tabs[i]}); tabRow(ctx, ts, 14, 20, shopTab, 12, SCREEN_W - 12); }   // 1.8.4 (#189)
    textRight(ctx, SCREEN_W - 20, 18, fmt("%d gold", party.gold), C::Gold);
    if (!shopFaction.empty()) { int r = reputation.count(shopFaction) ? reputation.at(shopFaction) : 0; textRight(ctx, SCREEN_W - 110, 18, factionName(shopFaction) + ": " + repTier(r), r >= 10 ? C::Cyan : r <= -10 ? C::Red : C::LtGray); }
    auto stock = shopItems(shopId);
    { std::vector<std::string> ids; if (shopTab == 0) for (auto& s2 : stock) ids.push_back(s2.first); else for (auto& s2 : party.inventory) ids.push_back(s2.id); catBarDraw(ctx, cv, itemCats(ids), (int)ids.size(), 20, 36, 600, shopCat, shopSort); }
    auto view = shopView();
    int n = (int)view.size();
    const ItemDef* cur = nullptr; int curReal = -1;
    for (int i = 0; i < shopRows(); i++) {
        int vi = shopScroll + i; if (vi >= n) break; int idx = view[vi];
        const ItemDef* d = gd->item(shopTab == 0 ? stock[idx].first : party.inventory[idx].id); if (!d) continue;
        bool sel = vi == shopSel; if (sel) { cur = d; curReal = idx; }
        listRow(ctx, in, 20, 50 + i * 16, 310, 16, sel);
        drawItemTile(*ctx.ts, cv, *d, 24, 52 + i * 16, 1);
        int price = shopTab == 0 ? itemBuyPrice(*d) : itemSellPrice(*d);
        int left = shopTab == 0 ? shopStockLeft(shopId, d->id, stock[idx].second) : -1;
        std::string nm = shopTab == 0 ? d->name : itemLabel(*d, party.inventory[idx].count);
        if (left >= 0) nm += left == 0 ? "  (sold out)" : fmt("  (%d left)", left);
        UI::text(ctx, 42, 52 + i * 16, nm, sel ? C::Yellow : ((shopTab == 0 && (party.gold < price || left == 0)) || (shopTab == 1 && d->quest) ? C::Gray : C::White));
        textRight(ctx, 326, 52 + i * 16, shopTab == 1 && d->quest ? "quest" : fmt("%d g", price), C::Gold);
    }
    if (n == 0) UI::text(ctx, 20, 52, shopCat != "All" ? "Nothing in this category." : shopTab == 0 ? "Nothing for sale." : "Nothing to sell.", C::Gray);
    if (cur) {
        panel(ctx, 350, 60, 270, 180, RGB(30, 30, 44));
        drawItemTile(*ctx.ts, cv, *cur, 360, 70, 2);
        ctx.font->bold = true; UI::text(ctx, 392, 70, cur->name, C::Gold); ctx.font->bold = false;
        UI::text(ctx, 392, 83, itemCategory(*cur), C::Gray);   // 1.1.5 (#26)
        // 1.1.5 (#26): text stops 8 px above the panel's bottom edge (panel 60..240)
        drawItemDetails(cv, *cur, 360, 98, 250, cur->equippable() ? 177 : 232);
        if (cur->equippable()) { int y = 180; for (auto& m : party.members) { const ItemDef* e = m.equip[cur->slot()].empty() ? nullptr : gd->item(m.equip[cur->slot()]); std::string cmp = e ? e->name : "-"; bool bad = !gearWarning(*cur, m).empty(); UI::text(ctx, 360, y, m.name.substr(0, 10) + ": " + cmp + (bad ? "  (poor fit)" : ""), bad ? RGB(235, 105, 95) : C::Gray); y += 13; if (y + 13 > 232) break; } }
        // quantity + total
        int maxQ = shopMaxQty(shopTab, curReal), q = std::max(1, shopQty);
        int unit = shopTab == 0 ? itemBuyPrice(*cur) : itemSellPrice(*cur);
        int qx = 360, qy = 248;
        button(ctx, in, qx, qy, 22, 18, "-", q > 1); 
        cv.fillRect(qx + 26, qy, 56, 18, RGB(20, 20, 30)); cv.rect(qx + 26, qy, 56, 18, C::Gold); textCenter(ctx, qx + 54, qy + 4, fmt("x%d", q), C::White);
        button(ctx, in, qx + 86, qy, 22, 18, "+", q < maxQ); button(ctx, in, qx + 114, qy, 38, 18, "max", maxQ > 1);
        UI::text(ctx, qx + 160, qy + 4, fmt("= %d g", unit * q), shopTab == 0 && unit * q > party.gold ? C::Red : C::Gold);
        bool can = maxQ > 0;
        button(ctx, in, qx, qy + 24, 150, 22, shopTab == 0 ? fmt("Buy %d", q) : fmt("Sell %d", q), can, can);
        if (shopTab == 0 && maxQ == 0) UI::text(ctx, qx, qy + 52, shopTab == 1 ? (cur->quest ? "A quest item - not for sale." : "") : shopStockLeft(shopId, cur->id, stock[curReal].second) == 0 ? "Sold out. Restocks every 3 days." : "You can't afford it.", C::Red);
    }
    int hx = keyHint(ctx, 20, SCREEN_H - 30, "Enter", shopTab == 0 ? "buy" : "sell"); hx = keyHint(ctx, hx, SCREEN_H - 30, "L/R", "quantity"); hx = keyHint(ctx, hx, SCREEN_H - 30, gKeys.label(A_INTERACT), shopTab == 0 ? "buy max" : "sell all");
    hx = keyHint(ctx, hx, SCREEN_H - 30, gPadGlyphs ? "LB/RB" : "PgUp/PgDn", shopTab == 0 ? "sell tab" : "buy tab"); keyHint(ctx, hx, SCREEN_H - 30, "Esc", "leave");
    { int kx = keyHint(ctx, 20, SCREEN_H - 44, gKeys.label(A_CATPREV) + "/" + gKeys.label(A_CATNEXT), "category"); keyHint(ctx, kx, SCREEN_H - 44, gKeys.label(A_SORT), "sort"); }
}

// ---------------- item details (1.1.3, layout 1.1.4) ----------------
// One look for an item's breakdown everywhere (pack, shop, equip picker): tags, a two-column grid of "label value"
// rows (gains green, losses red; equal resists to every damage type fold into "All resists"), then the description
// on its own, in a softer colour under a rule. Whatever does not fit above maxY is left out (the description before any stat).
// 1.1.4 (owner's #25): rows come in groups (weapon / defense + stat boosts / resists / use) and a group always starts
// on a new line, so boosts and resists never share one; every value in a column starts at the same x.
std::vector<std::pair<std::string, Color8>> Game::gearNotes(const ItemDef& d, const Character* who) const {
    std::vector<std::pair<std::string, Color8>> out; const Color8 INFO = RGB(170, 170, 190), BAD = RGB(235, 105, 95);
    auto classesWith = [&](bool weapon) { std::string s; for (auto& id : gd->classOrder) if (const ClassDef* c = gd->cls(id)) {
            bool ok = weapon ? c->trained(d.family) : (d.weight <= c->wears || std::find(d.wornby.begin(), d.wornby.end(), id) != d.wornby.end());
            if (ok) s += (s.empty() ? "" : ", ") + c->name; } return s; };
    if (d.type == IT_WEAPON && !d.family.empty()) { std::string c = classesWith(true); out.push_back({"Family: " + d.family + (c.empty() ? "" : " - trained: " + c), INFO}); }
    if (d.equippable() && d.type != IT_WEAPON && d.type != IT_ACCESSORY && d.weight > 0) { std::string c = classesWith(false); std::string wn = weightName(d.weight); wn[0] = (char)toupper((unsigned char)wn[0]); out.push_back({wn + " armour - worn well by: " + (c.empty() ? "nobody" : c), INFO}); }
    { std::string rq; for (int s = 0; s < ST_COUNT; s++) if (d.req[s] > 0) rq += (rq.empty() ? "" : ", ") + std::string(statName(s)) + " " + std::to_string(d.req[s]); if (!rq.empty()) out.push_back({"Needs " + rq, INFO}); }
    if (who && d.equippable()) { std::string wv = gearWarning(d, *who); if (!wv.empty()) out.push_back({who->name + ": " + wv, BAD}); }
    return out;
}
std::string Game::gearWarning(const ItemDef& d, const Character& who) const {
    std::string s;
    if (d.type == IT_WEAPON && !who.trainedWith(d, *gd)) s += "untrained: +1 AP, -10 to hit, no trait. ";
    if (d.type != IT_WEAPON && d.type != IT_ACCESSORY && !who.wearsWell(d, *gd)) {
        int best = 0; if (who.ranks.empty()) { if (auto* c = gd->cls(who.classId)) best = c->wears; } else for (auto& r : who.ranks) if (r.second > 0) if (auto* c = gd->cls(r.first)) best = std::max(best, c->wears);
        int st = std::max(1, d.weight - best);
        s += fmt("too heavy: -%d evasion, spells +%d%% MP, %d%% fail. ", 5 * st, 15 * st, 10 * st);
    }
    int sh = who.reqShort(d, *gd);
    if (sh > 0) s += d.type == IT_WEAPON ? fmt("%d stat point%s short: -%d to hit.", sh, sh == 1 ? "" : "s", std::min(30, sh * 4)) : fmt("%d stat point%s short: -%d evasion.", sh, sh == 1 ? "" : "s", std::min(30, sh * 3));
    return trim(s);
}

int Game::drawItemDetails(Canvas& cv, const ItemDef& d, int x, int y, int w, int maxY, const Character* who) {
    const Color8 UP = RGB(120, 215, 120), DOWN = RGB(235, 105, 95), LABEL = RGB(150, 150, 165);
    struct Row { std::string label, value; Color8 col; bool full; int group; };
    std::vector<Row> rows;
    int group = 0;
    auto num = [&](const std::string& label, int v, const char* suffix) { rows.push_back({label, fmt("%+d%s", v, suffix), v > 0 ? UP : DOWN, false, group}); };
    auto txt = [&](const std::string& label, const std::string& v, Color8 c, bool full) { rows.push_back({label, v, c, full, group}); };
    std::string tags;
    auto tag = [&](const char* t) { tags += std::string(tags.empty() ? "" : "  -  ") + t; };
    if (d.quest) tag("Quest item");
    if (d.unique) tag("Unique");
    if (d.twoHanded) tag("Two-handed");
    if (d.dmgMax) {
        txt("Damage", fmt("%d-%d %s", d.dmgMin, d.dmgMax, dmgTypeName(d.dmgType)), dmgColor(d.dmgType), true);
        txt("Range", std::to_string(d.range), C::White, false); txt("AP cost", std::to_string(d.ap), C::White, false);
        if (d.type == IT_WEAPON) txt("Scales", statName(d.scaleStat), C::White, false);
        if (d.accuracy) num("Accuracy", d.accuracy, "%");
        if (d.crit) num("Crit", d.crit, "%");
        if (!d.ammo.empty() && gd->item(d.ammo)) txt("Ammo", gd->item(d.ammo)->name, C::White, true);
    }
    group = 1;
    if (d.def) num("Defense", d.def, "");
    if (d.evasion) num("Evasion", d.evasion, "");
    for (int b = 0; b < ST_COUNT; b++) if (d.bonus[b]) num(statName(b), d.bonus[b], "");
    if (d.bonusHP) num("Max HP", d.bonusHP, "");
    if (d.bonusMP) num("Max MP", d.bonusMP, "");
    if (d.bonusAP) num("AP", d.bonusAP, "");
    if (d.bonusMove) num("Move", d.bonusMove, "");
    group = 2;
    {
        bool allSame = d.res[0] != 0; for (int r = 1; r < DT_COUNT; r++) if (d.res[r] != d.res[0]) allSame = false;
        if (allSame) num("All resists", d.res[0], "%");
        else for (int r = 0; r < DT_COUNT; r++) if (d.res[r]) { std::string n = dmgTypeName(r); if (!n.empty()) n[0] = (char)toupper((unsigned char)n[0]); num(n + " res.", d.res[r], "%"); }
    }
    group = 3;
    if (d.heal) txt("Heals", fmt("%d HP", d.heal), UP, false);
    if (d.mana) txt("Restores", fmt("%d MP", d.mana), UP, false);
    if (!d.effect.empty()) { std::string e = d.effect; for (auto& ch : e) if (ch == '_') ch = ' '; txt("Effect", e, C::Cream, true); }
    if (!d.useSpell.empty()) { const SpellDef* sp = gd->spell(d.useSpell); txt("Casts", sp ? sp->name : d.useSpell, C::Cream, true); }
    if (d.light) txt("Light", fmt("radius %d", d.light), C::White, false);
    const int lh = 13, colW = w / 2, gap = 8, groupGap = 3;
    int cy = y;
    if (!tags.empty() && cy + lh <= maxY) { UI::text(ctx, x, cy, tags, C::Gold); cy += lh + 2; }
    // Lay out: two columns, a group always starts a new line (a little apart from the one before), a full-width row (or
    // one too long for half the width) gets a line of its own. Each column's values start one gap after its widest label;
    // a row whose value would then run past its column moves to a line of its own and the layout is redone.
    struct Cell { int col, y; bool full; };
    std::vector<Cell> pos(rows.size());
    std::vector<bool> own(rows.size());
    for (size_t i = 0; i < rows.size(); i++) own[i] = rows[i].full || ctx.font->width(rows[i].label) + gap + ctx.font->width(rows[i].value) > colW - 6;
    int vx[2] = {0, 0}, endY = cy;
    for (int pass = 0; pass < 8; pass++) {
        int ly = cy, col = 0, lastGroup = -1;
        for (size_t i = 0; i < rows.size(); i++) {
            if (col == 1 && (own[i] || rows[i].group != lastGroup)) { col = 0; ly += lh; }
            if (col == 0 && lastGroup >= 0 && rows[i].group != lastGroup) ly += groupGap;
            pos[i] = {col, ly, (bool)own[i]}; lastGroup = rows[i].group;
            if (own[i]) ly += lh; else if (++col == 2) { col = 0; ly += lh; }
        }
        endY = col == 1 ? ly + lh : ly;
        vx[0] = vx[1] = 0;
        for (size_t i = 0; i < rows.size(); i++) if (!own[i]) vx[pos[i].col] = std::max(vx[pos[i].col], ctx.font->width(rows[i].label) + gap);
        bool moved = false;
        for (size_t i = 0; i < rows.size(); i++) if (!own[i] && vx[pos[i].col] + ctx.font->width(rows[i].value) > colW - 6) { own[i] = true; moved = true; break; }
        if (!moved) break;
    }
    for (size_t i = 0; i < rows.size(); i++) {
        const Row& r = rows[i]; const Cell& p = pos[i];
        if (p.y + lh > maxY) { endY = p.y; break; }
        int cx = x + p.col * colW;
        int off = p.full ? std::max(vx[0], ctx.font->width(r.label) + gap) : vx[p.col];
        UI::text(ctx, cx, p.y, r.label, LABEL);
        UI::text(ctx, cx + off, p.y, r.value, r.col);
    }
    cy = endY;
    {   // 1.4.0 (#67 #68): the weapon's family trait and any special effects, in plain words
        std::vector<std::string> fxl; if (!d.trait.empty()) fxl.push_back(traitDescribe(d.trait));
        for (auto& kv : d.fx) if (kv.first != "smoke") fxl.push_back(fxDescribe(kv.first, kv.second));
        if (!fxl.empty() && cy > y) cy += 3;
        for (auto& f : fxl) for (auto& l : ctx.font->wrap(f, w)) { if (cy + lh > maxY) break; UI::text(ctx, x, cy, l, C::Cyan); cy += lh; }
    }
    {   // 1.5.0 (#93): who uses it well, and what it costs the member it is for
        auto notes = gearNotes(d, who);
        if (!notes.empty() && cy > y) cy += 3;
        for (auto& n : notes) for (auto& l : ctx.font->wrap(n.first, w)) { if (cy + lh > maxY) break; UI::text(ctx, x, cy, l, n.second); cy += lh; }
    }
    if (!trim(d.desc).empty() && cy + lh <= maxY) {
        if (cy > y) { cy += 3; cv.fillRect(x, cy, w, 1, RGB(70, 70, 90)); cy += 5; }
        for (auto& l : ctx.font->wrap(d.desc, w)) { if (cy + lh > maxY) break; UI::text(ctx, x, cy, l, RGB(200, 195, 175)); cy += lh; }
    }
    return cy;
}

// ---------------- level up ----------------
// One screen per member with points to spend: pick the class that gets this level's rank (multiclassing, D&D style)
// and spread the stat points. Every row says what it does, and a preview shows the derived numbers before/after.
const char* statHelp(int s) {
    switch (s) {
        case ST_STR: return "Melee damage with STR weapons (+1 per 2 points over 8).";
        case ST_DEX: return "+1% to hit, +1 evasion, crits; +1 AP per 5, +1 Move per 3. Bows, guns, daggers.";
        case ST_CON: return "+3 max HP per point.";
        case ST_INT: return "+2 MP; arcane and tech spell damage (+1 per 2 over 8).";
        case ST_WIS: return "+2 MP; healing power (+1 per point over 8); holy spells.";
        case ST_CHA: return "Prices: buy 2% cheaper, sell 1% dearer per point (hero only).";
    }
    return "";
}
static const char* statShort(int s) {
    switch (s) {
        case ST_STR: return "Melee damage (STR weapons)";
        case ST_DEX: return "Hit, evasion, crit, AP, Move";
        case ST_CON: return "+3 max HP per point";
        case ST_INT: return "+2 MP, arcane/tech spell damage";
        case ST_WIS: return "+2 MP, healing power, holy spells";
        case ST_CHA: return "Better shop prices (hero)";
    }
    return "";
}
// learn what a new rank in a class teaches: its starting spells at rank 1, then its learn= list
void Game::applyRank(Character& m, const std::string& cls) {
    const ClassDef* c = gd->cls(cls); if (!c) return;
    int r = ++m.ranks[cls];
    std::vector<std::string> learn;
    if (r == 1) for (auto& sp : c->startSpells) learn.push_back(sp);
    for (auto& sl : c->spellLevels) if (sl.first == r) learn.push_back(sl.second);
    for (auto& id : learn) if (std::find(m.spells.begin(), m.spells.end(), id) == m.spells.end()) { m.spells.push_back(id); if (auto* sp = gd->spell(id)) log(m.name + " learned " + sp->name + "!"); }
}
std::string Game::rankPreview(const Character& m, const std::string& cls) const {
    const ClassDef* c = gd->cls(cls); if (!c) return "";
    int r = m.rank(cls) + 1;
    std::string s = fmt("%s rank %d: +%d HP, +%d MP", c->name.c_str(), r, c->hitDie, c->mpDie);
    std::vector<std::string> learn;
    if (r == 1) for (auto& sp : c->startSpells) learn.push_back(sp);
    for (auto& sl : c->spellLevels) if (sl.first == r) learn.push_back(sl.second);
    for (auto& id : learn) if (auto* sp = gd->spell(id)) { if (std::find(m.spells.begin(), m.spells.end(), id) == m.spells.end()) s += ". Learns " + sp->name + " - " + sp->desc; }
    // 1.4.0 (#71): the passive (rank 1) or perk this rank brings, and (#66) spells of this class that grow at this rank
    for (auto& pk : c->perks) if (pk.rank == r) s += std::string(". ") + (r == 1 ? "Passive" : "Perk") + ": " + pk.name + " - " + pk.desc;
    for (auto& kv : gd->spells) if (kv.second.cls == cls) for (auto& u : kv.second.upgrades) if (u.rank == r) {
        bool knows = std::find(m.spells.begin(), m.spells.end(), kv.first) != m.spells.end() || std::find(learn.begin(), learn.end(), kv.first) != learn.end();
        if (!knows) continue; std::string t = u.text; size_t dash = t.find(" - "); if (dash != std::string::npos && dash < 24) t = t.substr(dash + 3);
        s += ". " + kv.second.name + " grows: " + t; }
    { bool perkNow = false; for (auto& pk : c->perks) if (pk.rank == r) perkNow = true;
      if (!perkNow) for (auto& pk : c->perks) if (pk.rank > r) { s += fmt(". (Perk at %s rank %d: %s)", c->name.c_str(), pk.rank, pk.name.c_str()); break; } }
    if (learn.empty()) { int next = 0; for (auto& sl : c->spellLevels) if (sl.first > r && (next == 0 || sl.first < next)) next = sl.first; if (next) for (auto& sl : c->spellLevels) if (sl.first == next) if (auto* sp = gd->spell(sl.second)) { s += fmt(". (Next spell at rank %d: %s)", next, sp->name.c_str()); break; } }
    return s;
}
// Rite of Renewal / Draught of Forgetting: undo every stat point and class rank so they can be chosen again.
// Stats go back to the starting class's, all levels become unspent (3 stat points + 1 class rank each),
// class spells are forgotten (spells from other sources are kept) and are relearned through the new ranks.
void Game::respecMember(int mi) {
    if (mi < 0 || mi >= (int)party.members.size()) return;
    Character& m = party.members[mi];
    const ClassDef* sc = gd->cls(m.startClass.empty() ? m.classId : m.startClass); if (!sc) return;
    std::set<std::string> classSpells; for (auto& kv : gd->classes) { for (auto& sp : kv.second.startSpells) classSpells.insert(sp); for (auto& sl : kv.second.spellLevels) classSpells.insert(sl.second); }
    std::vector<std::string> keep; for (auto& sp : m.spells) if (!classSpells.count(sp)) keep.push_back(sp);
    for (int s = 0; s < ST_COUNT; s++) m.baseStats[s] = sc->stats[s];
    m.spells = keep; m.ranks.clear();
    applyRank(m, sc->id);                          // level 1 is always the starting class
    m.statPoints = 3 * (m.level - 1); m.rankPoints = m.level - 1;
    for (int f = 0; f < 8; f++) if (startsWith(m.favs[f], "spell:")) m.favs[f].clear();
    m.hp = std::min(m.hp, m.maxHP(*gd)); m.mp = std::min(m.mp, m.maxMP(*gd));
    if (std::find(levelUpQueue.begin(), levelUpQueue.end(), mi) == levelUpQueue.end() && (m.statPoints > 0 || m.rankPoints > 0)) levelUpQueue.push_back(mi);
    log(m.name + " lets go of old habits. Choose again.");
}
void Game::updateLevelUp(InputState& in, float dt) {
    (void)dt;
    if (levelUpQueue.empty()) { popMode(); return; }
    Character& m = party.members[levelUpQueue[0]];
    const auto& classes = gd->classOrder; int nc = (int)classes.size();
    int total = 0; for (int s = 0; s < ST_COUNT; s++) total += statAlloc[s];
    bool needRank = m.rankPoints > 0;
    int ncr = needRank ? nc : 0;                         // class rows exist only while a rank is waiting
    int rows = ncr + ST_COUNT + 1;                       // + confirm
    if (luRow >= rows) luRow = rows - 1;
    if (actP(in, A_UP)) luRow = (luRow + rows - 1) % rows;
    if (actP(in, A_DOWN)) luRow = (luRow + 1) % rows;
    int x = 20, y = menuHostedNow ? -6 : 16;   // 1.8.5: hosted, the title is in the window's title bar
    // mouse
    for (int i = 0; i < ncr; i++) { int ry = y + 74 + i * 18; if (in.mouseX >= x + 10 && in.mouseX < x + 290 && in.mouseY >= ry && in.mouseY < ry + 18) { if (in.mouseDX || in.mouseDY) luRow = i; if (in.mousePressed[0]) { luRow = i; luClass = i; } } }
    for (int s = 0; s < ST_COUNT; s++) { int ry = y + 74 + s * 34; if (in.mouseX >= x + 310 && in.mouseX < x + 590 && in.mouseY >= ry && in.mouseY < ry + 32) { if (in.mouseDX || in.mouseDY) luRow = ncr + s; if (in.mousePressed[0]) { luRow = ncr + s; if (in.mouseX >= x + 522 && in.mouseX < x + 546 && statAlloc[s] > 0) statAlloc[s]--; else if (in.mouseX >= x + 552 && in.mouseX < x + 576 && total < m.statPoints) statAlloc[s]++; } } }
    total = 0; for (int s = 0; s < ST_COUNT; s++) total += statAlloc[s];
    bool rankOk = !needRank || luClass >= 0;
    bool ready = total == m.statPoints && rankOk;
    bool confirmBtn = in.mousePressed[0] && in.mouseX >= x + 230 && in.mouseX < x + 370 && in.mouseY >= y + 412 && in.mouseY < y + 436;
    if (luRow < ncr) { if (actP(in, A_RIGHT) || actP(in, A_CONFIRM)) { luClass = luRow; if (actP(in, A_CONFIRM)) luRow = ncr; return; } if (actP(in, A_LEFT) && luClass == luRow) luClass = -1; }
    else if (luRow < ncr + ST_COUNT) { int s = luRow - ncr; if (actP(in, A_RIGHT) && total < m.statPoints) statAlloc[s]++; if (actP(in, A_LEFT) && statAlloc[s] > 0) statAlloc[s]--; if (actP(in, A_CONFIRM) && !ready) { if (total < m.statPoints) statAlloc[s]++; return; } }
    if ((actP(in, A_CONFIRM) || confirmBtn) && ready) {
        for (int s = 0; s < ST_COUNT; s++) { m.baseStats[s] += statAlloc[s]; statAlloc[s] = 0; }
        m.statPoints = 0;
        if (needRank && luClass >= 0 && luClass < nc) { applyRank(m, classes[luClass]); m.rankPoints--; }
        m.hp = std::min(m.maxHP(*gd), m.hp + 5); m.mp = m.maxMP(*gd);
        luClass = -1; luRow = 0;
        if (m.rankPoints <= 0 && m.statPoints <= 0) levelUpQueue.erase(levelUpQueue.begin());
        if (levelUpQueue.empty()) popMode();
        return;
    }
    if ((actP(in, A_CONFIRM) || confirmBtn) && !ready) { log(!rankOk ? "Choose a class for the new rank first." : fmt("Spend all %d stat points first.", m.statPoints)); gAudio.play("ui_error"); }
}
void Game::drawLevelUp(Canvas& cv, InputState& in) {
    if (levelUpQueue.empty()) return;
    Character& m = party.members[levelUpQueue[0]];
    uiRect(0, 0, SCREEN_W, SCREEN_H);
    int x = 20, y = menuHostedNow ? -6 : 16, w = 600, h = 446;
    if (!menuHostedNow) { cv.dim(0, 0, SCREEN_W, SCREEN_H, 140); panelTitle(ctx, x, y, w, h, "Level Up!"); }
    else hostTitle(x + 300 - 4, y + 22, "Level Up!");   // frame: no title bar, so the title sits at the top right
    const auto& classes = gd->classOrder; int nc = (int)classes.size();
    bool needRank = m.rankPoints > 0; int ncr = needRank ? nc : 0;
    int total = 0; for (int s = 0; s < ST_COUNT; s++) total += statAlloc[s];
    if (m.recolor) ctx.ts->drawRecolor(cv, m.tile, x + 14, y + 22, 3, m.color); else ctx.ts->draw(cv, m.tile, x + 14, y + 22, 3);
    ctx.font->bold = true; UI::text(ctx, x + 56, y + 24, fmt("%s reaches level %d", m.name.c_str(), m.level), C::Gold); ctx.font->bold = false;
    UI::text(ctx, x + 56, y + 40, m.classSummary(*gd), C::LtGray);
    std::string todo; if (needRank) todo = luClass >= 0 ? "Class rank chosen. " : "Pick a class for the new rank. ";
    todo += m.statPoints - total > 0 ? fmt("%d stat point%s left.", m.statPoints - total, m.statPoints - total == 1 ? "" : "s") : "All stat points placed.";
    UI::text(ctx, x + 56, y + 54, todo, C::Cyan);
    // ---- class ranks ----
    UI::text(ctx, x + 12, y + 62 - 2 + (needRank ? 0 : 0), needRank ? "" : "", C::Gold);
    if (needRank) {
        for (int i = 0; i < nc; i++) {
            const ClassDef* c = gd->cls(classes[i]); if (!c) continue;
            int ry = y + 74 + i * 18; bool sel = luRow == i, chosen = luClass == i;
            if (sel) { cv.fillRect(x + 10, ry, 280, 17, RGB(90, 70, 20)); UI::selCursor(ctx, x + 10, ry, 17); }
            ctx.ts->draw(cv, T(c->spriteCol, c->spriteRow), x + 16, ry + 2, 1);
            UI::text(ctx, x + 34, ry + 3, c->name, chosen ? C::Green : sel ? C::Yellow : C::White);
            int r = m.rank(classes[i]);
            textRight(ctx, x + 284, ry + 3, chosen ? fmt("rank %d -> %d", r, r + 1) : r ? fmt("rank %d", r) : "-", chosen ? C::Green : C::Gray);
        }
        int ci = luRow < nc ? luRow : (luClass >= 0 ? luClass : -1);
        if (ci >= 0) {
            const ClassDef* c = gd->cls(classes[ci]);
            int by = y + 74 + nc * 18 + 6;
            cv.fillRect(x + 10, by, 280, 120, RGB(24, 24, 36)); cv.rect(x + 10, by, 280, 120, C::DkGray);
            int cy = by + 4;
            if (c && !c->role.empty()) cy += wrapped(ctx, x + 16, cy, 268, c->role, C::LtGray) + 4;
            wrapped(ctx, x + 16, cy, 268, rankPreview(m, classes[ci]), C::Cream);
        }
    } else {
        UI::text(ctx, x + 12, y + 76, "Class ranks", C::Gold);
        wrapped(ctx, x + 12, y + 92, 276, "Every level also gives one class rank. This level's rank has been chosen; spend the remaining stat points on the right.", C::Gray);
    }
    // ---- stats ----
    for (int s = 0; s < ST_COUNT; s++) {
        int ry = y + 74 + s * 34; bool sel = luRow == ncr + s;
        if (sel) { cv.fillRect(x + 308, ry, 282, 32, RGB(90, 70, 20)); UI::selCursor(ctx, x + 308, ry, 32); }
        ctx.font->bold = true; UI::text(ctx, x + 316, ry + 3, statName(s), sel ? C::Yellow : C::White); ctx.font->bold = false;
        UI::text(ctx, x + 356, ry + 3, fmt("%d", m.baseStats[s] + statAlloc[s]), statAlloc[s] ? C::Green : C::LtGray);
        if (statAlloc[s]) UI::text(ctx, x + 380, ry + 3, fmt("(+%d)", statAlloc[s]), C::Green);
        int eq = m.stat(s, *gd) - m.baseStats[s]; if (eq) UI::text(ctx, x + 420, ry + 3, fmt("gear %+d", eq), C::Cyan);
        button(ctx, in, x + 522, ry + 1, 24, 15, "-", statAlloc[s] > 0); button(ctx, in, x + 552, ry + 1, 24, 15, "+", total < m.statPoints);
        ctx.font->draw(cv, *ctx.ts, x + 316, ry + 18, statShort(s), C::Gray);
    }
    // ---- preview ----
    Character after = m; for (int s = 0; s < ST_COUNT; s++) after.baseStats[s] += statAlloc[s];
    if (needRank && luClass >= 0 && luClass < nc) after.ranks[classes[luClass]]++;
    int py0 = y + 290; cv.fillRect(x + 308, py0, 282, 112, RGB(24, 24, 36)); cv.rect(x + 308, py0, 282, 112, C::DkGray);
    UI::text(ctx, x + 316, py0 + 4, "Preview", C::Gold);
    auto row = [&](int i, const std::string& label, int a, int b) { int col = i % 2, r = i / 2; int rx = x + 316 + col * 138, ry = py0 + 20 + r * 14; UI::text(ctx, rx, ry, label, C::Gray); UI::text(ctx, rx + 64, ry, a == b ? fmt("%d", a) : fmt("%d > %d", a, b), a == b ? C::White : (b > a ? C::Green : C::Red)); };
    int lo0, hi0, t0, lo1, hi1, t1; m.attackDamage(*gd, lo0, hi0, t0); after.attackDamage(*gd, lo1, hi1, t1);
    row(0, "Max HP", m.maxHP(*gd), after.maxHP(*gd)); row(1, "Max MP", m.maxMP(*gd), after.maxMP(*gd));
    row(2, "AP", m.maxAP(*gd), after.maxAP(*gd)); row(3, "Move", m.maxMove(*gd), after.maxMove(*gd));
    row(4, "Evasion", m.evasion(*gd), after.evasion(*gd)); row(5, "To-hit", m.stat(ST_DEX, *gd) * 2, after.stat(ST_DEX, *gd) * 2);
    row(6, "Max dmg", hi0, hi1); row(7, "Spell+", (m.stat(ST_INT, *gd) - 8) / 2, (after.stat(ST_INT, *gd) - 8) / 2);
    row(8, "Heal+", m.stat(ST_WIS, *gd) - 8, after.stat(ST_WIS, *gd) - 8); row(9, "Crit %", 5 + m.stat(ST_DEX, *gd) / 2, 5 + after.stat(ST_DEX, *gd) / 2);
    bool ready = total == m.statPoints && (!needRank || luClass >= 0);
    bool selC = luRow == ncr + ST_COUNT;
    button(ctx, in, x + 230, y + 412, 140, 24, "Confirm", ready, selC);
    { int hx = keyHint(ctx, x + 12, y + 418, "Up/Down", "row"); keyHint(ctx, hx, y + 418, "L/R", "choose / +-"); }
}

// name entry is handled by the desktop "New Game" dialog; these remain for completeness
// 1.4.0 (#76/#77): the text box a script's input statement opens (terminal passwords, combination locks). Keyboard:
// type, Backspace (Ctrl = clear), Enter, Esc. Pad: up/down change the last character, right adds one, left deletes,
// A enters, B cancels.
void Game::updateName(InputState& in, float dt) {
    (void)dt;
    for (char ch : in.typed) if (ch >= 32 && ch < 127 && (int)inputBuf.size() < inputMax) { inputBuf += ch; gAudio.play("text_key"); }
    if (in.pressed[K_BACKSPACE] && !inputBuf.empty()) { if (in.ctrl()) inputBuf.clear(); else inputBuf.pop_back(); gAudio.play("text_key"); }
    if (in.gamepadActive) {
        static const std::string CH = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
        if (in.pressed[K_PAD_RIGHT] && (int)inputBuf.size() < inputMax) { inputBuf += '0'; gAudio.play("text_key"); }
        if (in.pressed[K_PAD_LEFT] && !inputBuf.empty()) { inputBuf.pop_back(); gAudio.play("text_key"); }
        if ((in.pressed[K_PAD_UP] || in.pressed[K_PAD_DOWN]) ) {
            if (inputBuf.empty()) inputBuf += '0';
            char& c = inputBuf.back(); size_t i = CH.find((char)toupper((unsigned char)c)); if (i == std::string::npos) i = 0;
            i = (i + (in.pressed[K_PAD_UP] ? 1 : CH.size() - 1)) % CH.size(); c = CH[i]; gAudio.play("text_key");
        }
        if (in.pressed[K_PAD_A]) { textInputDone(true); in.eatKeys(); return; }
        if (in.pressed[K_PAD_B]) { textInputDone(false); in.eatKeys(); return; }
    }
    if (in.pressed[K_ENTER]) { textInputDone(true); in.eatKeys(); return; }
    if (in.pressed[K_ESCAPE]) { textInputDone(false); in.eatKeys(); return; }
    if (in.mousePressed[2]) { textInputDone(false); return; }
}
void Game::drawName(Canvas& cv, InputState& in) {
    (void)in;
    int ts = textScale(), bw = ts == 2 ? 560 : 400, lh = 12 * ts + 1;
    auto lines = ctx.font->wrap(inputPrompt.empty() ? std::string("Enter the code:") : inputPrompt, bw - 40, ts);
    int h = 30 + (int)lines.size() * lh + 12 + (16 * ts + 8) + 30;
    int x = (SCREEN_W - bw) / 2, y = std::max(4, (SCREEN_H - 72 - h) / 2);
    cv.dim(0, 0, SCREEN_W, SCREEN_H, 90); uiRect(0, 0, SCREEN_W, SCREEN_H);
    panelTitle(ctx, x, y, bw, h, inputTitle.empty() ? std::string("Enter") : inputTitle);
    int cy = y + 24; for (auto& l : lines) { UI::text(ctx, x + 20, cy, l, C::White, ts); cy += lh; }
    cy += 8;
    int fh = 16 * ts + 4; cv.fillRect(x + 20, cy, bw - 40, fh, RGB(10, 14, 10)); cv.rect(x + 20, cy, bw - 40, fh, C::Gray);
    std::string shown = inputBuf; bool caret = (ctx.frame / 20) % 2 == 0;
    UI::text(ctx, x + 26, cy + 3, shown + (caret ? "_" : " "), RGB(120, 255, 140), ts);
    textCenter(ctx, SCREEN_W / 2, y + h - 18, gPadGlyphs ? "D-pad: letters   A: enter   B: cancel" : "[Enter] OK    [Esc] Cancel", C::Gray);
}
