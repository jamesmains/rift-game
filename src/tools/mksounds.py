#!/usr/bin/env python3
"""Generates Triffall's default sound effects (WAV) and music (OGG via ffmpeg) procedurally.

    python3 tools/mksounds.py assets/audio

Everything here is synthesized (chip-tune style oscillators + noise), so the result is
free to ship and free to replace: drop your own .wav/.ogg files in assets/audio and point
assets/data/sounds.txt at them (or use the Sounds tab in the Editor).
"""
import math, os, struct, subprocess, sys, random, wave
import numpy as np

SR = 22050
OUT = sys.argv[1] if len(sys.argv) > 1 else "assets/audio"
WHAT = set(sys.argv[2:]) or {"sfx"}   # e.g. "mksounds.py assets/audio music ambient"; 1.3.1 (#50): ambient loops left the game, only made when asked
# 1.3.0: the game's music is the Pixel Fantasy pack (audio/music/pixel_fantasy), so the generated tracks are only made
# when asked for ("music"); they are no longer used by sounds.txt.

# ------------------------------------------------------------------ helpers
def t(n): return np.arange(n) / SR
def env(n, a=0.005, d=0.1, s=0.0, r=0.05):
    """ADSR-ish envelope over n samples (times in seconds)."""
    e = np.ones(n); A = int(a * SR); D = int(d * SR); R = int(r * SR)
    if A > 0: e[:A] = np.linspace(0, 1, A)
    if D > 0: e[A:A + D] = np.linspace(1, s, min(D, max(0, n - A)))
    e[A + D:] = s
    if R > 0 and n > R: e[-R:] *= np.linspace(1, 0, R)
    return e
def square(f, n, duty=0.5):
    ph = (np.cumsum(np.full(n, f) if np.isscalar(f) else f) / SR) % 1.0
    return np.where(ph < duty, 1.0, -1.0)
def saw(f, n):
    ph = (np.cumsum(np.full(n, f) if np.isscalar(f) else f) / SR) % 1.0
    return ph * 2 - 1
def tri(f, n):
    ph = (np.cumsum(np.full(n, f) if np.isscalar(f) else f) / SR) % 1.0
    return np.abs(ph * 4 - 2) - 1
def sine(f, n):
    ph = np.cumsum(np.full(n, f) if np.isscalar(f) else f) / SR
    return np.sin(2 * math.pi * ph)
def noise(n, seed=1):
    return np.random.RandomState(seed).uniform(-1, 1, n)
def lowpass(x, cutoff):
    rc = 1.0 / (2 * math.pi * cutoff); dt = 1.0 / SR; a = dt / (rc + dt)
    try:
        from scipy.signal import lfilter
        return lfilter([a], [1, -(1 - a)], x)
    except ImportError:
        pass
    y = np.empty_like(x); acc = 0.0
    for i in range(len(x)): acc += a * (x[i] - acc); y[i] = acc
    return y
def sweep(f0, f1, n): return np.geomspace(max(f0, 1), max(f1, 1), n)
def norm(x, peak=0.8):
    m = np.max(np.abs(x)) if len(x) else 1
    return x / m * peak if m > 0 else x
def save_wav(name, x):
    x = np.clip(x, -1, 1)
    path = os.path.join(OUT, "sfx", name + ".wav"); os.makedirs(os.path.dirname(path), exist_ok=True)
    with wave.open(path, "wb") as w:
        w.setnchannels(1); w.setsampwidth(2); w.setframerate(SR)
        w.writeframes((x * 32767).astype("<i2").tobytes())
def to_ogg(wavp, x, q):
    """1.1.6: encode x to the .ogg next to wavp. ffmpeg if it is on PATH, else the soundfile package (pip install
    soundfile); returns True when the .ogg was written (the caller then deletes the .wav)."""
    oggp = wavp[:-4] + ".ogg"
    try:
        r = subprocess.run(["ffmpeg", "-y", "-loglevel", "error", "-i", wavp, "-c:a", "libvorbis", "-q:a", str(q), oggp])
        if r.returncode == 0: return True
    except FileNotFoundError:
        pass
    try:
        import soundfile
        with soundfile.SoundFile(oggp, "w", SR, 1, format="OGG", subtype="VORBIS", compression_level=max(0.0, min(1.0, 1.0 - q / 10.0))) as f:
            y = np.clip(x, -1, 1)
            for i in range(0, len(y), 8192): f.write(y[i:i + 8192])   # libsndfile crashes on one big vorbis write
        return True
    except Exception as e:
        print("no ffmpeg or soundfile:", e); return False
def mix(*parts):
    n = max(len(p) for p in parts); out = np.zeros(n)
    for p in parts: out[:len(p)] += p
    return out
def seq(*parts):
    return np.concatenate(parts)
def blip(f, dur, wave_fn=square, **kw):
    n = int(dur * SR); return wave_fn(f, n) * env(n, **kw)

# ------------------------------------------------------------------ sound effects
def sfx():
    S = {}
    # UI
    S["ui_move"] = blip(880, 0.04, a=0.001, d=0.03, s=0, r=0.01) * 0.5
    S["ui_confirm"] = seq(blip(660, 0.05, d=0.05), blip(990, 0.09, d=0.08)) * 0.6
    S["ui_cancel"] = seq(blip(660, 0.05, d=0.05), blip(440, 0.09, d=0.08)) * 0.5
    S["ui_open"] = blip(sweep(300, 900, int(0.12 * SR)), 0.12, tri, d=0.1) * 0.7
    S["ui_close"] = blip(sweep(900, 300, int(0.12 * SR)), 0.12, tri, d=0.1) * 0.7
    S["ui_error"] = mix(blip(200, 0.18, square, d=0.15), blip(190, 0.18, square, d=0.15)) * 0.5
    S["ui_page"] = blip(1200, 0.03, a=0.001, d=0.02, r=0.005) * 0.4
    S["text_blip"] = blip(1500, 0.025, square, a=0.001, d=0.02, r=0.004) * 0.35
    # desktop
    n = int(1.6 * SR)
    S["boot"] = norm(mix(sine(261.6, n) * env(n, 0.01, 1.2, 0.2, 0.4), sine(329.6, n) * env(n, 0.25, 1.0, 0.2, 0.4), sine(392.0, n) * env(n, 0.5, 0.9, 0.2, 0.4), sine(523.3, n) * env(n, 0.75, 0.6, 0.3, 0.4)), 0.7)
    n = int(1.4 * SR)
    S["shutdown"] = norm(mix(sine(523.3, n) * env(n, 0.01, 0.8, 0.1, 0.4), sine(392.0, n) * env(n, 0.3, 0.7, 0.1, 0.4), sine(261.6, n) * env(n, 0.6, 0.7, 0.1, 0.4)), 0.7)
    S["desktop_click"] = blip(2400, 0.012, square, a=0.0005, d=0.01, r=0.002) * 0.5
    S["desktop_open"] = seq(blip(500, 0.04, tri, d=0.03), blip(750, 0.06, tri, d=0.05)) * 0.6
    S["desktop_close"] = seq(blip(750, 0.04, tri, d=0.03), blip(500, 0.06, tri, d=0.05)) * 0.6
    # exploration
    for i in range(1, 5):
        n = int(0.06 * SR); S["step%d" % i] = lowpass(noise(n, i * 11), 900 + i * 150) * env(n, 0.002, 0.05, 0, 0.01) * 0.9
    n = int(0.35 * SR)
    S["door_open"] = norm(mix(lowpass(noise(n, 5), 400) * env(n, 0.01, 0.3, 0, 0.05), saw(sweep(90, 60, n), n) * env(n, 0.02, 0.3, 0, 0.05) * 0.4), 0.6)
    S["door_locked"] = seq(blip(150, 0.06, square, d=0.05), np.zeros(int(0.03 * SR)), blip(150, 0.06, square, d=0.05)) * 0.6
    S["chest_open"] = seq(blip(523, 0.07, tri, d=0.06), blip(659, 0.07, tri, d=0.06), blip(784, 0.07, tri, d=0.06), blip(1047, 0.16, tri, d=0.14)) * 0.6
    S["pickup"] = seq(blip(880, 0.05, square, d=0.04), blip(1320, 0.1, square, d=0.09)) * 0.45
    S["gold"] = mix(seq(blip(2093, 0.05, sine, d=0.04), blip(2637, 0.12, sine, d=0.11)), seq(np.zeros(int(0.03 * SR)), blip(3136, 0.1, sine, d=0.09)) * 0.5) * 0.6
    S["equip"] = seq(lowpass(noise(int(0.05 * SR), 9), 2500) * env(int(0.05 * SR), 0.002, 0.04), blip(700, 0.08, tri, d=0.07)) * 0.6
    S["potion"] = blip(sweep(400, 1200, int(0.25 * SR)), 0.25, sine, a=0.01, d=0.2, s=0, r=0.05) * 0.6
    S["save"] = seq(blip(784, 0.08, tri, d=0.07), blip(988, 0.08, tri, d=0.07), blip(1175, 0.2, tri, d=0.18)) * 0.6
    S["levelup"] = seq(blip(523, 0.1, square, d=0.09), blip(659, 0.1, square, d=0.09), blip(784, 0.1, square, d=0.09), blip(1047, 0.3, square, d=0.28)) * 0.5
    S["quest"] = seq(blip(659, 0.09, tri, d=0.08), blip(880, 0.09, tri, d=0.08), blip(659, 0.09, tri, d=0.08), blip(1319, 0.3, tri, d=0.28)) * 0.55
    n = int(0.9 * SR)
    S["teleport"] = norm(sine(sweep(200, 2000, n), n) * env(n, 0.05, 0.7, 0, 0.2) + sine(sweep(220, 2200, n), n) * env(n, 0.05, 0.7, 0, 0.2) * 0.5, 0.6)
    n = int(1.2 * SR)
    S["rest"] = norm(mix(sine(392, n) * env(n, 0.1, 1.0, 0, 0.2), sine(494, n) * env(n, 0.3, 0.8, 0, 0.2), sine(587, n) * env(n, 0.5, 0.6, 0, 0.2)), 0.5)
    S["shop_buy"] = seq(blip(1568, 0.05, sine, d=0.04), blip(2093, 0.08, sine, d=0.07), blip(2637, 0.12, sine, d=0.1)) * 0.55
    S["shop_sell"] = seq(blip(2637, 0.05, sine, d=0.04), blip(2093, 0.08, sine, d=0.07), blip(1568, 0.12, sine, d=0.1)) * 0.55
    S["ambush"] = norm(mix(square(sweep(220, 110, int(0.4 * SR)), int(0.4 * SR)) * env(int(0.4 * SR), 0.005, 0.35), lowpass(noise(int(0.4 * SR), 3), 800) * env(int(0.4 * SR), 0.005, 0.3)), 0.7)
    # combat
    n = int(0.6 * SR)
    S["combat_start"] = norm(mix(square(sweep(110, 55, n), n) * env(n, 0.005, 0.5, 0, 0.1), lowpass(noise(n, 4), 600) * env(n, 0.005, 0.4)), 0.7)
    S["turn_player"] = seq(blip(880, 0.05, tri, d=0.04), blip(1175, 0.08, tri, d=0.07)) * 0.5
    S["turn_enemy"] = seq(blip(440, 0.05, tri, d=0.04), blip(330, 0.08, tri, d=0.07)) * 0.5
    n = int(0.14 * SR)
    S["attack_melee"] = norm(mix(lowpass(noise(n, 7), sweep(3000, 500, n).mean()) * env(n, 0.002, 0.12), saw(sweep(500, 80, n), n) * env(n, 0.002, 0.1) * 0.5), 0.7)
    n = int(0.25 * SR)
    S["attack_bow"] = norm(mix(lowpass(noise(n, 8), 2000) * env(n, 0.002, 0.2), sine(sweep(1500, 300, n), n) * env(n, 0.002, 0.15) * 0.4), 0.6)
    n = int(0.22 * SR)
    S["attack_gun"] = norm(mix(noise(n, 12) * env(n, 0.001, 0.18), square(sweep(300, 40, n), n) * env(n, 0.001, 0.08)), 0.8)
    n = int(0.3 * SR)
    S["attack_laser"] = norm(mix(square(sweep(2200, 300, n), n, 0.3) * env(n, 0.002, 0.25), sine(sweep(2300, 320, n), n) * env(n, 0.002, 0.25) * 0.5), 0.6)
    n = int(0.12 * SR)
    S["hit"] = norm(mix(lowpass(noise(n, 13), 1200) * env(n, 0.001, 0.1), sine(sweep(200, 60, n), n) * env(n, 0.001, 0.1)), 0.7)
    n = int(0.2 * SR)
    S["crit"] = norm(mix(noise(n, 14) * env(n, 0.001, 0.15), square(sweep(400, 50, n), n) * env(n, 0.001, 0.15)), 0.8)
    S["miss"] = blip(sweep(900, 400, int(0.1 * SR)), 0.1, sine, d=0.09) * 0.4
    S["block"] = seq(blip(1800, 0.03, square, a=0.001, d=0.025), blip(900, 0.06, tri, d=0.05)) * 0.5
    n = int(0.4 * SR)
    S["spell_cast"] = norm(mix(sine(sweep(400, 1600, n), n) * env(n, 0.02, 0.3, 0, 0.1), tri(sweep(600, 2400, n), n) * env(n, 0.02, 0.3, 0, 0.1) * 0.4), 0.6)
    n = int(0.5 * SR)
    S["spell_fire"] = norm(mix(lowpass(noise(n, 21), 1500) * env(n, 0.01, 0.4, 0, 0.1), saw(sweep(300, 80, n), n) * env(n, 0.01, 0.3) * 0.4), 0.7)
    n = int(0.5 * SR)
    S["spell_ice"] = norm(mix(sine(sweep(2500, 1200, n), n) * env(n, 0.005, 0.4, 0, 0.1), lowpass(noise(n, 22), 4000) * env(n, 0.005, 0.1) * 0.5, sine(3200, n) * env(n, 0.1, 0.3) * 0.3), 0.6)
    n = int(0.4 * SR)
    S["spell_lightning"] = norm(mix(noise(n, 23) * env(n, 0.001, 0.3), square(np.where(np.arange(n) % 800 < 400, 120, 90), n) * env(n, 0.001, 0.25) * 0.5), 0.8)
    n = int(0.45 * SR)
    S["spell_poison"] = norm(mix(lowpass(noise(n, 24), 700) * env(n, 0.01, 0.35, 0, 0.1), sine(sweep(500, 150, n), n) * env(n, 0.01, 0.35) * 0.5), 0.6)
    n = int(0.6 * SR)
    S["spell_heal"] = norm(seq(blip(659, 0.12, sine, d=0.1), blip(784, 0.12, sine, d=0.1), blip(988, 0.12, sine, d=0.1), blip(1319, 0.3, sine, d=0.28)) + sine(sweep(600, 1500, int(0.66 * SR)), int(0.66 * SR)) * env(int(0.66 * SR), 0.1, 0.5, 0, 0.1) * 0.3, 0.6)
    n = int(0.35 * SR)
    S["spell_buff"] = norm(sine(sweep(500, 1000, n), n) * env(n, 0.01, 0.3, 0, 0.05) + sine(sweep(750, 1500, n), n) * env(n, 0.05, 0.25, 0, 0.05) * 0.5, 0.55)
    n = int(0.9 * SR)
    S["explosion"] = norm(lowpass(noise(n, 31), 900) * env(n, 0.002, 0.8, 0, 0.1) + sine(sweep(120, 30, n), n) * env(n, 0.002, 0.5) * 0.7, 0.9)
    n = int(0.5 * SR)
    S["death_enemy"] = norm(mix(square(sweep(300, 40, n), n) * env(n, 0.005, 0.45), lowpass(noise(n, 32), 800) * env(n, 0.005, 0.3) * 0.6), 0.7)
    n = int(0.9 * SR)
    S["death_party"] = norm(seq(blip(440, 0.2, square, d=0.18), blip(349, 0.2, square, d=0.18), blip(262, 0.5, square, d=0.48)), 0.6)
    S["flee"] = seq(*[lowpass(noise(int(0.05 * SR), 40 + i), 1200) * env(int(0.05 * SR), 0.002, 0.04) for i in range(6)]) * 0.8
    S["victory"] = seq(blip(523, 0.12, square, d=0.1), blip(523, 0.12, square, d=0.1), blip(523, 0.12, square, d=0.1), blip(659, 0.3, square, d=0.28), blip(587, 0.12, square, d=0.1), blip(659, 0.5, square, d=0.48)) * 0.5
    S["defeat"] = seq(blip(392, 0.3, square, d=0.28), blip(370, 0.3, square, d=0.28), blip(349, 0.3, square, d=0.28), blip(330, 0.8, square, d=0.78)) * 0.5
    S["grenade_throw"] = blip(sweep(300, 600, int(0.3 * SR)), 0.3, sine, a=0.01, d=0.25) * 0.5
    S["reload"] = seq(blip(900, 0.03, square, a=0.001, d=0.025), np.zeros(int(0.08 * SR)), blip(1200, 0.03, square, a=0.001, d=0.025)) * 0.5
    S["unlock"] = seq(blip(1400, 0.03, square, a=0.001, d=0.025), np.zeros(int(0.05 * SR)), blip(1000, 0.03, square, a=0.001, d=0.025), blip(700, 0.15, tri, d=0.14)) * 0.55
    S["light"] = blip(sweep(300, 2500, int(0.5 * SR)), 0.5, sine, a=0.05, d=0.4, s=0, r=0.05) * 0.5
    S["stealth"] = blip(sweep(1500, 200, int(0.5 * SR)), 0.5, tri, a=0.05, d=0.4, s=0, r=0.05) * 0.4
    S["detect"] = seq(blip(1047, 0.08, sine, d=0.07), np.zeros(int(0.1 * SR)), blip(1047, 0.08, sine, d=0.07), np.zeros(int(0.1 * SR)), blip(1568, 0.25, sine, d=0.23)) * 0.5
    n = int(2.2 * SR)
    S["thunder"] = norm(lowpass(noise(n, 77), 300) * env(n, 0.02, 1.8, 0, 0.3) + lowpass(noise(n, 78), 900) * env(n, 0.005, 0.25) * 0.6 + sine(sweep(60, 25, n), n) * env(n, 0.01, 1.2) * 0.5, 0.8)
    S["craft"] = seq(blip(1200, 0.04, square, a=0.001, d=0.03), np.zeros(int(0.06 * SR)), blip(900, 0.04, square, a=0.001, d=0.03), np.zeros(int(0.06 * SR)), blip(1500, 0.15, tri, d=0.14)) * 0.55
    S["achievement"] = seq(blip(784, 0.08, tri, d=0.07), blip(1047, 0.08, tri, d=0.07), blip(1319, 0.08, tri, d=0.07), blip(1568, 0.28, tri, d=0.26)) * 0.55
    S["mine_click"] = blip(1800, 0.02, square, a=0.0005, d=0.015, r=0.003) * 0.4
    S["mine_boom"] = norm(lowpass(noise(int(0.5 * SR), 81), 700) * env(int(0.5 * SR), 0.002, 0.45) + sine(sweep(100, 30, int(0.5 * SR)), int(0.5 * SR)) * env(int(0.5 * SR), 0.002, 0.3) * 0.6, 0.8)
    S["card"] = lowpass(noise(int(0.05 * SR), 83), 3000) * env(int(0.05 * SR), 0.002, 0.04) * 0.6
    S["banter"] = blip(1100, 0.05, tri, d=0.04) * 0.35
    S["travel"] = seq(*[lowpass(noise(int(0.12 * SR), 90 + i), 700) * env(int(0.12 * SR), 0.01, 0.1) * (0.5 + 0.1 * i) for i in range(5)]) * 0.7
    for k, v in S.items(): save_wav(k, v)
    return sorted(S.keys())

# ------------------------------------------------------------------ music
NOTE = {n: i for i, n in enumerate(["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"])}
def freq(midi): return 440.0 * 2 ** ((midi - 69) / 12)
SCALES = {"minor": [0, 2, 3, 5, 7, 8, 10], "major": [0, 2, 4, 5, 7, 9, 11], "dorian": [0, 2, 3, 5, 7, 9, 10], "phrygian": [0, 1, 3, 5, 7, 8, 10], "penta": [0, 3, 5, 7, 10], "lydian": [0, 2, 4, 6, 7, 9, 11]}

def render_track(name, bpm, root, scale, bars, chords, style, seed, loop=True):
    """A tiny tracker: bass + chords/pad + lead + drums, mixed down to mono at SR."""
    rnd = random.Random(seed)
    sc = SCALES[scale]
    beat = 60.0 / bpm; bar = beat * 4
    total = int(bars * bar * SR) + (0 if loop else int(2 * SR))
    out = np.zeros(total)
    def deg(d, octave=0):  # scale degree (may exceed len) -> midi
        o, i = divmod(d, len(sc)); return root + sc[i] + 12 * (o + octave)
    def place(x, at):
        s = int(at * SR); e = min(total, s + len(x))
        if s < total: out[s:e] += x[:e - s]
    def tone(midi, dur, wave_fn, vol, duty=0.5, a=0.01, d=0.1, s=0.6, r=0.05, vib=0.0):
        n = int(dur * SR); f = freq(midi)
        if vib: f = f * (1 + vib * np.sin(2 * math.pi * 5.5 * t(n)))
        w = wave_fn(f, n, duty) if wave_fn is square else wave_fn(f, n)
        return w * env(n, a, d, s, r) * vol
    def kick(at, vol=0.9):
        n = int(0.18 * SR); place(sine(sweep(140, 40, n), n) * env(n, 0.001, 0.15) * vol, at)
    def snare(at, vol=0.5):
        n = int(0.12 * SR); place((lowpass(noise(n, int(at * 1000) % 977), 3000) * 0.8 + sine(190, n) * 0.3) * env(n, 0.001, 0.1) * vol, at)
    def hat(at, vol=0.18):
        n = int(0.03 * SR); place(noise(n, int(at * 1000) % 991 + 3) * env(n, 0.001, 0.025) * vol, at)
    # 1.0 styles: "bell" (glassy plucks with an echo - Frosthold, the ending), "clock" (ticking staccato arpeggios -
    # Brasswick), "desert" (droning bass, snake-charmer lead - Sunscar), "murk" (slow wobbling swamp - Murkwater, the
    # marsh), "eerie" (unresolved drones and whispers - the Scar), "ruin" (sparse, lonely pulses - Ashfall)
    if style in ("bell", "clock", "desert", "murk", "eerie", "ruin"):
        return render_style2(name, bpm, root, scale, bars, chords, style, seed, loop)
    for b in range(bars):
        chord = chords[b % len(chords)]            # scale degree of the chord root
        t0 = b * bar
        # pad / chords
        if style in ("warm", "calm", "dark", "sci"):
            for k, dg in enumerate([chord, chord + 2, chord + 4]):
                place(tone(deg(dg, 0), bar, tri if style != "dark" else saw, 0.08 if style != "calm" else 0.06, a=0.3, d=0.5, s=0.8, r=0.4), t0 + k * 0.01)
        # bass
        if style == "calm":
            place(tone(deg(chord, -2), bar * 0.95, sine, 0.22, a=0.02, d=0.3, s=0.7, r=0.2), t0)
        else:
            pat = [0, 0.5, 1, 1.5, 2, 2.5, 3, 3.5] if style in ("chip", "sci") else [0, 1.5, 2, 3]
            for i, bt in enumerate(pat):
                m = deg(chord, -2) + (12 if style == "sci" and i % 2 else 0)
                place(tone(m, beat * 0.45, square if style != "warm" else tri, 0.2, duty=0.5, a=0.005, d=0.1, s=0.5, r=0.03), t0 + bt * beat)
        # arpeggio / lead
        if style == "sci":
            for i in range(16):
                m = deg(chord + [0, 2, 4, 6][i % 4], 1 + (i // 8))
                place(tone(m, beat * 0.22, square, 0.09, duty=0.125, a=0.002, d=0.05, s=0.3, r=0.02), t0 + i * beat / 4)
        elif style == "chip":
            if b % 2 == 0 or rnd.random() < 0.4:
                steps = [0, 0.5, 1, 1.5, 2, 3, 3.5]; cur = chord + 7
                for st in steps:
                    cur += rnd.choice([-2, -1, 0, 1, 1, 2])
                    place(tone(deg(cur, 0), beat * 0.45, square, 0.13, duty=0.25, a=0.003, d=0.08, s=0.5, r=0.03, vib=0.004), t0 + st * beat)
        elif style == "warm":
            if b % 2 == 1:
                cur = chord + 7
                for st in [0, 1, 2, 2.5, 3]:
                    cur += rnd.choice([-1, 0, 1, 2]); place(tone(deg(cur, 0), beat * 0.9, tri, 0.14, a=0.02, d=0.2, s=0.6, r=0.1, vib=0.006), t0 + st * beat)
        elif style == "calm":
            if b % 4 in (1, 3):
                cur = chord + 7 + rnd.choice([0, 2, 4])
                place(tone(deg(cur, 0), bar * 0.7, sine, 0.12, a=0.1, d=0.5, s=0.6, r=0.3, vib=0.01), t0 + beat * rnd.choice([0, 1]))
        elif style == "dark":
            if b % 4 == 2:
                place(tone(deg(chord + 7 + rnd.choice([0, 1, 4]), 0), bar * 1.2, saw, 0.07, a=0.4, d=0.6, s=0.7, r=0.5, vib=0.004), t0)
        # drums
        if style in ("chip", "sci"):
            for bt in range(4):
                kick(t0 + bt * beat); hat(t0 + bt * beat + beat / 2)
                if bt in (1, 3): snare(t0 + bt * beat)
            if style == "chip" and b % 4 == 3: snare(t0 + 3.5 * beat, 0.35)
        elif style == "warm":
            kick(t0, 0.5); kick(t0 + 2.5 * beat, 0.4); snare(t0 + 2 * beat, 0.3)
            for bt in range(8): hat(t0 + bt * beat / 2, 0.08)
        elif style == "dark":
            if b % 2 == 0: kick(t0, 0.6)
            if b % 4 == 3: snare(t0 + 3 * beat, 0.25)
    # loop-friendly: no reverb tail; soft-clip and normalize
    out = np.tanh(out * 1.6)
    out = norm(out, 0.85)
    if loop:
        f = int(0.01 * SR); out[:f] *= np.linspace(0, 1, f); out[-f:] *= np.linspace(1, 0, f)
    else:
        f = int(0.5 * SR); out[-f:] *= np.linspace(1, 0, f)
    return out

def render_style2(name, bpm, root, scale, bars, chords, style, seed, loop=True):
    rnd = random.Random(seed)
    sc = SCALES[scale]
    beat = 60.0 / bpm; bar = beat * 4
    total = int(bars * bar * SR) + (0 if loop else int(3 * SR))
    out = np.zeros(total)
    def deg(d, octave=0):
        o, i = divmod(d, len(sc)); return root + sc[i] + 12 * (o + octave)
    def place(x, at):
        s0 = int(at * SR) % total if loop else int(at * SR)
        e = min(total, s0 + len(x)); out[s0:e] += x[:e - s0]
        if loop and s0 + len(x) > total: rest = x[total - s0:]; out[:len(rest)] += rest   # wrap tails round the loop
    def tone(midi, dur, wave_fn, vol, a=0.01, d=0.1, s_=0.6, r=0.05, vib=0.0, duty=0.5):
        n = int(dur * SR); f = freq(midi)
        if vib: f = f * (1 + vib * np.sin(2 * math.pi * 5.0 * t(n)))
        w = wave_fn(f, n, duty) if wave_fn is square else wave_fn(f, n)
        return w * env(n, a, d, s_, r) * vol
    def bell(midi, dur, vol):
        n = int(dur * SR); f = freq(midi); tt = t(n)
        w = np.sin(2 * math.pi * f * tt) + 0.5 * np.sin(2 * math.pi * f * 2.76 * tt) * np.exp(-tt * 6) + 0.25 * np.sin(2 * math.pi * f * 5.4 * tt) * np.exp(-tt * 9)
        return w * np.exp(-tt * (3.0 / dur)) * vol
    def tick(at, vol=0.12, hi=True):
        n = int(0.02 * SR); place(noise(n, int(at * 997) % 991 + 7) * env(n, 0.0005, 0.015) * vol * (1 if hi else 0.6), at)
    def drum(at, vol=0.5, f0=110, f1=45):
        n = int(0.25 * SR); place(sine(sweep(f0, f1, n), n) * env(n, 0.002, 0.22) * vol, at)
    for b in range(bars):
        chord = chords[b % len(chords)]; t0 = b * bar
        if style == "bell":
            place(tone(deg(chord, -1), bar * 1.05, sine, 0.10, a=0.6, d=0.6, s_=0.8, r=0.5), t0)
            place(tone(deg(chord + 4, -1), bar * 1.05, sine, 0.06, a=0.6, d=0.6, s_=0.8, r=0.5), t0)
            for i in range(8):
                if rnd.random() < 0.7:
                    m = deg(chord + rnd.choice([0, 2, 4, 7, 9]), 1)
                    b_ = bell(m, beat * 2.2, 0.16); place(b_, t0 + i * beat / 2); place(b_ * 0.35, t0 + i * beat / 2 + beat * 0.75)   # echo
        elif style == "clock":
            for i in range(8): tick(t0 + i * beat / 2, 0.10 if i % 2 else 0.16)
            for i in range(8):
                m = deg(chord + [0, 2, 4, 7][i % 4], 0 + (i // 4))
                place(tone(m, beat * 0.3, square, 0.10, a=0.002, d=0.06, s_=0.3, r=0.02, duty=0.25), t0 + i * beat / 2)
            place(tone(deg(chord, -2), beat * 0.8, tri, 0.22, a=0.005, d=0.2, s_=0.5, r=0.05), t0)
            place(tone(deg(chord + 4, -2), beat * 0.8, tri, 0.18, a=0.005, d=0.2, s_=0.5, r=0.05), t0 + 2 * beat)
            if b % 4 == 3: place(bell(deg(chord + 7, 1), beat * 3, 0.12), t0 + 3 * beat)
        elif style == "desert":
            place(tone(deg(0, -2), bar * 1.02, saw, 0.07, a=0.2, d=0.3, s_=0.9, r=0.2), t0)   # the drone never moves
            for bt in (0, 1.5, 2, 3): drum(t0 + bt * beat, 0.35 if bt else 0.5, 160, 70)
            for bt in (0.75, 2.75): tick(t0 + bt * beat, 0.12)
            if b % 2 == 1 or rnd.random() < 0.3:
                cur = chord + 7
                for st in (0, 0.5, 1, 1.5, 2, 3):
                    cur += rnd.choice([-1, -1, 0, 1, 2]); place(tone(deg(cur, 0), beat * 0.55, saw, 0.07, a=0.02, d=0.1, s_=0.6, r=0.08, vib=0.012), t0 + st * beat)
        elif style == "murk":
            n = int(bar * SR); wob = 1 + 0.02 * np.sin(2 * math.pi * 0.5 * t(n))
            place(sine(freq(deg(chord, -2)) * wob, n) * env(n, 0.3, 0.5, 0.8, 0.4) * 0.22, t0)
            place(tone(deg(chord + 2, -1), bar, tri, 0.06, a=0.5, d=0.5, s_=0.8, r=0.4), t0)
            if b % 2 == 0: drum(t0, 0.3, 80, 40)
            if rnd.random() < 0.6:
                m = deg(chord + rnd.choice([4, 6, 7]), 0); place(tone(m, beat * 1.6, sine, 0.12, a=0.15, d=0.3, s_=0.6, r=0.3, vib=0.02), t0 + beat * rnd.choice([1, 2]))
        elif style == "eerie":
            n = int(bar * 1.1 * SR)
            for dg, v in ((chord, 0.08), (chord + 1, 0.05), (chord + 5, 0.04)):
                f = freq(deg(dg, -1)); place((sine(f, n) + sine(f * 1.007, n)) * env(n, 0.8, 0.5, 0.9, 0.6) * v, t0)
            if rnd.random() < 0.5 and name != "crypt":   # 1.1.6 (#28): a breathy whisper - it was a slow low swell that read as waves
                k = int(beat * 0.9 * SR); w_ = noise(k, b + seed); w_ = lowpass(w_, 3800) - lowpass(w_, 1300)
                place(w_ * (0.7 + 0.3 * np.sin(2 * math.pi * 9 * t(k))) * env(k, 0.12, 0.2, 0.6, 0.3) * 0.22, t0 + beat)
            if b % 4 == 3: place(bell(deg(chord + 6, 1), beat * 4, 0.10), t0 + 2 * beat)
        elif style == "ruin":
            place(tone(deg(chord, -2), bar, tri, 0.14, a=0.4, d=0.5, s_=0.7, r=0.4), t0)
            for i in range(4):
                if rnd.random() < 0.55:
                    m = deg(chord + rnd.choice([0, 2, 4, 6]), 1)
                    p_ = tone(m, beat * 0.25, square, 0.07, a=0.002, d=0.05, s_=0.3, r=0.03, duty=0.125); place(p_, t0 + i * beat); place(p_ * 0.4, t0 + i * beat + beat * 0.5)
            if b % 2 == 0: tick(t0 + 2 * beat, 0.08)
    out = np.tanh(out * 1.5); out = norm(out, 0.85)
    if loop:
        f = int(0.01 * SR); out[:f] *= np.linspace(0, 1, f); out[-f:] *= np.linspace(1, 0, f)
    else:
        f = int(1.5 * SR); out[-f:] *= np.linspace(1, 0, f)
    return out

def save_music(name, x):
    wavp = os.path.join(OUT, "music", name + ".wav"); oggp = os.path.join(OUT, "music", name + ".ogg")
    os.makedirs(os.path.dirname(wavp), exist_ok=True)
    with wave.open(wavp, "wb") as w:
        w.setnchannels(1); w.setsampwidth(2); w.setframerate(SR)
        w.writeframes((np.clip(x, -1, 1) * 32767).astype("<i2").tobytes())
    if to_ogg(wavp, x, 3): os.remove(wavp)
    else: print("ogg encoding failed, keeping", wavp)

def music():
    tracks = [
        # name, bpm, root midi, scale, bars, chord degrees, style, seed, loop
        ("title", 84, 57, "dorian", 16, [0, 5, 3, 4], "calm", 1, True),
        ("overworld", 118, 60, "major", 16, [0, 3, 4, 0, 5, 3, 4, 4], "chip", 2, True),
        ("town", 96, 62, "major", 16, [0, 5, 3, 4], "warm", 3, True),
        ("forest", 100, 55, "lydian", 16, [0, 1, 4, 0], "warm", 9, True),
        ("dungeon", 72, 50, "phrygian", 16, [0, 1, 0, 4], "dark", 4, True),
        ("tech", 126, 52, "minor", 16, [0, 5, 6, 4], "sci", 5, True),
        ("combat", 150, 52, "minor", 16, [0, 0, 5, 6, 0, 0, 3, 4], "chip", 6, True),
        ("boss", 160, 50, "phrygian", 16, [0, 1, 0, 6, 0, 1, 5, 6], "chip", 7, True),
        ("victory", 130, 60, "major", 4, [0, 3, 4, 0], "chip", 8, False),
        ("gameover", 60, 50, "minor", 4, [0, 5, 3, 0], "dark", 10, False),
        # 1.0: a theme per town and per kind of dungeon, a night overworld and the ending
        ("brasswick", 108, 60, "major", 16, [0, 4, 5, 3], "clock", 21, True),
        ("ashfall", 80, 52, "dorian", 16, [0, 6, 5, 4], "ruin", 22, True),
        ("murkwater", 70, 50, "dorian", 16, [0, 3, 0, 4], "murk", 23, True),
        ("sunscar", 100, 52, "phrygian", 16, [0, 1, 0, 6], "desert", 24, True),
        ("frosthold", 66, 64, "lydian", 16, [0, 4, 1, 4], "bell", 25, True),
        ("crypt", 60, 48, "minor", 16, [0, 5, 1, 4], "eerie", 26, True),
        ("swamp", 64, 47, "phrygian", 16, [0, 1, 3, 1], "murk", 27, True),
        ("rift", 56, 49, "phrygian", 16, [0, 1, 6, 5], "eerie", 28, True),
        ("overworld_night", 76, 57, "dorian", 16, [0, 3, 5, 4], "calm", 29, True),
        ("ending", 72, 60, "major", 24, [0, 4, 5, 3, 0, 4, 3, 4], "bell", 30, True),
    ]
    names = []
    only = os.environ.get("TRACKS")   # TRACKS=brasswick,ending: just those
    for tr in tracks:
        if only and tr[0] not in only.split(","): continue
        print("music", tr[0]); save_music(tr[0], render_track(*tr)); names.append(tr[0])
    return names

# ------------------------------------------------------------------ ambient loops (1.0)
# Quiet beds that play under the music (Settings: SFX volume): wind, cave drips, the swamp, rain, a town, machines,
# night insects, the Rift; 1.1.6: a crypt, a storm, forest birdsong, a fire, the sea. Each loop is 24 s and seamless (the end crossfades into the start).
def ambient():
    L = 24.0; n = int(L * SR); rnd = np.random.RandomState(42)
    def seamless(x):
        f = int(2.0 * SR); y = x[:n].copy(); y[:f] = y[:f] * np.linspace(0, 1, f) + x[n:n + f] * np.linspace(1, 0, f); return y
    def bed(cut, seed, amp=1.0, lfo=0.07):
        x = lowpass(noise(n + int(2 * SR), seed), cut); tt = t(len(x))
        return x * (0.6 + 0.4 * np.sin(2 * math.pi * lfo * tt + seed)) * amp
    def drop(f, dur=0.25):
        k = int(dur * SR); tt = t(k); return np.sin(2 * math.pi * (f * (1 + 0.5 * np.exp(-tt * 40))) * tt) * np.exp(-tt * 18)
    def put(out, x, at):
        s0 = int(at * SR) % n; e = min(n, s0 + len(x)); out[s0:e] += x[:e - s0]
        if s0 + len(x) > n: out[:s0 + len(x) - n] += x[n - s0:]
    loops = {}
    # wind: two slow gusting noise bands
    # 1.1.6 (#28): a slow low swell read as surf; wind is now a higher band with uneven gusts and a faint whistle
    def gusts(seed, m):
        g = np.random.RandomState(seed); tt = t(m); y = np.zeros(m)
        for k in range(4): y += np.sin(2 * math.pi * g.uniform(0.07, 0.31) * tt + g.uniform(0, 6.3)) * g.uniform(0.5, 1.0)
        return np.clip(0.55 + 0.25 * y / 2.0, 0.08, 1.0)
    m_ = n + int(2 * SR); gst = gusts(21, m_); nz = noise(m_, 1)
    band = lowpass(nz, 2600) - lowpass(nz, 650)
    whistle = sine(780 * (1 + 0.06 * (gst - 0.5)), m_) * (gst ** 3) * 0.06
    loops["amb_wind"] = seamless(band * gst * 1.2 + lowpass(noise(m_, 2), 4500) * gst ** 2 * 0.25 + whistle)
    # cave: a low rumble and water dripping into pools
    x = seamless(bed(160, 3, 0.8, 0.03))
    for k in range(18):   # 1.1.6: lower, rounder drops (the high ones chirped like birds); same random draws as before
        f_ = rnd.uniform(900, 2200) * 0.42; a_ = rnd.uniform(0.15, 0.35); at_ = rnd.uniform(0, L)
        if k % 3 != 2: put(x, lowpass(drop(f_, 0.3), 1500) * a_ * 1.6, at_)
    loops["amb_cave"] = x
    # swamp: a wet bed, frogs croaking, a mosquito whine now and then
    x = seamless(bed(700, 5, 0.5, 0.09))
    for k in range(26):
        f = rnd.uniform(120, 220); kk = int(0.18 * SR); croak = square(f * (1 + 0.3 * np.sin(2 * math.pi * 30 * t(kk))), kk) * env(kk, 0.01, 0.15) * 0.10
        put(x, lowpass(croak, 900), rnd.uniform(0, L))
    for k in range(3): kk = int(1.2 * SR); put(x, sine(rnd.uniform(560, 640) * (1 + 0.02 * np.sin(2 * math.pi * 7 * t(kk))), kk) * env(kk, 0.4, 0.4, 0.5, 0.4) * 0.025, rnd.uniform(0, L))
    loops["amb_swamp"] = x
    # rain: bright hiss and fat drops
    x = seamless(bed(4000, 7, 0.55, 0.13) + bed(900, 8, 0.4, 0.05))
    for k in range(260): put(x, noise(int(0.01 * SR), k) * env(int(0.01 * SR), 0.0005, 0.008) * rnd.uniform(0.05, 0.2), rnd.uniform(0, L))
    loops["amb_rain"] = x
    # town: distant voices (band-limited noise that comes and goes) and a smith's hammer somewhere
    x = seamless(bed(1200, 9, 0.35, 0.21) - bed(300, 10, 0.3, 0.17))
    for k in range(10):
        at = rnd.uniform(0, L - 2); f = rnd.uniform(1800, 2600)
        for j in range(rnd.randint(2, 4)): kk = int(0.15 * SR); put(x, (sine(f, kk) + 0.5 * sine(f * 2.4, kk)) * np.exp(-t(kk) * 30) * 0.08, at + j * 0.55)
    loops["amb_town"] = x
    # machines: mains hum, a slow pump, relays clicking
    tt = t(n + int(2 * SR)); hum = (np.sin(2 * math.pi * 60 * tt) * 0.4 + np.sin(2 * math.pi * 120 * tt) * 0.25 + np.sin(2 * math.pi * 180 * tt) * 0.1) * (0.8 + 0.2 * np.sin(2 * math.pi * 0.25 * tt))
    x = seamless(hum * 0.5 + bed(300, 11, 0.3, 0.25))
    for k in range(30): put(x, noise(int(0.006 * SR), 100 + k) * 0.25, rnd.uniform(0, L))
    loops["amb_machine"] = x
    # night: crickets in pulses over a hush of wind
    x = seamless(bed(400, 12, 0.25, 0.04))
    for k in range(70):
        at = rnd.uniform(0, L); f = rnd.uniform(4200, 5200)
        for j in range(3): kk = int(0.03 * SR); put(x, sine(f, kk) * env(kk, 0.003, 0.02) * 0.05, at + j * 0.045)
    loops["amb_night"] = x
    # the Rift: two detuned drones breathing in and out
    tt = t(n + int(2 * SR)); breath = 0.5 + 0.5 * np.sin(2 * math.pi * (1 / 8.0) * tt)
    x = seamless((np.sin(2 * math.pi * 55 * tt) + np.sin(2 * math.pi * 55.6 * tt) + 0.4 * np.sin(2 * math.pi * 82.5 * tt)) * 0.25 * breath + bed(2500, 13, 0.15, 0.3))
    loops["amb_rift"] = x
    # ---- 1.1.6 (#28): loops for places the first set didn't fit (own random streams, so the ones above stay the same)
    r2 = np.random.RandomState(616)
    # crypt: still, heavy air - a deep hum, a slow low moan through the halls, grit falling from old stone. No drips, no chirps.
    tt = t(n + int(2 * SR)); moan = np.sin(2 * math.pi * (98 + 6 * np.sin(2 * math.pi * 0.045 * tt)) * tt) * (0.5 + 0.5 * np.sin(2 * math.pi * (1 / 12.0) * tt)) ** 2
    x = seamless(bed(120, 31, 0.9, 0.02) + moan * 0.07 + lowpass(noise(len(tt), 32), 300) * 0.15)
    for k in range(7):
        kk = int(r2.uniform(0.3, 0.8) * SR); gr = lowpass(noise(kk, 40 + k) * (r2.uniform(0, 1, kk) < 0.08), 1800) * env(kk, 0.01, 0.3, 0.3, 0.2)
        put(x, gr * 1.5, r2.uniform(0, L))
    loops["amb_crypt"] = x
    # storm: the rain loop's hiss, heavier, with thunder rolling in the distance
    x = seamless(bed(4000, 7, 0.65, 0.13) + bed(900, 8, 0.55, 0.05) + bed(250, 33, 0.35, 0.03))
    for k in range(3):
        kk = int(r2.uniform(3.0, 4.5) * SR); tk = t(kk); rumble = lowpass(lowpass(noise(kk, 50 + k), 140), 140)
        shape = (1 - np.exp(-tk * r2.uniform(3, 9))) * np.exp(-tk * 0.9) * (0.75 + 0.25 * np.sin(2 * math.pi * 1.7 * tk))
        put(x, rumble * shape * r2.uniform(5.0, 8.0), k * 8.0 + r2.uniform(0.5, 4.0))
    loops["amb_storm"] = x
    # forest / fields by day: leaves in a light breeze and birdsong (only on maps that are woods or farmland)
    m_ = n + int(2 * SR); gst = gusts(60, m_); nz = noise(m_, 61)
    x = seamless((lowpass(nz, 5000) - lowpass(nz, 1500)) * gst * 0.30)
    for k in range(16):
        at = r2.uniform(0, L); base = r2.uniform(2600, 4200); kind = r2.randint(0, 3)
        for j in range(r2.randint(2, 6)):
            kk = int(r2.uniform(0.05, 0.11) * SR); tk = t(kk)
            if kind == 0: fr = base * (1 + 0.25 * tk / tk[-1])            # rising chirp
            elif kind == 1: fr = base * (1.2 - 0.3 * tk / tk[-1])         # falling note
            else: fr = base * (1 + 0.08 * np.sin(2 * math.pi * 40 * tk))  # trill
            put(x, sine(fr, kk) * np.sin(math.pi * tk / tk[-1]) ** 2 * r2.uniform(0.04, 0.08), at + j * r2.uniform(0.11, 0.2))
    loops["amb_forest"] = x
    # fire: a campfire or hearth - a soft roar and crackles
    x = seamless(bed(350, 70, 0.45, 0.4) + bed(1200, 71, 0.12, 0.6))
    for k in range(150):
        kk = int(r2.uniform(0.002, 0.008) * SR); put(x, noise(kk, 200 + k) * env(kk, 0.0003, 0.004) * r2.uniform(0.1, 0.5), r2.uniform(0, L))
    for k in range(10):
        kk = int(0.09 * SR); put(x, lowpass(noise(kk, 400 + k), 2500) * np.exp(-t(kk) * 40) * r2.uniform(0.3, 0.6), r2.uniform(0, L))
    loops["amb_fire"] = x
    # sea: surf on a shore - long swells breaking and drawing back (for coasts; no base-game map uses it)
    tt = t(n + int(2 * SR)); P = 8.0   # 3 waves per 24 s loop, so it stays seamless
    ph = (tt % P) / P; swell = np.where(ph < 0.35, (ph / 0.35) ** 2, np.exp(-(ph - 0.35) * 5.0))
    nz = noise(len(tt), 80); x = seamless((lowpass(nz, 700) * 1.0 + (lowpass(nz, 3500) - lowpass(nz, 900)) * 0.7) * (0.12 + swell))
    loops["amb_sea"] = x
    only = os.environ.get("AMBIENT")   # 1.1.6: AMBIENT=amb_wind,amb_cave writes just those
    if only: loops = {k: v for k, v in loops.items() if k in only.split(",")}
    os.makedirs(os.path.join(OUT, "ambient"), exist_ok=True)
    for name, x in loops.items():
        x = norm(x, 0.7); wavp = os.path.join(OUT, "ambient", name + ".wav"); oggp = wavp[:-4] + ".ogg"
        with wave.open(wavp, "wb") as w:
            w.setnchannels(1); w.setsampwidth(2); w.setframerate(SR); w.writeframes((np.clip(x, -1, 1) * 32767).astype("<i2").tobytes())
        if to_ogg(wavp, x, 1): os.remove(wavp)
        print("ambient", name)
    return list(loops)

if __name__ == "__main__":
    if "sfx" in WHAT: names = sfx(); print("sfx:", len(names))
    if "music" in WHAT: music()
    if "ambient" in WHAT: ambient()
