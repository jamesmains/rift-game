#include "gfx.h"
#include "font_spleen.h"
#include <cstring>
#include <cstdlib>
#include <algorithm>
#include <vector>

#define STB_IMAGE_STATIC
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#include "stb_image.h"
#define STB_IMAGE_WRITE_STATIC
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

bool Image8::load(const std::string& path) {
    int n = 0;
    unsigned char* data = stbi_load(path.c_str(), &w, &h, &n, 4);
    if (!data) return false;
    px.resize((size_t)w * h);
    memcpy(px.data(), data, (size_t)w * h * 4);
    stbi_image_free(data);
    return true;
}

void Canvas::setClip(Rect r) {
    int x0 = std::max(0, r.x), y0 = std::max(0, r.y);
    int x1 = std::min(w, r.x + r.w), y1 = std::min(h, r.y + r.h);
    clip = {x0, y0, std::max(0, x1 - x0), std::max(0, y1 - y0)};
}

void Canvas::fillRect(int x, int y, int rw, int rh, Color8 c) {
    int x0 = std::max(x, clip.x), y0 = std::max(y, clip.y);
    int x1 = std::min(x + rw, clip.x + clip.w), y1 = std::min(y + rh, clip.y + clip.h);
    if (x1 <= x0 || y1 <= y0) return;
    if (c.a == 255) {
        for (int yy = y0; yy < y1; yy++) {
            Color8* row = &px[(size_t)yy * w];
            for (int xx = x0; xx < x1; xx++) row[xx] = c;
        }
    } else {
        for (int yy = y0; yy < y1; yy++) for (int xx = x0; xx < x1; xx++) put(xx, yy, c);
    }
}

void Canvas::rect(int x, int y, int rw, int rh, Color8 c) {
    fillRect(x, y, rw, 1, c); fillRect(x, y + rh - 1, rw, 1, c);
    fillRect(x, y, 1, rh, c); fillRect(x + rw - 1, y, 1, rh, c);
}

void Canvas::line(int x0, int y0, int x1, int y1, Color8 c) {
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    while (true) {
        put(x0, y0, c);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

void Canvas::blit(const Image8& img, int sx, int sy, int sw, int sh, int dx, int dy, int scale, Color8 tint, bool flipX) {
    bool tinted = !(tint.r == 255 && tint.g == 255 && tint.b == 255 && tint.a == 255);
    for (int y = 0; y < sh; y++) {
        int iy = sy + y; if (iy < 0 || iy >= img.h) continue;
        for (int x = 0; x < sw; x++) {
            int ix = sx + (flipX ? (sw - 1 - x) : x); if (ix < 0 || ix >= img.w) continue;
            Color8 c = img.px[(size_t)iy * img.w + ix];
            if (c.a == 0) continue;
            if (tinted) {
                c.r = (uint8_t)(c.r * tint.r / 255); c.g = (uint8_t)(c.g * tint.g / 255);
                c.b = (uint8_t)(c.b * tint.b / 255); c.a = (uint8_t)(c.a * tint.a / 255);
            }
            if (scale == 1) put(dx + x, dy + y, c);
            else fillRect(dx + x * scale, dy + y * scale, scale, scale, c);
        }
    }
}

void Canvas::blitRecolor(const Image8& img, int sx, int sy, int sw, int sh, int dx, int dy, int scale, Color8 color) {
    for (int y = 0; y < sh; y++) {
        int iy = sy + y; if (iy < 0 || iy >= img.h) continue;
        for (int x = 0; x < sw; x++) {
            int ix = sx + x; if (ix < 0 || ix >= img.w) continue;
            Color8 c = img.px[(size_t)iy * img.w + ix];
            if (c.a == 0) continue;
            Color8 o = color; o.a = (uint8_t)(c.a * color.a / 255);
            if (scale == 1) put(dx + x, dy + y, o);
            else fillRect(dx + x * scale, dy + y * scale, scale, scale, o);
        }
    }
}

void Canvas::dim(int x, int y, int rw, int rh, uint8_t alpha) { fillRect(x, y, rw, rh, Color8{0, 0, 0, alpha}); }

// ---------------- Tileset ----------------
bool Tileset::load(const std::string& path) {
    if (!img.load(path)) return false;
    tilesTransparent = img;
    for (auto& p : tilesTransparent.px) if (p.r == 0 && p.g == 0 && p.b == 0) p.a = 0;
    // each cell's black pixels that a flood from the cell's edge (4-way, through black) can't reach are part of the picture
    tilesInside = img;
    for (auto& p : tilesInside.px) p = Color8{0, 0, 0, 0};
    for (int cy = 0; 1 + cy * PITCH + TS <= img.h; cy++) for (int cx = 0; 1 + cx * PITCH + TS <= img.w; cx++) {
        int bx = 1 + cx * PITCH, by = 1 + cy * PITCH;
        auto black = [&](int x, int y) { Color8 c = img.px[(size_t)(by + y) * img.w + bx + x]; return c.r == 0 && c.g == 0 && c.b == 0; };
        bool any = false; for (int y = 0; y < TS && !any; y++) for (int x = 0; x < TS; x++) if (!black(x, y)) { any = true; break; }
        if (!any) continue;
        uint8_t seen[TS][TS] = {}; int stack[TS * TS], n = 0;
        for (int i = 0; i < TS; i++) { int e[4][2] = {{i, 0}, {i, TS - 1}, {0, i}, {TS - 1, i}};
            for (auto& q : e) if (!seen[q[1]][q[0]] && black(q[0], q[1])) { seen[q[1]][q[0]] = 1; stack[n++] = q[1] * TS + q[0]; } }
        while (n > 0) { int v = stack[--n], x = v % TS, y = v / TS; int d[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
            for (auto& q : d) { int nx = x + q[0], ny = y + q[1];
                if (nx >= 0 && ny >= 0 && nx < TS && ny < TS && !seen[ny][nx] && black(nx, ny)) { seen[ny][nx] = 1; stack[n++] = ny * TS + nx; } } }
        for (int y = 0; y < TS; y++) for (int x = 0; x < TS; x++) if (black(x, y) && !seen[y][x]) tilesInside.px[(size_t)(by + y) * img.w + bx + x] = Color8{0, 0, 0, 255};
    }
    return true;
}
void Tileset::drawBacking(Canvas& cv, Tile t, int dx, int dy, int scale, bool flipX) const {
    if (!t.valid()) return;
    cv.blit(tilesInside, 1 + t.c * PITCH, 1 + t.r * PITCH, TS, TS, dx, dy, scale, C::White, flipX);
}
void Tileset::draw(Canvas& cv, Tile t, int dx, int dy, int scale, Color8 tint, bool flipX) const {
    if (!t.valid()) return;
    cv.blit(tilesTransparent, 1 + t.c * PITCH, 1 + t.r * PITCH, TS, TS, dx, dy, scale, tint, flipX);
}
void Tileset::drawRecolor(Canvas& cv, Tile t, int dx, int dy, int scale, Color8 color) const {
    if (!t.valid()) return;
    cv.blitRecolor(tilesTransparent, 1 + t.c * PITCH, 1 + t.r * PITCH, TS, TS, dx, dy, scale, color);
}
void Tileset::drawOpaque(Canvas& cv, Tile t, int dx, int dy, int scale, Color8 tint) const {
    if (!t.valid()) return;
    cv.blit(img, 1 + t.c * PITCH, 1 + t.r * PITCH, TS, TS, dx, dy, scale, tint, false);
}
void Tileset::drawLit(Canvas& cv, Tile t, int dx, int dy, int scale, bool flipX) const {
    if (!t.valid()) return;
    int bx = 1 + t.c * PITCH, by = 1 + t.r * PITCH;
    long sum = 0; int n = 0;   // brightness of the picture's coloured pixels (near-black detail pixels left out)
    for (int y = 0; y < TS; y++) for (int x = 0; x < TS; x++) {
        int sx = bx + x, sy = by + y; if (sx >= img.w || sy >= img.h) continue;
        Color8 c = img.px[(size_t)sy * img.w + sx]; int l = (c.r * 3 + c.g * 6 + c.b) / 10;
        if (l > 24) { sum += l; n++; }
    }
    int lum = n ? (int)(sum / n) : 255;
    if (lum >= 130) { draw(cv, t, dx, dy, scale, C::White, flipX); return; }
    int k = (165 - lum) * 256 / std::max(1, 255 - lum);   // blend the coloured pixels towards white so they land near 165
    for (int y = 0; y < TS; y++) for (int x = 0; x < TS; x++) {
        int sx = bx + (flipX ? TS - 1 - x : x), sy = by + y; if (sx >= img.w || sy >= img.h) continue;
        Color8 c = tilesTransparent.px[(size_t)sy * tilesTransparent.w + sx];
        if (c.a == 0) continue;
        if ((c.r * 3 + c.g * 6 + c.b) / 10 > 24) { c.r = (uint8_t)(c.r + (255 - c.r) * k / 256); c.g = (uint8_t)(c.g + (255 - c.g) * k / 256); c.b = (uint8_t)(c.b + (255 - c.b) * k / 256); }
        cv.fillRect(dx + x * scale, dy + y * scale, scale, scale, c);
    }
}

// ---------------- Font ----------------
static void mapRow(const Tileset& ts, Font::Glyph* out, int row, int col0, const char* chars) {
    for (int i = 0; chars[i]; i++) {
        unsigned char ch = (unsigned char)chars[i];
        if (ch >= 128) continue;
        Font::Glyph g; g.t = T(col0 + i, row);
        int x0 = 12, x1 = -1;
        int bx = 1 + g.t.c * Tileset::PITCH, by = 1 + g.t.r * Tileset::PITCH;
        for (int y = 0; y < 12; y++) for (int x = 0; x < 12; x++) {
            Color8 c = ts.tilesTransparent.get(bx + x, by + y);
            if (c.a > 0) { x0 = std::min(x0, x); x1 = std::max(x1, x); }
        }
        if (x1 < 0) { x0 = 0; x1 = 3; }
        g.x0 = x0; g.x1 = x1 + 1;
        out[ch] = g;
    }
}

// The Clear face: Spleen 6x12, shifted down a row so its capitals sit where the classic face's do.
static const int CLEAR_YOFF = 1;
static inline bool clearPx(unsigned char ch, int x, int y) {
    int r = y - CLEAR_YOFF; if (ch < 32 || ch > 126 || r < 0 || r >= 12) return false;
    return (SPLEEN6X12[ch - 32][r] >> (5 - x)) & 1;
}

void Font::build(const Tileset& ts) {
    for (int i = 0; i < 128; i++) {
        clear[i] = ClearGlyph{};
        if (i <= 32 || i > 126) continue;
        int x0 = 6, x1 = -1;
        for (int y = 0; y < 12; y++) for (int x = 0; x < 6; x++) if (clearPx((unsigned char)i, x, y)) { x0 = std::min(x0, x); x1 = std::max(x1, x); }
        if (x1 < 0) continue;
        clear[i].x0 = x0; clear[i].x1 = x1 + 1; clear[i].valid = true;
    }
    for (int i = 0; i < 128; i++) { glyphs[i] = Glyph{}; boldGlyphs[i] = Glyph{}; }
    mapRow(ts, glyphs, 44, 78, "ABCDEFGHIJKLMNOPQRST12345");
    mapRow(ts, glyphs, 45, 78, "UVWXYZabcdefghijklmn67890");
    mapRow(ts, glyphs, 46, 78, "opqrstuvwxyz()[]{}<>+-?!^");
    mapRow(ts, glyphs, 47, 78, ":#_@%~$\"'&*=`|/\\.,;");
    mapRow(ts, boldGlyphs, 47, 97, "ABCDEF");
    mapRow(ts, boldGlyphs, 48, 78, "GHIJKLMNOPQRSTUVWXYZabcde");
    mapRow(ts, boldGlyphs, 49, 78, "fghijklmnopqrstuvwxyz");
    // bold falls back to regular for digits/punctuation
    for (int i = 0; i < 128; i++) if (!boldGlyphs[i].t.valid()) boldGlyphs[i] = glyphs[i];
    // tweak: make 'i','l','!' etc keep natural widths; space width
}

static inline int glyphW(const Font::Glyph& g) { return g.x1 - g.x0; }

int Font::width(const std::string& s, int scale) const {
    if (style == STYLE_CLEAR) {
        int wpx = 0;
        for (unsigned char ch : s) {
            if (ch == ' ') { wpx += 4 * scale; continue; }
            if (ch >= 128 || !clear[ch].valid) { wpx += 5 * scale; continue; }
            wpx += (clear[ch].x1 - clear[ch].x0 + 1 + (bold ? 1 : 0)) * scale;
        }
        return wpx;
    }
    const Glyph* gs = bold ? boldGlyphs : glyphs;
    int wpx = 0;
    for (unsigned char ch : s) {
        if (ch == ' ') { wpx += 4 * scale; continue; }
        if (ch >= 128 || !gs[ch].t.valid()) { wpx += 5 * scale; continue; }
        wpx += (glyphW(gs[ch]) + 1) * scale;
    }
    return wpx;
}

int Font::draw(Canvas& cv, const Tileset& ts, int x, int y, const std::string& s, Color8 color, int scale) const {
    if (style == STYLE_CLEAR) {
        int cx = x;
        for (unsigned char ch : s) {
            if (ch == ' ') { cx += 4 * scale; continue; }
            if (ch >= 128 || !clear[ch].valid) { cx += 5 * scale; continue; }
            const ClearGlyph& g = clear[ch]; int gw = g.x1 - g.x0;
            for (int py = 0; py < 12; py++) for (int px = 0; px < gw + (bold ? 1 : 0); px++) {
                // synthetic bold: each pixel also covers the one to its right (never doubling a pixel with alpha)
                bool on = (px < gw && clearPx(ch, g.x0 + px, py)) || (bold && px > 0 && clearPx(ch, g.x0 + px - 1, py));
                if (!on) continue;
                if (scale == 1) cv.put(cx + px, y + py, color); else cv.fillRect(cx + px * scale, y + py * scale, scale, scale, color);
            }
            cx += (gw + 1 + (bold ? 1 : 0)) * scale;
        }
        return cx - x;
    }
    const Glyph* gs = bold ? boldGlyphs : glyphs;
    int cx = x;
    for (unsigned char ch : s) {
        if (ch == ' ') { cx += 4 * scale; continue; }
        if (ch >= 128 || !gs[ch].t.valid()) { cx += 5 * scale; continue; }
        const Glyph& g = gs[ch];
        int bx = 1 + g.t.c * Tileset::PITCH + g.x0, by = 1 + g.t.r * Tileset::PITCH;
        cv.blitRecolor(ts.tilesTransparent, bx, by, g.x1 - g.x0, 12, cx, y, scale, color);
        cx += (glyphW(g) + 1) * scale;
    }
    return cx - x;
}

int Font::drawShadow(Canvas& cv, const Tileset& ts, int x, int y, const std::string& s, Color8 color, Color8 shadow, int scale) const {
    draw(cv, ts, x + scale, y + scale, s, shadow, scale);
    return draw(cv, ts, x, y, s, color, scale);
}

std::vector<std::string> Font::wrap(const std::string& s, int maxW, int scale) const {
    std::vector<std::string> lines;
    std::string cur, word;
    auto flushWord = [&]() {
        if (word.empty()) return;
        // a single word wider than the line (a long id, a path) is broken where it has to be rather than running off
        while (maxW > 0 && word.size() > 1 && width(word, scale) > maxW) {
            if (!cur.empty()) { lines.push_back(cur); cur.clear(); }
            size_t n = 1; while (n < word.size() && width(word.substr(0, n + 1), scale) <= maxW) n++;
            lines.push_back(word.substr(0, n)); word = word.substr(n);
        }
        std::string test = cur.empty() ? word : cur + " " + word;
        if (width(test, scale) <= maxW || cur.empty()) cur = test;
        else { lines.push_back(cur); cur = word; }
        word.clear();
    };
    for (char ch : s) {
        if (ch == '\n') { flushWord(); lines.push_back(cur); cur.clear(); }
        else if (ch == ' ') flushWord();
        else word += ch;
    }
    flushWord();
    lines.push_back(cur);
    return lines;
}

int Font::drawWrapped(Canvas& cv, const Tileset& ts, int x, int y, int maxW, const std::string& s, Color8 color, int scale, int lineGap) const {
    auto lines = wrap(s, maxW, scale);
    int cy = y;
    for (auto& l : lines) { draw(cv, ts, x, cy, l, color, scale); cy += lineHeight(scale) + lineGap; }
    return cy - y;
}

bool savePNG(const Canvas& cv, const std::string& path) {
    return stbi_write_png(path.c_str(), cv.w, cv.h, 4, cv.px.data(), cv.w * 4) != 0;
}
