#!/usr/bin/env python3
"""Triffall 1.3.0 (#44): stage the assets for the browser demo.

    python tools/web_assets.py <src assets dir> <out dir> [--full-music]

The browser has to download everything before the game starts, so the staged copy leaves out:
  - new-art/ (art sources, never used) and campaigns git ignores (the owner's local test campaigns),
  - music the demo never plays: it keeps the tracks the game asks for by itself (title, overworld, town, combat...)
    and those named by the demo's maps (demo.txt maps=) and the Tutorial's maps,
  - and re-encodes those tracks as smaller Ogg Vorbis (about half the size; same length, loop points and volume -
    the music licence allows the game to ship them, it's a size change only). --full-music keeps the files as they are.
sounds.txt is rewritten so tracks that were left out point at nothing (no error, just silence if a mod asks for one).
"""
import os, re, shutil, subprocess, sys

SRC, OUT = sys.argv[1], sys.argv[2]
FULL = "--full-music" in sys.argv
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(SRC)))   # the project folder (git root)

def ignored(rel):
    try:
        return subprocess.run(["git", "check-ignore", "-q", rel], cwd=ROOT).returncode == 0
    except OSError:
        return False

if os.path.exists(OUT): shutil.rmtree(OUT)
skip_campaigns = set()
camp_dir = os.path.join(SRC, "campaigns")
for c in (os.listdir(camp_dir) if os.path.isdir(camp_dir) else []):
    if ignored("src/assets/campaigns/%s/campaign.txt" % c): skip_campaigns.add(c)

def read(p):
    with open(p, encoding="utf-8", errors="replace") as f: return f.read()

# ---- which music the demo can play
sounds = read(os.path.join(SRC, "data", "sounds.txt"))
music = {}   # id -> [files]
for m in re.finditer(r"\[music\]\n((?:[^\n\[]+\n)*)", sounds.replace("\r\n", "\n")):
    kv = dict(l.split("=", 1) for l in m.group(1).splitlines() if "=" in l)
    if "id" in kv: music[kv["id"]] = [f.strip() for f in kv.get("file", "").split(",") if f.strip()]
wanted = {"title", "overworld", "overworld_night", "town", "forest", "dungeon", "tech", "combat", "boss", "fanfare", "gameover", "ending"}
demo = read(os.path.join(SRC, "data", "demo.txt"))
maps = re.search(r"^maps=(.*)$", demo, re.M).group(1).split(",")
map_files = [os.path.join(SRC, "data", "maps", m.strip() + ".txt") for m in maps if m.strip()]
tut = os.path.join(SRC, "campaigns", "tutorial", "maps")
if os.path.isdir(tut): map_files += [os.path.join(tut, f) for f in os.listdir(tut) if f.endswith(".txt")]
for mf in map_files:
    if os.path.exists(mf):
        m = re.search(r"^music=(\S+)", read(mf), re.M)
        if m: wanted.add(m.group(1))
keep_files = {f for i in wanted for f in music.get(i, [])}

# ---- copy
def skip(rel):
    rel = rel.replace("\\", "/")
    if rel.startswith("new-art/"): return True
    parts = rel.split("/")
    if parts[0] == "campaigns" and len(parts) > 1 and parts[1] in skip_campaigns: return True
    if rel.startswith("audio/music/") and rel[len("audio/"):] not in keep_files: return True
    return False

total = 0
for d, dirs, files in os.walk(SRC):
    for fn in files:
        src = os.path.join(d, fn); rel = os.path.relpath(src, SRC)
        if skip(rel): continue
        dst = os.path.join(OUT, rel); os.makedirs(os.path.dirname(dst), exist_ok=True)
        if rel.replace("\\", "/").startswith("audio/music/") and not FULL and fn.endswith(".ogg"):
            import soundfile as sf
            x, sr = sf.read(src, dtype="float32")
            # compression_level 0 = best .. 1 = smallest; 0.5 is about 80-100 kbit/s for this music
            # (libsndfile crashes on one big Vorbis write: write it in chunks)
            with sf.SoundFile(dst, "w", sr, x.shape[1] if x.ndim > 1 else 1, format="OGG", subtype="VORBIS", compression_level=0.5) as o:
                for i in range(0, len(x), 8192): o.write(x[i:i + 8192])
        else:
            shutil.copyfile(src, dst)
        total += os.path.getsize(dst)

# ---- sounds.txt: tracks that weren't copied point at no file
def strip_missing(m):
    block = m.group(0)
    fm = re.search(r"^file=(.*)$", block, re.M)
    if not fm: return block
    files = [f.strip() for f in fm.group(1).split(",") if f.strip()]
    kept = [f for f in files if f in keep_files]
    if kept == files: return block
    return block.replace(fm.group(0), "file=" + ",".join(kept))
s = read(os.path.join(OUT, "data", "sounds.txt")).replace("\r\n", "\n")
s = re.sub(r"\[music\]\n(?:[^\n\[]+\n)*", strip_missing, s)
with open(os.path.join(OUT, "data", "sounds.txt"), "w", encoding="utf-8", newline="\n") as f: f.write(s)
print("staged %s: %.1f MB, %d music files (%s), left out campaigns: %s" % (OUT, total / 1e6, len(keep_files), ", ".join(sorted(wanted)), ", ".join(sorted(skip_campaigns)) or "none"))
