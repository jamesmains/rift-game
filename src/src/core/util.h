#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstdarg>
#include <cctype>

// --------- RNG (xorshift, deterministic) ---------
struct Rng {
    uint64_t s = 0x9E3779B97F4A7C15ULL;
    explicit Rng(uint64_t seed = 12345) { reseed(seed); }
    void reseed(uint64_t seed) { s = seed * 0x9E3779B97F4A7C15ULL + 0xD1B54A32D192ED03ULL; if (!s) s = 1; next(); next(); }
    uint64_t next() { s ^= s << 13; s ^= s >> 7; s ^= s << 17; return s; }
    int range(int lo, int hi) { if (hi <= lo) return lo; return lo + (int)(next() % (uint64_t)(hi - lo + 1)); } // inclusive
    float frand() { return (float)(next() % 1000000) / 1000000.0f; }
    bool chance(int percent) { return range(1, 100) <= percent; }
    template <class T> T& pick(std::vector<T>& v) { return v[range(0, (int)v.size() - 1)]; }
    template <class T> const T& pick(const std::vector<T>& v) const { return v[const_cast<Rng*>(this)->range(0, (int)v.size() - 1)]; }
};

// --------- strings ---------
inline std::string trim(const std::string& s) {
    size_t a = s.find_first_not_of(" \t\r\n"); if (a == std::string::npos) return "";
    size_t b = s.find_last_not_of(" \t\r\n"); return s.substr(a, b - a + 1);
}
inline std::vector<std::string> split(const std::string& s, char sep) {
    std::vector<std::string> out; std::string cur;
    for (char c : s) { if (c == sep) { out.push_back(cur); cur.clear(); } else cur += c; }
    out.push_back(cur); return out;
}
inline bool startsWith(const std::string& s, const std::string& p) { return s.compare(0, p.size(), p) == 0; }
inline std::string fmt(const char* f, ...) {
    char buf[1024]; va_list ap; va_start(ap, f); vsnprintf(buf, sizeof buf, f, ap); va_end(ap); return buf;
}
inline std::string toUpper(std::string s) { for (auto& c : s) c = (char)toupper((unsigned char)c); return s; }
inline std::string toLower(std::string s) { for (auto& c : s) c = (char)tolower((unsigned char)c); return s; }
inline int clampi(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }

// --------- files ---------
inline bool readFile(const std::string& path, std::string& out) {
    FILE* f = fopen(path.c_str(), "rb"); if (!f) return false;
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    out.resize((size_t)n); if (n > 0) { size_t rd = fread(&out[0], 1, (size_t)n, f); (void)rd; }
    fclose(f); return true;
}
inline bool writeFile(const std::string& path, const std::string& data) {
    FILE* f = fopen(path.c_str(), "wb"); if (!f) return false;
    fwrite(data.data(), 1, data.size(), f); fclose(f); return true;
}
inline bool fileExists(const std::string& path) { FILE* f = fopen(path.c_str(), "rb"); if (!f) return false; fclose(f); return true; }

// --------- simple key=value / section config parser ---------
// Format:  [section]  then  key = value  lines. '#' comments.
struct ConfigSection { std::string name; std::vector<std::pair<std::string, std::string>> kv;
    std::string get(const std::string& k, const std::string& def = "") const { for (auto& p : kv) if (p.first == k) return p.second; return def; }
    int geti(const std::string& k, int def = 0) const { auto v = get(k); return v.empty() ? def : atoi(v.c_str()); }
    bool has(const std::string& k) const { for (auto& p : kv) if (p.first == k) return true; return false; }
};
inline std::vector<ConfigSection> parseConfig(const std::string& text) {
    std::vector<ConfigSection> secs;
    for (auto& raw : split(text, '\n')) {
        std::string l = trim(raw);
        if (l.empty() || l[0] == '#') continue;
        if (l[0] == '[') { ConfigSection s; s.name = trim(l.substr(1, l.find(']') - 1)); secs.push_back(s); continue; }
        size_t eq = l.find('=');
        if (eq == std::string::npos) continue;
        if (secs.empty()) secs.push_back(ConfigSection{});
        secs.back().kv.push_back({trim(l.substr(0, eq)), trim(l.substr(eq + 1))});
    }
    return secs;
}
