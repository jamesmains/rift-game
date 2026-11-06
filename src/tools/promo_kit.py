#!/usr/bin/env python3
"""Builds the itch.io / Steam promo kit from headless screenshots.

    python3 tools/promo_kit.py <raw_screenshot_dir> <out_dir>

Expects 640x480 PNGs captured by triffall_headless (see tools/promo_shots.txt) plus 00_desktop.png for the logo.
Everything is pixel-art: screenshots are upscaled with nearest-neighbour only, never filtered.
"""
import os, sys, glob, math
from PIL import Image, ImageDraw, ImageFont, ImageFilter

RAW, OUT = sys.argv[1], sys.argv[2]
os.makedirs(OUT, exist_ok=True)
for sub in ("steam", "itch", "screenshots_4x3_1920x1440", "screenshots_16x9_1920x1080", "icons"):
    os.makedirs(os.path.join(OUT, sub), exist_ok=True)

FONT_CANDIDATES = (
    ("/usr/share/fonts/truetype/google-fonts/Poppins-Bold.ttf", "/usr/share/fonts/truetype/google-fonts/Poppins-Regular.ttf"),
    (r"C:\Windows\Fonts\arialbd.ttf", r"C:\Windows\Fonts\arial.ttf"),
)
FONT_B = FONT_R = None
for bold_path, regular_path in FONT_CANDIDATES:
    if os.path.exists(bold_path) and os.path.exists(regular_path):
        FONT_B, FONT_R = bold_path, regular_path
        break

def font(size, bold=False):
    path = FONT_B if bold else FONT_R
    return ImageFont.truetype(path, size) if path else ImageFont.load_default(size=size)

# ---------------------------------------------------------------- logo (lifted from the desktop, pixel art)
def extract_logo():
    im = Image.open(os.path.join(RAW, "00_desktop.png")).convert("RGB"); px = im.load()
    xs, ys = [], []
    for y in range(360, 430):
        for x in range(300, 560):
            r, g, b = px[x, y]
            if (r > 200 and g > 180 and b < 160) or (r < 20 and g in range(40, 80) and b in range(40, 80)): xs.append(x); ys.append(y)
    box = (min(xs) - 1, min(ys) - 1, max(xs) + 2, max(ys) + 2)
    crop = im.crop(box).convert("RGBA"); p = crop.load()
    teal = (0, 128, 128)
    for y in range(crop.height):
        for x in range(crop.width):
            r, g, b, a = p[x, y]
            if abs(r - teal[0]) < 10 and abs(g - teal[1]) < 10 and abs(b - teal[2]) < 10: p[x, y] = (0, 0, 0, 0)
    return crop
LOGO = extract_logo()   # ~160x26, transparent

def logo_scaled(h, maxw=None):
    s = max(1, round(h / LOGO.height))
    if maxw: s = max(1, min(s, int(maxw // LOGO.width)))
    return LOGO.resize((LOGO.width * s, LOGO.height * s), Image.NEAREST)

def fit_lines(draw, text, size, maxw, bold):
    """Shrink the font until the text fits; below 60% of the asked size, break it into two lines instead."""
    fnt = font(size, bold)
    while draw.textlength(text, font=fnt) > maxw and size > 10:
        size -= 2; fnt = font(size, bold)
    return [(text, fnt)]

def glow(img, radius, color=(255, 200, 80, 110)):
    """Soft glow behind a transparent pixel logo."""
    pad = radius * 3
    base = Image.new("RGBA", (img.width + pad * 2, img.height + pad * 2), (0, 0, 0, 0))
    mask = Image.new("L", base.size, 0); mask.paste(img.split()[3], (pad, pad))
    mask = mask.filter(ImageFilter.GaussianBlur(radius))
    g = Image.new("RGBA", base.size, color); g.putalpha(mask)
    base.alpha_composite(g); base.alpha_composite(img, (pad, pad))
    return base

def shadow_text(draw, xy, text, fnt, fill=(255, 255, 255), shadow=(0, 0, 0), off=3, anchor="la"):
    x, y = xy
    draw.text((x + off, y + off), text, font=fnt, fill=shadow, anchor=anchor)
    draw.text((x, y), text, font=fnt, fill=fill, anchor=anchor)

def scene(name, crop=True):
    im = Image.open(os.path.join(RAW, name)).convert("RGB")
    if crop:  # keep the map area, drop HUD / turn strip / notifications
        box = (0, 72, 600, 396) if "combat" in name else (0, 40, 640, 396)
        if name.startswith("00_") or name.startswith("09_") or name.startswith("1"): box = (0, 0, 640, 480)
        im = im.crop(box)
    return im

def cover_fit(im, w, h, focus=None):
    """Fill w x h with a pixel-art screenshot: integer nearest upscale, then crop around a focus point (source coords)."""
    s = max(math.ceil(w / im.width), math.ceil(h / im.height))
    big = im.resize((im.width * s, im.height * s), Image.NEAREST)
    fx, fy = (im.width / 2, im.height / 2) if focus is None else focus
    x = int(min(max(0, fx * s - w / 2), big.width - w)); y = int(min(max(0, fy * s - h / 2), big.height - h))
    return big.crop((x, y, x + w, y + h))

def vignette(im, strength=0.55):
    w, h = im.size
    m = Image.new("L", (w, h), 0); d = ImageDraw.Draw(m)
    d.rectangle((0, 0, w, h), fill=int(255 * strength))
    # lighter centre
    grad = Image.radial_gradient("L").resize((w, h))
    m = Image.eval(grad, lambda v: int(v * strength))
    dark = Image.new("RGB", (w, h), (0, 0, 0))
    return Image.composite(dark, im, m)

TAGLINE = "An open-world RPG with tactical turn-based combat"
SUB = "A strange world behind a Windows 98 desktop."

def capsule(w, h, bg_name, logo_h, tag_size, out, tag=True, logo_pos="center", bottom_band=True, focus=None):
    im = cover_fit(scene(bg_name), w, h, focus)
    im = vignette(im, 0.6).convert("RGBA")
    if bottom_band:
        band = Image.new("RGBA", (w, h), (0, 0, 0, 0)); bd = ImageDraw.Draw(band)
        for i in range(h // 2, h): bd.line((0, i, w, i), fill=(0, 0, 0, int(200 * (i - h // 2) / (h / 2))))
        im.alpha_composite(band)
    lg = glow(logo_scaled(logo_h, w * 0.86), max(6, logo_h // 6))
    d = ImageDraw.Draw(im)
    portrait = h > w
    if logo_pos == "center":
        lx = (w - lg.width) // 2; ly = int(h * (0.16 if portrait else 0.30)) - lg.height // 2
    else:
        lx = int(w * 0.05); ly = int(h * 0.12)
    im.alpha_composite(lg, (lx, ly))
    if tag:
        maxw = int(w * 0.9)
        (t1, fnt), = fit_lines(d, TAGLINE, tag_size, maxw, True)
        if fnt.size < tag_size * 0.62:   # too narrow: two lines
            (t1, fnt), = fit_lines(d, "An open-world RPG with", tag_size, maxw, True); t1b = "X-COM style tactical combat"
        else: t1b = None
        (t2, fnt2), = fit_lines(d, SUB, int(tag_size * 0.8), maxw, False)
        ty = int(h * 0.80) - int(fnt.size * 1.3) * (2 if t1b else 1) if portrait else ly + lg.height + int(tag_size * 0.4)
        shadow_text(d, (w // 2, ty), t1, fnt, fill=(255, 245, 220), anchor="ma"); ty += int(fnt.size * 1.3)
        if t1b: shadow_text(d, (w // 2, ty), t1b, fnt, fill=(255, 245, 220), anchor="ma"); ty += int(fnt.size * 1.3)
        shadow_text(d, (w // 2, ty + int(fnt2.size * 0.2)), t2, fnt2, fill=(170, 230, 230), anchor="ma")
    im.convert("RGB").save(out, optimize=True)
    print("wrote", out, im.size)

ST = os.path.join(OUT, "steam"); IT = os.path.join(OUT, "itch")
# ---- Steam store graphics (sizes per Steamworks "Store graphical assets")
capsule(920, 430, "02_town.png", 96, 30, f"{ST}/header_capsule_920x430.png", focus=(320, 200))
capsule(462, 174, "02_town.png", 48, 0, f"{ST}/small_capsule_462x174.png", tag=False, focus=(320, 200))
capsule(1232, 706, "04_overworld_night.png", 150, 44, f"{ST}/main_capsule_1232x706.png")
capsule(748, 896, "07_combat_start.png", 130, 36, f"{ST}/vertical_capsule_748x896.png", focus=(190, 120))
capsule(600, 900, "07_combat_start.png", 110, 30, f"{ST}/library_capsule_600x900.png", focus=(190, 120))
capsule(3840, 1240, "05_overworld_day.png", 0, 0, f"{ST}/library_hero_3840x1240.png", tag=False, bottom_band=False) if False else None
# hero: no logo (Steam overlays the library logo itself)
hero = vignette(cover_fit(scene("05_overworld_day.png"), 3840, 1240), 0.45); hero.save(f"{ST}/library_hero_3840x1240.png", optimize=True); print("wrote hero")
# library logo: transparent PNG 1280x720 with the logo + tagline
lg = glow(logo_scaled(220), 14, (255, 200, 80, 70)); li = Image.new("RGBA", (1280, 720), (0, 0, 0, 0)); li.alpha_composite(lg, ((1280 - lg.width) // 2, 180))
d = ImageDraw.Draw(li); shadow_text(d, (640, 180 + lg.height + 10), TAGLINE, font(40, True), fill=(255, 245, 220), anchor="ma"); li.save(f"{ST}/library_logo_1280x720.png"); print("wrote library logo")
# page background
pb = vignette(cover_fit(scene("02_town.png"), 1438, 810), 0.75).filter(ImageFilter.GaussianBlur(1.2)); pb.save(f"{ST}/page_background_1438x810.jpg", quality=88); print("wrote page bg")
# community icon 184x184 (jpg) + client icon 32x32 (ico) from the desktop's "New Game" icon area -> use logo monogram instead: the rift glyph
def rift_icon(size):
    rift = [".....X.....", "....XPX....", "...XPPPX...", "..XPPWPPX..", ".XPPPWPPPX.", "XPPPPWPPPPX", ".XPPPWPPPX.", "..XPPWPPX..", "...XPPPX...", "....XPX....", ".....X....."]
    im = Image.new("RGBA", (11, 11), (0, 0, 0, 0)); p = im.load()
    for y, row in enumerate(rift):
        for x, c in enumerate(row):
            if c != ".": p[x, y] = (40, 10, 70, 255) if c == "X" else (140, 60, 220, 255) if c == "P" else (240, 230, 255, 255)
    s = size // 11; big = im.resize((11 * s, 11 * s), Image.NEAREST)
    out = Image.new("RGBA", (size, size), (0, 128, 128, 255)); out.alpha_composite(big, ((size - big.width) // 2, (size - big.height) // 2)); return out
rift_icon(184).convert("RGB").save(f"{ST}/community_icon_184x184.jpg", quality=95)
ic = rift_icon(32); ic.save(f"{ST}/client_icon_32x32.ico", sizes=[(32, 32)]); ic.save(f"{OUT}/icons/icon_32.png")
rift_icon(256).save(f"{OUT}/icons/icon_256.png"); rift_icon(512).save(f"{OUT}/icons/icon_512.png")
# ---- itch.io
capsule(630, 500, "02_town.png", 58, 0, f"{IT}/cover_630x500.png", tag=False, logo_pos="left", bottom_band=False, focus=(320, 185))
capsule(1200, 630, "04_overworld_night.png", 130, 40, f"{IT}/social_1200x630.png")
capsule(960, 540, "02_town.png", 100, 30, f"{IT}/banner_960x540.png", focus=(320, 185))
# ---- screenshots
SHOTS = {"02_town.png": "01_hollowmere", "08_combat_overwatch.png": "02_combat_overwatch", "07_combat_start.png": "03_combat_begins", "04_overworld_night.png": "04_storm_at_night",
         "06_dungeon.png": "05_greywood_cave", "09_worldmap.png": "06_world_map", "10_bestiary.png": "07_bestiary", "12_craft.png": "08_crafting", "13_bounty.png": "09_notice_board",
         "15_ashfall_fog.png": "10_ashfall_fog", "16_frosthold_snow.png": "11_frosthold_snow", "00_desktop.png": "12_the_desktop", "11_achievements.png": "13_achievements", "14_party.png": "14_party"}
for src, name in SHOTS.items():
    im = scene(src, crop=False)
    im.resize((1920, 1440), Image.NEAREST).save(f"{OUT}/screenshots_4x3_1920x1440/{name}.png", optimize=True)
    # 16:9: 2x game (1280x960) centred on a dark frame with a faint scanline pattern
    canvas = Image.new("RGB", (1920, 1080), (14, 12, 18)); cd = ImageDraw.Draw(canvas)
    for y in range(0, 1080, 4): cd.line((0, y, 1920, y), fill=(20, 18, 26))
    canvas.paste(im.resize((1280, 960), Image.NEAREST), (320, 60))
    cd.rectangle((318, 58, 1601, 1021), outline=(60, 50, 80), width=2)
    canvas.save(f"{OUT}/screenshots_16x9_1920x1080/{name}.png", optimize=True)
print("screenshots:", len(SHOTS))
