// Save files: atomic writes with a backup of the previous save, an autosave, versioned saves and upgrading old ones.
#include "game.h"
#include "../core/version.h"
#include "../core/audio.h"
#include <filesystem>
#include <ctime>

namespace fs = std::filesystem;

std::string Game::slotPath(int slot) const { return slot == AUTOSAVE_SLOT ? saveDir + "/autosave.sav" : saveDir + "/slot" + std::to_string(slot + 1) + ".sav"; }

// Write to a temporary file, read it back, keep the old save as .bak, then swap the new one in. A crash or a full disk
// half-way through can no longer leave a slot holding half a save.
bool writeSaveFile(const std::string& path, const std::string& text) {
    std::string tmp = path + ".tmp", check;
    if (!writeFile(tmp, text)) return false;
    std::error_code ec;
    if (!readFile(tmp, check) || check != text) { fs::remove(tmp, ec); return false; }
    // keep the previous save as .bak - but only if it is a whole save: a damaged file must never replace a good backup
    { std::string old; if (readFile(path, old)) { auto secs = parseConfig(old); bool whole = !secs.empty() && secs[0].name == "game"; bool member = false; for (auto& sc : secs) if (sc.name == "member") member = true;
        if (whole && member) fs::copy_file(path, path + ".bak", fs::copy_options::overwrite_existing, ec); } }
    ec.clear(); fs::rename(tmp, path, ec);   // replaces the old file atomically (POSIX, MSVC STL, libc++, libstdc++ on Windows)
    return !ec;                               // on failure the new save is still in .tmp and the old one untouched
}

bool Game::saveToSlot(int slot) {
    if (!running) return false;
    if (!writeSaveFile(slotPath(slot), serialize())) { notify("Could not write the save file!"); return false; }
    refreshSlots(); notify(slot == AUTOSAVE_SLOT ? std::string("Autosaved.") : fmt("Saved to slot %d.", slot + 1)); gAudio.play("save"); return true;
}

bool Game::loadSaveFile(const std::string& path) {
    std::string t; loadError.clear();
    if (!readFile(path, t)) {   // missing main file after an interrupted write: the newest intact copy is .tmp, then .bak
        for (const char* ext : {".tmp", ".bak"}) { std::string alt; if (readFile(path + ext, alt) && deserialize(alt)) { notify("Recovered the save from a backup copy."); log("The save file was missing; it was recovered from a backup copy."); return true; } }
        loadError = "Empty slot."; return false; }
    if (deserialize(t)) return true;
    std::string why = loadError.empty() ? "The save file is damaged." : loadError;
    if (startsWith(why, "This save was made by a newer")) { loadError = why; return false; }   // the backup is no better
    // a damaged save: fall back to the copy kept from the save before it
    std::string b; if (readFile(path + ".bak", b) && deserialize(b)) { loadError.clear(); notify("That save was damaged - loaded the previous save instead."); log("The save file was damaged; the backup from the save before it was loaded."); return true; }
    loadError = why; return false;
}
bool Game::loadFromSlot(int slot) {
    if (!loadSaveFile(slotPath(slot))) { notify(loadError.empty() ? "Save file is corrupt." : loadError); return false; }
    notify(slot == AUTOSAVE_SLOT ? std::string("Loaded the autosave.") : fmt("Loaded slot %d.", slot + 1)); gAudio.play("load"); return true;
}

// Autosave when the party arrives somewhere new (at most once a minute of play), so a crash or a wipe never costs much.
void Game::autosave() {
    if (!running || gameOver || loadingSave || inCombat() || party.members.empty()) return;
    if (lastAutosave >= 0 && playtimeSec - lastAutosave < 60) return;
    lastAutosave = playtimeSec;
    if (writeSaveFile(slotPath(AUTOSAVE_SLOT), serialize())) notify("Autosaved.");
}

std::string Game::crashState() const {
    std::string s = fmt("version %s, campaign %s, map %s (%d,%d), mode %d, day %d, playtime %d s", TRIFFALL_VERSION, campaignId.c_str(), mapId.c_str(), px, py, (int)mode, day, playtimeSec);
    for (auto& m : party.members) s += fmt("\n  %s lvl %d %s hp %d", m.name.c_str(), m.level, m.classId.c_str(), m.hp);
    s += "\nlast messages:";
    for (size_t i = msgLog.size() > 10 ? msgLog.size() - 10 : 0; i < msgLog.size(); i++) s += "\n  " + msgLog[i];
    return s;
}
void Game::emergencySave() { if (running && !loadingSave && !party.members.empty()) writeFile(saveDir + "/crash_recovery.sav", serialize()); }

// ---- upgrading saves from older versions ----
// assets/data/migrations.txt lists ids that were renamed or removed after release:
//   [rename]
//   item=old_id>new_id      (also equipment, crafted items and recipes' results)
//   flag=old_flag>new_flag
//   map=old_map>new_map
//   spell=old_spell>new_spell
// Any id the game no longer knows is dropped with a note in the log rather than breaking the load.
void Game::migrateSave(std::vector<ConfigSection>& secs, int fromFormat) {
    std::map<std::string, std::string> item, flag, mapr, spell;
    for (auto& sec : mergedConfig("migrations.txt")) if (sec.name == "rename") for (auto& kv : sec.kv) {
        auto p = split(kv.second, '>'); if (p.size() != 2) continue; std::string a = trim(p[0]), b = trim(p[1]);
        if (kv.first == "item") item[a] = b; else if (kv.first == "flag") flag[a] = b; else if (kv.first == "map") mapr[a] = b; else if (kv.first == "spell") spell[a] = b;
    }
    auto ren = [](const std::map<std::string, std::string>& m, const std::string& v) { auto it = m.find(v); return it == m.end() ? v : it->second; };
    auto renList = [&](const std::map<std::string, std::string>& m, const std::string& v) { std::string o; for (auto& x : split(v, ',')) { o += (o.empty() ? "" : ",") + ren(m, trim(x)); } return o; };
    for (auto& sec : secs) for (auto& kv : sec.kv) {
        std::string& k = kv.first; std::string& v = kv.second;
        if (sec.name == "game") {
            if (k == "inv") { auto p = split(v, ':'); v = ren(item, p[0]) + (p.size() > 1 ? ":" + p[1] : ""); }
            else if (k == "map" || k == "visited") v = ren(mapr, v);
            else if (startsWith(k, "flag.")) k = "flag." + ren(flag, k.substr(5));
        } else if (sec.name == "member") {
            if (startsWith(k, "equip")) v = ren(item, v);
            else if (k == "spells" || k == "favs") v = renList(k == "spells" ? spell : item, v);
        } else if (sec.name == "dynitem" && k == "spec") { size_t bar = v.find('|'); v = ren(item, v.substr(0, bar)) + (bar == std::string::npos ? "" : v.substr(bar)); }
        else if (sec.name == "explored" && k == "map") v = ren(mapr, v);
        else if (sec.name == "extra" && k == "town") v = ren(mapr, v);
    }
    // format steps. 1/2 -> 3 (1.0): nothing to convert; experience is no longer split, which only changes what comes next.
    (void)fromFormat;
}
// 1.5.0 (#86): [learn] recipe=id|condition - a recipe that a patch made known=0 (taught by a dialogue now) is given to a
// loaded save where the condition already holds, so nobody is left with a trial they can no longer craft.
void Game::applyLearnMigrations() {
    for (auto& sec : mergedConfig("migrations.txt")) if (sec.name == "learn") for (auto& kv : sec.kv) {
        if (kv.first != "recipe") continue;
        auto p = split(kv.second, '|'); std::string id = trim(p[0]); if (id.empty()) continue;
        if (p.size() < 2 || evalCond(trim(p[1]))) learnRecipes(id, false);
    }
}
