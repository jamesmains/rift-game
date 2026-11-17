// The in-game Update button (1.7.0 #140; 1.7.1 #147-#149).
// update-source.txt next to the exe says where new versions come from:
//  - a folder (install.ps1 writes the project's releases\ folder for the owner's own install): latest.json there, the
//    new version in <folder>\<version>\. Checked every CHECK_EVERY_LOCAL seconds.
//  - an https address (the public releases repo's .../releases/latest/download/, put in every full-game hand-out zip by
//    build.ps1): <address>latest.json names the version, the zip and its SHA-256. Checked at start, then every
//    CHECK_EVERY_REMOTE seconds. On Yes the game downloads the zip to %TEMP%\triffall-update (WinHTTP, https only),
//    refuses it if the SHA-256 differs, unzips it, and only then closes. 1.7.3: a GitHub release without latest.json
//    (Idea Jotter makes them with only the zips) is read through api.github.com/repos/<repo>/releases/latest instead:
//    tag v<version>, asset triffall-v<version>-windows.zip and the SHA-256 GitHub keeps for it (its "digest").
// The install itself is done by a copy of this exe (%TEMP%\triffall-update\triffall-updater-<pid>.exe started with
// --apply-update <plan>; no PowerShell, except that the owner's standard install still runs the project's install.ps1).
// It waits for the game to close, moves the old files aside, copies the new version in, keeps the player's Editor edits
// and own campaigns (assets-manifest.txt tells which asset files they changed), puts the old files back if anything
// fails, writes saves\update-result.txt and starts the game again.
// 1.7.5 (#153): a full-game copy without update-source.txt uses the public repo built in at compile time
// (TRIFFALL_RELEASES_REPO, from releases-repo.txt): the 1.7.3 release folder that went to itch had no update-source.txt.
// Never on: demo / no-Editor copies without update-source.txt, copies the itch app installed (it keeps .itch\ in the install folder and
// updates them itself), Steam builds and any build with TRIFFALL_SELF_UPDATE=OFF (TRIFFALL_NO_SELF_UPDATE).
#pragma once
#include <string>
#include <future>
#include <mutex>
#include <atomic>
#include <map>
#include <vector>
#include <functional>

class Updater {
public:
    // Desktop build: the exe's folder (with update-source.txt), the saves folder, and the --saves value to restart with
    // ("" = none was given). Reads a pending update-result.txt and starts the first check.
    void init(const std::string& exeDir, const std::string& saveDir, const std::string& savesArg);
    void setSource(const std::string& source, const std::string& saveDir);   // headless tests: a releases\ folder or an address
    bool enabled() const { return !source.empty(); }
    bool remote() const { return isUrl(source); }
    std::string offReason;                  // why this copy has no updater ("" = it has one, or no update-source.txt)
    bool useBuiltIn = false;                // 1.7.5: the game sets it: no update-source.txt -> the built-in repo (tests: off)
    std::string builtInRepo = builtInRepoDefault();   // "<owner>/<repo>" ("" = this build has none)
    std::string sourceFrom;                 // "file" / "built-in" / "" (tests)
    static std::string builtInRepoDefault();
    void tick(float dt);                    // every frame: a check now and then (and picks up a finished one)
    void checkNow(bool wait = false);       // start a check now (wait = until it's done; tests)
    bool available() const;                 // a newer complete release is ready to install
    std::string newVersion() const;         // its version ("" = none)
    std::string notes() const;              // its latest.json notes
    long long newSize() const;              // remote: the zip's size in bytes when latest.json says (0 = unknown)

    // remote: download -> check -> unzip (off the main thread). The desktop shows the progress, then calls launch().
    enum Stage { IDLE, DOWNLOADING, UNPACKING, READY, FAILED };
    bool startDownload();                   // false = nothing to download (folder mode: launch() at once)
    void cancelDownload();
    Stage stage() const { return (Stage)stageV.load(); }
    long long gotBytes() const { return got.load(); }
    long long totalBytes() const { return total.load(); }
    std::string downloadError() const { std::lock_guard<std::mutex> lk(mu); return dlError; }
    void finishDownload(bool wait = false); // picks up the worker's result (tick does it; tests wait)
    void testProgress(long long g, long long t) { stageV = g < 0 ? IDLE : DOWNLOADING; got = g; total = t; }   // headless: show the download window

    // Writes the plan, starts the helper. true = the caller must quit now. false = err says why (nothing changed).
    bool launch(std::string& err);
    bool dryRun = false;                    // headless: write the plan (and the helper copy), don't start it
    std::string scriptPath;                 // the last plan written
    // the result the helper left for this start (update-result.txt, read and deleted by init): ok / fail + text
    bool resultPending = false, resultOk = false; std::string resultVersion, resultText;
    static int compareVersions(const std::string& a, const std::string& b);   // <0, 0, >0 (dotted numbers)
    static constexpr float CHECK_EVERY_LOCAL = 60.0f;
    static constexpr float CHECK_EVERY_REMOTE = 4 * 3600.0f;
    static bool isUrl(const std::string& s);

    // --- the helper (also used in-process by the headless tests) ---
    static int applyMain(const std::string& planPath);   // the exe was started with --apply-update <plan>
    struct Plan {
        unsigned long pid = 0;
        std::string dest, from, version, result, saves, install, cleanup, log;
        std::vector<std::string> restart;   // arguments for the restarted game
        bool restartGame = true, testFail = false;
    };
    static bool apply(const Plan& p, std::string& text);   // the swap; text = what happened (ok) or why not (fail)
    static bool readPlan(const std::string& path, Plan& p);
    static void writePlan(const std::string& path, const Plan& p);

    // --- tools (static, testable) ---
    static std::string sha256Hex(const void* data, size_t n);      // upper-case hex, like Get-FileHash
    static std::string sha256File(const std::string& path);         // "" if unreadable
    static bool unzip(const std::string& zipPath, const std::string& destDir, std::string& err);
    static std::map<std::string, std::string> readManifest(const std::string& path);   // rel (backslashes) -> hash
    static std::map<std::string, std::string> hashAssets(const std::string& assetsDir);

    // headless tests: answer address fetches from files in a folder instead of the network (name = the address's last part)
    std::function<bool(const std::string& url, const std::string& toFile, std::string& body, std::string& err)> fetchHook;
    bool download(const std::string& url, const std::string& toFile, std::string& err) { std::string b; return fetch(url, toFile, b, err); }   // headless httpget
    bool allowLocalHttp = false;            // tests: http://127.0.0.1 is allowed too
    std::string lastUrl;                    // the last address fetched (tests)

private:
    struct Found { std::string version, notes, zip, sha256; long long size = 0; bool ok = false; };
    Found readLatest(const std::string& source);
    static std::string githubApiUrl(const std::string& source);
    static Found parseGithubRelease(const std::string& apiJson);
    bool fetch(const std::string& url, const std::string& toFile, std::string& body, std::string& err, long long maxBody = 1 << 20);
    std::string zipUrl(const Found& f) const;
    std::string source, saveDir, exeDir, savesArg;
    mutable std::mutex mu;
    Found found;
    std::future<Found> pending;
    float sinceCheck = 0;
    // download
    std::atomic<int> stageV{IDLE};
    std::atomic<long long> got{0}, total{0};
    std::atomic<bool> cancel{false};
    std::future<void> dlTask;
    std::string dlError, unpackedDir, dlVersion;
};
extern Updater gUpdater;
