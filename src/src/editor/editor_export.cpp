// Editor (Campaign tab, right column): "Play from a state" - the party a playtest starts with (level, class, gold,
// flags, items, companions; saved in the campaign's playtest.txt, which the game never reads) - and the campaign
// export: the whole campaign folder packed into one .zip (store-only, no compression, so no zlib needed) that
// another player unpacks into assets/campaigns/.
#include "editor.h"
#include <algorithm>
#include <filesystem>
#include <ctime>
#include <cstdio>

using namespace ED;
namespace fs = std::filesystem;
extern int gEdTop;
static const int TOP_BAR_BOTTOM = 18;

// ---------------------------------------------------------------- play state
void Editor::loadPlayState() {
    play = PlayState(); std::string t;
    if (!readFile(dataRoot + "/playtest.txt", t)) return;
    for (auto& sec : parseConfig(t)) if (sec.name == "playtest") {
        play.on = sec.geti("on", 0) != 0; play.level = clampi(sec.geti("level", 1), 1, 30); play.gold = std::max(0, sec.geti("gold", 500));
        play.cls = sec.get("class"); play.flags = sec.get("flags"); play.items = sec.get("items"); play.party = sec.get("party");
    }
}
bool Editor::savePlayState() {
    std::string o = "# Editor playtest state (Campaign tab > Play from a state). The game itself never reads this file.\n[playtest]\n";
    o += fmt("on=%d\nlevel=%d\ngold=%d\n", play.on ? 1 : 0, play.level, play.gold);
    if (!play.cls.empty()) o += "class=" + play.cls + "\n";
    if (!play.flags.empty()) o += "flags=" + play.flags + "\n";
    if (!play.items.empty()) o += "items=" + play.items + "\n";
    if (!play.party.empty()) o += "party=" + play.party + "\n";
    if (!writeFile(dataRoot + "/playtest.txt", o)) { say("Could not write playtest.txt"); return false; }
    play.dirty = false; return true;
}

void Editor::drawPlayState(int x, int y, int w, int h, InputState& in) {
    ui.panel(x, y, w, h); ui.header(x, y, w, "Play from a state" + std::string(play.dirty ? "  (unsaved)" : "")); helpBtn(x + w - 16, y + 1, "campaign.playstate", "tab.campaign");
    int lx = x + 8, fx = x + 96, fw = w - 104 - 58, ry = y + 24;
    bool on = play.on; if (ui.checkbox(lx, ry + 2, "Every playtest starts with this party", on)) { play.on = on; play.dirty = true; } ry += 18;
    for (auto& l : font->wrap(play.on ? "Play > and P over a tile start a fresh game with the state below." : "Off: Play > keeps the running game (or makes a level-1 hero). Play from the start below always uses it.", w - 16)) { ui.text(lx, ry, l, DIM); ry += 12; }
    ry += 6;
    ui.label(lx, ry + 3, "Class");
    { std::vector<std::string> ids{"(campaign default)"}; for (auto& c : gd->classOrder) ids.push_back(c);
      if (ui.choice(fx, ry, 150, play.cls.empty() ? "(campaign default)" : play.cls)) { int cur = 0; for (size_t i = 0; i < ids.size(); i++) if (ids[i] == play.cls) cur = (int)i;
          openListPicker("Hero class for playtests", ids, cur, [this, ids](int i) { play.cls = i <= 0 ? "" : ids[i]; play.dirty = true; }); } }
    ry += 22;
    ui.label(lx, ry + 3, "Level"); if (ui.intField(fx, ry, 60, play.level, 1, 30)) play.dirty = true;
    ui.label(fx + 76, ry + 3, "Gold"); if (ui.intField(fx + 112, ry, 80, play.gold, 0, 1000000)) play.dirty = true; ry += 22;
    ui.label(lx, ry + 3, "Flags"); if (ui.textField(fx, ry, fw, play.flags, 400)) play.dirty = true;
    flagPicker(fx + fw + 4, ry, 52, false, [this](const std::string& f) { play.flags = trim(play.flags).empty() ? f : trim(play.flags) + "," + f; play.dirty = true; }, "+ flag");
    if (ui.hover(lx, ry, 80, 18)) ui.tip("flag,flag=2 - as if the story had already set them (q_main=3 jumps the main quest ahead)"); ry += 22;
    ui.label(lx, ry + 3, "Items"); if (ui.textField(fx, ry, fw, play.items, 400)) play.dirty = true;
    if (ui.button(fx + fw + 4, ry, 52, 16, "+ item")) { std::vector<std::string> ids; for (auto& it : gd->items) ids.push_back(it.first);
        openListPicker("Add an item to the playtest pack", ids, 0, [this, ids](int i) { if (i < 0 || i >= (int)ids.size()) return; play.items = trim(play.items).empty() ? ids[i] + ":1" : trim(play.items) + "," + ids[i] + ":1"; play.dirty = true; }); }
    if (ui.hover(lx, ry, 80, 18)) ui.tip("id:count,id:count - on top of the class gear"); ry += 22;
    ui.label(lx, ry + 3, "Companions"); if (ui.textField(fx, ry, fw, play.party, 200)) play.dirty = true;
    if (ui.button(fx + fw + 4, ry, 52, 16, "+ class")) { std::vector<std::string> ids; for (auto& c : gd->classOrder) ids.push_back(c);
        openListPicker("Companion who joins at the start", ids, 0, [this, ids](int i) { if (i < 0 || i >= (int)ids.size()) return; play.party = trim(play.party).empty() ? ids[i] : trim(play.party) + "," + ids[i]; play.dirty = true; }); }
    if (ui.hover(lx, ry, 80, 18)) ui.tip("class ids, comma separated - like join= in dialogue (class or class:Name)"); ry += 24;
    // what is wrong with it (checked live)
    std::vector<std::string> probs;
    for (auto& f : split(play.items, ',')) { std::string id = trim(split(trim(f), ':')[0]); if (!id.empty() && !gd->item(id)) probs.push_back("unknown item '" + id + "'"); }
    for (auto& c : split(play.party, ',')) { std::string id = trim(split(trim(c), ':')[0]); if (!id.empty() && !gd->cls(id)) probs.push_back("unknown class '" + id + "'"); }
    if (!play.cls.empty() && !gd->cls(play.cls)) probs.push_back("unknown class '" + play.cls + "'");
    for (auto& p : probs) { ui.text(lx, ry, p, DANGER); ry += 13; }
    if (play.level > 1) for (auto& l : font->wrap(fmt("Level %d: the level-up screens (ranks, stats) open as the game starts.", play.level), w - 16)) { ui.text(lx, ry, l, DIM); ry += 12; }
    int by = y + h - 24;
    if (ui.button(lx, by, 70, 18, "Save", play.dirty, false, OK)) { if (savePlayState()) say("Saved playtest.txt"); }
    if (ui.button(lx + 76, by, 150, 18, "Play from the start", !camp.startMap.empty() && probs.empty(), false, OK)) {
        if (camp.dirty) saveCampaignForm();
        if (!saveAllDirty()) { say("Could not save everything."); return; }
        int sx = camp.startX, sy = camp.startY; playRequest = true; playMap = camp.startMap; playX = sx; playY = sy; playFresh = true; changed = true; wantsClose = true;
    }
    if (ui.hover(lx + 76, by, 150, 18)) ui.tip("A fresh game with this state, on the campaign's start tile (no intro)");
}

// ---------------------------------------------------------------- export (.zip)
static uint32_t crc32Of(const std::string& d) {
    static uint32_t table[256]; static bool init = false;
    if (!init) { for (uint32_t i = 0; i < 256; i++) { uint32_t c = i; for (int k = 0; k < 8; k++) c = c & 1 ? 0xEDB88320u ^ (c >> 1) : c >> 1; table[i] = c; } init = true; }
    uint32_t c = 0xFFFFFFFFu; for (unsigned char ch : d) c = table[(c ^ ch) & 0xFF] ^ (c >> 8); return c ^ 0xFFFFFFFFu;
}
static void put16(std::string& o, uint32_t v) { o += (char)(v & 0xFF); o += (char)((v >> 8) & 0xFF); }
static void put32(std::string& o, uint32_t v) { put16(o, v & 0xFFFF); put16(o, v >> 16); }
// a minimal ZIP writer: every file "stored" (method 0). Fine for text maps and small images; any unzip tool reads it.
bool writeStoredZip(const std::string& path, const std::vector<std::pair<std::string, std::string>>& files) {
    std::string out, central; std::time_t now = std::time(nullptr); std::tm* lt = std::localtime(&now);
    uint16_t dtime = lt ? (uint16_t)((lt->tm_hour << 11) | (lt->tm_min << 5) | (lt->tm_sec / 2)) : 0, ddate = lt ? (uint16_t)(((lt->tm_year - 80) << 9) | ((lt->tm_mon + 1) << 5) | lt->tm_mday) : 0x21;
    for (auto& f : files) {
        uint32_t crc = crc32Of(f.second), size = (uint32_t)f.second.size(), off = (uint32_t)out.size();
        put32(out, 0x04034b50); put16(out, 10); put16(out, 0x0800); put16(out, 0); put16(out, dtime); put16(out, ddate); put32(out, crc); put32(out, size); put32(out, size); put16(out, (uint32_t)f.first.size()); put16(out, 0);
        out += f.first; out += f.second;
        put32(central, 0x02014b50); put16(central, 20); put16(central, 10); put16(central, 0x0800); put16(central, 0); put16(central, dtime); put16(central, ddate); put32(central, crc); put32(central, size); put32(central, size);
        put16(central, (uint32_t)f.first.size()); put16(central, 0); put16(central, 0); put16(central, 0); put16(central, 0); put32(central, 0); put32(central, off); central += f.first;
    }
    uint32_t cdOff = (uint32_t)out.size(); out += central;
    put32(out, 0x06054b50); put16(out, 0); put16(out, 0); put16(out, (uint32_t)files.size()); put16(out, (uint32_t)files.size()); put32(out, (uint32_t)central.size()); put32(out, cdOff); put16(out, 0);
    return writeFile(path, out);
}

bool Editor::exportCampaign(std::string& outPath, int& fileCount, std::string& err) {
    if (!saveAllDirty()) { err = "could not save everything first"; return false; }
    if (camp.dirty) saveCampaignForm();
    std::error_code ec; fs::path root(dataRoot);
    if (!fs::is_directory(root, ec)) { err = "no campaign folder"; return false; }
    std::string id = campaignId.empty() ? "campaign" : campaignId;
    std::vector<std::pair<std::string, std::string>> files; size_t total = 0;
    for (auto it = fs::recursive_directory_iterator(root, ec); it != fs::recursive_directory_iterator(); it.increment(ec)) {
        if (ec) break; if (!it->is_regular_file()) continue;
        std::string rel = fs::relative(it->path(), root, ec).generic_string(); if (ec || rel.empty()) continue;
        if (rel == "playtest.txt" || rel.find("/.") != std::string::npos || rel[0] == '.') continue;   // editor-only / hidden files stay home
        std::string data; if (!readFile(it->path().string(), data)) continue;
        total += data.size(); if (total > 200u * 1024 * 1024) { err = "campaign is over 200 MB"; return false; }
        files.push_back({id + "/" + rel, data});
    }
    std::sort(files.begin(), files.end());
    std::string readme = "Triffall campaign: " + camp.name + (camp.version.empty() ? "" : " v" + camp.version) + (camp.author.empty() ? "" : " by " + camp.author) +
        "\r\n\r\nTo install: unzip this into Triffall's assets/campaigns/ folder, so that assets/campaigns/" + id + "/campaign.txt exists.\r\n"
        "It then has its own icon on the desktop and shows up in New Game.\r\n";
    files.insert(files.begin(), {id + "/README_INSTALL.txt", readme});
    std::string dir = exportDir.empty() ? assetDir + "/../exports" : exportDir; fs::create_directories(dir, ec);
    std::string ver = camp.version.empty() ? "" : "_" + camp.version; for (auto& c : ver) if (!isalnum((unsigned char)c) && c != '.' && c != '_' && c != '-') c = '_';
    outPath = (fs::path(dir) / (id + ver + ".zip")).string();
    if (!writeStoredZip(outPath, files)) { err = "could not write " + outPath; return false; }
    fileCount = (int)files.size(); lastExport = outPath; return true;
}

void Editor::drawExportPanel(int x, int y, int w, int h, InputState& in) {
    ui.panel(x, y, w, h); ui.header(x, y, w, "Share this campaign"); helpBtn(x + w - 16, y + 1, "campaign.export", "tab.campaign");
    int lx = x + 8, ry = y + 24;
    for (auto& l : font->wrap("Export packs the campaign folder (maps, dialogue, data, art) into one .zip with install notes. Others unzip it into assets/campaigns/.", w - 16)) { ui.text(lx, ry, l, DIM); ry += 13; }
    ry += 4;
    bool base = campaignId == "triffall" || campaignId.empty();
    if (ui.button(lx, ry, 120, 20, "Export .zip", !base, false, OK)) {
        std::string out, err; int n = 0;
        if (exportCampaign(out, n, err)) say(fmt("Exported %d files to ", n) + out); else say("Export failed: " + err);
    }
    if (base && ui.hover(lx, ry, 120, 20)) ui.tip("The base game is not a campaign - make or open one first (campaign picker at the top)");
    if (!lastExport.empty() && ui.button(lx + 126, ry, 100, 20, "Open folder")) { if (ui.req) ui.req->openURL = fs::path(lastExport).parent_path().string(); }
    ry += 26;
    if (!lastExport.empty()) { ui.text(lx, ry, "Last export: " + fs::path(lastExport).filename().string(), OK); if (ui.hover(lx, ry, w - 16, 13)) ui.tip(lastExport); ry += 14;
        for (auto& l : font->wrap("in the exports folder next to your saves (Open folder).", w - 16)) { ui.text(lx, ry, l, DIM); ry += 12; } }
    (void)in;
}
