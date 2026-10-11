#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Bryo's factory reels, in the tape's own format, into a C header.

A reel is what a tape plays before you record on it (docs/bryo-architecture.md, "Where TAPE's audio comes from").
The format is the tape's, so a reel plays straight from flash and copying it into a tape is a plain copy:

  IMA ADPCM, 4 bits, mono, 22,050 Hz, low nibble first, in blocks of 256 samples (128 bytes). Each block has the
  decoder state at its start (predictor, step index), so any block decodes on its own: slices, reverse and grains
  start anywhere. Each block also has its peak (0..255), which the screen draws.

The sounds are made here, from code alone (no samples, no download, nothing third party): BEAT is Felucca's
generated drums (tools/gen_waves.py); BELL, DRON and FOLD are small modular-synth patches written below (FM, detuned
oscillators into a filter, a wavefolder into a low-pass gate), one bar each, frozen with fixed seeds (below, before
reel_bell). The owner played the first melodic set (a CC0 piano, flute and a Karplus-Strong string) on the device
and found them a bit annoying, so these are deliberately plainer: rooted on A, few notes, soft edges.

  tools/gen_reels.py OUT.h
"""
import math
import random
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import gen_waves as gw  # noqa: E402
from sampleio import IMA_IDX, IMA_STEP, resample  # noqa: E402

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


# The melodic reels (BELL, DRON, FOLD) are little modular-synth patches, each frozen into one bar: an FM pair, a
# bank of detuned oscillators into a low-pass, a sine through a wavefolder into a low-pass gate. No samples: every
# sound is made here from sines and a saw. Every random choice comes from its own random.Random(seed), so the bytes
# are the same every run. They're all A minor pentatonic (A C D E G) and rooted on A, because that's what the
# factory scene (firmware/src/tpfactory.c) does with them: T2 pitches BELL's grains through a pentatonic phrase, T3
# rings A-rooted strings and random A minor grains off DRON, T4 plays half of FOLD and jumps its loop start, and the
# arpeggiator plays BELL and FOLD as single notes from their start. So they're raw material more than tunes: few
# notes, soft edges, and the movement in the timbre rather than in more notes.
#
# A bar is BAR samples: 2 s at 120 BPM rounded up to whole tape blocks (44,288 rather than 44,100: a reel is a
# whole number of blocks, so that's what loops), and the 16 steps divide it evenly. The tape's encoder starts each
# reel from silence (predictor 0, the smallest step), so the first sample has to be near zero or the decoder clicks
# while it catches up. BELL and FOLD start with a note's attack from zero and fade to nothing by the end of the bar
# (POLY plays them from the start as one note, so that first note has to be clean, with nothing wrapping round onto
# it). DRON is built on a circle instead (every oscillator does a whole number of cycles a bar, its filter runs two
# laps) and is then turned so the bar starts on a zero crossing: there's no seam at all.

BAR = -(-2 * SR // BLK) * BLK                # one 4/4 bar at 120 BPM, in whole blocks (173 of them)
STEP = BAR // 16                             # a 16th (BAR is 16 x 2768)
PENTA = (0, 3, 5, 7, 10)                     # A minor pentatonic, from A: A C D E G


def mtof(midi):
    return 440.0 * 2 ** ((midi - 69) / 12)


def penta(i, root=45):
    """the i-th note of A minor pentatonic up from root (MIDI; 45 is A2), i any integer"""
    return root + 12 * (i // 5) + PENTA[i % 5]


def euclid(k, n, rot=0):
    """k onsets spread as evenly as they go over n steps (a Euclidean rhythm), rotated by rot"""
    return [((i + rot) * k) % n < k for i in range(n)]


def lfo(i, phase=0.0, cycles=1):
    """0..1, a raised cosine that goes round `cycles` times a bar (so it loops with it)"""
    return 0.5 - 0.5 * math.cos(2 * math.pi * (cycles * i / BAR + phase))


def tail_fade(buf, ms):
    """the bar's last ms fade to nothing (raised cosine), so the loop comes back round to silence"""
    a = int(SR * ms / 1000)
    for j in range(a):
        buf[-1 - j] *= 0.5 - 0.5 * math.cos(math.pi * j / a)
    return buf


def svf_loop(x, cutoff, q=0.7, mode="low"):
    """a state-variable filter (the trapezoidal one: stable however fast cutoff moves) over a circular buffer,
    cutoff(i) in Hz read every 32 samples like the firmware's block. Two laps; the second one is kept, so it starts
    from where the first one ended."""
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


def echo(x, delay, fb, wet, damp=0.4):
    """a tape echo in a straight line (nothing wraps round): each repeat fb as loud and a little darker (a one-pole,
    damp), added at wet"""
    y, lp = [0.0] * len(x), 0.0
    for i in range(delay, len(x)):
        lp += (1 - damp) * ((x[i - delay] + y[i - delay]) * fb - lp)
        y[i] = lp
    return [a + b * wet / fb for a, b in zip(x, y)]


def fm_bell(f, n, vel, bright):
    """one FM bell: a sine carrier at f phase-modulated by a sine at 2f (a harmonic ratio, so the pitch is clear),
    the index falling from bright to mellow faster than the level; a second carrier 2.5 cents up for a slow shimmer;
    a short inharmonic glint at 3.53f (the glass) that's gone in 50 ms. A 6 ms attack: soft, but it still strikes."""
    w, out = 2 * math.pi * f / SR, []
    for j in range(n):
        t = j / SR
        env = min(1.0, j / (SR * 0.006)) * math.exp(-t / 0.33)
        idx = bright * (0.35 + 1.6 * math.exp(-t / 0.22))
        m = idx * math.sin(2 * w * j)
        v = math.sin(w * j + m) + 0.45 * math.sin(w * 1.00145 * j + 0.8 * m)
        v += 0.35 * bright * math.sin(3.53 * w * j) * math.exp(-t / 0.05)
        out.append(v * env * vel)
    return out


def bell_events(seed=11):
    """BELL's notes: [(step, midi, velocity, brightness)]. A3 on the one, then three more, never closer than three
    16ths; the fourth is picked between D4 and C4, the velocities wander a little"""
    rng = random.Random(seed)
    plan = [(0, 57, 1.0), (5, 64, 0.55), (9, 69, 0.42), (12, rng.choice((62, 60)), 0.36)]
    ev = []
    for step, midi, vel in plan:
        vel = vel if step == 0 else vel * rng.uniform(0.9, 1.1)
        ev.append((step, midi, vel, 1.0 - 0.55 * lfo(step * STEP)))   # the timbre: mellow mid-bar, bright at the ends
    return ev


def reel_bell():
    """FM bells on A minor pentatonic (one bar, 2 s): A3 struck on the one, then E4, A4 and a D4 or C4, softer and
    further apart than the beat (steps 0, 5, 9, 12), each ringing on under the next so they decay into each other.
    The FM index sets the timbre: the first and last notes are struck bright, the middle ones mellow, one slow
    change across the bar. It's a source for the pitched grains (T2) and for the arpeggiator (POLY plays it from
    the start), so the first note is A3 (MIDI 57) alone for 627 ms, and the bar is mostly A: a steady tone more
    than a tune. Each note falls by e in a third of a second, so the bar has room in it.
    The tail fades out over the bar's last 300 ms, so the loop comes back round to silence and the A3 again."""
    buf = [0.0] * BAR
    for step, midi, vel, bright in bell_events():
        at = step * STEP
        place(buf, at, fm_bell(mtof(midi), BAR - at, vel, bright))
    return tail_fade(buf, 300)


def saw_table(n=2048, harm=28):
    """one cycle of a band-limited saw (28 harmonics: up to 6 kHz at A3, under the reel's Nyquist)"""
    return [sum(math.sin(2 * math.pi * k * i / n) / k for k in range(1, harm + 1)) for i in range(n)]


def tri_table(n=2048, harm=15):
    return [sum((-1) ** (k // 2) * math.sin(2 * math.pi * k * i / n) / (k * k) for k in range(1, harm + 1, 2))
            for i in range(n)]


def osc(table, cycles, phase):
    """a wavetable oscillator doing exactly `cycles` cycles in a bar (so it loops), from phase 0..1"""
    n = len(table)
    out = []
    for i in range(BAR):
        p = ((cycles * i / BAR + phase) % 1.0) * n
        k = int(p)
        f = p - k
        out.append(table[k] * (1 - f) + table[(k + 1) % n] * f)
    return out


def reel_drone(seed=7):
    """a warm drone on A (one bar, 2 s, no rhythm at all): two saws a hair either side of A2 (they beat once a
    bar), a triangle on E3 (the fifth), a triangle and a quiet saw on A3, all into a resonant low-pass that opens
    from 250 Hz to 1.2 kHz and closes again once a bar, the level swelling a little with it. Over that a touch of
    air: noise through a band-pass that follows the same cycle. A bar is 2.0085 s, so a whole number of cycles a
    bar is a multiple of 0.4979 Hz: A2 is 221 of them (110.03 Hz), the saws 220 and 222, E3 331 (164.80 Hz), A3 442
    and 443. Everything is a loop and the filters run two laps, so the circle has no seam; then I turn it so the
    bar starts on the gentlest zero crossing there is (the decoder starts from silence and has to catch up)."""
    rng = random.Random(seed)
    saw, tri = saw_table(), tri_table()
    voices = [(saw, 220, 0.5), (saw, 222, 0.5), (tri, 331, 0.55), (tri, 442, 0.35), (saw, 443, 0.1)]
    buf = [0.0] * BAR
    for table, cycles, g in voices:
        for i, v in enumerate(osc(table, cycles, rng.random())):
            buf[i] += v * g
    buf = svf_loop(buf, lambda i: 250 * 2 ** (2.3 * lfo(i)), q=0.9)
    noise = svf_loop([rng.uniform(-1, 1) for _ in range(BAR)], lambda i: 1500 * 2 ** lfo(i), q=1.5, mode="band")
    buf = [(v + 0.06 * nz * (0.4 + 0.6 * lfo(i))) * (0.8 + 0.2 * lfo(i)) for i, (v, nz) in enumerate(zip(buf, noise))]
    n = len(buf)                             # the bar starts where the drone crosses zero most gently
    z = min((i for i in range(n) if (buf[i - 1] < 0) != (buf[i] < 0)),
            key=lambda i: max(abs(buf[(i + k) % n]) for k in range(-4, 12)))
    return buf[z:] + buf[:z]


def fold_pluck(f, vel, fold, n):
    """one West-coast pluck: a sine through a wavefolder (sin(drive x): more drive, more folds, more harmonics)
    into a low-pass gate, a vactrol-ish envelope that sets the level, the folder's drive and two one-pole
    low-passes all at once, so the note gets darker as it gets quieter. fold 0..1 is how hard it's folded."""
    w, out = 2 * math.pi * f / SR, []
    tau = 0.04 + 0.08 * vel                   # louder notes ring longer, as a real LPG does
    a = b = 0.0
    for j in range(n):
        t = j / SR
        env = min(1.0, j / (SR * 0.002)) * math.exp(-t / tau)
        drive = 1.0 + 4.5 * fold * env
        v = math.sin(drive * math.sin(w * j))
        c = 1 - math.exp(-2 * math.pi * (60 + 4500 * env * env) / SR)
        a += c * (v - a)
        b += c * (a - b)
        out.append(b * env * vel)
    return out


def fold_events(seed=23):
    """FOLD's notes: [(step, midi, velocity, fold)]. A Euclidean 5 of 16 from the one and two ghost notes where
    there's room (never on step 1: the first note stays alone); A2 on the one, the rest picked from A2..C4"""
    rng = random.Random(seed)
    gate = euclid(5, 16)
    hits = [i for i in range(16) if gate[i]]
    ghosts = rng.sample([i for i in (2, 6, 9, 15) if not gate[i] and not gate[i - 1]], 2)
    notes = [penta(i) for i in (0, 0, 1, 2, 3, 4, 5, 5, 6)]   # A2 C3 D3 E3 G3 A3 C4, A twice as likely
    ev = [(0, 45, 1.0, 0.8)]
    for i in hits[1:]:
        ev.append((i, rng.choice(notes), rng.uniform(0.5, 0.8), rng.uniform(0.3, 1.0)))
    for i in ghosts:
        ev.append((i, rng.choice(notes[2:]), rng.uniform(0.3, 0.42), rng.uniform(0.0, 0.3)))
    return sorted(ev)


def reel_fold():
    """wavefolded plucks through a low-pass gate (one bar, 2 s): a Euclidean 5 of 16 (steps 0, 4, 7, 10, 13) and
    two quiet ghost notes (steps 2 and 9 with this seed), notes from A minor pentatonic between A2 and C4, each with
    its own velocity and fold amount (the hard-folded ones buzz, the ghosts are nearly sines). The events are spread
    over the whole bar, so half a bar from anywhere is still a phrase, which is how T4 plays it. An echo a dotted
    8th behind, dark and quiet, fills the gaps a little. The first note is A2 (MIDI 45) on the one, folded fairly
    hard, alone for 251 ms (POLY plays the reel from its start as one note), and the bar's last 80 ms fade out, so
    nothing wraps round onto it."""
    buf = [0.0] * BAR
    for step, midi, vel, fold in fold_events():
        at = step * STEP
        place(buf, at, fold_pluck(mtof(midi), vel, fold, min(BAR - at, int(SR * 1.2))))
    return tail_fade(echo(buf, 3 * STEP, 0.3, 0.18, damp=0.55), 80)


REELS = [("BEAT", reel_beat), ("BELL", reel_bell), ("DRON", reel_drone), ("FOLD", reel_fold)]


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
