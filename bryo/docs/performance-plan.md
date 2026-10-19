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
| RIPPLE, four strings | ~250 | two line reads and two wrap tests a string-sample |
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
| RIPPLE per block: TONE, DEC, WET, each string's period and gain | 0.8 | sdiv2, hoist, memoize by knob and root |
| CHRONOS and COLOR: knob maths computed before their early returns | 1.0-1.3 | below the returns; cache per knob |
| the compressor, `src % NSRC`, the limiter's divide a sample while limiting | ~0.3 | caches, `& 3u`; the limiter: a Newton-updated reciprocal (near-identical: see below) |

## Tier C: resting on silence (identical output; nothing for the scene, ~130 a sample when nothing plays)

The output stage and the compressor's gain loop on all-zero blocks once their states are settled; a RIPPLE with
its input filter set, filtering silence while it rests.

## Tier D: spikes and layout

- `rs_pluck` puts 20-75k instructions into one block on every RIPL ARP step: at 75 % that block can pass 100 %.
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
would leave it); a RIPPLE with its input filter set skips the filter on silence while the filter's state is 0.
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
GRAIN and RIPPLE, COLOR CHRONOS mixer master, the chain and the modulators, the platform), and timing on the host,
put it this way:

| Where | Share of that 70-75 % | How sure |
| --- | --- | --- |
| the muted tracks still rendering their sources and GRAIN, then zeroed | about a third | measured (host time) |
| code run from flash: `chain_block` alone is 23.7 KB against a 16 KB cache, refetched every block; the screen's code evicts it between | 10-30 points | inferred, not measured |
| CHRONOS and the master | ~5 % | estimated |
| GRAIN | ~5 % | estimated |
| T2's POLY | ~3 % | estimated |
| modulators, arpeggiators | 1-1.5 % | estimated |

The plan, agreed: optimize before thinning features.

1. **Muted tracks sleep** (done, `f12f571`): -42 % on the host for one track heard.
2. **Measure on the device first** (this change): SETUP's load lines (per stage, `chain.c` `ld_*`), a probe of the
   same loop from RAM and from flash at boot (`main.c` `ld_probe`), and the **TEST** project (past FACTORY: the
   scene with T1's drums through GRAIN, run 32 here) so readings compare build to build.
3. **The hot code in RAM** (decided after the device numbers): a `.hot` section high in RAM, within call reach of
   flash both ways (the 23-bit call: within 4 MB; RAMTEXT at 0x01C00000 is just out of reach of XIP, so it can't
   be there), with pool objects moved into RAMTEXT and the debug area to make room, and about 1.1 s of sound memory.
   The PC guard's RAM window widened through a new hal/ function. The risk is a boot that faults, which needs
   FM-1-transporter to recover: built and checked here, flashed only when asked.
4. **The bit-identical trims** from the audits (~6-9 %): the room loop split into per-stage passes, the master's
   state in locals, the delay's 16-bit path, divides ahead of early returns, TAPE's fast path, POLY's second loop,
   the drum loops, GRAIN's recording, mixing and decoding, RIPPLE's per-string caches, the modulators' divides.
5. **Thinning, only if needed**: voices ending at -60 dB, modulators every second block, a LO-LO room, and last of
   all 64-sample blocks.

Found on the way: the drum engine's 16 instruments are bit for bit what they were at `f6ab58e`; the clicks heard on
the device are the overload's shedding (a drum voice cut in 1.5 ms), not a change in the drums.

**The first device readings (8da385f, TEST as first built):** R/F 239/239, so a loop runs from flash exactly as fast
as from RAM once it's in the cache: flash costs only on misses, which this loop can't show. TEST read SRC 37, GRN 21,
RES 10, COL 8 % and was over 100 % (noise: the shedding). The host's shares of TEST were SRC 28, GRN 20, RES 19, COL
8, SPC 11, ROOM 5, MST 3, PRE 5 %: no stage costs the FM-1 far more than its instructions say (RIPPLE's 10 is
likely its strings shed), the sources a little more. TEST cut down to FACTORY plus T1's drums through GRAIN (1.06 x
FACTORY on the host, where the first was 1.3 x), and the load lines drawn bright.

**The second reading (fd6d40c, TEST as it is now): 90 % CPU.** SRC 34, GRN 19, RES 7, COL 1, SPC 10, PRE 16, ROOM 8,
MST 3 (98 in all: the stages count the nested TIMER5, the meter takes it out); R/F 239/239. The host's shares of the
same TEST: PRE 6, SRC 34, GRN 25, RES 12, COL 2, SPC 10, ROOM 7, MST 3. Every stage costs the FM-1 what its
instructions say, except PRE, ten points over: the once-a-block work (the modulators, the arpeggiators, the grains'
plan). Code run once a block misses the cache every block, where a per-sample loop pays its misses once and runs 32
samples from the cache, and the scene's hardware divides (about 2 a sample) can't make ten points. The target, from
playing it: 65-70 % with a lot going on.

**Step 1 of the code in RAM (this change): the once-a-block work.** `HOT_CODE` (bryo.h) puts a function in `.hot`
(app.ld): 16 KiB at 0x01C70000, the pool's top, high enough that a call reaches every part of .text both ways. The
pool gave it without sound memory: `pol_rd` and `tape_rt` (14.7 KB) moved to `.lowpool` in RAMTEXT, after
`.ram_text`. The startup copies `.hot` like `.ram_text` and widens the PC guard's RAM window over both
(`fm1_guard_pc_ram`, hal/). In it: `mod_tick` and the modulators' engines and helpers, `arp_run` and the
arpeggiators' helpers, `gr_plan` and its helpers: 6 KB. What it still calls in flash is small and shared (`clamp`,
`memcpy`, `sine_i`, `isqrt`). Every audio fingerprint unchanged. If the theory holds, PRE goes from 16 toward 6 on
TEST; if it does, the per-block parts of the other stages are next.

**The third reading (c9bea3f): 60-70 % on TEST, from 90.** SRC 24, GRN 17, RES 6, COL 1, SPC 10, PRE 8, ROOM 7, MST 2.
PRE halved as expected, and SRC fell ten points with nothing of the sources moved: with the once-a-block code out of
the cache's way, the sources' loops stayed in it. The cache is the cost.

**Step 2 of the code in RAM (this change): the whole audio path.** `.hot` grows to 40 KiB at 0x01C6A000 (still in
reach of all of .text), RAMTEXT takes in the old debug area up to the boot probe (0x01C07F00), and `.lowpool` takes four
more pool objects (`drm_rt`, `mod_l`, `mrt`, `mda`); seven chunks of sound memory go too (143, 26.6 s, from 150 and
27.9). In `.hot`: `chain_block`, every engine's block and its per-sample routines (POLY's voice, TAPE's block and
reads, the drum voices and their setup, GRAIN's block, decode, mix, recording and planning, RIPPLE, COLOR, CHRONOS's
delay and rooms, the mixer, the master): 38.6 KB. What stays in flash is cold (REC's writes, USB's record mode, a
slice's jump, a sleeping track) or tiny and shared. One lesson: the compiler won't inline a function from one
section into another, so moving a caller made its small helpers calls; the per-sample ones (`clamp`, `sdiv2`,
`softclip`, the oscillators, `mem_at`, the CHRONOS and GRAIN helpers) are `always_inline` now, as they were inlined
before. Every audio fingerprint unchanged; two tests that counted chunks now count from `MEM_NC`.

**The fourth reading (7c22901): 65 % steady on TEST.** SRC 23, GRN 16, RES 5, COL 0, SPC 12, PRE 7, ROOM 6, MST 2:
the whole path in RAM gave four points more and a steady meter.

**Room for features (2026-10-13).** RIPPLE's new code went into `.hot` as written and filled it (40.0 of 40 KiB).
From playing it, 20 s of sound shared by the four tracks is enough, so the memory went from 143 chunks to 108 (20.1 s):
main RAM's 24 chunks are gone (its .data+.bss from 89.9 KB to 38.9 of 96: about 57 KB free for features) and the pool
gave 11 more, so `.hot` is 56 KiB at 0x01C66000 (39.7 used), with 16 KB of pool headroom. Run 15 (every device on all
four tracks) sounds a little different: with less memory one of its lines gets less.

**SYNTH's voice and the audio's tables in RAM (2026-10-18).** Since the 56 KiB area, everything new went into
`.hot` as written (RIPPLE FLOW, the p-locks, SPRD on the channel strip and COLOR, TWIN, the drums' pending
coefficients, HOLD), and SYNTH was never moved at all: `syn_block` and `syn_voice`, the biggest per-sample routine
there is, ran from flash on every scene with a SYNTH track (T2 in all three factory scenes), half the size of the
cache and fighting the screen's code for it. And the constant tables the RAM code reads (the sine, the pitch, the
envelopes' and filters' curves, ADPCM's steps, RIPPLE's partials, the drum kit) were still in flash: per-block reads
miss the cache the same way per-block code did.

I can't build for the pi32v2 here, so I measured the way the old numbers were: the unity build preprocessed, its
inline assembly taken out, `clang --target=i386 -Os`, `size -A`. At `f4a6bc9` that measure read `.hot` 64.5 KB where
the device's build printed 39.7 KiB (0.63 of it); today it read 84.2 KB, about 53 KiB on the device: nearly full.

- `.hot` grows to 72 KiB at 0x01C62000; the pool gives the 16 KiB (`app.ld`, `build.py`'s POOL_LEN and HOT_LEN).
  The reach still holds with room: a call only goes to code, which is the start of `.text` (its constants come
  after), so `.hot` could start well below this and still reach all of it. A wrong guess fails the link, not the boot.
- The pool's grain slots (`gslot`, 13 KB) move to main RAM's `.bss`, which had about 30 KB spare. They were in the
  pool only because main RAM was full of sound chunks when GRAIN was built.
- `syn_block`, `syn_voice`, `syn_start`, `syn_chord_voice`, `syn_note16` are `HOT_CODE`: 13.5 KB on the i386 measure,
  about 8.5 KiB on the device, and nothing they call is left in flash (their helpers were inlined already).
- `HOT_TABLE` (defined in the generated `felucca_tables.h`, `tools/gen_tables.py`'s `HOT` set) puts a constant table
  in `.data`, main RAM, copied there at startup like any initialised data: 22 tables, 8.8 KB, every table of 64 bytes
  or more the RAM code reads (found from `.hot`'s relocations on a `-fdata-sections` build). `REELS` stays in flash:
  it holds pointers, and in a section with the others it could conflict on an older compiler.

After it, on the i386 measure: main RAM .data+.bss 87.3 of 96 KB, the pool 257.7 of 264 KiB (12.6 KB headroom, the
build wants 8), `.hot` 97.7 KB (about 61.5 of 72 KiB on the device). Every audio fingerprint is unchanged: it's
the same code in another place. What it buys can only be read on the FM-1: TEST's CPU, SRC's line most of all, and
the factory scene's. If the theory holds, SRC drops a few points where a SYNTH track plays.

**WARBLE and VINYL (2026-10-18).** GRAIN's two new forms (`grainfx.c`) are `HOT_CODE` from the start: they run every
sample on FLOW. `gfx_flow` is about 100 host instructions a sample in its loop and 165 with its block's setup, a
track: the line's write and interpolated read (about 31), AGE's filters and curve (25), the surface (12 on VINYL). I
tried keeping the filters', the surface's and the line's state in locals for the block, so the line's stores can't
make the compiler reload them; at -O2 on the host it changed nothing (they were in registers already), but I kept it,
since the target's compiler is older and gets less from alias analysis. Run 33 (VINYL and WARBLE on the factory
scene's sound) is 3,328 against the scene's 2,998, run 34 (WARBLE on T3's grains) 3,093. `.hot` grows about 3.8 KiB
on the device (about 65.8 of 72), `.bss` 1.8 KB. The glitch reader costs nothing between glitches (it returns before
its loop); during one it decodes GRAIN's ADPCM buffer, a grain's worth. If the FM-1 reads high on run 33, the line's
interpolation and AGE's curve (which only runs while AGE or WEAR is above 0) are the places to look.

**The hearing floor (2026-10-19).** Not a speed-up of any routine: work that made nothing anyone could hear,
stopped. Every part already ended its own sound somewhere (a voice's envelope at -72 dB under its peak, a drum's at
-60, the strings and the delay on exact silence), but always against the sound itself, never against what reaches
the output. `hear.c` sets one floor, -72 dBFS at the output, and each part ends or rests under it in its own terms
(bryo-architecture.md, "The hearing floor"). At MASTER full and 0 dB nothing changes; at the boot's defaults (a
track at -6 dB, MASTER at -6) tails end 12 dB sooner under their peaks. On the factory scene that's 2,998 to 2,796,
6.7 %, because AMBIENT is long releases in a hall. Elsewhere the bookkeeping costs 4 to 16 a sample; I took most of
it out by working the floors out only when an input moves (the first version cost up to 26) and by measuring an
input's level only when the floor is over 0 for it, on every fourth or second sample. Under load the floor rises
before anything is capped, so the first thing shed is what can't be heard.

**Silence shares one chunk (2026-10-19).** The memory side: a RAM tape's all-zero chunks point at one const chunk in
flash and go back to the pool (bryo-architecture.md, "Silence shares one chunk"). `mem_at` gained a compare; the
checkpoint runs can't tell (5,288 to 5,284 on run 15, 3,069 to 3,052 on run 23: noise and inlining). `.hot` grew
about 2.4 KB on the i386 measure for both changes together (about 1.5 KiB on the device: 67.3 of 72); `.bss` 136 B;
the shared chunk is in `.rodata`, flash, so it costs no RAM. GRAIN's buffers stay out of it: lending their silence
lost sound when the memory was full.

**Shedding without clicks (2026-10-19).** The follow-up to the hearing floor: what it drops must go gently. A DRUM
part under the floor now fades at a 3 ms time constant instead of stopping, and a RIPPLE string over a lowered cap
fades over 12 ms before it rests; both measured clean against the sound's own sharpest corner. The voices live a few
milliseconds longer: 3 to 11 a sample on the DRUM runs, 22 on the factory scene (2,818, against 2,998 before the
floor). A grain cut takes at most about half of any track's grains.

**The load guard (2026-10-19).** Shedding used to wait for two halves over 85 % and take a step a half; a sudden jump
could miss a deadline before it caught up. Now it starts at 78 %, takes two steps over 88 %, and checks inside each
half after every block, shedding four steps at once with short fades when the render is heading past 92 % of its time
(bryo-architecture.md, "The load guard"). It costs a tick read and a divide a block. On the FM-1 the measure that
matters is `audio_late` staying at 0 under check 35 while `load_rush` climbs.
