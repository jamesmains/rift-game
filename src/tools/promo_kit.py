#!/usr/bin/env python3
"""Builds Triffall's image kit (itch.io, Steam, social, icons, wallpaper, screenshots) from headless screenshots.

    python src/tools/promo_kit.py <raw_screenshot_dir> <press_kit_dir>

tools/make-presskit.ps1 runs it after src/tools/presskit_shots.txt (960x540 game shots: the canvas a 1920x1080 screen
gives the game; 1280x720 Editor shots). press-kit/README.md lists every file it writes.

1.6.3 (#139): everything is the game's own pixels at whole-number scales. The TRIFFALL logo is drawn from the game's
classic bold letters (tiles.png rows 47-49, as the boot screen draws it), all other text is the game's Clear face
(Spleen 6x12 from src/src/core/font_spleen.h), the window frame and taskbar are the desktop's retro 90s look. Each
picture is laid out on a small "pixel canvas" and scaled up with nearest-neighbour only. No system fonts, no painted
or generated art (Steam asks about AI-made content).
"""
import os, re, sys, glob, math
from PIL import Image, ImageFilter

HERE = os.path.dirname(os.path.abspath(__file__))
SRC = os.path.dirname(HERE)                       # src/
RAW, KIT = sys.argv[1], sys.argv[2]
ART = os.path.join(KIT, "art")
DIRS = {k: os.path.join(ART, k) for k in ("steam", "itch", "icons")}
SHOTS_HD = os.path.join(KIT, "screenshots", "16x9_1920x1080")
SHOTS_NATIVE = os.path.join(KIT, "screenshots", "native")
for d in list(DIRS.values()) + [SHOTS_HD, SHOTS_NATIVE]:
    os.makedirs(d, exist_ok=True)

TAGLINE = "An old-school open-world fantasy game for players who love classic party RPGs."   # 1.8.2 (#167)
SHORT = "an old-school open-world party RPG"

# ---------------------------------------------------------------- the game's colours
TEAL = (0, 128, 128)           # the desktop
GREY, WHITE, DARK, BLACK, LIGHT = (192, 192, 192), (255, 255, 255), (128, 128, 128), (0, 0, 0), (223, 223, 223)
NAVY, TITLE2 = (0, 0, 128), (16, 132, 208)        # title bar gradient
AMBER, AMBER_SH = (255, 205, 90), (90, 50, 0)     # the boot screen's TRIFFALL
BIOS_GREY, BIOS_DIM, BIOS_RULE = (200, 200, 200), (120, 120, 120), (70, 70, 70)
CREAM = (200, 190, 150)                           # "an old-school RPG" under the boot logo
SKY, SKY2, RIFT, RIFT_HI = (22, 14, 40), (44, 28, 76), (150, 90, 230), (235, 220, 255)   # the launcher banner

# ---------------------------------------------------------------- fonts (the game's own)
def _load_spleen():
    text = open(os.path.join(SRC, "src", "core", "font_spleen.h"), encoding="utf-8").read()
    rows = re.findall(r"\{((?:0x[0-9a-fA-F]{2},?){12})\}", text)
    return [[int(v, 16) for v in r.split(",") if v] for r in rows]
SPLEEN = _load_spleen()
assert len(SPLEEN) == 95, len(SPLEEN)

def _clear_px(ch, x, y):           # gfx.cpp clearPx (CLEAR_YOFF 1)
    r = y - 1; o = ord(ch)
    if o < 32 or o > 126 or r < 0 or r >= 12 or x < 0 or x >= 6: return False
    return (SPLEEN[o - 32][r] >> (5 - x)) & 1

CLEAR = {}
for o in range(33, 127):
    xs = [x for y in range(12) for x in range(6) if _clear_px(chr(o), x, y)]
    if xs: CLEAR[chr(o)] = (min(xs), max(xs) + 1)

TILES = Image.open(os.path.join(SRC, "assets", "tiles.png")).convert("RGB")
CLASSIC_BOLD = {}
def _map_row(row, col0, chars):    # gfx.cpp mapRow
    for i, ch in enumerate(chars):
        bx, by = 1 + (col0 + i) * 13, 1 + row * 13
        on = [(x, y) for y in range(12) for x in range(12) if TILES.getpixel((bx + x, by + y)) != (0, 0, 0)]
        x0 = min(x for x, _ in on); x1 = max(x for x, _ in on) + 1
        CLASSIC_BOLD[ch] = (bx, by, x0, x1)
_map_row(47, 97, "ABCDEF"); _map_row(48, 78, "GHIJKLMNOPQRSTUVWXYZabcde"); _map_row(49, 78, "fghijklmnopqrstuvwxyz")

def text_width(s, scale=1, bold=False):
    w = 0
    for ch in s:
        if ch == " ": w += 4 * scale
        elif ch not in CLEAR: w += 5 * scale
        else: x0, x1 = CLEAR[ch]; w += (x1 - x0 + 1 + (1 if bold else 0)) * scale
    return w

def text(img, x, y, s, color, scale=1, bold=False, shadow=None):
    """The Clear face exactly as Font::draw draws it (synthetic bold = each pixel also covers the one to its right)."""
    if shadow: text(img, x + scale, y + scale, s, shadow, scale, bold)
    px = img.load(); cx = x; col = tuple(color) + ((255,) if img.mode == "RGBA" else ())
    for ch in s:
        if ch == " ": cx += 4 * scale; continue
        if ch not in CLEAR: cx += 5 * scale; continue
        x0, x1 = CLEAR[ch]; gw = x1 - x0
        for py in range(12):
            for pxx in range(gw + (1 if bold else 0)):
                on = (pxx < gw and _clear_px(ch, x0 + pxx, py)) or (bold and pxx > 0 and _clear_px(ch, x0 + pxx - 1, py))
                if not on: continue
                for sy in range(scale):
                    for sx in range(scale):
                        X, Y = cx + pxx * scale + sx, y + py * scale + sy
                        if 0 <= X < img.width and 0 <= Y < img.height: px[X, Y] = col
        cx += (gw + 1 + (1 if bold else 0)) * scale
    return cx - x

def wrap(s, maxw, scale=1, bold=False):
    """Splits s into as few lines as fit maxw, balanced (the longest line as short as it can be)."""
    words = s.split()
    for n in range(1, len(words) + 1):
        best = None
        # try every split into n lines (n is small): greedy on a shrinking width finds the most even one
        for target in range(text_width(s, scale, bold) // n, maxw + 1, 2):
            lines, cur = [], ""
            for w in words:
                t = (cur + " " + w).strip()
                if text_width(t, scale, bold) <= target or not cur: cur = t
                else: lines.append(cur); cur = w
            lines.append(cur)
            if len(lines) <= n and all(text_width(l, scale, bold) <= maxw for l in lines): best = lines; break
        if best: return best
    return [s]

def text_lines(img, cx, y, lines, color, scale=1, bold=False, shadow=None, gap=2):
    """Lines centred on cx; returns the y under the last one."""
    for ln in lines:
        text(img, cx - text_width(ln, scale, bold) // 2, y, ln, color, scale, bold, shadow)
        y += (12 + gap) * scale
    return y

def logo(scale, color=AMBER, shadow=AMBER_SH, word="TRIFFALL"):
    """TRIFFALL in the classic bold letters, as Desktop::drawBoot draws it (shadow 3/4 of a letter pixel down-right)."""
    sh = max(1, scale * 3 // 4) if shadow else 0
    w = sum((CLASSIC_BOLD[c][3] - CLASSIC_BOLD[c][2] + 1) * scale for c in word) + sh
    im = Image.new("RGBA", (w, 12 * scale + sh), (0, 0, 0, 0)); px = im.load()
    for off, col in (((sh, sh), shadow), ((0, 0), color)) if shadow else (((0, 0), color),):
        cx = off[0]
        for c in word:
            bx, by, x0, x1 = CLASSIC_BOLD[c]
            for y in range(12):
                for x in range(x0, x1):
                    if TILES.getpixel((bx + x, by + y)) == (0, 0, 0): continue
                    for sy in range(scale):
                        for sx in range(scale): px[cx + (x - x0) * scale + sx, off[1] + y * scale + sy] = col + (255,)
            cx += (x1 - x0 + 1) * scale
    return im.crop(im.getbbox())

# ---------------------------------------------------------------- pixel helpers
def up(im, k):
    return im.resize((im.width * k, im.height * k), Image.NEAREST)

def shot(name):
    return Image.open(os.path.join(RAW, name)).convert("RGB")

def crop_at(im, w, h, cx, cy):
    """A w x h piece of a screenshot centred on (cx, cy), kept inside the picture."""
    x = int(min(max(0, cx - w // 2), im.width - w)); y = int(min(max(0, cy - h // 2), im.height - h))
    return im.crop((x, y, x + w, y + h))

BAYER = [[0, 8, 2, 10], [12, 4, 14, 6], [3, 11, 1, 9], [15, 7, 13, 5]]
def dither_fade(im, side, width, color=BLACK):
    """Fades a pixel picture into `color` over `width` pixels at one side, with 4x4 ordered dithering (no blur)."""
    im = im.copy(); px = im.load()
    for y in range(im.height):
        for i in range(width):
            x = i if side == "left" else im.width - 1 - i if side == "right" else None
            if x is None: continue
            if (i + 0.5) / width * 16 < BAYER[y % 4][x % 4] + 0.5: px[x, y] = color
    return im

def bevel(img, x, y, w, h, outer=(LIGHT, BLACK), inner=(WHITE, DARK)):
    """Retro raised edge: outer and inner light (top/left) and dark (bottom/right) lines."""
    px = img.load()
    for i, (lt, dk) in enumerate((outer, inner)):
        for X in range(x + i, x + w - i): px[X, y + i] = lt; px[X, y + h - 1 - i] = dk
        for Y in range(y + i, y + h - i): px[x + i, Y] = lt; px[x + w - 1 - i, Y] = dk
        px[x + w - 1 - i, y + i] = dk; px[x + i, y + h - 1 - i] = dk

def window(img, x, y, w, h, title, client):
    """A retro window like the desktop's (grey frame, navy-to-blue title bar, close box) around `client`."""
    px = img.load()
    for Y in range(y, y + h):
        for X in range(x, x + w): px[X, Y] = GREY
    bevel(img, x, y, w, h)
    tx, ty, tw, th = x + 3, y + 3, w - 6, 16
    for X in range(tx, tx + tw):
        t = (X - tx) / max(1, tw - 1)
        c = tuple(int(NAVY[i] + (TITLE2[i] - NAVY[i]) * t) for i in range(3))
        for Y in range(ty, ty + th): px[X, Y] = c
    text(img, tx + 4, ty + 2, title, WHITE, bold=True)
    bx, by = tx + tw - 16, ty + 2                          # close box
    for Y in range(by, by + 12):
        for X in range(bx, bx + 14): px[X, Y] = GREY
    bevel(img, bx, by, 14, 12, outer=(WHITE, BLACK), inner=(GREY, DARK))
    for i in range(6):
        for d in (0, 1):
            px[bx + 4 + i + d, by + 3 + i] = BLACK; px[bx + 9 - i + d, by + 3 + i] = BLACK
    cx, cy = x + 3, ty + th + 1
    cw, ch = w - 6, h - (cy - y) - 3
    img.paste(client.crop((0, 0, cw, ch)) if client.size != (cw, ch) else client, (cx, cy))
    return cw, ch

def client_size(w, h):
    return w - 6, h - 23

DESK = shot("10_desktop.png")
def taskbar(w):
    """The desktop's own taskbar (Start button left, clock right), stretched to w by repeating its middle."""
    bar = DESK.crop((0, DESK.height - 28, DESK.width, DESK.height))
    out = Image.new("RGB", (w, 28)); out.paste(bar.crop((0, 0, 78, 28)), (0, 0))
    mid = bar.crop((200, 0, 201, 28))
    for X in range(78, w - 78): out.paste(mid, (X, 0))
    out.paste(bar.crop((bar.width - 78, 0, bar.width, 28)), (w - 78, 0))
    return out

def desktop_icons(h):
    """The column of desktop icons (Triffall, My Computer, Minesweeper, Solitaire, Shut Down) from the real desktop."""
    return DESK.crop((4, 0, 92, min(h, 278)))

def paste_rgba(dst, src, xy):
    dst.paste(src, xy, src)

def save(im, path, **kw):
    im.save(path, **kw); print("wrote", os.path.relpath(path, KIT), im.size)

COMBAT = shot("art_combat.png")       # the fight in Hollowmere's fields, no target box
FIGHT = (655, 240)                    # its middle in 960x540 coordinates
MAP_BOX = (0, 30, 924, 418)           # the map without the action bar, turn strip, key hints, hotbar, party bar
COMBAT_MAP = COMBAT.crop(MAP_BOX)
FIGHT_MAP = (FIGHT[0], FIGHT[1] - 30)

# ---------------------------------------------------------------- icon (16x16, scaled by whole numbers)
ICON16 = [
    # . clear  k black edge  s sky  S lighter sky  r rift glow  w rift bolt  a amber T  b its shadow
    ".kkkkkkkkkkkkkk.",
    "kSSSSSSSSSSSSSSk",
    "ksssssssssssrwsk",
    "ksaaaaaaaasrwssk",
    "ksaaaaaaaabrwssk",
    "kssbbaabbbbwsssk",
    "kssssaabsssrwwsk",
    "kssssaabssssrwsk",
    "kssssaabsssrwssk",
    "kssssaabsssrwssk",
    "kssssaabssrwsssk",
    "kssssaabssrwsssk",
    "kssssaabsssrwssk",
    "ksssssbbsssssssk",
    "kssssssssssssssk",
    ".kkkkkkkkkkkkkk.",
]
ICON_COLORS = {"k": BLACK, "s": SKY, "S": SKY2, "r": RIFT, "w": RIFT_HI, "a": AMBER, "b": AMBER_SH}

def icon_base():
    im = Image.new("RGBA", (16, 16), (0, 0, 0, 0)); px = im.load()
    for y, row in enumerate(ICON16):
        for x, c in enumerate(row):
            if c != ".": px[x, y] = ICON_COLORS[c] + (255,)
    return im

def icon(size):
    base = icon_base()
    k = max(1, size // 16); big = up(base, k)
    out = Image.new("RGBA", (size, size), (0, 0, 0, 0)); out.alpha_composite(big, ((size - big.width) // 2, (size - big.height) // 2))
    return out

# ---------------------------------------------------------------- the kit
def cover():
    """itch.io cover 630x500 (also Idea Jotter's project cover): the fight inside a retro window on the desktop."""
    W, H, k = 315, 250, 2
    c = Image.new("RGB", (W, H), TEAL)
    lg = logo(3); paste_rgba(c, lg, ((W - lg.width) // 2, 9))
    sw = text_width(SHORT); text(c, (W - sw) // 2, 9 + lg.height + 3, SHORT, WHITE, shadow=(0, 64, 64))
    wy = 9 + lg.height + 20; wh = H - 28 - wy - 2
    cw, ch = client_size(W - 16, wh)
    # 1.8.2 (#167): the aimed shot (hit and crit chances over the goblin) says "tactical" at thumbnail size
    window(c, 8, wy, W - 16, wh, "Triffall", crop_at(shot("01_combat.png"), cw, ch, *COVER_FOCUS))
    c.paste(taskbar(W), (0, H - 28))
    return up(c, k)

COVER_FOCUS = (690, 232)              # the target box and the fight under it in 01_combat

def banner():
    """itch.io banner 960x300: the boot screen's logo and BIOS lines, the fight fading in from the right."""
    W, H, k = 480, 150, 2
    c = Image.new("RGB", (W, H), BLACK)
    scene = dither_fade(crop_at(COMBAT, 236, H, FIGHT[0] - 6, FIGHT[1] - 10), "left", 64)
    c.paste(scene, (W - scene.width, 0))
    lg = logo(3); paste_rgba(c, lg, (12, 12))
    text(c, 14, 12 + lg.height + 3, SHORT, CREAM)
    for i in range(W // 2 + 10):
        c.putpixel((12 + i, 12 + lg.height + 20), BIOS_RULE)
    lines = ["Triffall BIOS v1.0 (C) 1998", "Memory Test: 65536K OK", "Loading Aldmoor...", "Starting Triffall..._"]
    for i, ln in enumerate(lines): text(c, 12, 12 + lg.height + 26 + i * 14, ln, BIOS_GREY)
    return up(c, k)

def social():
    """Social card 1200x630: logo, the tagline, and the fight in a window on the desktop (icons and taskbar)."""
    W, H, k = 600, 315, 2
    c = Image.new("RGB", (W, H), TEAL)
    c.paste(desktop_icons(H - 28), (0, 0))
    lg = logo(4); lx = 96 + (W - 96 - lg.width) // 2; paste_rgba(c, lg, (lx, 10))
    ty = text_lines(c, 96 + (W - 96) // 2, 10 + lg.height + 6, wrap(TAGLINE, W - 112, bold=True), WHITE, bold=True,
                    shadow=(0, 64, 64), gap=1)
    wy = ty + 6; wh = H - 28 - wy - 3
    cw, ch = client_size(W - 104, wh)
    window(c, 96, wy, W - 104, wh, "Triffall", crop_at(COMBAT, cw, ch, *FIGHT))
    c.paste(taskbar(W), (0, H - 28))
    return up(c, k)

def desktop_wallpaper():
    """1920x1080 press-kit wallpaper: the game's own desktop at 2x."""
    return up(DESK, 2) if DESK.size == (960, 540) else DESK.resize((1920, 1080), Image.NEAREST)

BG_BASE, BG_INK = (0, 78, 80), (0, 98, 100)     # the desktop's teal, darkened (#167: "darkened a bit for the itch page")
BG_CELL = 30                                    # pixel-canvas cell; 960x540 = 32x18 cells, both even

def _game_sprites():
    """Every creature, class and item picture the base game uses (sheet 0 col,row), in file order."""
    out = []
    d = os.path.join(SRC, "assets", "data")
    for f, key in (("classes.txt", "sprite"), ("creatures.txt", "sprite"), ("items.txt", "tile")):
        for m in re.finditer(r"^%s=(\d+),(\d+)\s*$" % key, open(os.path.join(d, f), encoding="utf-8").read(), re.M):
            t = (int(m.group(1)), int(m.group(2)))
            if t not in out: out.append(t)
    return out

def sprite_mask(col, row):
    """The 12x12 picture as a set of lit pixels (anything not black)."""
    bx, by = 1 + col * 13, 1 + row * 13
    return [(x, y) for y in range(12) for x in range(12) if TILES.getpixel((bx + x, by + y)) != (0, 0, 0)]

def background(w=960, h=540, k=2):
    """1920x1080 itch.io page background, tileable both ways: the game's monsters, heroes and gear as faint 1-bit
    stamps on dark teal, on every other cell of a 30-pixel grid (a checkerboard, so rows stagger). The picture is a
    whole number of 60x60 repeats and no stamp crosses an edge, so copies placed side by side or stacked join with
    no seam."""
    import random
    rnd = random.Random(167)
    pool = [t for t in _game_sprites() if len(sprite_mask(*t)) >= 18]
    c = Image.new("RGB", (w, h), BG_BASE); px = c.load()
    cols, rows = w // BG_CELL, h // BG_CELL
    assert cols * BG_CELL == w and rows * BG_CELL == h and cols % 2 == 0 and rows % 2 == 0
    last = []
    for r in range(rows):
        for q in range(cols):
            if (q + r) % 2: continue
            t = rnd.choice(pool)
            while t in last: t = rnd.choice(pool)
            last = (last + [t])[-12:]
            ox, oy = q * BG_CELL + (BG_CELL - 12) // 2, r * BG_CELL + (BG_CELL - 12) // 2
            for x, y in sprite_mask(*t): px[ox + x, oy + y] = BG_INK
    return up(c, k)

def avatar():
    return icon(512)

# Steam capsules: the fight under the logo. Valve allows only the art and the game's name on capsules (no tagline).
FIGHT_TOP = 138                       # just above the goblins' row in COMBAT_MAP
def capsule(w, h, logo_frac, sc=None, focus=FIGHT_MAP, with_logo=True):
    """Lays the picture out on a w/k x h/k pixel canvas (k = the largest whole number that divides both sides, up to
    4, keeping at least 240x120 of the game's pixels) and scales it up. The logo sits on a black band at the top that
    dithers into the fight, whose top row starts right under it."""
    k = max(1, min(w // 240, h // 120, 4))
    while (w % k or h % k) and k > 1: k -= 1
    pw, ph = w // k, h // k
    sc = COMBAT_MAP if sc is None else sc
    if pw > sc.width or ph > sc.height:   # wider than the screenshot: the scene doubled inside the canvas first
        zs = max(math.ceil(pw / sc.width), math.ceil(ph / sc.height))
        sc = up(sc, zs); focus = (focus[0] * zs, focus[1] * zs)
    else: zs = 1
    if not with_logo:
        return up(crop_at(sc, pw, ph, *focus), k)
    lg = None
    for s in range(12, 0, -1):
        lg = logo(s)
        if lg.width <= pw * 0.9 and lg.height <= ph * logo_frac: break
    pad = max(4, lg.height // 4); band_h = lg.height + pad * 2; fade = 8
    c = crop_at(sc, pw, ph, focus[0], FIGHT_TOP * zs - band_h - fade // 2 + ph // 2)
    bp = c.load()
    for y in range(band_h + fade):
        for x in range(pw):
            if y < band_h or (y - band_h + 0.5) / fade * 16 < BAYER[y % 4][x % 4] + 0.5: bp[x, y] = BLACK
    paste_rgba(c, lg, ((pw - lg.width) // 2, pad))
    return up(c, k)

def write_icons():
    for s in (16, 32, 48, 64, 128, 256, 512):
        save(icon(s), os.path.join(DIRS["icons"], f"icon_{s}.png"))
    big = icon(256)
    big.save(os.path.join(DIRS["icons"], "app.ico"), sizes=[(16, 16), (32, 32), (48, 48), (64, 64), (128, 128), (256, 256)])
    print("wrote art/icons/app.ico")
    # the game's own copy for the exe and the window (src/platform/triffall.rc)
    big.save(os.path.join(SRC, "src", "platform", "app.ico"), sizes=[(16, 16), (32, 32), (48, 48), (64, 64), (128, 128), (256, 256)])
    # the browser build's favicon (inline in src/web/shell.html)
    import base64, io
    buf = io.BytesIO(); icon(32).save(buf, "PNG"); fav = base64.b64encode(buf.getvalue()).decode()
    shell = os.path.join(SRC, "web", "shell.html"); html = open(shell, encoding="utf-8").read()
    html2 = re.sub(r'(<link rel="icon" type="image/png" href="data:image/png;base64,)[^"]*(">)', lambda m: m.group(1) + fav + m.group(2), html)
    if html2 != html: open(shell, "w", encoding="utf-8", newline="\n").write(html2); print("updated the favicon in src/web/shell.html")
    icon(32).save(os.path.join(DIRS["steam"], "client_icon_32x32.ico"), sizes=[(32, 32)])
    ci = Image.new("RGB", (184, 184), SKY); i176 = icon(176); ci.paste(i176, (4, 4), i176)
    save(ci, os.path.join(DIRS["steam"], "community_icon_184x184.jpg"), quality=95)

def write_screenshots():
    # all store screenshots at 1920x1080: game shots 2x; Editor shots (1280x720) 3x then halved so its text stays even
    n = 0
    for src in sorted(glob.glob(os.path.join(RAW, "*.png"))):
        name = os.path.basename(src)
        if name.startswith("art_") or not name[:2].isdigit(): continue
        im = Image.open(src).convert("RGB")
        if im.size == (960, 540): out = up(im, 2)
        elif im.size == (1280, 720): out = im.resize((3840, 2160), Image.NEAREST).resize((1920, 1080), Image.BOX)
        else: continue
        out.save(os.path.join(SHOTS_HD, name), optimize=True); n += 1
    print("screenshots 1920x1080:", n)
    # five at the size the game draws them (no scaling): fight, overworld, talk, the desktop launcher, the Editor
    for name in ("01_combat.png", "04_overworld.png", "06_dialogue.png", "11_launcher.png", "16_editor_maps.png"):
        save(shot(name), os.path.join(SHOTS_NATIVE, name), optimize=True)

if __name__ == "__main__":
    save(cover(), os.path.join(KIT, "cover.png"), optimize=True)
    save(banner(), os.path.join(DIRS["itch"], "banner_960x300.png"), optimize=True)
    save(social(), os.path.join(ART, "social_card_1200x630.png"), optimize=True)
    save(avatar(), os.path.join(ART, "avatar_512x512.png"), optimize=True)
    save(background(), os.path.join(ART, "background_1920x1080.png"), optimize=True)
    save(desktop_wallpaper(), os.path.join(ART, "wallpaper_desktop_1920x1080.png"), optimize=True)
    write_icons()
    ST = DIRS["steam"]
    save(capsule(920, 430, 0.24), os.path.join(ST, "header_capsule_920x430.png"), optimize=True)
    save(capsule(462, 174, 0.30), os.path.join(ST, "small_capsule_462x174.png"), optimize=True)
    save(capsule(1232, 706, 0.22), os.path.join(ST, "main_capsule_1232x706.png"), optimize=True)
    save(capsule(600, 900, 0.14), os.path.join(ST, "library_capsule_600x900.png"), optimize=True)
    save(capsule(748, 896, 0.14), os.path.join(ST, "vertical_capsule_748x896.png"), optimize=True)
    # library hero: no logo (Steam lays the library logo over it); library logo: the logo alone, transparent
    save(capsule(3840, 1240, 0, shot("04_overworld.png").crop((0, 0, 960, 466)), (480, 233), with_logo=False), os.path.join(ST, "library_hero_3840x1240.png"), optimize=True)
    lg = logo(12); li = Image.new("RGBA", (1280, 720), (0, 0, 0, 0)); li.alpha_composite(lg, ((1280 - lg.width) // 2, (720 - lg.height) // 2))
    save(li, os.path.join(ST, "library_logo_1280x720.png"))
    pb = up(crop_at(shot("05_town.png"), 719, 405, 480, 230), 2).filter(ImageFilter.GaussianBlur(1.2))
    pb = Image.blend(pb, Image.new("RGB", pb.size, BLACK), 0.45)
    save(pb, os.path.join(ST, "page_background_1438x810.jpg"), quality=88)
    # key art: the fight, logo and tagline (not for Steam capsules)
    ka = capsule(1920, 1080, 0.20)
    kl = wrap(TAGLINE, 460, bold=True); kh = 8 + 13 * len(kl) + 1
    kp = Image.new("RGB", (480, kh), BLACK)
    text_lines(kp, 240, 4, kl, WHITE, bold=True, gap=1)
    ka.paste(up(kp, 4), (0, 1080 - kh * 4))
    save(ka, os.path.join(ART, "key_art_1920x1080.png"), optimize=True)
    write_screenshots()
