// Software renderer: everything in the game draws into a Canvas (RGBA framebuffer).
// The platform layer uploads the finished framebuffer to the screen once per frame.
#pragma once
#include <cstdint>
#include <string>
#include <vector>

struct Color8 { uint8_t r, g, b, a; };
constexpr Color8 RGB(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255) { return {r, g, b, a}; }

// A few named colors used everywhere.
namespace C {
    constexpr Color8 Black   = {0, 0, 0, 255};
    constexpr Color8 White   = {255, 255, 255, 255};
    constexpr Color8 Gray    = {128, 128, 128, 255};
    constexpr Color8 LtGray  = {192, 192, 192, 255};
    constexpr Color8 DkGray  = {64, 64, 64, 255};
    constexpr Color8 Teal    = {0, 128, 128, 255};
    constexpr Color8 Navy    = {0, 0, 128, 255};
    constexpr Color8 Blue    = {16, 132, 208, 255};
    constexpr Color8 Red     = {218, 0, 0, 255};
    constexpr Color8 Green   = {0, 153, 0, 255};
    constexpr Color8 Yellow  = {255, 220, 60, 255};
    constexpr Color8 Orange  = {255, 140, 0, 255};
    constexpr Color8 Purple  = {168, 44, 234, 255};
    constexpr Color8 Cyan    = {0, 220, 220, 255};
    constexpr Color8 Pink    = {255, 100, 170, 255};
    constexpr Color8 Brown   = {170, 97, 32, 255};
    constexpr Color8 Cream   = {240, 230, 200, 255};
    constexpr Color8 Panel   = {24, 24, 32, 255};
    constexpr Color8 PanelHi = {52, 52, 70, 255};
    constexpr Color8 Gold    = {230, 190, 60, 255};
    constexpr Color8 Trans   = {0, 0, 0, 0};
}

struct Image8 {
    int w = 0, h = 0;
    std::vector<Color8> px;
    bool load(const std::string& path);
    Color8 get(int x, int y) const { return px[(size_t)y * w + x]; }
};

struct Rect { int x, y, w, h; bool contains(int px, int py) const { return px >= x && py >= y && px < x + w && py < y + h; } };

// Reference to a 12x12 tile in the tileset by (column,row).
struct Tile { int16_t c = -1, r = -1; bool valid() const { return c >= 0; } };
constexpr Tile T(int c, int r) { return Tile{(int16_t)c, (int16_t)r}; }

class Canvas {
public:
    int w = 0, h = 0;
    std::vector<Color8> px;
    Rect clip{0, 0, 0, 0};

    void init(int W, int H) { w = W; h = H; px.assign((size_t)W * H, C::Black); clip = {0, 0, W, H}; }
    void clear(Color8 c) { for (auto& p : px) p = c; }
    void setClip(Rect r);
    void resetClip() { clip = {0, 0, w, h}; }
    inline void put(int x, int y, Color8 c) {
        if (x < clip.x || y < clip.y || x >= clip.x + clip.w || y >= clip.y + clip.h) return;
        if (c.a == 255) { px[(size_t)y * w + x] = c; return; }
        if (c.a == 0) return;
        Color8& d = px[(size_t)y * w + x];
        int a = c.a, ia = 255 - a;
        d.r = (uint8_t)((c.r * a + d.r * ia) / 255);
        d.g = (uint8_t)((c.g * a + d.g * ia) / 255);
        d.b = (uint8_t)((c.b * a + d.b * ia) / 255);
        d.a = 255;
    }
    void fillRect(int x, int y, int rw, int rh, Color8 c);
    void rect(int x, int y, int rw, int rh, Color8 c);          // 1px outline
    void line(int x0, int y0, int x1, int y1, Color8 c);
    void hline(int x, int y, int len, Color8 c) { fillRect(x, y, len, 1, c); }
    void vline(int x, int y, int len, Color8 c) { fillRect(x, y, 1, len, c); }
    // Blit a region of an image with integer scale; alpha-blended; optional tint (multiply) and
    // "recolor" (replace every opaque non-black pixel with a single color, used for tinting 1-bit sprites).
    void blit(const Image8& img, int sx, int sy, int sw, int sh, int dx, int dy, int scale = 1,
              Color8 tint = C::White, bool flipX = false);
    void blitRecolor(const Image8& img, int sx, int sy, int sw, int sh, int dx, int dy, int scale, Color8 color);
    // Dim / darken the whole canvas (for modal overlays)
    void dim(int x, int y, int rw, int rh, uint8_t alpha);
};

// The tileset: 12x12 tiles at 13px pitch starting at (1,1). Includes a bitmap font.
class Tileset {
public:
    Image8 img;
    static constexpr int TS = 12;  // tile size
    static constexpr int PITCH = 13;
    bool load(const std::string& path);
    // Draw tile (col,row) at pixel position with scale; black background pixels of the tile become transparent.
    void draw(Canvas& cv, Tile t, int dx, int dy, int scale = 1, Color8 tint = C::White, bool flipX = false) const;
    void drawRecolor(Canvas& cv, Tile t, int dx, int dy, int scale, Color8 color) const;
    // Draw tile keeping black background opaque (for floor/wall tiles that are meant to be opaque)
    void drawOpaque(Canvas& cv, Tile t, int dx, int dy, int scale = 1, Color8 tint = C::White) const;
    // 1.0.14: a 1px ring (and the small holes inside the picture) in one colour, drawn before the sprite itself, so a
    // character drawn for a black background stays readable on grass, sand or floors.
    void drawOutline(Canvas& cv, Tile t, int dx, int dy, int scale, Color8 color, bool flipX = false) const;
    // 1.0.14: draws a character so it reads against the ground: a very dark picture (the grey warrior) is drawn lighter,
    // keeping its near-black details. Same as draw() for everything bright enough already.
    void drawLit(Canvas& cv, Tile t, int dx, int dy, int scale, bool flipX = false) const;
    Image8 tilesTransparent; // copy of img with pure-black made transparent
};

// Bitmap font. Two faces, chosen in Settings > Access > Font (GameSettings::fontStyle):
//   STYLE_CLASSIC - the tileset's own glyphs (rows 44-49), the game's original look
//   STYLE_CLEAR   - Spleen 6x12 (built in, src/core/font_spleen.h), plainer letterforms that are easier to read
// Both are proportional, 12px line height, so every layout works with either.
class Font {
public:
    enum { STYLE_CLASSIC = 0, STYLE_CLEAR = 1, STYLE_COUNT = 2 };
    static const char* styleName(int s) { return s == STYLE_CLEAR ? "Clear" : "Classic"; }
    struct Glyph { Tile t; int x0 = 0, x1 = 0; }; // trimmed horizontal extent inside the 12px cell
    Glyph glyphs[128];
    Glyph boldGlyphs[128];
    struct ClearGlyph { int x0 = 0, x1 = 0; bool valid = false; };
    ClearGlyph clear[128];
    bool bold = false;
    int style = STYLE_CLASSIC;
    void build(const Tileset& ts);
    int width(const std::string& s, int scale = 1) const;
    int lineHeight(int scale = 1) const { return 12 * scale; }
    // Draw text; returns pixel width drawn.
    int draw(Canvas& cv, const Tileset& ts, int x, int y, const std::string& s, Color8 color, int scale = 1) const;
    int drawShadow(Canvas& cv, const Tileset& ts, int x, int y, const std::string& s, Color8 color, Color8 shadow, int scale = 1) const;
    // Word-wrap into lines no wider than maxW
    std::vector<std::string> wrap(const std::string& s, int maxW, int scale = 1) const;
    int drawWrapped(Canvas& cv, const Tileset& ts, int x, int y, int maxW, const std::string& s, Color8 color, int scale = 1, int lineGap = 1) const;
};

bool savePNG(const Canvas& cv, const std::string& path);
