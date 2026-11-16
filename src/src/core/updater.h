// 1.7.0 (#140): the in-game Update button.
// A copy installed by install.ps1 has update-source.txt next to its exe, naming the releases\ folder that
// build.ps1 -Release publishes into (latest.json + releases\<version>\). The game reads latest.json now and then
// (off the main thread); when it names a newer version whose folder holds Triffall.exe, the desktop shows an Update
// button. Clicking it writes a small PowerShell helper to %TEMP%, starts it and closes the game. The helper waits for
// the game to exit, installs the new version (the project's install.ps1 when the game is the standard install, which
// keeps saves and Editor edits; else a plain copy that keeps saves\), puts the old files back if anything fails,
// writes saves\update-result.txt and starts the game again, which then says how it went.
// Copies without update-source.txt (build\game, hand-out zips, itch / Steam) never show the button: those stores
// update the game themselves.
#pragma once
#include <string>
#include <future>
#include <mutex>

class Updater {
public:
    // Desktop build: the exe's folder (with update-source.txt), the saves folder, and the --saves value to restart with
    // ("" = none was given). Reads a pending update-result.txt and starts the first check.
    void init(const std::string& exeDir, const std::string& saveDir, const std::string& savesArg);
    void setSource(const std::string& releasesDir, const std::string& saveDir);   // headless tests: a releases\ folder of its own
    bool enabled() const { return !source.empty(); }
    void tick(float dt);                    // every frame: a check every CHECK_EVERY seconds (and picks up a finished one)
    void checkNow(bool wait = false);       // start a check now (wait = until it's done; tests)
    bool available() const;                 // a newer complete release is ready to install
    std::string newVersion() const;         // its version ("" = none)
    std::string notes() const;              // its latest.json notes
    // Writes the helper script and starts it. true = the caller must quit now. false = err says why (nothing changed).
    bool launch(std::string& err);
    bool dryRun = false;                    // headless: write the script, don't start it
    std::string scriptPath;                 // the last helper written
    // the result the helper left for this start (update-result.txt, read and deleted by init): ok / fail + text
    bool resultPending = false, resultOk = false; std::string resultVersion, resultText;
    static int compareVersions(const std::string& a, const std::string& b);   // <0, 0, >0 (dotted numbers)
    static constexpr float CHECK_EVERY = 60.0f;
private:
    struct Found { std::string version, notes; bool ok = false; };
    static Found readLatest(const std::string& source);
    std::string source, saveDir, exeDir, savesArg;
    mutable std::mutex mu;
    Found found;
    std::future<Found> pending;
    float sinceCheck = 0;
};
extern Updater gUpdater;
