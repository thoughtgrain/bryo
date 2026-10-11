#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Bryo's factory reels, in the tape's own format, into a C header.

A reel is what a tape plays before you record on it (docs/bryo-architecture.md, "Where TAPE's audio comes from").
The format is the tape's, so a reel plays straight from flash and copying it into a tape is a plain copy:

  IMA ADPCM, 4 bits, mono, 22,050 Hz, low nibble first, in blocks of 256 samples (128 bytes). Each block has the
  decoder state at its start (predictor, step index), so any block decodes on its own: slices, reverse and grains
  start anywhere. Each block also has its peak (0..255), which the screen draws.

The sounds are made here from material already in the repository (no download, nothing third party beyond the
CC0 set): Felucca's generated drums (tools/gen_waves.py), the CC0 piano and flute (assets/samples-cc0, Versilian
Studios, see its ATTRIBUTION.txt), and a Karplus-Strong string made here. The three melodic reels are generative
patches, one bar each, frozen with fixed seeds (below, before reel_keys).

  tools/gen_reels.py OUT.h
"""
import math
import random
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import gen_waves as gw  # noqa: E402
from sampleio import IMA_IDX, IMA_STEP, read_any_wav, resample  # noqa: E402

SRC = Path(__file__).resolve().parents[1]
SR = 22050
BLK = 256
TAPE_SAMPLES = 288 * BLK                     # a tape's capacity (firmware/src/tape.c TAPE_NBLK)
PEAK = 0.8 * 32767


def norm(x, peak=PEAK):
    m = max(1e-9, max(abs(v) for v in x))
    return [v * peak / m for v in x]


def place(buf, at, s, gain=1.0):
    for i, v in enumerate(s):
        if at + i >= len(buf):
            break
        buf[at + i] += v * gain


def pitched(x, sr, semis):
    """x played back semis semitones up (resampled to SR, shorter when up)"""
    return resample(x, sr * 2 ** (semis / 12), SR)


def reel_beat():
    """one bar of 16ths at 120 BPM (2 s): kick, snare, closed and open hats, clap, rim"""
    kit = {k: resample(norm(v, 1.0), gw.SR, SR) for k, v in (
        ("K", gw.drum_kick()), ("S", gw.drum_snare()), ("H", gw.drum_metal_hat(0.045)),
        ("O", gw.drum_metal_hat(0.28)), ("C", gw.drum_clap()), ("R", gw.drum_rim()))}
    pat = {"K": "x.....x.x..x....", "S": "....x.......x...", "H": "x.x.x.x.x.x.x.x.",
           "O": "...............x", "C": "............x...", "R": "..........x...x."}
    gain = {"K": 1.0, "S": 0.8, "H": 0.35, "O": 0.3, "C": 0.6, "R": 0.4}
    step = SR // 8                               # a 16th at 120 BPM
    buf = [0.0] * (16 * step)
    for k, p in pat.items():
        for i, c in enumerate(p):
            if c == "x":
                place(buf, i * step, kit[k], gain[k])
    return buf


# The melodic reels (KEYS, AIR, PLUK) are little generative modular patches, each frozen into one bar: a clock, a
# Turing machine or a sample and hold picking notes from a scale, Euclidean gates, an LFO slowly moving a filter, an
# echo. Every random choice comes from its own random.Random(seed), so the bytes are the same every run; the seeds
# are the ones whose bar I liked. They share one scale (A minor without the F: A B C D E G), so they sit together
# when all four play at power-on.
#
# A bar is BAR samples: 2 s at 120 BPM rounded up to whole tape blocks (44,288 rather than 44,100: a reel is a
# whole number of blocks, so that's what loops), and the 16 steps divide it evenly. Each reel is built on a circle:
# a note or an echo that runs past the end carries on at the start, and the filters run two laps so the second lap
# starts from where the first one ended. The loop has no seam to click.

BAR = -(-2 * SR // BLK) * BLK                # one 4/4 bar at 120 BPM, in whole blocks (173 of them)
STEP = BAR // 16                             # a 16th (BAR is 16 x 2768)
SCALE = (0, 2, 3, 5, 7, 10)                  # A minor without the F, from A: A B C D E G


def scale_note(i, root=57):
    """the i-th note of SCALE up from root (MIDI; 57 is A3), i any integer"""
    return root + 12 * (i // len(SCALE)) + SCALE[i % len(SCALE)]


def euclid(k, n, rot=0):
    """k onsets spread as evenly as they go over n steps (a Euclidean rhythm), rotated by rot"""
    return [((i + rot) * k) % n < k for i in range(n)]


def wplace(buf, at, s, gain=1.0):
    """place, but around the circle: what runs past the end comes in at the start"""
    n = len(buf)
    for i, v in enumerate(s):
        buf[(at + i) % n] += v * gain


def seam(buf, out_ms, in_ms):
    """a dip at the loop point, out_ms going out and in_ms coming in (raised cosine). The tape's encoder starts
    the reel from silence (predictor 0, the smallest step), so a reel that starts loud would click as the decoder
    catches up: a few milliseconds of fade let it follow. AIR's is longer, a breath at the bar."""
    a, b = int(SR * out_ms / 1000), int(SR * in_ms / 1000)
    for j in range(b):
        buf[j] *= 0.5 - 0.5 * math.cos(math.pi * j / b)
    for j in range(a):
        buf[-1 - j] *= 0.5 - 0.5 * math.cos(math.pi * j / a)
    return buf


def lfo(i, phase=0.0, cycles=1):
    """0..1, a raised cosine that goes round `cycles` times a bar (so it loops with it)"""
    return 0.5 - 0.5 * math.cos(2 * math.pi * (cycles * i / BAR + phase))


def svf_loop(x, cutoff, q=0.7, mode="low"):
    """a state-variable filter (the trapezoidal one: stable however fast cutoff moves) over a circular buffer,
    cutoff(i) in Hz read every 32 samples like the firmware's block. Two laps; the second one is kept."""
    n, k = len(x), 1 / q
    out = [0.0] * n
    ic1 = ic2 = 0.0
    for lap in range(2):
        for i in range(n):
            if not i & 31:
                g = math.tan(math.pi * min(cutoff(i), 0.45 * SR) / SR)
                a1 = 1 / (1 + g * (g + k))
                a2, a3 = g * a1, g * g * a1
            v3 = x[i] - ic2
            v1 = a1 * ic1 + a2 * v3
            v2 = ic2 + a2 * ic1 + a3 * v3
            ic1, ic2 = 2 * v1 - ic1, 2 * v2 - ic2
            if lap:
                out[i] = v2 if mode == "low" else v1
    return out


def echo_loop(x, delay, fb, wet, damp=0.3):
    """a tape echo around the circle: each repeat fb as loud and a little darker (one-pole, damp), added at wet.
    Three laps, so the repeats that wrap round have caught up with themselves (fb^15 or so: nothing left)."""
    n = len(x)
    y = [0.0] * n                            # the echo line: what comes back
    lp = 0.0
    for _ in range(3):
        for i in range(n):
            j = (i - delay) % n
            lp += (1 - damp) * ((x[j] + y[j]) * fb - lp)
            y[i] = lp
    return [a + b * wet / max(fb, 1e-9) for a, b in zip(x, y)]


def turing(rng, length=16, write=24, flip=0.3):
    """a Turing machine: a 16-bit shift register that feeds its last bit back to the front, flipped now and then
    with probability flip. It runs `write` steps with flips (the knob a little left of locked, finding a phrase),
    then length steps locked (no flips): a 16-step loop, which is what the reel keeps. -> the register per step"""
    reg = rng.getrandbits(16)
    for _ in range(write):
        bit = reg >> 15 & 1
        if rng.random() < flip:
            bit ^= 1
        reg = (reg << 1 | bit) & 0xFFFF
    out = []
    for _ in range(length):
        reg = (reg << 1 | reg >> 15 & 1) & 0xFFFF
        out.append(reg)
    return out


_PIANO = {}


def piano(midi):
    """the CC0 piano at a MIDI note, from the nearest of its C3 C4 C5 (never more than 6 semitones off)"""
    if midi not in _PIANO:
        root = min((48, 60, 72), key=lambda r: (abs(midi - r), r))
        sr, x = read_any_wav(SRC / f"assets/samples-cc0/PIANO/0{2 + (root - 48) // 12}_GPiano_sus_C{root // 12 - 1}"
                             "_v2_rr1_Player.wav")
        _PIANO[midi] = pitched(norm(x, 1.0), sr, midi - root)
    return _PIANO[midi]


def keys_events(seed=17):
    """KEYS' notes: [(step, sub, midi, velocity, length in samples)]; sub is the ratchet's offset in samples"""
    rng = random.Random(seed)
    reg = turing(rng)
    gate = euclid(7, 16, 2)
    for i in range(16):                      # the register's bit 0 opens a few more (a second gate source)
        if reg[i] & 1 and reg[i] >> 9 & 1 and not gate[i]:
            gate[i] = True
    on = [i for i in range(16) if gate[i]]
    ratchet = rng.choice([i for i in on if i % 4 and (i + 1) % 16 not in on] or on)
    ev = []
    for n, i in enumerate(on):
        r = reg[i]
        deg = ((r >> 4) & 0xFF) * 10 // 256  # 8 bits of the register, quantized to 10 notes of the scale
        midi = scale_note(deg)
        if r >> 13 & 7 == 7:                 # one time in eight: the octave up
            midi += 12
        vel = 0.55 + 0.15 * (r >> 1 & 3)
        if i % 4 == 0:
            vel = min(1.0, vel + 0.2)        # the beat leans a little
        gap = ((on[(n + 1) % len(on)] - i - 1) % 16 + 1) * STEP
        ln = int(gap * (0.35 + 0.6 * (r >> 11 & 3) / 3))   # how long the gate stays open: staccato to legato
        if i == ratchet:                     # a ratchet: three 32nd-note triplets, falling away
            for k in range(3):
                ev.append((i, k * STEP // 3, midi, vel * (1 - 0.25 * k), STEP // 3))
        else:
            ev.append((i, 0, midi, vel, ln))
    return ev


def reel_keys():
    """the CC0 piano played by a Turing machine (one bar, 2 s): a locked 16-step register, 8 of its bits read as
    a note of A minor (no F) over 10 steps of the scale, now and then an octave up. The gates are a Euclidean 7 of
    16 plus a few the register's own bits open, the gate lengths come from two more bits (staccato to legato), one
    note is ratcheted. A slow resonant low-pass opens and closes once a bar, and an echo three 16ths behind."""
    buf = [0.0] * BAR
    for step, sub, midi, vel, ln in keys_events():
        x = piano(midi)
        rel = int(SR * 0.08)
        n = min(len(x), ln + rel * 4)
        env = [math.exp(-j / (SR * 0.6)) * (1.0 if j < ln else math.exp(-(j - ln) / rel)) for j in range(n)]
        wplace(buf, step * STEP + sub, [a * b for a, b in zip(x, env)], vel)
    buf = svf_loop(buf, lambda i: 600 * 2 ** (3 * lfo(i, 0.6)), q=1.4)
    return seam(echo_loop(buf, 3 * STEP, 0.4, 0.3, damp=0.5), 3, 3)


_FLUTE = {}


def flute(midi, n):
    """n samples of the CC0 flute at a MIDI note, its sustain looped (crossfaded) as long as it has to be"""
    if midi not in _FLUTE:
        root = 60 if midi < 66 else 72
        sr, x = read_any_wav(SRC / f"assets/samples-cc0/FLUTE/0{(root - 60) // 12}_LDFlute_susvib_C{root // 12 - 1}"
                             "_v1_1.wav")
        _FLUTE[midi] = pitched(norm(x, 1.0), sr, midi - root)
    x = _FLUTE[midi]
    ls, le = len(x) // 4, len(x) * 19 // 20    # the loop: from a quarter in to near the end
    xf = (le - ls) // 4
    out = list(x[:le])
    while len(out) < n:                      # each lap crossfades (equal power) from the tail into the loop start
        tail = out[-xf:]
        del out[-xf:]
        for j in range(xf):
            t = j / xf
            out.append(tail[j] * math.cos(t * math.pi / 2) + x[ls + j] * math.sin(t * math.pi / 2))
        out += x[ls + xf:le]
    return out[:n]


def air_events(seed=3):
    """AIR's upper voice: [(start step, length in steps, midi)]; a sample and hold over chord tones"""
    rng = random.Random(seed)
    tones = [scale_note(i) for i in range(4, 11)]   # E4 .. E5
    lens = rng.choice(((6, 5, 5), (5, 6, 5), (7, 4, 5), (5, 4, 7)))
    ev, at, last = [], rng.randrange(0, 3), None
    for ln in lens:
        midi = rng.choice([m for m in tones if m != last and (last is None or abs(m - last) <= 7)])
        ev.append((at, ln, midi))
        at, last = at + ln, midi
    return ev


def reel_air():
    """the CC0 flute as a slow modular pad (one bar, 2 s). Under it a drone, A3, the flute's sustain looped round
    the bar and swelling with a slow LFO; over it a sample and hold: three notes from the chord's tones held for
    uneven lengths (5, 6, 7 16ths: they don't land on the beat), each fading in and out slowly so they overlap, the
    last one's tail fading into the first one. A low-pass breathes over the whole bar, slower than the notes."""
    buf = [0.0] * BAR
    xf = int(SR * 0.25)
    d = flute(57, BAR + xf)
    d = [d[i] * (1 - i / xf) + d[BAR + i] * i / xf if i < xf else d[i] for i in range(BAR)]
    buf = [v * 0.45 * (0.6 + 0.4 * lfo(i, 0.25)) for i, v in enumerate(d)]
    for at, ln, midi in air_events():
        n = ln * STEP + int(SR * 0.45)
        fi, fo = int(SR * 0.18), int(SR * 0.45)
        s = flute(midi, n)
        wplace(buf, at * STEP, [v * min(1.0, j / fi, (n - j) / fo) for j, v in enumerate(s)], 0.6)
    return seam(svf_loop(buf, lambda i: 900 * 2 ** (2.2 * lfo(i, 0.1)), q=0.9), 60, 90)


def ks(freq, secs, bright, decay, seed):
    """a Karplus-Strong string at SR: a burst of noise (low-passed: bright 0..1) round a delay line with an
    averaging filter and an all-pass to tune the fraction of a sample; decay is the loss per pass (0.99..0.999)"""
    rng = random.Random(seed)
    p = SR / freq - 0.5                      # the averaging filter delays half a sample
    n = int(p)
    if p - n < 0.1:
        n -= 1
    d = p - n
    c = (1 - d) / (1 + d)
    lp, line = 0.0, []
    for _ in range(n):
        lp += (0.15 + 0.85 * bright) * (rng.uniform(-1, 1) - lp)
        line.append(lp)
    m = sum(line) / n
    line = [v - m for v in line]
    out, prev, ax, ay = [], 0.0, 0.0, 0.0
    for i in range(int(secs * SR)):
        v = line[i % n]
        out.append(v)
        f = decay * 0.5 * (v + prev)
        prev = v
        ay = c * f + ax - c * ay             # the tuning all-pass
        ax = f
        line[i % n] = ay
    return out


def pluk_events(seed=5):
    """PLUK's notes: [(step, sub, midi, velocity, brightness)]"""
    rng = random.Random(seed)
    low = [45] + [scale_note(i, 45) for i in rng.sample((-2, 2, 3, 4), 2)]     # a 3-note bass cycle from A2
    high = [scale_note(i, 45) for i in rng.sample(range(5, 13), 5)]            # sample and hold, 5 long
    ev, lo, hi = [], 0, 0
    gl, gh = euclid(5, 16, 0), euclid(7, 16, 3)
    rat = rng.choice([i for i in range(16) if gh[i] and not gl[i]])
    for i in range(16):
        b = lfo(i * STEP, 0.35)
        if gl[i]:
            ev.append((i, 0, low[lo % 3], 0.9 if i == 0 else 0.7, 0.3 + 0.4 * b))
            lo += 1
        if gh[i]:
            midi = high[hi % 5]
            if hi == 5:                      # the second time round, its first note jumps the octave
                midi += 12
            acc = rng.random() < 0.35
            vel, br = (0.75, 0.6 + 0.4 * b) if acc else (0.45, 0.25 + 0.5 * b)
            if i == rat:
                for k in range(2):           # a ratchet: two 32nds
                    ev.append((i, k * STEP // 2, midi, vel * (1 - 0.3 * k), br))
            else:
                ev.append((i, 0, midi, vel, br))
            hi += 1
    return ev


def reel_pluck():
    """plucked strings in two Euclidean rhythms against each other (one bar, 2 s): a bass 5 of 16 walking a
    3-note cycle, and over it 7 of 16 (rotated) reading a 5-note sample and hold, so the phrase lands differently
    each time round the bar; one note is accented now and then (louder, brighter), one ratchets, one jumps an
    octave. The pluck's brightness follows a slow LFO, and an echo a quarter-note triplet behind (it doesn't sit
    on the 16ths, which is what makes it sound like a patch rather than a sequencer)."""
    buf = [0.0] * BAR
    for step, sub, midi, vel, br in pluk_events():
        f = 440.0 * 2 ** ((midi - 69) / 12)
        wplace(buf, step * STEP + sub, ks(f, 0.9, br, 0.996 + 0.003 * br, midi * 31 + step), vel)
    return seam(echo_loop(buf, BAR // 6, 0.35, 0.35, damp=0.4), 3, 2)


REELS = [("BEAT", reel_beat), ("KEYS", reel_keys), ("AIR", reel_air), ("PLUK", reel_pluck)]


def encode(pcm):
    """-> (bytes, [(pred, idx)] at each block start, [peak 0..255] per block); the encoder is the tape's own
    (firmware/src/tape.c ima_enc), run continuously from 0, 0"""
    n = (len(pcm) + BLK - 1) // BLK * BLK
    pcm = [int(max(-32768, min(32767, round(v)))) for v in pcm] + [0] * (n - len(pcm))
    pred, idx, out, states, peaks = 0, 0, bytearray(), [], []
    for b in range(0, n, BLK):
        states.append((pred, idx))
        peaks.append(min(255, max(abs(v) for v in pcm[b:b + BLK]) >> 7))
        nib = []
        for x in pcm[b:b + BLK]:
            step = IMA_STEP[idx]
            diff, code = x - pred, 0
            if diff < 0:
                code, diff = 8, -diff
            vd = step >> 3
            if diff >= step:
                code |= 4
                diff -= step
                vd += step
            if diff >= step >> 1:
                code |= 2
                diff -= step >> 1
                vd += step >> 1
            if diff >= step >> 2:
                code |= 1
                vd += step >> 2
            pred = max(-32768, min(32767, pred - vd if code & 8 else pred + vd))
            idx = max(0, min(88, idx + IMA_IDX[code & 7]))
            nib.append(code)
        out += bytes(a | (c << 4) for a, c in zip(nib[0::2], nib[1::2]))
    return bytes(out), states, peaks


def c_bytes(name, data, per=24):
    rows = [", ".join(str(b) for b in data[i:i + per]) for i in range(0, len(data), per)]
    return f"static const uint8_t {name}[{len(data)}] = {{\n    " + ",\n    ".join(rows) + "\n};\n"


def main():
    if len(sys.argv) != 2:
        raise SystemExit(__doc__)
    parts = ["/* generated by tools/gen_reels.py: Bryo's factory reels (the tape's format: firmware/src/tape.c) */",
             "#pragma once", "#include <stdint.h>", ""]
    table = []
    for i, (name, make) in enumerate(REELS):
        random.seed(1 + i)                       # the drums' noise (gen_waves uses the global generator): fixed
        pcm = norm(make())[:TAPE_SAMPLES]
        data, st, pk = encode(pcm)
        parts.append(c_bytes(f"REEL{i}_DATA", data))
        parts.append(f"static const int16_t REEL{i}_PRED[{len(st)}] = {{{', '.join(str(p) for p, _ in st)}}};")
        parts.append(f"static const uint8_t REEL{i}_IDX[{len(st)}] = {{{', '.join(str(x) for _, x in st)}}};")
        parts.append(c_bytes(f"REEL{i}_PEAK", bytes(pk)))
        table.append(f'    {{"{name}", {len(st)}u, REEL{i}_DATA, REEL{i}_PRED, REEL{i}_IDX, REEL{i}_PEAK}},')
    parts += [f"#define NREEL {len(REELS)}u",
              "/* reel_t {name, blocks, data, predictor and step index at each block, peak per block} (tape.c) */",
              "#define REELS_INIT \\", "\\\n".join(table).rstrip(",") + "", ""]
    parts.append("#define REEL_NAMES_INIT " + ", ".join(f'"{n}"' for n, _ in REELS))
    Path(sys.argv[1]).write_text("\n".join(parts) + "\n")
    print(f"gen_reels: {len(REELS)} reels")


if __name__ == "__main__":
    main()
