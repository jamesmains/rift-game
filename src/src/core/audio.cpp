#include "audio.h"
#include "util.h"
#include <filesystem>
#include <algorithm>

Audio gAudio;

void Audio::init(AudioBackend* b, const std::string& assetDir) {
    be = b; dir = assetDir + "/audio"; defsPath = assetDir + "/data/sounds.txt";
    if (!fileExists(defsPath)) writeFile(defsPath, defaultDefsText());
    loadDefs();
}

bool Audio::loadDefs() {
    if (be) be->unloadAll();
    defs.clear(); order.clear(); missing.clear(); lastError.clear();
    std::string text;
    if (!readFile(defsPath, text)) { lastError = "cannot read sounds.txt"; return false; }
    for (auto& sec : parseConfig(text)) {
        if (sec.name != "sound" && sec.name != "music" && sec.name != "ambient") continue;
        SoundDef d; d.id = sec.get("id"); if (d.id.empty()) continue;
        d.kind = sec.name == "music" ? "music" : sec.name == "ambient" ? "ambient" : sec.get("kind", "sfx");
        for (auto& f : split(sec.get("file"), ',')) { std::string s = trim(f); if (!s.empty()) d.files.push_back(s); }
        d.volume = sec.geti("volume", 100); d.pitchVar = sec.geti("pitchvar", 0); d.loop = sec.geti("loop", 1) != 0; d.desc = sec.get("desc");
        if (d.kind == "sfx" && be) for (auto& f : d.files) { int h = be->loadSound(dir + "/" + f); if (h < 0) lastError = "missing audio file: " + f; d.handles.push_back(h); }
        if (!defs.count(d.id)) order.push_back(d.id);
        defs[d.id] = d;
    }
    // if the music that is playing was redefined, restart it so a new file takes effect
    if (!currentMusic.empty()) { std::string m = currentMusic; currentMusic.clear(); if (be) be->stopMusic(); music(m); }
    return true;
}

bool Audio::saveDefs() const {
    std::string s = "# Triffall sound map. [sound] = effect, [music] = track. file= is relative to assets/audio (several files, comma separated, pick at random).\n# volume= percent, pitchvar= +/- percent random pitch (sfx), loop=0/1 (music). Edit in the Editor > Sounds.\n\n";
    for (auto& id : order) {
        auto it = defs.find(id); if (it == defs.end()) continue; const SoundDef& d = it->second;
        s += d.kind == "music" ? "[music]\n" : d.kind == "ambient" ? "[ambient]\n" : "[sound]\n";
        s += "id=" + d.id + "\n";
        std::string files; for (size_t i = 0; i < d.files.size(); i++) files += (i ? "," : "") + d.files[i];
        s += "file=" + files + "\n";
        if (!d.desc.empty()) s += "desc=" + d.desc + "\n";
        if (d.volume != 100) s += "volume=" + std::to_string(d.volume) + "\n";
        if (d.kind != "music" && d.pitchVar) s += "pitchvar=" + std::to_string(d.pitchVar) + "\n";
        if (d.kind == "music" && !d.loop) s += "loop=0\n";
        s += "\n";
    }
    return writeFile(defsPath, s);
}

void Audio::play(const std::string& ev) {
    if (!enabled || ev.empty()) return;
    auto it = defs.find(ev);
    if (it == defs.end() || it->second.kind != "sfx") { if (std::find(missing.begin(), missing.end(), ev) == missing.end()) missing.push_back(ev); return; }
    // the same event fired many times in one frame (area spells) plays once
    if (ev == lastEv && timeAcc - lastSameT < 0.03f) return;
    lastEv = ev; lastSameT = timeAcc;
    recent.push_back(ev); if (recent.size() > 32) recent.erase(recent.begin());
    SoundDef& d = it->second;
    if (!be || d.handles.empty()) return;
    int i = d.handles.size() > 1 ? (int)(rnd() % d.handles.size()) : 0;
    if (d.handles[i] < 0) return;
    float pitch = 1.0f; if (d.pitchVar > 0) pitch = 1.0f + ((int)(rnd() % (2 * d.pitchVar + 1)) - d.pitchVar) / 100.0f;
    be->playSound(d.handles[i], masterV * sfxV * d.volume / 100.0f, pitch);
}

void Audio::music(const std::string& ev) {
    if (!enabled) return;
    std::string want = ev;
    if (!want.empty()) { auto it = defs.find(want); if (it == defs.end() || it->second.kind != "music") { if (std::find(missing.begin(), missing.end(), want) == missing.end()) missing.push_back(want); return; } }
    if (want == currentMusic) { pendingMusic = currentMusic; fadingOut = false; return; }
    pendingMusic = want;
    if (currentMusic.empty()) startMusicNow(want); else fadingOut = true;
}

void Audio::ambient(const std::string& ev) {
    if (!enabled) return;
    std::string want = ev;
    if (!want.empty()) { auto it = defs.find(want); if (it == defs.end() || it->second.kind != "ambient") want.clear(); }
    if (want == currentAmbient) { pendingAmbient = want; ambFadingOut = false; return; }
    pendingAmbient = want; ambFadingOut = true;
}

void Audio::startMusicNow(const std::string& ev) {
    currentMusic = ev; pendingMusic = ev; fade = 0.0f; fadingOut = false;
    if (ev.empty()) { if (be) be->stopMusic(); return; }
    recent.push_back("music:" + ev); if (recent.size() > 32) recent.erase(recent.begin());
    const SoundDef& d = defs[ev];
    if (be) { if (d.files.empty() || !be->playMusic(dir + "/" + d.files[rnd() % d.files.size()], 0.0f, d.loop)) { lastError = "cannot play music " + ev; currentMusic.clear(); } }
}

void Audio::setVolumes(int master, int musicPct, int sfx, int ambience) {
    masterV = clampi(master, 0, 100) / 100.0f; musicV = clampi(musicPct, 0, 100) / 100.0f; sfxV = clampi(sfx, 0, 100) / 100.0f; ambV = clampi(ambience, 0, 100) / 100.0f;
}

void Audio::update(float dt) {
    timeAcc += dt;
    if (fadingOut) { fade -= dt * 2.5f; if (fade <= 0) { fade = 0; startMusicNow(pendingMusic); } }
    else if (fade < 1) fade = std::min(1.0f, fade + dt * 1.5f);
    // ambient: fade out, swap, fade in
    if (ambFadingOut) { ambFade -= dt * 1.2f; if (ambFade <= 0) { ambFade = 0; ambFadingOut = false; currentAmbient = pendingAmbient;
            if (be) { be->stopAmbient(); if (!currentAmbient.empty()) { const SoundDef& d = defs[currentAmbient]; if (d.files.empty() || !be->playAmbient(dir + "/" + d.files[rnd() % d.files.size()], 0.0f)) lastError = "cannot play ambient " + currentAmbient; } }
            if (!currentAmbient.empty()) { recent.push_back("ambient:" + currentAmbient); if (recent.size() > 32) recent.erase(recent.begin()); } } }
    else if (!currentAmbient.empty() && ambFade < 1) ambFade = std::min(1.0f, ambFade + dt * 0.8f);
    if (be && !currentAmbient.empty()) { auto it = defs.find(currentAmbient); float v = it == defs.end() ? 1.0f : it->second.volume / 100.0f; be->setAmbientVolume(masterV * ambV * v * ambFade); }
    if (be) {
        if (!currentMusic.empty()) {
            auto it = defs.find(currentMusic);
            float v = it == defs.end() ? 1.0f : it->second.volume / 100.0f;
            be->setMusicVolume(masterV * musicV * v * fade);
            if (it != defs.end() && !it->second.loop && !be->musicPlaying() && fade >= 1) currentMusic.clear(); // one-shot track ended
        }
        be->update();
    }
}

void Audio::refresh(SoundDef& d) {
    d.handles.clear();
    if (d.kind == "sfx" && be) for (auto& f : d.files) d.handles.push_back(be->loadSound(dir + "/" + f));
    if (d.kind == "music" && d.id == currentMusic) { currentMusic.clear(); if (be) be->stopMusic(); music(d.id); }
}
bool Audio::importFile(const std::string& srcPath, const std::string& sub, std::string& relOut) {
    std::error_code ec; std::filesystem::path src(srcPath);
    if (!std::filesystem::is_regular_file(src, ec)) { lastError = "file not found: " + srcPath; return false; }
    std::string ext = toLower(src.extension().string());
    if (ext != ".wav" && ext != ".ogg" && ext != ".mp3") { lastError = "only .wav, .ogg and .mp3 files are supported"; return false; }
    std::filesystem::create_directories(dir + "/" + sub, ec);
    std::string name = src.filename().string(); std::string dst = dir + "/" + sub + "/" + name;
    if (std::filesystem::exists(dst, ec) && !std::filesystem::equivalent(src, dst, ec)) { int n = 2; std::string stem = src.stem().string(); while (std::filesystem::exists(dir + "/" + sub + "/" + stem + "_" + std::to_string(n) + ext, ec)) n++; name = stem + "_" + std::to_string(n) + ext; dst = dir + "/" + sub + "/" + name; }
    if (!std::filesystem::equivalent(src, dst, ec)) { std::filesystem::copy_file(src, dst, std::filesystem::copy_options::overwrite_existing, ec); if (ec) { lastError = "copy failed: " + ec.message(); return false; } }
    relOut = sub + "/" + name; return true;
}

std::vector<std::string> Audio::availableFiles(const std::string& sub) const {
    std::vector<std::string> out; std::error_code ec;
    std::string base = dir + (sub.empty() ? "" : "/" + sub);
    for (auto& e : std::filesystem::recursive_directory_iterator(base, ec)) {
        if (!e.is_regular_file()) continue;
        std::string ext = toLower(e.path().extension().string());
        if (ext != ".wav" && ext != ".ogg" && ext != ".mp3") continue;
        std::string rel = std::filesystem::relative(e.path(), dir, ec).generic_string();
        out.push_back(rel);
    }
    std::sort(out.begin(), out.end());
    return out;
}

// 1.0.7: the loops were mixed about as loud as the music (wind and rain are broadband noise, so they sound louder still);
// these defaults sit them well under it. Settings > Ambience volume scales them all.
static int ambDefaultVolume(const std::string& id) {
    if (id == "amb_wind" || id == "amb_rain") return 30;
    if (id == "amb_machine" || id == "amb_rift" || id == "amb_town") return 35;
    return 45;   // cave drips, swamp frogs, night insects are quieter recordings
}

std::string Audio::defaultDefsText() {
    struct E { const char* id; const char* file; const char* desc; int vol; int pv; };
    static const E sfx[] = {
        {"ui_move", "sfx/ui_move.wav", "menu cursor moves", 60, 0}, {"ui_confirm", "sfx/ui_confirm.wav", "menu confirm", 80, 0}, {"ui_click", "sfx/ui_click.wav", "mouse click in a menu (soft)", 45, 4}, {"ui_cancel", "sfx/ui_cancel.wav", "menu back", 80, 0},
        {"ui_open", "sfx/ui_open.wav", "a game screen opens", 80, 0}, {"ui_close", "sfx/ui_close.wav", "a game screen closes", 80, 0}, {"ui_error", "sfx/ui_error.wav", "can't do that", 80, 0},
        {"ui_page", "sfx/ui_page.wav", "tab / page change", 60, 0}, {"text_blip", "sfx/text_blip.wav", "dialogue line appears", 50, 10},
        {"boot", "sfx/boot.wav", "desktop boot chime", 80, 0}, {"shutdown", "sfx/shutdown.wav", "desktop shut down", 80, 0}, {"desktop_click", "sfx/desktop_click.wav", "desktop icon / button click", 70, 0},
        {"desktop_open", "sfx/desktop_open.wav", "desktop window opens", 80, 0}, {"desktop_close", "sfx/desktop_close.wav", "desktop window closes", 80, 0},
        {"step", "sfx/step1.wav,sfx/step2.wav,sfx/step3.wav,sfx/step4.wav", "party footstep", 45, 8},
        {"door_open", "sfx/door_open.wav", "door opens", 90, 5}, {"door_locked", "sfx/door_locked.wav", "locked door / chest", 80, 0}, {"chest_open", "sfx/chest_open.wav", "chest opened", 90, 0},
        {"pickup", "sfx/pickup.wav", "item received", 80, 5}, {"gold", "sfx/gold.wav", "gold received", 80, 5}, {"equip", "sfx/equip.wav", "equip / unequip", 80, 5}, {"potion", "sfx/potion.wav", "consumable used", 80, 5},
        {"save", "sfx/save.wav", "game saved", 90, 0}, {"levelup", "sfx/levelup.wav", "level up", 90, 0}, {"quest", "sfx/quest.wav", "quest / flag update", 80, 0}, {"teleport", "sfx/teleport.wav", "map transition / recall", 70, 0},
        {"rest", "sfx/rest.wav", "resting at an inn", 80, 0}, {"shop_buy", "sfx/shop_buy.wav", "bought an item", 80, 0}, {"shop_sell", "sfx/shop_sell.wav", "sold an item", 80, 0}, {"ambush", "sfx/ambush.wav", "random encounter", 90, 0},
        {"combat_start", "sfx/combat_start.wav", "combat begins", 90, 0}, {"turn_player", "sfx/turn_player.wav", "a party member's turn", 60, 0}, {"turn_enemy", "sfx/turn_enemy.wav", "an enemy's turn", 50, 0},
        {"attack_melee", "sfx/attack_melee.wav", "melee swing", 90, 10}, {"attack_bow", "sfx/attack_bow.wav", "bow / thrown shot", 90, 8}, {"attack_gun", "sfx/attack_gun.wav", "gunshot", 90, 6}, {"attack_laser", "sfx/attack_laser.wav", "energy weapon", 90, 6},
        {"hit", "sfx/hit.wav", "damage lands", 90, 10}, {"crit", "sfx/crit.wav", "critical hit", 100, 5}, {"miss", "sfx/miss.wav", "attack misses", 70, 10}, {"block", "sfx/block.wav", "defend / no damage", 70, 5},
        {"spell_cast", "sfx/spell_cast.wav", "generic spell", 90, 5}, {"spell_fire", "sfx/spell_fire.wav", "fire spell", 90, 5}, {"spell_ice", "sfx/spell_ice.wav", "cold spell", 90, 5}, {"spell_lightning", "sfx/spell_lightning.wav", "shock spell", 90, 5},
        {"spell_poison", "sfx/spell_poison.wav", "poison spell", 90, 5}, {"spell_heal", "sfx/spell_heal.wav", "healing", 90, 0}, {"spell_buff", "sfx/spell_buff.wav", "buff / debuff", 80, 5}, {"explosion", "sfx/explosion.wav", "grenade / area blast", 100, 5},
        {"death_enemy", "sfx/death_enemy.wav", "enemy slain", 90, 10}, {"death_party", "sfx/death_party.wav", "party member knocked out", 90, 0}, {"flee", "sfx/flee.wav", "fled from combat", 80, 0},
        {"victory", "sfx/victory.wav", "combat won (jingle)", 80, 0}, {"defeat", "sfx/defeat.wav", "party wiped", 80, 0}, {"grenade_throw", "sfx/grenade_throw.wav", "thrown item", 80, 5}, {"reload", "sfx/reload.wav", "ammo / misc click", 70, 0},
        {"unlock", "sfx/unlock.wav", "unlock spell / key used", 80, 0}, {"thunder", "sfx/thunder.wav", "storm thunder", 80, 10}, {"craft", "sfx/craft.wav", "item crafted", 85, 0}, {"achievement", "sfx/achievement.wav", "achievement unlocked", 80, 0},
        {"mine_click", "sfx/mine_click.wav", "minesweeper / solitaire click", 60, 5}, {"mine_boom", "sfx/mine_boom.wav", "minesweeper mine", 80, 0}, {"card", "sfx/card.wav", "solitaire card", 60, 10}, {"banter", "sfx/banter.wav", "companion banter", 50, 0}, {"travel", "sfx/travel.wav", "fast travel", 80, 0}, {"light", "sfx/light.wav", "light spell", 80, 0}, {"stealth", "sfx/stealth.wav", "stealth spell", 80, 0}, {"detect", "sfx/detect.wav", "detect spell", 80, 0},
    };
    static const E mus[] = {
        {"title", "music/title.ogg", "desktop / main menu", 70, 1}, {"overworld", "music/overworld.ogg", "overworld travel", 70, 1}, {"town", "music/town.ogg", "towns and villages", 70, 1}, {"forest", "music/forest.ogg", "wilderness locations", 70, 1},
        {"dungeon", "music/dungeon.ogg", "caves, crypts, castles", 70, 1}, {"tech", "music/tech.ogg", "tech ruins, the ship", 70, 1}, {"combat", "music/combat.ogg", "normal combat", 75, 1}, {"boss", "music/boss.ogg", "boss combat", 80, 1},
        {"fanfare", "music/victory.ogg", "after a won fight (plays once)", 70, 0}, {"gameover", "music/gameover.ogg", "game over (plays once)", 70, 0},
        {"brasswick", "music/brasswick.ogg", "Brasswick", 70, 1}, {"ashfall", "music/ashfall.ogg", "Ashfall's ruins", 70, 1}, {"murkwater", "music/murkwater.ogg", "Murkwater", 70, 1},
        {"sunscar", "music/sunscar.ogg", "Sunscar Outpost", 70, 1}, {"frosthold", "music/frosthold.ogg", "Frosthold Shrine", 70, 1}, {"crypt", "music/crypt.ogg", "crypts and barrows", 70, 1},
        {"swamp", "music/swamp.ogg", "the Drowning Marsh", 70, 1}, {"rift", "music/rift.ogg", "the Scar", 75, 1}, {"overworld_night", "music/overworld_night.ogg", "overworld at night", 65, 1},
        {"ending", "music/ending.ogg", "the epilogue and credits", 75, 1},
    };
    std::string s = "# Triffall sound map. [sound] = effect, [music] = track. file= is relative to assets/audio (several files, comma separated, pick at random).\n# volume= percent, pitchvar= +/- percent random pitch (sfx), loop=0/1 (music). Edit in the Editor > Sounds.\n#\n# Maps can set music=<track> in their [map] header; items/spells can set sound=<event>; creatures sound=<event> for their death.\n\n";
    for (auto& e : sfx) { s += fmt("[sound]\nid=%s\nfile=%s\ndesc=%s\n", e.id, e.file, e.desc); if (e.vol != 100) s += fmt("volume=%d\n", e.vol); if (e.pv) s += fmt("pitchvar=%d\n", e.pv); s += "\n"; }
    for (auto& e : mus) { s += fmt("[music]\nid=%s\nfile=%s\ndesc=%s\n", e.id, e.file, e.desc); if (e.vol != 100) s += fmt("volume=%d\n", e.vol); if (!e.pv) s += "loop=0\n"; s += "\n"; }
    static const char* amb[][3] = {{"amb_wind", "ambient/amb_wind.ogg", "wind (overworld, snow)"}, {"amb_cave", "ambient/amb_cave.ogg", "caves and crypts: drips"}, {"amb_swamp", "ambient/amb_swamp.ogg", "the marsh: frogs"},
        {"amb_rain", "ambient/amb_rain.ogg", "rain and storms outdoors"}, {"amb_town", "ambient/amb_town.ogg", "towns by day"}, {"amb_machine", "ambient/amb_machine.ogg", "tech ruins: hum"},
        {"amb_night", "ambient/amb_night.ogg", "outdoors at night: crickets"}, {"amb_rift", "ambient/amb_rift.ogg", "the Scar"}};
    for (auto& e : amb) s += fmt("[ambient]\nid=%s\nfile=%s\ndesc=%s\nvolume=%d\n\n", e[0], e[1], e[2], ambDefaultVolume(e[0]));
    return s;
}
