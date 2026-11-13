// 1.1.11 (#35): word and line helpers shared by the Editor's text boxes (single-line fields and the multi-line editor).
// Word = letters, digits, '_' (Notepad's rule). Ctrl+Left/Right, Ctrl+Backspace/Delete and double-click use these.
#pragma once
#include <string>
#include <cctype>
#include <algorithm>

namespace txt {
// 0 = space, 1 = word character, 2 = punctuation (each punctuation run is its own "word")
inline int cls(char c) { unsigned char u = (unsigned char)c; if (c == ' ' || c == '\t') return 0; if (isalnum(u) || c == '_' || u >= 128) return 1; return 2; }
inline int clampPos(const std::string& s, int p) { return p < 0 ? 0 : p > (int)s.size() ? (int)s.size() : p; }
// start of the word before p (skips spaces first)
inline int wordLeft(const std::string& s, int p) {
    p = clampPos(s, p);
    while (p > 0 && cls(s[p - 1]) == 0) p--;
    if (p > 0) { int c = cls(s[p - 1]); while (p > 0 && cls(s[p - 1]) == c) p--; }
    return p;
}
// start of the next word (Windows: skips the rest of this word, then the spaces after it)
inline int wordRight(const std::string& s, int p) {
    p = clampPos(s, p); int n = (int)s.size();
    if (p < n) { int c = cls(s[p]); if (c != 0) while (p < n && cls(s[p]) == c) p++; }
    while (p < n && cls(s[p]) == 0) p++;
    return p;
}
// the word (or run of spaces / punctuation) at p, for double-click: [a, b)
inline void wordAt(const std::string& s, int p, int& a, int& b) {
    int n = (int)s.size(); if (n == 0) { a = b = 0; return; }
    p = clampPos(s, p); if (p >= n) p = n - 1;
    if (p > 0 && cls(s[p]) != 1 && cls(s[p - 1]) == 1) p--;   // just after a word: take that word
    int c = cls(s[p]); a = p; b = p + 1;
    if (c != 2) { while (a > 0 && cls(s[a - 1]) == c) a--; while (b < n && cls(s[b]) == c) b++; }
    else { while (a > 0 && s[a - 1] == s[p]) a--; while (b < n && s[b] == s[p]) b++; }   // "====" goes as one
}
inline int indentOf(const std::string& s) { int i = 0; while (i < (int)s.size() && s[i] == ' ') i++; return i; }
inline bool isWordAt(const std::string& s, size_t pos, size_t len) {   // s[pos, pos+len) is a whole word
    if (pos > 0 && cls(s[pos - 1]) == 1) return false;
    if (pos + len < s.size() && cls(s[pos + len]) == 1) return false;
    return true;
}
inline std::string lower(std::string v) { for (auto& c : v) c = (char)tolower((unsigned char)c); return v; }
// find 'q' in 's' from 'from' (case / whole word options); -1 = none
inline int findIn(const std::string& s, const std::string& q, int from, bool matchCase, bool wholeWord) {
    if (q.empty() || from > (int)s.size()) return -1;
    std::string hs = matchCase ? s : lower(s), hq = matchCase ? q : lower(q);
    size_t p = hs.find(hq, (size_t)std::max(0, from));
    while (p != std::string::npos) { if (!wholeWord || isWordAt(s, p, q.size())) return (int)p; p = hs.find(hq, p + 1); }
    return -1;
}
inline int rfindIn(const std::string& s, const std::string& q, int before, bool matchCase, bool wholeWord) {   // last match that starts before 'before'
    if (q.empty() || before <= 0) return -1;
    int best = -1, p = -1;
    while ((p = findIn(s, q, p + 1, matchCase, wholeWord)) >= 0 && p < before) best = p;
    return best;
}
}
