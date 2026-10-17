# Hardware checkpoint: memory by usage, GRAIN's buffer and the full chain (2026-10-08)

Everything since phase 2 has only run on the host, and the whole chain (GRAIN, RIPPLE, COLOR, CHRONOS) is built now.
I want to know three things only the FM-1 can tell me: whether the audio interrupt keeps up with it all on, whether the
shared memory behaves with real timing (USB packets and screen frames cutting in), and whether anything sounds wrong
that the host tests can't hear. This page is the run sheet. It takes about 30 minutes.

## On the host first: `tests/checkpoint_sim.sh`

There's no emulator for the FM-1's pi32v2 core (it's JieLi's own instruction set; QEMU doesn't know it), so the
device's real load can only come from the device. What I can do on the host is run the firmware's own sources
through this run sheet, with the hardware stubbed as the tests do (`tests/checkpoint_sim.c`):

- **Every run rendered to a WAV** (`build/checkpoint/runN.wav`) to listen to before touching the device.
- **Each run's cost** in host instructions per output sample (callgrind): a ratio against what Felucca's engines cost,
  not the FM-1's percentage.
- **The interrupts for real.** The audio interrupt and TIMER5's `usb_poll` fire as asynchronous signals that cut into
  the main loop at any instruction, as on the device (TIMER5 outranks the audio; `usb_poll` never runs nested in it),
  faster than the device does, while the main loop turns knobs, changes TRACKS, records, freezes, draws, and a WAV
  after WAV arrives over the drive. Every so often it stops both and checks the shared memory's books: each chunk
  owned once, listed where its owner says, none lost. I checked the check: with the WAV capture allocating from the
  interrupt again (the bug the review round found), it fails on every try within seconds.

What it gives now, after COLOR and CHRONOS (host instructions per output sample, the whole chain, four tracks; the
first six runs cost about 40 more than at the first checkpoint, the two new devices' calls while they're off):

| Run | What | Host cost | What the firmware reported |
| --- | --- | ---: | --- |
| 1 | four reels, GRAIN off | 902 | |
| 2 | WET 100, the defaults (each buffer recording) | 1,576 | 4 grains sounding, 44 chunks of buffers |
| 3 | dense grains on all four (the pool: 16) | 2,318 | 16 grains (4 4 4 4) |
| 4 | the same, all backwards | 2,698 | 16 grains |
| 5 | the same, SCAN TAPE | 1,960 | 16 grains |
| 6 | dense, TRACKS 2 (the pool: 24) | 2,243 | 24 grains (12 12 0 0) |
| 7 | a blank tape recorded 12 s, then looped | 1,005 | the tape 12.00 s long, 65 chunks |
| 8 | the freeze tapped on and off | 1,020 | frozen at 3 s, let go at 9 s |
| 9 | SYNTH played, transport stopped, GRAIN on | 659 | grains sounding after the phrase |
| 10 | a 20 s WAV over TAPE3.WAV while playing | - | the tape 20.00 s, TRACK 3'S TAPE REPLACED |
| 11 | RIPPLE WET 60 on all four | 1,847 | 16 strings |
| 12 | RIPPLE and GRAIN dense | 3,248 | 16 strings, 16 grains |
| 13 | COLOR on all four (DRV 60, CRSH 40 BOTH, NOIS 30, TILT 30) | 1,630 | |
| 14 | CHRONOS on all four (DLY 50, VERB 40) | 1,784 | 72 chunks, all 16-bit, 14.5 s free |
| 15 | everything on all four | 4,859 | 16 grains, 16 strings, 72 chunks of CHRONOS, 3.3 s free |
| 16 | TRACKS 3, GRAIN WET 100, DRV 40, CHRONOS | 2,310 | 54 chunks of CHRONOS, 11.7 s free |
| 17 | the mixer: LOW HIGH FILT PAN on all four, the compressor at AMT 60 | 1,701 | 5.7 dB taken off |
| 23 | GRAIN dense on one track alone (the pool: 32), the others plain reels | 2,976 | 31 grains |

Runs 3 to 6, 12 and 15 cost less than they did (3,304, 4,063, 2,929, 2,736, 4,228, 5,842) since GRAIN's grains became
one pool that shrinks as more tracks use it (32 for one track, 24 for two, 16 for three or four, shared by what each
asks: docs/bryo-architecture.md, "GRAIN, as built"). Run 23 is the case that grew: one track can now sound 32.

Runs 3 to 17 test each part at its limit. How the instrument will actually be played is a different number, so
runs 18 to 22 are realistic setups (each played for 6 s, a synth track's keys going):

| Run | Setup | Host cost | Against nothing on |
| --- | --- | ---: | ---: |
| 18 | three tracks as I'd play them: a played synth through GRAIN and CHRONOS, a driven reel with a little EQ, a reel through RIPPLE, the compressor | 1,653 | 1.8x |
| 19 | a four-track groove: crushed drums, a grain track, a filtered synth bass, a reverb, two tracks panned, the compressor | 1,584 | 1.7x |
| 20 | an ambient pad on two tracks: a synth stretched by dense grains into a long reverb, a reel through the strings | 2,077 | 2.2x |
| 21 | the groove's busiest moment: denser grains, strings, a delay, every channel filtered | 2,439 | 2.6x |
| 22 | the groove, bounced onto track 4 as it plays | 1,756 | 1.9x |
| 24 | the groove, modulated: an LFO a beat on the crush, a SEQ on grain size, T1's drums ducking T2's filter (FOLLOW), an ADSR on the bass's cutoff, a stereo LFO on T4's pan | 1,812 | 1.9x |
| 25 | the groove with every slot on every track, 32 depths each (the most there can be) | 3,161 | 3.3x |
| 26 | DRUM busy alone: FUNK at VARY 100, FILL 100, EVOL 1, long decays; the others plain reels | 1,308 | 1.4x |
| 27 | the groove with track 1 a DRUM track (BOSA, SEED 9, VARY 50) in place of its drum reel | 1,898 | 2.0x |
| 28 | four DRUM tracks, busy rhythms at VARY 100 with long decays: every voice there is (20) | 2,482 | 2.6x |

Run 24 is run 19 (now 1,595: every run costs about 5 more since the modulation's clock ticks whether or not anything
is modulated) plus the modulation I'd reach for: 84 of the 195 extra are the modulators, the rest the devices
re-tuning as their knobs move. Run 25 is the ceiling, not a scene (docs/bryo-architecture.md, "Modulation, as
built").

The everyday setups sit under the 2,000 target, a dense pad at it, a busy build-up 20 % over; the all-on case
(run 15, 6.3x) isn't one anybody plays. GRAIN's density is what moves the number most. One reading from the device
(the CPU with four reels playing, run 1) turns these ratios into percentages.

These are measurements, not listening: the WAVs are there to be heard.
No sample past full scale and no sudden jump (over 12,000 between neighbouring samples) in any of them. The stress:
over 20 s, 131,965 audio blocks and 86,110 USB sectors cut into the main loop while RIPPLE's and CHRONOS's knobs
took memory and gave it back too, and the books balanced at every one of 9,839 checks.

What that leaves for the device: the load. Run 4 costs 4.5 times run 1 on the host, and run 15, everything on all
four, 6.5 times; on the FM-1 those ratios are the question. And how GRAIN's level sits against the dry sound is worth a listen: at WET 100 the grains peak around a
third of the reels.

## Build and install

1. `tools/get_toolchain.sh` once, if `~/.jieli/toolchain` isn't there, and the AC79 SDK in `~/fw-AC79_AIoT_SDK`
   (BUILDING.md has the details).
2. `./build.sh`. Its last lines print the image size, the RAM and the pool. Note them: I expect the pool at about
   312 KB of 336 and RAM at about 80 KB of 96. The build refuses if the pool's 8 KB of headroom is gone.
3. Install `build/bryo.fwsc` from the web installer, as always. The update loader is untouched, so going back to
   stock works the same way it did.

## Where to read the load

Hold GLO: the mixer shows **CPU** in its bottom-right corner. It's the audio interrupt's load in 5 % steps, and it
turns bright at 85 %, where the firmware starts shedding grains. That's the number I need. (The memory picture
above it shows each track's seconds, the GRAIN buffers and the time free.)

## The runs

Write down the CPU reading for each, and anything you hear.

| # | Set up | What I expect | Write down |
| --- | --- | --- | --- |
| 1 | Power on, PLAY. Four factory reels, GRAIN off | the baseline | CPU |
| 2 | Each track: EDIT for GRAIN, WET 100 (KNOB 1 on GRAIN 2), the rest default | each track records its last bar and grains it | CPU; any click when WET goes up |
| 3 | Each track: RATE 100, SIZE 500 (GRAIN page 1) | 4 grains a track (four tracks share 16) | CPU; does it crackle, does it settle? |
| 4 | Same, REV 100 (GRAIN 2) | every grain backwards: the dearest read | CPU |
| 5 | Same, SCAN TAPE (GRAIN 3, KNOB 1 all the way left) | no buffer to record | CPU |
| 6 | Hold GLO, turn SELECT down to TRACKS 2 | tracks 3 and 4 fade out; 1 and 2 share 24, 12 each | CPU; a click as they go? |
| 7 | TRACKS 4 again, back to the defaults (or power-cycle). Track 2 on a blank tape: hold the POLY key to clear it, REC, let it record 10 to 15 s, REC again | the tape grows while you record and loops what you recorded | does the loop length match? any gap or click at the loop's seam? |
| 8 | Tap the 0 key while track 1's GRAIN plays (WET 100) | the buffer freezes into a 1-bar loop, the 0 key's LED lights; tap again to let go | does it loop in time? |
| 9 | Track 1 to SYNTH (hold HOME, turn SELECT), GRAIN WET 100, transport stopped, play a few notes | what you play is granulated without PLAY | does it sound? |
| 10 | Plug into a computer, copy a 20 s WAV onto the drive as TAPE3.WAV, eject, replug | track 3's tape is the whole 20 s (memory allowing) | the length on TAPE's page; any audio dropout while it copied |
| 11 | Power-cycle. Each track: RIPPLE WET 60 (FX until RIPPLE, KNOB 4) | four strings ring through each track | CPU |
| 12 | Same, plus each track's GRAIN at the cap (run 3's settings) | the strings and the grains together | CPU; crackle? |
| 13 | Power-cycle. Each track's COLOR: DRV 60, CRSH 40, NOIS 30, TILT 30 (FX, KNOB 1-4); CMOD BOTH (COLOR 2) | every track driven, crushed and noisy | CPU |
| 14 | Power-cycle. Each track's CHRONOS 2: DLY 50, VERB 40 | echoes and a room on every track | CPU; does the mixer's memory ribbon show it? |
| 15 | Runs 12, 13 and 14 together (everything on) | the hardest case there is | CPU; does it crackle, does shedding settle it? |
| 15b | TRACKS 4 again, GRAIN off on tracks 2-4 (WET 0), track 1 still at RATE 100, SIZE 500 | one track with the whole pool: "40 OF 40 GRAINS" on GRAIN's page | CPU |
| 16 | Plugged into a computer: is there an output named Bryo? Pick it, play something; on the FM-1, stopped, hold REC a second | the USB record mode, the level bar moving, the sound in your headphones | did the computer switch its output to Bryo by itself when plugged in? any clicks in what you hear? |
| 17 | REC, 10 s of something you know, REC, white key 2, REC | track 2's tape is the take; PLAY plays it | the length on TAPE's page; does it sound like what the computer played (mono, 22 kHz)? |
| 18 | Record the FM-1 on the computer (Bryo as its input) while it plays | the master output arrives (Felucca's, untested with Bryo) | any dropouts? |
| 19 | GLO, EDIT: each track's LOW +6, FILT -40, PAN apart; EDIT again: MASTER, AMT 60 | the channels shaped and placed, the mix glued, the meter moving | CPU; does the compressor pump? |
| 20 | Power-cycle, PLAY. Track 1 on COLOR: hold LFO, turn KNOB 2 (CRSH) to +40 %, let go. Tap LFO, LFO 4: SYNC BPM. Hold ENV on GRAIN 2, KNOB 1 (WET) +80 %; play a white key | the crush breathing in time; grains swelling in on each key and on each loop | CPU; does the crush step audibly (zipper), or glide? |
| 21 | On the mixer's channel page, hold LFO and turn KNOB 4 (PAN) to +60 %; tap LFO, LFO 3: SPRD 100 | the track moving across the stereo field, left and right apart | does it sound wide, or just wobbly? |
| 22 | Power-cycle. Track 1 to DRUM (hold HOME, turn SELECT), PLAY. Tap each white key; then HOME (VARY), SEED 10, FILL 80 | ROCK; each instrument on its key; the beat varying, a fill every other loop or so | CPU; how the kit sounds on the speaker (the bass drum, the hats' brightness); do the steps sit in time? |
| 23 | Tracks 2 to 4 to DRUM too, each its own PATN (run 28's case) | four drum machines at once | CPU at the most voices there can be |
| 24 | Record a take on track 1 (its own tape), change some knobs, tap SAVE; power-cycle | "P1 SAVED"; after the power cycle the same session, track 1 playing user reel P1T1; the save with the take: one soft gap (a fade out, a fade in), no stutter; tap SAVE again while it plays, nothing new recorded: no gap at all | how long the gap is; any click at either end; is everything back? |
| 25 | PRESETS: turn to P2, SAVE (empty: saved at once); change the tempo; turn back to P1, OCT+, OCT+ again | P1 loaded after the second press, the tempo it had | does the second-press question read clearly? |
| 26 | PRESETS, EDIT (the PRESETS page), turn past H to the factory ones; for each, focus a track of its source (TAPE on a reel, a SYNTH, POLY, DRUM track), OCT+, play it; SAVE held after one | each loads with no question and sounds like its name; none much louder or quieter than the last; SAVE held puts the track back | by ear: which to retune (they were level-matched on the host, not listened to) |
| 27 | On a track: SCL held, turn a knob; SCL held, press the lit pad; SAVE held. Then save the track as preset A, power-cycle, load A into another track | the knob, then the page, to defaults; SAVE held brings them back; A survives the power cycle and loads | that SCL is held (not a tap) |
| 28 | Track 1: GRAIN WET 80, RATE 60, SIZE 300, PLAY; GRAIN 6, GRNS 12. Then track 2 a SYNTH with GRAIN WET 80, play chords; then track 2 GRNS 30 | track 1 reads "n OF 12 HELD" and its cloud stays as thick when track 2 comes in; track 2 holding 30 of the pool of 30: track 1 keeps its 12, track 2 gets the other 18 | does a held cloud sound steady while the others change? CPU with both dense |
| 29 | Load a busy project, PLAY a while, then stop and wait 5 s; then PLAY, and mute three tracks, leaving one with grains, RIPPLE (RIPL ARP on) and a room | stopped: CPU well under the 50 % it read, no hiss even with COLOR's NOIS up; muted: CPU lower than with them heard, no crackle; unmuting brings them straight back | the CPU numbers for both; any click as a device wakes |
| 30 | Walk the pages: EDIT, FX, LFO, SEQ, ARP; PLAY with GRAIN, RIPPLE and CHRONOS up; arm REC; change a knob | blue page names on the blue pads' pages, purple on the modulators'; grains, ringing strings, the room's tail, the step playing in orange; the tempo bar red while REC is armed; a yellow ✱ after the change | do the hues read as intended on the panel (RGB565, its gamma), is the orange lively without being loud, can the yellow ✱ be seen at a glance; anything that flickers |
| 31 | Power-cycle (the factory scene), PLAY. Then track 1 (DRUM): an LFO on one drum's level (on the KIT page hold the drum's white key, then LFO, KNOB 3 (LVL) to +40 %, let go). Then track 3, FX until CHRONOS 3 (room B's): QUAL to HI, then back to LO | the scene and the depth together without crackle, near 75 %; room B at HI about 2 % more and a little brighter | the CPU for the scene, with the depth, with room B at HI; does LO's tail sound dull or grainy? |
| 32 | Track 2 on a cleared tape: hold REC and turn SELECT to 2 BARS, let go, tap REC, PLAY. Then turn the tempo from 120 to 100 and back | the take starts on the downbeat and stops by itself after 2 bars, then loops in time with the drums; TAPE 4 shows SYNC 2BR; at 100 BPM it slows (and drops in pitch) and stays on the beat | does the take start and end clean (no click at the seam)? does it stay in time after a minute? CPU |

## What the FM-1 read

First time on the device: 2026-10-10, installed from stock V15 (FM-1_015) with `tools/fm1_install.py`. The
prediction scales each run's host cost (`checkpoint_sim run N` under callgrind, measured the same day in a linux/amd64
container) by run 1's reading, 25 % for 940: about 37.6 a sample per percent. If the readings track it, the host's
count is a fair proxy for the device; where they don't, the run's DSP costs the device more (or less) than the host.
Run 4's host cost is missing: valgrind crashes on it under emulation, though the run itself is clean (natively, and
under the address and undefined-behaviour sanitizers).

| # | Host cost | Predicted | Read | Heard |
| --- | ---: | ---: | ---: | --- |
| 1 | 940 | (the scale) | 25 % | clean |
| 2 | 1,766 | 47 % | | |
| 3 | 2,533 | 67 % | | |
| 4 | ? | | | |
| 5 | 2,172 | 58 % | | |
| 6 | 2,357 | 63 % | | |
| 11 | 1,987 | 53 % | | |
| 12 | 3,484 | 93 % (sheds) | | |
| 13 | 1,741 | 46 % | | |
| 14 | 1,709 | 45 % | | |
| 15 | 4,902 | 130 % (sheds) | | |
| 19 | 1,716 | 46 % | | |
| 22 | 1,750 | 47 % | | |
| 29 | 810 | 22 % (was 46 %) | | |
| 30 | 2,652 | 71 % (was 85 %) | | |
| 31 | 3,226 (the factory scene, AMBIENT after 2026-10-15: bowed bells, longer grains and tails; 3,200 before it, measured as in the note of 2026-10-11 below; 2,998 by callgrind on 2026-10-17, after that day's changes) | 75 % (was 81 %) | | |

Measured again on 2026-10-17 (GATE 400, scene tempo, freeze to tape, p-locks, SPRD on the channel and COLOR):
by callgrind, run 1 1,020, 15 5,267, 24 1,987, 25 2,806, 31 2,998, 32 2,896 (TEST now plays GLITCH at its 136
BPM). P-locks and SPRD cost nothing until used. Worth reading on the FM-1 with the new build: run 31 and 32 for the
scene, and a p-locked SEQ with SPRD on a channel FILT for the general paths, which the host can't weigh for the
target's cache.

Measured again on 2026-10-14 (the bass drum's longer body, the resonators every sample, TRAN and BODY at 0, and
the other session's CHRONOS and RIPPLE forms pulled in between): run 1 1,007, 15 5,169, 19 1,747, 25 2,766, 29 668,
30 2,302, 31 2,718. The -Os cache simulation of the factory scene reads 3,077 (2,999 the day before), 1.79 divides
a sample.

Measured again on 2026-10-13 after RIPPLE 4 (MODE DRV RIPL, DRV 25 on by default: a cubic on every string every
sample) and the harness resetting the master, the rooms and the shedding caps between runs: run 1 978, 15 5,005
(sixteen strings), 19 1,679, 25 2,663, 29 633, 30 2,238, 31 2,608. The -Os cache simulation of the factory scene
(docs/performance-plan.md's measure) went 2,924 to 2,999, about 2 % of the FM-1 at the scale above; the device
hasn't read this build yet. The rows below are the readings as they stand.

The host costs were measured again on 2026-10-11, after the second round from playing it (the arpeggiator on every
source, RIPPLE's own: about 7 a sample more on every run) and after the grains went to half rate with a bigger
pool (runs 3, 5, 6, 12, 15 now sound 18 or 30 grains where they sounded 16 or 24, for a little less). Row 1 stays at 940, the build the 25 % was read on,
since it sets the scale. Read again after the grain pool stopped capping a track at
what RATE and SIZE ask and BODY's wave followed the grains' notes: those moved them by 1 to 7 a sample. And again after
six pages a device (NPK 24, for GRAIN's GRNS): 1 to 2 a sample more; then the step keys' lights, 1 to 4 more; then resting on silence (runs 29 and 30 new: what
stopped costs, and three tracks muted). And after the shared rooms (two rooms A and B every track sends to, in
place of a room per track): where several tracks had rooms the cost fell (run 14, CHRONOS on all four, 1,879 to
1,678; run 15 5,072 to 4,871; run 20 1,864 to 1,753); with one room it rose a little (runs 18, 21, 30: 12 to 31 a
sample: the room always runs both sides now, and its damping rounds toward zero). Then round 3 (bar record, the
zoom, SYNC): 2 to 22 a sample more (run 1 is 1018).

**The factory scene, 2026-10-10:** CPU above 80 % and crackles once the drums came in, where the host predicted
about 45 % (1,720). So the scale from run 1 doesn't hold for every kind of work: the target divides where the host
multiplies (see bryo-status.md's decision of the day). After the fixes the host reads 1,547; the device's reading
is the next number to take.

**The factory scene again, 2026-10-11:** one modulation depth on a drum's LVL put it over 80 % with crackles. The
depth itself costs about 19 a sample (half a percent); the scene had crept up with the day's features, and the 1,547
above was taken another way. Measured the way I measure it now (5 s of the scene playing, after a second's warm-up,
under callgrind: row 31) it was 2,886 after the fixes, 3,046 (81 %) before the rooms. CHRONOS was the biggest single
part (about 620 a sample: three rooms and two delays). With the two shared rooms, both at QUAL LO in the scene, it's
2,821, 75 %. Run sheet row 31 asks for the device's number.

**The factory scene, 2026-10-12:** on the FM-1 it went from a comfortable 65-70 % to about 80 % over the last few
updates. The host doesn't see it: measured the same way at every commit since the shared rooms (5 s of the scene
after a second's warm-up, under callgrind), it's flat, 3,088 at the rooms (`cb9fb69`) and 3,095 at `a96691f`, and
3,304 before the rooms. So whatever moved is something the host can't count. What I found and fixed: divides the
target does in hardware where x86 multiplies. None was per sample; they were per block, but there were more of them:
POLY's END in hundredths (three divides where the percent took one) asked by every voice every block, POLY's FINE
through `cents_inc` (two more) even at 0, and the loop window (six) asked by TAPE and by GRAIN every block. Now END
and the window are kept per track and worked out only when their inputs move, and `cents_inc` runs only with FINE
off 0. That's roughly 30 to 45 divides a block in the scene, about 1 to 2 % by run 1's scale: not the whole of it.
The rest I can't measure from here. Code and constants run from flash through the cache (`app.ld`: `.text` and
`.rodata` in XIP), so a build that moves the hot loops can cost cache misses the host never sees. The way to find it
is on the device: the scene's CPU on `cb9fb69` (the rooms), `631ab95` (SYNC) and this build tells which update it
was, and whether it's code or layout.

**Found, later the same day: signed divides by a power of two.** I'd been reading the host's count as if a divide
by 32768 or 2 were a shift. It isn't, for a signed value: `/` rounds toward zero and `>>` toward minus infinity, so
the compiler has to emit a fix-up or a divide, and the target's (and x86 at -Os, it turns out) emits the divide. The
shared rooms put four more in the innermost loop: the combs' damping went from `>> 15` to `sp_mul` (`/ 32768`), on
purpose, so a tail rounds to 0 and the room can rest. With the allpasses' `o_ / 2` and the comb gains that was 12
hardware divides a room sample, 6 an output sample for two rooms at LO; the mixer's mono sum for REC IN,
`(vl + vr) / 2`, 4 more a sample on every build since GRAIN. Measured with the x86 -Os build (where they're real
`idiv`s too): 20.3 divides an output sample on the scene, 10 of them these. Now `sdiv2` (dsp.c) does the same
rounding with a shift and a mask, bit for bit, and the scene has 7.8 a sample left (per block: the modulators',
the grains' block lookups; the grains' block index too, 1.4 to 1.2). At the 20-plus cycles a hardware divide costs, 10 a sample is on the order of 5 to 8 %
of the FM-1, which is the size of what you heard. The I-cache finding above still stands as the other part.

**The performance pass, 2026-10-12 (docs/performance-plan.md):** nine host-visible items and two sweeps of
hardware divides, every one bit-identical (the audio fingerprints, `checkpoint_sim fp`, hold on all 31 runs). The
scene by the simulator with the attributes kept: 3,384 to 2,917 host instructions a sample, 7.5 to 1.9 hardware
divides. The suite's load tables are the host harness's count, without the inlining, and still: run 1 1,018 to 956, the
groove 1,719 to 1,632, every slot on every track 2,913 to 2,674, everything on 4,913 to 4,633, stopped 809 to 618
(the last with Tier C, resting on silence).

## What I'll do with the numbers

- **Run 3 under about 70 %:** GRAIN fits as it is; then run 15 decides the rest (below).
- **Run 3 between 70 and 85 %:** I'll take the cheap savings first. The buffer recording can skip decoding when
  FDBK is 0 (about 100 a track on the host), and the pools can shrink (32 / 24 / 16 are a table: GR_POOL).
- **Run 3 over 85 %, or crackling:** the cap has to come down before anything else gets built, and I'll measure
  where the cycles go with the debug build's console (`BRYO_MSC=0`: `cpu_pct` there is the same number).
- **Run 15 over 85 %:** grains and strings shed first as they do now; if that isn't enough, CHRONOS needs a shedding
  step too (the room at a quarter rate is the first idea), and I'd decide it with this number in hand.
- **Anything in runs 7 to 10 wrong:** that's a timing bug the host can't show me, and it comes first. Tell me what
  you did and what you heard.
