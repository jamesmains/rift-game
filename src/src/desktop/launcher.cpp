// 1.6.1 (#134): the Triffall launcher - the game's own window on the fake desktop, opened by the "Triffall" icon that the
// first-run installer puts there. It is the hub for everything about the game: continue / start / load an adventure
// (each campaign keeps its own saves, #135), How to Play, Achievements, settings, the Editor and the credits.
// 1.6.2 (#137): the Field Notes and Files pages were removed (little used); what you learn about monsters is in the
// Journal's Bestiary, and the documents that hold terminal passwords are found in the world (signs, data disks). Win98 window outside, the game's own night-and-rift look inside.
#include "desktop.h"
#include "../core/audio.h"
#include "../core/version.h"
#include "../game/world.h"
#include <algorithm>
#include <cmath>
#include <ctime>
#include <filesystem>

namespace {
const Color8 L_BG = RGB(16, 14, 28), L_PANEL = RGB(28, 24, 46), L_EDGE = RGB(92, 70, 146), L_TEXT = RGB(226, 220, 206),
             L_DIM = RGB(140, 134, 162), L_AMBER = RGB(255, 205, 90), L_VIOLET = RGB(176, 120, 246), L_SEL = RGB(62, 42, 104),
             L_HOVER = RGB(78, 56, 128), L_GOOD = RGB(120, 220, 140);
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
        "EDITOR", "  Edits maps (the overworld and every dungeon are plain map files", "  you can paint), tile kinds, items, monsters, spells, classes, shops,", "  quests, dungeons, recipes and dialogue. The Campaign button picks", "  which campaign you edit or creates a new one; new campaigns show", "  up on the Play page.", "",
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
    bool down = hover && in.mouseDown[0];
    cv.fillRect(x, y, w, h, !enabled ? RGB(24, 22, 36) : down ? L_SEL : hover ? L_HOVER : primary ? RGB(54, 38, 92) : L_PANEL);
    cv.rect(x, y, w, h, !enabled ? RGB(52, 48, 70) : primary || hover ? L_AMBER : L_EDGE);
    if (hover && (in.gamepadActive || padNavWindows)) cv.rect(x + 2, y + 2, w - 4, h - 4, L_AMBER);
    Font& f = *ctx.font; f.bold = primary;
    int tw = f.width(label);
    f.draw(cv, *ctx.ts, x + (w - tw) / 2 + (down ? 1 : 0), y + (h - 12) / 2 + (down ? 1 : 0), label, !enabled ? RGB(84, 80, 100) : primary ? L_AMBER : L_TEXT);
    f.bold = false;
    bool clicked = hover && in.mousePressed[0];
    if (clicked) { mouseUsed = true; gAudio.play("desktop_click"); }
    return clicked;
}

// a box of text lines with a scroll bar; the wheel scrolls it while the pointer is over it, PgUp/PgDn (updateWindow) too
void Desktop::launchScrollText(Canvas& cv, InputState& in, DeskWindow& w, int x, int y, int bw, int bh, const std::vector<std::string>& lines, bool active) {
    Font& f = *ctx.font;
    cv.fillRect(x, y, bw, bh, RGB(10, 9, 18)); cv.rect(x, y, bw, bh, L_EDGE);
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
        cv.fillRect(sx, sy, 5, sh, RGB(32, 28, 50)); cv.fillRect(sx, ty, 5, th, L_EDGE);
    }
}

// the banner: a night sky over Aldmoor with the Rift torn through it, the five heroes on the ridge, the logo
void Desktop::drawLauncherBanner(Canvas& cv, int x, int y, int bw, int bh) {
    cv.setClip({x, y, bw, bh});
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
      f.draw(cv, *ctx.ts, x + 15, y + 15, "TRIFFALL", RGB(90, 50, 0), 3); f.draw(cv, *ctx.ts, x + 12, y + 12, "TRIFFALL", L_AMBER, 3);
      f.bold = false; f.style = keep;
      f.draw(cv, *ctx.ts, x + 14, y + 52, "an old-school RPG", RGB(210, 196, 160));
      f.draw(cv, *ctx.ts, x + bw - 8 - f.width(TRIFFALL_VERSION_LABEL), y + bh - 15, TRIFFALL_VERSION_LABEL, L_DIM); }
    cv.resetClip();
    cv.rect(x, y, bw, bh, L_EDGE);
}

void Desktop::drawLauncher(Canvas& cv, DeskWindow& w, InputState& in, bool active) {
    Font& f = *ctx.font;
    cv.fillRect(w.x + 4, w.y + 24, w.w - 8, w.h - 28, L_BG);
    int bx = w.x + 6, by = w.y + 26, bw = w.w - 12, bh = 84;
    drawLauncherBanner(cv, bx, by, bw, bh);
    // ---- the page list (left) ----
    int nx = w.x + 6, ny = by + bh + 6, nw = 128, ch = w.y + w.h - 6 - ny;
    static const int ICONS[][2] = {{2, 7}, {0, 3}, {1, 4}, {3, 8}, {3, 3}};
    for (int p = 0; p < LP_COUNT; p++) {
        int ry = ny + p * 26; bool sel = w.page == p;
        bool hv = active && !mouseUsed && in.mouseX >= nx && in.mouseX < nx + nw && in.mouseY >= ry && in.mouseY < ry + 24;
        if (active) hot(nx, ry, nw, 24);
        cv.fillRect(nx, ry, nw, 24, sel ? L_SEL : hv ? L_HOVER : L_PANEL);
        if (sel) cv.fillRect(nx, ry, 3, 24, L_AMBER);
        if (hv && (in.gamepadActive || padNavWindows)) cv.rect(nx, ry, nw, 24, L_AMBER);
        int ic = ICONS[p][0], ir = ICONS[p][1]; iconCell(PAGE_ACTIONS[p], ic, ir);
        for (int yy = 0; yy < 16; yy++) for (int xx = 0; xx < 16; xx++) { Color8 c = icons.get(32 + ic * 32 + xx * 2, 32 + ir * 32 + yy * 2); if (c.a) cv.put(nx + 7 + xx, ry + 4 + yy, c); }
        f.bold = sel; f.draw(cv, *ctx.ts, nx + 30, ry + 6, PAGE_NAMES[p], sel ? L_AMBER : L_TEXT); f.bold = false;
        if (hv && in.mousePressed[0]) { if (w.page != p) { w.page = p; w.scroll = 0; w.pick = 0; gAudio.play("desktop_click"); } mouseUsed = true; }
    }
    { int ly = ny + LP_COUNT * 26 + 6;
      cv.hline(nx + 4, ly - 4, nw - 8, L_EDGE);
      if (launchButton(cv, in, nx, ly, nw, 22, "Settings", true, false, active)) { openWindow("settings"); return; }   // (a new window: w is stale after it)
      ly += 26;
#ifndef TRIFFALL_NO_EDITOR
      if (launchButton(cv, in, nx, ly, nw, 22, "Editor", true, false, active)) { runAction("editor"); return; }
      ly += 26;
#endif
      (void)ly; }
    // ---- the page (right) ----
    int cx = nx + nw + 6, cy = ny, cw = w.x + w.w - 6 - cx;
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

    if (w.page == LP_PLAY) {
        // the adventures (campaigns) on this computer
        int lw = 176, rowH = 30, maxRows = std::max(1, (ch - 22) / rowH);
        heading(cx, cy + 2, "Adventures");
        int first = clampi(w.scroll, 0, std::max(0, (int)camps.size() - maxRows)); w.scroll = first;
        if (active && in.wheel && in.mouseX >= cx && in.mouseX < cx + lw && in.mouseY >= cy + 18) w.scroll = clampi(w.scroll - in.wheel, 0, std::max(0, (int)camps.size() - maxRows));
        for (int i = 0; i < maxRows && first + i < (int)camps.size(); i++) {
            const CampaignInfo& c = camps[first + i]; int ry = cy + 18 + i * rowH; bool sel = c.id == launchCampaign;
            bool hv = active && !mouseUsed && in.mouseX >= cx && in.mouseX < cx + lw && in.mouseY >= ry && in.mouseY < ry + rowH - 2;
            if (active) hot(cx, ry, lw, rowH - 2);
            cv.fillRect(cx, ry, lw, rowH - 2, sel ? L_SEL : hv ? L_HOVER : L_PANEL);
            if (sel) cv.rect(cx, ry, lw, rowH - 2, L_AMBER);
            for (int yy = 0; yy < 16; yy++) for (int xx = 0; xx < 16; xx++) { Color8 px = icons.get(32 + c.iconC * 32 + xx * 2, 32 + c.iconR * 32 + yy * 2); if (px.a) cv.put(cx + 6 + xx, ry + 6 + yy, px); }
            std::string nm = c.name; while (f.width(nm) > lw - 34 && !nm.empty()) nm.pop_back();
            f.draw(cv, *ctx.ts, cx + 28, ry + 3, nm, sel ? L_AMBER : L_TEXT);
            std::string tag = c.id == "triffall" ? "the main story" : c.id == "tutorial" ? "start here" : c.author.empty() ? "campaign" : "by " + c.author;
            while (f.width(tag) > lw - 34 && !tag.empty()) tag.pop_back();
            f.draw(cv, *ctx.ts, cx + 28, ry + 15, tag, c.id == "tutorial" ? L_GOOD : L_DIM);
            if (hv && in.mousePressed[0]) { mouseUsed = true; gAudio.play("desktop_click");
                if (sel && dblTimer > 0 && dblIcon == 300 + first + i) { dblTimer = 0; openNewGame(c.id); return; }   // double-click: New Game for it
                pickCampaign(c.id); dblTimer = 0.4f; dblIcon = 300 + first + i; }
        }
        // the picked one: what it is, its newest save, and the big buttons
        int dx = cx + lw + 10, dw = cx + cw - dx;
        if (cur) {
            int y = cy + 2;
            if (game->running) {
                std::string run = "Playing now: " + game->heroName + (game->campaignId != "triffall" ? " (" + game->campaign.name + ")" : "");
                while (f.width(run) > dw && !run.empty()) run.pop_back();
                f.draw(cv, *ctx.ts, dx, y, run, L_GOOD);
                if (launchButton(cv, in, dx, y + 16, dw, 24, "Resume", true, true, active)) { runAction("resume"); return; }
                y += 48;
            }
            f.bold = true; f.draw(cv, *ctx.ts, dx, y, cur->name, L_AMBER); f.bold = false; y += 14;
            std::string by2 = cur->id == "triffall" ? "The main story" : cur->author.empty() ? "" : "by " + cur->author;
            if (!cur->version.empty()) by2 += (by2.empty() ? "version " : ", version ") + cur->version;
            if (!by2.empty()) { f.draw(cv, *ctx.ts, dx, y, by2, L_DIM); y += 14; }
            y += 4;
            auto ls = f.wrap(cur->desc.empty() ? "No description." : cur->desc, dw);
            int maxL = game->running ? 4 : 7;
            for (size_t i = 0; i < ls.size() && (int)i < maxL; i++) { f.draw(cv, *ctx.ts, dx, y, ls[i], L_TEXT); y += 13; }
            if (cur->id == "tutorial" && !game->running) { y += 2; f.draw(cv, *ctx.ts, dx, y, "Recommended for your first time.", L_GOOD); y += 13; }
            int byB = cy + ch - 92;
            int nw2 = newestSlot();
            cv.hline(dx, byB - 26, dw, L_EDGE);
            if (nw2 >= 0) { std::string s = "Newest save: " + slotLine(slots[nw2]); std::string a = ago(slots[nw2].saved); while (f.width(s) > dw && !s.empty()) s.pop_back(); f.draw(cv, *ctx.ts, dx, byB - 20, s, L_TEXT); if (!a.empty()) f.draw(cv, *ctx.ts, dx, byB - 8, (nw2 == 3 ? "autosave, " : fmt("slot %d, ", nw2 + 1)) + a, L_DIM); }
            else f.draw(cv, *ctx.ts, dx, byB - 16, "No saves for this adventure yet.", L_DIM);
            if (w.focusFirst && active) { w.focusFirst = false; w.focusX = dx + dw / 2; w.focusY = (nw2 >= 0 ? byB + 4 : byB + 34) + 13; }   // a controller starts on Continue (or New Game)
            if (launchButton(cv, in, dx, byB + 4, dw, 26, "Continue", nw2 >= 0, nw2 >= 0 && !game->running, active)) { loadSlotAsk(nw2); return; }
            if (launchButton(cv, in, dx, byB + 34, dw, 26, "New Game", true, nw2 < 0 && !game->running, active)) { openNewGame(cur->id); return; }
            if (launchButton(cv, in, dx, byB + 64, dw, 24, "Load a save...", true, false, active)) { w.page = LP_SAVES; w.scroll = 0; w.pick = std::max(0, nw2); }
        }
    } else if (w.page == LP_SAVES) {
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
        f.draw(cv, *ctx.ts, cx, y, std::string("Version ") + TRIFFALL_VERSION, L_TEXT); y += 18;
        for (auto* l : credits) { f.draw(cv, *ctx.ts, cx, y, l, L_TEXT); y += 13; }
        y += 10;
        int bw2 = std::min(200, cw);
        if (launchButton(cv, in, cx, y, bw2, 24, "Join the Discord", true, false, active)) { runAction("discord"); return; }
        y += 30;
        if (game && game->demoMode && !game->storeUrl.empty()) { if (launchButton(cv, in, cx, y, bw2, 24, "Wishlist Triffall", true, true, active)) { runAction("wishlist"); return; } y += 30; }
    }
}
