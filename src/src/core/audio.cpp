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

std::string Audio::defaultDefsText() {
    struct E { const char* id; const char* file; const char* desc; int vol; int pv; };
    static const E sfx[] = {
        {"ui_move", "sfx/ui_move.wav", "menu cursor moves", 60, 0}, {"ui_confirm", "sfx/ui_confirm.wav", "menu confirm", 80, 0}, {"ui_click", "sfx/ui_click.wav", "mouse click in a menu (soft)", 45, 4},
        {"ui_cancel", "sfx/ui_cancel.wav", "menu back", 80, 0}, {"ui_open", "sfx/ui_open.wav", "a game screen opens", 80, 0}, {"ui_close", "sfx/ui_close.wav", "a game screen closes", 80, 0},
        {"ui_error", "sfx/ui_error.wav", "can't do that", 80, 0}, {"ui_page", "sfx/ui_page.wav", "tab / page change", 60, 0}, {"text_blip", "sfx/text_blip.wav", "dialogue line appears", 50, 10},
        {"boot", "sfx/boot.wav", "desktop boot chime", 80, 0}, {"shutdown", "sfx/shutdown.wav", "desktop shut down", 80, 0}, {"desktop_click", "sfx/desktop_click.wav", "desktop icon / button click", 70, 0},
        {"desktop_open", "sfx/desktop_open.wav", "desktop window opens", 80, 0}, {"desktop_close", "sfx/desktop_close.wav", "desktop window closes", 80, 0},
        {"step", "sfx/pixel_combat/boots_on_generic_ground_1_1.ogg,sfx/pixel_combat/boots_on_generic_ground_1_2.ogg,sfx/pixel_combat/boots_on_generic_ground_1_3.ogg,sfx/pixel_combat/boots_on_generic_ground_2_1.ogg,sfx/pixel_combat/boots_on_generic_ground_2_2.ogg,sfx/pixel_combat/boots_on_generic_ground_2_3.ogg", "party footstep on ground whose tile kind has no step= (dirt, paths, carpets...)", 45, 8},
        {"door_open", "sfx/door_open.wav", "door opens", 90, 5}, {"door_locked", "sfx/door_locked.wav", "locked door / chest", 80, 0}, {"chest_open", "sfx/chest_open.wav", "chest opened", 90, 0},
        {"pickup", "sfx/pickup.wav", "item received", 80, 5}, {"gold", "sfx/gold.wav", "gold received", 80, 5}, {"equip", "sfx/equip.wav", "equip / unequip", 80, 5},
        {"potion", "sfx/potion.wav", "consumable used", 80, 5}, {"save", "sfx/save.wav", "game saved", 90, 0}, {"levelup", "sfx/levelup.wav", "level up", 90, 0},
        {"quest", "sfx/quest.wav", "quest / flag update", 80, 0}, {"teleport", "sfx/pixel_combat/teleport_downer_1.ogg,sfx/pixel_combat/teleport_downer_2.ogg", "map transition / recall", 70, 0},
        {"rest", "sfx/rest.wav", "resting at an inn", 80, 0}, {"shop_buy", "sfx/shop_buy.wav", "bought an item", 80, 0}, {"shop_sell", "sfx/shop_sell.wav", "sold an item", 80, 0},
        {"ambush", "sfx/ambush.wav", "random encounter", 90, 0}, {"combat_start", "sfx/combat_start.wav", "combat begins", 90, 0}, {"turn_player", "sfx/turn_player.wav", "a party member's turn", 60, 0},
        {"turn_enemy", "sfx/turn_enemy.wav", "an enemy's turn", 50, 0},
        {"attack_melee", "sfx/pixel_combat/simple_whoosh_1.ogg,sfx/pixel_combat/simple_whoosh_2.ogg,sfx/pixel_combat/simple_whoosh_3.ogg,sfx/pixel_combat/bamboo_whip_1.ogg,sfx/pixel_combat/bamboo_whip_2.ogg,sfx/pixel_combat/bamboo_whip_3.ogg", "melee swing", 90, 10},
        {"attack_bow", "sfx/pixel_combat/whoosh_sweep_1.ogg,sfx/pixel_combat/whoosh_sweep_2.ogg,sfx/pixel_combat/whoosh_sweep_3.ogg", "bow / thrown shot", 90, 8},
        {"attack_gun", "sfx/pixel_combat/hollow_point_1.ogg,sfx/pixel_combat/hollow_point_2.ogg,sfx/pixel_combat/hollow_point_3.ogg", "gunshot", 90, 6},
        {"attack_laser", "sfx/pixel_combat/laser_shot_1.ogg,sfx/pixel_combat/laser_shot_2.ogg,sfx/pixel_combat/mecha_laser_pistol_1.ogg,sfx/pixel_combat/mecha_laser_pistol_2.ogg", "energy weapon", 90, 6},
        {"hit", "sfx/pixel_combat/gut_punch_1.ogg,sfx/pixel_combat/gut_punch_2.ogg,sfx/pixel_combat/gut_kick_1.ogg,sfx/pixel_combat/gut_kick_2.ogg,sfx/pixel_combat/strong_punch_1.ogg,sfx/pixel_combat/strong_punch_2.ogg", "damage lands", 90, 10},
        {"crit", "sfx/pixel_combat/crunchy_burst_1.ogg,sfx/pixel_combat/crunchy_burst_2.ogg,sfx/pixel_combat/crunch_kick_1.ogg,sfx/pixel_combat/crunch_kick_2.ogg,sfx/pixel_combat/mecha_gore_cruncher_1.ogg,sfx/pixel_combat/mecha_gore_cruncher_2.ogg", "critical hit", 100, 5},
        {"miss", "sfx/pixel_combat/sand_swipe_1.ogg,sfx/pixel_combat/sand_swipe_2.ogg,sfx/pixel_combat/sand_swipe_3.ogg", "attack misses", 70, 10},
        {"block", "sfx/pixel_combat/kick_block_1.ogg,sfx/pixel_combat/kick_block_2.ogg,sfx/pixel_combat/kick_block_3.ogg,sfx/pixel_combat/deflected_hit_1.ogg,sfx/pixel_combat/deflected_hit_2.ogg", "defend / no damage", 70, 5},
        {"spell_cast", "sfx/pixel_combat/casting_buff_1.ogg,sfx/pixel_combat/casting_buff_2.ogg,sfx/pixel_combat/casting_buff_3.ogg,sfx/pixel_combat/hollow_spell_1.ogg,sfx/pixel_combat/hollow_spell_2.ogg", "generic spell", 90, 5},
        {"spell_fire", "sfx/pixel_combat/flame_ball_1.ogg,sfx/pixel_combat/flame_ball_2.ogg,sfx/pixel_combat/flame_ball_3.ogg,sfx/pixel_combat/fire_hit_1.ogg,sfx/pixel_combat/fire_hit_2.ogg", "fire spell", 90, 5},
        {"spell_ice", "sfx/spell_ice.wav", "cold spell", 90, 5},
        {"spell_lightning", "sfx/pixel_combat/voltaic_blast_1.ogg,sfx/pixel_combat/voltaic_blast_2.ogg,sfx/pixel_combat/electric_hit_1.ogg,sfx/pixel_combat/electric_hit_2.ogg", "shock spell", 90, 5},
        {"spell_poison", "sfx/pixel_combat/slime_ball_1.ogg,sfx/pixel_combat/slime_ball_2.ogg,sfx/pixel_combat/slime_ball_3.ogg", "poison spell", 90, 5},
        {"spell_heal", "sfx/pixel_combat/healing_gusts_1.ogg,sfx/pixel_combat/healing_gusts_2.ogg,sfx/pixel_combat/healing_gusts_3.ogg", "healing", 90, 0},
        {"spell_buff", "sfx/pixel_combat/generic_buff_1.ogg,sfx/pixel_combat/generic_buff_2.ogg,sfx/pixel_combat/generic_buff_3.ogg", "buff / debuff", 80, 5},
        {"explosion", "sfx/pixel_combat/pyro_burst_1.ogg,sfx/pixel_combat/pyro_burst_2.ogg,sfx/pixel_combat/bass_hit_1.ogg,sfx/pixel_combat/bass_hit_2.ogg,sfx/pixel_combat/cruncher_1.ogg,sfx/pixel_combat/cruncher_2.ogg", "grenade / area blast", 100, 5},
        {"death_enemy", "sfx/pixel_combat/thud_1.ogg,sfx/pixel_combat/thud_2.ogg,sfx/pixel_combat/thud_3.ogg,sfx/pixel_combat/crunching_1.ogg,sfx/pixel_combat/crunching_2.ogg", "enemy slain (creatures without sound=)", 90, 10},
        {"death_party", "sfx/death_party.wav", "party member knocked out", 90, 0}, {"flee", "sfx/pixel_combat/windy_passby_1.ogg,sfx/pixel_combat/windy_passby_2.ogg", "fled from combat", 80, 0},
        {"victory", "sfx/victory.wav", "combat won (jingle)", 80, 0}, {"defeat", "sfx/defeat.wav", "party wiped", 80, 0},
        {"grenade_throw", "sfx/pixel_combat/wind_shaker_1.ogg,sfx/pixel_combat/wind_shaker_2.ogg,sfx/pixel_combat/wind_shaker_3.ogg", "thrown item", 80, 5},
        {"reload", "sfx/pixel_combat/metallic_click_1.ogg,sfx/pixel_combat/metallic_click_2.ogg,sfx/pixel_combat/metallic_click_3.ogg", "ammo / misc click", 70, 0},
        {"unlock", "sfx/pixel_combat/lock_1.ogg,sfx/pixel_combat/lock_2.ogg,sfx/pixel_combat/lock_3.ogg", "unlock spell / key used", 80, 0},
        {"light", "sfx/pixel_combat/aura_rise_1.ogg,sfx/pixel_combat/aura_rise_2.ogg", "light spell", 80, 0},
        {"stealth", "sfx/pixel_combat/underwater_1.ogg,sfx/pixel_combat/underwater_2.ogg", "stealth spell", 80, 0},
        {"detect", "sfx/pixel_combat/complex_rise_1.ogg,sfx/pixel_combat/complex_rise_2.ogg", "detect spell", 80, 0}, {"thunder", "sfx/thunder.wav", "storm thunder", 80, 10},
        {"craft", "sfx/craft.wav", "item crafted", 85, 0}, {"achievement", "sfx/achievement.wav", "achievement unlocked", 80, 0}, {"mine_click", "sfx/mine_click.wav", "minesweeper / solitaire click", 60, 5},
        {"mine_boom", "sfx/mine_boom.wav", "minesweeper mine", 80, 0}, {"card", "sfx/card.wav", "solitaire card", 60, 10}, {"banter", "sfx/banter.wav", "companion banter", 50, 0},
        {"travel", "sfx/travel.wav", "fast travel", 80, 0},
        {"step_grass", "sfx/pixel_combat/boots_on_grass_1.ogg,sfx/pixel_combat/boots_on_grass_2.ogg,sfx/pixel_combat/boots_on_grass_3.ogg,sfx/pixel_combat/boots_on_grass_4.ogg,sfx/pixel_combat/boots_on_grass_5.ogg,sfx/pixel_combat/boots_on_grass_6.ogg", "footstep on grass, forest floor, fields (tile kind step=)", 45, 8},
        {"step_sand", "sfx/pixel_combat/boots_on_sand_1.ogg,sfx/pixel_combat/boots_on_sand_2.ogg,sfx/pixel_combat/boots_on_sand_3.ogg,sfx/pixel_combat/boots_on_sand_4.ogg,sfx/pixel_combat/boots_on_sand_5.ogg,sfx/pixel_combat/boots_on_sand_6.ogg", "footstep on sand, desert, ash and rubble", 45, 8},
        {"step_snow", "sfx/pixel_combat/boots_on_sand_snow_1.ogg,sfx/pixel_combat/boots_on_sand_snow_2.ogg,sfx/pixel_combat/boots_on_sand_snow_3.ogg,sfx/pixel_combat/boots_on_sand_snow_4.ogg,sfx/pixel_combat/boots_on_sand_snow_5.ogg,sfx/pixel_combat/boots_on_sand_snow_6.ogg", "footstep in snow", 45, 8},
        {"step_stone", "sfx/pixel_combat/boots_on_concrete_dungeon_1.ogg,sfx/pixel_combat/boots_on_concrete_dungeon_2.ogg,sfx/pixel_combat/boots_on_concrete_dungeon_3.ogg,sfx/pixel_combat/boots_on_concrete_dungeon_4.ogg,sfx/pixel_combat/boots_on_concrete_dungeon_5.ogg,sfx/pixel_combat/boots_on_concrete_dungeon_6.ogg", "footstep on dungeon stone and cave floors (with an echo)", 45, 8},
        {"step_hard", "sfx/pixel_combat/boots_on_concrete_1.ogg,sfx/pixel_combat/boots_on_concrete_2.ogg,sfx/pixel_combat/boots_on_concrete_3.ogg,sfx/pixel_combat/boots_on_concrete_4.ogg,sfx/pixel_combat/boots_on_concrete_5.ogg,sfx/pixel_combat/boots_on_concrete_6.ogg", "footstep on streets, tiles, concrete and ice", 45, 8},
        {"step_wood", "sfx/pixel_combat/hard_step_1.ogg,sfx/pixel_combat/hard_step_2.ogg,sfx/pixel_combat/hard_step_3.ogg,sfx/pixel_combat/hard_step_4.ogg,sfx/pixel_combat/hard_step_5.ogg,sfx/pixel_combat/hard_step_6.ogg", "footstep on wooden floors, bridges and boardwalks", 45, 8},
        {"step_metal", "sfx/pixel_combat/boots_on_metal_1.ogg,sfx/pixel_combat/boots_on_metal_2.ogg,sfx/pixel_combat/boots_on_metal_3.ogg,sfx/pixel_combat/boots_on_metal_4.ogg,sfx/pixel_combat/boots_on_metal_5.ogg,sfx/pixel_combat/boots_on_metal_6.ogg", "footstep on metal floors and grates", 45, 8},
        {"step_water", "sfx/pixel_combat/sparkly_water_1.ogg,sfx/pixel_combat/sparkly_water_2.ogg,sfx/pixel_combat/sparkly_water_3.ogg,sfx/pixel_combat/sparkly_water_4.ogg", "footstep through shallow water and mire", 45, 8},
        {"death_machine", "sfx/pixel_combat/mecha_limb_explosion_1.ogg,sfx/pixel_combat/mecha_limb_explosion_2.ogg,sfx/pixel_combat/mecha_engine_burst_1.ogg,sfx/pixel_combat/mecha_engine_burst_2.ogg", "a machine is destroyed (drones, bots, turrets, the mech)", 90, 8},
        {"death_spirit", "sfx/pixel_combat/energy_dissipate_1.ogg,sfx/pixel_combat/energy_dissipate_2.ogg,sfx/pixel_combat/energy_dissipate_3.ogg", "a spirit fades (wraiths, wisps, wights)", 90, 8},
        {"death_bones", "sfx/pixel_combat/hit_rattle_1.ogg,sfx/pixel_combat/hit_rattle_2.ogg,sfx/pixel_combat/hit_rattle_3.ogg", "a skeleton falls apart", 90, 8},
        {"death_slime", "sfx/pixel_combat/wet_splash_1.ogg,sfx/pixel_combat/wet_splash_2.ogg,sfx/pixel_combat/wet_splash_3.ogg", "a slime or toad bursts", 90, 8},
        {"death_big", "sfx/pixel_combat/thud_big_1.ogg,sfx/pixel_combat/thud_big_2.ogg,sfx/pixel_combat/thud_big_3.ogg", "a big creature falls (ogres, trolls, bears, dragons, golems)", 90, 8},
        {"death_rift", "sfx/pixel_combat/mecha_manablast_1.ogg,sfx/pixel_combat/mecha_manablast_2.ogg,sfx/pixel_combat/mecha_manablast_3.ogg", "a rift creature is unmade (aliens, demons, horrors)", 90, 8},
        {"spell_summon", "sfx/pixel_combat/noise_summon_1.ogg,sfx/pixel_combat/noise_summon_2.ogg", "a creature is called (Call Wolf, Raise Dead)", 90, 5},
        {"spell_summon_tech", "sfx/pixel_combat/mecha_energy_movement_1.ogg,sfx/pixel_combat/mecha_energy_movement_2.ogg", "a machine is deployed (Deploy Turret)", 90, 5},
        {"spell_dark", "sfx/pixel_combat/dark_energy_sphere_1.ogg,sfx/pixel_combat/dark_energy_sphere_2.ogg,sfx/pixel_combat/dark_energy_sphere_3.ogg", "dark magic (Dark Bolt, Drain, Dark Heal)", 90, 5},
        {"spell_rift", "sfx/pixel_combat/magisplosion_1.ogg,sfx/pixel_combat/magisplosion_2.ogg,sfx/pixel_combat/magisplosion_3.ogg", "rift magic (Rift Surge)", 90, 5},
        {"spell_holy", "sfx/pixel_combat/energetic_impact_1.ogg,sfx/pixel_combat/energetic_impact_2.ogg,sfx/pixel_combat/energetic_impact_3.ogg", "holy magic (Holy Light, Smite)", 90, 5},
        {"spell_emp", "sfx/pixel_combat/forced_shutdown_1.ogg,sfx/pixel_combat/forced_shutdown_2.ogg", "EMP Pulse", 90, 5},
        {"spell_meteor", "sfx/pixel_combat/eruption_1.ogg,sfx/pixel_combat/eruption_2.ogg", "Meteor", 90, 5},
        {"spell_chain", "sfx/pixel_combat/shimmer_electric_1.ogg,sfx/pixel_combat/shimmer_electric_2.ogg", "Chain Lightning", 90, 5},
        {"spell_laser", "sfx/pixel_combat/mecha_white_laser_1.ogg,sfx/pixel_combat/mecha_white_laser_2.ogg", "Laser Beam", 90, 5},
        {"spell_drone", "sfx/pixel_combat/mecha_detonator_1.ogg,sfx/pixel_combat/mecha_detonator_2.ogg", "Drone Strike", 90, 5},
        {"spell_shield", "sfx/pixel_combat/bonus_max_shield_1.ogg,sfx/pixel_combat/bonus_max_shield_2.ogg", "protection (Iron Skin, Sanctuary)", 85, 5},
        {"spell_bless", "sfx/pixel_combat/bonus_regen_rate_1.ogg,sfx/pixel_combat/bonus_regen_rate_2.ogg", "Bless", 85, 5},
        {"spell_cure", "sfx/pixel_combat/simple_heal_1.ogg,sfx/pixel_combat/simple_heal_2.ogg", "Cure", 90, 0},
        {"spell_revive", "sfx/pixel_combat/energy_riser_1.ogg,sfx/pixel_combat/energy_riser_2.ogg", "Revive", 90, 0},
        {"skill_slash", "sfx/pixel_combat/sword_slash_1.ogg,sfx/pixel_combat/sword_slash_2.ogg,sfx/pixel_combat/sword_slash_3.ogg", "Cleave", 90, 8},
        {"skill_bash", "sfx/pixel_combat/mecha_piercing_punch_1.ogg,sfx/pixel_combat/mecha_piercing_punch_2.ogg", "Shield Bash", 90, 8},
        {"skill_volley", "sfx/pixel_combat/flying_blades_1.ogg,sfx/pixel_combat/flying_blades_2.ogg", "Volley", 90, 6},
        {"skill_warcry", "sfx/pixel_combat/growing_strength_1.ogg,sfx/pixel_combat/growing_strength_2.ogg", "War Cry", 85, 5},
        {"skill_mark", "sfx/pixel_combat/mecha_guarantee_hit_1.ogg,sfx/pixel_combat/mecha_guarantee_hit_2.ogg", "Mark Target", 85, 5},
        {"skill_overcharge", "sfx/pixel_combat/mecha_stats_1.ogg,sfx/pixel_combat/mecha_stats_2.ogg", "Overcharge", 85, 5},
        {"skill_hack", "sfx/pixel_combat/scifi_select_1.ogg,sfx/pixel_combat/scifi_select_2.ogg", "Hack", 85, 5},
        {"gather_mine", "sfx/pixel_combat/bitcrusher_1.ogg,sfx/pixel_combat/bitcrusher_2.ogg,sfx/pixel_combat/bitcrusher_3.ogg", "working a vein with a pickaxe (resources.txt sound=)", 85, 8}
    };
    static const E mus[] = {   // 1.3.0: the Pixel Fantasy pack by AlkaKrab (same as data/sounds.txt)
        {"title", "music/pixel_fantasy/01_dawn_of_blades.ogg", "desktop / main menu - Dawn of Blades", 160, 1},
        {"overworld", "music/pixel_fantasy/22_banners_in_the_wind.ogg", "overworld travel by day - Banners in the Wind", 110, 1},
        {"overworld_night", "music/pixel_fantasy/06_moonlit_vale.ogg", "overworld at night - Moonlit Vale", 160, 1},
        {"town", "music/pixel_fantasy/28_tales_of_the_hearth.ogg", "towns and interiors without their own music= - Tales of the Hearth", 115, 1},
        {"forest", "music/pixel_fantasy/13_the_forgotten_grove.ogg", "other hand-made maps without music= (wilderness, ruins) - The Forgotten Grove", 115, 1},
        {"dungeon", "music/pixel_fantasy/02_echoes_of_the_keep.ogg,music/pixel_fantasy/16_silent_citadel.ogg", "caves, mines, castles (a random one of the two each time) - Echoes of the Keep / Silent Citadel", 105, 1},
        {"tech", "music/pixel_fantasy/29_arcane_whispers.ogg", "tech ruins, the metro, the wreck - Arcane Whispers", 110, 1},
        {"combat", "music/pixel_fantasy/23_the_last_watch.ogg", "normal combat - The Last Watch", 100, 1},
        {"boss", "music/pixel_fantasy/24_blood_and_honor.ogg", "combat with a boss - Blood and Honor", 115, 1},
        {"fanfare", "music/pixel_fantasy/fx_1.ogg", "after a won fight (a short sting, plays once) - sting 1", 125, 0},
        {"gameover", "music/pixel_fantasy/fx_3.ogg", "game over (a short sting, plays once) - sting 3", 110, 0},
        {"ending", "music/pixel_fantasy/17_hymn_of_valor.ogg", "the epilogue and credits - Hymn of Valor", 115, 1},
        {"brasswick", "music/pixel_fantasy/12_march_of_iron.ogg", "Brasswick - March of Iron", 105, 1},
        {"ashfall", "music/pixel_fantasy/15_legends_of_the_flame.ogg", "Ashfall's ruins - Legends of the Flame", 95, 1},
        {"murkwater", "music/pixel_fantasy/21_the_silent_lake.ogg", "Murkwater - The Silent Lake", 100, 1},
        {"sunscar", "music/pixel_fantasy/23_the_last_watch.ogg", "Sunscar Outpost - The Last Watch", 100, 1},
        {"frosthold", "music/pixel_fantasy/08_frostbound_path.ogg", "Frosthold Shrine - Frostbound Path", 145, 1},
        {"crypt", "music/pixel_fantasy/07_chant_of_the_fallen.ogg", "crypts - Chant of the Fallen", 135, 1},
        {"swamp", "music/pixel_fantasy/11_whispers_in_the_fog.ogg,music/pixel_fantasy/18_the_dark_moor.ogg", "the Drowning Marsh (a random one of the two each time) - Whispers in the Fog / The Dark Moor", 120, 1},
        {"rift", "music/pixel_fantasy/26_echoes_of_eternity.ogg", "the Scar - Echoes of Eternity", 125, 1},
        {"dawn_of_blades", "music/pixel_fantasy/01_dawn_of_blades.ogg", "Pixel Fantasy track 1: Dawn of Blades (also: title)", 160, 1},
        {"echoes_of_the_keep", "music/pixel_fantasy/02_echoes_of_the_keep.ogg", "Pixel Fantasy track 2: Echoes of the Keep (also: dungeon)", 160, 1},
        {"twilight_march", "music/pixel_fantasy/03_twilight_march.ogg", "Pixel Fantasy track 3: Twilight March", 150, 1},
        {"ballad_of_ashenwood", "music/pixel_fantasy/04_ballad_of_ashenwood.ogg", "Pixel Fantasy track 4: Ballad of Ashenwood", 160, 1},
        {"riders_of_the_storm", "music/pixel_fantasy/05_riders_of_the_storm.ogg", "Pixel Fantasy track 5: Riders of the Storm (also: combat)", 110, 1},
        {"moonlit_vale", "music/pixel_fantasy/06_moonlit_vale.ogg", "Pixel Fantasy track 6: Moonlit Vale (also: overworld_night)", 160, 1},
        {"chant_of_the_fallen", "music/pixel_fantasy/07_chant_of_the_fallen.ogg", "Pixel Fantasy track 7: Chant of the Fallen (also: crypt)", 135, 1},
        {"frostbound_path", "music/pixel_fantasy/08_frostbound_path.ogg", "Pixel Fantasy track 8: Frostbound Path (also: frosthold)", 145, 1},
        {"old_tavern", "music/pixel_fantasy/09_the_old_tavern.ogg", "Pixel Fantasy track 9: The Old Tavern", 135, 1},
        {"crown_of_thorns", "music/pixel_fantasy/10_crown_of_thorns.ogg", "Pixel Fantasy track 10: Crown of Thorns (also: combat)", 160, 1},
        {"whispers_in_the_fog", "music/pixel_fantasy/11_whispers_in_the_fog.ogg", "Pixel Fantasy track 11: Whispers in the Fog (also: swamp)", 150, 1},
        {"march_of_iron", "music/pixel_fantasy/12_march_of_iron.ogg", "Pixel Fantasy track 12: March of Iron (also: brasswick)", 105, 1},
        {"forgotten_grove", "music/pixel_fantasy/13_the_forgotten_grove.ogg", "Pixel Fantasy track 13: The Forgotten Grove (also: forest)", 115, 1},
        {"sacred_springs", "music/pixel_fantasy/14_sacred_springs.ogg", "Pixel Fantasy track 14: Sacred Springs", 160, 1},
        {"legends_of_the_flame", "music/pixel_fantasy/15_legends_of_the_flame.ogg", "Pixel Fantasy track 15: Legends of the Flame (also: ashfall)", 95, 1},
        {"silent_citadel", "music/pixel_fantasy/16_silent_citadel.ogg", "Pixel Fantasy track 16: Silent Citadel (also: dungeon)", 105, 1},
        {"hymn_of_valor", "music/pixel_fantasy/17_hymn_of_valor.ogg", "Pixel Fantasy track 17: Hymn of Valor (also: ending)", 115, 1},
        {"dark_moor", "music/pixel_fantasy/18_the_dark_moor.ogg", "Pixel Fantasy track 18: The Dark Moor (also: swamp)", 120, 1},
        {"call_of_the_raven", "music/pixel_fantasy/19_call_of_the_raven.ogg", "Pixel Fantasy track 19: Call of the Raven", 100, 1},
        {"lament_of_kings", "music/pixel_fantasy/20_lament_of_kings.ogg", "Pixel Fantasy track 20: Lament of Kings", 105, 1},
        {"silent_lake", "music/pixel_fantasy/21_the_silent_lake.ogg", "Pixel Fantasy track 21: The Silent Lake (also: murkwater)", 100, 1},
        {"banners_in_the_wind", "music/pixel_fantasy/22_banners_in_the_wind.ogg", "Pixel Fantasy track 22: Banners in the Wind (also: overworld)", 110, 1},
        {"last_watch", "music/pixel_fantasy/23_the_last_watch.ogg", "Pixel Fantasy track 23: The Last Watch (also: sunscar)", 100, 1},
        {"blood_and_honor", "music/pixel_fantasy/24_blood_and_honor.ogg", "Pixel Fantasy track 24: Blood and Honor (also: boss)", 115, 1},
        {"tales_by_firelight", "music/pixel_fantasy/25_tales_by_firelight.ogg", "Pixel Fantasy track 25: Tales by Firelight", 130, 1},
        {"echoes_of_eternity", "music/pixel_fantasy/26_echoes_of_eternity.ogg", "Pixel Fantasy track 26: Echoes of Eternity (also: rift)", 125, 1},
        {"broken_crown", "music/pixel_fantasy/27_the_broken_crown.ogg", "Pixel Fantasy track 27: The Broken Crown", 100, 1},
        {"tales_of_the_hearth", "music/pixel_fantasy/28_tales_of_the_hearth.ogg", "Pixel Fantasy track 28: Tales of the Hearth (also: town)", 115, 1},
        {"arcane_whispers", "music/pixel_fantasy/29_arcane_whispers.ogg", "Pixel Fantasy track 29: Arcane Whispers (also: tech)", 110, 1},
        {"hidden_glade", "music/pixel_fantasy/30_the_hidden_glade.ogg", "Pixel Fantasy track 30: The Hidden Glade", 90, 1},
        {"sting_1", "music/pixel_fantasy/fx_1.ogg", "Pixel Fantasy sting 1, plays once (also: fanfare)", 125, 0},
        {"sting_2", "music/pixel_fantasy/fx_2.ogg", "Pixel Fantasy sting 2, plays once", 140, 0},
        {"sting_3", "music/pixel_fantasy/fx_3.ogg", "Pixel Fantasy sting 3, plays once (also: gameover)", 110, 0},
    };
    std::string s = "# Triffall sound map. [sound] = effect, [music] = track. file= is relative to assets/audio (several files, comma separated, pick at random).\n# volume= percent, pitchvar= +/- percent random pitch (sfx), loop=0/1 (music). Edit in the Editor > Sounds.\n#\n# Maps can set music=<track> in their [map] header; items/spells can set sound=<event>; creatures sound=<event> for their death.\n\n";
    for (auto& e : sfx) { s += fmt("[sound]\nid=%s\nfile=%s\ndesc=%s\n", e.id, e.file, e.desc); if (e.vol != 100) s += fmt("volume=%d\n", e.vol); if (e.pv) s += fmt("pitchvar=%d\n", e.pv); s += "\n"; }
    for (auto& e : mus) { s += fmt("[music]\nid=%s\nfile=%s\ndesc=%s\n", e.id, e.file, e.desc); if (e.vol != 100) s += fmt("volume=%d\n", e.vol); if (!e.pv) s += "loop=0\n"; s += "\n"; }
    // 1.3.1 (#50): no [ambient] loops in the base game any more
    return s;
}
