#!/usr/bin/env python3
"""Renders 64x64 achievement icon pairs (achieved / unachieved) for Steamworks from achievements.txt tiles.
    python3 tools/achievement_icons.py assets out_dir
"""
import sys, os
from PIL import Image
assets = sys.argv[1] if len(sys.argv) > 1 else "assets"; out = sys.argv[2] if len(sys.argv) > 2 else "achievement_icons"
os.makedirs(out, exist_ok=True)
sheet = Image.open(os.path.join(assets, "tiles.png")).convert("RGBA")
text = open(os.path.join(assets, "data", "achievements.txt")).read()
cur = {}
entries = []
for raw in text.splitlines():
    l = raw.strip()
    if l.startswith("[achievement]"): cur = {}; entries.append(cur)
    elif "=" in l and not l.startswith("#") and cur is not None: k, v = l.split("=", 1); cur[k.strip()] = v.strip()
for e in entries:
    if "id" not in e: continue
    c, r = [int(x) for x in e.get("tile", "0,19").split(",")]
    tile = sheet.crop((1 + c * 13, 1 + r * 13, 1 + c * 13 + 12, 1 + r * 13 + 12))
    # black = transparent in the 1-bit sheet
    px = tile.load()
    for y in range(12):
        for x in range(12):
            if px[x, y][:3] == (0, 0, 0): px[x, y] = (0, 0, 0, 0)
    for state, bg, fg in (("achieved", (40, 30, 10), (255, 220, 120)), ("unachieved", (30, 30, 34), (90, 90, 100))):
        im = Image.new("RGBA", (64, 64), bg + (255,))
        col = tile.copy(); cp = col.load()
        for y in range(12):
            for x in range(12):
                if cp[x, y][3]: cp[x, y] = fg + (255,)
        im.paste(col.resize((48, 48), Image.NEAREST), (8, 8), col.resize((48, 48), Image.NEAREST))
        im.save(os.path.join(out, f"{e['id']}_{state}.png"))
print("wrote", len(entries) * 2, "icons to", out)
