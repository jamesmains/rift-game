// Audio: named sound events + music tracks defined in assets/data/sounds.txt, played through a platform backend.
// Game code only ever says gAudio.play("hit") or gAudio.music("dungeon"); which file that is (and how loud) is data.
#pragma once
#include <string>
#include <vector>
#include <map>
#include <functional>

struct AudioBackend {
    virtual ~AudioBackend() {}
    virtual int loadSound(const std::string& path) = 0;                  // returns handle >= 0, or -1
    // 1.3.4 (#52): load with the lo-fi "crunch" applied (rate = sample-and-hold rate in Hz, bits = bit depth; 0 = off).
    // A backend without it plays the file clean.
    virtual int loadSoundFx(const std::string& path, int rate, int bits) { (void)rate; (void)bits; return loadSound(path); }
    virtual void unloadSound(int handle) { (void)handle; }                // frees one handle (the Editor reloads a sound after a change)
    virtual void playSound(int handle, float volume, float pitch) = 0;
    virtual bool playMusic(const std::string& path, float volume, bool loop) = 0;
    virtual void stopMusic() = 0;
    virtual void setMusicVolume(float volume) = 0;
    virtual bool musicPlaying() = 0;
    virtual void update() = 0;
    virtual void unloadAll() = 0;
    // 1.0: a second looping stream under the music for ambience (wind, drips, rain...). Optional for a backend.
    virtual bool playAmbient(const std::string& path, float volume) { (void)path; (void)volume; return false; }
    virtual void stopAmbient() {}
    virtual void setAmbientVolume(float volume) { (void)volume; }
};

struct SoundDef {
    std::string id;
    std::string kind = "sfx";              // sfx | music | ambient (a loop under the music, at the Ambience volume)
    std::vector<std::string> files;        // relative to assets/audio; sfx with several files pick one at random
    int volume = 100;                      // percent
    int pitchVar = 0;                      // sfx: +/- percent random pitch
    int volVar = 0;                        // sfx: +/- percent random volume (1.3.4)
    int crunchRate = 0, crunchBits = 0;    // sfx: lo-fi at load - sample-and-hold rate (Hz) and bit depth; 0 = off (1.3.4, #52)
    int loadedRate = 0, loadedBits = 0;    // what the handles were loaded with (play() reloads after an Editor change)
    int lastPick = -1;                     // the file played last time (several files: never the same one twice in a row)
    bool loop = true;                      // music
    std::string desc;                      // shown in the editor
    std::vector<int> handles;              // loaded sfx handles (parallel to files; -1 = failed)
};

// 1.3.4 (#52): every sound the game itself plays ("triggers"), for the Editor's Sounds tab. A trigger with a fallback
// plays the fallback when sounds.txt has no entry for it (older sounds.txt files keep working).
struct SoundTrigger { const char* id; const char* group; const char* fallback; const char* when; };
const std::vector<SoundTrigger>& soundTriggers();
const SoundTrigger* soundTrigger(const std::string& id);
std::vector<std::string> soundGroups();    // group names in display order

class Audio {
public:
    void init(AudioBackend* b, const std::string& assetDir);
    bool loadDefs();                       // (re)reads sounds.txt and (re)loads the sfx files
    bool saveDefs() const;
    static std::string defaultDefsText();  // the shipped defaults, written when sounds.txt is missing
    void play(const std::string& ev);                  // one-shot sfx (unknown events are ignored, recorded in `missing`; a trigger falls back)
    std::string resolve(const std::string& ev) const;  // the entry an event plays: itself, else its trigger fallback(s), else ""
    static void crunch(float* samples, int frames, int channels, int srcRate, int rate, int bits);   // lo-fi in place (1.3.4)
    void music(const std::string& ev);                 // switch music (fades); "" = stop
    void stopMusic() { music(""); }
    void ambient(const std::string& ev);               // switch the ambient loop (crossfades); "" = silence
    std::string currentAmbient, pendingAmbient; float ambFade = 0; bool ambFadingOut = false;
    void setVolumes(int master, int music, int sfx, int ambience = 60);
    void refresh(SoundDef& d);             // (re)load the sfx handles of one definition after its files changed
    bool importFile(const std::string& srcPath, const std::string& sub, std::string& relOut); // copy an external file into assets/audio/<sub>
    void update(float dt);
    bool has(const std::string& ev) const { return defs.count(ev) > 0 || !resolve(ev).empty(); }   // 1.3.4: a trigger that falls back counts
    std::vector<std::string> sfxChoices() const;      // effects for the Editor's pickers: entries, then triggers without one (1.3.4)
    const SoundDef* def(const std::string& ev) const { auto it = defs.find(ev); return it == defs.end() ? nullptr : &it->second; }
    std::vector<std::string> availableFiles(const std::string& sub) const;   // list audio/<sub>/* files
    std::string audioDir() const { return dir; }

    std::map<std::string, SoundDef> defs;
    std::vector<std::string> order;        // ids in file order (editor display)
    std::string currentMusic, pendingMusic;
    std::vector<std::string> recent;       // last played events (tests / editor)
    std::vector<std::string> missing;
    std::string lastError;
    bool enabled = true;
private:
    AudioBackend* be = nullptr; std::string dir, defsPath;
    float masterV = 0.8f, musicV = 0.7f, sfxV = 0.8f, ambV = 0.6f;
    float fade = 1.0f; bool fadingOut = false; float lastSameT = 0; std::string lastEv;
    float timeAcc = 0;
    void startMusicNow(const std::string& ev);
    void loadHandles(SoundDef& d);         // (re)load the sfx handles of one definition, crunch included
    unsigned rngState = 12345;
    unsigned rnd() { rngState = rngState * 1664525u + 1013904223u; return rngState >> 8; }
};
extern Audio gAudio;
