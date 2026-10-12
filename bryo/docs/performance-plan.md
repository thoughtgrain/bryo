# Bryo: the performance plan (2026-10-12)

What I found when I went through every engine's inner loops and the signal flow, and the order I'm doing the fixes
in. The rule for all of it: **the output samples don't change.** Every item marked "identical" produces the same bits;
the audio fingerprints (`tests/bryo_audio.txt`, taken by `checkpoint_sim fp`) prove it on every run of the suite,
and anything that would move them is listed apart and not done without a decision.

Scale: the factory scene costs about 3,400 host instructions an output sample under callgrind, and 37.6 of those are
about 1 % of the FM-1 (by run 1's device reading). Hardware divides are counted apart: the FM-1 does every `/` and
`%` in hardware (20-odd cycles), even by a constant, even by a power of two for a signed value, and the host count
sees each as one instruction. `sdiv2()` (dsp.c) is the shift form that rounds the same way.

## Where the scene's time goes

| Part | Host/sample | What it is |
| --- | ---: | --- |
| ADPCM decode and read (8 POLY voices, DRON, the grains) | ~600 | per nibble a call with its state through memory, three branches, two clamps; per sample two bounds checks and a block lookup |
| RESONATOR, four strings | ~250 | two line reads and two wrap tests a string-sample |
| DRUM voices | ~330 | four passes over the metal cluster, a copy pass into the mix |
| the SVF (`tsvf_mode`) | ~200 | called per sample through pointers, its bypass test re-evaluated per sample |
| GRAIN | ~255 | two 40-slot scans a track a block; a whole-block decode when recording at FDBK 0 |
| the modulators | ~130 | 40 % of it copying knob arrays int16 by int16, the length re-evaluated each iteration |
| chain, mixer, output | ~460 | zero-then-add buffers, a byte-loop memset, `rt->last` stored for nobody, the output stage on silence |

## Tier A: the host-visible work (identical output)

| # | Where | Change | Saving |
| --- | --- | --- | ---: |
| A1 | `tape.c` `ima_step`, `ima_dec` (grains too) | inline, state in registers across the block, the delta without branches, the index step from a table, two nibbles a byte load | 150-180 |
| A2 | `tape.c` `tape_read`/`tape_at`, `poly.c` | once a block, the first and last index the voice touches; inside one decoded block, a tight loop over the cached samples | 130-150 |
| A3 | `dsp.c` `tsvf_mode`, `poly.c` | always_inline, state in locals, the "filter on?" test outside the loop | 90-120 |
| A4 | `chain.c`, `libc.c` | the first source or track assigns instead of zero-then-add; `rt->last` only when REC or a FOLLOW listens; word-wise memset/memcpy | 60-90 |
| A5 | `mod.c` the array copy | the length hoisted, 32-bit words | ~38 |
| A6 | `reso.c` the string loop | carry the previous read as the second tap, wrap-free runs, the input-gain pass folded | 30-40 |
| A7 | `grain.c` the slot scans | a per-track slot mask instead of walking all 40 slots twice a track a block | 25-30 |
| A8 | `grain.c` the buffer write | at FDBK 0 no decode of the block about to be overwritten, no `/100` | 25-30 |
| A9 | `drum_voice.c`, `drum.c` | the metal cluster in one pass; the voice adds into the mix in its last pass | 27-33 |

## Tier B: the hardware divides the host hides (identical output)

About 8-9 a sample in the scene, nearly all per block on values that don't move. Three tools do most of it: a
`DB_Q10[49]` table for `db_q10`, a `PCT_Q15[101]` table for `x * 32767 / 100`, and guards where a knob sits at its
identity value (AMT 100, SKEW 0, FOLD 0, SMTH 0, SWNG 0, ENV 0, FDBK 0, DUB 0).

| Where | Divides/sample | Fix |
| --- | ---: | --- |
| the modulators: SKEW and FOLD at 0, SEQ's beat maths, LFO SYNC, AMT 100, SMTH 0, `engine % NME` | 3.2 | guards; shifts (every SEQ division and the scene's LFO rates are powers of two); `& 3u` |
| POLY per voice per block: the pitch step, RES, ENV, the filter coefficients | 1.25 | cache per voice and per track; guard ENV 0 |
| `db_q10` (every rendered source, COLOR, LOW and HIGH) | 0.3-1.25 | the table |
| GRAIN: `gr_cdist`'s `% len`, the seam, the envelope ramp, `gr_plan`, knob maths | ~1.2 | fold loops, shifts, a reciprocal at grain start, the table |
| RESONATOR per block: TONE, DEC, WET, each string's period and gain | 0.8 | sdiv2, hoist, memoize by knob and root |
| SPACE and COLOR: knob maths computed before their early returns | 1.0-1.3 | below the returns; cache per knob |
| the compressor, `src % NSRC`, the limiter's divide a sample while limiting | ~0.3 | caches, `& 3u`; the limiter: a Newton-updated reciprocal (near-identical: see below) |

## Tier C: resting on silence (identical output; nothing for the scene, ~130 a sample when nothing plays)

The output stage and the compressor's gain loop on all-zero blocks once their states are settled; a RESONATOR with
its input filter set, filtering silence while it rests.

## Tier D: spikes and layout

- `rs_pluck` puts 20-75k instructions into one block on every RES ARP step: at 75 % that block can pass 100 %.
  Spreading the burst over the following blocks is near-identical, not identical.
- `gr_pitch` with SCAL on: 146 divides in one block at every grain start; a running index.
- The hot-code layout (the cache simulation of 2026-10-12): only a device reading can confirm it.

## Not identical: listed, not done

- The `softclip` linear short-circuit (≤ 1 LSB below -30 dBFS; nothing to gain in the scene).
- Folding gains into one multiply where a clamp sits between two (mixer's pan and level, grains' WET).
- Nearest rounding in the rooms (a limit cycle: already decided against).
- The limiter's reciprocal (≤ 1 LSB on the release slope): Tier B lists it; it waits for a decision.
- A decoded-block cache shared by POLY voices: identical, but the voices rarely share a block, so it's not worth
  its memory.

## Outside the audio interrupt

- `mod_peak` costs the main loop about 60 a sample: four calls a track a poll, each with three divides, for a
  value that only changes with a knob or a depth. Compute it in `mod_rebuild`.
- The LFO's 1/3-beat division overflows its phase at each cycle's end (`mod.c`, `21845`): a 0.4 % phase glitch.
  A bug, fixed as a separate change with its own test.

## How each step is checked

1. `tests/run_tests.sh`: the suite, the screens' goldens, and the audio fingerprints of every checkpoint run and the
   factory scene (`tests/bryo_audio.txt`) unchanged.
2. The target type check, and `sh tests/mod_audit.sh`: 0 targets stepping.
3. The simulator's count of hardware divides a sample and `checkpoint_sim.sh`'s load tables.

## Order

A1-A3 first (the sources are the biggest third), then A4-A5 and the whole of Tier B as one pass (the same three tools
everywhere), then A6-A9, then C. D and the two items outside the interrupt as their own changes.
