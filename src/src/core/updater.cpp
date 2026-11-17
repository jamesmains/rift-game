// 1.7.0 (#140), 1.7.1 (#147 #148 #149): the in-game Update button - see updater.h.
#include "updater.h"
#include "version.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cctype>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <thread>
#include <vector>
#include <algorithm>
#if defined(_WIN32) && !defined(__EMSCRIPTEN__)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <winhttp.h>
#include <shellapi.h>
#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "shell32.lib")
#define TRIFFALL_CAN_UPDATE 1
#endif

int triffallInflateRaw(char* out, int olen, const char* in, int ilen);   // gfx.cpp: stb_image's inflate (raw deflate)

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
bool startsWith(const std::string& s, const std::string& p) { return s.size() >= p.size() && s.compare(0, p.size(), p) == 0; }
std::string lower(std::string s) { for (char& c : s) c = (char)tolower((unsigned char)c); return s; }
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
long long jsonNumber(const std::string& j, const std::string& key) {
    size_t at = j.find("\"" + key + "\""); if (at == std::string::npos) return 0;
    at = j.find(':', at + key.size() + 2); if (at == std::string::npos) return 0;
    at = j.find_first_not_of(" \t\r\n\"", at + 1); if (at == std::string::npos) return 0;
    return atoll(j.c_str() + at);
}
std::string mb(double bytes) { char b[32]; snprintf(b, sizeof b, "%.1f MB", bytes / (1024.0 * 1024.0)); return b; }

// ---- SHA-256 (FIPS 180-4) ----
struct Sha256 {
    uint32_t h[8] = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
    uint64_t len = 0; uint8_t buf[64]; size_t n = 0;
    static uint32_t ror(uint32_t x, int r) { return (x >> r) | (x << (32 - r)); }
    void block(const uint8_t* p) {
        static const uint32_t k[64] = {
            0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
            0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
            0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
            0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
            0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
            0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
            0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
            0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};
        uint32_t w[64];
        for (int i = 0; i < 16; i++) w[i] = (uint32_t)p[4 * i] << 24 | (uint32_t)p[4 * i + 1] << 16 | (uint32_t)p[4 * i + 2] << 8 | p[4 * i + 3];
        for (int i = 16; i < 64; i++) {
            uint32_t s0 = ror(w[i - 15], 7) ^ ror(w[i - 15], 18) ^ (w[i - 15] >> 3), s1 = ror(w[i - 2], 17) ^ ror(w[i - 2], 19) ^ (w[i - 2] >> 10);
            w[i] = w[i - 16] + s0 + w[i - 7] + s1;
        }
        uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4], f = h[5], g = h[6], hh = h[7];
        for (int i = 0; i < 64; i++) {
            uint32_t S1 = ror(e, 6) ^ ror(e, 11) ^ ror(e, 25), ch = (e & f) ^ (~e & g), t1 = hh + S1 + ch + k[i] + w[i];
            uint32_t S0 = ror(a, 2) ^ ror(a, 13) ^ ror(a, 22), mj = (a & b) ^ (a & c) ^ (b & c), t2 = S0 + mj;
            hh = g; g = f; f = e; e = d + t1; d = c; c = b; b = a; a = t1 + t2;
        }
        h[0] += a; h[1] += b; h[2] += c; h[3] += d; h[4] += e; h[5] += f; h[6] += g; h[7] += hh;
    }
    void add(const void* data, size_t m) {
        const uint8_t* p = (const uint8_t*)data; len += m;
        if (n == 0) while (m >= 64) { block(p); p += 64; m -= 64; }
        while (m) { size_t t = std::min(64 - n, m); memcpy(buf + n, p, t); n += t; p += t; m -= t; if (n == 64) { block(buf); n = 0; } }
    }
    std::string hex() {
        uint64_t bits = len * 8; uint8_t pad = 0x80, z = 0; add(&pad, 1); while (n != 56) add(&z, 1);
        uint8_t L[8]; for (int i = 0; i < 8; i++) L[i] = (uint8_t)(bits >> (56 - 8 * i)); add(L, 8);
        static const char* hx = "0123456789ABCDEF"; std::string o;
        for (int i = 0; i < 8; i++) for (int s = 28; s >= 0; s -= 4) o += hx[(h[i] >> s) & 15];
        return o;
    }
};
uint32_t crc32(const uint8_t* p, size_t n) {
    static uint32_t t[256]; static bool made = false;
    if (!made) { for (uint32_t i = 0; i < 256; i++) { uint32_t c = i; for (int k = 0; k < 8; k++) c = c & 1 ? 0xEDB88320u ^ (c >> 1) : c >> 1; t[i] = c; } made = true; }
    uint32_t c = 0xFFFFFFFFu; for (size_t i = 0; i < n; i++) c = t[(c ^ p[i]) & 255] ^ (c >> 8); return c ^ 0xFFFFFFFFu;
}
uint32_t le32(const uint8_t* p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }
uint16_t le16(const uint8_t* p) { return (uint16_t)(p[0] | p[1] << 8); }

// a path relative to an assets folder, written the way the manifest does (backslashes)
std::string relKey(const fs::path& p, const fs::path& base) {
    std::string r = p.lexically_relative(base).generic_u8string();
    std::replace(r.begin(), r.end(), '/', '\\');
    return r;
}
void writeManifest(const fs::path& file, const std::map<std::string, std::string>& m) {
    std::ofstream o(file, std::ios::binary);
    for (auto& kv : m) o << kv.second << "  " << kv.first << "\r\n";
}
fs::path workDir(bool dryRun, const std::string& saveDir) {
    std::error_code ec;
    if (dryRun) return fs::path(saveDir) / "triffall-update";
    return fs::temp_directory_path(ec) / "triffall-update";
}
void logLine(const std::string& logPath, const std::string& t) {
    if (logPath.empty()) return;
    std::ofstream o(fs::u8path(logPath), std::ios::binary | std::ios::app);
    char ts[32]; time_t now = time(nullptr); struct tm* lt = localtime(&now); if (lt) strftime(ts, sizeof ts, "%Y-%m-%dT%H:%M:%S", lt); else ts[0] = 0;
    o << ts << " " << t << "\r\n";
}
bool isCoreCampaign(const std::string& id) { return id == "triffall" || id == "tutorial"; }
}

bool Updater::isUrl(const std::string& s) { std::string l = lower(s); return startsWith(l, "https://") || startsWith(l, "http://"); }

int Updater::compareVersions(const std::string& a, const std::string& b) {
    auto parts = [](const std::string& v) { std::vector<long> r; std::string cur; for (char c : v + ".") { if (c == '.') { r.push_back(atol(cur.c_str())); cur.clear(); } else if (isdigit((unsigned char)c)) cur += c; else if (c == '-' || c == '+' || c == ' ') break; } return r; };
    auto x = parts(a), y = parts(b);
    for (size_t i = 0; i < x.size() || i < y.size(); i++) {
        long p = i < x.size() ? x[i] : 0, q = i < y.size() ? y[i] : 0;
        if (p != q) return p < q ? -1 : 1;
    }
    return 0;
}

std::string Updater::sha256Hex(const void* data, size_t n) { Sha256 s; s.add(data, n); return s.hex(); }
std::string Updater::sha256File(const std::string& path) {
    std::ifstream f(fs::u8path(path), std::ios::binary); if (!f) return "";
    Sha256 s; std::vector<char> buf(1 << 16);
    while (f) { f.read(buf.data(), buf.size()); std::streamsize n = f.gcount(); if (n > 0) s.add(buf.data(), (size_t)n); }
    return s.hex();
}
std::map<std::string, std::string> Updater::readManifest(const std::string& path) {
    std::map<std::string, std::string> m;
    std::stringstream ss(readAll(fs::u8path(path))); std::string l;
    while (std::getline(ss, l)) {
        if (!l.empty() && l.back() == '\r') l.pop_back();
        if (l.size() > 66 && l[64] == ' ' && l[65] == ' ') m[l.substr(66)] = l.substr(0, 64);
    }
    return m;
}
std::map<std::string, std::string> Updater::hashAssets(const std::string& assetsDir) {
    std::map<std::string, std::string> m; std::error_code ec;
    fs::path base = fs::u8path(assetsDir);
    if (!fs::is_directory(base, ec)) return m;
    for (fs::recursive_directory_iterator it(base, ec), end; !ec && it != end; it.increment(ec)) {
        if (!it->is_regular_file(ec)) continue;
        std::string rel = relKey(it->path(), base);
        if (startsWith(lower(rel), "new-art\\")) continue;   // the art sources never ship
        m[rel] = sha256File(it->path().u8string());
    }
    return m;
}

// a zip as build.ps1 makes it (Compress-Archive: stored or deflated entries, maybe backslashes in names). Every entry's
// CRC is checked; names that would leave destDir are refused.
bool Updater::unzip(const std::string& zipPath, const std::string& destDir, std::string& err) {
    std::ifstream f(fs::u8path(zipPath), std::ios::binary);
    if (!f) { err = "Couldn't open the downloaded file."; return false; }
    f.seekg(0, std::ios::end); long long size = (long long)f.tellg();
    long long tail = std::min<long long>(size, 65557);
    std::vector<uint8_t> t((size_t)tail); f.seekg(size - tail); f.read((char*)t.data(), tail);
    long long eocd = -1;
    for (long long i = tail - 22; i >= 0; i--) if (le32(&t[(size_t)i]) == 0x06054b50) { eocd = i; break; }
    if (eocd < 0) { err = "The downloaded file isn't a zip."; return false; }
    const uint8_t* e = &t[(size_t)eocd];
    uint32_t count = le16(e + 10), cdSize = le32(e + 12), cdOff = le32(e + 16);
    if (cdOff == 0xFFFFFFFFu || count == 0xFFFF) { err = "The downloaded zip is too big to unpack (zip64)."; return false; }
    std::vector<uint8_t> cd(cdSize); f.seekg(cdOff); f.read((char*)cd.data(), cdSize);
    if ((uint32_t)f.gcount() != cdSize) { err = "The downloaded zip is cut short."; return false; }
    fs::path dest = fs::u8path(destDir); std::error_code ec; fs::create_directories(dest, ec);
    size_t at = 0;
    for (uint32_t k = 0; k < count; k++) {
        if (at + 46 > cd.size() || le32(&cd[at]) != 0x02014b50) { err = "The downloaded zip is damaged."; return false; }
        const uint8_t* p = &cd[at];
        uint16_t method = le16(p + 10); uint32_t crc = le32(p + 16), csize = le32(p + 20), usize = le32(p + 24);
        uint16_t nlen = le16(p + 28), xlen = le16(p + 30), clen = le16(p + 32); uint32_t lho = le32(p + 42);
        if (at + 46 + nlen > cd.size()) { err = "The downloaded zip is damaged."; return false; }
        std::string name((const char*)p + 46, nlen);
        at += 46 + nlen + xlen + clen;
        std::replace(name.begin(), name.end(), '\\', '/');
        if (name.empty() || name[0] == '/' || name.find(':') != std::string::npos || ("/" + name + "/").find("/../") != std::string::npos) { err = "The downloaded zip has a bad file name (" + name + ")."; return false; }
        fs::path out = dest / fs::u8path(name);
        if (name.back() == '/') { fs::create_directories(out, ec); continue; }
        uint8_t lh[30]; f.seekg(lho); f.read((char*)lh, 30);
        if (f.gcount() != 30 || le32(lh) != 0x04034b50) { err = "The downloaded zip is damaged."; return false; }
        f.seekg((long long)lho + 30 + le16(lh + 26) + le16(lh + 28));
        std::vector<uint8_t> comp(csize); if (csize) f.read((char*)comp.data(), csize);
        if ((uint32_t)f.gcount() != csize && csize) { err = "The downloaded zip is cut short."; return false; }
        std::vector<uint8_t> data;
        if (method == 0) data.swap(comp);
        else if (method == 8) {
            data.resize(usize);
            if (usize && triffallInflateRaw((char*)data.data(), (int)usize, (const char*)comp.data(), (int)csize) != (int)usize) { err = "Couldn't unpack " + name + "."; return false; }
        } else { err = "The downloaded zip packs " + name + " in a way the game can't unpack."; return false; }
        if (data.size() != usize || crc32(data.data(), data.size()) != crc) { err = name + " in the download is damaged."; return false; }
        fs::create_directories(out.parent_path(), ec);
        std::ofstream o(out, std::ios::binary);
        if (!o) { err = "Couldn't write " + out.u8string() + "."; return false; }
        o.write((const char*)data.data(), data.size());
        if (!o) { err = "Couldn't write " + out.u8string() + " (disk full?)."; return false; }
    }
    return true;
}

// ---- fetching (https only; WinHTTP follows GitHub's redirect to its file host by itself) ----
bool Updater::fetch(const std::string& url, const std::string& toFile, std::string& body, std::string& err, long long maxBody) {
    { std::lock_guard<std::mutex> lk(mu); lastUrl = url; }
    if (fetchHook) return fetchHook(url, toFile, body, err);
#ifdef TRIFFALL_CAN_UPDATE
    std::string l = lower(url);
    bool https = startsWith(l, "https://"), local = allowLocalHttp && startsWith(l, "http://127.0.0.1");
    if (!https && !local) { err = "Updates are only fetched over https."; return false; }
    int wn = MultiByteToWideChar(CP_UTF8, 0, url.c_str(), -1, nullptr, 0);
    std::wstring w(wn > 0 ? wn : 1, 0); MultiByteToWideChar(CP_UTF8, 0, url.c_str(), -1, &w[0], wn); w.resize(wcslen(w.c_str()));
    URL_COMPONENTS uc{}; uc.dwStructSize = sizeof uc;
    wchar_t host[256], path[2048]; uc.lpszHostName = host; uc.dwHostNameLength = 256; uc.lpszUrlPath = path; uc.dwUrlPathLength = 2048;
    wchar_t extra[1024]; uc.lpszExtraInfo = extra; uc.dwExtraInfoLength = 1024;
    if (!WinHttpCrackUrl(w.c_str(), 0, 0, &uc)) { err = "The update address is not valid."; return false; }
    std::wstring fullPath = std::wstring(path) + extra;
    std::wstring agent = L"Triffall-Updater/" + std::wstring(TRIFFALL_VERSION, TRIFFALL_VERSION + strlen(TRIFFALL_VERSION));
    HINTERNET s = WinHttpOpen(agent.c_str(), WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!s) s = WinHttpOpen(agent.c_str(), WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!s) { err = "Couldn't start a download (error " + std::to_string(GetLastError()) + ")."; return false; }
    WinHttpSetTimeouts(s, 15000, 15000, 30000, 60000);
    DWORD policy = WINHTTP_OPTION_REDIRECT_POLICY_DISALLOW_HTTPS_TO_HTTP; WinHttpSetOption(s, WINHTTP_OPTION_REDIRECT_POLICY, &policy, sizeof policy);
    HINTERNET c = WinHttpConnect(s, host, uc.nPort, 0);
    HINTERNET r = c ? WinHttpOpenRequest(c, L"GET", fullPath.c_str(), nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, https ? WINHTTP_FLAG_SECURE : 0) : nullptr;
    bool ok = false; DWORD code = 0;
    auto netErr = [&](DWORD e) {
        if (e == ERROR_WINHTTP_NAME_NOT_RESOLVED || e == ERROR_WINHTTP_CANNOT_CONNECT || e == ERROR_WINHTTP_TIMEOUT || e == ERROR_WINHTTP_CONNECTION_ERROR)
            err = "Couldn't reach the update server. Is this PC online?";
        else if (e == ERROR_WINHTTP_SECURE_FAILURE) err = "The update server's secure connection couldn't be checked.";
        else err = "The download failed (error " + std::to_string(e) + ").";
    };
    if (!r || !WinHttpSendRequest(r, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) || !WinHttpReceiveResponse(r, nullptr)) netErr(GetLastError());
    else {
        DWORD sz = sizeof code;
        WinHttpQueryHeaders(r, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &code, &sz, WINHTTP_NO_HEADER_INDEX);
        if (code != 200) err = code == 404 ? "Not found on the update server." : "The update server answered " + std::to_string(code) + ".";
        else {
            DWORD len = 0; sz = sizeof len;
            if (WinHttpQueryHeaders(r, WINHTTP_QUERY_CONTENT_LENGTH | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &len, &sz, WINHTTP_NO_HEADER_INDEX) && len && !toFile.empty()) total = len;
            std::ofstream o; if (!toFile.empty()) { o.open(fs::u8path(toFile), std::ios::binary); if (!o) err = "Couldn't save the download in " + toFile + "."; }
            std::vector<char> buf(1 << 16);
            ok = err.empty();
            while (ok) {
                DWORD n = 0;
                if (!WinHttpReadData(r, buf.data(), (DWORD)buf.size(), &n)) { netErr(GetLastError()); ok = false; break; }
                if (n == 0) break;
                if (cancel) { err = "cancelled"; ok = false; break; }
                if (!toFile.empty()) { o.write(buf.data(), n); if (!o) { err = "Couldn't save the download (disk full?)."; ok = false; } got += n; }
                else { body.append(buf.data(), n); if ((long long)body.size() > maxBody) { err = "The update server sent too much."; ok = false; } }
            }
        }
    }
    if (r) WinHttpCloseHandle(r); if (c) WinHttpCloseHandle(c); WinHttpCloseHandle(s);
    return ok;
#else
    (void)toFile; (void)body; (void)maxBody; err = "Downloads aren't possible on this system."; return false;
#endif
}

std::string Updater::zipUrl(const Found& f) const {
    // the zip of exactly that version: .../releases/latest/download/ -> .../releases/download/v<version>/ (GitHub), so a
    // release published while this one downloads can't swap the file under the checked hash
    std::string base = source; const std::string latest = "/releases/latest/download/";
    size_t at = base.rfind(latest);
    if (at != std::string::npos && at + latest.size() == base.size()) base = base.substr(0, at) + "/releases/download/v" + f.version + "/";
    return base + f.zip;
}

// 1.7.3: the GitHub API's address for the newest release of the repo in a https://github.com/<owner>/<repo>/releases/
// latest/download/ source ("" = not such a source)
std::string Updater::githubApiUrl(const std::string& src) {
    const std::string host = "https://github.com/", tail = "/releases/latest/download/";
    if (!startsWith(lower(src), host) || src.size() <= host.size() + tail.size() || src.compare(src.size() - tail.size(), tail.size(), tail) != 0) return "";
    std::string repo = src.substr(host.size(), src.size() - host.size() - tail.size());
    if (std::count(repo.begin(), repo.end(), '/') != 1 || repo.front() == '/' || repo.back() == '/' || repo.find_first_of("?#%\\ ") != std::string::npos) return "";
    return "https://api.github.com/repos/" + repo + "/releases/latest";
}

// 1.7.3: the newest GitHub release as the API describes it (Idea Jotter makes the releases and attaches only the zips, no
// latest.json): version = its tag without the "v", notes = its text, and the zip triffall-v<version>-windows.zip with the
// size and SHA-256 GitHub keeps for it ("digest"). No such zip yet (still uploading) or no digest = no update yet.
Updater::Found Updater::parseGithubRelease(const std::string& j) {
    Found f;
    std::string tag = trimmed(jsonString(j, "tag_name"));
    f.version = (!tag.empty() && (tag[0] == 'v' || tag[0] == 'V')) ? tag.substr(1) : tag;
    if (f.version.empty() || f.version.find_first_not_of("0123456789.") != std::string::npos) { f.version.clear(); return f; }
    // the text: Markdown from the release page, made plain enough for a message box
    std::string n = trimmed(jsonString(j, "body")), plain;
    for (size_t i = 0; i < n.size(); i++) {
        char c = n[i];
        if (c == '*' || c == '`' || c == '#' || c == '\r') continue;
        if (c == '-' && (i == 0 || n[i - 1] == ' ') && i + 1 < n.size() && n[i + 1] == ' ') { while (!plain.empty() && plain.back() == ' ') plain.pop_back(); if (!plain.empty() && std::string(".!?:;").find(plain.back()) == std::string::npos) plain += ";"; continue; }
        if (c == ' ' && !plain.empty() && plain.back() == ' ') continue;
        plain += c;
    }
    plain = trimmed(plain);
    while (!plain.empty() && plain[0] == ';') plain = trimmed(plain.substr(1));
    if (plain.size() > 300) { plain = plain.substr(0, 297); size_t sp = plain.find_last_of(' '); if (sp > 200) plain.resize(sp); plain += "..."; }
    f.notes = plain;
    // the asset: from its "name" up to the next asset's "url" (the fields of one asset)
    std::string want = "triffall-v" + f.version + "-windows.zip";
    size_t assets = j.find("\"assets\"");
    for (size_t at = assets == std::string::npos ? std::string::npos : j.find("\"name\"", assets); at != std::string::npos; at = j.find("\"name\"", at + 6)) {
        if (jsonString(j.substr(at), "name") != want) continue;
        size_t end = j.find("\"browser_download_url\"", at);
        std::string a = j.substr(at, end == std::string::npos ? std::string::npos : end - at);
        std::string state = jsonString(a, "state"), digest = trimmed(jsonString(a, "digest"));
        if (!state.empty() && state != "uploaded") break;
        if (lower(digest.substr(0, 7)) != "sha256:") break;
        f.zip = want; f.sha256 = digest.substr(7); f.size = jsonNumber(a, "size");
        f.ok = f.sha256.size() == 64 && f.sha256.find_first_not_of("0123456789abcdefABCDEF") == std::string::npos;
        break;
    }
    return f;
}

Updater::Found Updater::readLatest(const std::string& src) {
    Found f; std::string j;
    if (isUrl(src)) {
        std::string err;
        if (!fetch(src + "latest.json", "", j, err)) {
            // 1.7.3: no latest.json on the release (Idea Jotter's GitHub releases): ask GitHub's API about it instead
            std::string api = githubApiUrl(src), body;
            if (api.empty() || !fetch(api, "", body, err, 4 << 20)) return f;
            f = parseGithubRelease(body);
            if (f.version.empty() || compareVersions(f.version, TRIFFALL_VERSION) <= 0) return Found();
            return f;
        }
    }
    else j = readAll(fs::u8path(src) / "latest.json");
    if (j.empty()) return f;
    f.version = trimmed(jsonString(j, "version")); f.notes = trimmed(jsonString(j, "notes"));
    if (f.version.empty() || compareVersions(f.version, TRIFFALL_VERSION) <= 0) return f;
    std::error_code ec;
    if (isUrl(src)) {
        // 1.7.1 (#147): the zip's name (a plain file name, nothing else) and SHA-256 must be there
        f.zip = trimmed(jsonString(j, "zip")); f.sha256 = trimmed(jsonString(j, "sha256")); f.size = jsonNumber(j, "size");
        bool nameOk = f.zip.size() > 4 && lower(f.zip.substr(f.zip.size() - 4)) == ".zip" && f.zip.find_first_of("/\\:?#%") == std::string::npos && f.zip.find("..") == std::string::npos;
        bool hashOk = f.sha256.size() == 64 && f.sha256.find_first_not_of("0123456789abcdefABCDEF") == std::string::npos;
        f.ok = nameOk && hashOk;
    } else
        // complete = its folder holds the game (build.ps1 -Release copies exe + assets there before writing latest.json)
        f.ok = fs::exists(fs::u8path(src) / fs::u8path(f.version) / "Triffall.exe", ec);
    return f;
}

std::string Updater::builtInRepoDefault() {
#ifdef TRIFFALL_RELEASES_REPO
    return TRIFFALL_RELEASES_REPO;
#else
    return "";
#endif
}

void Updater::init(const std::string& exeDir0, const std::string& saveDir0, const std::string& savesArg0) {
    exeDir = exeDir0; saveDir = saveDir0; savesArg = savesArg0;
    source.clear(); offReason.clear(); sourceFrom.clear(); { std::lock_guard<std::mutex> lk(mu); found = Found(); }
    finishDownload(true); stageV = IDLE; { std::lock_guard<std::mutex> lk(mu); dlVersion.clear(); unpackedDir.clear(); dlError.clear(); }
    // what the helper left after the last update
    fs::path res = fs::path(saveDir) / "update-result.txt";
    std::string r = readAll(res);
    if (!r.empty()) {
        std::vector<std::string> lines; std::stringstream ss(r); std::string l; while (std::getline(ss, l)) lines.push_back(trimmed(l));
        resultPending = true; resultOk = !lines.empty() && lines[0] == "ok";
        resultVersion = lines.size() > 1 ? lines[1] : ""; resultText = lines.size() > 2 ? lines[2] : "";
        std::error_code ec; fs::remove(res, ec);
    }
#ifdef TRIFFALL_NO_SELF_UPDATE
    offReason = "store"; return;   // Steam (or a build made without the updater): the store updates the game
#endif
    std::string src = trimmed(readAll(fs::path(exeDir) / "update-source.txt"));
    sourceFrom = src.empty() ? "" : "file";
    // 1.7.5 (#153): no file (or an empty one) -> the public releases repo this build knows
    if (src.empty() && useBuiltIn && !builtInRepo.empty()) { src = "https://github.com/" + builtInRepo + "/releases/latest/download/"; sourceFrom = "built-in"; }
    if (src.empty()) return;
    std::error_code ec;
    // 1.7.1 (#149): the itch app installed this copy (it keeps .itch\receipt.json.gz in the install folder) and updates it
    // itself; replacing its files would make it repair or download them again
    if (fs::exists(fs::path(exeDir) / ".itch", ec)) { offReason = "itch"; return; }
    if (isUrl(src)) {
        if (!startsWith(lower(src), "https://")) return;
        if (src.back() != '/') src += '/';
    } else if (!fs::is_directory(fs::u8path(src), ec)) return;
    source = src;
    // leftovers of an earlier update (the helper can't delete its own exe)
    fs::path dir = workDir(false, saveDir);
    for (fs::directory_iterator it(dir, ec), end; !ec && it != end; it.increment(ec)) {
        std::string n = it->path().filename().u8string(); std::error_code e2;
        if (startsWith(n, "triffall-updater-") || (startsWith(n, "update-") && n.size() > 4 && n.substr(n.size() - 4) == ".txt")) fs::remove(it->path(), e2);
        else if (startsWith(n, "v") && it->is_directory(e2)) fs::remove_all(it->path(), e2);
    }
    checkNow();
}

void Updater::setSource(const std::string& src, const std::string& saveDir0) {
    if (isUrl(src)) { source = src; if (source.back() != '/') source += '/'; }
    else source = fs::absolute(fs::path(src)).u8string();
    saveDir = saveDir0;
    std::lock_guard<std::mutex> lk(mu); found = Found();
}

void Updater::checkNow(bool wait) {
    if (source.empty()) return;
    if (pending.valid()) { if (!wait) return; Found f = pending.get(); std::lock_guard<std::mutex> lk(mu); found = f; }
    sinceCheck = 0;
    std::string src = source;
    pending = std::async(std::launch::async, [this, src]() { return readLatest(src); });
    if (wait) { Found f = pending.get(); std::lock_guard<std::mutex> lk(mu); found = f; }
}

void Updater::tick(float dt) {
    if (source.empty()) return;
    if (pending.valid() && pending.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
        Found f = pending.get(); std::lock_guard<std::mutex> lk(mu);
        // a failed check (offline) doesn't hide an update already found
        if (f.ok || !found.ok || !isUrl(source)) found = f;
    }
    finishDownload();
    sinceCheck += dt;
    // 1.7.1 (#149): the owner's local folder every minute; the public repo at start and then every few hours
    if (sinceCheck >= (isUrl(source) ? CHECK_EVERY_REMOTE : CHECK_EVERY_LOCAL) && stage() != DOWNLOADING && stage() != UNPACKING) checkNow();
}

bool Updater::available() const { std::lock_guard<std::mutex> lk(mu); return found.ok; }
std::string Updater::newVersion() const { std::lock_guard<std::mutex> lk(mu); return found.ok ? found.version : ""; }
std::string Updater::notes() const { std::lock_guard<std::mutex> lk(mu); return found.ok ? found.notes : ""; }
long long Updater::newSize() const { std::lock_guard<std::mutex> lk(mu); return found.ok ? found.size : 0; }

bool Updater::startDownload() {
    if (!isUrl(source)) return false;
    Found f; { std::lock_guard<std::mutex> lk(mu); f = found; }
    if (!f.ok) return false;
    if (stage() == DOWNLOADING || stage() == UNPACKING) return true;
    if (stage() == READY && dlVersion == f.version) return true;
    finishDownload(true);
    { std::lock_guard<std::mutex> lk(mu); dlError.clear(); }
    cancel = false; got = 0; total = f.size; stageV = DOWNLOADING;
    fs::path dir = workDir(dryRun, saveDir);
    std::string url = zipUrl(f);
    dlTask = std::async(std::launch::async, [this, f, dir, url]() {
        std::error_code ec; fs::create_directories(dir, ec);
        fs::path zip = dir / fs::u8path(f.zip), unpacked = dir / ("v" + f.version);
        auto fail = [&](const std::string& why) { fs::remove(zip, ec); fs::remove_all(unpacked, ec); { std::lock_guard<std::mutex> lk(mu); dlError = why; } stageV = FAILED; };
        fs::remove(zip, ec);
        std::string body, err;
        if (!fetch(url, zip.u8string(), body, err)) { fail(err); return; }
        stageV = UNPACKING;
        std::string sum = sha256File(zip.u8string());
        if (lower(sum) != lower(f.sha256)) { fail("The downloaded file doesn't match the release (its SHA-256 checksum is wrong), so it wasn't used."); return; }
        fs::remove_all(unpacked, ec);
        if (!unzip(zip.u8string(), unpacked.u8string(), err)) { fail(err); return; }
        if (!fs::exists(unpacked / "Triffall.exe", ec)) { fail("The download has no Triffall.exe in it."); return; }
        fs::remove(zip, ec);
        { std::lock_guard<std::mutex> lk(mu); unpackedDir = unpacked.u8string(); dlVersion = f.version; }
        stageV = READY;
    });
    return true;
}

void Updater::finishDownload(bool wait) {
    if (!dlTask.valid()) return;
    if (!wait && dlTask.wait_for(std::chrono::seconds(0)) != std::future_status::ready) return;
    dlTask.get();
    if (cancel) { stageV = IDLE; cancel = false; }
}

void Updater::cancelDownload() { if (stage() == DOWNLOADING || stage() == UNPACKING) cancel = true; else if (stage() == FAILED) stageV = IDLE; }

// ---- the plan the helper follows ----
void Updater::writePlan(const std::string& path, const Plan& p) {
    std::ofstream o(fs::u8path(path), std::ios::binary);
    o << "# Triffall updater plan, written by the game (v" TRIFFALL_VERSION "). Safe to delete.\n";
    o << "pid=" << p.pid << "\ndest=" << p.dest << "\nfrom=" << p.from << "\nversion=" << p.version << "\nresult=" << p.result
      << "\nsaves=" << p.saves << "\ninstall=" << p.install << "\ncleanup=" << p.cleanup << "\nlog=" << p.log
      << "\nrestartgame=" << (p.restartGame ? 1 : 0) << "\ntestfail=" << (p.testFail ? 1 : 0) << "\n";
    for (auto& a : p.restart) o << "arg=" << a << "\n";
}
bool Updater::readPlan(const std::string& path, Plan& p) {
    std::string t = readAll(fs::u8path(path)); if (t.empty()) return false;
    std::stringstream ss(t); std::string l;
    while (std::getline(ss, l)) {
        if (!l.empty() && l.back() == '\r') l.pop_back();
        size_t eq = l.find('='); if (l.empty() || l[0] == '#' || eq == std::string::npos) continue;
        std::string k = l.substr(0, eq), v = l.substr(eq + 1);
        if (k == "pid") p.pid = strtoul(v.c_str(), nullptr, 10);
        else if (k == "dest") p.dest = v; else if (k == "from") p.from = v; else if (k == "version") p.version = v;
        else if (k == "result") p.result = v; else if (k == "saves") p.saves = v; else if (k == "install") p.install = v;
        else if (k == "cleanup") p.cleanup = v; else if (k == "log") p.log = v;
        else if (k == "restartgame") p.restartGame = v == "1"; else if (k == "testfail") p.testFail = v == "1";
        else if (k == "arg") p.restart.push_back(v);
    }
    return !p.dest.empty() && !p.from.empty();
}

// 1.7.1 (#148): the swap. The player's own changes to assets\ (Editor edits, campaigns they made or added) are found by
// comparing the old copy with its assets-manifest.txt (how assets\ looked when that version was installed):
//  - the new release has the same file as before (or doesn't have it): the player's version goes into the new copy
//  - the new release changed it too: the release's version is used, the player's goes to saves\editor-edits-<version>\
//  - a whole campaign the player deleted stays deleted when the release didn't change it
// Same rules as install.ps1 / tools\pull-editor-edits.ps1 use for the owner's install.
bool Updater::apply(const Plan& p, std::string& text) {
    fs::path dest = fs::u8path(p.dest), from = fs::u8path(p.from);
    std::error_code ec;
    logLine(p.log, "updating " + p.dest + " to " + p.version + " from " + p.from);
    if (!fs::exists(from / "Triffall.exe", ec)) { text = "The new version's folder (" + p.from + ") has no Triffall.exe."; return false; }
#ifdef TRIFFALL_CAN_UPDATE
    if (!p.install.empty()) {   // the owner's standard install: install.ps1 (it also brings Editor edits back into the project)
        logLine(p.log, "running " + p.install);
        wchar_t sysdir[MAX_PATH]; GetSystemDirectoryW(sysdir, MAX_PATH);
        std::wstring ps = std::wstring(sysdir) + L"\\WindowsPowerShell\\v1.0\\powershell.exe";
        std::wstring cmd = L"\"" + ps + L"\" -NoProfile -NonInteractive -ExecutionPolicy Bypass -File \"" + fs::u8path(p.install).wstring() + L"\"";
        SECURITY_ATTRIBUTES sa{sizeof sa, nullptr, TRUE};
        HANDLE out = p.log.empty() ? INVALID_HANDLE_VALUE : CreateFileW(fs::u8path(p.log).wstring().c_str(), FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE, &sa, OPEN_ALWAYS, 0, nullptr);
        STARTUPINFOW si{}; si.cb = sizeof si; PROCESS_INFORMATION pi{};
        if (out != INVALID_HANDLE_VALUE) { si.dwFlags = STARTF_USESTDHANDLES; si.hStdOutput = out; si.hStdError = out; si.hStdInput = GetStdHandle(STD_INPUT_HANDLE); }
        std::vector<wchar_t> line(cmd.begin(), cmd.end()); line.push_back(0);
        std::wstring wd = fs::u8path(p.install).parent_path().wstring();
        BOOL ok = CreateProcessW(ps.c_str(), line.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr, wd.c_str(), &si, &pi);
        if (out != INVALID_HANDLE_VALUE) CloseHandle(out);
        if (!ok) { text = "install.ps1 couldn't be started (error " + std::to_string(GetLastError()) + ")."; return false; }
        WaitForSingleObject(pi.hProcess, 15 * 60 * 1000);
        DWORD code = 1; GetExitCodeProcess(pi.hProcess, &code); CloseHandle(pi.hThread); CloseHandle(pi.hProcess);
        if (code != 0) { text = "install.ps1 stopped (exit code " + std::to_string(code) + "; see " + p.log + ")."; return false; }
        text.clear(); return true;
    }
#endif
    // 1) what the player changed, before anything moves
    auto oldMan = readManifest((dest / "assets-manifest.txt").u8string());
    bool haveMan = !oldMan.empty();
    auto now = hashAssets((dest / "assets").u8string());
    auto newMan = readManifest((from / "assets-manifest.txt").u8string());
    bool newHadMan = !newMan.empty();
    if (!newHadMan) newMan = hashAssets((from / "assets").u8string());
    std::vector<std::string> keep, aside, goneCamps;
    for (auto& kv : now) {
        const std::string& rel = kv.first; const std::string& h = kv.second;
        auto b = oldMan.find(rel);
        if (haveMan) { if (b != oldMan.end() && b->second == h) continue; }   // as installed
        else if (newMan.count(rel)) continue;   // no manifest (very old copy): only files the release doesn't have are surely the player's
        auto n = newMan.find(rel);
        if (n != newMan.end() && n->second == h) continue;   // the release has exactly that
        if (n == newMan.end() || (b != oldMan.end() && n->second == b->second)) keep.push_back(rel);
        else aside.push_back(rel);
    }
    for (auto& kv : oldMan) {
        const std::string& rel = kv.first;
        size_t a = rel.find('\\'), b = a == std::string::npos ? a : rel.find('\\', a + 1);
        if (a == std::string::npos || b == std::string::npos || lower(rel.substr(0, a)) != "campaigns" || lower(rel.substr(b + 1)) != "campaign.txt" || now.count(rel)) continue;
        std::string id = rel.substr(a + 1, b - a - 1), prefix = rel.substr(0, b + 1);
        if (isCoreCampaign(lower(id))) continue;
        bool same = true;
        for (auto& nk : newMan) if (startsWith(nk.first, prefix)) { auto o = oldMan.find(nk.first); if (o == oldMan.end() || o->second != nk.second) { same = false; break; } }
        if (same) goneCamps.push_back(id);
    }
    logLine(p.log, "player's changes: " + std::to_string(keep.size()) + " kept, " + std::to_string(aside.size()) + " saved aside, " + std::to_string(goneCamps.size()) + " deleted campaign(s)");

    // 2) the swap: the old files wait in .update-old until the new ones are all in
    auto keepName = [](const std::string& n) { std::string l = lower(n); return l == "saves" || l == "update-source.txt" || l == ".update-old"; };
    fs::path old = dest / ".update-old";
    fs::path asideDir = fs::u8path(p.saves) / ("editor-edits-" + p.version);
    bool asideExisted = fs::exists(asideDir, ec);
    bool hadSource = fs::exists(dest / "update-source.txt", ec);
    fs::remove_all(old, ec);
    fs::create_directories(old, ec);
    std::vector<std::string> moved;
    try {
        std::vector<fs::path> entries;
        for (auto& e : fs::directory_iterator(dest)) entries.push_back(e.path());
        for (auto& e : entries) { std::string n = e.filename().u8string(); if (keepName(n)) continue; fs::rename(e, old / e.filename()); moved.push_back(n); }
        for (auto& e : fs::directory_iterator(from)) {
            std::string n = e.path().filename().u8string();
            if (lower(n) == "update-source.txt" && hadSource) continue;   // the copy keeps where it updates from
            if (lower(n) == "saves") continue;
            fs::copy(e.path(), dest / e.path().filename(), fs::copy_options::recursive | fs::copy_options::overwrite_existing);
        }
        if (!newHadMan) writeManifest(dest / "assets-manifest.txt", newMan);
        { std::ofstream v(dest / "version.txt", std::ios::binary); v << p.version << "\r\n"; if (!v) throw std::runtime_error("couldn't write version.txt"); }
        if (p.testFail) throw std::runtime_error("a test asked the update to fail here");
        for (auto& rel : keep) {
            fs::path to = dest / "assets" / fs::u8path(rel); fs::create_directories(to.parent_path());
            fs::copy_file(old / "assets" / fs::u8path(rel), to, fs::copy_options::overwrite_existing);
        }
        for (auto& rel : aside) {
            fs::path to = asideDir / fs::u8path(rel); fs::create_directories(to.parent_path());
            fs::copy_file(old / "assets" / fs::u8path(rel), to, fs::copy_options::overwrite_existing);
        }
        for (auto& id : goneCamps) fs::remove_all(dest / "assets" / "campaigns" / fs::u8path(id));
    } catch (std::exception& e) {
        std::string why = e.what();
        logLine(p.log, "copy failed, putting the old version back: " + why);
        std::vector<fs::path> now2;
        for (fs::directory_iterator it(dest, ec), end; !ec && it != end; it.increment(ec)) now2.push_back(it->path());
        for (auto& e2 : now2) {
            std::string n = e2.filename().u8string(); std::error_code e3;
            if (keepName(n) && !(lower(n) == "update-source.txt" && !hadSource)) continue;
            fs::remove_all(e2, e3);
        }
        for (auto& n : moved) { std::error_code e3; fs::rename(old / fs::u8path(n), dest / fs::u8path(n), e3); if (e3) logLine(p.log, "couldn't put back " + n + ": " + e3.message()); }
        fs::remove_all(old, ec);
        if (!asideExisted) fs::remove_all(asideDir, ec);
        text = "Copying the new version failed (" + why + "). The old version was put back.";
        return false;
    }
    fs::remove_all(old, ec);
    // 3) say what happened to the player's changes
    text.clear();
    if (!keep.empty()) text = "Your own changes (" + std::to_string(keep.size()) + " file" + (keep.size() == 1 ? "" : "s") + " made in the Editor or added) were kept.";
    if (!aside.empty()) {
        text += std::string(text.empty() ? "" : " ") + "The update also changed " + std::to_string(aside.size()) + " file" + (aside.size() == 1 ? "" : "s") +
            " you had edited (" + aside[0] + (aside.size() > 1 ? ", ..." : "") + "): the game now uses the new version, and yours " + (aside.size() == 1 ? "is" : "are") + " in " + asideDir.u8string() + ".";
    }
    logLine(p.log, "done. " + text);
    return true;
}

int Updater::applyMain(const std::string& planArg) {
    std::string planPath = planArg;
#ifdef TRIFFALL_CAN_UPDATE
    { int n = 0; LPWSTR* a = CommandLineToArgvW(GetCommandLineW(), &n); if (a && n >= 3) planPath = fs::path(a[2]).u8string(); if (a) LocalFree(a); }
#endif
    Plan p;
    if (!readPlan(planPath, p)) return 1;
    logLine(p.log, "helper started");
    std::string text; bool ok = false;
#ifdef TRIFFALL_CAN_UPDATE
    if (p.pid) {   // wait for the game to close
        HANDLE h = OpenProcess(SYNCHRONIZE, FALSE, p.pid);
        if (h) { DWORD w = WaitForSingleObject(h, 60000); CloseHandle(h); if (w == WAIT_TIMEOUT) text = "Triffall did not close."; }
        Sleep(400);
    }
#endif
    if (text.empty()) { try { ok = apply(p, text); } catch (std::exception& e) { ok = false; text = e.what(); } }
    if (!ok) logLine(p.log, "failed: " + text);
    std::replace(text.begin(), text.end(), '\n', ' '); std::replace(text.begin(), text.end(), '\r', ' ');
    { std::ofstream o(fs::u8path(p.result), std::ios::binary); o << (ok ? "ok" : "fail") << "\r\n" << p.version << "\r\n" << text << "\r\n"; }
    std::error_code ec;
    if (!p.cleanup.empty()) fs::remove_all(fs::u8path(p.cleanup), ec);
    fs::remove(fs::u8path(planPath), ec);
#ifdef TRIFFALL_CAN_UPDATE
    if (p.restartGame) {
        fs::path exe = fs::u8path(p.dest) / "Triffall.exe";
        std::wstring cmd = L"\"" + exe.wstring() + L"\"";
        for (auto& a : p.restart) cmd += L" \"" + fs::u8path(a).wstring() + L"\"";
        STARTUPINFOW si{}; si.cb = sizeof si; PROCESS_INFORMATION pi{};
        std::vector<wchar_t> line(cmd.begin(), cmd.end()); line.push_back(0);
        std::wstring wd = fs::u8path(p.dest).wstring();
        if (CreateProcessW(exe.wstring().c_str(), line.data(), nullptr, nullptr, FALSE, 0, nullptr, wd.c_str(), &si, &pi)) { CloseHandle(pi.hThread); CloseHandle(pi.hProcess); }
        else logLine(p.log, "couldn't start " + exe.u8string() + " (error " + std::to_string(GetLastError()) + ")");
    }
#endif
    return ok ? 0 : 2;
}

bool Updater::launch(std::string& err) {
    Found f; { std::lock_guard<std::mutex> lk(mu); f = found; }
    if (source.empty() || !f.ok) { err = "There is no update to install."; return false; }
    Plan p;
    p.version = f.version;
    if (isUrl(source)) {
        std::lock_guard<std::mutex> lk(mu);
        if (stage() != READY || dlVersion != f.version || unpackedDir.empty()) { err = "The update hasn't been downloaded yet."; return false; }
        p.from = p.cleanup = unpackedDir;
    } else p.from = (fs::u8path(source) / fs::u8path(f.version)).u8string();
    // the folder to update: where this exe really is
    fs::path exe = fs::path(exeDir) / "Triffall.exe";
    unsigned long pid = 0;
#ifdef TRIFFALL_CAN_UPDATE
    { wchar_t buf[32768]; DWORD n = GetModuleFileNameW(nullptr, buf, 32768); if (n > 0 && n < 32768 && !dryRun) exe = fs::path(std::wstring(buf, n)); }
    pid = GetCurrentProcessId();
#endif
    fs::path dest = exe.parent_path();
    std::error_code ec;
    fs::path dir = workDir(dryRun, saveDir);
    fs::create_directories(dir, ec);
    if (!fs::is_directory(dir, ec)) { err = "Couldn't make a folder for the updater in " + dir.u8string() + "."; return false; }
    p.pid = dryRun ? 0 : pid; p.dest = dest.u8string(); p.saves = fs::path(saveDir).u8string();
    p.result = (fs::path(saveDir) / "update-result.txt").u8string(); p.log = (dir / "update.log").u8string();
    if (!savesArg.empty()) { p.restart.push_back("--saves"); p.restart.push_back(fs::path(savesArg).u8string()); }
    if (!isUrl(source)) {   // the owner's standard install, updated from the project's releases\: install.ps1 also keeps Editor edits
        fs::path install = fs::u8path(source).parent_path() / "install.ps1";
        const char* la = getenv("LOCALAPPDATA");
        if (la && fs::exists(install, ec)) {
            std::string a = lower(fs::path(dest).lexically_normal().u8string()), b = lower((fs::path(la) / "Programs" / "Triffall").lexically_normal().u8string());
            while (!a.empty() && (a.back() == '\\' || a.back() == '/')) a.pop_back();
            while (!b.empty() && (b.back() == '\\' || b.back() == '/')) b.pop_back();
            if (a == b) p.install = install.u8string();
        }
    }
    fs::path plan = dir / ("update-" + std::to_string(pid) + ".txt");
    writePlan(plan.u8string(), p);
    if (!fs::exists(plan, ec)) { err = "Couldn't write the updater's plan (" + plan.u8string() + ")."; return false; }
    scriptPath = plan.u8string();
    if (dryRun) return true;
#ifdef TRIFFALL_CAN_UPDATE
    // the helper is this very exe, copied out of the way (the game's folder is about to be replaced)
    fs::path helper = dir / ("triffall-updater-" + std::to_string(pid) + ".exe");
    fs::copy_file(exe, helper, fs::copy_options::overwrite_existing, ec);
    if (ec) { err = "Couldn't prepare the updater (" + ec.message() + ")."; return false; }
    std::wstring cmd = L"\"" + helper.wstring() + L"\" --apply-update \"" + plan.wstring() + L"\"";
    std::wstring wdir = dir.wstring();
    STARTUPINFOW si{}; si.cb = sizeof si; PROCESS_INFORMATION pi{};
    std::vector<wchar_t> line(cmd.begin(), cmd.end()); line.push_back(0);
    // it must outlive the game: break away from a job the game may run in (when the job allows it)
    BOOL ok = CreateProcessW(helper.wstring().c_str(), line.data(), nullptr, nullptr, FALSE, CREATE_NEW_PROCESS_GROUP | CREATE_BREAKAWAY_FROM_JOB, nullptr, wdir.c_str(), &si, &pi);
    if (!ok) { std::vector<wchar_t> l2(cmd.begin(), cmd.end()); l2.push_back(0); ok = CreateProcessW(helper.wstring().c_str(), l2.data(), nullptr, nullptr, FALSE, CREATE_NEW_PROCESS_GROUP, nullptr, wdir.c_str(), &si, &pi); }
    if (!ok) { err = "The updater couldn't be started (error " + std::to_string(GetLastError()) + ")."; return false; }
    CloseHandle(pi.hThread); CloseHandle(pi.hProcess);
    return true;
#else
    err = "Updating from inside the game isn't possible on this system.";
    return false;
#endif
}
