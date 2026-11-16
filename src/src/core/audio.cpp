#include "audio.h"
#include "util.h"
#include <filesystem>
#include <algorithm>
#include <cmath>
#include "sounds_builtin.h"

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
        d.volVar = sec.geti("volvar", 0); d.crunchRate = sec.geti("crunchrate", 0); d.crunchBits = sec.geti("crunchbits", 0);   // 1.3.4 (#52)
        loadHandles(d);
        if (!defs.count(d.id)) order.push_back(d.id);
        defs[d.id] = d;
    }
    // if the music that is playing was redefined, restart it so a new file takes effect
    if (!currentMusic.empty()) { std::string m = currentMusic; currentMusic.clear(); if (be) be->stopMusic(); music(m); }
    return true;
}

bool Audio::saveDefs() const {
    std::string s = "# Triffall sound map. [sound] = effect, [music] = track. file= is relative to assets/audio (several files, comma separated, pick at random).\n# volume= percent, pitchvar= +/- percent random pitch (sfx), volvar= +/- percent random volume (sfx), crunchrate= Hz and crunchbits= bit depth (sfx: lo-fi, 0 = off), loop=0/1 (music). Edit in the Editor > Sounds.\n\n";
    for (auto& id : order) {
        auto it = defs.find(id); if (it == defs.end()) continue; const SoundDef& d = it->second;
        s += d.kind == "music" ? "[music]\n" : d.kind == "ambient" ? "[ambient]\n" : "[sound]\n";
        s += "id=" + d.id + "\n";
        std::string files; for (size_t i = 0; i < d.files.size(); i++) files += (i ? "," : "") + d.files[i];
        s += "file=" + files + "\n";
        if (!d.desc.empty()) s += "desc=" + d.desc + "\n";
        if (d.volume != 100) s += "volume=" + std::to_string(d.volume) + "\n";
        if (d.kind != "music" && d.pitchVar) s += "pitchvar=" + std::to_string(d.pitchVar) + "\n";
        if (d.kind == "sfx" && d.volVar) s += "volvar=" + std::to_string(d.volVar) + "\n";
        if (d.kind == "sfx" && d.crunchRate) s += "crunchrate=" + std::to_string(d.crunchRate) + "\n";
        if (d.kind == "sfx" && d.crunchBits) s += "crunchbits=" + std::to_string(d.crunchBits) + "\n";
        if (d.kind == "music" && !d.loop) s += "loop=0\n";
        s += "\n";
    }
    return writeFile(defsPath, s);
}

std::string Audio::resolve(const std::string& ev) const {
    std::string cur = ev;
    for (int guard = 0; guard < 8 && !cur.empty(); guard++) {   // a trigger without its own entry plays its fallback (1.3.4)
        auto it = defs.find(cur); if (it != defs.end()) return it->second.kind == "sfx" ? cur : "";
        const SoundTrigger* t = soundTrigger(cur); cur = t && t->fallback ? t->fallback : "";
    }
    return "";
}

std::vector<std::string> Audio::sfxChoices() const {
    std::vector<std::string> o;
    for (auto& id : order) { auto it = defs.find(id); if (it != defs.end() && it->second.kind == "sfx") o.push_back(id); }
    for (auto& t : soundTriggers()) if (!defs.count(t.id) && t.fallback && !resolve(t.id).empty()) o.push_back(t.id);
    return o;
}

void Audio::play(const std::string& ev) {
    if (!enabled || ev.empty()) return;
    std::string id = resolve(ev);
    if (id.empty()) { if (std::find(missing.begin(), missing.end(), ev) == missing.end()) missing.push_back(ev); return; }
    auto it = defs.find(id);
    // the same event fired many times in one frame (area spells) plays once
    if (ev == lastEv && timeAcc - lastSameT < 0.03f) return;
    lastEv = ev; lastSameT = timeAcc;
    recent.push_back(ev); if (id != ev) recent.push_back(id); while (recent.size() > 32) recent.erase(recent.begin());
    SoundDef& d = it->second;
    if (!be) return;
    if (d.loadedRate != d.crunchRate || d.loadedBits != d.crunchBits) refresh(d);   // the Editor changed the crunch
    if (d.handles.empty()) return;
    int n = (int)d.handles.size(), i = 0;
    if (n > 1) { i = (int)(rnd() % (n - (d.lastPick >= 0 && d.lastPick < n ? 1 : 0))); if (d.lastPick >= 0 && d.lastPick < n && i >= d.lastPick) i++; }   // never the same file twice in a row
    d.lastPick = i;
    if (d.handles[i] < 0) return;
    float pitch = 1.0f; if (d.pitchVar > 0) pitch = 1.0f + ((int)(rnd() % (2 * d.pitchVar + 1)) - d.pitchVar) / 100.0f;
    float vol = d.volume / 100.0f; if (d.volVar > 0) vol *= std::max(0.0f, 1.0f + ((int)(rnd() % (2 * d.volVar + 1)) - d.volVar) / 100.0f);
    be->playSound(d.handles[i], masterV * sfxV * vol, pitch);
}

void Audio::loadHandles(SoundDef& d) {
    if (be) for (int h : d.handles) if (h >= 0) be->unloadSound(h);
    d.handles.clear(); d.lastPick = -1; d.loadedRate = d.crunchRate; d.loadedBits = d.crunchBits;
    if (d.kind != "sfx" || !be) return;
    for (auto& f : d.files) {
        int h = (d.crunchRate > 0 || d.crunchBits > 0) ? be->loadSoundFx(dir + "/" + f, d.crunchRate, d.crunchBits) : be->loadSound(dir + "/" + f);
        if (h < 0) lastError = "missing audio file: " + f;
        d.handles.push_back(h);
    }
}

// 1.3.4 (#52): the lo-fi "crunch". Sample-and-hold down to `rate` (the steps alias, like an old sound chip played back
// a low-rate sample) and quantise to `bits` levels measured from the file's own peak, so a quiet file crunches as much
// as a loud one. Interleaved float samples, changed in place. rate <= 0 or >= the file's rate, and bits <= 0 or >= 16,
// leave that half off.
void Audio::crunch(float* x, int frames, int channels, int srcRate, int rate, int bits) {
    if (!x || frames <= 0 || channels <= 0) return;
    if (rate > 0 && srcRate > 0 && rate < srcRate) {
        double step = (double)srcRate / rate, next = 0;
        std::vector<float> held(channels, 0.0f);
        for (int i = 0; i < frames; i++) {
            if (i >= next) { for (int c = 0; c < channels; c++) held[c] = x[i * channels + c]; next += step; }
            for (int c = 0; c < channels; c++) x[i * channels + c] = held[c];
        }
    }
    if (bits > 0 && bits < 16) {
        float peak = 0; for (int i = 0; i < frames * channels; i++) peak = std::max(peak, std::fabs(x[i]));
        if (peak <= 0) return;
        float levels = (float)(1 << (bits - 1));
        for (int i = 0; i < frames * channels; i++) x[i] = std::round(x[i] / peak * levels) / levels * peak;
    }
}


void Audio::music(const std::string& ev, bool again) {
    if (!enabled) return;
    std::string want = ev;
    if (again || want != finishedOneShot) finishedOneShot.clear();
    else if (currentMusic.empty()) { pendingMusic = want; fadingOut = false; return; }   // 1.5.2 (#104): it already played once
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
            if (it != defs.end() && !it->second.loop && !be->musicPlaying() && fade >= 1) { finishedOneShot = currentMusic; currentMusic.clear(); } // one-shot track ended
        }
        be->update();
    }
}

void Audio::refresh(SoundDef& d) {
    loadHandles(d);
    if (d.kind == "music" && d.id == currentMusic) { currentMusic.clear(); if (be) be->stopMusic(); music(d.id, true); }
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

// 1.3.4: the shipped sounds.txt itself, compiled into sounds_builtin.h by tools/mksoundsdefault.py (build.ps1 runs it)
std::string Audio::defaultDefsText() {
    std::string s; for (int i = 0; kBuiltinSounds[i]; i++) s += kBuiltinSounds[i];
    return s;
}
