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

## Where it stands (2026-10-12, the same day)

Tier A and Tier B are in, each step a commit, each checked the same way: every audio fingerprint unchanged, every
screen unchanged, the modulation audit at 0, the target type check clean. The factory scene, measured by the
simulator with the firmware's attributes kept (the host harness defines `__attribute__` away, so the suite's own
load tables don't see the inlining; `always_inline` is what the target compiles):

| Step | Host instr/sample | Hardware divides/sample |
| --- | ---: | ---: |
| before A1 (`51aa4ac`'s parent chain, the shift forms in) | 3,384 | 7.5 |
| A1 the decoder | 3,265 | 7.5 |
| A2, A3 POLY's block run, the SVF inlined | 3,095 | 7.5 |
| A4, A5 the chain's buffers, the modulators' copy | 3,024 | 7.5 |
| A6-A9 the strings, the grain slots, the drums | 2,930 | 6.5 |
| B1 the modulators' divides, the tables | 2,919 | 3.7 |
| B2 the engines' divides | 2,917 | 1.9 |

13.8 % fewer instructions and 5.6 fewer hardware divides a sample (at 20-odd cycles each, another 3 % or so of the
FM-1 by run 1's scale), the sound bit for bit the same. Two things I decided not to do on the way, because they
weren't identical: skipping `rt->last` when nothing listens (a FOLLOW's first block after it's assigned would see
stale input) and skipping the buffer's block decode at FDBK 0 (a block the head leaves early keeps its old content,
and that's stored data). The device's reading of this build is the number that matters next; the simulator says
the I-cache misses didn't move (31-33 a sample at 16 KB), so the layout question stands as it was.

**Tier C and the two items outside the interrupt, later the same day.** The compressor's gain loop returns on a
block whose peak is 0 (0 times any gain is 0; its envelope still runs); `master_block` writes silence on an all-zero
block when its DC blockers hold 0, the limiter sits at its floor and the low-cut is off (every state as running it
would leave it); a RESONATOR with its input filter set skips the filter on silence while the filter's state is 0.
`mod_peak_dev` keeps the depths' part of a peak per track and target for the list it was summed on (`mod_gen`,
bumped by `mod_rebuild`), the knob's part read live: the polls had asked four times a track every pass, a scan and
three divides each. The 1/3-beat LFO fix: `fr` is the exact fraction of the division where the division isn't a
multiple of 256 (the old form reached 65789 and the phase wrapped 0.4 % early every cycle); a test holds the phase to
the exact fraction every block. The fingerprints held through all of it (no checkpoint run uses the 1/3-beat rate);
the scene is 2,922 (the two silence scans cost 5 a sample), and run 29, the transport stopped, is what Tier C pays
for: 743 to 618 on the suite's count.

## Order

A1-A3 first (the sources are the biggest third), then A4-A5 and the whole of Tier B as one pass (the same three tools
everywhere), then A6-A9, then C. D and the two items outside the interrupt as their own changes.

## Round 2 (2026-10-12, evening): one track heard at 70-75 %

From playing it: 70-75 % with only T2 playing, the rest muted. Five read-only audits of the whole path (sources,
GRAIN and RESONATOR, COLOR SPACE mixer master, the chain and the modulators, the platform), and timing on the host,
put it this way:

| Where | Share of that 70-75 % | How sure |
| --- | --- | --- |
| the muted tracks still rendering their sources and GRAIN, then zeroed | about a third | measured (host time) |
| code run from flash: `chain_block` alone is 23.7 KB against a 16 KB cache, refetched every block; the screen's code evicts it between | 10-30 points | inferred, not measured |
| SPACE and the master | ~5 % | estimated |
| GRAIN | ~5 % | estimated |
| T2's POLY | ~3 % | estimated |
| modulators, arpeggiators | 1-1.5 % | estimated |

The plan, agreed: optimize before thinning features.

1. **Muted tracks sleep** (done, `f12f571`): -42 % on the host for one track heard.
2. **Measure on the device first** (this change): SETUP's load lines (per stage, `chain.c` `ld_*`), a probe of the
   same loop from RAM and from flash at boot (`main.c` `ld_probe`), and the **TEST** project (past FACTORY: the
   scene with every stage at work, run 32 here) so readings compare build to build.
3. **The hot code in RAM** (decided after the device numbers): a `.hot` section high in RAM, within call reach of
   flash both ways (the 23-bit call: within 4 MB; RAMTEXT at 0x01C00000 is just out of reach of XIP, so it can't
   be there), with pool objects moved into RAMTEXT and the debug area to make room, and about 1.1 s of sound memory.
   The PC guard's RAM window widened through a new hal/ function. The risk is a boot that faults, which needs
   FM-1-transporter to recover: built and checked here, flashed only when asked.
4. **The bit-identical trims** from the audits (~6-9 %): the room loop split into per-stage passes, the master's
   state in locals, the delay's 16-bit path, divides ahead of early returns, TAPE's fast path, POLY's second loop,
   the drum loops, GRAIN's recording, mixing and decoding, RESONATOR's per-string caches, the modulators' divides.
5. **Thinning, only if needed**: voices ending at -60 dB, modulators every second block, a LO-LO room, and last of
   all 64-sample blocks.

Found on the way: the drum engine's 16 instruments are bit for bit what they were at `f6ab58e`; the clicks heard on
the device are the overload's shedding (a drum voice cut in 1.5 ms), not a change in the drums.
