// 1.7.0 (#140): the in-game Update button - see updater.h.
#include "updater.h"
#include "version.h"
#include <cstdio>
#include <cstdlib>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <vector>
#if defined(_WIN32) && !defined(__EMSCRIPTEN__)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#define TRIFFALL_CAN_UPDATE 1
#endif

namespace fs = std::filesystem;
Updater gUpdater;

namespace {
std::string readAll(const fs::path& p) {
    std::ifstream f(p, std::ios::binary); if (!f) return "";
    std::stringstream ss; ss << f.rdbuf(); std::string s = ss.str();
    if (s.size() >= 3 && (unsigned char)s[0] == 0xEF && (unsigned char)s[1] == 0xBB && (unsigned char)s[2] == 0xBF) s.erase(0, 3);
    return s;
}
std::string trimmed(const std::string& s) {
    size_t a = s.find_first_not_of(" \t\r\n"), b = s.find_last_not_of(" \t\r\n");
    return a == std::string::npos ? "" : s.substr(a, b - a + 1);
}
// the string value of "key" in a small flat JSON object (latest.json); handles \" \\ \n \t and \uXXXX (ASCII only)
std::string jsonString(const std::string& j, const std::string& key) {
    size_t at = j.find("\"" + key + "\""); if (at == std::string::npos) return "";
    at = j.find(':', at + key.size() + 2); if (at == std::string::npos) return "";
    at = j.find_first_not_of(" \t\r\n", at + 1); if (at == std::string::npos || j[at] != '"') return "";
    std::string out;
    for (size_t i = at + 1; i < j.size(); i++) {
        char c = j[i];
        if (c == '"') return out;
        if (c == '\\' && i + 1 < j.size()) {
            char e = j[++i];
            if (e == 'n') out += ' '; else if (e == 't') out += ' ';
            else if (e == 'u' && i + 4 < j.size()) { int v = (int)strtol(j.substr(i + 1, 4).c_str(), nullptr, 16); out += v > 0 && v < 128 ? (char)v : '?'; i += 4; }
            else out += e;
        } else out += c;
    }
    return out;
}
// a narrow (ANSI, as the rest of the game uses) path as UTF-8, single-quoted for PowerShell
std::string psQuote(const fs::path& p) {
    std::string s = p.u8string(), o = "'";
    for (char c : s) { if (c == '\'') o += "''"; else o += c; }
    return o + "'";
}
}

int Updater::compareVersions(const std::string& a, const std::string& b) {
    auto parts = [](const std::string& v) { std::vector<long> r; std::string cur; for (char c : v + ".") { if (c == '.') { r.push_back(atol(cur.c_str())); cur.clear(); } else if (isdigit((unsigned char)c)) cur += c; else if (c == '-' || c == '+' || c == ' ') break; } return r; };
    auto x = parts(a), y = parts(b);
    for (size_t i = 0; i < x.size() || i < y.size(); i++) {
        long p = i < x.size() ? x[i] : 0, q = i < y.size() ? y[i] : 0;
        if (p != q) return p < q ? -1 : 1;
    }
    return 0;
}

Updater::Found Updater::readLatest(const std::string& source) {
    Found f;
    std::string j = readAll(fs::u8path(source) / "latest.json");
    if (j.empty()) return f;
    f.version = trimmed(jsonString(j, "version")); f.notes = trimmed(jsonString(j, "notes"));
    std::error_code ec;
    // complete = its folder holds the game (build.ps1 -Release copies exe + assets there before writing latest.json)
    f.ok = !f.version.empty() && compareVersions(f.version, TRIFFALL_VERSION) > 0 && fs::exists(fs::u8path(source) / fs::u8path(f.version) / "Triffall.exe", ec);
    return f;
}

void Updater::init(const std::string& exeDir0, const std::string& saveDir0, const std::string& savesArg0) {
    exeDir = exeDir0; saveDir = saveDir0; savesArg = savesArg0;
    // what the helper left after the last update
    fs::path res = fs::path(saveDir) / "update-result.txt";
    std::string r = readAll(res);
    if (!r.empty()) {
        std::vector<std::string> lines; std::stringstream ss(r); std::string l; while (std::getline(ss, l)) lines.push_back(trimmed(l));
        resultPending = true; resultOk = !lines.empty() && lines[0] == "ok";
        resultVersion = lines.size() > 1 ? lines[1] : ""; resultText = lines.size() > 2 ? lines[2] : "";
        std::error_code ec; fs::remove(res, ec);
    }
    std::string src = trimmed(readAll(fs::path(exeDir) / "update-source.txt"));
    if (src.empty()) return;
    std::error_code ec;
    if (!fs::is_directory(fs::u8path(src), ec)) return;
    source = src;
    checkNow();
}

void Updater::setSource(const std::string& releasesDir, const std::string& saveDir0) {
    source = fs::absolute(fs::path(releasesDir)).u8string(); saveDir = saveDir0;
    std::lock_guard<std::mutex> lk(mu); found = Found();
}

void Updater::checkNow(bool wait) {
    if (source.empty()) return;
    if (pending.valid()) { if (!wait) return; Found f = pending.get(); std::lock_guard<std::mutex> lk(mu); found = f; }
    sinceCheck = 0;
    std::string src = source;
    pending = std::async(std::launch::async, [src]() { return readLatest(src); });
    if (wait) { Found f = pending.get(); std::lock_guard<std::mutex> lk(mu); found = f; }
}

void Updater::tick(float dt) {
    if (source.empty()) return;
    if (pending.valid() && pending.wait_for(std::chrono::seconds(0)) == std::future_status::ready) { Found f = pending.get(); std::lock_guard<std::mutex> lk(mu); found = f; }
    sinceCheck += dt;
    if (sinceCheck >= CHECK_EVERY) checkNow();
}

bool Updater::available() const { std::lock_guard<std::mutex> lk(mu); return found.ok; }
std::string Updater::newVersion() const { std::lock_guard<std::mutex> lk(mu); return found.ok ? found.version : ""; }
std::string Updater::notes() const { std::lock_guard<std::mutex> lk(mu); return found.ok ? found.notes : ""; }

bool Updater::launch(std::string& err) {
    Found f; { std::lock_guard<std::mutex> lk(mu); f = found; }
    if (source.empty() || !f.ok) { err = "There is no update to install."; return false; }
    // the folder to update: where this exe really is
    fs::path exe = fs::path(exeDir) / "Triffall.exe";
    unsigned long pid = 0;
#ifdef TRIFFALL_CAN_UPDATE
    { wchar_t buf[32768]; DWORD n = GetModuleFileNameW(nullptr, buf, 32768); if (n > 0 && n < 32768) exe = fs::path(std::wstring(buf, n)); }
    pid = GetCurrentProcessId();
#endif
    fs::path dest = exe.parent_path();
    std::error_code ec;
    fs::path dir = fs::temp_directory_path(ec) / "triffall-update";
    if (dryRun) dir = fs::path(saveDir) / "triffall-update";
    fs::create_directories(dir, ec);
    if (!fs::is_directory(dir, ec)) { err = "Couldn't make a folder for the updater in " + dir.string() + "."; return false; }
    std::string restart = savesArg.empty() ? "@()" : "@('--saves', " + psQuote(fs::path("\"" + savesArg + "\"")) + ")";
    std::string s;
    s += "# Triffall updater, written by the game (v" TRIFFALL_VERSION ") to install " + f.version + " and start it again. Safe to delete.\r\n";
    s += "$ErrorActionPreference = 'Stop'\r\n$ProgressPreference = 'SilentlyContinue'\r\n";
    s += "$gamePid = " + std::to_string(pid) + "\r\n";
    s += "$dest = " + psQuote(dest) + "\r\n";
    s += "$releases = " + psQuote(fs::u8path(source)) + "\r\n";
    s += "$version = " + psQuote(fs::u8path(f.version)) + "\r\n";
    s += "$result = " + psQuote(fs::path(saveDir) / "update-result.txt") + "\r\n";
    s += "$restart = " + restart + "\r\n";
    s += R"PS($log = Join-Path $PSScriptRoot 'update.log'
function Say($t) { try { Add-Content -LiteralPath $log -Value ((Get-Date -Format s) + ' ' + $t) } catch {} }
$ok = $false; $text = ''
try {
    Say "updating $dest to $version"
    for ($i = 0; $gamePid -gt 0 -and $i -lt 240 -and (Get-Process -Id $gamePid -ErrorAction SilentlyContinue); $i++) { Start-Sleep -Milliseconds 250 }
    if ($gamePid -gt 0 -and (Get-Process -Id $gamePid -ErrorAction SilentlyContinue)) { throw 'Triffall did not close.' }
    Start-Sleep -Milliseconds 400
    $from = Join-Path $releases $version
    if (-not (Test-Path -LiteralPath (Join-Path $from 'Triffall.exe'))) { throw "The new version's folder ($from) has no Triffall.exe." }
    $install = Join-Path (Split-Path $releases -Parent) 'install.ps1'
    $standard = Join-Path $env:LOCALAPPDATA 'Programs\Triffall'
    if ((Test-Path -LiteralPath $install) -and ($dest.TrimEnd('\') -ieq $standard.TrimEnd('\'))) {
        # the standard install: install.ps1 also keeps the Editor edits made in it
        Say "running $install"
        $ErrorActionPreference = 'Continue'
        $out = & "$env:SystemRoot\System32\WindowsPowerShell\v1.0\powershell.exe" -NoProfile -NonInteractive -ExecutionPolicy Bypass -File $install 2>&1 | Out-String
        $code = $LASTEXITCODE
        $ErrorActionPreference = 'Stop'
        Say $out
        if ($code -ne 0) { $last = (($out.Trim() -split "`r?`n") | Where-Object { $_.Trim() -and $_ -notmatch '^\s*(\+|At |~)' } | Select-Object -Last 1) -replace '^powershell\.exe : ', ''; throw "install.ps1 stopped: $last" }
    } else {
        # any other copy: replace everything but saves\ (the old files wait in .update-old until the copy worked)
        $keep = @('saves', 'update-source.txt', '.update-old')
        $old = Join-Path $dest '.update-old'
        if (Test-Path -LiteralPath $old) { Remove-Item -LiteralPath $old -Recurse -Force }
        New-Item -ItemType Directory $old | Out-Null
        try {
            Get-ChildItem -LiteralPath $dest -Force | Where-Object { $keep -notcontains $_.Name } | Move-Item -Destination $old
            Copy-Item -Path (Join-Path $from '*') -Destination $dest -Recurse -Force
            Set-Content -LiteralPath (Join-Path $dest 'version.txt') $version -Encoding UTF8
        } catch {
            $why = $_.Exception.Message
            Say "copy failed, putting the old version back: $why"
            $moved = @(Get-ChildItem -LiteralPath $old -Force | ForEach-Object { $_.Name })
            Get-ChildItem -LiteralPath $dest -Force | Where-Object { $keep -notcontains $_.Name -and $moved -contains $_.Name } | Remove-Item -Recurse -Force
            Get-ChildItem -LiteralPath $old -Force | Move-Item -Destination $dest
            Remove-Item -LiteralPath $old -Recurse -Force
            throw $why
        }
        Remove-Item -LiteralPath $old -Recurse -Force -ErrorAction SilentlyContinue
    }
    $ok = $true
    Say 'done'
} catch { $text = $_.Exception.Message; Say "failed: $text" }
try { [IO.File]::WriteAllLines($result, [string[]]@($(if ($ok) { 'ok' } else { 'fail' }), $version, ($text -replace "`r?`n", ' '))) } catch { Say "could not write $result" }
$exe = Join-Path $dest 'Triffall.exe'
try { if ($restart.Count) { Start-Process -FilePath $exe -WorkingDirectory $dest -ArgumentList $restart } else { Start-Process -FilePath $exe -WorkingDirectory $dest } } catch { Say "could not start ${exe}: $($_.Exception.Message)" }
)PS";
    fs::path script = dir / ("update-" + std::to_string(pid) + ".ps1");
    {
        std::ofstream o(script, std::ios::binary);
        if (!o) { err = "Couldn't write the updater script (" + script.string() + ")."; return false; }
        o << "\xEF\xBB\xBF";   // UTF-8 with a BOM: PowerShell 5.1 reads a file without one as the ANSI code page
        o << s;
    }
    scriptPath = script.string();
    if (dryRun) return true;
#ifdef TRIFFALL_CAN_UPDATE
    wchar_t sysdir[MAX_PATH]; GetSystemDirectoryW(sysdir, MAX_PATH);
    std::wstring ps = std::wstring(sysdir) + L"\\WindowsPowerShell\\v1.0\\powershell.exe";
    std::wstring cmd = L"\"" + ps + L"\" -NoProfile -NonInteractive -ExecutionPolicy Bypass -WindowStyle Hidden -File \"" + script.wstring() + L"\"";
    std::wstring wdir = dir.wstring();
    STARTUPINFOW si{}; si.cb = sizeof si; PROCESS_INFORMATION pi{};
    std::vector<wchar_t> line(cmd.begin(), cmd.end()); line.push_back(0);
    // it must outlive the game: break away from a job the game may run in (when the job allows it)
    BOOL ok = CreateProcessW(ps.c_str(), line.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW | CREATE_NEW_PROCESS_GROUP | CREATE_BREAKAWAY_FROM_JOB, nullptr, wdir.c_str(), &si, &pi);
    if (!ok) { std::vector<wchar_t> l2(cmd.begin(), cmd.end()); l2.push_back(0); ok = CreateProcessW(ps.c_str(), l2.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW | CREATE_NEW_PROCESS_GROUP, nullptr, wdir.c_str(), &si, &pi); }
    if (!ok) { err = "The updater couldn't be started (error " + std::to_string(GetLastError()) + ")."; return false; }
    CloseHandle(pi.hThread); CloseHandle(pi.hProcess);
    return true;
#else
    err = "Updating from inside the game isn't possible on this system.";
    return false;
#endif
}
