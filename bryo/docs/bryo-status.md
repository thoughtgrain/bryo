# Bryo: where it stands, and how to pick it up

Updated 2026-10-10. This is the page I'd want to read first after a break, or hand to anyone joining: what Bryo
is right now, how it's put together, how to build, test and measure it, what I decided and why, and what's still
open. It points into the longer docs instead of repeating them. When something here and the code disagree, the
code wins, and this page needs fixing.

## The short version

Bryo is replacement firmware for the M-VAVE FM-1, forked from Felucca 1.0.3 (`b22a24b`). It turns the FM-1 into
a four-track sound-sculpting instrument after the PRD (my product spec, which lives outside this repo; the docs
cite its sections as "PRD 2.2" and so on). Each track is a source (TAPE, SYNTH, POLY or DRUM) through a fixed chain:
GRAIN, RESONATOR, COLOR, SPACE, then a channel strip into the mix and a master compressor. Four modulator slots
per track move any of it: hold a slot's pad, turn a knob.

**Phases 1 to 9 of 10 are built, with one piece open (p-locks).** On 2026-10-10 it ran on an FM-1 for the first
time: installed over USB-MIDI from stock V15, it boots, plays the four factory reels clean, and run 1 reads 25 % CPU.
The rest of the hardware checkpoint (`docs/hardware-checkpoint.md`) is under way.

| Phase | State |
| --- | --- |
| 1 Skeleton | done: boots on Felucca's hardware layer; install, UBOOT and calibration kept |
| 2 TAPE + reels | done: ADPCM tapes, factory reels, slices, REC and overdub, clear and undo |
| 3 USB audio in + SYNTH + POLY | done: the USB record mode, the SYNTH and POLY sources |
| 3b GRAIN, 3c memory by usage | done: the live buffer, one grain pool shared by demand; tapes that grow, TRACKS, REC IN |
| 4 RESONATOR | done: four strings, keys set the root |
| 5 COLOR + SPACE | done |
| 6 Mixer + routing | done: channel strips (LOW HIGH FILT PAN), master compressor; routing is REC IN |
| 7 Modulation | done but p-locks: LFO ADSR SEQ FOLLOW, hold-and-turn depths, MONO's clear and undo |
| 7b DRUM | done: a fourth source, a CR-78-inspired drum machine of Bryo's own sounds, patterns, rhythms and SEED |
| 8 Projects | done: six slots in flash (A/B), SAVE, the project view, the last one back at power-on; takes kept in user reels |
| 9 Screen | done: the dot-grid screens; a rail under each modulated knob (reach, set, a block where it is now) |
| 10 Tools + docs | started: the manual (`docs/manual/`); still to do: reel upload tool, installer text |

## The instrument on one page

```
 per track (x4):
   source ─► GRAIN ─► RESONATOR ─► COLOR ─► SPACE ─► channel (LOW HIGH FILT, level, PAN) ─┐
   TAPE | SYNTH | POLY | DRUM   (stereo from GRAIN on)                                     │
     ▲ REC IN: what REC prints onto the tape (the others, one track, itself)               ▼
     └──────────────────────────────────────────── sum ─► master compressor ─► MASTER ─► out
 four modulator slots per track: LFO | ADSR | SEQ | FOLLOW, each with a depth to any knob
```

- **Pages and knobs.** KNOB 1 to 4 always edit the four values on screen. HOME is the source (its pages: TAPE 3,
  SYNTH 5, POLY 4, DRUM 4, each ending with LVL); EDIT is GRAIN then RESONATOR; FX is COLOR then SPACE; LFO ENV SEQ are
  the modulators (LFO: M1-M3, picked with OP1-OP3 while it's held; ENV: M2; SEQ: M4) and ARP the arpeggiator; GLO is
  the mixer (EDIT there: the channel, MASTER, SETUP); ALGORITHM turned is REC IN. A pad held +
  SELECT picks that pad's thing (HOME: the source; a slot: its engine; GLO: TRACKS). Every control in every
  context is in `docs/controls.tsv`, checked by `tests/controls_check.py`. GRAIN, RESONATOR and COLOR have ROUT:
  INS (WET crossfades) or SEND (the dry at full, the device added).
- **Modulation.** A slot's pad tapped opens its page as you let go; held, KNOB 1 to 4 set its depth to the knobs on
  the page shown (the strip shows the depths while it's held; a modulated knob gets a mark). MONO held half a
  second clears the track's depths; SAVE held undoes the last clear (tape or modulation).
- **The keys.** White keys play slices (TAPE), notes (SYNTH, POLY) or the drum kit (DRUM: on its STEP page they
  write steps, accents, live hits, or wipe), set RESONATOR's root on its page, move GRAIN's cursor on its page, and
  pick SEQ steps on a SEQ page. Black keys: OP1 to OP4 mute, OP5 reverse, OP6 half
  speed, MONO and POLY clear (held), 0 freezes GRAIN; or, with SETUP's KEYS on NOTE (GLO held + OCT+), the 27 keys
  are a keyboard from F on SYNTH and POLY, and the black keys play.
- **ARP.** Per track, on SYNTH and POLY: the keys held go in, one note at a time comes out in time (KEY, LTCH, ADD;
  UP DOWN UPDN RND PLAY; 1-3 octaves; RATE, GATE, SWNG). `arp.c`.
- **DRUM.** Sixteen synthesized instruments on the white keys, a pattern of 2 to 4 bars, ten written rhythms
  (PATN), and SEED: any value but 0 plays a version that varies the pattern the way a player would (the backbone
  kept, ghost notes, the hats thinning and filling, fills at the end), the same version every time. OCT- / OCT+ pick
  the bar; POLY held clears the pattern.
- **Projects.** SAVE tapped saves the whole instrument into the project loaded last; PRESETS opens the six slots
  (OCT+ loads, SAVE saves, POLY held deletes, OCT- leaves); the last project saved comes back at power-on. A take on
  a track's own tape goes into a user reel named after it.
- **The USB record mode.** Stopped, REC held a second: the FM-1 plays the computer and records it; trim, level,
  fades, then a white key picks the track.

## How it runs

One core, no RTOS. Three contexts, and the rules between them are most of the architecture:

- **The audio ISR** renders a control block (CTL = 32 samples, 0.73 ms at 44.1 kHz) at a time: `chain_block` in
  `chain.c`, which calls `mod_tick`, then each track's source and devices, the mixer, the master stage. The main loop
  can't interrupt it. It sheds load above 85 % (grains first, then strings; never the tape).
- **TIMER5** scans the input and services USB. It can nest inside the audio ISR only for the USB audio endpoints'
  service (`uac_service`, `uaco_service`); `usb_poll` never runs nested.
- **The main loop** owns everything that allocates: the shared memory, the depth lists, the UI. Its rule for memory
  shared with the ISR: shorten the list (or set the flag) first, publish (`RING_PUBLISH`), then free. For anything
  the ISR reads as a whole (a depth list), build it in the half the ISR isn't reading and flip one byte.

**Memory.** Sound lives in one pool of 152 chunks (28 s of tape-format sound: `mem.c`) handed out by use: tapes as
long as what's on them, GRAIN's live buffers, RESONATOR's strings and SPACE's lines while they're on. Nothing is
set aside per track. When it runs short: a cleared tape's chunks first, then a parked track's tape, then the end of
the longest tape. Main RAM holds 82.0 of 96 KiB (.bss; 50.7 of it sound on purpose, the rest of the 96 the stack's), the pool 326.2 of 336 KiB (2026-10-09 with DRUM, 32-bit
build: the firmware preprocessed, its inline assembly taken out, compiled `-m32 -Os`, `size -A`).

**Parameters.** `param.c` holds every knob as an integer in its own range (`tp[t]`), with a descriptor (name,
range, format) per knob. Up to 20 knobs a device or engine, in pages of four. The main loop writes them; the ISR
reads them once per block. With modulation, the device code reads through `TPD(t, array)` instead: a pointer to
`tp`'s own array, or, when that array has depths, to a copy with the depths added (`mod.c`). Pitches and filter
cutoffs also get a finer value than the knob holds (`mod_pitch16`, `mod_fine8`).

**Fixed point throughout,** no 64-bit arithmetic and no compiler builtins on the target (the JieLi compiler would
turn them into library calls). Q15 audio, Q12 and Q10 gains, Q8 for fractions of a knob or a sample.

## The files

All under `bryo/`. `firmware/src/bryo.c` is the one translation unit (it includes every module in order, as
Felucca's `felucca.c` did); `tests/bryo_host.c` includes the same files for the host.

| File | What |
| --- | --- |
| `firmware/src/bryo.h` | what the main loop and the ISR share (`sys`, `track[]`), the keys' layout |
| `dsp.c` | fixed-point building blocks: the state-variable filter, oscillators, soft clip |
| `master.c` | the output stage: DC block, speaker EQ, limiter, USB level |
| `param.c` | every knob: descriptors, defaults, formats; the device, source and engine tables |
| `mod.c` | modulation: the depths, the lists, the clock, the four engines, the fine values |
| `mem.c` | the shared sound memory: chunks and owners |
| `tape.c` | TAPE: ADPCM tapes, reels, the head, REC and overdub, clear and undo |
| `synth.c`, `poly.c`, `source.c` | SYNTH, POLY, and the source table |
| `arp.c` | the arpeggiator: the keys into one note at a time, in time, before SYNTH and POLY |
| `drum_voice.c`, `drum.c` | DRUM: the one synthesizer every instrument shares; the kit, the rhythms, SEED's versions, the player |
| `grain.c` | GRAIN: the live buffer, the scheduler, the shared pool of grains |
| `reso.c`, `color.c`, `space.c` | RESONATOR, COLOR, SPACE |
| `mixer.c` | the channel strips and the master compressor |
| `usbrec.c` | the USB record mode (the endpoint itself is in `usb.c`) |
| `chain.c` | a control block of the whole instrument; TRACKS; shedding; the main loop's memory polls |
| `reel.c`, `vdisk.c`, `msc.c` | user reels in flash; the FM-1 as a USB drive (FAT12, WAV in and out) |
| `project.c` | projects: six A/B slots in flash, the packed stream, takes into reels, the last one at power-on |
| `tpfactory.c` | the eight factory track presets (two a source): knob lists from the defaults, and their loader |
| `tpreset.c` | track presets: eight tracks' sounds (A..H) in the seventh A/B pair, copied forward unparsed; the track snapshot SAVE held puts back (a preset load, SCL's reset) |
| `ui.c`, `ui_input.c`, `ui_px.c`, `ui_viz.c` | the screen's regions; buttons, keys and knobs into actions; the dot grid and pictograms; the pictures |
| kept from Felucca | `main.c`, `audio.c`, `usb.c`, `panel.c`, `settings.c`, `storage*.c`, `console.c`, `ota*.c`, `lcd.c`, `gfx.c`, `midi_uart.c`, the HAL in `firmware/hal/` |

Tests and tools:

| File | What |
| --- | --- |
| `tests/run_tests.sh` | everything below that's quick, plus Felucca's kept hardware and installer tests; ends "ALL HOST TESTS PASSED" |
| `tests/bryo_host.c` | Bryo's chain, input and screens on the host: 594 checks, and every screen rendered |
| `tests/bryo_golden.txt`, `ui_golden.py` | each screen's pixel fingerprint |
| `tests/checkpoint_sim.sh` | the hardware checkpoint on the host: 28 runs rendered to WAV, each one's cost under callgrind, and a stress test with the audio and USB interrupts cutting into the main loop while the memory's books are checked |
| `tests/mod_audit.sh` | every knob a modulator can move: does it change the sound, does it step at the blocks, what it costs (`--cost`) |
| `tests/controls_check.py`, `coverage.py` | the controls map; line coverage of Bryo's files |
| `tools/gen_reels.py`, `gen_tables.py` | the factory reels; the generated tables (`build/gen/`) |
| `tools/gen_manual_shots.py` | the manual's pictures from the screen renders (the suite checks they're current) |

## Building, testing, measuring

```sh
tests/run_tests.sh                       # the host suite (C compiler, Python with Pillow and fontTools)
sh tests/checkpoint_sim.sh 20            # the 28 runs, their cost (valgrind), 20 s of interrupt stress
sh tests/mod_audit.sh [--cost]           # how modulation behaves, knob by knob (numpy)
python3 tests/ui_golden.py update tests/bryo_golden.txt build/bryo_ui   # after a deliberate screen change
./build.sh                               # the device image: needs JieLi's toolchain and SDK (BUILDING.md)
```

The type check the suite runs is worth running alone after any firmware change: `clang -fsyntax-only
--target=i386-unknown-linux-gnu -ffreestanding -fno-builtin -Wall -Wno-unused-function -Ifirmware/hal
-Ifirmware/src -Ibuild/gen firmware/src/bryo.c` (no warnings is the bar; the explicit 32-bit ELF target, not `-m32`,
so it means the same on macOS, where clang would otherwise check for Mach-O). For memory-safety, build `tests/bryo_host.c` or
`tests/checkpoint_sim.c` with `-fsanitize=address,undefined -fno-sanitize=shift`; `CHECKPOINT_SLOW=1` slows the
stress test's timers for the sanitizer.

**The unit of cost** is host instructions per output sample of `chain_block`, under callgrind, because I can't
measure the device here. Where it stands (`checkpoint_sim.sh`, 2026-10-10; runs 1, 19, 24 and 15 measured again after the arpeggiator went
in, about 4 a sample for the four arpeggiators and the new reels; the rest are from before it):

| Run | What | Cost |
| --- | --- | ---: |
| 1 | four reels, nothing on | 921 |
| 19 | a realistic four-track groove | 1,594 |
| 24 | the groove, modulated the way I'd play it | 1,815 (87 of it the modulators) |
| 21 | the groove's busiest moment | 2,454 |
| 23 | GRAIN at its densest on one track | 2,986 |
| 25 | every slot on every track, 128 depths | 3,161 |
| 26 | DRUM busy alone (FUNK, VARY 100, FILL 100, long decays), the others plain reels | 1,308 |
| 27 | the groove with a DRUM track in place of its drum reel | 1,898 |
| 28 | four DRUM tracks at once, every voice (20) | 2,482 |
| 15 | everything on, all four tracks | 4,908 |

My target for a realistic four-track scene is 2,000. The first device reading (run 1, 2026-10-10: 25 %, in 5 % steps)
puts the ratio at about 37.6 host instructions a sample per percent, if the load scales with the host's count: the
groove would be about 43 %, the target 53 %, and everything on (run 15) about 130 %, which sheds. The next runs
test that it scales.

**Flash:** the image runs in place from 568 KiB. On the same host measure (LLVM, `-Os`, 32-bit), Bryo is now 139 KB
of code and 159 KB of constants (302 KB; it was 281 on 10-08, before modulation and the LEVEL pages). Allowing the
device's code to be up to 1.34 times bigger, that leaves roughly 230 to 280 KB free. `./build.sh` gives the real
number.

## The rules I work by

- Every new file starts with `/* SPDX-License-Identifier: GPL-3.0-only */` (or `#` for scripts).
- Never touch the update loader's path (`ota_hw.c`, the loader). The HAL is header-only and I only add to it.
- No 64-bit math and no builtins in firmware code.
- The ISR never allocates and never waits; the main loop owns memory (the rule above).
- Measure before optimizing, and measure what's heard, not a proxy. The modulation audit taught me that the hard
  way: my click counter read a saw's own edges as clicks, and I nearly shipped smoothing that cost 25 to 50 a sample
  for nothing. The block-edge test is the one that tells.
- A sound change is deliberate: the screens have fingerprints, the checkpoint has its numbers, and a default that
  changes what you hear gets said in the commit.
- Docs are first person, casual, and say how and why. The architecture doc keeps an "as built" section per part.

## Decisions, and why (the ones that shape everything)

- **2026-10-07: a source per track, a fixed chain after it.** The PRD's chain stays fixed; what starts it is an
  interface (`source_t`), so adding a source doesn't touch the devices. TAPE, SYNTH, POLY and DRUM so far.
- **2026-10-07: tapes are IMA ADPCM, mono, 22.05 kHz.** Four times the time per byte of 16-bit, cheap to decode
  in the ISR; the decoded quality suits a sound-sculpting instrument.
- **2026-10-07: the screen is a dot grid** (2 x 2 px dots, one ink), drawn per region only when it changes.
  Readable at a glance, small in RAM, and it gave back 59.5 KB of the pool.
- **2026-10-07: line up with the Torso S-4** where Bryo does the same job: names, knob order, what a modulator
  has (`docs/s4-alignment.md`). The S-4's WAVE and RANDOM are one LFO here, random a shape of it, so every shape
  gets the random knobs. SEQ is the PRD's own addition.
- **2026-10-08: memory by usage.** No per-track budgets: one shared pool, given by what's in use, taken back in a
  fixed order. Tapes grow as they record (up to 23.6 s).
- **2026-10-08: GRAIN's grains are one pool shared by demand** (32 for one track, 24 for two, 16 for three or
  four), split by what each track's settings ask for.
- **2026-10-08: the USB record mode is its own mode.** Everything else stops while the computer is recorded;
  REC held a second opens it. Audio out to the computer in that mode is future work.
- **2026-10-09: modulation reads a modulated copy only of what's modulated.** A track without depths reads exactly
  what it did; an array with depths is copied and summed once a block. The first version copied whole tracks and
  cost 7 KB of main RAM.
- **2026-10-09: every source has a LEVEL** on its last page, as the S-4 does; that needed five pages a device
  (NPK 20).
- **2026-10-09: the modulation depths are a sorted list per track, not a dense table** (and storage.c streams a
  page at a time): 6.2 KiB of main RAM back as stack headroom. `mod_dep()` / `mod_dep_set()` read and set them.
- **2026-10-09: projects are a field-by-field stream, packed, never a struct dump** (versioned, counted arrays,
  clamped on load, depths by array and knob): a newer Bryo reads older projects. Six slots, A/B, Bryo's own magic so
  Felucca never misreads one. A take on a track's own tape goes into a user reel named after the project and track.
- **2026-10-09: GRAIN, RESONATOR and COLOR can be sends** (ROUT: INS or SEND, per device, per track): the dry at
  full, the device added at WET. Per track rather than shared aux buses: no new memory or CPU, every track keeps
  its own effects. SPACE was always a send.
- **2026-10-09: DRUM's sounds and rhythms are Bryo's own.** CR-78-inspired in the instrument list and the
  character, written from scratch: one data-driven voice, a patch row per instrument. A first pass reused
  Felucca's drum voices; I replaced them so the kit is ours. "CR-78" stays in the docs, never on screen.
- **2026-10-09: SEED is a pure function of (version, bar, step, instrument).** No state, so the same SEED always
  plays the same thing, the screen can draw what will play, and an LFO or SEQ on SEED just picks versions. Each
  instrument varies by its role (the backbone kept, the time thinned and filled, the colour ornamented), and only
  with instruments the pattern already uses.
- **2026-10-09: pitches and filter cutoffs modulate finer than their knobs** (1/256 of a step), so vibrato and
  resonant sweeps glide; a SEQ's steps on a pitch still land on semitones.
- **2026-10-10: one spelling and one meaning per idea across sources, devices and modulators** (the consistency
  review in the architecture doc): TYPE for a filter's LP/BP/HP, DRV, DEC, times shown as times, a track's loop
  as the modulators see it is its own (DRUM's: the pattern), and a depth turned back to 0 catches there.
- **2026-10-10: a way back, and a sound to keep.** SCL held + a knob resets it, + the page's pad resets the page;
  a track's sound saves as one of eight presets (the PRESETS view's second page) and loads into any track without a
  question, since SAVE held puts the track back. A preset is the sound, not the music or the mix: the pattern,
  level and REC IN stay with the track. They live in the last two free flash sectors, as one more A/B pair. Eight
  factory presets follow them, two a source, level-matched to the defaults; they need tuning by ear on hardware.
- **2026-10-10: the pages follow two rules, through a layout, not by moving storage.** LVL is KNOB 4 of a source's
  last page (ROUT left of it); a modulator's AMT OFS are KNOB 3-4 of its last page; SYNTH turns its pages in
  POLY's order. `PLAYOUT` maps page places to stored knobs, so the DSP, the depths and the projects didn't move
  (SEQ's new SPRD AMT OFS made the project format version 2). DRUM's hits trigger the modulators as keys do; HOME
  turns the page as it's let go; OCT moves DRUM's bar only on DRUM's own pages.

- **2026-10-10: ARP is an arpeggiator, the modulators are three swappable and a sequencer.** The ARP pad played a
  fourth modulator slot; now it's a real arpeggiator (`arp.c`) that sits between the keys and SYNTH or POLY and hands
  the source one note at a time, always with a silent block between steps, so neither the sources nor the KEY
  triggers needed anything new. The three swappable modulators (LFO, ADSR, FOLLOW: never SEQ) live on the LFO pad
  (held + OP1-OP3 picks), ENV keeps M2, SEQ keeps M4. Slots keep their stored numbers (slot 2 the SEQ, slot 3 the old
  ARP slot, now M3), so projects and depths didn't move; the project format went to 3 for the arpeggiator's knobs.
  It isn't in track presets: like the pattern, it's the music, not the sound.
- **2026-10-10: the panel's preferences are the instrument's.** SETUP (the mixer's fourth page): KEYS (shortcuts or
  a keyboard), KNOB (1X, 2X, 4X, ACC: Felucca's acceleration), HOLD, LEDS. KEYS and KNOB live in a byte of the
  settings record no Felucca engine uses (settings.c), so going back to Felucca reads nothing from it.
- **2026-10-10: the factory scene evolves, and it's always there.** With nothing saved, Bryo starts on a scene that
  changes by itself (`tpfactory.c` tpf_scene): T1 a DRUM on FUNK whose SEED M3 picks and EVOL moves on, M4 retuning
  the kit; T2 POLY on BELL, its arpeggiator on a latched Am7 at random, M4 moving the chord a bar at a time (Am7 Dm7
  Em7); T3 DRON ringing strings M4 walks (A C D E G) and grains pitched at random in A minor; T4 POLY on FOLD, its
  arpeggiator up and down the pentatonic in swung sixteenths with T2's chords. An arpeggiator's notes held without a
  key (latched, piled up) follow the transport and are saved in projects (format 4). FACTORY, past P6 in the
  project view, loads it again whatever's saved (`pj_factory`), and saves nowhere until you pick a slot.
- **2026-10-10: a blank project.** OCT+ on an EMPTY project slot starts one there: defaults, empty tapes, no reels,
  no pattern, no depths. The factory reels BELL, DRON and FOLD are synthesized modular patches (FM bells, a
  drone, wavefolded plucks; one bar each, no samples); BEAT is the drum loop.

- **2026-10-10: the FM-1 crackled with the scene's drums (CPU past 80 %), and the host had said ~45 %.** What I
  found and did: (1) the target's compiler keeps a division by a constant as a hardware divide, which the host's
  counts hide (x86 makes it a multiply); one ran on every track on every sample even with GRAIN off
  (`dry * (100 - WET) / 100`), others in SPACE's spread, SYNTH's drive and TAPE's loop crossfade. They're gains now,
  worked out once a block (the scene's sound differs by at most 16 LSB, 72 dB under it). Any new per-sample code:
  no `/` or `%` that isn't a power of two. (2) DRUM's voices stop at -60 dB, not -84. (3) Overload sheds DRUM's
  voices after the grains and the strings (5 a track down to 2, a calm second each back). (4) The scene's drums
  6 dB up, T4's TILT gone (COLOR off). Host: the scene 1,720 -> 1,547, run 1 944 -> 921; the device should gain
  more than that (the divides). Its CPU reading on the scene is the number to take next.

## Known considerations and open questions

- **P-locks** (a SEQ step's own depth): waiting on a decision. SEQ's step values are in.
- **SPRD on PAN** pulses (a tremolo) at 100 instead of moving the sound, because PAN is a balance. Left for now; the
  options are in the architecture doc ("Modulation, as built", open points). My lean: let SPRD reach every knob of
  COLOR and the channel strip, the S-4's way.
- **VEL** (ADSR) does nothing: the keys have no velocity, and MIDI in doesn't reach the tracks yet.
- **FOLLOW's USB source** listens to nothing until the computer's audio comes in outside the record mode.
- **SEQ steps on TUNE** are a percentage of 48 semitones; a step page in semitones would be friendlier.
- **The device itself:** the CPU load is unknown (the hardware checkpoint), the USB OUT endpoint (EP4) is untested
  on hardware, macOS might pick Bryo as its default output when plugged in, and the flash size is an estimate.
- **The channel's FILT bottoms out near -29 dB** at its lowest corner (the filter's precision); inaudible under
  the passband, but it isn't infinite.
- **DRUM's kit is voiced by measurement** (pitch, level, decay) and waits on a listen: the hats and maracas are
  bright (energy to 17 kHz), the bass drum sits low for a small speaker, the cymbal rings 9 s at its design.
- **DRUM's FILL and EVOL** read "no change" in the modulation audit: they act at the pattern's end, past its 3 s
  window. Not a fault, but the audit can't see them.
- Still open from the plan: what the white keys do on a SYNTH track while GRAIN's page is up (they play notes).

## What's next

1. **The hardware checkpoint** (`docs/hardware-checkpoint.md`): run 1 is in (25 %, clean); the rest of the run
   sheet, then listen. Everything after depends on those numbers.
2. **Phase 10, tools and docs:** the manual is drafted (`docs/manual/`, checked on hardware as the checkpoint
   goes); the installer's text.
3. **DRUM by ear:** listen to the kit and the rhythms, tune `DRM_KIT` and `DRM_PRESET`.
4. Decisions waiting: p-locks, SPRD on PAN, and whether shared aux buses (one reverb all tracks send to) are worth
   a chain rework (ROUT's per-track sends are in).

## Where the details are

| Doc | What it holds |
| --- | --- |
| `README.md` | what Bryo is, what works on the panel, building and testing |
| `docs/manual/` | the manual: step by step, with the screens (its pictures: `tools/gen_manual_shots.py`) |
| `docs/bryo-status.md` | this page |
| `docs/bryo-architecture.md` | the plan against the hardware, the budgets, and an "as built" section per part: how each one works, what it costs, how it's tested, and what I changed my mind on |
| `docs/controls.tsv` | every control, in every context, with its status and phase |
| `docs/s4-alignment.md` | Bryo's devices and modulators against the Torso S-4's, knob by knob |
| `docs/hardware-checkpoint.md` | the host's numbers, and the run sheet for the first time on the device |
| `docs/ui-assessment.md` | the screen's design review |
| `docs/screens/bryo_screens.png` | every screen, as the host renders it |
| `docs/FELUCCA-README.md`, `NOTICE.md`, `ACKNOWLEDGEMENTS.md` | where it came from, and the GPL notice |

## How it got here

About 50 commits over three days, each one a working step (`git log --oneline -- .`):

- **2026-10-07:** the plan; phase 1 on Felucca's hardware layer; the dot-grid screen and a picture per page; TAPE
  and the factory reels (phase 2); user reels in flash and the FM-1 as a USB drive; SYNTH and POLY; the S-4
  alignment.
- **2026-10-08:** GRAIN (and its live buffer); memory by usage, TRACKS, REC IN; a review round; the hardware
  checkpoint's run sheet and its host simulation; RESONATOR, COLOR, SPACE; the USB record mode; the mixer;
  realistic load runs; GRAIN's shared pool.
- **2026-10-09:** modulation; TUNE played live; a LEVEL on every source; the modulation audit (RESONATOR's pitch
  glides, fine pitch); fine filter cutoffs; this page; DRUM (its own voice and kit, the source, SEED); sends
  (ROUT); projects (phase 8); the RAM pass; phase 9's rails.
