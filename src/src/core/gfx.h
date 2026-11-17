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

// Reference to a 12x12 tile by (column,row) on a sprite sheet. s = the sheet (1.2.0): 0 = "tiles" (the campaign's own
// sheets/tiles.png, else the base game's assets/tiles.png), others = the order Tileset::useCampaign registered them.
// Written in data files as "col,row" (sheet 0) or "col,row@sheet" (tileText / parseTile in data.h).
struct Tile { int16_t c = -1, r = -1; uint8_t s = 0; bool valid() const { return c >= 0; } };
constexpr Tile T(int c, int r) { return Tile{(int16_t)c, (int16_t)r, 0}; }
constexpr Tile tileAt(Tile t, int dc, int dr) { return Tile{(int16_t)(t.c + dc), (int16_t)(t.r + dr), t.s}; }   // a neighbour on the same sheet
inline bool sameTile(Tile a, Tile b) { return a.c == b.c && a.r == b.r && a.s == b.s; }
// Sheet names for the active campaign (index = Tile::s). sheetIndex adds an unknown name (it then draws nothing).
int sheetIndex(const std::string& name);
const std::string& sheetName(int s);
int sheetCount();

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
    void fillRaw(int x, int y, int rw, int rh, Color8 c);   // fillRect without the UI colour map (1.8.3)
};
// 1.8.3 (#170): while set (Theme::beginUiMap), fillRect / rect / Font::draw pass their colour through it (kind 0 fill,
// 1 text): older fixed-colour screens take the active UI theme's colours. Never set for the world or the Editor.
extern Color8 (*gCanvasMap)(Color8 c, int kind, int x, int y);

// How a sheet's picture is cut into tiles (sheets/<name>.txt [sheet] section). Every sheet is converted when it loads
// to the base layout (12x12 cells at a 13 px pitch from 1,1; see-through pixels black), so drawing is the same for all.
struct SheetGrid {
    int tw = 12, th = 12, ox = 1, oy = 1, gx = 1, gy = 1;   // tile size, offset of the first tile, gap between tiles
    int clear = 0; Color8 key{0, 0, 0, 255};                // see-through: 0 black (+ transparent pixels), 1 transparent pixels only, 2 the key colour (+ transparent)
    std::string title;
    int cols(int imgW) const { return tw + gx > 0 ? (imgW - ox + gx) / (tw + gx) : 0; }
    int rows(int imgH) const { return th + gy > 0 ? (imgH - oy + gy) / (th + gy) : 0; }
};
SheetGrid readSheetGrid(const std::string& infoPath);       // defaults (the Urizen layout) when the file or keys are missing
std::string sheetGridText(const SheetGrid& g);              // the [sheet] section as written by the Editor
SheetGrid guessSheetGrid(int imgW, int imgH);               // a likely grid for a picture of this size
bool convertSheet(const Image8& src, const SheetGrid& g, Image8& out, int& cols, int& rows);   // to the base layout

// One loaded sprite sheet in the base layout.
struct SheetPics {
    std::string name, path, info;       // name (Tile::s order), the picture file and its sheets/<name>.txt
    bool loaded = false, own = false;   // own = in the campaign's folder (else the base game's)
    int cols = 0, rows = 0; SheetGrid grid;
    Image8 img, trans, inside;          // as Tileset::img / tilesTransparent / tilesInside
    std::vector<uint8_t> empty;         // per cell (row * cols + col): nothing drawn in it
};

// The tileset: 12x12 tiles at 13px pitch starting at (1,1). Includes a bitmap font.
class Tileset {
public:
    Image8 img;
    static constexpr int TS = 12;  // tile size
    static constexpr int PITCH = 13;
    bool load(const std::string& path);
    // 1.2.0 (#36): the sprite sheets of a campaign. Sheet 0 "tiles" = campaignDir/sheets/tiles.png if there is one,
    // else the base game's tiles.png (img above, which the font and desktop always use). Then every other
    // campaignDir/sheets/*.png and the base game's baseDataDir/sheets/*.png (the base ones are always loaded so
    // inherited data draws; `visible` = the ones a campaign's pickers offer). Re-registers the sheet names.
    void useCampaign(const std::string& baseDataDir, const std::string& campaignDir, bool inherit, bool keepOrder = false);   // keepOrder: a reload after the Editor changed a sheet
    std::vector<SheetPics> sheets;
    std::string baseDataDir, campaignDir; bool inherit = true;
    bool visible(int s) const;          // offered in this campaign's pickers
    const SheetPics* sheet(int s) const { return s >= 0 && s < (int)sheets.size() ? &sheets[s] : nullptr; }
    int sheetCols(int s) const;         // size in tiles (0 = nothing loaded)
    int sheetRows(int s) const;
    bool cellEmpty(Tile t) const;       // nothing drawn in the cell (or outside the sheet)
    const Image8& pics(int s, int which) const;   // which: 0 img, 1 transparent, 2 inside
    // Draw tile (col,row) at pixel position with scale; black background pixels of the tile become transparent.
    void draw(Canvas& cv, Tile t, int dx, int dy, int scale = 1, Color8 tint = C::White, bool flipX = false) const;
    void drawRecolor(Canvas& cv, Tile t, int dx, int dy, int scale, Color8 color) const;
    // Draw tile keeping black background opaque (for floor/wall tiles that are meant to be opaque)
    void drawOpaque(Canvas& cv, Tile t, int dx, int dy, int scale = 1, Color8 tint = C::White) const;
    // 1.0.14: draws a character so it reads against the ground: a very dark picture (the grey warrior) is drawn lighter,
    // keeping its near-black details. Same as draw() for everything bright enough already.
    void drawLit(Canvas& cv, Tile t, int dx, int dy, int scale, bool flipX = false) const;
    // 1.0.15: the picture's own black pixels inside its outline (eyes, gaps between arms and body...), drawn black
    // before the sprite, so a character on a trail or a floor looks as it does on the sheet's black instead of the
    // ground showing through it. Black connected to the cell's edge stays see-through (no box around the sprite).
    void drawBacking(Canvas& cv, Tile t, int dx, int dy, int scale, bool flipX = false) const;
    Image8 tilesTransparent; // copy of img with pure-black made transparent
    Image8 tilesInside;      // 1.0.15: only the black pixels not reachable from their cell's edge (drawBacking)
    static void makeDerived(const Image8& img, Image8& trans, Image8& inside, std::vector<uint8_t>* empty = nullptr, int cols = 0, int rows = 0);
private:
    std::vector<uint8_t> baseEmpty; int baseCols = 0, baseRows = 0;
};

// Bitmap font. Two faces; since 1.1.1 every text uses STYLE_CLEAR (no setting), STYLE_CLASSIC only draws the logo:
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
    int style = STYLE_CLEAR;
    void build(const Tileset& ts);
    int width(const std::string& s, int scale = 1) const;
    int lineHeight(int scale = 1) const { return 12 * scale; }
    // Draw text; returns pixel width drawn.
    int draw(Canvas& cv, const Tileset& ts, int x, int y, const std::string& s, Color8 color, int scale = 1) const;
    int drawShadow(Canvas& cv, const Tileset& ts, int x, int y, const std::string& s, Color8 color, Color8 shadow, int scale = 1) const;
    // Word-wrap into lines no wider than maxW
    std::vector<std::string> wrap(const std::string& s, int maxW, int scale = 1) const;
    int drawWrapped(Canvas& cv, const Tileset& ts, int x, int y, int maxW, const std::string& s, Color8 color, int scale = 1, int lineGap = 1) const;
    // 1.8.3 (#170): the small-label face, Spleen 5x8 (8 px line, 5 px cells, proportional like the 6x12), and the
    // tileset's Classic glyphs (rows 44-49) for titles whatever `style` is (bold digits fall back to regular).
    int widthSmall(const std::string& s, int scale = 1) const;
    int drawSmall(Canvas& cv, int x, int y, const std::string& s, Color8 color, int scale = 1) const;
    int widthClassic(const std::string& s, bool boldFace, int scale = 1) const;
    int drawClassic(Canvas& cv, const Tileset& ts, int x, int y, const std::string& s, Color8 color, bool boldFace, int scale = 1) const;
    struct SmallGlyph { int x0 = 0, x1 = 0; bool valid = false; };
    SmallGlyph smallG[128];
};

bool savePNG(const Canvas& cv, const std::string& path);
