#!/usr/bin/env python3
"""Builds the itch.io / Steam promo kit and the store screenshots from headless screenshots.

    python tools/promo_kit.py <raw_screenshot_dir> <out_dir> <screenshot_dir>

1.3.0: expects the shots of tools/presskit_shots.txt: 960x540 game shots (the canvas a 1920x1080 screen gives the
game) and 1280x720 Editor shots (16_*, 17_*), plus 10_desktop.png for the logo (1.6.2 numbering). tools/make-presskit.ps1 runs it all.
Everything is the game's own pixel art: screenshots are upscaled with nearest-neighbour only, the logo is the
desktop's TRIFFALL logo, text is a system font. No generated or painted art (Steam asks about AI-made content).
"""
import os, sys, glob, math
from PIL import Image, ImageDraw, ImageFont, ImageFilter

RAW, OUT, SHOTDIR = sys.argv[1], sys.argv[2], sys.argv[3]
for sub in ("steam", "itch", "icons"):
    os.makedirs(os.path.join(OUT, sub), exist_ok=True)
os.makedirs(SHOTDIR, exist_ok=True)

FONT_CANDIDATES = (
    (r"C:\Windows\Fonts\arialbd.ttf", r"C:\Windows\Fonts\arial.ttf"),
    ("/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf", "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"),
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
    im = Image.open(os.path.join(RAW, "10_desktop.png")).convert("RGB"); px = im.load()
    W, H = im.size
    xs, ys = [], []
    for y in range(H // 2, H - 30):          # the logo sits bottom right, above the taskbar
        for x in range(W // 2, W):
            r, g, b = px[x, y]
            if r > 200 and g > 180 and b < 160: xs.append(x); ys.append(y)
    box = (min(xs) - 2, min(ys) - 2, max(xs) + 3, max(ys) + 3)
    crop = im.crop(box).convert("RGBA"); p = crop.load()
    bg = px[box[0], box[1]]
    for y in range(crop.height):
        for x in range(crop.width):
            r, g, b, a = p[x, y]
            if abs(r - bg[0]) < 12 and abs(g - bg[1]) < 12 and abs(b - bg[2]) < 12: p[x, y] = (0, 0, 0, 0)
    return crop
LOGO = extract_logo()

def logo_scaled(h, maxw=None):
    s = max(1, round(h / LOGO.height))
    if maxw: s = max(1, min(s, int(maxw // LOGO.width)))
    return LOGO.resize((LOGO.width * s, LOGO.height * s), Image.NEAREST)

def fit(draw, text, size, maxw, bold):
    fnt = font(size, bold)
    while draw.textlength(text, font=fnt) > maxw and size > 10:
        size -= 2; fnt = font(size, bold)
    return fnt

def glow(img, radius, color=(255, 200, 80, 110)):
    """Soft glow behind the transparent pixel logo."""
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
    if crop and im.size == (960, 540):   # keep the map, drop the party bar (and the combat action bar)
        im = im.crop((0, 30, 960, 466) if "combat" in name else (0, 0, 960, 466))
    return im

def cover_fit(im, w, h, focus=None, at=(0.5, 0.5), minscale=1):
    """Fill w x h with a pixel-art screenshot: integer nearest upscale, then crop so the focus point (source coords)
    lands at the fraction `at` of the picture (as far as the edges allow)."""
    s = max(math.ceil(w / im.width), math.ceil(h / im.height), minscale)
    big = im.resize((im.width * s, im.height * s), Image.NEAREST)
    fx, fy = (im.width / 2, im.height / 2) if focus is None else focus
    x = int(min(max(0, fx * s - w * at[0]), big.width - w)); y = int(min(max(0, fy * s - h * at[1]), big.height - h))
    return big.crop((x, y, x + w, y + h))

def vignette(im, strength=0.55):
    w, h = im.size
    grad = Image.radial_gradient("L").resize((w, h))
    m = Image.eval(grad, lambda v: int(v * strength))
    return Image.composite(Image.new("RGB", (w, h), (0, 0, 0)), im, m)

TAGLINE = "An old-school RPG with turn-based tactical combat"
SUB = "hiding inside a Windows 98 desktop"
BATTLE = (665, 185)      # where the fight is in art_combat.png (after the crop)
TOWN = (480, 230)

def capsule(w, h, bg_name, logo_h, tag_size, out, tag=True, logo_pos="center", bottom_band=True, focus=None, quality=None, minscale=1):
    """The logo (and tagline) sit in the top part on a dark gradient; the focus point (the fight) lands lower down."""
    portrait = h > w
    at = (0.5, 0.62) if portrait else ((0.62, 0.62) if logo_pos == "left" else (0.5, 0.68 if logo_h else 0.5))
    im = cover_fit(scene(bg_name), w, h, focus, at, minscale)
    im = vignette(im, 0.5).convert("RGBA")
    band = Image.new("RGBA", (w, h), (0, 0, 0, 0)); bd = ImageDraw.Draw(band)
    if logo_h:   # dark top for the logo and tagline
        top = int(h * (0.48 if portrait else 0.62 if tag else 0.42))
        for i in range(top): bd.line((0, i, w, i), fill=(0, 0, 0, int(215 * (1 - (i / top) ** 2))))
    if bottom_band:
        for i in range(int(h * 0.8), h): bd.line((0, i, w, i), fill=(0, 0, 0, int(160 * (i - h * 0.8) / (h * 0.2))))
    im.alpha_composite(band)
    d = ImageDraw.Draw(im)
    if logo_h:
        lg = glow(logo_scaled(logo_h, w * 0.86), max(6, logo_h // 6))
        if logo_pos == "center":
            lx = (w - lg.width) // 2; ly = int(h * (0.06 if portrait else 0.05 if tag else 0.08))
        else:
            lx = int(w * 0.03); ly = int(h * 0.04)
        im.alpha_composite(lg, (lx, ly))
    if tag:
        maxw = int(w * 0.9)
        fnt = fit(d, TAGLINE, tag_size, maxw, True); fnt2 = fit(d, SUB, int(tag_size * 0.8), maxw, False)
        ty = ly + lg.height - max(6, logo_h // 6) * 2
        shadow_text(d, (w // 2, ty), TAGLINE, fnt, fill=(255, 245, 220), anchor="ma")
        shadow_text(d, (w // 2, ty + int(fnt.size * 1.35)), SUB, fnt2, fill=(170, 230, 230), anchor="ma")
    im = im.convert("RGB")
    if quality: im.save(out, quality=quality)
    else: im.save(out, optimize=True)
    print("wrote", out, im.size)

ST = os.path.join(OUT, "steam"); IT = os.path.join(OUT, "itch")
# ---- key art (replaces the AI-made triffall-key-art.jpg / -cover / -banner of 0.9)
capsule(1920, 1080, "art_combat.png", 200, 54, os.path.join(OUT, "key_art_1920x1080.png"), focus=BATTLE)
# ---- Steam store graphics (sizes per Steamworks "Store graphical assets")
# 1.6.2 (#136): Valve allows only the art, the game's name and an official subtitle on capsules - no marketing line,
# so every Steam capsule (and the library logo) is the logo alone. The tagline stays on the key art and itch's social card.
capsule(920, 430, "art_combat.png", 110, 0, f"{ST}/header_capsule_920x430.png", tag=False, focus=BATTLE)
capsule(462, 174, "art_combat.png", 48, 0, f"{ST}/small_capsule_462x174.png", tag=False, focus=BATTLE)
capsule(1232, 706, "art_combat.png", 160, 0, f"{ST}/main_capsule_1232x706.png", tag=False, focus=BATTLE)
capsule(748, 896, "art_combat.png", 130, 0, f"{ST}/vertical_capsule_748x896.png", tag=False, focus=BATTLE)
capsule(600, 900, "art_combat.png", 110, 0, f"{ST}/library_capsule_600x900.png", tag=False, focus=BATTLE)
# hero: no logo (Steam overlays the library logo itself)
hero = vignette(cover_fit(scene("04_overworld.png"), 3840, 1240), 0.45); hero.save(f"{ST}/library_hero_3840x1240.png", optimize=True); print("wrote hero")
# library logo: transparent PNG 1280x720 with the logo only (1.6.2: no tagline, see above)
lg = glow(logo_scaled(220), 14, (255, 200, 80, 70)); li = Image.new("RGBA", (1280, 720), (0, 0, 0, 0)); li.alpha_composite(lg, ((1280 - lg.width) // 2, (720 - lg.height) // 2))
li.save(f"{ST}/library_logo_1280x720.png"); print("wrote library logo")
pb = vignette(cover_fit(scene("05_town.png"), 1438, 810, TOWN), 0.75).filter(ImageFilter.GaussianBlur(1.2)); pb.save(f"{ST}/page_background_1438x810.jpg", quality=88); print("wrote page bg")
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
capsule(630, 500, "art_combat.png", 56, 0, f"{IT}/cover_630x500.png", tag=False, logo_pos="left", bottom_band=False, focus=BATTLE)
capsule(960, 300, "05_town.png", 64, 0, f"{IT}/banner_960x300.png", tag=False, focus=TOWN)
capsule(1200, 630, "04_overworld.png", 120, 36, f"{IT}/social_1200x630.png")
# ---- screenshots, 1920x1080: game shots 2x; Editor shots (1280x720) 3x then halved, so its text stays even
n = 0
for src in sorted(glob.glob(os.path.join(RAW, "*.png"))):
    name = os.path.basename(src); im = Image.open(src).convert("RGB")
    if name.startswith("art_"): continue   # 1.6.2: capsule backgrounds, not store screenshots
    if im.size == (960, 540): out = im.resize((1920, 1080), Image.NEAREST)
    elif im.size == (1280, 720): out = im.resize((3840, 2160), Image.NEAREST).resize((1920, 1080), Image.BOX)
    else: continue
    out.save(os.path.join(SHOTDIR, name), optimize=True); n += 1
print("screenshots:", n)
