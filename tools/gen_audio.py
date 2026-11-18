# -*- coding: utf-8 -*-
"""Procedurally synthesises all original music loops and sound effects (48 kHz)."""
import os
import numpy as np
import soundfile as sf

SR = 48000
ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
OUT = os.path.join(ROOT, "romfs", "audio")
rng = np.random.default_rng(1234)


def t_arr(dur):
    return np.arange(int(dur * SR)) / SR


def midi(n):
    return 440.0 * 2 ** ((n - 69) / 12.0)


def env_adsr(n, a, d, s, r, sus_len=None):
    a, d, r = int(a * SR), int(d * SR), int(r * SR)
    e = np.ones(n) * s
    a = max(a, 1)
    e[:min(a, n)] = np.linspace(0, 1, a)[:min(a, n)]
    if a < n:
        dd = min(d, n - a)
        e[a:a + dd] = np.linspace(1, s, max(d, 1))[:dd]
    if r > 0 and n > r:
        e[n - r:] *= np.linspace(1, 0, r)
    return e


def lowpass(x, cutoff):
    # one pole low pass, vectorised via cumulative approach (loop in python is fine for our sizes)
    a = np.exp(-2 * np.pi * cutoff / SR)
    y = np.empty_like(x)
    acc = 0.0
    b = 1 - a
    for i in range(len(x)):
        acc = b * x[i] + a * acc
        y[i] = acc
    return y


def fast_lowpass(x, cutoff):
    from scipy.signal import lfilter  # optional
    a = np.exp(-2 * np.pi * cutoff / SR)
    return lfilter([1 - a], [1, -a], x)


try:
    import scipy  # noqa
    LP = fast_lowpass
except Exception:
    LP = lowpass


def saw(f, t, detune=0.0):
    ph = (f * (1 + detune)) * t
    return 2 * (ph - np.floor(ph + 0.5))


def sq(f, t):
    return np.sign(np.sin(2 * np.pi * f * t))


def tri(f, t):
    ph = f * t
    return 2 * np.abs(2 * (ph - np.floor(ph + 0.5))) - 1


def sine(f, t):
    return np.sin(2 * np.pi * f * t)


def noise(n):
    return rng.uniform(-1, 1, n)


def place(buf, sig, start, gain=1.0, pan=0.0):
    s = int(start * SR)
    if s >= buf.shape[0]:
        return
    e = min(buf.shape[0], s + len(sig))
    l = np.cos((pan + 1) * np.pi / 4)
    r = np.sin((pan + 1) * np.pi / 4)
    buf[s:e, 0] += sig[:e - s] * gain * l * 1.414
    buf[s:e, 1] += sig[:e - s] * gain * r * 1.414


def reverb(buf, mix=0.25, room=0.82):
    out = buf.copy()
    delays = [0.0297, 0.0371, 0.0411, 0.0437, 0.0533, 0.0617]
    for ch in range(2):
        acc = np.zeros(buf.shape[0])
        for i, d in enumerate(delays):
            dn = int((d + ch * 0.0023) * SR)
            y = buf[:, ch].copy()
            # feedback comb approximated with several taps
            g = room
            tap = np.zeros_like(y)
            k = 1
            while k * dn < len(y) and g ** k > 0.02:
                tap[k * dn:] += y[:-k * dn] * (g ** k)
                k += 1
            acc += tap
        out[:, ch] += LP(acc / len(delays), 6000) * mix
    return out


def write_ogg(path, data, sr):
    # libsndfile's vorbis encoder crashes on large single writes; write in blocks
    ch = 1 if data.ndim == 1 else data.shape[1]
    with sf.SoundFile(path, "w", sr, ch, format="OGG", subtype="VORBIS") as f:
        for i in range(0, len(data), 4096):
            f.write(data[i:i + 4096])


def save(name, buf, loop=False, gain=0.9):
    if loop is not False:
        n = loop
        tail = buf[n:]
        main = buf[:n].copy()
        main[:len(tail)] += tail[:n]
        buf = main
    peak = np.max(np.abs(buf)) or 1
    buf = buf / peak * gain
    os.makedirs(OUT, exist_ok=True)
    write_ogg(os.path.join(OUT, name + ".ogg"), buf.astype(np.float32), SR)
    print("wrote", name, round(len(buf) / SR, 2), "s")


# ---------------------------------------------------------------- instruments
def kick(dur=0.35):
    t = t_arr(dur)
    f = 50 + 120 * np.exp(-t * 30)
    ph = 2 * np.pi * np.cumsum(f) / SR
    return np.sin(ph) * np.exp(-t * 9)


def snare(dur=0.25):
    t = t_arr(dur)
    n = LP(noise(len(t)), 7000) * np.exp(-t * 18)
    body = np.sin(2 * np.pi * 190 * t) * np.exp(-t * 25)
    return n * 0.8 + body * 0.5


def hat(dur=0.06):
    t = t_arr(dur)
    n = noise(len(t))
    n = n - LP(n, 6000)
    return n * np.exp(-t * 60)


def clap(dur=0.2):
    t = t_arr(dur)
    n = noise(len(t))
    n = LP(n - LP(n, 900), 5000)
    e = np.exp(-t * 30) + 0.6 * np.exp(-np.maximum(t - 0.012, 0) * 30) * (t > 0.012)
    return n * e


def pad_note(freqs, dur, cutoff=1800, att=0.25):
    t = t_arr(dur)
    s = np.zeros(len(t))
    for f in freqs:
        s += saw(f, t, 0.004) + saw(f, t, -0.005) + 0.5 * saw(f * 2, t, 0.002)
    s = LP(s / len(freqs), cutoff)
    return s * env_adsr(len(t), att, 0.3, 0.8, 0.3)


def pluck(f, dur=0.35, bright=4000):
    t = t_arr(dur)
    s = sq(f, t) * 0.5 + saw(f, t) * 0.5
    s = LP(s, bright) * np.exp(-t * 9)
    return s


def bass(f, dur):
    t = t_arr(dur)
    s = LP(saw(f, t) + 0.6 * sq(f / 2, t), 700)
    return s * env_adsr(len(t), 0.005, 0.1, 0.7, 0.05)


def bell(f, dur=1.2):
    t = t_arr(dur)
    s = sine(f, t) + 0.5 * sine(f * 2.76, t) * np.exp(-t * 4) + 0.3 * sine(f * 5.4, t) * np.exp(-t * 8)
    return s * np.exp(-t * 3)


def brass(freqs, dur):
    t = t_arr(dur)
    s = np.zeros(len(t))
    for f in freqs:
        vib = 1 + 0.004 * np.sin(2 * np.pi * 5.5 * t)
        s += saw(f * vib, t)
    cut = 900 + 2500 * env_adsr(len(t), 0.06, 0.3, 0.5, 0.2)
    s = LP(s / len(freqs), 2600) * 0.6 + LP(s / len(freqs), 900) * 0.4
    return s * env_adsr(len(t), 0.04, 0.2, 0.8, 0.25)


# ---------------------------------------------------------------- music
def music_menu():
    bpm = 118
    beat = 60 / bpm
    bars = 8
    total = bars * 4 * beat
    buf = np.zeros((int((total + 3) * SR), 2))
    prog = [(48, [60, 64, 67]), (45, [57, 60, 64]), (41, [57, 60, 65]), (43, [59, 62, 67])]
    for bar in range(bars):
        root, chord = prog[bar % 4]
        t0 = bar * 4 * beat
        place(buf, pad_note([midi(n) for n in chord], 4 * beat + 0.2, 2200), t0, 0.22, 0)
        for e in range(8):
            place(buf, bass(midi(root - 12 + (12 if e % 4 == 3 else 0)), beat / 2 * 0.9), t0 + e * beat / 2, 0.45)
        for b in range(4):
            place(buf, kick(), t0 + b * beat, 0.75)
            if b % 2 == 1:
                place(buf, clap(), t0 + b * beat, 0.4, 0.1)
            for h in range(2):
                place(buf, hat(), t0 + b * beat + h * beat / 2 + beat / 4, 0.18, 0.3)
        arp = chord + [chord[0] + 12, chord[1] + 12]
        for s in range(16):
            n = arp[(s * 3 + bar) % len(arp)] + 12
            place(buf, pluck(midi(n), 0.3), t0 + s * beat / 4, 0.13, -0.3 + 0.6 * ((s % 4) / 3))
    # melody hook
    mel = [(0, 76, 1), (1, 74, .5), (1.5, 72, .5), (2, 74, 1), (3, 67, 1), (4, 72, 1), (5, 71, .5), (5.5, 69, .5), (6, 71, 2)]
    for rep in range(bars // 2):
        for (b, n, l) in mel:
            place(buf, pluck(midi(n), l * beat + 0.2, 3000) * 1.2, rep * 8 * beat + b * beat, 0.16, 0)
    buf = reverb(buf, 0.18)
    save("music_menu", buf, loop=int(total * SR), gain=0.8)


def music_tension():
    bpm = 84
    beat = 60 / bpm
    bars = 8
    total = bars * 4 * beat
    buf = np.zeros((int((total + 3) * SR), 2))
    chords = [[45, 52, 60], [45, 52, 60], [41, 48, 57], [43, 50, 59]]
    for bar in range(bars):
        t0 = bar * 4 * beat
        ch = chords[bar % 4]
        place(buf, pad_note([midi(n) for n in ch], 4 * beat + 0.5, 900, 0.8), t0, 0.3)
        # heartbeat
        for b in range(4):
            if b % 2 == 0:
                place(buf, kick(0.3) * 0.9, t0 + b * beat, 0.5)
                place(buf, kick(0.3) * 0.6, t0 + b * beat + 0.22, 0.4)
        # ticking
        for s in range(8):
            place(buf, hat(0.03), t0 + s * beat / 2, 0.12 if s % 2 else 0.2, 0.5 if s % 2 else -0.5)
        # high tremolo
        t = t_arr(4 * beat)
        trem = sine(midi(ch[2] + 12), t) * (0.5 + 0.5 * np.sin(2 * np.pi * 8 * t)) * env_adsr(len(t), 1.0, 0.1, 1, 0.5)
        place(buf, trem, t0, 0.05)
    buf = reverb(buf, 0.3)
    save("music_tension", buf, loop=int(total * SR), gain=0.7)


def music_banker():
    bpm = 70
    beat = 60 / bpm
    bars = 4
    total = bars * 4 * beat
    buf = np.zeros((int((total + 4) * SR), 2))
    chords = [[40, 47, 55, 59], [41, 48, 56, 60], [40, 47, 55, 59], [39, 46, 55, 58]]
    for bar in range(bars):
        t0 = bar * 4 * beat
        place(buf, pad_note([midi(n) for n in chords[bar]], 4 * beat + 1, 700, 1.0), t0, 0.35)
        for b in range(4):
            t = t_arr(beat)
            low = sine(midi(chords[bar][0] - 12), t) * np.exp(-t * 3)
            place(buf, low, t0 + b * beat, 0.6)
        place(buf, bell(midi(chords[bar][3] + 12), 2.0), t0 + 2 * beat, 0.08, 0.4)
    buf = reverb(buf, 0.35)
    save("music_banker", buf, loop=int(total * SR), gain=0.7)


# ---------------------------------------------------------------- sfx
def sfx():
    # cursor hover tick
    t = t_arr(0.05)
    save("sfx_hover", (sine(1800, t) * np.exp(-t * 90))[:, None].repeat(2, 1), gain=0.35)
    # select
    t = t_arr(0.18)
    s = sine(880 + 600 * (t < 0.06), t) * np.exp(-t * 20)
    save("sfx_select", s[:, None].repeat(2, 1), gain=0.5)
    # back
    t = t_arr(0.16)
    s = sine(600 - 200 * (t > 0.06), t) * np.exp(-t * 20)
    save("sfx_back", s[:, None].repeat(2, 1), gain=0.45)
    # case open: latch clicks + whoosh
    buf = np.zeros((int(0.9 * SR), 2))
    for i, st in enumerate((0.0, 0.07)):
        t = t_arr(0.04)
        c = LP(noise(len(t)), 5000) * np.exp(-t * 150) + sine(2500, t) * np.exp(-t * 120) * 0.5
        place(buf, c, st, 0.8, -0.3 + 0.6 * i)
    t = t_arr(0.6)
    w = LP(noise(len(t)), 1200) * np.sin(np.pi * t / 0.6) ** 2
    place(buf, w, 0.15, 0.5)
    save("sfx_open", buf, gain=0.7)
    # reveal good: rising chime
    buf = np.zeros((int(2.0 * SR), 2))
    for i, n in enumerate([72, 76, 79, 84]):
        place(buf, bell(midi(n), 1.4), i * 0.08, 0.5, -0.4 + i * 0.27)
    save("sfx_good", reverb(buf, 0.3), gain=0.6)
    # reveal bad: sad descending brass
    buf = np.zeros((int(2.2 * SR), 2))
    for i, n in enumerate([62, 61, 60]):
        place(buf, brass([midi(n), midi(n - 5)], 0.4), i * 0.38, 0.6)
    t = t_arr(1.0)
    f = midi(59) * (1 - 0.03 * np.sin(2 * np.pi * 6 * t))
    place(buf, LP(saw(f, t) + saw(f * 0.75, t), 1500) * env_adsr(len(t), 0.05, 0.2, 0.8, 0.4), 1.14, 0.5)
    save("sfx_bad", reverb(buf, 0.25), gain=0.6)
    # neutral reveal
    buf = np.zeros((int(1.4 * SR), 2))
    place(buf, bell(midi(67), 1.2), 0, 0.6)
    place(buf, bell(midi(72), 1.2), 0.1, 0.4)
    save("sfx_neutral", reverb(buf, 0.3), gain=0.5)
    # phone ring (two rings)
    buf = np.zeros((int(3.2 * SR), 2))
    for r in range(2):
        t = t_arr(0.9)
        s = (sine(1000, t) + sine(1250, t)) * (np.sin(2 * np.pi * 20 * t) > 0) * env_adsr(len(t), 0.01, 0.1, 1, 0.05)
        place(buf, s, r * 1.5, 0.4)
    save("sfx_phone", buf, gain=0.55)
    # drumroll with cymbal
    dur = 1.8
    buf = np.zeros((int((dur + 1.8) * SR), 2))
    k = 0.0
    while k < dur:
        place(buf, snare(0.08) * (0.3 + 0.7 * k / dur), k, 0.5, rng.uniform(-0.2, 0.2))
        k += 0.035
    t = t_arr(1.8)
    cym = noise(len(t))
    cym = (cym - LP(cym, 4000)) * np.exp(-t * 2.5)
    place(buf, cym, dur, 0.7)
    place(buf, kick(0.5), dur, 0.9)
    save("sfx_drumroll", buf, gain=0.75)
    # fanfare
    buf = np.zeros((int(3.0 * SR), 2))
    seq = [(0, [60, 64, 67]), (0.18, [60, 64, 67]), (0.36, [60, 64, 67]), (0.54, [65, 69, 72]), (1.0, [67, 71, 74]), (1.3, [72, 76, 79])]
    for i, (st, ch) in enumerate(seq):
        d = 0.16 if i < 3 else (0.4 if i < 5 else 1.4)
        place(buf, brass([midi(n) for n in ch], d), st, 0.6)
    place(buf, kick(), 1.3, 0.8)
    t = t_arr(1.5)
    cym = noise(len(t)); cym = (cym - LP(cym, 4000)) * np.exp(-t * 2)
    place(buf, cym, 1.3, 0.4)
    save("sfx_fanfare", reverb(buf, 0.3), gain=0.75)
    # applause
    dur = 4.0
    buf = np.zeros((int(dur * SR), 2))
    nclaps = 700
    for i in range(nclaps):
        st = rng.uniform(0, dur - 0.2)
        g = np.sin(np.pi * st / dur) ** 0.6
        place(buf, clap(0.08), st, 0.25 * g * rng.uniform(0.4, 1.0), rng.uniform(-0.9, 0.9))
    save("sfx_applause", reverb(buf, 0.3), gain=0.6)
    # crowd gasp/aww
    t = t_arr(1.4)
    n = noise(len(t))
    sw = 400 + 600 * np.sin(np.pi * t / 1.4)
    s = LP(n, 1200) - LP(n, 300)
    s = s * np.sin(np.pi * t / 1.4) ** 2
    st = np.stack([s, LP(noise(len(t)), 1100) * np.sin(np.pi * t / 1.4) ** 2], 1)
    save("sfx_aww", reverb(st, 0.3), gain=0.5)
    # whoosh
    t = t_arr(0.7)
    n = noise(len(t))
    s = (LP(n, 2500) - LP(n, 400)) * np.sin(np.pi * t / 0.7) ** 3
    save("sfx_whoosh", np.stack([s, s[::-1] * 0.7], 1), gain=0.5)
    # dramatic sting
    buf = np.zeros((int(2.5 * SR), 2))
    place(buf, kick(1.0) * 1.2, 0, 1.0)
    place(buf, brass([midi(45), midi(52), midi(57), midi(60)], 1.4), 0, 0.6)
    save("sfx_sting", reverb(buf, 0.35), gain=0.75)
    # lock-in blip (multiplayer choice)
    t = t_arr(0.25)
    s = sine(660, t) * np.exp(-t * 15) + sine(990, t) * np.exp(-np.maximum(t - 0.08, 0) * 15) * (t > 0.08)
    save("sfx_lock", s[:, None].repeat(2, 1), gain=0.5)


if __name__ == "__main__":
    import sys
    sfx()
    music_menu()
    music_tension()
    music_banker()
