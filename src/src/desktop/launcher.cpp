// 1.6.1 (#134): the Triffall launcher - the game's own window on the fake desktop, opened by the "Triffall" icon that the
// first-run installer puts there. It is the hub for everything about the game: continue / start / load an adventure
// (each campaign keeps its own saves, #135), How to Play, Achievements, settings, the Editor and the credits.
// 1.6.2 (#137): the Field Notes and Files pages were removed (little used); what you learn about monsters is in the
// Journal's Bestiary, and the documents that hold terminal passwords are found in the world (signs, data disks). Retro window outside, the game's own night-and-rift look inside.
#include "desktop.h"
#include "../core/theme.h"
#include "../core/audio.h"
#include "../core/version.h"
#include "../core/updater.h"
#include "../game/world.h"
#include <algorithm>
#include <cmath>
#include <ctime>
#include <filesystem>

namespace {
// 1.8.3 (#175 #179 #183): the launcher's colours follow the UI theme (they were a fixed night-and-rift violet)
Color8 L_BG, L_PANEL, L_EDGE, L_TEXT, L_DIM, L_AMBER, L_VIOLET, L_SEL, L_HOVER, L_GOOD, L_SELTEXT, L_LIST;
void launcherColors() {
    const UiTheme& t = Theme::th();
    if (Theme::native()) { L_BG = t.face; L_PANEL = t.face; L_EDGE = RGB(128, 128, 128); L_TEXT = RGB(0, 0, 0); L_DIM = RGB(128, 128, 128); L_AMBER = RGB(0, 0, 128); L_VIOLET = RGB(0, 0, 128); L_SEL = RGB(0, 0, 128); L_HOVER = RGB(216, 216, 232); L_GOOD = RGB(0, 100, 0); L_SELTEXT = RGB(255, 255, 255); L_LIST = RGB(255, 255, 255); }
    else if (Theme::onebit()) { L_BG = t.back; L_PANEL = t.back; L_EDGE = t.text; L_TEXT = t.text; L_DIM = t.dim; L_AMBER = t.accent; L_VIOLET = t.mp; L_SEL = RGB(36, 35, 42); L_HOVER = RGB(36, 35, 42); L_GOOD = t.good; L_SELTEXT = t.accent; L_LIST = t.back; }
    else { L_BG = t.back; L_PANEL = t.panel; L_EDGE = t.hi; L_TEXT = t.text; L_DIM = t.dim; L_AMBER = t.accent; L_VIOLET = t.move; L_SEL = t.rowSel; L_HOVER = RGB(69, 75, 99); L_GOOD = t.good; L_SELTEXT = t.accent; L_LIST = t.well; }
}
const char* PAGE_NAMES[] = {"Play", "Saves", "How to Play", "Achievements", "About"};
const char* PAGE_ACTIONS[] = {"launcher", "saves", "readme", "achievements", "about"};
}

// The old Readme.txt, now the launcher's How to Play page
static const std::vector<std::string>& howToPlayLines() {
    static const std::vector<std::string> lines = {
        "HOW TO PLAY - THE FIRST TEN MINUTES", "  New to Triffall? Pick the Tutorial on the Play page first: a short", "  adventure with old Wick that teaches everything below by doing it.", "",
        "  1. Pick Triffall (the main story) and press New Game. Name your", "     hero, pick a class. (You can add ranks in any class later, so", "     don't agonise.)",
        "  2. Walk out of the cottage and talk to Elder Mara at the north-west", "     edge of the village. Your journal (J) always says where to go", "     next; M shows it on the world map.",
        "  3. Recruit Bram at the inn. Buy torches and potions - caves are dark.", "  4. When you meet monsters the fight happens right where you stand.",
        "     Move, attack, end turn. Stand next to walls for cover; hit things", "     from two sides to flank them. Fleeing is allowed and often smart.",
        "  5. Save at shrines (in towns and in most dungeons). The game", "     also autosaves whenever you reach a new place, so a bad fight", "     never costs more than a few minutes. Continue on the Play page", "     picks up from your newest save.",
        "  6. Talk to everyone. Most towns have three or four people who need", "     help, and a few choices in the story change how the world ends.",
        "  Stuck? Settings > Access has large text, slower or instant text,", "  colour-blind friendly colours and no flashing. Settings > Controls", "  rebinds everything, keyboard or gamepad.", "",
        "SAVES", "  Every adventure (the main story, the Tutorial, every campaign you", "  add or make) keeps its own three save slots and its own autosave.", "  The Saves page shows the ones for the adventure picked on Play.", "",
        "EXPLORING (defaults - rebind in Settings > Controls)", "  Arrow keys / WASD or left stick ........ move", "  E / Enter / A button .................... talk, open, interact", "  Tab / Start button ....................... party, items, spells, quests", "  B / Q ..................................... spell menu / item menu", "  1-8 ....................................... favorites hotbar", "  Esc / B button ........................... pause (save, load, settings)", "  F5 ....................................... quick save (on a save shrine)", "  Mouse: click a neighbouring tile to step or interact.", "",
        "YOUR CHARACTER", "  Every level gives 3 stat points and one CLASS RANK. Put the rank", "  in any class (multiclassing): each rank adds that class's HP and", "  MP and teaches its spells at the rank listed in classes.txt.", "  STR  melee damage with STR weapons (+1 per 2 points over 8)", "  DEX  +2% to hit, +1 evasion, crits; +1 AP per 5, +1 Move per 3;", "       damage with bows, guns, daggers and rapiers", "  CON  +3 max HP per point", "  INT  +2 MP; arcane/tech spell damage (+1 per 2 over 8)", "  WIS  +2 MP; healing power (+1 per point over 8)", "  CHA  shop prices (the hero's CHA counts)", "  The level-up screen previews HP, MP, AP, Move, damage and more", "  before you confirm. Changed your mind? Brother Caul (Brasswick)", "  and Abbot Tyr (Frosthold) perform the Rite of Renewal for gold,", "  and a Draught of Forgetting does it anywhere.", "",
        "COMBAT (turn based, fought in place)", "  Each character has AP (actions) and MV (tiles of movement).", "  Move: pick a highlighted tile (arrows or mouse) and confirm.", "  Attack: pick an enemy in range; hit chance is shown.", "  A toggles normal / aimed attack. S spells, Q items, D defend,", "  F flee, T end turn, R action menu, C center camera, 1-8 favorites.", "  The box in the top-left describes the tile under the cursor.", "  AREA SPELLS AND BOMBS HIT EVERYONE in the blast - you too.", "  The preview flashes any ally (or you) who would be caught.", "  Hitting a monster with an element shows WEAK / resists / immune", "  and the info box remembers it. Weapon oils change your element.", "  Other monsters hear a fight and join it if they're close by.", "  Drinking or applying an item costs 4 AP; throwing one costs 3.", "",
        "DARKNESS", "  Caves, crypts and ruins are dark. Your party only lights a few", "  tiles around itself; past that you see the shape of the room but", "  not who stands in it, and you can't aim at what you can't see.", "  Attacks into shadow are 25% harder unless you see in the dark", "  (undead, machines and many cave dwellers do). Torches (use them", "  from the Items menu), the Light spell, flares, burning foes,", "  braziers and wall torches all push the dark back.", "",
        "CRAFTING", "  Smiths (forge), alchemists (alchemy) and tinkers (workbench) in", "  towns open a crafting screen; field recipes work from the Items", "  menu (C). Each station has a skill that grows as you use it.", "  Recipes have a difficulty: above your skill they can fail and", "  ruin half the materials; well below it they can come out fine or", "  masterwork. Forges and workbenches also Reinforce gear (+1 to +3)", "  and Salvage it for parts; alchemy benches Enchant it with rift", "  dust. Boss trophies unlock masterwork recipes; formulas teach more.", "",
        "THE WORLD", "  Rubble, vines, ice and boulders block some paths: dig with a", "  shovel, blast with a charge (light it and RUN) or a fire spell,", "  burn, melt or shove. Levers open doors and drain water. Glinting", "  spots are ore veins and herb patches - they grow back in a few days.", "  Notice boards post three bounties a day; take up to three at once.", "  Shops sell stacks: Left/Right sets the quantity, E buys the most.", "  Weapons and armour are in limited stock and restock every 3 days.", "",
        "SNEAKING AND NOISE", "  Not every fight has to happen. Throw Rattle Stones or a bottle", "  (Items > Throw..., or the hotbar) where monsters can hear it:", "  they go to look and barely notice you while they do. Lead them", "  over a trap and they may never reach you. In the dark, with no", "  light, people and beasts that can't see in the dark notice you", "  later - the dead and machines see you anyway.", "",
        "WHAT YOU LEARN", "  What you find out about a monster - what hurts it, what it", "  casts - goes into the Journal's Bestiary, for every save.", "  Lost a fight? Reload: you still know what you learned.", "  Hesk's Barrow and Greywood Cave hide places marked as deadly:", "  come back with the right weapon or element. There is always a", "  save shrine close by.", "",
        "TERMINALS", "  Old machines in the world ask for passwords. Notes, logs and", "  data disks nearby hold the clues; what you read is kept in", "  the Journal, under Notes.", "",
        "FAVORITES", "  In the Spells or Items menu, highlight something and press 1-8.", "  Utility spells (Light, Knock, Recall, Sense Treasure, Camouflage,", "  healing) can be cast from the Spells menu outside combat.", "",
        "EDITOR", "  Edits maps (the overworld and every dungeon are plain map files", "  you can paint), tile kinds, items, monsters, spells, classes, shops,", "  quests, dungeons, recipes and dialogue. The Campaign button picks", "  which campaign you edit or creates a new one; new campaigns show", "  up on the Play page (Delete... there removes one you no longer", "  want; the main story and the Tutorial always stay).", "",
        "THE DESKTOP", "  Double-click an icon to open it. Right-click it (or E / pad X)", "  to change its picture. Drag icons anywhere: the desktop keeps", "  their places, pictures and the wallpaper for next time.",
    };
    return lines;
}

void Desktop::openLauncher(int page, const std::string& campaign) {
    if (!campaign.empty()) launchCampaign = campaign;
    { bool known = false; for (auto& c : listCampaigns(assetDir)) if (c.id == launchCampaign) known = true; if (!known) launchCampaign = "triffall"; }
    bool had = false; for (auto& w : windows) if (w.type == "launcher") had = true;
    openWindow("launcher");
    DeskWindow& w = windows.back();
    if (page >= 0 && w.page != page) { w.page = page; w.scroll = 0; w.pick = 0; }
    else if (!had) w.page = LP_PLAY;
    slotsCampaign = launchCampaign; refreshSlots();
}

int Desktop::newestSlot() const {
    int best = -1; long long bestT = -1;
    for (int i = 0; i < 4; i++) if (slots[i].exists) {
        long long t = slots[i].saved;
        if (t <= 0) { std::error_code ec; auto ft = std::filesystem::last_write_time(slotPath(i), ec); if (!ec) t = (long long)std::chrono::duration_cast<std::chrono::seconds>(ft.time_since_epoch()).count(); }
        if (t > bestT) { bestT = t; best = i; }
    }
    return best;
}

bool Desktop::launchButton(Canvas& cv, InputState& in, int x, int y, int w, int h, const std::string& label, bool enabled, bool primary, bool active) {
    bool hover = active && enabled && !mouseUsed && in.mouseX >= x && in.mouseY >= y && in.mouseX < x + w && in.mouseY < y + h;
    if (active && enabled) hot(x, y, w, h);
    Theme::Pen p{&cv, ctx.font, ctx.ts};
    Theme::button(p, x, y, w, h, label, !enabled ? Theme::BS_DISABLED : primary ? (Theme::native() ? Theme::BS_DEFAULT : Theme::BS_SELECTED) : Theme::BS_NORMAL, hover);
    if (hover && (in.gamepadActive || padNavWindows)) { if (Theme::onebit()) Theme::glow(cv, x, y, w, h); else Theme::frameRect(cv, x + 2, y + 2, w - 4, h - 4, Theme::native() ? RGB(0, 0, 0) : Theme::th().accentHi); }
    bool clicked = hover && in.mousePressed[0];
    if (clicked) { mouseUsed = true; gAudio.play("desktop_click"); }
    return clicked;
}

// a box of text lines with a scroll bar; the wheel scrolls it while the pointer is over it, PgUp/PgDn (updateWindow) too
void Desktop::launchScrollText(Canvas& cv, InputState& in, DeskWindow& w, int x, int y, int bw, int bh, const std::vector<std::string>& lines, bool active) {
    Font& f = *ctx.font;
    cv.fillRect(x, y, bw, bh, L_LIST); cv.rect(x, y, bw, bh, L_EDGE);
    int maxShow = std::max(1, (bh - 10) / 13), n = (int)lines.size(), maxScroll = std::max(0, n - maxShow);
    if (active && in.wheel && in.mouseX >= x && in.mouseX < x + bw && in.mouseY >= y && in.mouseY < y + bh) w.scroll -= in.wheel * 3;
    w.scroll = clampi(w.scroll, 0, maxScroll);
    for (int i = 0; i < maxShow && w.scroll + i < n; i++) {
        const std::string& l = lines[w.scroll + i];
        bool head = !l.empty() && l[0] != ' ' && l == toUpper(l) && l.size() > 3;   // section titles in capitals
        f.draw(cv, *ctx.ts, x + 8, y + 5 + i * 13, l, head ? L_AMBER : L_TEXT);
    }
    if (maxScroll > 0) {
        int sx = x + bw - 8, sy = y + 3, sh = bh - 6, th = std::max(14, sh * maxShow / n), ty = sy + (sh - th) * w.scroll / maxScroll;
        cv.fillRect(sx, sy, 5, sh, L_HOVER); cv.fillRect(sx, ty, 5, th, L_EDGE);
    }
}

// the banner: a night sky over Aldmoor with the Rift torn through it, the five heroes on the ridge, the logo
void Desktop::drawLauncherBanner(Canvas& cv, int x, int y, int bw, int bh, Rect clip) {
    if (clip.w <= 0) clip = {x, y, bw, bh};
    cv.setClip(clip);
    for (int r = 0; r < bh; r++) {
        float k = (float)r / bh;
        cv.hline(x, y + r, bw, RGB((uint8_t)(10 + 40 * k), (uint8_t)(8 + 12 * k), (uint8_t)(30 + 46 * k)));
    }
    for (int i = 0; i < 70; i++) {   // stars (fixed places, a slow twinkle)
        uint32_t h = (uint32_t)(i * 2654435761u + 12345u);
        int sx = x + (int)(h % (uint32_t)bw), sy = y + (int)((h >> 11) % (uint32_t)std::max(1, bh - 30));
        float tw = 0.5f + 0.5f * sinf(ctx.time * (0.7f + (h % 7) * 0.3f) + (float)(h % 100));
        uint8_t b = (uint8_t)(110 + 120 * tw);
        cv.put(sx, sy, RGB(b, b, (uint8_t)std::min(255, b + 20)));
    }
    // the Rift: a jagged violet tear with a glow, gently shimmering
    int rx = x + bw * 70 / 100;
    std::vector<std::array<int, 2>> pts;
    for (int k = 0; k <= 8; k++) { int py = y + 4 + k * (bh - 26) / 8; int off = (int)(sinf(k * 2.1f + 1.3f) * 9 + sinf(ctx.time * 1.7f + k) * 1.5f); pts.push_back({rx + off, py}); }
    for (int g = 7; g >= 0; g -= 1) {
        Color8 c = g > 4 ? RGB(110, 50, 200, 40) : g > 1 ? RGB(170, 90, 255, 90) : g == 1 ? RGB(220, 170, 255) : RGB(255, 245, 255);
        for (size_t k = 0; k + 1 < pts.size(); k++) for (int d = -g; d <= g; d++) cv.line(pts[k][0] + d, pts[k][1], pts[k + 1][0] + d, pts[k + 1][1], c);
    }
    // the ridge and the trees on it (pictures from the tileset, drawn as silhouettes)
    auto ridge = [&](int px) { float t = (float)(px - x); return y + bh - 22 + (int)(sinf(t * 0.021f) * 6 + sinf(t * 0.057f + 1.0f) * 3); };
    for (int px = x; px < x + bw; px++) cv.vline(px, ridge(px), y + bh - ridge(px), RGB(8, 6, 16));
    Tile tree; if (const TileKind* k = tileKind("pine_t")) tree = k->prop.valid() ? k->prop : !k->ground.empty() ? k->ground[0] : Tile();
    if (tree.valid()) for (int tx = x + 150; tx < x + bw - 10; tx += 23) { if (abs(tx - rx) < 30) continue; ctx.ts->drawRecolor(cv, tree, tx, ridge(tx + 12) - 22, 2, RGB(14, 10, 26)); }
    // the five heroes, lit violet by the Rift
    if (ctx.gd) { int hx = x + bw * 70 / 100 - 140; int n = 0;
        for (auto& id : ctx.gd->classOrder) { const ClassDef* c = ctx.gd->cls(id); if (!c || n >= 5) continue;
            int px = hx + n * 26 - 10; ctx.ts->drawRecolor(cv, T(c->spriteCol, c->spriteRow), px, ridge(px + 12) - 22, 2, RGB(150, 118, 210)); n++; } }
    // the logo (the game's own letters) and the version
    { Font& f = *ctx.font; int keep = f.style; f.style = Font::STYLE_CLASSIC; f.bold = true;
      f.draw(cv, *ctx.ts, x + 15, y + 15, "TRIFFALL", RGB(90, 50, 0), 3); f.draw(cv, *ctx.ts, x + 12, y + 12, "TRIFFALL", RGB(255, 205, 90), 3);   // the banner keeps its own colours in every theme
      f.bold = false; f.style = keep;
      f.draw(cv, *ctx.ts, x + 14, y + 52, "an old-school RPG", RGB(210, 196, 160));
      f.draw(cv, *ctx.ts, x + bw - 8 - f.width(TRIFFALL_VERSION_LABEL), y + bh - 15, TRIFFALL_VERSION_LABEL, RGB(140, 134, 162)); }
    cv.resetClip();
}

void Desktop::drawLauncher(Canvas& cv, DeskWindow& w, InputState& in, bool active) {
    Font& f = *ctx.font;
    launcherColors();
    Theme::NoMap nm; const UiTheme& th = Theme::th(); Theme::Pen pen{&cv, ctx.font, ctx.ts};
    // the inside: below the 90s title bar (frame, native) or the one-bit striped bar
    const bool one = Theme::onebit(), nat = Theme::native();
    int ix = one ? w.x + 1 : w.x + 4, iy = one ? w.y + 22 : w.y + 24, iw = one ? w.w - 2 : w.w - 8, ih = one ? w.h - 23 : w.h - 28;
    cv.fillRect(ix, iy, iw, ih, L_BG);
    if (w.page == LP_PLAY) { drawLauncherHome(cv, w, in, active, {ix, iy, iw, ih}); return; }
    // ---- another page: a Back button and the page's title, the page below ----
    if (launchButton(cv, in, ix + 8, iy + 6, 70, 22, "< Back", true, false, active)) { w.page = LP_PLAY; w.scroll = 0; w.pick = 0; return; }
    int ny = iy + 34, ch = iy + ih - 6 - ny;
    int nx = ix + 2, nw = -6;   // (the old page list is gone: the page uses the whole width)
    (void)th; (void)pen; (void)nat;
    // ---- the page (right) ----
    int cx = ix + 8, cy = ny, cw = iw - 16; (void)nx; (void)nw;
    auto camps = listCampaigns(assetDir);
    int ci = 0; for (size_t i = 0; i < camps.size(); i++) if (camps[i].id == launchCampaign) ci = (int)i;
    const CampaignInfo* cur = camps.empty() ? nullptr : &camps[ci];
    auto pickCampaign = [&](const std::string& id) { if (id == launchCampaign) return; launchCampaign = id; slotsCampaign = id; refreshSlots(); w.pick = 0; saveDesktopSettings(); };
    auto heading = [&](int hx, int hy, const std::string& t) { f.bold = true; f.draw(cv, *ctx.ts, hx, hy, t, L_AMBER); f.bold = false; };
    auto ago = [](long long t) -> std::string { if (t <= 0) return ""; long long d = (long long)time(nullptr) - t; if (d < 90) return "just now"; if (d < 5400) return fmt("%d min ago", (int)(d / 60)); if (d < 129600) return fmt("%d h ago", (int)(d / 3600)); return fmt("%d days ago", (int)(d / 86400)); };
    auto slotLine = [&](const SaveSlotInfo& s) { return fmt("%s, level %d - %s", s.hero.c_str(), s.level, s.mapName.c_str()); };
    auto loadSlotAsk = [&](int slot) {
        if (game->running) showDialog("Load Game", "Loading will discard unsaved progress in the running game. Continue?", [this, slot]() { loadSlot(slot); }, true);
        else loadSlot(slot); };
    auto campaignSwitcher = [&](int sx, int sy, int sw) {   // < name > for the pages that show one adventure's saves
        if (launchButton(cv, in, sx, sy, 22, 20, "<", camps.size() > 1, false, active) && !camps.empty()) pickCampaign(camps[(ci + camps.size() - 1) % camps.size()].id);
        cv.fillRect(sx + 26, sy, sw - 52, 20, RGB(10, 9, 18)); cv.rect(sx + 26, sy, sw - 52, 20, L_EDGE);
        std::string nm = cur ? cur->name : launchCampaign; f.draw(cv, *ctx.ts, sx + 26 + (sw - 52 - f.width(nm)) / 2, sy + 4, nm, L_TEXT);
        if (launchButton(cv, in, sx + sw - 22, sy, 22, 20, ">", camps.size() > 1, false, active) && !camps.empty()) pickCampaign(camps[(ci + 1) % camps.size()].id);
    };

    if (w.page == LP_SAVES) {
        heading(cx, cy + 2, "Saves");
        campaignSwitcher(cx + 60, cy, cw - 60);
        f.draw(cv, *ctx.ts, cx, cy + 26, "Every adventure keeps its own three slots and autosave.", L_DIM);
        int ly = cy + 44, rowH = 46;
        for (int i = 0; i < 4; i++) {
            int ry = ly + i * rowH; bool sel = w.pick == i; const SaveSlotInfo& s = slots[i];
            bool hv = active && !mouseUsed && in.mouseX >= cx && in.mouseX < cx + cw && in.mouseY >= ry && in.mouseY < ry + rowH - 4;
            if (active) hot(cx, ry, cw, rowH - 4);
            cv.fillRect(cx, ry, cw, rowH - 4, sel ? L_SEL : hv ? L_HOVER : L_PANEL);
            cv.rect(cx, ry, cw, rowH - 4, sel ? L_AMBER : L_EDGE);
            drawIcon(cv, s.exists ? 3 : 1, s.exists ? 10 : 1, cx + 6, ry + 5, 1);
            std::string label = i == 3 ? std::string("Autosave") : fmt("Slot %d", i + 1);
            if (s.exists) {
                f.draw(cv, *ctx.ts, cx + 46, ry + 6, label + ": " + s.hero + fmt("  (level %d)", s.level), sel ? L_AMBER : L_TEXT);
                std::string a = ago(s.saved);
                f.draw(cv, *ctx.ts, cx + 46, ry + 21, fmt("%s - %02d:%02d played", s.mapName.c_str(), s.playtime / 3600, (s.playtime / 60) % 60) + (a.empty() ? "" : " - saved " + a), L_DIM);
            } else f.draw(cv, *ctx.ts, cx + 46, ry + 13, label + (i == 3 ? ": none yet - made when you travel" : ": empty"), L_DIM);
            if (hv && in.mousePressed[0]) { mouseUsed = true;
                if (sel && s.exists && dblTimer > 0 && dblIcon == 200 + i) { dblTimer = 0; loadSlotAsk(i); return; }
                w.pick = i; dblTimer = 0.4f; dblIcon = 200 + i; gAudio.play("desktop_click"); }
        }
        int byB = ly + 4 * rowH + 4, bw2 = (cw - 8) / 2;
        bool ok = w.pick >= 0 && w.pick < 4 && slots[w.pick].exists;
        if (launchButton(cv, in, cx, byB, bw2, 26, "Load", ok, true, active)) { loadSlotAsk(w.pick); return; }
        if (launchButton(cv, in, cx + bw2 + 8, byB, bw2, 26, "Delete...", ok, false, active)) {
            int slot = w.pick; std::string camp = slotsCampaign;
            showDialog("Delete Save", (slot == 3 ? std::string("Delete the autosave") : fmt("Delete slot %d", slot + 1)) + (cur ? " of " + cur->name : std::string()) + " permanently?",
                [this, slot, camp]() { slotsCampaign = camp; std::string p = slotPath(slot); remove(p.c_str()); remove((p + ".bak").c_str()); refreshSlots(); }, true); return; }
    } else if (w.page == LP_HELP) {
        heading(cx, cy + 2, "How to Play");
        std::vector<std::string> lines; for (auto& l : howToPlayLines()) { if (f.width(l) <= cw - 20) { lines.push_back(l); continue; } for (auto& wl : f.wrap(l, cw - 20)) lines.push_back(wl); }
        launchScrollText(cv, in, w, cx, cy + 18, cw, ch - 18, lines, active);
    } else if (w.page == LP_ACHIEV) {
        auto& A = game->achievements; int n = (int)A.size();
        heading(cx, cy + 2, "Achievements");
        { std::string c = fmt("%d of %d unlocked", (int)game->unlocked.size(), n); f.draw(cv, *ctx.ts, cx + cw - f.width(c), cy + 2, c, L_DIM); }
        int ly = cy + 18, lh = ch - 18, rowH = 30, maxShow = std::max(1, (lh - 6) / rowH);
        cv.fillRect(cx, ly, cw, lh, RGB(10, 9, 18)); cv.rect(cx, ly, cw, lh, L_EDGE);
        if (active && in.wheel && in.mouseX >= cx && in.mouseY >= ly) w.scroll -= in.wheel;
        w.scroll = clampi(w.scroll, 0, std::max(0, n - maxShow));
        for (int i = 0; i < maxShow && w.scroll + i < n; i++) {
            auto& a = A[w.scroll + i]; bool got = game->unlocked.count(a.id) > 0; int ry = ly + 4 + i * rowH;
            cv.fillRect(cx + 6, ry + 1, 26, 26, got ? RGB(70, 54, 20) : RGB(30, 28, 40)); cv.rect(cx + 6, ry + 1, 26, 26, got ? L_AMBER : RGB(60, 56, 80));
            ctx.ts->draw(cv, a.tile, cx + 7, ry + 2, 2, got ? C::White : RGB(90, 86, 110));
            bool hide = a.hidden && !got;
            f.bold = true; f.draw(cv, *ctx.ts, cx + 40, ry + 1, hide ? "???" : a.name, got ? L_AMBER : L_DIM); f.bold = false;
            std::string d = hide ? "Hidden achievement" : a.desc; while (f.width(d) > cw - 120 && !d.empty()) d.pop_back();
            f.draw(cv, *ctx.ts, cx + 40, ry + 14, d, got ? L_TEXT : L_DIM);
            if (got) f.draw(cv, *ctx.ts, cx + cw - 70, ry + 8, "unlocked", L_GOOD);
        }
        if (n > maxShow) { int sx = cx + cw - 8, sy = ly + 3, sh = lh - 6, th = std::max(14, sh * maxShow / n), ty = sy + (sh - th) * w.scroll / std::max(1, n - maxShow); cv.fillRect(sx, sy, 5, sh, RGB(32, 28, 50)); cv.fillRect(sx, ty, 5, th, L_EDGE); }
    } else if (w.page == LP_ABOUT) {
        heading(cx, cy + 2, "About Triffall");
        int y = cy + 22;
        static const char* credits[] = {
            "An open-world RPG with tactical turn-based combat, wrapped", "in a nostalgic desktop.", "",
            "Art: Urizen 1-bit tileset by vurmux (CC0).", "Desktop icons: Cute Desktop Icons by Mayke-Arth.", "Music: Pixel Fantasy by AlkaKrab.",
            "Sound effects: Pixel Combat by Helton Yan (CC BY 4.0).", "Engine: C++ software renderer + raylib.",
        };
        f.draw(cv, *ctx.ts, cx, y, std::string("Version ") + TRIFFALL_VERSION, L_TEXT);
        // 1.7.0 (#140): a newer version ready to install (only in a copy installed with an update source)
        if (gUpdater.available()) {
            std::string u = "Version " + gUpdater.newVersion() + " is ready.";
            int ux = cx + f.width(std::string("Version ") + TRIFFALL_VERSION) + 16;
            f.draw(cv, *ctx.ts, ux, y, u, L_AMBER);
            int bx2 = ux + f.width(u) + 10, bw3 = std::min(110, cx + cw - bx2);
            if (bw3 >= 70 && launchButton(cv, in, bx2, y - 5, bw3, 22, "Update now", true, true, active)) { askUpdate(); return; }
        } else if (gUpdater.enabled()) f.draw(cv, *ctx.ts, cx + f.width(std::string("Version ") + TRIFFALL_VERSION) + 16, y, "(up to date)", L_DIM);
        y += 18;
        for (auto* l : credits) { f.draw(cv, *ctx.ts, cx, y, l, L_TEXT); y += 13; }
        y += 10;
        int bw2 = std::min(200, cw);
        if (launchButton(cv, in, cx, y, bw2, 24, "Join the Discord", true, false, active)) { runAction("discord"); return; }
        y += 30;
        if (game && game->demoMode && !game->storeUrl.empty()) { if (launchButton(cv, in, cx, y, bw2, 24, "Wishlist Triffall", true, true, active)) { runAction("wishlist"); return; } y += 30; }
    }
}

// ---------------- 1.8.3 (#175 #179 #183): the launcher's first page in each theme ----------------
void Desktop::drawLauncherHome(Canvas& cv, DeskWindow& w, InputState& in, bool active, Rect a) {
    Font& f = *ctx.font; Theme::NoMap nm; const UiTheme& th = Theme::th(); Theme::Pen pen{&cv, ctx.font, ctx.ts};
    const bool one = Theme::onebit(), nat = Theme::native();
    auto camps = listCampaigns(assetDir);
    int ci = 0; for (size_t i = 0; i < camps.size(); i++) if (camps[i].id == launchCampaign) ci = (int)i;
    const CampaignInfo* cur = camps.empty() ? nullptr : &camps[ci];
    auto pickCampaign = [&](const std::string& id) { if (id == launchCampaign) return; launchCampaign = id; slotsCampaign = id; refreshSlots(); w.pick = 0; saveDesktopSettings(); };
    auto tagOf = [](const CampaignInfo& c) { return c.id == "triffall" ? std::string("the main story") : c.id == "tutorial" ? std::string("start here") : c.author.empty() ? std::string("campaign") : "by " + c.author; };
    auto icon = [&](int ic, int ir, int x, int y, int size) { for (int yy = 0; yy < size; yy++) for (int xx = 0; xx < size; xx++) { Color8 c = icons.get(32 + ic * 32 + xx * 32 / size, 32 + ir * 32 + yy * 32 / size); if (c.a) cv.put(x + xx, y + yy, c); } };
    const int newest = newestSlot();
    const bool hasSave = newest >= 0;
    auto loadSlotAsk = [&](int slot) { if (game->running) showDialog("Load Game", "Loading will discard unsaved progress in the running game. Continue?", [this, slot]() { loadSlot(slot); }, true); else loadSlot(slot); };
    const int nAch = (int)game->achievements.size(), nGot = (int)game->unlocked.size();
#ifndef TRIFFALL_NO_EDITOR
    const bool editor = !(game && game->demoMode);
#else
    const bool editor = false;
#endif
    // what each Begin entry does; returns true when the window may be gone (a new window opened)
    enum { B_RESUME, B_CONTINUE, B_NEW, B_LOAD, B_DELETE, B_SEP, B_SAVES, B_HELP, B_ACH, B_SETTINGS, B_EDITOR, B_ABOUT };
    auto act = [&](int b) -> bool {
        switch (b) {
            case B_RESUME: runAction("resume"); return true;
            case B_CONTINUE: if (hasSave) { loadSlotAsk(newest); return true; } return false;
            case B_NEW: if (cur) { openNewGame(cur->id); return true; } return false;
            case B_LOAD: case B_SAVES: w.page = LP_SAVES; w.scroll = 0; w.pick = std::max(0, newest); gAudio.play("desktop_click"); return false;
            case B_DELETE: if (cur) { askDeleteCampaign(cur->id); return true; } return false;
            case B_HELP: w.page = LP_HELP; w.scroll = 0; gAudio.play("desktop_click"); return false;
            case B_ACH: w.page = LP_ACHIEV; w.scroll = 0; gAudio.play("desktop_click"); return false;
            case B_SETTINGS: openWindow("settings"); return true;
            case B_EDITOR: runAction("editor"); return true;
            case B_ABOUT: w.page = LP_ABOUT; w.scroll = 0; gAudio.play("desktop_click"); return false;
        }
        return false;
    };
    struct Entry { int id; std::string label, sub; bool enabled, primary; std::string right; };
    std::vector<Entry> begin;
    if (game->running) begin.push_back({B_RESUME, "Resume", "back to " + game->heroName, true, true, ""});
    begin.push_back({B_CONTINUE, "Continue", hasSave ? "newest save: " + slots[newest].hero + ", " + slots[newest].mapName : "no saves for this adventure yet", hasSave, hasSave && !game->running, ""});
    begin.push_back({B_NEW, "New Game", "pick a name and a class", true, !hasSave && !game->running, ""});
    begin.push_back({B_LOAD, "Load a save", "3 slots per adventure", true, false, ""});
    if (cur && !isCoreCampaign(cur->id)) begin.push_back({B_DELETE, "Delete this adventure...", "", true, false, ""});
    begin.push_back({B_SEP, "", "", false, false, ""});
    begin.push_back({B_SAVES, "Saves", "", true, false, ""});
    begin.push_back({B_HELP, "How to Play", "", true, false, ""});
    begin.push_back({B_ACH, "Achievements", fmt("%d of %d", nGot, nAch), true, false, fmt("%d/%d", nGot, nAch)});
    begin.push_back({B_SETTINGS, "Settings", "", true, false, ""});
    if (editor) begin.push_back({B_EDITOR, "Editor", "make your own campaigns", true, false, ""});
    begin.push_back({B_ABOUT, "About", "", true, false, ""});
    auto clickCampaign = [&](size_t i, bool sel) -> bool {   // a click on an adventure: pick it; a double-click starts a New Game
        mouseUsed = true; gAudio.play("desktop_click");
        if (sel && dblTimer > 0 && dblIcon == 300 + (int)i) { dblTimer = 0; openNewGame(camps[i].id); return true; }
        pickCampaign(camps[i].id); dblTimer = 0.4f; dblIcon = 300 + (int)i; return false;
    };
    auto blurb = [&]() { return cur ? (cur->desc.empty() ? std::string("No description.") : cur->desc) : std::string(""); };
    auto padStart = [&](int x, int y) { if (w.focusFirst && active) { w.focusFirst = false; w.focusX = x; w.focusY = y; } };   // a controller starts on the main entry

    if (nat) {
        // ---- desktop-native: a menu bar, the banner in a sunken well, a tree view and a details pane ----
        int mx = a.x + 6; static const char* MENUS[] = {"File", "Adventure", "View", "Help"}; static const int MACT[] = {B_SETTINGS, B_NEW, B_HELP, B_ABOUT};
        for (int k = 0; k < 4; k++) {
            int mw = f.width(MENUS[k]) + 12; bool hv = active && !mouseUsed && in.mouseX >= mx && in.mouseX < mx + mw && in.mouseY >= a.y + 1 && in.mouseY < a.y + 18;
            if (active) hot(mx, a.y + 1, mw, 17);
            if (hv) Theme::bevel(cv, mx, a.y + 1, mw, 17, th.hi, th.lo2);
            f.draw(cv, *ctx.ts, mx + 6, a.y + 3, MENUS[k], RGB(0, 0, 0)); cv.hline(mx + 6, a.y + 14, f.width(std::string(1, MENUS[k][0])), RGB(0, 0, 0));
            if (hv && in.mousePressed[0]) { mouseUsed = true; if (act(MACT[k])) return; }
            mx += mw;
        }
        int bannerH = 88; Color8 blk{0, 0, 0, 255}; Theme::well(cv, a.x + 4, a.y + 20, a.w - 8, bannerH, &blk);
        drawLauncherBanner(cv, a.x + 6, a.y + 22, a.w - 12, bannerH - 4);
        // the tree
        int tx = a.x + 4, ty = a.y + bannerH + 26, tw = 210, tBottom = a.y + a.h - 40, th2 = tBottom - ty;
        Color8 white{255, 255, 255, 255}; Theme::well(cv, tx, ty, tw, th2, &white);
        struct Node { int depth; std::string label; int ic, ir; int kind; int arg; bool sel; bool dim; };   // kind: 0 folder/none, 1 campaign, 2 slot, 3 action
        std::vector<Node> nodes;
        nodes.push_back({0, "Adventures", 3, 0, 0, 0, false, false});
        for (size_t i = 0; i < camps.size(); i++) nodes.push_back({1, camps[i].name, camps[i].iconC, camps[i].iconR, 1, (int)i, camps[i].id == launchCampaign, false});
        nodes.push_back({0, "Saves", 3, 0, 3, B_SAVES, false, false});
        bool anySave = false; for (int i = 0; i < 4; i++) if (slots[i].exists) { anySave = true; nodes.push_back({1, (i == 3 ? std::string("Autosave") : fmt("Slot %d", i + 1)) + ": " + slots[i].hero, 3, 10, 2, i, false, false}); }
        if (!anySave) nodes.push_back({1, "(no saves yet)", -1, -1, 0, 0, false, true});
        static const int AICON[][2] = {{1, 4}, {3, 8}, {2, 3}, {3, 3}, {3, 3}};
        nodes.push_back({0, "How to Play", AICON[0][0], AICON[0][1], 3, B_HELP, false, false});
        nodes.push_back({0, "Achievements", AICON[1][0], AICON[1][1], 3, B_ACH, false, false});
        nodes.push_back({0, "Settings", AICON[2][0], AICON[2][1], 3, B_SETTINGS, false, false});
        if (editor) nodes.push_back({0, "Editor", 2, 4, 3, B_EDITOR, false, false});
        nodes.push_back({0, "About", AICON[4][0], AICON[4][1], 3, B_ABOUT, false, false});
        int rowH = std::max(16, std::min(24, (th2 - 8) / std::max(1, (int)nodes.size())));
        for (size_t k = 0; k < nodes.size(); k++) {
            const Node& nd = nodes[k]; int ry = ty + 4 + (int)k * rowH; if (ry + rowH > tBottom) break;
            int nx0 = tx + 6 + nd.depth * 18;
            if (nd.depth) cv.hline(nx0 - 12, ry + rowH / 2, 8, RGB(128, 128, 128));
            if (nd.ic >= 0) icon(nd.ic, nd.ir, nx0, ry + (rowH - 16) / 2, 16);
            int lx = nx0 + (nd.ic >= 0 ? 20 : 2); std::string lbl = nd.label; while (lbl.size() > 3 && f.width(lbl) > tx + tw - 8 - lx) lbl.pop_back();
            bool hv = active && !mouseUsed && nd.kind && in.mouseX >= tx + 2 && in.mouseX < tx + tw - 2 && in.mouseY >= ry && in.mouseY < ry + rowH;
            if (nd.kind && active) hot(lx - 2, ry, f.width(lbl) + 4, rowH);
            if (nd.sel) cv.fillRect(lx - 2, ry + (rowH - 14) / 2, f.width(lbl) + 4, 14, RGB(0, 0, 128));
            else if (hv) { for (int xx = lx - 2; xx < lx + f.width(lbl) + 2; xx += 2) { cv.put(xx, ry + (rowH - 14) / 2, RGB(0, 0, 0)); cv.put(xx, ry + (rowH + 12) / 2, RGB(0, 0, 0)); } }
            f.draw(cv, *ctx.ts, lx, ry + (rowH - 12) / 2, lbl, nd.sel ? RGB(255, 255, 255) : nd.dim ? RGB(128, 128, 128) : RGB(0, 0, 0));
            if (hv && in.mousePressed[0]) {
                if (nd.kind == 1) { if (clickCampaign((size_t)nd.arg, nd.sel)) return; }
                else if (nd.kind == 2) { mouseUsed = true; loadSlotAsk(nd.arg); return; }
                else if (nd.kind == 3) { mouseUsed = true; if (act(nd.arg)) return; }
            }
        }
        // details and Start
        int dx = tx + tw + 8, dw = a.x + a.w - 4 - dx, dy = ty - 4;
        std::string head = cur ? cur->name + " - " + tagOf(*cur) : std::string("Triffall");
        int detH = std::min(150, (tBottom - dy) / 2 + 10);
        Theme::panelTitled(pen, dx, dy, dw, detH, head);
        { int yy = dy + 20; auto ls = f.wrap(blurb(), dw - 24); for (size_t i = 0; i < ls.size() && yy < dy + detH - 46; i++) { f.draw(cv, *ctx.ts, dx + 12, yy, ls[i], RGB(0, 0, 0)); yy += 15; }
          f.draw(cv, *ctx.ts, dx + 12, dy + detH - 42, hasSave ? "Saves: newest " + slots[newest].hero + ", " + slots[newest].mapName : std::string("Saves: none for this adventure yet"), RGB(128, 128, 128));
          f.draw(cv, *ctx.ts, dx + 12, dy + detH - 24, std::string("Version ") + TRIFFALL_VERSION, RGB(128, 128, 128)); }
        int sy = dy + detH + 6, sh = tBottom - sy;
        Theme::panelTitled(pen, dx, sy, dw, sh, "Start");
        struct SB { int id; const char* label; std::string expl; bool enabled, def; };
        std::vector<SB> sbs;
        if (game->running) sbs.push_back({B_RESUME, "Resume", "Back to " + game->heroName + ".", true, true});
        sbs.push_back({B_NEW, "New Game", "Pick a name and a class.", true, !game->running && !hasSave});
        sbs.push_back({B_CONTINUE, "Continue", hasSave ? "Your newest save." : "Nothing to continue yet.", hasSave, !game->running && hasSave});
        sbs.push_back({B_LOAD, "Load a save...", "3 named slots per adventure.", true, false});
        int pitch = std::min(32, (sh - 24) / (int)sbs.size());
        for (size_t k = 0; k < sbs.size(); k++) {
            int by = sy + 20 + (int)k * pitch;
            bool hv = active && sbs[k].enabled && !mouseUsed && in.mouseX >= dx + 12 && in.mouseX < dx + 152 && in.mouseY >= by && in.mouseY < by + 26;
            if (active && sbs[k].enabled) hot(dx + 12, by, 140, 26);
            Theme::button(pen, dx + 12, by, 140, 26, sbs[k].label, !sbs[k].enabled ? Theme::BS_DISABLED : sbs[k].def ? Theme::BS_DEFAULT : Theme::BS_NORMAL, hv);
            if (sbs[k].def) padStart(dx + 82, by + 13);
            f.draw(cv, *ctx.ts, dx + 164, by + 7, sbs[k].expl, sbs[k].enabled ? RGB(0, 0, 0) : RGB(128, 128, 128));
            if (hv && in.mousePressed[0]) { mouseUsed = true; gAudio.play("desktop_click"); if (act(sbs[k].id)) return; }
        }
        if (cur && !isCoreCampaign(cur->id)) { int bx = dx + dw - 100; if (launchButton(cv, in, bx, sy + sh - 32, 90, 24, "Delete...", true, false, active)) { askDeleteCampaign(cur->id); return; } }
        Theme::rule(cv, a.x + 4, a.y + a.h - 34, a.w - 8);
        f.draw(cv, *ctx.ts, a.x + 8, a.y + a.h - 24, "Tip: every launcher page is also in the Start menu.", RGB(128, 128, 128));
        if (launchButton(cv, in, a.x + a.w - 100, a.y + a.h - 28, 94, 24, "Close", true, false, active)) { w.wantClose = true; mouseUsed = true; }
        return;
    }

    if (one) {
        // ---- one-bit: the banner in an ash frame, adventure cards on the left, the Begin list with the torch on the right ----
        int bw = std::min(596, a.w - 100), bx = a.x + (a.w - bw) / 2, by = a.y + 8;
        drawLauncherBanner(cv, bx, by, bw, 84); cv.rect(bx - 1, by - 1, bw + 2, 86, th.dim);
        int lx = a.x + 16, ly = by + 98, lw = std::min(250, (a.w - 48) / 2 - 20);
        Theme::title(pen, lx, ly, "Adventure");
        int cy = ly + 20, cardH = 46, maxCards = std::max(1, std::min((int)camps.size(), (a.y + a.h - 70 - cy) / (cardH + 8)));
        int first = clampi(w.scroll, 0, std::max(0, (int)camps.size() - maxCards)); w.scroll = first;
        if (active && in.wheel && in.mouseX >= lx && in.mouseX < lx + lw && in.mouseY >= cy) w.scroll = clampi(w.scroll - in.wheel, 0, std::max(0, (int)camps.size() - maxCards));
        for (int k = 0; k < maxCards && first + k < (int)camps.size(); k++) {
            const CampaignInfo& c = camps[first + k]; int ry = cy + k * (cardH + 8); bool sel = c.id == launchCampaign;
            bool hv = active && !mouseUsed && in.mouseX >= lx && in.mouseX < lx + lw && in.mouseY >= ry && in.mouseY < ry + cardH;
            if (active) hot(lx, ry, lw, cardH);
            if (sel) Theme::glow(cv, lx, ry, lw, cardH); else for (int xx = lx; xx < lx + lw; xx += 2) { cv.put(xx, ry, hv ? th.accent : th.dim); cv.put(xx, ry + cardH - 1, hv ? th.accent : th.dim); }
            if (!sel) for (int yy = ry; yy < ry + cardH; yy += 2) { cv.put(lx, yy, hv ? th.accent : th.dim); cv.put(lx + lw - 1, yy, hv ? th.accent : th.dim); }
            icon(c.iconC, c.iconR, lx + 8, ry + 7, 32);
            std::string nm = c.name; while (nm.size() > 3 && f.width(nm) > lw - 60) nm.pop_back();
            f.draw(cv, *ctx.ts, lx + 48, ry + 8, nm, sel ? th.accent : th.text);
            f.draw(cv, *ctx.ts, lx + 48, ry + 25, tagOf(c), c.id == "tutorial" ? th.good : th.text);
            if (hv && in.mousePressed[0]) { if (clickCampaign((size_t)(first + k), sel)) return; }
        }
        int byy = cy + maxCards * (cardH + 8) + 4;
        { auto ls = f.wrap(blurb(), lw); for (size_t i = 0; i < ls.size() && byy < a.y + a.h - 56; i++) { f.draw(cv, *ctx.ts, lx, byy, ls[i], th.dim); byy += 14; } }
        if (!hasSave && byy < a.y + a.h - 36) f.draw(cv, *ctx.ts, lx, byy + 14, "no saves for this adventure yet", th.dim);
        // Begin
        int rx = a.x + a.w / 2 - 4, rw = a.x + a.w - 24 - rx;
        Theme::title(pen, rx, ly, "Begin");
        const ItemDef* torch = ctx.gd ? ctx.gd->item("torch_item") : nullptr;
        int yy = cy, rowH = std::max(18, std::min(27, (a.y + a.h - 30 - cy) / (int)begin.size()));
        int chosen = -1; for (size_t k = 0; k < begin.size(); k++) if (begin[k].primary) { chosen = (int)k; break; }
        int hoverRow = -1; for (size_t k = 0; k < begin.size(); k++) { int ry = cy + (int)k * rowH; if (begin[k].id != B_SEP && active && in.mouseX >= rx && in.mouseX < rx + rw && in.mouseY >= ry && in.mouseY < ry + rowH - 4) hoverRow = (int)k; }
        if (hoverRow >= 0) chosen = hoverRow;
        for (size_t k = 0; k < begin.size(); k++) {
            const Entry& e = begin[k]; int ry = yy + (int)k * rowH;
            if (e.id == B_SEP) { for (int xx = rx; xx < rx + rw; xx += 2) cv.put(xx, ry + rowH / 2 - 2, th.dim); continue; }
            if (active && e.enabled) hot(rx, ry, rw, rowH - 4);
            if (e.primary) padStart(rx + rw / 2, ry + (rowH - 4) / 2);
            if ((int)k == chosen) { Theme::glow(cv, rx, ry, rw, rowH - 4); if (torch) drawItemTile(*ctx.ts, cv, *torch, rx + 6, ry + (rowH - 16) / 2, 1); }
            f.draw(cv, *ctx.ts, rx + 24, ry + (rowH - 16) / 2, e.label, !e.enabled ? th.dim : (int)k == chosen ? th.accent : th.text);
            std::string right = e.id == B_CONTINUE && !e.enabled ? std::string("nothing yet") : e.right;
            if (!right.empty()) f.draw(cv, *ctx.ts, rx + rw - 8 - f.width(right), ry + (rowH - 16) / 2, right, th.dim);
            if ((int)k == hoverRow && e.enabled && in.mousePressed[0] && !mouseUsed) { mouseUsed = true; gAudio.play("desktop_click"); if (act(e.id)) return; }
        }
        ctx.font->drawSmall(cv, a.x + 16, a.y + a.h - 14, gPadGlyphs ? "a: open   d-pad: choose   b: close" : "enter: open   up/down: choose   esc: close", th.dim);   // 1.8.5 (#200)
        return;
    }

    // ---- Adventurer's Frame: box art, the story and the adventures on the left, a carved Begin panel on the right ----
    int lw = std::min(300, a.w * 46 / 100), lx = a.x + 6, ly = a.y + 6, lh = a.h - 12;
    Color8 blk{0, 0, 0, 255}; Theme::well(cv, lx, ly, lw, lh, &blk);
    int cw2 = lw - 2, bh2 = 84;
    drawLauncherBanner(cv, lx + 1, ly + 1, cw2 * 2, bh2, {lx + 1, ly + 1, cw2, bh2});
    drawLauncherBanner(cv, lx + 1 - cw2, ly + 2 + bh2, cw2 * 2, bh2, {lx + 1, ly + 2 + bh2, cw2, bh2});
    int ty = ly + 2 + 2 * bh2 + 12;
    { auto ls = f.wrap(blurb(), lw - 28); for (size_t i = 0; i < ls.size() && i < 4; i++) { f.draw(cv, *ctx.ts, lx + 12, ty, ls[i], th.text); ty += 15; } }
    int listTop = std::max(ty + 8, ly + lh - 132);
    cv.fillRect(lx + 12, listTop, lw - 24, 1, th.selLo);
    f.draw(cv, *ctx.ts, lx + 12, listTop + 8, "Adventure", th.dim);
    int rowsY = listTop + 24, rowH = 34, maxRows = std::max(1, (ly + lh - 4 - rowsY) / rowH);
    int first = clampi(w.scroll, 0, std::max(0, (int)camps.size() - maxRows)); w.scroll = first;
    if (active && in.wheel && in.mouseX >= lx && in.mouseX < lx + lw && in.mouseY >= rowsY) w.scroll = clampi(w.scroll - in.wheel, 0, std::max(0, (int)camps.size() - maxRows));
    for (int k = 0; k < maxRows && first + k < (int)camps.size(); k++) {
        const CampaignInfo& c = camps[first + k]; int ry = rowsY + k * rowH; bool sel = c.id == launchCampaign;
        bool hv = active && !mouseUsed && in.mouseX >= lx + 8 && in.mouseX < lx + lw - 8 && in.mouseY >= ry - 3 && in.mouseY < ry + rowH - 4;
        if (active) hot(lx + 8, ry - 3, lw - 16, rowH - 2);
        if (sel) { cv.fillRect(lx + 8, ry - 3, lw - 16, 30, th.rowSel); cv.rect(lx + 8, ry - 3, lw - 16, 30, th.accent); }
        else if (hv) cv.fillRect(lx + 8, ry - 3, lw - 16, 30, RGB(30, 33, 46));
        icon(c.iconC, c.iconR, lx + 12, ry - 1, 24);
        std::string nm = c.name; while (nm.size() > 3 && f.width(nm) > lw - 64) nm.pop_back();
        f.draw(cv, *ctx.ts, lx + 42, ry, nm, sel ? th.accent : th.text);
        ctx.font->drawSmall(cv, lx + 42, ry + 15, tagOf(c), c.id == "tutorial" ? th.good : th.dim);
        if (hv && in.mousePressed[0]) { if (clickCampaign((size_t)(first + k), sel)) return; }
    }
    // Begin
    int px = lx + lw + 6, pw = a.x + a.w - 6 - px;
    Theme::panel(cv, px, ly, pw, lh);
    Theme::title(pen, px + 18, ly + 12, "Begin");
    int bx = px + 12, bwid = pw - 24, by = ly + 34;
    int avail = ly + lh - 8 - by, need = 0; for (auto& e : begin) need += e.id == B_SEP ? 12 : (e.sub.empty() ? 22 : 30) + 5;
    bool compact = need > avail;
    for (auto& e : begin) {
        if (e.id == B_SEP) { Theme::rule(cv, bx + 2, by + 4, bwid - 4); by += 12; continue; }
        bool withSub = !e.sub.empty() && !compact; int bh = withSub ? 30 : 22;
        if (by + bh > ly + lh - 6) break;
        bool hv = active && e.enabled && !mouseUsed && in.mouseX >= bx && in.mouseX < bx + bwid && in.mouseY >= by && in.mouseY < by + bh;
        if (active && e.enabled) hot(bx, by, bwid, bh);
        Theme::button(pen, bx, by, bwid, bh, e.label, !e.enabled ? Theme::BS_DISABLED : e.primary ? Theme::BS_SELECTED : Theme::BS_NORMAL, hv, "", "", withSub ? e.sub : "");
        if (e.primary) { f.draw(cv, *ctx.ts, bx + bwid - 14, by + (bh - 12) / 2, ">", th.accent); padStart(bx + bwid / 2, by + bh / 2); }
        if (hv && (in.gamepadActive || padNavWindows)) Theme::frameRect(cv, bx - 2, by - 2, bwid + 4, bh + 4, th.accentHi);
        if (hv && in.mousePressed[0]) { mouseUsed = true; gAudio.play("desktop_click"); if (act(e.id)) return; }
        by += bh + 5;
    }
}
