# Bryo architecture: from the PRD to the FM-1

Written 2026-10-07 as the plan for turning the PRD (Bryo: a 4-track generative sound-sculpting instrument) into
firmware on the FM-1: the PRD is the goal; this doc is where I check it against what the hardware can actually do,
decide what to keep from Felucca, and set the order to build in. Since then each part has been built, and each has
an "as built" section below saying how it works, what it costs, how it's tested and what I changed my mind on. The
sections before "Phase 1, as built" are the plan as I wrote it (the module map there is the plan, not the files).

**For where it all stands now, start with [bryo-status.md](bryo-status.md):** the state, the real file map, how
to build, test and measure, the decisions and the open questions on one page.

## Decisions so far (2026-10-07)

| Question | Decision |
| --- | --- |
| Where audio comes from | All of it: reels (factory + uploaded), resampling inside Bryo, **and USB audio in from the computer, early** (phase 3), **plus synth engines**. |
| What starts a track's chain | **A source engine per track**, picked like a modulator engine: TAPE, SYNTH, POLY, and engines still to be decided. The source flows into GRAIN, RIPPLE, COLOR and SPACE. |
| Tape format | IMA ADPCM, mono, 22.05 kHz; tapes take what they record from one shared memory (28 s in all; see "Memory by usage"). |
| Gestures where the hardware differs | Adapt what makes sense; **ask before changing anything else**. The remaps below stand. |
| MONO and POLY keys | Hold for 0.5 s with a countdown ring, plus one level of undo. |

**POLY (decided):** a polyphonic sample player: the white keys play a sample chromatically, and each voice has its
own envelope and filter before the track's chain. Its three HOME pages and their knobs are under "Source engines".
(The black key named POLY, which clears the tape, is a different thing; the screen always says "POLY SOURCE" for
the engine.)

Still open: what the white keys do on a SYNTH track while GRAIN or another tape-style page is focused (for
now they play the synth's notes on every page).

## What the hardware really gives me

I had the Felucca source read for facts before planning, because a few of them change the design:

| Fact | Where it comes from | Why it matters |
| --- | --- | --- |
| **No audio input.** I2S out to the codec only; the ADC reads only the MASTER pot and the battery; USB audio is device to computer only. | `hal/fm1_audio.h`, `hal/fm1_adc.h`, `src/usb.c` | TAPE can't record a mic or line in. It records what Bryo itself makes (see "Where TAPE's audio comes from"). |
| **RAM: 344 KiB pool + 96 KiB .data/.bss**, no external RAM. | `app.ld` | Four live tapes have to share about 150 KiB. That sets the tape format. |
| **Flash: 1 MiB.** The app area is 0x4000–0x92FFF; Felucca's data region is 0x97000–0xDFFFF (296 KiB). | `hal/fm1_flash.h`, `src/storage.c` | Saved projects with audio compete for about 280 KiB. |
| **Flash writes block interrupts.** Erase and program run with IRQs off; audio drops out. | `hal/fm1_flash.h`, `src/storage_hw.c` | A page programs in well under a half buffer, so a save that only programs plays on; an erase stops the sound, so the output fades out around it (`out_fade`) and the copy a project save writes is erased ahead (see "Saving while playing"). |
| **7 relative encoders, no push switches:** SELECT, ALGORITHM, PRESETS, KNOB 1–4. | `src/panel.c`, `hal/fm1_input.h` | The PRD's "push SELECT", and PRESETS and ALGORITHM as buttons, need another gesture. |
| **14 buttons:** FX SCL ENV LFO EDIT GLO HOME SAVE ARP SEQ PLAY REC, plus OCT− OCT+. | `src/panel.c` | The PRD's 12 pads match. The PRD's **SEL** pad is the one printed **SCL**. |
| **27 keys:** 16 white, 11 black (F3..G5), one single-colour LED each. | `hal/fm1_input.h` | Colour identity for the modulator slots lives on the screen only. |
| **MASTER:** an analog pot read by the ADC and turned into a digital gain. | `src/main.c` | Works as the PRD says. |
| **Screen:** 240×240 RGB565 over SPI at 12 MHz; anti-aliased text; knob arcs from a precomputed mask. | `src/lcd.c`, `src/gfx.c`, `src/ui_graph.c` | The 4-value strip with arcs is buildable on what's there. |
| **CPU clock:** not documented. Felucca renders 128-frame halves (2.9 ms) and sheds voices at 85% load. | `src/audio.c` | I budget in host instructions per sample (Felucca's method) and measure on the device early. |
| **I can't build device firmware here.** The JieLi toolchain host (pkgman.jieliapp.com) and the SDK host (gitee.com) are blocked by this environment's network policy. | | I verify on the host (tests, renders, instruction counts). You build and flash with `./build.sh`. |

## The PRD, adjusted to the hardware

These are my proposals. Each keeps the PRD's intent and only changes the gesture or the scale.

1. **SELECT has no push.** To assign an engine to a modulator slot, *hold the slot's pad and turn SELECT*:
   the engine list opens and scrolls, and releasing the pad commits the choice. The same gesture
   (hold, turn, release) is how depth is set, so there's one idiom to learn.
2. **PRESETS and ALGORITHM are encoders.** Turning PRESETS opens the project manager and scrolls its slots;
   turning ALGORITHM opens the routing matrix and moves through its cells. Inside both views, **OCT+
   confirms** (load, or toggle a route) and **OCT− backs out**, which is the idiom Felucca users already
   know. Pressing any device or modulator pad also leaves the view.
3. **The black keys, left to right:** F#3 G#3 A#3 C#4 = **OP1–OP4** (mutes), D#4 = **OP5** (reverse),
   F#4 = **OP6** (half speed), G#4 = **PIT** (pitch down), A#4 = **GLO** (pitch up), C#5 = **MONO**,
   D#5 = **POLY**, F#5 = **0** (freeze). That's the PRD's order applied to the physical order. The code
   doesn't record the printed labels, so this needs checking against the panel.
4. **MONO and POLY are destructive** (clear modulation, clear the tape). I'd make them *hold for half a
   second* with a countdown ring on screen, and keep one level of undo (SAVE held, as in Felucca). A brushed
   key shouldn't erase a take.
5. **Colour identity is screen-only.** The LEDs are single colour: a pad's LED shows focus (lit) and hold
   (blinking). Slot colours (cyan, amber, green, magenta) appear on the dials and headers. For colour-blind
   users, each slot also gets a glyph (1–4 in a small pill) wherever its colour appears, so colour is never
   the only cue.
6. **64 grains per track becomes 64 grain slots, with a measured cap on how many sound at once.** Felucca's
   GRAIN engine runs 36 grains in total. 256 at once is very unlikely to fit next to four resonators and
   four Space devices. DENSITY schedules into 64 slots, but only the first *N* sound (N = 16 per track to
   start, tuned on the device) and the scheduler drops the oldest. It sounds like 64-grain density at
   normal settings and stays inside the budget at extremes.

## Source engines: what starts each track

Each track's first device is a **source engine** in the HOME slot. TAPE is one source; SYNTH is another;
more can be added without touching the rest of the chain. In the code (`source.c`) it's a table of render
functions, one row per engine; the knobs and the names live in `param.c` beside the devices':

```c
typedef struct {
    /* one block of track t into out; keys: the white keys held (0 when the track isn't the focused one, or
     * the source isn't the track's any more) */
    void (*render)(uint32_t t, uint32_t keys, int32_t *out, uint32_t n);
} source_t;
```

I first sketched it with `init`, `key` and `knobs` hooks too. In practice every engine wants the keys as a
bitmask once per block (the ISR reads them, so a note starts within 0.7 ms), its knobs come from `dev_p()` /
`dev_v()` in `param.c`, and nothing needed an init the zeroed state didn't already give.

How it works: the HOME pad focuses the source; holding HOME and turning SELECT picks the source engine
(the same hold-and-turn idiom as modulator engines). Why an interface: the PRD's chain stays fixed, the
sound-making front end stays open, and a new engine is one file plus one table row.

- **TAPE:** the ADPCM looper below. White keys are 16 slices.
- **SYNTH:** a simple subtractive voice built from Felucca's ANALOG engine (`eng_analog.c`: oscillators,
  filter, envelope), trimmed to a few voices. White keys are chromatic, and OCT−/OCT+ shift octaves.
- **POLY:** a sound played across the keys, up to four notes at once, each through its own ADSR and filter
  (see "POLY, as built").
- **DRUM:** a drum machine, CR-78-inspired and built from scratch: sixteen synthesized instruments on the white
  keys, a pattern of 2 to 4 bars, written rhythms to start from, and SEED, which plays a varied version of the
  pattern (see "DRUM, as built").
- **Later engines:** to be defined; each is one file plus a row in the `source_t` table.

Any source's output can still be printed onto the track's tape with REC, so every track keeps a tape
buffer even when TAPE isn't its source.

## Where TAPE's audio comes from

With no input, a tape records what the instrument makes. That's still a lot:

- **Reels in flash:** factory sounds built into the app (Felucca's generated drums and the CC0 instrument
  samples are already here), plus your own audio uploaded over USB with `tools/fm1_sample_upload.py` or the
  browser (record in the browser, upload, as Felucca does today). Any reel loads into any tape.
- **Resampling inside Bryo:** REC records the track's own chain output (sound on sound through
  RIPPLE, COLOR and SPACE back onto the tape). ALGORITHM routes another track's output into this track's
  tape, which is the PRD's live overdub between tracks.
- **Sources that make sound from nothing:** RIPPLE played from the keys (Karplus-Strong excited by a
  noise burst) and COLOR's noise generator. A blank tape plus RIPPLE plus REC is a way to start from
  silence.
- **USB audio from the computer (decided: early).** A UAC1 OUT endpoint (computer to FM-1), 44.1 kHz
  16-bit stereo, into a ring buffer the audio ISR reads. The work: a second streaming interface in the
  descriptors (kept apart from the existing IN endpoint, so macOS and Windows still see one class-compliant
  device), clock drift between the computer and the codec handled by nudging the read rate, and an
  underrun that plays silence instead of clicking. It becomes an INPUT choice for any track's tape.

## The tape format, and why

How I'd store it: **IMA ADPCM, 4 bits per sample, mono, 22,050 Hz, in 256-sample blocks with a
predictor/step index per block.**

Why:

- **Length.** 16-bit stereo 44.1 kHz would give each track about a third of a second. ADPCM at 22.05 kHz
  is 11 KB per second, so 36 KiB per track gave about **3.3 s per track** (more at half speed). Since
  "Memory by usage" the tracks share the RAM instead: 28 s in all, up to 23.6 s on one tape, four tracks whatever
  they record.
- **It's the format Felucca's user samples already use** (`eng_sample.c`, `tools/fm1_sample_upload.py`).
  Loading a reel from flash into a tape, or saving a tape, is a straight copy with no transcoding, and the
  upload tools keep working.
- **Random access.** Slices, grains and reverse playback start decoding at a block boundary (at most 255
  samples to skip), as Felucca's GRAIN engine already does with its seek index.
- **Overdub that readers can't catch half-written.** The write head decodes the old block, mixes in the new
  audio, re-encodes it into a 128-byte staging block, and commits the block and its index entry together.
  A grain reading the same spot sees either the old block or the new one, never a mix.
- **Stereo comes later in the chain.** GRAIN's spread, RIPPLE and SPACE are stereo; the tape itself is
  mono. That's the trade for length.

## Budgets

### RAM (the 344 KiB pool, and main RAM's spare room)

| Use | Bytes | How it's sized |
| --- | ---: | --- |
| Shared sound memory | 251,328 pool + 50,688 main RAM | 143 chunks of tape-format sound (26.6 s in all), handed out by usage: tapes as long as what's on them, GRAIN's live buffers while GRAIN is on, RIPPLE's strings and SPACE's lines while they're on. See "Memory by usage" |
| RIPPLE | 4 chunks a track, while on | 4 strings × 1,056 samples × 2 B (lowest string F1, 43.7 Hz), from the shared memory while WET is up |
| SPACE delay | 8 chunks a track, while DLY is up | 8,192 samples × 2 B at 22.05 kHz (TIME's 370 ms); 4 chunks at 8 bits when memory is short |
| SPACE rooms | 10 chunks a room (two: A and B), while a track sends to it | the room (a comb a chunk, the allpasses in a fifth) and the pre-delay's line (5 chunks; 3 at 8 bits); see "The shared rooms, as built" |
| COLOR | 0 | 64 B of state a track |
| Screen canvas | 5,460 (main RAM) | 2 bits a dot (1.6 KB) and two 4-row line buffers (3.8 KB); see "The screen as dots" |
| GRAIN | 9,728 pool + 2,900 main RAM | 32 grain slots × 304 B (a 128-sample decoded window each), and each track's buffer bookkeeping; the buffers themselves are shared memory |
| USB record mode | 12,288 pool + 900 main RAM | the monitor's ring (1,024 stereo frames, 23 ms) and the take's (4,096 mono samples, 186 ms); the take itself is shared memory |
| SYNTH | 576 | 4 tracks × 3 voices of oscillator and filter state (.bss) |
| POLY | 8,288 | 4 tracks × 4 voices × a 256-sample block reader (pool), plus 448 B of voice state (.bss) |
| The drive | 17,340 | the FAT, root and write cache of the USB drive (the 36 KB WAV inbox is gone: a WAV lands in shared memory) |
| Modulation | 485 main RAM + 6,016 pool | the depths (a sorted list of up to 32 a track) and MONO's undo in main RAM; the ISR's lists, the slots' state and the modulated knob arrays in the pool |
| Settings storage | 256 main RAM | a page of bounce buffer for the settings record (storage.c; it was a whole 3,840-byte payload) |
| DRUM | 3,700 pool + 1,650 main RAM | 4 tracks × 5 voices (a voice's coefficients and state, 176 B) in the pool; each track's pattern, accents and the instruments' own knobs (304 B a track), the undo and the screen's render in main RAM |

Measured now (KiB, 32-bit build, 2026-10-09 with DRUM): the pool holds 326.2 of 336 (9.8 spare; build.py keeps 8),
main RAM's .bss 81.0 of 96 (it was 87.2 before the RAM pass below; 82.0 since the track snapshot, 2026-10-10). DRUM's voices first sat in main RAM and took
it to 90.3; they're ISR-only state, so they moved to the pool with the rest of it.

The RAM pass (2026-10-09). Main RAM's .bss isn't all overhead: 50.7 KB of it is sound (24 chunks of the shared
memory live there on purpose), and what .bss leaves is the stack's. The deepest stack I can see on the host build
(`-fstack-usage`) is the main loop's input path, about 2 KB, with the audio interrupt's chain (chain_block's 1.9 KB
frame and what it calls, about 2.5 KB) nested on top and TIMER5 on that: 5 to 6 KB in all, so the 8.8 KiB left was
enough but not generous. Two things were sized for jobs they no longer had:

- the depths were a dense table, a byte for every slot x target x track (2,624 B, and MONO's undo a copy of a
  track's, 658 B), when a track can only have 32 depths (mod_nudge refuses more). They're a list per track now,
  sorted by target and slot (so the same depths are the same bytes, which the project's fingerprint relies on, and
  mod_rebuild needs no sort): 388 B, the undo 97. Read through `mod_dep()`, set through `mod_dep_set()`.
- storage.c's buffer held a whole payload (3,840 B) from when Felucca kept its projects there; only the 572-byte
  settings record uses it now. A payload is checked in place a page at a time, read straight into the caller's
  buffer, and programmed through a 256-byte bounce buffer (the driver wants RAM sources). Felucca's own storage test
  (torn writes at every byte, bit rot, sequence wrap) passes unchanged.

6.2 KiB back, all of it left as stack headroom (15 KiB free). The 24 sound chunks, the screen's line buffers and the
update path's buffers stay as they are. The USB record mode's two rings first went into main RAM and took it to 92.2, too close to the stack, so
they live in the pool's room that was kept for USB audio in.

What gives way when memory runs short: chunks come off a cleared tape first, then a parked track's tape (TRACKS),
then the end of the longest tape. Nothing is set aside per track, so there's no constant to shrink any more.

### Flash: the installed image (measured 2026-10-08)

The app's image runs in place from flash: 0x4000..0x92FFF, 568 KiB of room once the package header is in
(`firmware/app.ld`, XIP 0x8DFBC bytes). I can't build for the pi32v2 here (the JieLi toolchain is behind the
network), so I measured the unity build the way the device build starts (LLVM IR, `-Os`) on a 32-bit host target,
and Felucca 1.0.3 the same way, as a yardstick:

| Build | Code | Constants | Code in RAM | Sum |
| --- | ---: | ---: | ---: | ---: |
| Bryo, 2026-10-08 | 120 KB | 157 KB | 4 KB | 281 KB |
| Bryo, 2026-10-09 (modulation, LEVEL pages) | 139 KB | 159 KB | 4 KB | 302 KB |
| Felucca 1.0.3 | 254 KB | 236 KB | 4 KB | 494 KB |

Constants are the same bytes on any target. Code isn't: but Felucca shipped and fits its 568 KiB, so the pi32v2's
code can be at most about 1.34 times this measure. That puts Bryo (2026-10-09) between about 300 and 350 KB:
roughly 230 to 280 KB free. `./build.sh` prints the real number; that's the one to trust.

Where Bryo's goes: the four factory reels 98 KB (ADPCM, 11 KB a second of sound), Felucca's anti-aliased fonts 43 KB
(only their sizes are still used, by the boot, crash and update screens: about 40 KB to win back if it's needed,
without touching the update path's code), tables 5 KB; the UI's code about 55 KB, the DSP and the main loop the rest.
A device like COLOR or SPACE costs 2 to 6 KB of code, a page of the UI 1 to 3 KB.

### Flash (Felucca's 296 KiB data region, rewritten as Bryo's)

| Area | Bytes | What |
| --- | ---: | --- |
| Projects | 6 × 4 KiB × 2 (A/B) = 48 KiB | Machine state: devices, sources, patterns, mixer, routing, modulators, which reel each tape holds; packed, about 2 KB each (see "Projects, as built": eight didn't fit beside the reels) |
| Track presets | 1 × 4 KiB × 2 (A/B) = 8 KiB (0xDF000, 0xFE000) | Eight tracks' sounds, A..H, about 250 B each packed (see "Track presets and SCL's reset, as built") |
| Reel slots | 6 × 40 KiB = 240 KiB | Saved tapes and uploaded audio, all the same format; a long one spans the slots after it (21.5 s at most); a project points to reels |
| Settings | unchanged | 0xFC000–0xFEFFF, as Felucca |

A project points at the reels its tracks play; a track's own recorded tape is copied into a user reel when it's
saved (see "Projects, as built").

### CPU

I'll budget in Felucca's unit: host instructions per 44.1 kHz output sample, measured with callgrind,
because I can't measure the device here. Felucca's heaviest single engine is about 1,500; the device copes
with its mixes. My target for all four tracks, everything on, is **at most 2,000**:

| Stage, per track | Estimate | Notes |
| --- | ---: | --- |
| TAPE read | 20 | ADPCM decode at 22.05 kHz, linear interpolation to 44.1 kHz |
| GRAIN | 240 | 8 sounding grains × about 30 (decoding, window ramp, interpolation, pan), plus about 100 recording its buffer (measured: see "GRAIN, as built") |
| RIPPLE | 60 | 4 strings (measured nearer 240 a track on the host: see "RIPPLE, as built") |
| COLOR | 30 | drive table, crush, follower plus noise (measured 180 a track with every knob on, 0 at the defaults: see "COLOR, as built") |
| SPACE | 80 | delay plus reverb at 22.05 kHz (measured 220 a track with both on: see "SPACE, as built") |
| Mixer | 60 for all | measured 145 a track with LOW, HIGH and FILT all on (0 at their defaults), the pan 10, the compressor 2: see "The mixer, as built" |
| Modulators | 10 | control rate (every 32 samples), not per sample (measured 84 for all four tracks modulated the way I'd play them, 5 with none: see "Modulation, as built") |
| **Track total** | **~440** | **× 4 = ~1,760**, plus the mixer and compressor ~60 |

The audio ISR's 85% guard stays. Instead of shedding voices it sheds *grains* first (lowers the sounding
cap), then RIPPLE strings, and never the tape itself. COLOR and SPACE aren't shed (yet; see "SPACE, as built").

## What I keep, reuse, and remove

**Kept as is (the hardware and the install path, untouched):**

- `firmware/hal/*`, `firmware/crt0.S`, `firmware/app.ld` (pool sizes aside)
- `firmware/loader/*`, `src/ota.c`, `src/ota_hw.c`: the update loader and the M-UPGRADE path, byte for
  byte, so installing stays exactly as easy as it is now
- `src/usb.c` (MIDI, USB audio out, console, the UBOOT and M-UPGRADE SysEx entries), `src/storage.c` and
  `src/storage_hw.c` (A/B flash), `src/lcd.c`, `src/gfx.c`, `src/libc.c`, `src/midi_uart.c`,
  `src/midi_clock.c`, `src/console.c`, `src/settings_persist.c`
- `src/main.c`'s boot: watchdog, boot-loop guard, OCT− + OCT+ UBOOT countdown, hardware calibration
- `src/panel.c`: button, key and encoder mapping with calibration
- `src/audio.c`'s DMA and ISR frame, timing and load meter (the render call inside changes)
- Tools: `tools/build.py`, `tools/fm1pkg_make.py`, `tools/fm1_install.py`, the web installer
  (`web/index_pkg.html`, `web/fm1ota.js`, `web/fm1pkg.js`, `web/make_site.py`), the font, icon and palette
  generators

**Reused as material (code moved into the new devices, credits kept):**

| From | Into | What |
| --- | --- | --- |
| `eng_sample.c`, `eng_grain.c` | `tape.c`, `grain.c` | ADPCM decode, the seek index, grain windows |
| `phys_dsp.c` (DaisySP port), `phys_symp.c` (Rings port) | `reso.c` | the Karplus-Strong string, damping, sympathetic coupling |
| `eng_lofi.c`, `dsp.c` | `color.c` | bit and rate reduction, drive tables |
| `fx.c` | `space.c` | the delay and ROOM reverb (SPRING maybe later) |
| `slice_store.c`, `gen_samples.py` | `reel.c`, factory reels | the flash slot format, the bundled sound set |
| `ui_draw.c`'s `card_t`, `ui_graph.c`'s `knob()` | `ui_strip.c` | the 4-value strip and arcs |
| `editor.c` (sample transfer only) | `link.c` | uploading reels over USB SysEx |

**Removed** (they stay in git history; this is a full rewrite and none of it fits Bryo):

- the 13 synth engines (`eng_*.c`, `fm6_core.c`, `fm4_convert.c`, `up_fm6.c`, `drum_voice.c`), `voice.c`
  and its note allocator
- the note sequencer, chords, arpeggiator, song chain, motion, the old mod matrix, perform layer, slicer
  (`seq.c`, `chord.c`, `song_chain.c`, `motion.c`, `mod.c`, `perform.c`, `slicer.c`)
- presets, favorites and user preset banks (`upreset.c`, `favorites.c`, `params.c`'s pages)
- Felucca's UI (`ui*.c`) and the web editor (`web/editor.html`) with its parameter protocol
  (`editor*.c`); the editor protocol is entirely Felucca's parameters
- their tests, the golden sound renders and CPU baselines, replaced by Bryo's own

## The new module map (the plan; the files as built are in bryo-status.md)

All under `bryo/firmware/src/`. `bryo.c` replaces `felucca.c` as the single translation unit (the same
unity-build approach), and `tools/build.py` points at it.

```
bryo.c            unity build: config, HAL, then every module in order
bryo_config.h     the budgets as constants (TAPE_BYTES, GRAIN_SLOTS, GRAIN_SOUNDING, …)
bryo.h            shared types: track_t, device params, mod slots, the project struct

audio engine (audio ISR, per 32-sample control block)
  tape.c          ADPCM tape: record, overdub, loop start/length, speed/direction, 16 slices
  grain.c         64-slot grain scheduler reading the tape; size, density, pitch, spread
  reso.c          Karplus-Strong network: root, feedback, damping, mix; chromatic keys
  color.c         drive, crush, follower-driven noise and its tone
  space.c         per-track delay and small reverb
  mixer.c         track level, pan, filter, master compressor, ALGORITHM routing into tapes
  chain.c         one track = source → grain → reso → color → space; the 4 tracks; grain shedding
  source.c        the source_t table; picking a track's source engine
  synth.c         SYNTH: a small subtractive voice (from eng_analog.c)
  usbrec.c        the USB record mode: the OUT endpoint's frames, the monitor, the take (usb.c: the endpoint)

control
  mod.c           4 slots per track; engines WAVE, RANDOM, ADSR/FOLLOW, SEQUENCER; depths; sums
  plock.c         per-step locks for SEQUENCER slots
  transport.c     PLAY/STOP, tempo, MIDI clock in/out (wraps midi_clock.c)
  param.c         the parameter table: 5 devices × 4 knobs per track + mixer; ranges, curves, names

storage
  reel.c          reel slots in flash and factory reels in the app image; load and save a tape
  project.c       8 project slots (A/B); save and recall full state
  link.c          USB SysEx: reel upload and download (what the sample tools call)

UI (main loop)
  ui.c            focus (device or modulator), holds, key modes, views (project, routing, mixer)
  ui_input.c      buttons, keys and encoders into actions; hold-to-assign; p-lock gestures
  ui_strip.c      the 4-value strip: base arcs, modulation arcs, motion dots, summed white dot
  ui_head.c       the dynamic header ([TRACK 2 : COLOR], [MOD 1 : WAVE]), transport, tempo
  ui_views.c      project manager, routing matrix, global mixer
```

Tests, under `bryo/tests/`, all host-run:

- `tape_test.c`: record, overdub, slices and reverse; the ADPCM round-trip error; a reader never sees a
  half-written block
- `grain_test.c`, `reso_test.c`, `color_test.c`, `space_test.c`: stability at the parameter corners, no
  DC, no clipping, the cost per sample
- `mod_test.c`, `plock_test.c`: each engine's shape, depth summing, locks per step
- `budget_test.c`: callgrind instruction counts for the whole chain, 4 tracks, worst case, against
  `budget.txt`
- `ui_render.c` (new), `ui_golden.py`, `ui_screens.py`: every Bryo screen drawn and fingerprinted, as I did
  for Felucca's
- Kept from Felucca: `input_test`, `storage_test`, `usb_desc_test`, `uac_test`, `ota_test`, `ldr_test`,
  `install_test.py` and the installer's web tests, so the hardware and install paths stay covered

## Phase 1, as built (2026-10-07)

Done: `bryo.c` builds the kept hardware layer with Bryo's core, and Felucca's instrument is removed (96 files).

| File | What it is |
| --- | --- |
| `firmware/src/bryo.c` | the unity build |
| `firmware/src/bryo.h` | the shared state (`sys`), frame sizes, the key layout (white 0..15, black OP1..0) |
| `firmware/src/chain.c` | the four tracks per control block; a test voice per track for now; shedding |
| `firmware/src/master.c` | Felucca's output stage, moved from `fx.c` unchanged |
| `firmware/src/param.c` | five devices and four modulator slots per track, four knobs each, with the PRD's names |
| `firmware/src/settings.c` | the settings record, kept in Felucca's layout so calibration and USB prefs carry over |
| `firmware/src/ui.c`, `ui_input.c` | the header, the 4-value strip with dials, the footer; pads, keys, knobs, LEDs (the track tiles moved into the mixer: see "The screen, revised") |
| `firmware/src/panel.c` | the panel map plus HARDWARE CALIBRATION (moved here from Felucca's UI) |
| `tests/bryo_host.c` | the chain, the input mapping, the settings carry-over, every screen (one ink: one set) |
| `tests/bryo_golden.txt` | the screens' pixel fingerprints |
| `tests/run_tests.sh` | everything above plus a clang type check of the firmware and the kept hardware tests |

How I verified it without the JieLi toolchain: `clang -fsyntax-only -m32` over `bryo.c` in every build-flag
combination (zero diagnostics; the unchanged Felucca tree also gives zero, so it's a fair stand-in for the
compiler's front end), `build.py`'s register-access rules, the kept hardware tests, and Bryo's own host tests.
**Not verified: a device build and a run on the FM-1.** That needs `./build.sh` on a machine with the toolchain.

The installer also learned one thing: on a device running Bryo, "Return to official V15" skips Felucca's backup
step (Bryo has no Felucca-format data) and asks for a confirmation instead.

Porting the material files: they're gone from the tree but not from history. For example
`git show 6aa6073:moss/firmware/src/eng_grain.c` prints Felucca's GRAIN engine as imported.

Still open from phase 1: Felucca's MENU is gone (palette, speaker EQ, LEDs, HOLD, USB serial, ABOUT with the
source QR code). The settings it wrote still apply. **Which gesture opens Bryo's system menu is a question for
you** (Felucca used HOME held; Bryo's HOME is a device pad).

## The screen, revised (2026-10-07)

After phase 1, two changes to the screen, from your review:

- **Track picking moved under GLO.** Hold GLO: the mixer comes up and white keys 1 to 4 pick the track (KNOB 1
  to 4 set the levels); let go and you're back on the page. Tap GLO and the mixer stays up. The PRD's SEL (the
  SCL pad) is free; I propose holding it for the system menu.
- **The space the track tiles took is a visualization per page** (`firmware/src/ui_viz.c`), drawn from the
  page's values: TAPE's loop window, the sample lit where GRAIN's grains read it (solid blocks in a stereo lane), RIPPLE's response, COLOR's sine through the device,
  SPACE's echoes and tail, each modulator's shape in its slot colour, and the tracks in the mixer. The page's
  last-turned knob is drawn in the accent and named in the panel's caption with its value, and on COLOR the
  inset follows it (the drive curve, the noise, or the tone filter). The DSP will use the same mappings the
  pictures do, so what's drawn is what you'll hear.
- **The screen is a groovebox OLED now: a dot grid, bitmap type, pictograms that carry the value.** The first
  pass (icons, tags, filled shapes in anti-aliased type) didn't land; what you wanted was the Syntakt/Digitakt
  screen, so I rebuilt the main screens on a 120 x 120 grid of 2 x 2 px dots (`firmware/src/ui_px.c`). Why a
  grid: at 240 px on a 1.5" panel, a 1 px stroke is a hairline, while a 2 px dot is about the size of an
  Elektron OLED's pixel, and drawing everything on the same grid is what makes the type, the pictograms and the
  plots read as one system.
  - **Type** is two bitmap faces I drew as ASCII art in `tools/gen_px_font.py` (generated into
    `build/gen/ui_pxfont.h` like the other headers): 5 x 7 for labels and values (doubled to the right for the
    bold titles) and 3 x 5 for small print. Square corners on purpose.
  - **One ink: white on black, fixed for now.** I first let the palette pick the ink (amber, green, paper...),
    but you wanted the look settled before any colour comes back, so these screens ignore the palette setting
    (it's still stored, so nothing is lost when a palette comes back). There's a dim ink for guides and
    unlit things; values are full ink, since dim numbers were hard to read. Emphasis is inversion and borders,
    never colour: the last-turned knob's label is inverted in the strip and in the plot under it, and REC is a
    solid dot in the tempo bar.
  - **The header** is a box with the track (`T1`) or slot (`M2`), the page in bold, and an inverted tempo bar
    holding REC, the transport and the metronome, like the references.
  - **The strip** is four pictograms, and each one is the value: a dial's pointer, a fader's fill, a slope's
    length, a keyboard with the root key lit, chevrons for tape speed, stacked takes for DUB, a dot field for
    density. FDBK's dial ring turns to chaos as feedback rises: a slow wobble first, then jitter near the top, the
    way a loop close to self-oscillation smears. Every point stays within 2 dots of the circle, so however wild it
    gets it never leaves its box or touches its neighbours. The label sits under it (4 letters, Elektron-style: `STRT`, `TUNE`, `DRV`, `SWNG`), the value
    under that in the dim ink.
  - **The plot below** speaks the same language (TAPE is a reel-to-reel with the reels close over the middle, so it
    reads in proportion, and no tape path from the reels down, which only cluttered it; the run along the bottom is the whole tape with the sample drawn on it, lit inside the
    loop window and dim outside; the first version was a dashed strip that read as a road): 1-dot lines, square nodes where a value sits, dotted drop
    lines and guides, 3 x 5 labels under the stretch they name (the ADSR page is the clearest example of the
    pattern). Messages show as an inverted box over the plot, so the title never moves.
  - **Kept as it was:** the region signatures (a region redraws only when what it shows changes), the
    mappings each plot uses (they're still the DSP's), and the update path. UPDATE MODE moved onto the grid
    too; HARDWARE CALIBRATION keeps Felucca's look since it's a one-time setup screen.
- **The mixer carries its meters in the faders, and each track has a channel strip.** On the mixer the four
  pictograms are faders: two notches show the level you set, and the fill inside is the track's live meter, so
  one glance gives both. The selected track's fader has a border, and the channel panel below is tagged with the
  same track, so it's clear whose EQ and filter you're looking at. That freed the panel below for the selected track's channel: LOW and HIGH shelves
  (+-12 dB), FILT (one knob, DJ-style: low-pass to the left of 0, high-pass to the right, 0 is open) and PAN.
  Below the faders they're drawn as one response curve over 8 octaves with a node on each corner, plus two
  speaker bars for the pan. Hold EDIT on the mixer and KNOB 1-4 set them; let go and you're back on the levels.
  The values live per track in `tp[].ch` (`firmware/src/param.c`) and the curve is `ch_resp()` in
  `firmware/src/ui_px.c`, which is the response the mixer DSP will apply in phase 6. The PRD's master
  compressor, which the old plan had on this page, still needs a home; I left it for phase 6.
- **SEQ shows its steps' values, and the keyboard got simpler.** The SEQ page drew 16 identical blocks with the word
  SEQUENCE under them, which said nothing the header didn't. Now each step is a bar as tall as its value, with
  the steps past LEN left as dim slots, SLEW drawn as the glide between values and SWING nudging the off-beats.
  For that the slot needs values, so each SEQ slot stores 16 (`tp[].steps`, 0..100) with a default pattern that
  accents the beats; it moves nothing until the slot is assigned a depth, and setting a step's value (hold the
  slot, hold a white key, turn a knob) arrives with phase 7. RIPPLE's ROOT pictogram is now one outline with
  dividers and thin black keys, the root key filled (or a black one hollow), instead of a key-by-key drawing.
- **Second pages, reached by pressing the same pad again.** The PRD gives each device exactly four knobs, and
  all of them were on screen. What was missing is what the sound needs that the PRD never gave a knob, so I
  added a page 2 where it matters. Pressing a pad again steps through its pages: EDIT goes GRAIN, GRAIN 2,
  RIPPLE, GRAIN; FX goes COLOR, COLOR 2, SPACE, SPACE 2; HOME and the slot pads toggle page 1 and 2. The
  header shows the page as a block and a dot after the title.

  | Page 2 | Knobs | Why |
  | --- | --- | --- |
  | TAPE 2 | FADE, REV, HALF, GAIN | the loop's crossfade (no clicks at the seam); OP5/OP6's reverse and half speed, now visible; the record gain |
  | GRAIN 2 | MIX, JIT, WIN, REV | without a dry/wet GRAIN always replaces the tape; where grains land, their window, the chance one plays backwards |
  | COLOR 2 | LVL, MIX, SRR, GATE | drive adds loudness with nothing to take it back; a blend; the rate split off CRSH (which keeps the bits); where the noise's envelope opens |
  | SPACE 2 | DMIX, RMIX, PRE, WIDE | the PRD gives no wet level for the delay or the reverb; the reverb's pre-delay; the stereo width |

  (Since renamed to the Torso S-4's names, with more pages: see `docs/s4-alignment.md`.)

  RIPPLE stays at one page: its four knobs already cover it. (The modulators' pages are under "Modulators".) Every page 2 knob also shows in the
  picture below, not only as a number: GAIN scales (and clips) the sample on the tape, FADE draws ramps at the
  loop's ends, REV and HALF turn and halve the chevrons; WIN rounds the grains, REV makes some hollow, JIT adds
  whiskers; COLOR's curve and wave include MIX and LVL, and GATE draws its line over the noise; SPACE's echoes
  and tail scale by their mixes and the tail waits for PRE; the LFO starts at PHAS, fades in, shows the beat ticks
  when locked, the key when restarted and its next time round dotted; ADSR bends each stage by its curve and
  draws the loop and the softest key's envelope; FOLLOW draws its envelope over what it listens to; SEQ hollows the steps PROB skips and marks STRT. A page 2 knob you turn is tagged with its value.
  Switch-like values (REV, LOOP, TRIG, CLK, MODE, DIR, SHPE) print their names, not numbers.
- **Modulators, lined up with the Torso S-4.** I checked the S-4's modulator reference (Torso's docs for WAVE,
  RANDOM, ADSR and FOLLOW, and its CC map) and gave Bryo's engines the same knobs, with one change you asked for:
  the S-4's WAVE and RANDOM are a single LFO here, and random is one of its shapes. Why: they share everything
  but the shape (rate, phase, depth, offset, stereo spread, sync), so one core is less to learn and less code,
  and it means the random knobs (SMTH, VAR, LEN) work on every shape: a square that glides, a sine whose level
  drifts each cycle. The engines are now LFO, ADSR, SEQ (the PRD's) and FOLLOW; hold the focused slot's pad and
  turn SELECT to pick one (this was phase 7's plan; it's in now because FOLLOW can't be reached any other way).

  | Engine | Page 1 | Page 2 | Page 3 | Page 4 |
  | --- | --- | --- | --- | --- |
  | LFO | RATE, SHPE (SIN TRI SQR SAW RND), SKEW, FOLD | CURV, SMTH, VAR, LEN | AMT, OFS, PHAS, SPRD | SYNC, TRIG, FADE |
  | ADSR | ATK, DEC, SUS, REL | ACRV, DCRV, RCRV, SPRD | VEL, LOOP, AMT, OFS | |
  | SEQ | LEN, RATE, SLEW, SWNG | DIR, TRIG, PROB, STRT | | |
  | FOLLOW | SRC (SELF, T1-T4, USB), GAIN, RISE, FALL | HOLD (sample and hold), AMT, OFS, SPRD | | |

  VAR is the S-4's VARIATION: each time round the loop drifts from the last by up to that much (0: the same loop
  forever); on RND that's the steps themselves, on the other shapes each cycle's level, repeating every LEN
  cycles. SMTH is a slew on any shape. SPRD is the right channel's phase against the left, drawn as a dim second
  trace. The picture is a window of time (2026-10-12; it drew two cycles whatever RATE was): 4 seconds free-running,
  a dim tick a second, or 2 bars on SYNC BPM, a tick a beat, so RATE shows as how many cycles fit, half a cycle to
  16, the window's width named under the plot. AMT scales the slot as a whole (the S-4's AMOUNT); the PRD's hold-and-turn still sets each target's
  depth. The ARP pad's slot started as the LFO on RND with a little VAR, so the random modulator was one press
  away (2026-10-10: that slot is M3 on the LFO pad now, and ARP is the arpeggiator: "The arpeggiator, the keys as a
  keyboard, SETUP"). ADSR's MODE/SENS from the last pass went to FOLLOW, where they belong. What I couldn't confirm from the
  S-4 docs: FOLLOW's exact knob names (the docs describe a source, rise and fall times and sample and hold), so
  those four names are mine.

The controls are mapped in `docs/controls.tsv` (one row per context, control and gesture, with status and
phase), validated by `tests/controls_check.py`, which also fails if a physical control goes unmentioned.

## Phase 2, as built (2026-10-07)

The tape works end to end on the host: `firmware/src/tape.c`, with `tests/bryo_host.c` checking the codec round
trip (40 dB on a sine), the loop window, reverse, a slice played while stopped, a recorded take, clear and undo, and
the refusal that protects a take. What it does and why:

- **Each track has a RAM tape and can play a reel instead.** Four factory reels live in flash
  (`tools/gen_reels.py` makes them, in the tape's own format: BEAT from the generated drums, KEYS and AIR from
  the CC0 piano and flute, PLUK from the generated plucks; about 95 KB of flash; 2026-10-10: BEAT, then BELL DRON
  FOLD, synthesized modular patches, no samples). TAPE 3's REEL picks TAPE or a
  reel. A reel plays straight from flash, so browsing reels never touches your tape; the first REC copies the reel
  onto the tape (and REEL turns to TAPE), and is refused if the tape already holds a take, so nothing is lost by
  accident.
- **Reading is block-cached.** A reader decodes one 256-sample block and then reads any position in it, so
  speed, reverse and slices cost one block decode per 256 samples. A second reader carries the old head through
  a crossfade whenever the head jumps: at the loop's seam (FADE long, at least 2 ms) and on a slice.
- **Writing commits whole blocks.** REC stages the block under the head, mixes each sample (old x DUB + new x
  GAIN), and re-encodes the block from its own stored state when the head leaves it. Other blocks keep their
  states, and a reader re-decodes a block after it's committed, so it hears the old block or the new one.
- **What REC records, for now: the other three tracks' mix** (one control block late, 0.7 ms). With no audio
  input that's the meaningful source in this phase; ALGORITHM (phase 6) makes the routing a choice, and the
  track's own device output joins when the devices exist.
- **Clearing keeps the data until it's overwritten.** POLY held 0.5 s marks the tape empty (a reel: back to the
  tape, cleared); SAVE held brings it back, as long as REC hasn't written over it. That costs no RAM for an undo
  copy.
- **The keys are slices everywhere for now.** The phase 1 test tone is gone. White key n jumps the head to
  slice n of the loop while playing, and plays just that slice while stopped. RIPPLE (phase 4) and SEQ (phase 7)
  will give the keys their own jobs on their pages.
- **The screen draws the real sound.** The tape view, GRAIN and FOLLOW read each block's peak, which the
  encoder stores with the block; the tape view names what plays (a reel, TAPE or EMPTY), shows REC armed, and
  cuts the moving head through the sound while it plays.

RAM: the four tapes are 152 KB of the pool (data, block states and peaks), the readers and writers 6 KB; with the
screen canvas that's about 218 KB of the 344 KB pool.

Not in this phase, on purpose: **user reel slots in flash and the upload tool** (they need the flash layout that
projects use, so they move to phase 8 with projects), and **the `source_t` interface** (TAPE is the only source
until SYNTH and POLY arrive in phase 3, which is when the interface earns its keep).

## Files over USB: the FM-1 as a drive (2026-10-07)

You asked for projects and sounds to be easy to move when the FM-1 is plugged in, both ways, instead of an upload
tool. So the FM-1 shows up as a small USB drive named BRYO, next to its audio and MIDI ports. Copy files off to
keep them; copy a WAV on to load it. Built and tested on the host; not tried on a computer yet.

What's on it (`firmware/src/vdisk.c` makes the volume on the fly; nothing is stored twice):

| File | What it is |
| --- | --- |
| `TAPE1.WAV` .. `TAPE4.WAV` | what each track plays now (its tape or its reel) |
| `REEL1.WAV` .. `REEL4.WAV` | the factory reels (read-only) |
| `USER1.WAV` .. `USER6.WAV` | your reels, the slots that hold a sound |
| `README.TXT` | how the drive works, in short |

How it's built, and why:

- **No real file system, a made-up one.** The flash has 296 KiB for data and blocks interrupts while it writes, so a
  real FAT on it would be tiny and would stall the audio on every write. Instead the volume (FAT12, 63.5 MiB, 16 KiB
  clusters) is generated when it's read: the WAVs are decoded from the tapes' ADPCM as the computer reads them, at
  22,050 Hz mono 16-bit. Each file gets a 2 MiB region, so a bigger file written over it still lands in one run.
- **Writing is best effort, and I say so on the drive.** A drive never hears "here is a file called X": it sees
  sectors. A sector that starts with a WAV header starts a capture; the sectors after it (and wherever the FAT
  chains the file next) are converted as they arrive (any rate, 8 to 32-bit PCM or float, any channels, to the
  tape format) into the shared memory (it was a 36 KB RAM inbox until "Memory by usage"). The directory entry that points at it names it: `TAPEn.WAV` replaces track n's
  tape; `USERn.WAV` replaces your reel n; any other name goes to the first free user reel, named after the file;
  a file never named in the root (dropped into a folder) goes to a free reel after 3 s. The FAT and directory the
  computer writes are kept in RAM and read back as written, so its view holds until you eject.
- **Your reels live in flash** (`firmware/src/reel.c`): six slots of 40 KiB in Felucca's user-sample area, in the
  tape's format, played straight from flash. A slot's header is written last as the commit record, so a save torn
  by a power cut leaves the slot empty, never half old and half new. TAPE 3's REEL lists them by name.
- **USB:** a Mass Storage interface (SCSI, bulk-only) on EP3, where the serial console was. The console needs EP3
  too, so it's now a debug build (`BRYO_MSC=0`). The transport is `firmware/src/msc.c` (no registers, tested with a
  RAM disk in `tests/usb_msc_test.c`); `usb.c` adds the descriptors and the EP3 glue. The device presents audio +
  MIDI + the drive with device class 0 and its own bcdDevice (3.x9), so a computer doesn't reuse what it learned
  about another layout. The update loader doesn't build any of it and stays byte for byte as it was.

What to expect:

- **Speed:** one 64-byte packet each way per poll (2 kHz): about 128 KB/s, so a 3.3 s WAV copies in a few seconds.
- **New files appear after you eject and plug back in.** The drive doesn't tell the computer its contents changed.
- **Deleting a file in Finder or Explorer doesn't delete the sound.** Clear it on the FM-1 (POLY held).
- **A WAV is kept as long as free memory allows (21.5 s at most as a user reel); stereo is mixed to mono.** That's
  the tape's format. (It was cut to 3.3 s until "Memory by usage".)
- **Saving a user reel stutters the sound for about a second** (the flash turns interrupts off while it writes),
  and the screen says SAVING.
- **What can still go wrong:** a WAV scattered across an old, full volume whose FAT is written after its data can
  arrive garbled. Copying onto a freshly plugged-in drive avoids it.

Recording from the computer (USB audio in) is next, then a Bluetooth feasibility check: the radio's stack is
JieLi's closed binary, and linking it into GPL-3.0 firmware is a licensing question to settle before any code.

## SYNTH, as built (2026-10-07)

SYNTH is the first source engine after TAPE, and it's what made `source_t` real.

**Files:** `synth.c` (the voice), `source.c` (the table), `param.c` (`SYN_P`, `tp[t].src`, `tp[t].syn`,
`dev_p` / `dev_v` / `dev_name`), `chain.c` (`chain_source`: the crossfade and REC), the pictograms in `ui_px.c`,
the picture in `ui_viz.c` (`viz_synth`), the source picked in `ui_input.c`.

**The voice.** It's Felucca's ANALOG engine, trimmed. There are two band-limited oscillators of one wave (SIN TRI
SQR SAW, and PLS, a 25 % pulse), the second DTUN cents up and blended in by MIX, plus white noise (NOIS) and drive
(DRV, up to 3x into the soft clip). Then comes the resonant trapezoidal low-pass, and one ADSR that sets the level
and, by ENV (-100..100), moves the cutoff up to 96 steps of the cutoff scale (about 8 octaves). KTRK makes the
cutoff follow the keys. I kept one envelope because two would need a page I don't have. ENV going negative gives
the "closing" sweeps a second envelope is usually for.

**Pages** (HOME steps through them, as EDIT and FX do theirs):

| Page | KNOB 1 | KNOB 2 | KNOB 3 | KNOB 4 |
| --- | --- | --- | --- | --- |
| OSC | WAVE | DTUN (ct) | MIX | NOIS |
| FILTER | CUT (shown in Hz) | RES | ENV | KTRK |
| AMP | ATK | DEC | SUS | REL (shown in ms / s) |
| VOICE | VOIC (1..3) | GLID | TUNE (st) | FINE (ct) |
| LEVEL | DRV | | | LVL |

The times and the cutoff print in real units. A new pair of formats, `F_TIME` and `F_HZ`, read Felucca's
`TIME_MS_X10` and `CUTOFF_HZ`, so "392Hz" on the strip is the frequency the filter really sits at.

**Voices.** There are three per track (`SYN_NV`). A new key takes a silent voice, else the quietest releasing one,
else the oldest. A stolen voice keeps its phases and filter state and attacks from where its level is, and every
block ramps the level from the last block's, so neither a steal nor a fast envelope clicks. With VOIC at 1 it
plays legato. A second key glides the one voice there (GLID), and letting the top key go glides back to the
highest key still down.

**Keys.** White key k is the semitone k above C of the track's octave (OCT- / OCT+), plus TUNE. That's sixteen
semitones, because the black keys are the PRD's macros. The VOICE page prints the range the keys cover ("C3-D#4").
With the project's SCAL on (the mixer's KEY page), a key outside the scale plays the nearest note in it; FINE tunes
the whole voice in cents ("KEY, ROOT and FINE, as built").

**Picking it.** Hold HOME and turn SELECT, the same idiom as a modulator slot's engine. Each track keeps both
TAPE's and SYNTH's knobs, so switching back and forth loses nothing.

**Switching never clicks.** `chain_source()` keeps a gain per source per track and ramps the old one out and the
new one in over about 2 ms. A source that isn't the track's renders nothing. A synth note held across a switch
waits, and releases the moment the track switches back and the key is seen up. I tried "keep rendering until it's
silent" first. It meant a TAPE track switched away kept decoding its loop forever while the transport ran, unheard.

**REC on a SYNTH track prints the synth onto its tape.** The tape renders last in `chain_source()`, so it can
take the sum of the other source as its input: play a line, switch to TAPE, and slice it. (On a TAPE track REC
still records the other tracks.)

**A fix to the tape it uncovered.** Recording a sine through the synth showed a click at every 256-sample block
boundary of the take. `tape_commit()` re-encoded a recorded block starting from the decoder state of the sound
that was there before: a different level and step size, so each block began wrong until the encoder caught up.
Now a re-encoded block starts from its own first sample, with the step size of its first move. Phase 2's REC had
the same bug (its test only compared block peaks, so it passed). The synth test now checks the take's pitch to
under 1 Hz, which the bug made 240 Hz instead of 220.

**Cost.** I haven't measured it on the FM-1 yet. Per voice per sample there are two oscillators, a noise step
when NOIS is up, and one filter, with coefficients once per block. That's the same work as a Felucca ANALOG
voice, and Felucca ran eight of those under its 85 % shedding line. Twelve voices (three per track, all four
tracks on SYNTH) is more than that, so `chain_shed()` will need to take the oldest releasing voices once it's
measured. RAM: 44 bytes a voice, 576 bytes for all four tracks.

**Tests** (`tests/bryo_host.c`, `test_synth`): picking the source and its pages; silence without a key; A3 from
white key 10 at OCT 3 (220.0 Hz) and A4 at OCT 4 (440.0 Hz); the release to a free voice; three keys, three voices,
and the fourth taking one; VOIC 1's glide up and back; the filter darkening a saw; REC printing the note onto
the tape (220.0 Hz back off it); and the crossfade from SYNTH to TAPE with no step bigger than the note's own.
The five screens are in the golden set.

## POLY, as built (2026-10-07)

POLY is the third source engine: a sound played across the keys, up to four notes at once.

**Files:** `poly.c` (the voices), `param.c` (`POL_P`, `tp[t].pol`), `source.c` (its row), `tape.c` (`tape_view_of`
and `tape_name_of`: a view of any reel choice, not only the one the track's TAPE plays), `dsp.c` (`tsvf_mode`: the
SVF's band-pass and high-pass outputs), `ui_px.c` (its pictograms), `ui_viz.c` (`viz_poly`, and `vz_filter`, which
SYNTH's filter page now shares).

**What it plays.** REEL is the same list TAPE's REEL is: the track's own tape, the factory reels, your reels. A
voice reads straight from where the sound is: flash through the XIP window, or the RAM tape. So POLY copies nothing
and costs no tape RAM, and choosing TAPE means you can record a phrase onto the track and then play it as an
instrument. Each voice has its own 256-sample block reader (tape.c's), 520 bytes each, 8 KB for all sixteen.

**Pitch.** The key ROOT names (A3 at first, BELL's note; it was C3, white key 1 at OCT 3, until ROOT came) plays
the sound as it was recorded: the head reads 0.5 tape samples per output sample, as TAPE does at 100 %. Each key
above is a semitone higher (the step comes from `pitch_inc`, so it's exact to the table). FINE tunes it in cents,
TUNE moves the whole range; the SAMPLE page prints the range ("C3-D#4"). ROOT and FINE came later ("KEY, ROOT and
FINE, as built").

**Each voice:** starts at STRT, runs the ADSR (the same block-rate envelope as SYNTH's), and passes a
state-variable filter: TYPE LP, BP or HP, with ENV moving the cutoff by the envelope. With the filter wide open as
LP (the default) it's skipped, so an untouched POLY plays the sound bit for bit. At the sound's end a voice fades
over one block and frees itself, even with the key held. Voices are taken the way SYNTH takes them.

**Pages:**

| Page | KNOB 1 | KNOB 2 | KNOB 3 | KNOB 4 |
| --- | --- | --- | --- | --- |
| SAMPLE | REEL | STRT | END | TUNE |
| ENV | ATK | DEC | SUS | REL |
| FILTER | CUT | RES | TYPE (LP BP HP) | ENV (-100..100) |
| LEVEL | VOIC (1..4) | ROOT | FINE (ct) | LVL |

**Tests** (`test_poly`): a 440 Hz sine saved as a user reel plays at 440.0 Hz on key 1, 659.2 on key 8 and 880.0 on
key 13; four keys take four voices and a fifth takes the oldest; VOIC turned down releases the voices above it;
STRT 50 starts half way, and a held note plays to the end and frees itself; LP and HP each take the sine down; REEL
TAPE plays nothing while the tape is empty and the sound once it holds one. Three screens are in the golden set.

**A budget problem it brought into view.** The pool now holds 282 KB of its 344 KB: the tapes (152 KB), the screen
canvas (59.5 KB, twice what the budget table planned), the drive's WAV inbox (38 KB, which the plan didn't have)
and POLY's readers (8 KB). That leaves about 62 KB, and phases 3b to 5 plan about 151 KB more (RIPPLE, SPACE,
GRAIN, the USB audio ring). Something has to give before RIPPLE lands. The candidates are: the inbox sharing the
canvas's memory (it's only in use while a WAV arrives), a shorter SPACE delay (the plan's first choice), and a
smaller canvas.

## The screen as dots (2026-10-08)

Bryo drew every screen strip into Felucca's full-colour canvas: 240 x 124 pixels of RGB565, 59.5 KB of the pool.
But every Bryo screen is a grid of 2 x 2 px dots in three shades. I checked all 58 renders: four colours in all,
and every dot one colour, except the UPDATE MODE countdown, whose lower half sat one pixel off the grid. So the
canvas stored 16 bits a pixel to hold 2 bits a dot.

**Now:** `ui_px.c` keeps the strip as dots, 2 bits each: background, dim, ink, and a spare fourth colour (the last
other colour drawn). `px_blit` turns them into the LCD's pixels only as they're sent, two dot rows at a time into
one of two small line buffers: one fills while the other goes out (the LCD has one transfer in flight at a time,
so the buffer being filled is never the one being sent). The dots take 1.6 KB, the line buffers 3.8 KB, both in
main RAM. `px_begin(h)` and `px_blit(y)` replace `cv_begin` and `cv_blit`; strips must start and end on even rows.

**What changed on screen:** nothing on the 57 page screens: their renders are byte for byte what they were. UPDATE
MODE's two strips are now 106 + 108 rows, so its lower line lands on the grid (one pixel up). Felucca's
anti-aliased type is gone from the last screens that used it: HARDWARE CALIBRATION is redrawn in `ui.c` on the dot
grid, and the one-line screens (boot, UBOOT, the crash screen, the update's progress) keep their calls and rows,
but `draw_text_line` and `draw_text_box` are Bryo's own dot versions (`GFX_DOT_TEXT`). I didn't touch `ota_hw.c`:
the update path calls the same function names it always did.

**Memory**, measured on a 32-bit build: the pool went from 281.6 KB to 222.1 KB (122 KB free of 336), main RAM from
23.0 KB to 27.2 KB (71 KB free of 96). Together about 193 KB is free against the ~151 KB phases 3b to 5 plan, so
the SPACE delay can stay 16-bit; storing it in 8 bits (32 KB back) stays in reserve.

**Tests:** the canvas clips at its edges, keeps 2 bits a dot and reaches the screen as 2 x 2 pixel dots in the
right colours; the calibration, boot, update and crash screens are in the golden set.

## GRAIN, as built (2026-10-08, the live buffer)

GRAIN is the first device after the source to make sound (`grain.c`). It's the S-4's MOSAIC: grains of a live buffer
of what the source plays.

**The buffer.** My first GRAIN read the track's tape, to save the 22-33 KB a live buffer costs a track. That made
SYNTH and POLY second-class: you had to print them onto the tape before GRAIN could touch them, and there was no
time-stretch of what you'd just played. With the shared memory ("Memory by usage") a buffer only costs anything
while GRAIN is on, so now it keeps the last few bars of the source: the tape as it plays, or the synth as you play
it. It's in the tape's own format, so grains read it the same way, and it's in whole bars:

| TRACKS | Bars | At 120 BPM |
| --- | --- | --- |
| 3 or 4 | 1 | 2 s, 11 chunks |
| 2 | 2 | 4 s, 22 chunks |
| 1 | 4 | 8 s, 44 chunks |

Never past 12 s, so a slow tempo halves the bars until it fits (4 bars at 40 BPM would be 24 s: it holds 2). The
write head starts on the bar line when the transport starts and wraps at the buffer's end, so the buffer is always
whole bars. It records while the transport plays, and always on a SYNTH or POLY track, because you play those without
the transport. A buffer is sized while GRAIN is on (WET above 0) and SCAN reads it; WET 0, SCAN TAPE or a parked
track gives the chunks back. When nothing is free it takes them from the end of the longest tape, like any device
switched on.

**FDBK** (the S-4's FEEDBACK) is how much of the buffer stays as new sound goes in: 0 keeps only the last bars, 100
layers forever. That's also what makes switching the source smooth: the old sound stays in the buffer until the write
head passes, then fades at FDBK each pass, so TAPE to SYNTH crossfades in the grains over a bar instead of cutting.

**Freeze** (the 0 black key: held, while it's held; tapped, latched until the next tap, its LED lit, as a tapped GLO
latches the mixer) stops every buffer recording and holds its bars, and the grains go on playing
them: a loop locked to the tempo. Change the tempo while frozen and the loop's pace follows. It's a transition tool:
freeze, switch the source or mute, let go to carry on.

**SCAN** (page 3, with WARP, OFST and FDBK beside it: set it up left to right) picks what grains read and where they
start, the cursor:

| SCAN | Reads | The cursor |
| --- | --- | --- |
| TAPE | the track's tape, inside TAPE's loop window (no buffer, no memory) | moves at WARP's speed (100 %: the tape's pace); frozen, it stops |
| STR | the buffer | moves at WARP's speed against the write head's: 100 keeps a fixed distance, lower falls behind and stretches time |
| POS | the buffer | stays at OFST of it (0: the bar line) |
| DLY | the buffer | trails the write head by OFST of the buffer: grains of what played that long ago |

STR is the default. On the GRAIN page the white keys move the cursor to their sixteenth (of the tape's loop, or of
the buffer, where a bar's sixteenths are its sixteenth notes); in POS and DLY a key sets the spot or the trail until
OFST is turned again.

**A grain** starts around the cursor (SPRY: up to half the window either way), lasts SIZE, and reads at PTCH plus a
random +-PRND semitones, held to SCAL's scale. REV is the share that plays backwards. CONT shapes the window, from
square with 2 ms ends to a full Hann. SPRD places it in the stereo field, and its level is 1 / sqrt(the expected
overlap). RATE (1..80 a second, square law) and PATN (EVEN, SWNG, CLST, RND) time the starts. Grains start while the
transport plays, on a SYNTH or POLY track, while frozen, or while a white key is held on the GRAIN page.

**WET starts at 0**, so a track sounds as before, and takes no memory, until you turn it up.

**From GRAIN on, the track is stereo** (SPRD needs it). `track_rt[].last`, what the other tracks' REC hears, stays
mono (the two sides' average).

**Reading without a copy.** Each grain decodes the ADPCM itself into a 128-sample window, starting from the stored
decoder state of the window's block. A forward grain slides its window and decodes each sample once; a backward one
re-decodes from its block's start at each refill, which is why the window is 128 samples rather than 64.

**A pool of grains, shared by what each track asks** (changed 2026-10-08; it was 32 in four fixed groups of 8, so
16 a track at most). How many grains may sound at once now follows how many tracks have GRAIN on (WET above 0, in
use): **40 for one, 30 for two, 18 for three or four** (since 2026-10-11, when the grains went to half rate; it was
32, 24, 16). Several dense clouds at once are the expensive case, and one dense cloud the musical one (a freeze, a
transition, a pad), so the pool shrinks as more tracks use it and a track alone can have all 40.

The pool is shared by need, every control block (`gr_plan`). A track's need is the grains a second times SIZE (what
overlaps), plus half again and two for the bunching PATN and chance make. A track asking less than an even share of
what's left gets what it asks, round by round; the rest is split evenly among the tracks that want more. So a dense
pad (asking 62) beside a sparse grain track at the defaults (asking 3) keeps 27 of 30, and only two dense tracks
thin each other (15 and 15). Any grain slot serves any track now; the shares keep the total inside the pool.

The need is only who gives way when the pool runs short, not a cap (2026-10-11, from playing it: one track alone
read "5 OF 9 GRAINS", as if the other 31 were locked away). When every track has what it asks and some is left,
the rest is split evenly among them all, so a track alone always has the whole 40, and two light tracks of 30 have
8 + 10 and 3 + 9. That costs nothing when nothing asks for more: RATE and SIZE still decide how many start, the
share only decides how many may sound. Where it pays is the bunching (CLST's three close grains, RND's short gaps,
a SEQ on SIZE), which the need's headroom guessed at and the leftover now covers; and the screen's "OF" is the real
ceiling.

**GRNS: a track holds a number (2026-10-11, from playing it: "can I just control the amount of grains on the track
and it takes away from others? And lock it so it stays the same").** One knob does both, GRAIN 6's GRNS: AUTO (0, the
default) shares as above; 1 to 40 holds that many. `gr_plan_into` now runs the sharing twice through one helper
(`gr_split`, the round-by-round rule above, returning what's left): first the held tracks, each asking its number,
then the AUTO tracks, by need, out of what the held ones left; whatever is still over goes to the AUTO tracks only.
So a held track gets exactly its number while the pool has it, whatever comes and goes (its cloud doesn't thin when
another track's GRAIN comes on), and never more, which is the lock. The pool itself still shrinks with the tracks in
use (40, 30, 18): that's the CPU, and a number can't buy past it. More held than the pool: the held tracks share
it by the same rule (12 and 20 of 30: 12 and 18; 40 and 40: 15 and 15), and the AUTO ones get none ("TAKEN: NO
GRAINS"). A track that wouldn't be heard (muted, stopped, silent) holds nothing, as before. GRNS is a choice the
plan acts on, not a modulation target (`nomod`). The page reads "5 OF 12 HELD" for a held track, "GRAINS" for AUTO.
No per-sample cost: the plan is per block, a few compares more.

Why one knob and not an amount plus a LOCK switch: a number that's kept whatever happens already is the lock, and
AUTO is the unlocked state, so a second control would only make combinations that mean the same. Why a 6th page:
GRAIN's 20 knobs were full (see "Six pages (NPK 24)"). (GRNS is now GRAIN 6 alone: ROUT moved to page 1, below.)

When a share drops (another track's GRAIN comes on), nothing is cut: the grains sounding finish, and new ones are
skipped until the track is inside its share, so a cloud thins over one grain's length. A grain's level counts only
the grains its track may sound (1 / sqrt of the smaller of what overlaps and its share), so a thinned cloud doesn't
also get quieter. GRAIN's page says "5 OF 12 GRAINS": what sounds and the share now. Shedding shrinks the pool a
quarter at a time down to half, and a second without it gives an eighth back. TRACKS no longer decides the split,
except that a parked track's GRAIN isn't on.

What it did to the load (`tests/checkpoint_sim.sh`): dense grains on all four tracks went from 3,304 to 2,318 (16
grains instead of 32), everything on all four from 5,842 to 4,859; the realistic setups (runs 18 to 22) didn't
change, as none of them reaches the pool. The case that grew is one track alone at 32 grains: 2,976 (run 23).

**Cost, measured** (instructions per output sample for the whole chain, four tracks, the host's 64-bit build under
callgrind; a ballpark for the FM-1, not its cycles):

| All four tracks | Per sample |
| --- | ---: |
| GRAIN off (tapes only) | 856 |
| WET 100 at the defaults: about 2 grains a track, each buffer recording | 1,528 |
| WET 100, RATE 100, SIZE 500: 8 grains a track, the cap | 3,238 |
| the same, every grain backwards | 3,961 |
| the same with SCAN TAPE (no buffer to record) | 2,869 |
| TRACKS 2: two tracks of 16 grains | 2,598 |

Recording a buffer costs about 100 a track: each tape sample is decoded, mixed with FDBK and encoded again. The
first GRAIN cost 9,760 at a cap of 16. A forward grain re-decoded its whole block every time it left its window, and
the window's shape was a table read per sample. Now the window slides forward, the envelope is taken once a block
and ramped, and the grain's state sits in locals for the block. Whether the cap holds on the FM-1 needs the
hardware: the table above is the case where it matters.

**The picture shows grains, not the sample.** Across is where a grain reads; up and down is its pitch, with the
sound's own pitch dotted through the middle and two octaves to each edge, so PTCH, PRND and SCAL move grains up and
down. A grain shows up only while it sounds. It covers what it reads, so a grain slowed down by PTCH is narrower and
a sped-up one wider. Its height is its window (CONT) swelled by the level there, the played part is solid and the
rest dim, and its playhead is an arrow pointing the way it runs (REV: right to left). With nothing sounding, a dim
outline of one grain from the knobs sits at the cursor.

- With the buffer (STR, POS, DLY) the field is the whole buffer, a measure or a few: the bar lines dotted, the beats
  ticked, the write head a solid line while it records (gone while frozen), the cursor the triangle on top, and the
  sound's level along the bottom. At WET 0 it shows the bars it will hold, empty.
- With SCAN TAPE the field is the stretch of the tape the grains can reach, zoomed around the cursor (SPRY's scatter
  plus a few grains' length, never less than 256 samples), SPRY's reach dotted under it. My first picture drew the
  whole loop window, and at that scale an 80 ms grain was a dot or two wide.

**Tests** (`test_grain`, `test_grain_buffer`): a grain's decoder against the tape's reader, both ways; one grain's
output sample for sample; pitch inside a grain (440.2 Hz at PTCH 0, 880.2 at +12, 440.2 backwards); WET 0 changes
nothing; SPRD 0 centred, 100 spread; the cap and shedding; WARP; SCAL MAJ with PRND 12; the GRAIN page's keys; an
empty tape. The buffer: its bars by TRACKS and tempo (and the 12 s limit), none at WET 0 or SCAN TAPE, the write head
on the bar line, the beat recorded; switching TAPE to SYNTH carries the beat until the write head passes; FDBK 50
halves it a pass; freeze holds the buffer while grains loop it, and the loop's pace follows the tempo; POS and DLY
cursors, a key's spot and OFST taking over again; SYNTH played with the transport stopped, recorded and granulated;
16 grains with TRACKS 2, 16-8-8 with 3; a buffer taking from the longest tape when nothing is free.

**Drums through the buffer (2026-10-12).** On the FM-1, GRAIN on the factory scene's drums sounded noisy. I rendered
T1 alone with GRAIN off and on and found three causes, all in how the buffer is recorded and read; they're fixed:

- *Clipping.* Inside the chain a source runs above full scale (the scene's drums peak near 52,000), and the buffer's
  16 bits clamped every kick and snare (about 420 samples in 10 s). It now records at half level and a grain of it
  plays at twice (`gr_record`, `gr_start`); a block's stored peak is the sound's, so the screen and the silence
  skipping see what they did.
- *Aliasing.* Halving the rate by averaging pairs left the hats' top end folded into the buffer at -16 dB of it, as
  hiss. A half-band low-pass (-1 0 9 16 9 0 -1)/32 takes it to about -23 dB and folds what's left to the top
  instead of the middle. Three multiplies per buffer sample.
- *The seam.* In STR the cursor started on the write head at PLAY and kept pace with it, so grains kept starting
  where the newest sound meets the bars-old, and the seam's duck faded them in and out (up to a tenth of grain blocks
  in a second). Now, once enough is recorded, the cursor keeps a grain's length plus SPRY's reach plus two blocks
  behind the head (at most half the buffer); 28 ducked blocks in 12 s instead of about 2,700.

The rest of the noise is the 4-bit ADPCM (about -23 dB on drums, louder just after a hit); that's the price of the
long buffer in little memory, and I left it. Turning WET to 0 also lost a fade's tail: the dry ramp ended a step
short and the grains' fade finished after the 2x up's three-sample delay; both now land at the block's end.

## RIPPLE, as built (2026-10-08)

RIPPLE was called RESONATOR until 2026-10-13. The old name said what it was, a resonator, not what it does to a
sound; and the S-4's device is RING, so neither name was a quote. RIPPLE names the picture (strings drawn as decaying
ripples) and the slow pitch movement knob I added the same day. The module is still `reso.c` and the enum is still
`RP_*`: renaming files costs a diff of every include and buys nothing you can hear.

RIPPLE comes after GRAIN (`reso.c`): four tuned strings the track rings through, the S-4's RING with the PRD's
strings. Its picture was drawn before it made sound, with the mappings written next to it, so I built the strings to
those mappings: what the panel draws is what you hear.

**A string** is a Karplus-Strong loop: a delay line one period long, a one-pole low-pass inside the loop, and a
loop gain. TONE is the low-pass (0 dark, 100 bright, nothing taken off). DEC is the ring time to -60 dB, from 80 ms
at 0 to 20 s at 100, and the loop gain comes from it per string, so a low string and a high one ring for the same
time. The low-pass delays the loop a little, which would pull every string flat, so the delay line is shortened by
exactly that: A3 measures 220.0 Hz.

**The four strings** sit on the first four partials SCAL keeps above the root, the same table the picture draws from:

| SCAL | Strings above the root |
| --- | --- |
| HARM | harmonics 1, 2, 3, 4 (each string adds its own overtones, so the whole series rings) |
| MAJ | root, major third, fifth, octave |
| MIN | root, minor third, fifth, octave |
| PEN | root, second, third, fifth |

**What rings them** is the track's sound after GRAIN, through a filter first: CUT, RES and TYPE (low-, band- or
high-pass) on the same state-variable filter POLY uses. The filter is skipped while it's open, so it costs nothing
until you turn it. How much of the sound goes in falls as DEC rises: a long ring builds up louder from the same input,
and this keeps it from building without bound (the loop clamps besides). WET blends the strings with the track,
ramped across each block. The strings sit across the stereo field, 1 and 3 left, 2 and 4 right, two to one.

**The keys on the RIPPLE page** set the root: a semitone a key from C of the track's octave, so OCT-/OCT+ shift
it, and it holds until PTCH is turned again (the way GRAIN's keys hold a spot until OFST is turned). On a TAPE track
a key also plucks the strings, a burst of noise one period long, softened by TONE. That's the PRD's sound from
nothing: a blank tape, RIPPLE and REC. On a SYNTH or POLY track the keys keep playing the source and the strings
take its notes as their root, so the resonance follows what you play. The picture shows a key's root as PTCH's.

**Memory**: a string's delay line is a chunk of the shared memory (1,056 samples, so the lowest string is F1,
43.7 Hz), four a track, taken while WET is above 0 on a track in use and given back otherwise, like GRAIN's buffer.

**Cost, measured** (host instructions per output sample, the whole chain, four tracks): the reels alone 873;
RIPPLE at WET 60 on all four (16 strings) 1,818, so about 950 for the strings, around 50 a string with its share
of the blend. My budget said 60 a track; it's nearer 240. The first version cost 1,160: a division per sample in the
blend, the input's share worked out per string, and each string summed into both sides; now the blend is Q15, the
share is worked out once per track, and the strings are summed once per side. With GRAIN at its cap too: 4,416. That
pair is the number for the FM-1 to settle.

**Shedding** takes strings after grains: once GRAIN's allowance is down to half, each shed takes a string (down to
two), and a second without one gives a string back.

**Tests** (`test_reso`): no strings and no memory at WET 0; four chunks at WET up; a key on a TAPE track plucks at
its note (A3 at 220.0 Hz, measured by autocorrelation so the partials don't fool it); DEC rings longer as it rises;
SCAL MAJ puts the strings on the third and fifth; turning PTCH takes the root back; a reel through DEC 100 and TONE
100 rings loud and never past full scale; on a SYNTH track the keys play the synth and the strings follow; shedding
takes a string and gives it back; a parked track holds no strings; WET 0 gives the chunks back. And
`tests/checkpoint_sim.sh` renders and measures RIPPLE on all four tracks (runs 11 and 12).

**Silence is silent (2026-10-11, from playing it: "a background noise when everything is turned off ... noise stays
even when play is paused").** I traced it on the host by playing a reel through each device in turn, stopping, and
looking 20 s later at COLOR's NOIS envelope (NOIS follows its input's level, so any residue holds it open: it's the
most sensitive detector there is). Three integer residues and one oscillator:
- **The strings** (`rs_settle`): a right shift rounds down, so near g = 1 (DEC high) a string a little below 0
  never shrank: `(lp * g) >> 15` gave lp back for |lp| under about 32768 / (32768 - g). Four strings stuck that way
  left an offset of about 740 (-33 dB). The master's DC filter hid it from the output, but COLOR's noise opened on
  it: the steady hiss. On a silent block the loop now rounds toward 0, its gain and its damping's new value (not the
  step: a step of -0.9 rounded toward 0 is none, and a string above 0 stalls instead). Under sound the round-down is
  lost in the signal, and the toward-0 loop costs about 8 more a string a sample, so it runs only on silent blocks:
  +14 to +18 a sample on runs 11 and 12, where everywhere was +185.
- **The filter in front** at RES 80-90 held a residue of a few dozen for good; DEC 100's strings rang that up to
  -16 dB with nothing playing. On a silent block with the state under 256 it's let go to 0.
- **GRAIN's BODY TONE low-pass** had the same round-down (an offset of 12, rung up to 935 by the strings): its new
  value rounds toward 0 now (once a block per sample at the half rate, a few instructions, only when BODY is on).
- **BODY's TRI and SIN** droned on with the transport stopped: an oscillator with nothing gating it. It has a gate
  now, open while the track's grains sound (`any` in grain_block), shut about 0.3 s after them. SRC needs none.
Tested (`test_settle`): each case, 20 s after the sound stops, has every string, the filter and BODY's low-pass at
exactly 0 and COLOR's noise closed; BODY TRI's gate and level at 0. Each fix fails its mutant (the old rounding, the
filter kept, the low-pass floor, no gate, the settling loop never chosen). SPACE at FDBK 100 still repeats forever,
as a delay at full feedback should; at 90 it dies away to 0.

**Colour on the screens (2026-10-11, from a conversation about delight: "throw in color for extra state info, and as
delight in grains, resonator, reverb, arp ... show it for activity ... and a color to show modulator in their
bars").** The screens stay white on black, the OLED look; colour says where you are and what's alive, and nothing
else (no per-track colours: they'd compete with the page's). Five accents (`ui_px.c`), all from Okabe and Ito's
colour-blind-safe set, all 4.5:1 or more on black so they work as text:
- sky blue #56B4E9 (9.1:1), the blue pads' pages: the title and its page dots (`focus_btn`);
- reddish purple #CC79A7 (6.9:1), the purple pads' (LFO SEQ ENV ARP) pages, and modulation wherever it shows: a
  knob's corner mark, its rail's reach and live block, the depths while a slot pad is held;
- orange #E69F00 (9.3:1), alive: a grain's played stretch, a ringing string's stem and node, SPACE's tail and echoes
  while it sounds (`space_sounding`: not resting, |wet| past -66 dB within 0.1 s), the ARP note and the SEQ and DRUM
  step playing, TAPE's running playhead, the mixer's meters;
- vermillion #D55E00 (5.4:1, black on it the same), recording: the tempo bar turns red (the dot stays in it) and
  TAPE's REC tag; the CPU reading at 85 % or more;
- yellow #F0E442 (15.9:1), something to know: the unsaved ✱ (moved out of the white tempo bar, where yellow can't
  be read, to just before it), FROZEN, "HELD".
None of them is the only cue: the dot says recording, the ✱ is a shape, a played grain is solid where the rest is
dim, a ringing string gets a stem, the step playing is the filled one (and its key lights), a modulated knob has its
rail. To have the colours at all the canvas went from 2 bits a dot to 4 (`pxc`, 16 colours a strip, given out as
drawn; a 17th would fall back to ink): the header alone needs three. 1.6 KB more RAM, the same work a pixel to send.
RIPPLE keeps a level a string for the picture (`lvl`: a block's last loop sample held, falling an eighth a block;
+5 to +14 a sample on runs 11, 12 and 30). The screens' signatures carry what's coloured (the focus pad, the strings
ringing, SPACE sounding, the SEQ step), so a change redraws. Tested: the canvas test for 4 bits and the 17th colour,
`test_resting` for what the screen reads as sounding and resting (and two pictures, `space_live` and `reso_live`), the
goldens for every screen.

**Resting, and what shedding gives back (2026-10-11, from playing it: "CPU is sitting at 50 % with nothing playing";
"when I mute other tracks ... the cpu actually seems to go up to 85 % and audio crackles").** I rebuilt both on the
host (runs 29 and 30) and measured where the time went. Three causes:
- **Everything worked on silence.** Stopped, every device still ran on digital zeros: SPACE 553 a sample, the
  channel filters 453, the strings, COLOR. Now each one rests when its input block is digital silence and its own
  state shows its tail has gone, and wakes the block sound comes back, carrying on from its state as it is:
  - SPACE: |wet| <= 16 (-66 dB) out for longer than its lines are long (the delay's, and the pre-delay plus the
    longest comb and the allpasses), then its lines stand still; its loops' filters are let go to 0 as it rests.
  - RIPPLE: the strings' sum |out| <= 2 for longer than a line (1,056 samples), then the strings stand still; a
    pluck or a RIPL ARP step wakes them.
  - COLOR: the noise shut and nothing held in CRSH or TILT (|s| <= 16).
  - The channel: no gain moving and every filter state under 512.
  The thresholds aren't arbitrary: in integers SPACE's combs cycle at about ±16 and the channel's FILT at a few
  hundred (-55 dB out) for good once a tail is gone, and a low shelf holds a few dozen. Those cycles were a faint
  sound of their own with nothing playing, and they'd keep a device awake forever; resting puts them to 0.
- **A muted track cost as much as a heard one.** Mute is a cut after the devices, so they ran on. Now a muted
  track's devices after GRAIN hear silence (`chain_block`): they ring down and rest, and unmuted they start from
  rest on the track's sound. GRAIN itself keeps recording its buffer (unmuted, its grains have the recent bars).
  *Changed 2026-10-12: a muted track sleeps.* From playing it: 70-75 % with one track heard and three muted. The
  three still rendered their sources and GRAIN, about a third of the work, all of it zeroed. Now a muted track that
  isn't recording sleeps (`src_sleep`, `grain_sleep`): its source keeps its time and makes no sound (TAPE's heads,
  seams, slices and SYNC run as ever with nothing read; DRUM's pattern steps on and still moves the modulators; POLY
  and SYNTH follow the keys with every voice silent), GRAIN neither records nor plays but keeps its write head on its
  place in the bars, and RIPL ARP doesn't pluck. The modulators and arpeggiators run on. A mute was already a cut, so
  what changes is only at waking: notes start fresh and the buffer holds the bars from before the mute until it's
  written over. The scene with T2 alone: 42 % less work on the host. Tested (`test_sleep`): asleep 3 s, T3's tape
  head, T1's pattern step and T3's buffer head end exactly where they do awake, nothing of theirs sounds, and
  unmuted they all play again.
- **Shedding gave back blindly.** It took grains (then strings, then drum voices) after two halves over 85 %, and
  gave an eighth back a second after the last shed whatever the load: a track near the edge went over, shed, came
  back and went over again, a crackle a second. Now it also sheds on one half over 95 %, and gives back only while
  the heaviest half of the last second stays under 70 % (`sys.load_pk`, audio.c; `chain_room`). Muting the others
  gives a lone track the bigger grain pool (40), and that's what lets it have them only while there's room.
Measured: run 29 (the busy groove stopped) 1,717 to 798, about 46 % to 21 % on the device's scale; run 30 (one track
heard with dense grains, strings, RIPL ARP and a room, the others muted) 3,200 to 2,618, about 85 % to 70 %; the
playing runs -125 to +17 (the silence checks a block cost a little where nothing rests). What still runs stopped:
GRAIN recording silence into a SYNTH or POLY track's buffer (about 240 in run 29's kind of scene), the master, the
sources' clocks. Tested (`test_resting`): stopped 8 s, every device rests with the track's out exactly 0 and SPACE's
and the strings' lines standing still; PLAY wakes them at once; muted, they rest while the transport plays, and
unmuted wake; with the heaviest half at 80 % nothing comes back, at 50 % an eighth does. Each part fails its
mutant (never resting, the tail ignored, a pluck not waking, the mute feeding sound, the blind give-back).

**A pluck's shape (2026-10-12).** From playing RIPL ARP: "the resonator always sounds the same". Every pluck was the
same full burst of noise into all four strings at once, at TONE's brightness, so a pattern of them was identical
hits. RIPPLE 3's three empty knobs now shape it (`rs_pluck`):

- **PICK** (-100..100): how hard. An offset on TONE for the burst alone: below 0 a mallet (the burst low-passed
  harder, dark and round), above a pick (bright, snappy); TONE still sets how the strings ring after.
- **STRM** (0..100 ms): a strum. String 1 at once, string k after k x STRM, block by block (`rs_strum`, 0.7 ms
  steps). A new pluck drops the strings a strum hasn't reached yet, so a fast arpeggio with a slow strum just
  doesn't finish its chords, rather than piling them up.
- **VARY** (0..100 %): each pluck its own brightness (up to VARY x 0.3 points either way) and level (down to
  1 - VARY / 2), each string a little lower again, so the strings don't come in as one block.

0 on all three is the pluck as it was, bit for bit (no extra random draws, full level written as it was): projects
saved before them stored 0 in those slots. They're modulation targets like any knob. Tested (`test_reso_pluck`):
PICK's burst brighter at +60 than -60; STRM 50's strings 50 ms apart, the top one waiting until 150 ms; a new pluck
dropping a strum under way; VARY 0 full level, VARY 100 every pluck and string its own, none under a quarter.

## COLOR, as built (2026-10-08)

COLOR is the S-4's DEFORM after RIPPLE (`firmware/src/color.c`). It takes no memory, only a few numbers a track,
and its picture (`viz_color`) calls the same `color_shape()` the sound goes through, so what's drawn is what's
played. In order:

| Knob | What it does | How |
| --- | --- | --- |
| DRV | gain 1..16 into a soft clip | Felucca's tanh table. At 0 there's no clip at all, so the default leaves the sound sample for sample untouched |
| CRSH, CMOD | 16 down to 2 bits (BIT), a sample held 1..12 samples (RATE), or both | bits cut toward zero, so no DC creeps in |
| NOIS, NDEC, NTON | noise riding the sound's envelope | the envelope opens at once and falls 60 dB over NDEC; NTON is a one-pole filter on the noise. Silence stays silent |
| TILT | the low end against the high around 700 Hz | ±6 dB each way at ±100 |
| WET, LVL | dry against coloured; the output, -24..+6 dB | both ramp across a block |

I changed two things from the picture's first draft while building it. DRV's gain is now continuous (Q8) instead of
16 integer steps, since 1 → 2 was a 6 dB jump on the first detent; and the clip isn't normalised to full scale
any more, because that made DRV 1 already 2.4 dB louder. The picture changed with it.

**Load**: nothing while every colouring knob is at 0 and LVL at 0 dB (the default). With everything on, 182 host
instructions a sample a track; the first draft was 205 before I moved its state into locals (the compiler couldn't
know the sample buffers didn't alias it, so it reloaded every field every sample). That's six times my 30 estimate,
but the estimate only counted the drive: it's spread evenly across the features, so there's no single thing to cut.

**Tests** (`test_color`): the defaults leave a sine bit-exact; LVL -6 halves it; DRV 100 squares a sine (crest
1.04); CRSH 100 leaves 2 bits, RATE holds 12 samples, BOTH does both; NOIS on silence is silence, after a hit it's
there and gone by twice NDEC; NTON -100 against +100; TILT ±100 moves 100 Hz and 8 kHz the right ways; WET 0 is the
dry sound however much drive; every knob at either end with a square past full scale stays bounded.

## SPACE, as built (2026-10-08)

SPACE is the S-4's VAST, last on a track (`firmware/src/space.c`): a delay and Felucca's ROOM reverb, run at half the
rate (22.05 kHz) to halve both their memory and their load. The track goes in as mono (both sides averaged, and two
samples into one), and the wet comes back up to 44.1 kHz by stepping halfway between samples.

**The delay.** TIME (10..370 ms) back, and when TIME turns the read point glides there instead of jumping: a tape
echo's bend, about 90 ms long and never faster than 3/8 of a sample a sample (within a fifth), so there's no click.
FDBK 100 is a loop gain of 0.98. TONE sits in the feedback: below 0 a low-pass (to about 430 Hz), above 0 a
high-pass (to about 650 Hz), so each echo is darker or thinner than the last. DLY 100 puts the first echo at 0.8 of
the sound, which is the height the picture draws. SPRD moves the right echo up to 10 ms ahead of the left.

**The room.** Four damped combs and two allpasses, Freeverb's shape, as Felucca's ROOM. SIZE sets the combs' lengths
(40 % to 100 % of 38..46 ms); DEC the time the tail takes to fall 60 dB (0.2..4.2 s, each comb's gain from
RIPPLE's `rs_gain`); TONE the combs' damping, or a high-pass in front above 0; PRE the pre-delay, up to 200 ms.
For stereo, the right side runs its own pair of allpasses fed the combs with alternating signs, and SPRD blends it in
(0 is mono). (Since 2026-10-11 the rooms are two the tracks share, on the mixer, always stereo; SPRD is the delay's
alone: see "The shared rooms, as built".)

**Memory by usage.** DLY and VERB now default to 0, like GRAIN's and RIPPLE's WET: a device that's off takes
nothing. With DLY up the delay takes 8 chunks (8,192 samples); with VERB up the room takes 5 and its pre-delay 5. A
line is 16-bit when there's room for it with 3 s still free, and starts 8-bit when there isn't: 4 chunks for the
delay and 3 for the pre-delay, companded like A-law (sign, a 3-bit segment, 4 bits inside it), so the noise rides
under the sound, about 46 dB below a quiet sine's echo. When nothing is free it takes from the end of the longest tape,
as GRAIN and RIPPLE do. A line never changes format while it sounds: it's chosen when it's switched on. A parked
track holds nothing. Each line counts how many times it's been handed over, so one given back and taken again between
two audio blocks still starts from silence. The SPACE page draws a part that's off (DLY or VERB at 0) dim, as it
would be at 100, so TIME, FDBK, SIZE and DEC still show what they set.

**A bug the tests found:** the room kept ringing on its own. After the sound had gone, its output sat at a DC of -36
for good: every `>> 15` in a feedback loop rounds toward minus infinity, and four combs feeding back keep that bias
alive. Rounding to nearest still left a 28-step limit cycle. The comb's gain, the delay's feedback and the allpasses
now cut toward zero, which only ever takes energy out, so a loop with nothing in it falls to exactly 0.

**Load**: 220 host instructions a sample a track with both on (the first draft was 283; the same locals fix as COLOR).
The estimate was 80. The combs and allpasses are about 16 each per half-rate sample, the delay with SPRD's second
tap about 60.

**Tests** (`test_space`): the defaults take no memory and leave the sound alone; TIME 100 gives one echo at 100.0 ms
of 0.8; FDBK 50 a second at 200.0 ms at 0.45; TONE either way loses more each pass; SPRD 100 puts the right echo 10.0
ms ahead; TIME turned while echoing glides with no jump; with 20 chunks free the delay starts 8-bit in 4 chunks, its
echo where 16 bits put it; with nothing free it takes its 4 chunks from the longest tape; DEC 100 against 0 at
0.5..1 s; the room's sides decorrelate at SPRD 100 and match at 0; PRE 100 holds the room back 100 ms; every knob at
its top with noise in stays bounded (TONE either way, SIZE 0..100); a parked track's SPACE holds nothing; DLY and VERB
at 0 give every chunk back.

**On the checkpoint** (`tests/checkpoint_sim.sh`, runs 13 to 16, host instructions per output sample, four tracks):

| Run | What | Host cost |
| --- | --- | ---: |
| 1 | four reels, nothing on (the baseline) | 902 |
| 13 | COLOR on all four (DRV 60, CRSH 40 BOTH, NOIS 30, TILT 30) | 1,630 |
| 14 | SPACE on all four (DLY 50, VERB 40): 72 chunks, all 16-bit, 14.5 s still free | 1,784 |
| 15 | everything on all four: GRAIN at the cap, RIPPLE, COLOR, SPACE | 5,842 |
| 16 | three tracks as I'd play them: TRACKS 3, GRAIN WET 100, DRV 40, SPACE | 2,310 |

Run 15 is 6.5 times the baseline: the 2,000 target holds only for setups like run 16. That's the hardware
checkpoint's question now, more than before. If the FM-1 can't carry run 15, the shedding order (grains, then
strings) would need a step for SPACE as well (the room at a quarter rate, say), and that's a decision for when I have
the device's number. The stress run cuts the interrupts in while RIPPLE and SPACE take and give back memory as
knobs turn, and the books balance (RIPPLE's strings and SPACE's lines are in the check now too).

## The shared rooms, as built (2026-10-11)

**Why.** Playing the factory scene with one LFO on a drum's LVL put the FM-1 over 80 % and it crackled. The depth
itself costs about 19 host instructions a sample, half a percent; the scene was the problem. Measured 5 s into it
after a second's warm-up, it had crept from 2,886 to 3,046 a sample over the day's features (about 81 % on the
device's scale of 37.6 a percent), and SPACE was its biggest part, about 620: a whole room for each of the three
tracks with VERB up, and two delays. A room is Freeverb's four combs and four allpasses at 22.05 kHz whichever
track it's on, so three rooms cost three times one, and three similar rooms on a small speaker don't sound like
three rooms anyway. The middle way that was asked for: two rooms the tracks share, and a quality setting for each.

**What it is now.**

- Two rooms, **A** and **B** (`room[]` in `space.c`), each with SIZE, DEC, TONE, QUAL and PRE (`rmp[]`, `RM_P` in
  `param.c`). They're the mix's, like the master compressor: one pair for the instrument, saved with the project,
  and not modulation targets (no track's slot owns them, and a room's lengths are what the main loop sizes its
  memory for). I first put their knobs on mixer pages; asked to keep them on FX, they're on SPACE's pages now (see
  "The room's knobs on SPACE" below).
- Each track's SPACE keeps its delay (TIME FDBK DLY TONE SPRD) and sends to one room: **ROOM** (A or B) on SPACE 2,
  **VERB** the amount on page 1, next to DLY. Tracks 1-2 start on A, 3-4 on B. ROOM is `nomod` (it picks the memory).
- The send is a bus. `space_block` adds the track's mono at half rate, times VERB times the track's level (Q12: so
  the fader and mute take the track's share out of the room, as a post-fader send does), ramped across the block,
  into the room's input `R->in[]`. `chain_block` runs `space_rooms` once after the tracks, before the master
  compressor: each room on what it was sent, its tail added to the mix, its input cleared.
- **QUAL**: HI runs a room at 22.05 kHz as before; LO at 11.025 kHz. The sends are averaged in pairs first (so a high
  sound doesn't fold down into the room: tested with 10 kHz against 1 kHz), the combs' and allpasses' lengths
  halve (`sp_room`: the same times), the comb gains are worked out for the same DEC (`rs_gain` counts samples at
  44.1 kHz, so each sample counts four), TONE's high-pass coefficient doubles, and the tail steps back up to 44.1 kHz
  a quarter of the way at a time (unrolled: no multiply by j). Half the work and a darker, softer tail: it suits a
  big ambient room more than a tight one. A QUAL change restarts the room from silence, since its lengths then mean
  other times.
- The room is always stereo now: the right side's own allpass pair on a sign-alternating mix of the combs, as
  before, but not blended by SPRD any more; SPRD is the delay's (the right echo ahead).

**Memory.** A room's lines (its combs and allpasses, 5 chunks, and the pre-delay, 5 or 3 at 8 bits) belong to
`MEM_ROOM + r` and are taken while any track in use sends to it (VERB up, or modulated up, and its ROOM that room)
and given back when none does. So run 14 (SPACE on all four) holds 52 chunks where it held 72. The mixer's memory
ribbon draws the rooms after the tracks, dim, under an R where it fits.

**Resting, and a limit cycle it uncovered.** A room rests like a track's SPACE did: silence in and |wet| <= 16 out
for longer than its pre-delay plus its longest comb and allpasses (at its rate). The first run of the resting test
never rested: room A sat at -24 for good. The old room's output was scaled by VERB after it, which had hidden its
integer limit cycle under the threshold; with the send scaled before it, the raw cycle showed. Its source was the
combs' damping filter, the one `>> 15` left in the loop (it floors). With it rounding toward zero (`sp_mul`, as the
comb gains and the allpasses already did), the loop with nothing in it falls to 0 and the room rests.

**Projects and presets.** The rooms are stored after the depths (a count, then each room's knobs). A track preset
doesn't hold the rooms, and the room a track sends to stays the track's when one loads (the rooms are the mix's).
(I first wrote a conversion that made the two rooms from a version 7 project's room-per-track; it went the same day
with the rest of the old-format reading: see "Round 3" below.)

**The factory scene** sets the rooms: A small (SIZE 45: T1's kit, T2's bells), B large and long (SIZE 80, DEC 65:
T3's strings), both at QUAL LO. That last part is a choice made for the number: at HI the scene is 2,949 a sample
(78 %), at LO 2,821 (75 %), inside the 65 to 75 % the device read before. A new project starts at HI.

**The room's knobs on SPACE (later the same day).** The mixer's ROOMS pages felt one step removed: you shape a
room where you send to it. SPACE has three pages now: TIME FDBK DLY VERB / ROOM SIZE DEC PRE / TONE SPRD RTON QUAL.
SIZE DEC RTON QUAL PRE are descriptors in SPACE's table (stored places 3 and 8-11, `nomod`) that `SP_RMK` maps to
the room's knobs: `sp_knob()` hands the page (and the picture's tag) the value of the room the track's ROOM picks,
so turning one changes that room for every track on it; the track's own values in those places are never read
(the DSP reads `rmp`). The picture's label names the room, or the room and every track on it when another track
shares it ("B: T1 T2"). LFO held on one says THE ROOM IS SHARED: NO DEPTHS. Tested: a track's SIZE turns its room's and not the other's, ROOM
moves which room the page edits, QUAL is the room's, no depth on a room knob.

**What it costs.** Host instructions a sample, same harness before and after:

| | Before | Now |
| --- | ---: | ---: |
| the factory scene (3 rooms, now 2 at LO) | 3,046 | 2,821 |
| run 14: SPACE on all four (4 rooms, now 2) | 1,879 | 1,678 |
| run 15: everything on all four | 5,072 | 4,871 |
| run 16: TRACKS 3 with SPACE | 2,363 | 2,291 |
| run 20: the ambient pad (2 rooms, now 1) | 1,864 | 1,753 |
| run 18: one track in a room | 1,779 | 1,810 |
| run 21: the busiest groove (one room) | 2,498 | 2,516 |
| run 30: one track heard, a room | 2,623 | 2,635 |

A room costs about 157 a sample at HI and 95 at LO. Where only one track had a room it costs a little more than
before (12 to 31): the room always runs both sides now, the damping rounds toward zero, and every track with VERB
up runs its send. Where several did, it's one room's cost however many send.

**Tests.** `test_space`: VERB up takes room A's lines (5 + 5 at 16 bits) and nothing for B or the track; DEC 100
against 0; the room's sides decorrelate; PRE 100 ms; QUAL LO's tail as long as HI's and stepped back up smoothly, a
10 kHz send not folded down, HI's image at 18 kHz 21 dB under what it mirrors; two tracks in one room share its
memory, each alone gives the same tail, both together the sum; the send at level 0 sends nothing and at half level
half; a track moved to B makes B take its lines and A keep its own, and VERB 0 gives them back; every knob at its
top, both QUALs, bounded. `test_resting`: the room rests and stands still. `test_projects`: the rooms and each track's ROOM come back as saved, a track preset keeps the track's ROOM; the
biggest project still fits (3,748 of 3,840 bytes). Mutations each caught: the send ignoring the level, the room
never resting, the damping's floor shift back, the migration ignoring VERB, ROOM not kept by a preset, every track
on A, LO without the pair average, HI without its halfway step. The checkpoint's stress run turns ROOM and a room's
knobs (QUAL among them) while the interrupts cut in, and its books check the rooms' chunks.

## Phase 9: the strip shows modulation, as built (2026-10-09)

The PRD's phase 9 asked for modulation arcs on the knobs, moving dots and a summed value. The plan was written
before the screen became a dot grid, when every knob was going to be a dial; the pictograms are mostly not dials now
(faders, switches, small graphs), so an arc can't sit on all of them. What I built instead is one element that fits
every cell: a **rail** under the knob, in the strip's bottom rows, only on a knob something modulates:

| On the rail | What it is |
| --- | --- |
| dotted, the whole width | the knob's range |
| solid | how far its depths can take it from where it's set: an LFO both ways, ADSR, SEQ and FOLLOW the way each depth's sign points, clamped to the range |
| a tick (dim) | where the knob is set |
| a 3 x 3 block | where it is now: the value the sound gets, every depth summed. It's read from the ISR's modulated copy (`TPD`), so it's the same number the DSP uses, not a picture of it |

The rest of the strip is as it was: the pictogram and the value still show the knob as set (that's what turning the
knob changes), the corner mark still says "modulated", and holding a slot's pad still shows its depths.

**Why a band of its own.** The block moves as often as the modulation does, and redrawing the whole strip is 240 x 98
px, about 30 ms on the 12 MHz link. The rails are a separate 240 x 10 px band with their own signature (positions
quantised to the rail's 24 dots), so a moving block costs about 3 ms, and only when it actually moves a dot. When the
strip does redraw (a knob turned, a page changed), it draws the rails with it and takes their signature, so the band
isn't drawn twice. The test runs an LFO for 1.8 s: 104 block moves across the reach, the strip redrawn 0 times.

**Also in this phase:** LFO RATE with SYNC BPM prints the division it plays at (16BR 8BR 4BR 3BR 2BR 6/4 1BR 3/4 1/2
3/8 1/4 3/16 1/8 1/8T 1/16 1/32) instead of its 0..127.

**Where:** `ui.c` (`rail_of`, `rails_draw`, `rails_sig`, `draw_rails`), on device and source pages and the channel
strip; not on a slot's own page (its knobs aren't targets) or with a DRUM instrument held (its own knobs aren't
either).

**Tested** (`test_rails`): no rail under an unmodulated knob; DRV's set position and its LFO reach (10 % to 90 % at
50 % +- 40); the block following the LFO, the band redrawing and the strip not; and the six screens it changes in the
goldens.

**Not done, on purpose:** the pictures below the strip still draw the knobs as set. Drawing them from the modulated
values would redraw the 92-px panel at the modulation's rate, which is the cost the rails were designed to avoid; I'd
want the hardware's numbers before trying it.

## Projects, as built (2026-10-09)

Phase 8: a project is the whole instrument, saved to flash and brought back. SAVE tapped saves it; PRESETS opens
the project view; a power cycle brings back the project saved last.

**What a project holds:** every track's knobs on every page (source, GRAIN, RIPPLE, COLOR, SPACE, the slots'
engines and knobs, the channel strip), which source each track runs and its SYNTH, POLY and DRUM knobs, the SEQ
steps, the DRUM pattern, accents and each instrument's own knobs, every modulation depth, REC IN, each track's
level, mute and octave, TRACKS, the master compressor and the tempo. And what each tape plays: a factory reel or one
of your reels by its number. A track's own RAM tape with a take on it can't survive a power cycle, so SAVE copies it
into a user reel named after the project and the track ("P2T3"), switches the track to that reel (the same sound,
and the RAM goes back to the pool), and the project points there. The next SAVE reuses that reel if the take still
fits it, and doesn't write it again at all if nothing was recorded since. With no reel free, the project is saved
without that take and SAVE names the tracks ("P2 SAVED. NO FREE REEL: T3").

**Where (`firmware/src/project.c`):** six slots, each an A/B pair of 4 KiB sectors in the data region Felucca kept
its projects and presets in (Bryo's user reels have its sample area, so this was the room left):

| Slot | Copy A | Copy B |
| --- | --- | --- |
| P1..P4 | 0x97000, 0x99000, 0x9B000, 0x9D000 | the sector after each |
| P5 | 0x9F000 (was Felucca's FM6 bank) | 0xDC000 (was its preset area) |
| P6 | 0xDD000 | 0xDE000 |

A save goes to the copy that isn't in charge: erase it, program the payload, then a 32-byte commit record LAST
(magic, slot, copy, seq, lengths, the payload's CRC, its own CRC). Loading takes the valid copy with the newest seq,
so a save cut off by a power cut leaves the copy before it in charge; the test cuts the power 600 bytes into a save
and gets the earlier project back. The magic is Bryo's own ("BRYP"), not storage.c's "FELU": a Felucca reinstalled
over Bryo sees empty sectors instead of trying to load a Bryo project as its own (and the test puts a Felucca record
in a slot's sector: it reads as empty).

**The format:** a byte stream written and read field by field, never a struct dump, so RAM layouts can change
freely. A version byte, then every array as its count and its values; reading takes as many values as were saved and
fit, defaults the rest, and clamps each one to its knob's range. A project with more tracks than the FM-1 has is
refused before anything changes. (Since 2026-10-11 only the current version is read: projects had barely started,
so the fix-ups for each older version went; see "Round 3".) The depths are stored sparsely and
by (array, knob), not by target number, because target numbers moved when DRUM arrived and will move again. The
stream is packed with PackBits (a run of a byte, or bytes as they are), a byte at a time, straight to and from the
flash: no RAM holds a whole project, just a 256-byte page on the way out. The test scene packs to 1,945 bytes
(3,670 unpacked); the very most a project can be is about 3.8 KB, inside a sector's 3,840.

**The project view** (PRESETS turned): the slot it points at, large, with its tempo and each track's source; the six
slots below, three by two, each with its tracks' sources (T S P D) or EMPTY; the loaded one marked. OCT+ loads,
SAVE saves into it, POLY held deletes it (the user reels it saved stay: they're yours, on the drive too), OCT- or any
page pad leaves. Two things ask for a second press within 3 s: loading over changes not saved (the state is
fingerprinted when a project loads or saves) and saving over a different project. SAVE tapped anywhere else saves
into the project loaded or saved last, without asking: that's the quick save the PRD wants.

**Loading** stops the transport and lets REC go, sets everything, rebuilds the depth lists (published whole, as
always), parks tracks above TRACKS, and drops the undo buffers (they belonged to the session before). Each track's
own RAM tape is let go: what a project keeps of a tape is in its reel.

**What it costs:** 0.5 KB of main RAM (the page buffer, the packer, the six summaries; .bss was 87.2 of 96 KiB then,
81.0 after the RAM pass). A
save erases and programs one sector (well under a second of flash work) plus a user reel for each new take (about a
second each), with the sound stuttering while the flash holds the bus, as reel saves always have; the screen says
SAVING first. (Since 2026-10-11 a save that writes no reel doesn't stop the sound, and one that does stops it once,
faded: "Saving while playing" below.)

**Tested** (`test_projects`): an empty flash boots to the defaults; a scene far from them (four sources, a DRUM
pattern, SEQ steps, depths on a DRUM knob, a device and the channel, levels, mutes, octaves, TRACKS 3, the tempo, the
compressor) saves, power-cycles and comes back byte for byte, its depth lists live; a take kept in reel P2T1 and
played from it after a power cycle; a second save writes no new reel; no reel free reported per track; the power
cut; the Felucca record; the project view's every control, both questions, delete, and SAVE on a page.

**Not yet:** the device itself (the run sheet has two rows for it), names (projects are P1 to P6), and p-locks (when
they come, they're a new array in the stream: older projects just won't have them).

## Track presets and SCL's reset, as built (2026-10-10)

Two ways back from a sound that got away from me: put a knob (or a page) back to where it started, or save a track's
sound I like and bring it back on any track later.

**SCL resets.** SCL had nothing on it (the system menu was only ever proposed), so it's the reset key:

- **SCL held + turn a knob:** that knob jumps to its default. One detent is enough; the direction doesn't matter.
- **SCL held + the page's own pad** (HOME on a source page, EDIT on GRAIN or RIPPLE, FX on COLOR or SPACE, a
  slot's pad on its page, GLO on the mixer, EDIT on the channel and master pages): every knob on the page shown goes
  to its default, and the pad doesn't do its usual thing (no page turn). It's `focus_btn()`, the same function that
  lights the pad, so the "page's pad" is always the lit one.
- **SAVE held** puts back everything the hold reset: the first reset of an SCL hold takes a snapshot of the track
  (and the levels, REC IN and the master, since a reset can be on those pages) and sets the undo to it.
- What isn't reset: PATN (turning it writes a pattern over the one there, so a reset would be a pattern write;
  SCL + PATN says "TURN IT" instead) and the modulation depths (MONO clears those, so the two stay separate).
- While SCL is held the footer reads KNOB:DEFAULT PAD:PAGE.

**Track presets.** A preset is one track's *sound*, A to H, eight of them, on the PRESETS view's second page (EDIT
steps the view's pages, as it steps the mixer's):

- **What's in one:** the source and its knobs (DRUM's with each instrument's own), the four devices, the channel
  strip, the four modulator slots (engines, knobs, a SEQ's steps) and the depths.
- **What isn't:** the music and the mix. The DRUM pattern, and PATN and LEN (which belong to it), the level, mute,
  octave and REC IN stay with the track a preset is loaded into. A tape is far too big, so a preset names the reel the
  track was playing; a track on its own tape names none, and the track it's loaded into keeps the reel it's on.
- **The sources a track isn't on are left out** (stored as empty arrays, so the loading track keeps its own values
  there). That halved a preset: a default track is about 200 bytes packed, one I'd actually make about 250.
- **OCT+** loads the preset pointed at into the focused track, with no question, so I can step through presets with
  PRESETS and OCT+ and listen. **SAVE held** puts the track back as it was before the last load (the same snapshot
  SCL uses). **SAVE** saves the focused track there (over another one: SAVE again within 3 s). **POLY held** deletes
  it. The strip shows the preset's letter, source, slots' engines and depth count, and the track it goes to.

**Factory presets.** After A..H, PRESETS turns on into eight factory ones, two a source (`tpfactory.c`):

| # | Name | Source | What it is |
|---|---|---|---|
| 1 | DRIFT | TAPE | the tape through soft grains (WET 40, 220 ms), warmed (TILT -25), into its room and a short echo; an LFO sways the grains' spray over a bar |
| 2 | RINGS | TAPE | the tape ringing RIPPLE's strings tuned to a major chord (WET 45); a SEQ steps their root on semitones |
| 3 | BASS | SYNTH | one saw voice, a low filter (217 Hz) its envelope opens, a short glide, drive, an octave down |
| 4 | PAD | SYNTH | detuned saws, a 1 s attack and 2 s release, the cutoff breathing over four bars, well into its room |
| 5 | KEYS | POLY | a plucked envelope, the filter a little closed and following it, a little of its room |
| 6 | SWELL | POLY | notes that fade in and hang, through grains and well into its room, the cutoff drifting |
| 7 | TIGHT | DRUM | short, bright drums with a little drive and more accent |
| 8 | DUB | DRUM | longer, darker drums that vary (SEED 7, VARY 50) into a feedback delay whose feedback swells |

Each is a short list of knobs away from the defaults, kept in the program (a few hundred bytes for all eight), not
in the presets' flash. Loading one sets the track's sound to its defaults (devices, channel strip, slots, the
source's knobs) and then the list, with the same rules as a preset of my own: the pattern, level, REC IN and reels
stay with the track. They can't be saved over or deleted; a changed one saves into A..H.

How I checked them, since I can't listen here: `test_tpfactory` loads each onto a track and plays two seconds (TAPE
on a factory reel, SYNTH and POLY with a key held, DRUM its pattern), with the main loop's memory polls running so
GRAIN, RIPPLE and SPACE get their buffers. Each has to sound (peak over -30 dBFS), never reach full scale, differ
from its source's defaults (more than -20 dB of difference energy), and peak within 3 dB of the default sound it
replaces, so stepping through them doesn't jump in level. That last one needed each source's LVL trimmed (BASS -6,
DUB -7, TIGHT -3, DRIFT +3, SWELL +4); they now land within about 2 dB. Whether they *sound good* is for the
hardware run sheet: these are starting points, and I'd expect to retune them by ear.

**Where they live.** Flash had exactly two 4 KiB sectors left: 0xDF000 (after the sixth project) and 0xFE000
(Felucca's second copy of its FM6 bank, which Bryo never uses). They're one more A/B pair, so the bank is the
projects' seventh "slot" (`pj_sector(PJ_N, c)`) with the same commit record, CRC and power-cut behaviour: a save
writes the other copy and commits it last. The bank is a stream like a project's ('B' 'K', a version, the count,
then each preset as its unpacked length and its record), packed with the same PackBits writer. A save or delete
copies the other presets forward from the copy in charge a record at a time (the length prefix means they're copied
as bytes, never parsed), so no RAM holds the bank. The record's length comes from a dry run of the writer
(`pw.dry`), the same code counting instead of writing.

**How many fit:** eight typical presets use about half the bank's 3,840 bytes. The test fills it with the densest
tracks I can build (DRUM, every knob and every SEQ step random, 32 depths) and six fit before a save says
"PRESETS FULL"; that refusal changes nothing (the new copy never gets its commit record).

**What it costs:** the snapshot is a whole track plus its depths in main RAM, 1,000 bytes: .bss went from 81.0 to
82.0 of 96 KiB, leaving 14 KiB for the stack (its deepest path is about 4 KB with the audio interrupt nested).

**Tested** (`test_tpresets`): a save and the list; a load into another track (what comes, what stays, the reel
rules both ways, DRUM's pattern knobs kept, the project counted as changed); the snapshot back; a power cycle; an
empty preset refused; a delete; a save cut by a power cut (the bank as it was); a full bank; the view's SAVE (asked
over another), OCT+, SAVE held, POLY held and EDIT; SCL on a knob, on a page, PATN, the mixer's levels, and its undo.

## From playing it on the FM-1, round 2 (2026-10-11)

A list from an evening with the device. What each was, and what I did:

**The black-key shortcuts under GLO.** KEYS NOTE (the black keys as notes) switched the shortcuts off with no way to
them short of leaving the mode. Held GLO makes the black keys shortcuts again, in either mode: the OP keys mute, OP5
OP6 flip REV HALF, MONO and POLY clear, 0 freezes. Their LEDs show the mutes while GLO is held, and nothing in KEYS NOTE
otherwise. The 0 key also froze GRAIN when played as a note, because `chain.c` read the key itself: a flag the UI
keeps each frame (`sys.black_short`) says when the black keys are shortcuts, and the freeze reads it.

**DRUM's STEP while playing.** The keys wrote into the bar OCT picked, but the page drew the bar under the playhead,
so writing as it played looked broken. STEP now always draws the picked bar (with the playhead when it's in it); the
other DRUM pages still follow the playhead.

**A DRUM track's RIPPLE page.** The keys set the strings' root and struck the kit. They pluck the strings alone
now, as on TAPE (on SYNTH and POLY the keys still play the source and the strings follow its notes).

**ENV is the track's envelope, not a modulator.** ENV opened M2, which made it a second door to a modulator the LFO
pad already reaches (LFO held + OP2). It opens the source's own envelope page now: SYNTH's AMP and POLY's ENV (each
their second page), DRUM's KIT (every instrument's decay, with TUNE TONE DRV), and TAPE's, which is new:

- **TAPE's slice envelope** (ATK DEC SUS REL, TAPE's page 2; the page order is the envelope second, as SYNTH's and
  POLY's). Stopped, a key's slice rises over ATK, falls over DEC to SUS while that key is held, and fades over REL
  when it's let go; the head stops once it's silent. Playing, the loop isn't gated: a key's jump fades in over ATK
  and that's all, so the envelope never silences a running loop. The defaults (ATK 0, SUS 100, REL 10 s) change
  nothing: a tapped slice plays to its end as before.
- It runs at block rate (`tape_env`, one divide a block for the stage's step, none a sample): the level at each
  block's end, the samples ramped between. Tested stopped (a held slice dies away at SUS 0, REL cuts it short, a long
  ATK rises) and playing (a jump fades in, a held key at SUS 0 doesn't fade the loop).
- Its knobs are stored at 12-15 (the layout puts them on page 2), so nothing else moved. A project from before them
  stored 0s there, which would silence every slice: projects are version 5 now, and an older one gets the defaults. A
  track preset is copied forward unparsed, so it has no version to ask; a TAPE record with all four at 0 gets the
  defaults instead (no one would save that envelope: it's silence).

The LFO pad's M1-M3 can each be any of the four engines now, LFO ADSR SEQ FOLLOW (a SEQ there has its own steps,
like M4). M4 on the SEQ pad stays the sequencer.

**Page keys.** Holding a page's pad (HOME, EDIT, FX, ENV, LFO, SEQ, ARP) turns the white keys into a map of its pages:
key n glows for page n, the page on screen is lit, every other key goes dark (the LEDs have three levels: off, the dim
glow, lit), and pressing key n goes there. EDIT's keys run through GRAIN's pages and then RIPPLE's, FX's through
COLOR's and SPACE's, since those pads already step through both devices. While a pad is held the keys don't play.

For that to work the pad can't act as it goes down, so EDIT, FX and ARP now turn their page as they're let go, the way
HOME and the slot pads already did; a pad that was used for a key doesn't turn it (`ui.pg_pad`, `ui.pg_used`). One
gesture had to move: HOME + a white key picked DRUM's instrument on its pages, and it's a page jump now like everywhere
else; STEP's INST knob picks the instrument (it always could).

**Step keys (2026-10-11, from playing it: "the sequencer on drums isn't brightening the active step on the white
keys ... update that for seq as well").** Where the white keys set steps they now show them (`ui_leds`, before the
page keys, which still win while a pad is held). DRUM's STEP page in HITS and ACC: the bar's set steps for INST (or
its accents) lit, all 16 glowing; while it plays, the key under the playhead (`drm_now`) flips, a set step going dim
for it and an empty one lit, so the playhead reads over both, the usual step-sequencer convention. LIVE and ERAS
leave the keys alone (they play the kit there). A SEQ slot's page: the steps inside LEN glow, counted from STRT and
round past 16 the way the SEQ plays them; the step playing and the keys held are lit. The step playing comes from
the ISR's slot state through `mod_seq_now` (a 16-bit read, the one field the main loop reads of it). A slot only
runs while it modulates something, so a SEQ with no depths yet would never move; the main loop now names the SEQ
on screen (`mod_show`) and `mod_tick` steps it even then, so you can watch it while you write the steps (its output
goes nowhere). Cost: a compare a track a block, 1 to 4 a sample on the checkpoint runs; the shown SEQ's own stepping
only while its page is up. Tested (`test_step_leds`): three steps lit and all glowing; a second playing, the
playhead's key flipped every block it's in the bar; ACC lights accents; LIVE and GLO show no steps; a SEQ of LEN 8
from STRT 3 glows steps 3 to 10, plays them one lit at a time with no depths assigned, and a held key lights.
Mutants: no DRUM playhead, no SEQ playhead, held keys dark, STRT ignored, the shown SEQ not run.

**Each drum's own knobs modulate.** An instrument's TUNE DEC LVL TONE lived on a held-key page and weren't targets, so
an LFO couldn't swell the hats or a SEQ tune the snare. They're 64 targets now (16 instruments x 4), numbered after
the channel strip (`MOD_TDIN`), so every target before them, and every project's depths, keep their numbers; a
project stores a depth by array and knob, and the instruments are four new knob arrays of 16 values (four
instruments each, `MA_DIN0..3`), which fit the machinery the others use (the modulated copy, the strip, the rails).
A hit reads its instrument's knobs from the modulated copy as it's struck, so each hit samples the modulator. The
gesture: the drum's key held, then the modulator's pad, a knob (in that order; the pad first, a key is a page jump).
The pool grew by 640 B (the four arrays' modulated copies). The audit (`mod_audit.sh`) runs them too: each moves its
drum when the test pattern plays it.

**The arpeggiator on every source, and RIPPLE's own.** The arpeggiator only took SYNTH and POLY. It takes every
source now, and RIPPLE got one of its own:

- **The core is shared.** `arp_run` in arp.c is the arpeggiator with its state and its knobs passed in (an `arp_rt_t`
  and an `AR_*` array); `arp_block` is the ARP pad's, per track, around it (the clear and the project's notes, which
  only that one has). Everything that read `tp[t].arp` inside (the step's length, the notes' order, the pick) takes
  the knob array instead, so a second arpeggiator is a second state and a knob array, nothing more.
- **TAPE and DRUM.** Their keys aren't pitches: white key n is slice n, or drum n. So `arp_block` runs them with base
  0 (the "notes" are the key numbers 0..15) and RNGE forced to 1 (an octave of a slice means nothing), and a step
  hands back the one key it picked, `1 << n`, instead of bit 0 at a moved `key_base`. The source sees one key go down
  each step (GATE keeps a block of silence between them), which is exactly a slice jump or a drum hit, so tape.c and
  drum.c didn't change. On TAPE playing, each step jumps the head; stopped, each plays its slice, through the slice
  envelope. The screen's walk shows the slices or drums ("3 DRUMS", "HOLD SOME SLICES"), no octaves.
- **RIPPLE's arpeggio** is MODE ORDR RNGE RATE, stored as RIPPLE's knobs 12-15 (so ROUT and everything else
  kept their places). It first went on RIPPLE's page 3; it's now RIPL ARP, the ARP pad's page 3 (below). `reso_arp_block` (reso.c) builds an `AR_*` array from them
  (GATE 50: a pluck has no length, the gate only spaces the steps) and runs `arp_run` on its own state. Its keys are
  the white keys on its page, a semitone each above C of OCT, as the page's keys were already. chain.c turns a step's
  rising edge into a pluck: `reso_block` gets the note, makes it the strings' root and plucks, whatever the source
  plays. With MODE on, the keys on its page are only the arpeggio's (the source gets none, SYNTH and POLY too, or
  every key would play twice); off, the page behaves as before. A LTCH or ADD pattern goes on plucking off the page.
  Its knobs aren't modulation targets (switches and a pattern you set, like the ARP page's).
- Projects are version 6: an older one stored 0s where the arpeggio's knobs are, which would be RNGE 0 and RATE 1/4,
  so it gets the defaults (OFF, UP, 1, 1/16).
- The header's page dots: RIPPLE's long title and four pages left the tempo bar no room, and the stop square
  covered the last dot. When the bar would be too short the dots go two apart with the shown page a tick instead of a
  block. (With RIPL ARP moved off RIPPLE nothing needs it today, but the unsaved mark and REC can make the bar short
  too, so it stays.)

What it costs: every track now goes through `arp_block` each block, where TAPE and DRUM used to skip it, and the
RIPPLE arpeggio's checks run on all four. Measured first, that was about 20 host instructions a sample on every
checkpoint run, most of it the arpeggiator's off path (a copy of its knobs for TAPE and DRUM, then `arp_run`'s
bookkeeping). `arp_block` now has a fast path for off and quiet, the state of almost every track almost every block:
it keeps what the off path keeps (the keys held, the transport's edge) and returns the keys. That brought it to about
7 a sample (run 19 1,622 to 1,628; run 15 4,910 to 4,921), and SYNTH and POLY tracks with the arpeggiator off take the
fast path too. RAM: RIPPLE's arpeggio state, one `arp_rt_t` a track.

Tested: TAPE's arpeggio walks the held slices (and RNGE 3 still walks them once), DRUM's walks the drums UPDN with the
kit seeing one key a step, RIPPLE's plucks at C3 and E3 in turn with the tape kept quiet, stops when the keys are
let go, and a latched one goes on plucking an octave apart off its page. Each was checked against a mutant (the step
handing back every key, RNGE kept, the page's keys still reaching the source, no pluck, no new root, the project
fix-up gone), and the factory scene's latched arpeggios, which the move of the project's notes first broke (they were
put back before the "turned on" clear), are back.

**RIPL ARP moves to the ARP pad.** Having the strings' arpeggio on RIPPLE's page 3 split arpeggios across two
pads. It's the ARP pad's page 3 now, titled RIPL ARP, with MODE as its on/off. Nothing about storage changed: the
knobs are still RIPPLE's 12-15 (projects, presets and the version 6 fix-up as they were). What changed is where
the screen points:

- `ARP_P` has a third page (`ARP_RES`, 8-11) with the same descriptors as RIPPLE's 12-15 (a test checks they
  match), and `ui_page` points those four at `tp[t].dev[DEV_RESO][12..15]` instead of `tp[t].arp`. Everything that
  goes through `ui_page` (a turn, the cells, SCL's defaults, the page keys' count) followed with no change of its own.
  RIPPLE's layout is back to three pages; the layout test knows 12-15 are shown on another pad's page.
- The keys: a new flag, `sys.keys_rarp`, is the RIPL ARP page being up. With RIPL ARP's MODE on, its keys go to
  `reso_arp_block` and the source gets none (the track's own arpeggio neither); MODE OFF, the page plays the track
  as the other ARP pages do. RIPPLE's own page is as it was before the arpeggio: its keys set the root (and pluck
  on TAPE and DRUM), a running RIPL ARP or not.
- POLY held on RIPL ARP clears its notes (`reso_arp_clear_req`, read at the next block), as on the ARP page.
- The screen is the ARP page's walk, fed RIPPLE's state and knobs (`viz_arp` builds the same `AR_*` array
  `reso_arp_block` does).

**Knowing when you haven't saved.** Losing a cleared track and not knowing whether a save had happened came from
playing it. The header now shows a small asterisk in the tempo bar whenever the state differs from the last save or
load. It's `pj_changed()`, the project's fingerprint (a hash over the track state, 3.3 KB, about 5 us on the host),
so I don't run it every frame: `ui_input` refreshes `ui.unsaved` four times a second and the header redraws when it
flips. The manual has a "Saving, in short" section up front: tap and hold SAVE, what POLY held clears and how it
warns first, and how to go back to the last save when undo can't help.

**BODY: a floor under the grains (2026-10-11).** At WET 100 the source is gone, and with it what held a cloud down.
GRAIN's page 4 (stored 16-19, so nothing else moved; ROUT stays KNOB 4 of the last page, then page 5, now 6) adds BODY,
MODE, ROOT (NOTE until the wave followed the grains, below), TONE: a layer under the grains, part of the wet side (it goes into the grains' half-rate mix before WET,
so WET sets it with them, and the 2x up costs it nothing more).

- **SRC** is the track's source (the dry block, every other sample at the half rate) low-passed; **TRI** and **SIN**
  a wave playing the grains' notes over ROOT (`pitch_inc`, doubled for the half rate), -12 dBFS at BODY 100. All three go through one one-pole
  low-pass at TONE (CUTOFF_HZ's scale, 30 Hz to 4.2 kHz; the coefficient w / (1 + w), w = 2 pi f / 22050, worked
  out once a block). The input's held to +-32767 so the product fits 32 bits at TONE's top.
- Its level ramps across the block from last block's; a new MODE takes it down to 0 over one block and starts the
  new one from 0 the next (0.7 ms), so neither steps.
- Why these three, from the listening renders (reel 3 through GRAIN at WET 100): an oscillator only grounds the
  cloud when it's in key, so it needs NOTE; SRC is in key and in time by construction. On the FM-1's small speaker
  (little below about 150 Hz) a low sine nearly vanishes and a triangle's odd harmonics carry it, so TRI is the one
  for the speaker, SIN for headphones.
- Cost (host instructions a sample, a track with it on): TRI about 1, SRC about 3, SIN about 6.5. Off (BODY 0, the
  default), it does two compares a block (`gr_body`, out of line); the four-track host runs still read about 18 a
  sample more with it compiled in (run 3 2,483 to 2,502), none of it in `gr_body`: the x86-64 build lays the grain
  code out differently (its decoding inlined otherwise). Compiled out, they're back where they were. The device's
  compiler lays it out its own way, so I left it.
- **The wave follows the grains (2026-10-11, from playing it: "I want the body to follow the scale the grains play,
  not a singular tone").** Each grain that starts (`gr_start`, once it has a slot: a skipped or silent one doesn't
  count) leaves its pitch, PTCH + PRND held to SCAL in 1/16 semitones, folded into one octave (0 to 191), in
  `bto`; the wave sits at ROOT plus that. The grains' pitch is relative to the source's own (0 is the sound as
  recorded) and SCAL's scales are rooted there, so ROOT is where the source's own pitch lands: grains a fifth up put
  the wave on ROOT's fifth, and a pentatonic cloud plays the pentatonic over ROOT. Folding keeps it in the octave over
  ROOT whatever PTCH's octave, so it stays a floor (PTCH -19 is ROOT's fourth, not a note an octave and a half
  down). It glides there, 1/16 of the way a block (`bst`, about 12 ms; rounding away from 0 so it arrives): at
  RATE's fast end a new note every few blocks, and steps that fast would buzz, a glide blurs them into a line.
  Coming on from BODY 0 it starts on the note, no glide up from ROOT. With PTCH 0 and PRND 0 it's ROOT, as NOTE was,
  so a project or preset sounds the same as it did unless its grains move; the knob's only new name is ROOT (same
  place, range and default, so no version). Cost: one modulo a grain start and a few adds a block, nothing a sample
  (runs within 1 to 7 a sample, the two changes together).
- **ROOT RES (2026-10-11, from playing it: "the body sound is not flowing into the resonator and has no effect on
  it").** It did flow: GRAIN's output, BODY in it, is what RIPPLE hears. But a string only rings at its own
  partials. On the host, BODY on the strings' note added about 6 dB, a third away nothing (0.1 dB); and under RIPL ARP
  the strings' root moves every step while BODY sat on its own ROOT. ROOT's lowest value (23, below C1, shown RES)
  now takes RIPPLE's root (`reso_root16`: PTCH, a white key on its page, or RIPL ARP's note; last block's, 0.7 ms),
  with the grains' degrees over it as before, so BODY always lands on the strings. Measured at the strings' note
  (C3) with RIPPLE at WET 100: a sine on ROOT A2 rings it at 41, on RES at 6,842. A projects' or preset's ROOT is
  24 or more, so nothing stored changes. The knob's picture marks no key on RES (the root is another device's).
- Projects were version 7 for this (8 since the shared rooms): an older one stored 0s there (NOTE held to C1, TONE 30 Hz), so it gets the defaults. A track
  preset has no version: BODY, MODE and TONE all 0 means one from before, and gets them too (BODY stays off either
  way).

Tested (`test_grain_body`, each case run twice, BODY on and off, so the difference is BODY alone): SRC at TONE 200 Hz
keeps a 100 Hz and drops a 3 kHz under a tenth of it, TONE up lets it through; TRI at A2 is 110 Hz and SIN at A3
220 Hz, whatever the source; PTCH +7 puts TRI on ROOT's fifth (165 Hz) and PTCH -19 on its fourth (147 Hz, folded);
PRND 12 with SCAL PEN has the wave's notes all pentatonic (4 or 5 of them in a second), there most of the time and
gliding the rest, with no step; BODY turned up starts on the grains' note; WET 0 silences it; BODY 0 to 100 at once ramps in; a MODE change while it sounds moves
no more at the switch than a few times what it moves playing. Projects (a version 6 one gets the defaults, a version 7
one keeps it) and presets (one from before gets the defaults). Each fails its mutant.

**Silent grains cost nothing (2026-10-11).** I measured every grain's loudest output sample across the checkpoint
runs: grains are either silent or plainly heard, almost nothing between. 12 to 18 % of the grain work was grains
reading nothing (a buffer not yet recorded into, the gaps between hits), 31 to 40 % when it's a SYNTH being played
(the gaps between notes); 0 % reading a reel straight off the tape. Each block of a tape or a buffer stores its peak
as it's written (`c->peak`, the screen's), so knowing a stretch is silent costs a few reads, no decoding:

- **At its start** (`gr_start`): a grain whose whole read is silent (`gr_quiet`: every block's peak 0, under 128,
  about -48 dB) isn't started. No work, and its slot stays free for a grain that's heard.
- **Each block** (`grain_block`): a grain whose read this block is silent skips its decoding and mixing and moves on,
  its window to be read again where there's sound. The answer is kept per grain (`qb`, `qq`) while it stays in the
  same block (about 16 control blocks at its own pitch), and looked at again when a block is stored.
- The block being written counts as sound (its stored peak is the old sound's until it's stored), and so does a
  stretch over 256 blocks (not worth the looking).

What it saves: one dense track 3,130 to 2,849, a played SYNTH (run 20) 2,012 to 1,841, every grain backwards 2,964
to 2,792, two dense tracks 2,425 to 2,328; reading a reel with no silence in it (run 5), 17 more for the looking. I
didn't raise the pool with it: the saving is only there when there's silence to skip, and the pool is sized for the
worst case, sound throughout. Where the pool is full, the slots silent grains took now go to grains that are heard,
so the cloud is denser at the same cost. The cost: a stretch quieter than about -48 dB, granulated alone, falls
silent.

Tested (`test_grain_silence`): the check inside silence, at its edge, round the loop's end, the block being written; a
grain inside silence isn't started, one in sound is; a grain across 16 silent blocks (their sound left in, marked
silent) works out nothing inside them, keeps its place exactly, and sounds again past them. Each fails its mutant.

**No clicks in the grains (2026-10-11).** Asked whether grains are smoothed: each has a window (a raised-cosine ramp
at each end, CONT long, 2 ms at the least), so a grain on its own starts and ends without a click. Measuring rather
than assuming (a sine through GRAIN, counting output samples whose second difference passes 600: a corner, where a
sine this loud bends at most about 90), the window was never the problem. What clicked:

- **A grain off the end.** A grain reading past the buffer's end (or the tape's) read silence for the rest of its
  life: cut mid-way. The buffer goes round, and so does the tape's loop, so now the read does too (`gr_mix`, on the
  slow path it already took there).
- **The seam.** The live buffer is a loop: behind the write head the newest sound, ahead of it a bar ago's. A pure
  200 Hz sine fits the 2 s bar exactly, so my first test couldn't see it; at 201.3 Hz (as music is) every grain that
  crossed it clicked (up to 194 corners in 4 s). Worse, the head writes a block at a time and stores it when it's
  full, so until then the stretch just behind the head still reads as a bar ago, and then switches. So a grain is
  silent inside the zone from TAPE_BLK behind the write head up to it, with the window's own curve down to it and up
  from it over GR_DUCK (1.5 ms of what it reads) either side. The zone moves with the head a sample a half-rate
  sample (when it jumped a block at a time, its own edges clicked). A grain is checked once a block
  (`gr_cdist`); only one near the zone runs the ducking loop (`gr_mix` inlined twice, the duck a constant): 4 % of
  grain-blocks in the densest run.
- **A stale window.** A grain decodes 128 samples ahead. When the head overtakes it (a grain pitched down, or
  reversed), those samples get rewritten after it decoded them, and it plays the old version into the new. When a
  block is stored (`gr_bcommit` counts them, `cver`), a grain whose window holds it decodes again (663 times in the
  densest run's 6 s).
- **WET.** It scaled each grain's level a block at a time, and WET 0 killed the grains at once. Now WET is applied
  to the track's grains together, ramped across the block, and at 0 they fade out over that block before they stop.

After: no corners in any case (CONT 0, 50, 100; PTCH +12, -12, +24; REV; SPRY 100; DLY just behind the head; STR at
WARP 200; POS at 98 %; 10 ms grains; WET stepped and to 0), on either sine; the worst bend 260 where it was 5,813.
`test_grain_clicks` keeps eight of them, and each part of the fix fails it when taken out. The level is the same
(RMS within a few percent in every SCAN).

What it costs: the dense runs went up (run 23 2,820 to 3,130; run 3 2,314 to 2,483; run 15 4,870 to 5,041; the
groove +18), and almost none of it is the fix's own work (the refreshes, the wraps and the ducked blocks are all
rare). It's grains doing their work again: a grain that ran off the buffer's end read silence cheaply for the rest
of its life, and in run 23 (500 ms grains in a 2 s buffer) a quarter of them did. That was a bug (quieter clouds,
a click each); the cost of the sound it should have made is the right cost. If the device needs it back, the pool
is where to take it (40, 30, 18).

**The pool goes to grains you'd hear (2026-10-11, from playing it).** One track's grains stayed at a low share with
no other track making grains you could hear. The pool's size and its split counted every track with GRAIN's WET above
0, and a SYNTH or POLY track with GRAIN on starts grains all the time (its buffer always records), reading silence
when nobody plays it. The factory scene has three such tracks, so a dense cloud on the fourth got a third of 18.
Muted tracks and a stopped TAPE-scan track counted too.

Now a track takes a share only when its grains would be heard (`gr_wants`): WET above 0, not muted, something to read
(a tape; a buffer with sound in it), and grains able to start (grain_block's rule). Whether a buffer has sound is
already known per block: each committed block stores its peak, so the buffer keeps a count of its blocks with a peak
of 1 or more (above about -48 dB), adjusted as each block is written (`gr_bcommit`) and counted again once after a
resize (`gr_buf_loud`, the ISR's, before the plan). No per-sample cost. A track that stops wanting keeps the grains
it has sounding (they finish) and starts none; the moment sound goes into its buffer (a block, 12 ms) it's back in.
The GRAIN page says why a track has none: MUTED, STOPPED, NO TAPE, or SILENT.

Tested: three silent POLY tracks with GRAIN on leave the fourth all 40; a chord on one gives it its share of 30;
muting it gives the 40 back; SCAN TAPE stopped takes none; two tracks asking less than 30 between them each get
what they ask and half of what's over. Mutants: no leftover (a lone track held at its need), the leftover's odd one
dropped; the wave not following, not folded, not gliding, not starting on the note. GRNS: 12 held beside an AUTO
track (12 and 18 of 30), two held past the pool (12 and 18; 15 and 15), one holding all 30 (the AUTO one TAKEN,
its screen a golden), a held track alone keeping 12 not 40, and GRNS not a modulation target. Mutants: the held
ones ignored, GRNS read as AUTO, held tracks shared with the AUTO ones, the leftover given to held tracks, GRNS made
a target. Mutants caught: the old WET-only rule, mute ignored,
silence ignored, the loud count not kept, a stopped TAPE scan counted.

**Saving while playing (2026-10-11, from playing it).** SAVE clicked while playing. Bryo's code runs from the flash,
so while the flash erases a 4 KB sector nothing runs, the audio interrupt included, and the driver zeroed the audio
buffer first (else the DMA loops the last half: a buzz): a cut to silence and back, a click at each end, and a reel's
run of erases a stutter. Programming a 256-byte page takes well under a half buffer (2.9 ms), so the audio rides
over those; it's the erases.

- **The fade** (`master.c` `out_fade`, at the end of the output stage): the main loop asks for silence; the gain
  slides to 0 over 256 frames (5.8 ms) and `out_quiet` counts the blocks rendered silent since. Before an erase,
  `storage_hw.c` asks for it and waits until both of the DMA's halves hold only silence (on TIMER4, 30 ms at most;
  no half played in 8 ms means no audio running, as at boot, and nothing to wait for). The gap stays open over a run
  of erases, and `st_poll` lets it go 40 ms after the last: the sound fades back in. A reel's save is one soft gap.
- **Erasing ahead** (`project.c`): a project keeps two copies and a save writes the older. If that one is already
  erased, the save only programs and the sound plays on (`pj_blank`, the 4 KB read through XIP). After a save the
  other copy is the next one written: it's erased at once if this save stopped the sound anyway (it erased: a reel,
  or its own copy), else marked (`pj_pre_off`) and erased by `pj_poll` at the next quiet moment, the transport
  stopped and every track's meter at its floor (64, about -54 dB, where its decay stops) for a second. Never while
  the transport runs, even silent: a sound could start on the next step. Boot erases it before the audio runs, and a
  load marks it. The copy just written is read back and in charge, so the older isn't needed as a fallback.

So a project save while playing (no new take) is seamless; one with a new take stops the sound once, faded, for its
reel's erases. Pausing the erase itself (the flash's erase suspend) would make that seamless too, but it needs
bring-up on the device; parked.

Tested on the host (the NOR model counts erases): boot erases the next copy; a save while playing erases nothing,
twice in a row; while playing (even muted) the erase ahead waits, stopped and silent a second it happens; a save
with a take erases the next copy in the same gap; after two saves and a power cycle, boot erases the older; the fade
reaches silence over 6 ms and says so, and comes back to full. Each fails its mutant. Not testable here:
`storage_hw.c`'s wait and `st_poll` (the device's), which the run sheet's row 24 now asks about.

**More grains: rendering them at half rate (2026-10-11).** Asked for more grains, I profiled first (run 23, one
track, 31 grains, functions not inlined): the mix (reading each grain's window, interpolating, the envelope, the pan)
was about half the grain cost, the ADPCM decoding the other half, about 66 host instructions a grain a sample in all.
The live buffer and the tape are both 22.05 kHz, so a grain has nothing in it above 11 kHz, yet each one was computed
at 44.1 kHz. Now:

- Each grain renders at 22.05 kHz: a block of 32 output samples is 16 grain samples, the read position stepping twice
  as far and the envelope ramping over 16 steps. The track's grains mix into a 16-sample stereo bus.
- The bus goes up to 44.1 kHz once a track: the even samples are the bus's, the odd ones a cubic's midpoint between
  two (-1 9 9 -1)/16. That keeps three samples of history a track (`hl`, `hr`) and puts the grains two half-rate
  samples (about 90 us) after the dry, which nobody hears in a grain. When a track's grains have all ended and the
  tail is 0, the upsampler is skipped (`hq`).
- Decoding didn't change: it's per buffer sample read, whatever the output rate. The shared decoded-block cache I
  also proposed would be the next step for that half.

What it sounds like: at a grain's own pitch it's at least as clean as before. The old path interpolated linearly
between 22 kHz samples, which at 8 kHz is about 3 dB down with an image about 11 dB under; the cubic is about 2 dB
down with its image about 14 dB under, and below 5 kHz the image is 28 dB or more under (82 dB at 1 kHz). A grain
pitched up now folds what it lifts past 11 kHz, where before it folded past 22 kHz, so a high, bright grain pitched up
an octave can sound a little grittier. The one-grain test still checks every sample against a model (the tape read
at half rate, the window, the pan, then the cubic with its carried history).

What it bought, and where I spent it. A grain costs about 43 a sample now instead of 59 (from runs 2 and 3), but the
upsampler adds about 12 a sample for each track with grains sounding. I raised the pool only as far as keeps every
dense run at or under what it cost before: 40, 30, 18 (from 32, 24, 16). The slot mask is two words now (40 slots;
`gr_on`, `gr_set`, `gr_clr`), and the pool grew 2.4 KB (eight more slots), to 329.2 of 336 KiB.

| Run (host instructions a sample) | Before | Now |
| --- | ---: | ---: |
| 3: dense on four tracks | 2,363 (16 grains) | 2,320 (18) |
| 4: the same, all backwards | 2,743 (16) | 2,751 (18) |
| 6: dense on two tracks | 2,273 (24) | 2,219 (30) |
| 15: everything on four tracks | 4,921 (16) | 4,872 (18) |
| 23: one track alone | 3,017 (31) | 2,823 (40) |
| 2: four tracks at the defaults (a few grains each) | 1,652 | 1,702 |
| 19: the realistic groove | 1,628 | 1,649 |

So the dense cases got more grains for the same or less; light grain use pays the upsampler's fixed cost (run 2:
+50, the groove +21). Tested against mutants: a linear upsampler, no history carried, the tail dropped when the last
grain ends, the read stepping at full rate, and a one-word mask (which `grain_count` had hidden by counting through
the same mask: a test now checks the words themselves).

## From playing it on the FM-1, round 3 (2026-10-11)

**TAPE's STRT did nothing at LEN 100.** The loop window was STRT plus LEN, both as a share of the whole tape, and
when it ran past the end the window slid back to fit. At LEN 100 (the default) it always slid back to 0, so moving
STRT never took the start of the sample away. `tape_window` now starts at STRT and trims at the tape's end (LEN is
still a share of the whole tape, so STRT 40 LEN 30 is 40 % to 70 %; STRT 90 LEN 30 is 90 % to the end). GRAIN's
SCAN TAPE and the picture use the same window. Tested at LEN 100, 30, and trimmed at the end.

**ROTA seemed to do nothing.** It did, but only at PLAY and for the slice keys: it moved where the loop starts
counting, and once the loop was running nothing you heard changed until the next PLAY. It rotates the tape you hear
now: turned while the loop runs, the head jumps by the same share of the loop (crossfaded over FADE or the 2 ms
declick), so the same moment of the bar plays another part of the tape, and it plays on from there (`rt->rota` is
what the head last went by; a change while stopped only moves where PLAY starts). Under modulation it jumps when the
whole percent changes. Tested: ROTA 0 to 25 while playing moves the head a quarter of the loop on, then it runs on.

**Deleting a reel.** There was no way to: deleting a project left its reels, POLY held on a track playing one only
sent the track back to its tape, and deleting the file on the drive does nothing (the drive's README said "clear
it on the FM-1", which you couldn't). The project view has a third page, **REELS** (EDIT steps PROJECTS, PRESETS,
REELS): the six slots, each reel's name and length (a long reel's other slots read (CONT.)), and on the strip which
tracks play the one pointed at, by TAPE's REEL or POLY's. POLY held deletes it (`uslot_delete`, reel.c), from any
slot it covers (`uslot_owner`). The audio stops reading it first: `uslot_ok` goes to 0 and tape_view asks it every
block, so the next block reads the slot as empty, before the header's sector is erased (the erase holds the
interrupts, and after it nothing reads the slot). The header is all that makes a reel, so one erase frees every slot
a long one spanned. A track or project that names it plays nothing there and its REEL shows the slot's U number,
as for any empty slot. No undo: the flash is erased. Tested: a 1.3-slot reel deleted from its second slot frees both
and silences the POLY track on it, another reel plays on, deleting one a track plays silences that track at once,
an empty slot says so; checked against a delete that only clears the flag.

**Recording exactly some bars.** For drum loops: REC held + SELECT sets FREE, 1, 2, 4 or 8 bars (`sys.rec_bars`;
the header puts the number before REC's dot). Armed with bars, a take waits for a downbeat (`TB_WAIT`): PLAY's, or
the next bar's when it's armed while playing (`mclk.beat >> 18`, four beats a bar, read at the block's start). It
then counts the bars in output samples (`rec_left`, 10,584,000 / BPM a bar: one divide when it starts) and, on the
sample it runs out, ends the take (`TB_DONE`). The main loop lets REC go (`tape_poll`) and the screen says how long
the take is. A blank tape waiting has its head parked at 0 (it was running from before and would have filled its
first chunks).

The catch was the length. A tape is whole 256-sample blocks (11.6 ms), so a take ended on a block would drift
against the tempo every time round. The tape now has an exact end: `tape_ctl.trim`, the samples the last block holds
past it (the view's length is the blocks minus that), set where the take ends, cleared wherever a tape is replaced
and when a steal shortens it. A reel keeps it in its header's `name[7]` (names are four letters and their 0, so the
byte was always 0: old reels read as before), so a project saving the take keeps it exact, and a reel copied onto a
tape for REC keeps it too. At 120 BPM 2 bars are 88,200 tape samples, exactly; at 130 one bar is 40,707 (the
40,707.7 rounded down: under a sample a time round). Stopping the transport mid-take keeps what it has.

Tested: setting the bars doesn't arm or open the USB record mode; 2 bars at 120 from PLAY are 88,200 samples, REC
lets go by itself and the take loops once every 4 s; saved into a reel it's still 88,200; armed at 1.4 s while
playing at 130 it waits for the downbeat, then one bar is 40,707 samples. Each fails its mutant (no trim, no wait for
the downbeat, the reel dropping the trim, REC never letting go).

**The zoom on a loop's edges, and POLY's END.** STRT and LEN were whole percents, which on a 20 s tape is 200 ms a
step: no way to set a loop's start on a hit. They're hundredths of a percent now (0-10,000, `F_PCT100`: the strip
shows whole percents, and `param_step` makes a click 100 of them, a whole percent, as before), and so are POLY's STRT
and END. `pct100_of` (tape.c) takes a share of a length in two parts (len / 10,000 x v + len % 10,000 x v / 10,000:
the whole product would pass 32 bits on a long tape). (The first version kept whole percents and put the
hundredths in hidden knobs, so saved presets would still read; with compatibility dropped the same day, the knobs
just got finer.)

HOME held + STRT or LEN (POLY: STRT or END) is the zoom (`zoom_edge`, ui.c; `ui.zoom`): a click moves the edge a
hundredth instead of a hundred (a fast turn further, as KNOB says), the strip shows "12.34" in orange, and
the picture (`vz_zoom`) is the sound itself around the edge, decoded (each column the lowest and highest sample
under it), half a percent either way (23 to 370 ms), what plays bright and the rest dim, the edge's time under it.
HOME's let-go doesn't turn the page after a zoom; turning without HOME moves whole percents and keeps the
hundredths.

Finer edges made modulation step where it hadn't: an LFO on STRT moved the window's start every block, and every
time it passed the head the head jumped. A head just short of the window's start (within a sixteenth of the loop)
now plays on into it; only one far outside jumps. POLY's END cut a note in one block (0.7 ms); it fades over six (4.4
ms) now (`POL_ENDF`). The footer says KEYS:PAGES KNOB:ZOOM while HOME is held there.

POLY had no end: **END** (default 100 %) joins page 1 after STRT, and VOIC moves to the last page beside LVL. A note
stops at END as at the sound's end, with the same fade. The picture lights the sound from STRT to END.

Project space: with the hundredths in the knobs themselves, and SPACE's room-knob places stored as 0, the biggest
project there can be is 3,772 bytes of the sector's 3,840, with SYNC in.

Tested: 7 clicks with HOME held put the loop's start at 0.07 % to the sample; LEN 99.97 %; HOME's let-go ends the
zoom without turning the page; whole-percent turns keep the hundredths; zoomed past 0 holds at 0.00; END at 25 %
stops a held note at 0.580 s of a 2.32 s sound (then its fade); POLY's zoom on END and STRT; STRT moved a little past
the head doesn't jump it, far past does. Each fails its mutant (END ignored, no zoom, no tolerance).

**What follows the tempo.** Asked why the tempo didn't seem to move the LFOs, grains and arpeggiator, I measured it
first: at 60, 120 and 180 BPM (and turned while playing) the arpeggiator, DRUM, the SEQ and the LFOs on SYNC BPM (the
default) all scale with it. What didn't: the tapes and reels (they play at their own speed), GRAIN's RATE (grains a
second) and SPACE's TIME (ms), and an LFO on FREE. Those three now have a SYNC each, all locked to the same beat
clock (`mclk`, mod.c) rather than a free counter of their own, so none of them drifts against the drums:

- **TAPE SYNC** (TAPE 4, KNOB 3; stored 17): OFF or 1, 2, 4, 8, 16 bars. The loop window lasts that many bars at the
  tempo: the head's step is the window over the bars' output samples (one divide a block), with the pitch following
  as on a tape machine (SPD's size is set aside, its sign and REV kept). Every block, where the beat says the head
  belongs (`tape_sync_frac`: the beat's place in the bars, plus ROTATE and where a slice key moved it, `soff`) is
  compared with where it is, and the step is pulled by at most 1/64 to close the gap over about 2,000 samples: a
  soft lock, so it never jumps, and the step's rounding (under 1/2,048) never adds up. A gap over an eighth of the
  loop (a new tempo, a slice key, PLAY) jumps there, crossfaded. A take of set bars on a blank tape sets SYNC to its
  bars. Measured: 30 s at 100 BPM with a 2 s reel as one bar (2.4 s), at most 14 samples (0.6 ms) off the beat; 140
  BPM while playing, locked again within 2 s, at most 19 off; a slice key moves the loop and it keeps time from there.
- **GRAIN SYNC** (GRAIN 6, KNOB 2; stored 21): RATE picks a division (1 bar .. 1/64, with triplets), and a grain
  starts in the block each step of that grid falls in (`gr_snext`: the next step at or after a beat, in PATN's
  rhythm: EVEN, SWNG the odd steps a third late, CLST three a quarter step apart then quiet for the rest of four, RND
  each step's grain at a place in it from the step's number). The pool's planning reads the division's grains a
  second (`gr_rate_gps`). Measured: 1/4 at 120, 8 grains in 4 s, each in its beat's block.
- **SPACE SYNC** (SPACE 4, KNOB 1; stored 12): TIME picks a division in 96ths of a beat (1/64 .. 1/2, dotted and
  triplets). The line is 370 ms, so one longer at the tempo is halved until it fits (`sp_sync_d96`; a 1/4 at 120 BPM
  plays 1/8), which keeps it on the grid; the strip names what plays. The glide on TIME still smooths a change.
  Measured: 1/8 at 120 and 100 BPM, 250.0 and 299.3 ms; 1/4 at 120, 250.4.

Project room: the three new knobs would have run the biggest project past its sector, so a project now stores the
places of SPACE's room knobs (the track's own, never read) as 0, which packs to almost nothing, and they're 0 in
memory too: the biggest project is 3,804 bytes with the new knobs. The default project costs 10 a sample more
(2,831: the SYNC checks a block).

**Compatibility dropped.** Projects had barely started, so old formats aren't kept up with any more: a project is
read only at the current version (9; anything else reads as one this Bryo can't), the track presets' bank only at
its version (2; another reads as empty), and the fix-ups for each older version are gone (SEQ's SPRD AMT OFS,
TAPE's envelope, RIPPLE's arpeggio, GRAIN's BODY, the rooms from a room per track, and the old-preset guesses).
What stayed is the format itself: counted arrays, clamped values, depths by array and knob. ROTA: only the knob
itself rotates the loop live now; a modulated ROTA moves where PLAY and the slices start (and SYNC's lock), as it
did before, because the live jump made it step under an LFO (the modulation audit found it; I hadn't run it after
the ROTA change). The audit also marks a row SILENT now instead of STEPS when the modulated sound is next to nothing
(-60 dBFS): there its edge measure is noise (POLY's END modulated below a held note ends it, as it should).

## KEY, ROOT and FINE, as built (2026-10-12)

Tuning for the two sources that play notes, SYNTH and POLY, and a scale the keyboard can be held to. TAPE stays a tape
machine: its pitch is its speed, on purpose.

**Files:** `param.c` (`KY_P`, `keyp`, `KSCAL_MASK`, `key_snap`; POLY's ROOT and FINE, SYNTH's FINE), `synth.c` and
`poly.c` (the snap where a note starts, FINE through `cents_inc`), `ui.c` / `ui_input.c` / `ui_px.c` / `ui_viz.c`
(the mixer's KEY page, `viz_key`, `vz_cents`), `project.c` (format 10), `tpreset.c` (bank 3, the snapshot),
`tpfactory.c` (the scene's ROOTs).

**KEY and SCAL, one pair for the project.** The mixer got a page between MASTER and SETUP (EDIT tapped three
times): KEY (C to B) and SCAL (OFF, then 15 scales since 2026-10-13, below). I made it the project's rather than a knob per
track because tracks that disagree on the key are almost never what I want, and one place to change it means the
whole song moves together. It's stored with the project, after the master compressor, and SCL's snapshot keeps it
like the master's knobs.

**One list of scales (2026-10-13).** KEY, GRAIN's SCAL and the KEY picture had three lists of their own (eight,
five and eight names) and three mask tables saying the same thing. Now `param.c` holds the list once: `SCALE_MASK`
(15 masks, bit n: n semitones above the root) with `SCALE_NAMES` for the knobs and `N_SCALE_FULL` for the picture.
The order is the seven modes (MAJ MIN DOR PHR LYD MIX LOC), the altered minors (HMIN MMIN), the pentatonics (PEN
MPEN), BLUE, and the symmetric ones (WHOL DIM CHR); MAJ and MIN keep their old indices, so a project's KEY means
what it did. KEY's list is OFF then the 15 (`key_mask()`); GRAIN's is OFF, KEY, then the 15 (`gr_mask()`), KEY
being the project's scale as it stands, every semitone while the project's SCAL is OFF. A grain's SCAL holds the
pitch *shift*, so the mask is read from the sound's own pitch, not transposed to the key: a sound in A with MIN keeps
the grains on A minor's intervals. `gr_pitch()` used to find the semitone's place in the octave with `% 12` on a
signed value, two hardware divides a grain; it now carries the place along as it walks. RIPPLE's SCAL is a chord
list, not a scale list (a string set is a chord): HARM MAJ MIN PEN SUS2 SUS4 DOM7 MAJ7 MIN7 DIM AUG 5TH OCT, one
table `RS_PART16[13][24]` in `reso.c` that the strings take the first four of and the picture draws whole (it was
two tables, one each side, that had to agree). The SCAL pictogram on RIPPLE now lights the notes the four strings
make rather than a scale with the same name.

**Snap to the nearest, not skip.** With SCAL on, the keyboard stays a piano: every key still plays, but a note
outside the scale sounds as the nearest one in it, and between two the lower (`key_snap`). The other way, white keys
walking the scale, packs more octaves under the hand, but it breaks the picture of the keyboard you already know and
makes the black keys meaningless; snapping keeps both, and a wrong note just can't sound. Every scale here has no gap
wider than three semitones, so nothing ever moves more than one (the test walks every key, scale and note). A scale
is a 12-bit mask; the snap doubles it to 24 bits so looking a few semitones down or up never wraps, and it runs once
when a note starts, so the ISR pays nothing per sample.

**Where it applies.** At the one place each source turns a key into a note: `syn_note16` (SYNTH, legato's glides
too) and `pol_start` (POLY). The arpeggiator's notes go through the same place, so an arpeggio follows the scale
without arp.c knowing about it. A held note keeps its pitch when SCAL changes; the next one follows. GRAIN's and
RIPPLE's own SCAL stay theirs: they're sound design, not the keyboard.

**ROOT (POLY).** The note the sound was recorded at: that key plays it as it is, every other key against it. It was
always C3 (white key 1 at OCT 3), and a sound recorded at A had to be fixed with TUNE -9, which then read as a
transposition. ROOT is a setting, not a modulation target (`nomod`): modulated, it could only step by semitones, and
TUNE already does that live and smooth.

**FINE (SYNTH and POLY).** -50 to +50 cents, on top of TUNE, for a sound that sits between two notes or a synth
against a recording. It goes through `cents_inc`, which takes whole sixteenths of a semitone from the table and the
rest as a factor, so it's exact to the cent instead of to the 6.25 cents a sixteenth is; at 0 it's the same step as
before, bit for bit. SYNTH's DTUN rides on it (the second oscillator is FINE + DTUN cents up). Both are modulation
targets, and the audit shows neither steps.

**The pages.** SYNTH's VOICE page is VOIC GLID TUNE FINE now and DRV moved to the LEVEL page beside LVL (DRV focused
there draws its curve, LVL the level), so the tuning knobs sit together. POLY's last page is VOIC ROOT FINE LVL: the
picture shows the voices, ROOT's note "AS IS", FINE on a cents meter and the level. The KEY page draws an octave, C
to B, the scale's notes solid and the rest dotted (the same dotted frame an unused knob gets), a mark under the key's
note, and beside it the name, how many notes it keeps and "ELSE: NEAREST".

**In key from the start.** Every factory reel is built on A (BELL's first note A3, DRON an A2 drone, FOLD A minor
pentatonic from A2), and the factory scene's music is A minor, but the first cut left KEY on C and SCAL OFF, and
ROOT at C3 while POLY's first reel is BELL: a new POLY track played every key 9 semitones sharp of its name (that
was true before ROOT too; ROOT is what could fix it). So a new project, and the scene (which says so itself, in
case the default ever moves), start on A MIN, and ROOT starts on A3: key C3 on BELL now sounds C3. A user reel
still needs its ROOT set by hand; there's nothing in a WAV that says what note it is. The KEYS and SWELL presets
inherit both. The scene sounds as it did: its arpeggios' notes are all in A minor already (its test says so), and
the chords move by TUNE, which is added after the snap.

**Formats.** Projects are version 10 (KEY and SCAL after the master's knobs), the track presets' bank version 3 (a
POLY preset's ROOT would read as nothing in a bank 2). Nothing older is read, as since "Compatibility dropped".

**Cost.** One `key_snap` a note start, and per POLY voice per block a `cents_inc` in place of `pitch_inc`: one more
divide by 100 and a multiply, a block. SYNTH pays it only with FINE off 0. The load tables didn't move.

**Tests** (`test_key_tuning`): SCAL OFF changes no note; C MAJ (C#3 to C3, F#3 to F3, B3 and C4 as they are); A PEN
(C to B, D to C#, G to F#); every key, scale and note lands in the scale within a semitone; SYNTH and POLY play C3 for
the C#3 key in C MAJ; POLY reads the sound at 0.5 on ROOT's key, half that with ROOT an octave up, x1.0293 at FINE
+50; SYNTH's A3 goes from 220.00 to 226.44 Hz at FINE +50; a project keeps G DOR; the KEY page from EDIT x3, KNOB 2
on POLY's page 4 to ROOT A3, SYNTH's FINE on KNOB 4. Three new screens are in the golden set (`mixer_key`,
`poly_tuning`, `synth_tuning`).

## Sends: ROUT, as built (2026-10-09)

GRAIN, RIPPLE and COLOR each have a WET, and until now it was always a crossfade: turning WET up turned the dry
sound down, because the device sat in the track's line (an insert). That's right for some things (a track that's
all grains) and wrong for others: a little resonance or a bit of parallel drive under a drum loop wants the loop at
full and the effect added. So each of the three now has ROUT, INS or SEND, on its own page:

| Device | Where ROUT is | INS (as before) | SEND |
| --- | --- | --- | --- |
| GRAIN | page 4, KNOB 4 | dry x (1 - WET) + grains x WET | dry + grains x WET |
| RIPPLE | page 3 (its own page), KNOB 4 | dry x (1 - WET) + strings x WET | dry + strings x WET |
| COLOR | page 3, KNOB 3 (left of LVL) | dry x (1 - WET) + coloured x WET, then LVL | dry + coloured x WET, then LVL |

SPACE has no ROUT: DLY and VERB were always levels added to a full dry, which is a send already.

This is a send per track, not a shared aux bus: each track's devices stay its own, nothing new is allocated, and the
CPU is the same. (A shared bus, one reverb that four tracks send into, was the other option; it would save memory
when several tracks want the same effect, at the cost of shared knobs and a rework of the chain and the mixer.
Parked, not ruled out.)

How it works. Each device keeps the dry's share from the last block (INS: 1 - WET; SEND: all of it) and ramps it
across the next block when it changes, so flipping ROUT under a held note doesn't click (the test holds a sine
through the flip: its largest sample-to-sample move is 118 against a steady 50, where the doubled level's own slope
is about 100; without the ramp it's 912). While nothing changes and ROUT is INS, each device runs its old code, so
INS sounds exactly as it did. The cost of the extra branch: run 19 went from 1,602 to 1,615, run 15 from 4,875 to
4,909.

One thing to know about COLOR as a send: with nothing colouring (DRV, CRSH, NOIS and TILT at 0), the "coloured"
sound is the dry itself, so a send adds the dry to itself: +6 dB at WET 100. That's what parallel processing does,
and COLOR's LVL sits right beside ROUT for it. With drive on, it's parallel saturation: the transients stay clean
and the drive fills in underneath.

ROUT isn't a modulation target (`nomod`): a switch flipping at an LFO's rate isn't a sound. The pictures follow it:
RIPPLE's response adds the strings to the flat dry line instead of blending, COLOR's transfer curve adds the
coloured sound to the straight line.

Tested (`test_sends`): for each device at WET 100, SEND minus INS equals the dry sound (the same run at WET 0) to
within 3 of a 3,959 peak, which is the definition of a send; the ROUT flip without a click (the check fails with
the ramp taken out); ROUT on the three devices and never a modulation target.

## The mixer, as built (2026-10-08)

The mixer's knobs did nothing until now: the channel page drew a response no sound followed. `firmware/src/mixer.c`
makes them heard, in the S-4's order, and adds the master compressor the PRD left without a home.

**Each track's channel** (GLO, then EDIT: LOW HIGH FILT PAN), after the track's devices:

- **LOW and HIGH** are shelves of +-12 dB that turn over (half their gain) at 210 Hz and 1.9 kHz. A shelf is one
  pole, trapezoidal (Zavalishin's form, which stays true near the top, where a plain one-pole left the high shelf 4
  dB short at 8 kHz), with its corner moved by the gain so the turnover stays put. The slope is two octaves wide,
  and the picture now draws it that wide (it drew one).
- **FILT** is the DJ filter: left a low-pass coming down from 10 kHz to 92 Hz, right a high-pass going up from 40 Hz
  to 4.5 kHz, 12 dB an octave, no bump. It's POLY's state-variable filter, unrolled with its input held to +-65,535 so
  its states can't overflow without clamping each one. At its lowest corner it bottoms out near -29 dB instead of
  falling forever: the filter's precision runs out there, and nobody will hear 1 kHz at -29 dB under 92 Hz.
- **PAN** comes after the level: equal power across, with both sides at full in the centre, so turning it from the
  middle only ever lowers one side (a balance: the tracks are stereo already). The picture's bars follow the same
  law now: full height both sides at the centre.

**The master compressor** (EDIT tapped twice: MASTER) works on the four tracks' sum, before MASTER and the output
stage's limiter, where the S-4 puts it. AMT is the threshold from 0 down to -30 dB at 4:1, with a 6 dB soft knee and
the level made up by half of what a full-scale sound loses, so turning it up glues the mix instead of making it
quieter. ATK closes it in 1 to 100 ms, REL opens it in 20 to 990 ms (990, because 1000 doesn't fit the strip's five
characters). MIX is parallel compression: for a gain-based compressor it's a blend of the gain toward 1, so it costs
nothing. It follows the sum's peak once a control block (0.73 ms) and works in octaves (a log2 and an exp2 of 17-entry
tables) to get the gain, ramped across the next block. The MASTER page draws the in/out curve from the same function
(`mx_curve`), the threshold as a node, and what it takes off now as a bar with its dB.

**EDIT on the mixer** now steps LEVELS, CHANNEL, MASTER and back, each latched; held from the levels, it's still the
channel while held.

**At the defaults nothing runs**: a flat channel, a centred pan and AMT 0 leave the sound sample for sample what it
was, and the tests check that. The default path costs what it did before. The pan's centre came out at 0.9999 from
the sine table, so it's snapped to exactly full, and the level is applied in the summing loop unless the pan moves.

**Load** (host instructions per sample, `tests/checkpoint_sim.sh` run 17: every channel's LOW, HIGH, FILT and PAN on,
the compressor at AMT 60): 1,699 against 925 for nothing on, so about 145 a track with all three filters on, 10 for
the pan, 2 for the compressor. The budget said 60 for the whole mixer; that was the compressor's and the pan's
share, not three filters a side. Unrolling the filter and keeping its state in locals took it from 210.

**Tests** (`test_mixer`): the defaults are untouched; LOW +12 lifts 60 Hz 10.9 dB and leaves 8 kHz; HIGH -12 cuts
8 kHz 11.3 dB and leaves 60 Hz; FILT -100 passes 50 Hz and holds 1 kHz under -25 dB; FILT +100 holds 1 kHz under -20 and
passes 10 kHz; FILT -50 is -3 dB at its corner (970 Hz) and 25 dB down two octaves on; PAN -100, 0, +100 and -50 (the far
side at 0.54); AMT 100 turns 20 dB in into 5 dB out and lifts the quiet sound 4.4 dB, the meter shows it, MIX 0 gives
the dry sound back; ATK 1 ms holds a hit down within 0.7 ms against 16 ms at 100; AMT back to 0 glides home and leaves
the sound untouched; every channel knob at its top with a square past full scale stays bounded. The controls test
walks EDIT through LEVELS, CHANNEL, MASTER and back, and the stress turns the channel and compressor knobs.

## Modulation, as built (2026-10-09)

The four slots had pages, knobs and pictures since phase 2, but they moved nothing. `firmware/src/mod.c` runs them
now, and the PRD's gesture sets where they go.

**The gesture.** A slot's pad does two jobs now, told apart by what happens while it's down:

- **Tap it** and its page opens as you let go (again: its next page, as before). It used to open as the pad went
  down; it waits for the let-go so a hold can mean something else.
- **Hold it and turn a knob** on whatever page is up (a device, the source, the channel strip) and you set that
  slot's depth to that knob, -100..100 %. While the pad is held, the header's box says M1..M4 and the strip's value
  row shows the slot's depths instead of the knobs' values ("--" where there can't be one). Let go and you're still
  on the page you were on. A knob any slot moves gets a small mark in its cell's corner.
- **Turn a depth back to 0 and it catches there.** A depth that would reach or cross 0 stops at 0 (and is gone from
  the list), and a turn that keeps going in that direction stays at 0 until the knob rests for 250 ms; after that
  it goes on into the other sign. Turning back the way it came isn't held. I added it because removing a depth
  meant landing on exactly 0 with an accelerated encoder, and overshooting to -3 left a quiet depth you couldn't
  see on any other page. The clamp lives in `mod_nudge` (so every caller gets it); the 250 ms hold is the UI's
  (`ui.catch_*` in `on_knob`), since only a hand turning a knob needs it.
- **Hold it and turn SELECT** and the slot changes engine (LFO, ADSR, SEQ, FOLLOW) without opening its page; a
  message names the new engine. Its depths stay, so you can try an envelope where an LFO was without re-routing.
- **MONO held half a second** clears the focused track's depths, every slot (the slots' own knobs stay). **SAVE held**
  undoes it, or the tape clear, whichever came last. It's the same half-second, same undo as POLY's tape clear,
  so there's one rule for the two destructive keys.
- **A slot's pad + MONO held half a second** clears only that slot's depths on the page shown (`mod_clear_slot`,
  the page's four targets from `page_target`); the other slots' depths on the same knobs, and that slot's depths on
  other pages, stay. SAVE held undoes it the same way, since it snapshots the track's list into MONO's undo. The
  slot is read as MONO goes down (`ui.mono_slot`), and letting the pad go first calls the clear off: otherwise a
  pad released a moment early would turn a one-slot clear into a whole-track one.
- **A SEQ page** takes the white keys: hold one or more and KNOB 1 sets those steps' values (they're framed on
  screen); let go and KNOB 1 is LEN again. The keys don't play the track while a SEQ page is up, the way GLO's
  view borrows them to pick tracks.

What a depth means: at 100 %, the slot's full swing moves the knob by its whole range. An LFO swings both ways
around where you set the knob; ADSR, SEQ and FOLLOW push one way, and the depth's sign says which. The result is
clamped to the knob's range. So a filter at the middle with an LFO at 50 % sweeps end to end, and an envelope at
100 % on a WET of 0 opens it all the way.

**What can be a target.** Every knob of GRAIN, RIPPLE, COLOR and SPACE, every knob of the three sources (each
source keeps its own depths, as it keeps its own knobs), and the channel strip's LOW, HIGH, FILT and PAN. Not the
reel choices (TAPE's REEL and POLY's REEL): the main loop prepares memory for the reel a track plays, and the ISR
switching it under the main loop's feet is how you get a tape playing a reel that was never copied. A track holds
up to 32 depths; the 33rd is refused with a message. Not the levels or the master compressor: the PRD's
modulation is per track, and those aren't.

**A dozen a slot (2026-10-13).** On top of the 32 a track, one slot holds at most 12 depths (`MOD_SLOT_MAX`,
`mod_slot_count()`; `mod_dep_set` says -2 and `mod_nudge` -127, a code of its own so the message can name the
slot: "12 DEPTHS ON M1: PAD+MONO CLEARS"). Two reasons. A modulator on a dozen knobs is already everything moving
at once; past that it's a wash, and the knobs' marks stop telling you anything. And without it one slot could take
the whole track's 32 and leave the other three nothing. Twelve times four is 48, so the track's cap still binds
when every slot is busy. While a slot's pad is held the footer counts its depths ("DEPTH 7/12 OP:M1-3", the SEQ
"KNOB:DEPTH 7/12", the rest "DEPTH 7/12 SEL:ENG": 19 letters at most, since DRUM's "T2 BAR 1/2" takes the right
end). Changing a slot's engine (its pad + SELECT) keeps its depths, it always did (`param_engine` resets the slot's
own knobs, not the list), and the message now says so when another page is up: "M1 IS ADSR, 4 DEPTHS KEPT".

**How it runs**, and why it's built this way:

- The main loop owns the depths (`mdep`, a byte per slot per target: 144 targets). Every change rebuilds the track's list of the
  depths that aren't 0, sorted by target, into the half of a double buffer the ISR isn't reading, then flips one
  byte. The ISR can't be interrupted by the main loop, so it always sees a whole list, old or new, and never a half
  edited one. Each entry carries the knob's range and its depth already multiplied by it, so the ISR's work per
  depth is a multiply, a shift, a clamp.
- Once a control block (0.73 ms), before any track renders, `mod_tick` advances the clock, runs the slots that
  some depth uses (a slot without depths costs nothing), then for each knob array that has depths (a device's 16, a
  source's 16, the channel's 4) copies the knobs as you set them and adds the depths. The devices read their knobs
  through `TPD(t, array)`, which points at that copy or at the track's own array. An array without depths reads
  exactly what it read before, so a track without modulation sounds sample for sample as it did. I tried copying a
  whole track first: it cost more and took 7 KB of main RAM, which is how I ended up with arrays.
- The memory polls ask `mod_peak` instead of the knob: GRAIN's WET, RIPPLE's WET, SPACE's DLY and VERB at 0 but
  modulated up keep their memory ready, so an envelope can open a reverb that was off.

**The engines**, against the S-4's (see "Modulators, lined up with the Torso S-4" for the knobs):

- **LFO.** RATE is 0.02 to 20 Hz on a log scale (10 octaves over the knob), or with SYNC BPM one of 16 divisions from
  16 bars to a 32nd, locked to the beat so it stays in time. The shape comes from the same `lfo_at` the picture
  draws with, so what you see is what moves; it's evaluated at two neighbouring points and interpolated, so a slow
  LFO glides instead of stepping. PHAS starts it later; TRIG KEY restarts it on a key; FADE fades it in over FADE %
  of a cycle from each restart (PLAY, or a key); SMTH is a slew up to about 0.4 s; VAR drifts RND's steps each time
  round, or on the other shapes each cycle's level, repeating every LEN cycles. AMT and OFS place it. SPRD is a
  second output half a cycle on at 100.
- **ADSR.** The times are the synth's (1 ms to 10 s), each stage bent by its curve exactly as the picture bends it.
  A key down on the track opens it and holds it at SUS; let go, it releases. It also fires once when the track's
  loop starts: a TAPE track's at its seam (and when PLAY starts), a SYNTH or POLY track's every four bars. That
  one-shot rises, decays and releases on its own, so an envelope does something on a loop nobody is playing. LOOP
  cycles attack and decay while a key is held. **VEL** does nothing yet: the FM-1's keys are switches with no
  velocity, and MIDI in doesn't reach Bryo's tracks. It waits for that.
- **SEQ.** A step a RATE (1 bar to 1/32, now named on the strip instead of 0..5), LEN steps from STRT, in DIR's order
  (forward, reverse, ping-pong, random), each played with PROB's chance (a step that doesn't play holds the last
  value), SLEW gliding over that share of a step, SWNG pushing the off-beats up to a third of a step late. It
  counts from PLAY, or from a key with TRIG KEY, on the same clock as the LFO, so the two stay in step.
- **FOLLOW.** The envelope of the source's last block (SELF: this track; T1..T4: that track, after its devices and
  level), times GAIN, rising and falling at RISE and FALL, and with HOLD sampled once a division. **USB** listens
  to nothing for now: outside the record mode the computer's audio isn't coming in. When USB audio in outside the
  record mode lands (the future work I noted for audio), it's one line.
- **SPRD** is a right-channel output on every engine (the LFO's half a cycle on; ADSR and FOLLOW lag up to 200 ms).
  Only PAN uses it, as we agreed: with SPRD the left and right channels get their own pan positions, so the sound
  moves across instead of just sideways. Every other target takes the left output.

**A LEVEL on every source** (2026-10-09). The S-4 puts a LEVEL on each of its sources; Bryo had the mixer fader
(not a modulation target) and COLOR's LVL (one per track, after GRAIN and RIPPLE). Now every source's last page
ends with LVL, -24 to +6 dB like COLOR's: TAPE 3 (REEL ROTA LVL), SYNTH 5, POLY 4. It's applied as the source is
mixed into the track (`chain_source`), ramped across the block, and skipped at 0 dB so nothing changes until it's
turned. A SYNTH or POLY track prints onto its tape after it (what you hear is what goes on); a TAPE track's REC
still records at GAIN. SYNTH's four pages were full, so a device or engine can now have five pages (NPK 20, from 16),
and the targets are numbered by NPK: GRAIN..SPACE 0..79, the sources 80..139, the channel 140..143.

**Six pages (NPK 24, 2026-10-11).** GRAIN's GRNS needed a 21st knob. Raising NPK moved every modulation target's
number (GRAIN..SPACE 0..95, the sources 96..191, the channel 192..195), which costs nothing stored: projects and
presets keep each array with its count and each depth as (array, knob), and the numbers live only in RAM. An older
file's arrays are 20 long and the rest come from the defaults (a project load starts from `param_defaults`). What
did matter: the biggest project there can be (`test_project_worst`: every knob random, every step and pattern set,
32 depths a track) packed to 3,805 of the sector's 3,840 bytes at 20 wide and didn't fit at 24. So a project and a
preset now write each array only up to its last knob with a label (`pdesc_len`): past it every knob is empty and
reads as 0 anyway. That packs the biggest to 3,741, under what it was.

**Every effect opens with its mix (2026-10-11, from playing it: "wet and send options should be farther up in the
page count for all parameters that have them").** ROUT and WET are KNOB 3 and 4 of page 1 on GRAIN, RIPPLE and
COLOR; SPACE's sends, DLY and VERB, the same. Bringing an effect in is the first thing you do with it, and with
WET 0 (the default) nothing else on its pages is heard, so it was backwards to have WET on GRAIN 2 and ROUT on
the last page of everything. Only the `PLAYOUT` rows moved: stored order is the same, so projects, presets,
modulation targets and the DSP are untouched. The rest close up behind: GRAIN SIZE RATE ROUT WET / PTCH SPRD SPRY
CONT / REV PATN SCAL PRND / SCAN WARP OFST FDBK / BODY MODE ROOT TONE / GRNS; RIPPLE PTCH DEC ROUT WET / CUT
RES TYPE SCAL / TONE; COLOR DRV CRSH ROUT WET / NOIS TILT NDEC NTON / CMOD . . LVL; SPACE TIME FDBK DLY VERB /
SIZE DEC TONE SPRD / PRE (since the shared rooms: TIME FDBK DLY VERB / ROOM TONE SPRD). The layout lint (`test_mod`) checks the rule: an effect's page 1 ends ROUT WET (SPACE DLY
VERB), a source's (and COLOR's) last page ends LVL, a modulator's AMT OFS. The other rule ("ROUT KNOB 4 of the last
page") is gone with it. The pictures in the visualizations don't care which page they're on (they follow the knob
turned), so nothing else changed but the tests and the manual. Reading one, an empty knob past what was
stored is set to 0 (it was left as it was, which after a slot changed engine could be the old engine's default).
An older Bryo reads the newer files too: it takes the counts it knows and skips the rest. GRAIN's pages: 5 is PATN
SCAL PRND and an empty KNOB 4; 6 is GRNS, two empty, ROUT (ROUT stays KNOB 4 of the last page). Cost: 384 B of main
RAM (`tp`), 416 B of the pool (`mda`), and the modulators copy 24 a modulated array where they copied 20: 29 a
sample on run 25 (every slot of every track), 6 on the modulated groove. Run 25 picks its depths by target number,
so it now translates the old numbers to keep modulating the same knobs (without that it read 3,197: a different
set of knobs, COLOR's among them, not a cost).

**TUNE plays live now.** SYNTH and POLY used to add TUNE to a note when its key went down, so a modulated TUNE
only reached the next note. The listening examples caught it: a SEQ on TUNE over a held note played one pitch. Now
the voice adds TUNE every block (SYNTH to its pitch, POLY to its head's step), so a SEQ on TUNE over a held note is
an arpeggio, and (with the 1/16-semitone pitch below) an LFO on it is vibrato. Played normally, nothing changes. One catch: SEQ's steps are 0..100 %
and TUNE spans 48 semitones, so a step lands on the nearest semitone (25 % is an octave, 15 % a fifth, 21 % a minor
seventh); a step page that shows semitones on a TUNE target would be the friendlier way, later.

**How well everything modulates** (2026-10-09, `tests/mod_audit.sh`). I measured every knob a slot can move rather
than guess: track 1 alone, the knob's device on and the knobs it depends on set, 3 s without and 3 s with an LFO on
it (1 Hz, depth 50; 100 for a switch), and for SYNTH and POLY once over a played phrase and once over a held note.
`tests/mod_audit.py` reports how much the sound changes, whether it jumps where the control blocks start (a knob
applied a block at a time in steps does), and, with `--cost`, the instructions.

- Every knob moves the sound. The ones that seem not to act on their own need what they act on: STRT a LEN under
  100, GRAIN's OFST a SCAN on POS or DLY, its SCAL some PRND, POLY's DEC a SUS under 100; ROTA acts at the next
  start, DUB and GAIN while recording; ATK DEC REL VOIC GLID (and POLY's STRT) at the next note. A switch (REV,
  HALF, WAVE, TYPE, CMOD...) needs a depth of about 70 to flip: the swing has to cross half a step.
- RIPPLE's PTCH was the one that jumped: the strings' delay changed length a block at a time, and their read
  point jumped (34 clicks a second on a sweep, a 4.6 dB edge at the blocks' starts). Now the root glides half the
  way a block and each string reaches its new length sample by sample across the block: no edge, no clicks. A key's
  root still lands at once (it's a new note, and a pluck). It costs nothing while the pitch holds.
- The pitch knobs (SYNTH's and POLY's TUNE, RIPPLE's and GRAIN's PTCH) modulate in 1/16 semitones now (`mod_f16`,
  read through `mod_pitch16`): a knob holds semitones, so an LFO on TUNE used to step, a trill rather than vibrato.
  Now a small depth is vibrato. A SEQ's steps still land on semitones (a step is a note).
- I got one thing wrong on the way, and the tool says so in its header. My first pass flagged SYNTH (MIX, CUT, ENV,
  RES, TUNE) and SPACE's TIME as clicking and buzzing at the blocks' rate. They weren't stepping: a saw's own edges
  count as clicks, and a moving pitch or filter smears its harmonics into the blocks' frequencies. I smoothed them
  anyway (MIX, NOIS and DRV ramped, the filter in four steps a block, TIME's target ramped), measured no difference
  in what the edge test finds, and 25 to 50 more instructions a sample on the groove (runs 18, 19), so I took that
  back out. The edge test is the one that tells.
- TAPE's HALF flags as a step: it's a switch, the speed changes at once by design.
- The channel's LOW stepped (found 2026-10-11, and fixed): the shelf's gain was set once a block, so an LFO on it
  jumped up to a dB at every block's start (the audit's edge: 15.0 dB against 0.3 without). Three things were wrong,
  all in `mix_channel`: the gain jumped at the block's edge; a shelf passing through 0 dB stopped running, so its
  low-pass held a stale value and pulled toward it when the gain came back; and the strip switched off the moment
  LOW HIGH and FILT read 0, cutting a gain still on its way down. Now both shelves' gains ramp across the block from
  last block's (as PAN's already did), a shelf runs while its gain is non-0 this block or the last, and a shelf coming
  back on seeds its low-pass from the signal as it is. Tests: LOW and HIGH swung 0/+6 and -12/+12 dB a block (the
  largest sample-to-sample move 14,589 before, 656 after; HIGH 12,605 and 3,464), and a shelf back on after the
  signal moved stays on the signal's side. The audit now finds no knob stepping.

  The ramp's per-sample arithmetic cost on a strip whose gains hold, so the loop has a held path without it
  (`ch_side`, inlined with the ramp a constant). On the host a strip with every shelf and the filter on still costs
  about 32 a sample more (run 17: 1,753 to 1,785), and none of it is arithmetic: it's three extra register moves a
  sample in the shelves, the x86-64 compiler laying out the inlined copies differently. I tried the gain as its own
  flag (as the old loop had it) and four steps a block instead of a ramp (worse: 1,816, and a 24 dB swing became
  four 6 dB steps); neither helped, and the device's compiler (32-bit, -Os) lays it out its own way, so I left it.
  Everything else moved by under 15.
- Cost: a slot with one depth adds about 20 instructions a sample; what the device does with the moving knob comes
  on top, in proportion (the channel filter switched on, more grains at a higher RATE, more notes overlapping with a
  longer REL). Nothing costs more just for being modulated.
- The filters' cutoffs modulate finely too (2026-10-09): SYNTH's, POLY's and RIPPLE's CUT and the channel's FILT
  join the pitches on the fine path, which now keeps 1/256 of a knob's step (`mod_f8`; pitches read it as 1/16
  semitones through `mod_pitch16`, cutoffs as their Q8 through `mod_fine8`). A knob holds whole semitones of the
  filter's scale (FILT whole percent), so a slow, deep, resonant sweep climbed a staircase: on a 0.06 Hz sweep,
  a semitone every 110 ms. Now the cutoff moves a little every block. The proof is in the tests (each filter gets a
  value off the semitones on 3 blocks in 4 and never moves more than half a step between blocks); my audio measure
  (the resonance's peak tracked over noise) was too jittery to see a staircase that fine either way, so the before
  and after renders are for the ear. Cost: 5 more on the modulated groove (run 24: 1,798), the channel filter's
  coefficients now made every block while FILT moves. Unmodulated, every cutoff is exactly what it was.
- LVL still moves in whole dB under modulation (ramped across each block, so no step is heard as a click).

**The clock** is one counter of beats (Q16), counted exactly from the tempo with its remainder carried, reset when
PLAY starts and running on while stopped, so a synced LFO still moves when you're tweaking a stopped loop.

**Load** (host instructions per output sample, `tests/checkpoint_sim.sh`):

| Run | What | Total | Modulation itself |
| --- | --- | ---: | ---: |
| 1 | four reels, nothing modulated | 934 (929 before) | 5 (the clock) |
| 19 | the four-track groove, unmodulated | 1,595 | 0 |
| 24 | the groove modulated the way I'd play it: an LFO a beat on T1's crush, a SEQ on T2's grain size, T1's drums ducking T2's filter through FOLLOW, the ADSR opening T3's synth cutoff, a stereo LFO on T4's pan | 1,793 | 84 |
| 25 | every slot on every track, 32 depths each (128) | 3,123 | 469 |

So the realistic case costs about 195 on top of the groove: 84 for the modulators and the rest is the devices
doing more work because their knobs move (the channel filter re-tuning every block, the synth's cutoff, GRAIN's
grains at changing sizes). My budget said 10 for the modulators; that was a guess at the summing alone, before
four engines with curves, interpolation and a clock. The groove modulated stays under the 2,000 I set for a
realistic four-track scene. Run 25 is the ceiling, and it's not a scene anyone plays: it modulates whatever the
first 32 targets of each slot are (since the sources' LEVEL pages, NPK 20, those land on other knobs, and more
devices wake: 3,123, up from 2,858; the modulation itself 469, 24 of it the pitch knobs' finer path).

**Memory**: the depths and their undo 2.9 KB of main RAM when this was written (485 B since the RAM pass: a sorted
list a track, see "Budgets"); the lists, the slots' state and the
modulated arrays 5.9 KB of the pool (now 322.4 of 336 KiB). Code about 9 KB on the host measure.

**What's still open:**

- **P-locks** (a step's own depth: a SEQ slot's pad and a white key held, then a knob). The SEQ's step values are
  in; per-step depths are a decision I haven't had an answer on, so `docs/controls.tsv` lists them as proposed.
- **LFO RATE with SYNC BPM** printed its 0..127; since phase 9 it prints the division.
- **Seeing it move.** Since phase 9, a rail under each modulated knob with a block where it is now; the pictures
  below still draw the knobs as set.
- **SPRD on PAN, a known consideration** (decided 2026-10-09 to leave it for now). SPRD gives the left and right
  channels their own pan; PAN is a balance (tracks are stereo), so at SPRD 100 the two swing apart in opposite
  directions and each turns its own side down at the same time: the sound pulses (a tremolo) instead of moving.
  Around 50 it circles. The S-4's SPREAD means the same (the right channel's modulation shifted against the left),
  but there it probably reaches every stereo parameter. Options I laid out: keep it; make SPRD on PAN a width
  (always movement); or let SPRD reach every knob of the parts that already run left and right apart (COLOR, the
  channel strip), the S-4's way, measured before it's kept. My lean is the last.

**Tests** (`test_mod`, `test_mod_engines`): no depths, no copy; the reels and empty knobs can't be targets; an LFO
at 100 sweeps PAN end to end, at 25 a quarter each way, clamped at the ends; AMT 0 silences a slot; SPRD mirrors
the right pan; SYNC BPM gives one cycle a beat at 120 BPM (689 blocks); ADSR opens on a key to the top, holds SUS,
releases, and fires a one-shot when PLAY starts; SEQ steps on time, forward, reverse, ping-pong, random, with SLEW's
glide, SWNG's late off-beat, PROB 0 holding; LOOP comes round while held; FADE starts quiet; VAR drifts RND's steps
and the cycles' levels; FOLLOW rises with a loud source, falls back, and with HOLD moves only on the beat; the 33rd
depth is refused; the gestures (hold + knob on a device, the source and the channel; tap opens; hold + SELECT; MONO
held clears and SAVE undoes, once; a SEQ page's keys and KNOB 1). The checkpoint's stress turns depths, engines and
slot knobs and clears and undoes while the audio interrupt cuts in, and checks every modulated knob stays in its
range and every array is read from the right place.

**Modulation reaches the sound (a guard in the suite, 2026-10-09).** test_mod and test_mod_engines check the knob
values modulation produces; `test_mod_audio` checks the audio. For a knob of each kind (TAPE's SPD, COLOR's DRV,
SYNTH's CUT under an ADSR with a note held, POLY's TUNE, DRUM's TUNE under a SEQ, the channel's FILT under FOLLOW),
1.1 s of track 1 with the depth against the same without: each differs by 2 to 5 dB of the sound for the pitch and
drive ones (the waves no longer line up), -10 and -15 dB for the filters; and a depth of 0 renders exactly the sound
without one. It compares the track's own output, before the output stage, because the DC blocker, speaker EQ and
limiter carry their state over from whatever played before (the first version compared after them and called two
identical renders different). With the ISR's depth list emptied on purpose, all six fail.

## DRUM, as built (2026-10-09)

DRUM is the fourth source: a drum machine on a track. I wanted the feel of a CR-78, the preset rhythm box with its
soft, round, slightly toy-like kit, so the instrument list and the character come from there. The sounds, the
synthesis and the rhythms are Bryo's own, written from scratch: no samples, no copied circuits, and no code or
values from Felucca's drum engine. (The first pass carried Felucca's voice code with three types added; I replaced
it outright because the kit should be ours.) "CR" or "CR-78" only ever appears in the docs, never on screen.

**The kit.** Sixteen instruments, one per white key, left to right: BD SD CP RS LC LB HB CL CB MA TB GU HH OH MB CY
(bass drum, snare, clap, rim shot, low conga, low bongo, high bongo, claves, cowbell, maracas, tambourine, guiro,
hi-hat, open hat, metal beat, cymbal). The hi-hat and the metal beat cut the open hat short (1.5 ms), the way one
pedal would. The user picked more sounds over spare keys: accent isn't a key, it's ACNT on PATTERN and the ACC
mode on STEP.

**One voice, sixteen patches.** `drum_voice.c` is a single small synthesizer every instrument shares; each
instrument is one row of numbers in `drum.c` (`DRM_KIT`). I went data-driven on purpose: tuning the kit is editing a
table, not code, and every instrument costs the same code path. A voice has three optional layers:

- **TONE**: one or two sine partials (the second at a ratio and level), struck: the pitch starts BEND above and
  falls back with its own time, the body decays from the strike. The bass drum, the congas and bongos, the rim
  and the claves live here.
- **NOISE**: white noise, a metal bank (four square waves at inharmonic ratios, or a bell's two a flat fifth
  apart), or a mix, through one filter (low, band or high) under three envelopes: a rise subtracted from the rest
  (maracas swell in), a fast HIT that BURSTS can re-strike (the clap's hands, the guiro's teeth, the tambourine's
  jingles) and a slow TAIL that can wait for the last burst (the clap's room).
- **DRIVE**: a soft clip with its small-signal level kept.

The envelopes pick their form by range: the fast ones are per-sample Q15 products (`e = e k >> 15`, which always
reaches zero); the slow ones are Q30 per control block, ramped linearly inside it, because a per-sample Q15 factor
can't hold a decay of seconds. Every strike starts from rest (phases, filter, the noise seed), so a hit renders the
same every time. The levels are matched to a -7 dBFS peak per instrument; the voice's output is linear to -6 dBFS
with a soft knee above.

**The pages** (HOME, four of them; HOME turns the page as it's let go on a DRUM track, so it can be held):

| Page | KNOB 1 | KNOB 2 | KNOB 3 | KNOB 4 |
| --- | --- | --- | --- | --- |
| PATTERN | PATN: a written rhythm, written over the pattern when turned | LEN: 2 to 4 bars | SWNG: off-sixteenths up to a third of a step late | ACNT: an accented step's extra punch |
| VARY | SEED: 0 as written, 1 to 200 a version | VARY: how far a version strays | FILL: the chance of a fill in the last bar | EVOL: a new version every 1, 2, 4 or 8 loops |
| KIT | TUNE: every instrument, semitones | DEC: every decay, a quarter to four times | TONE: every filter, an octave each way | DRV: drive over the kit |
| STEP | INST: the instrument the keys write | BAR: the bar shown (OCT- / OCT+ too) | MODE: HITS, ACC, LIVE, ERAS | LVL: the source's level |

PATN, LEN, INST, BAR and MODE aren't modulation targets (`pdesc_t.nomod`): PATN writes the pattern, the rest are the
page's own state. Everything else is, so an LFO on SEED, or a SEQ stepping it, gives a groove that keeps finding new
versions.

**The keys.** On PATTERN, VARY and KIT the white keys play the kit; holding one makes KNOB 1 to 4 that instrument's
own TUNE, DEC, LVL and TONE (the header's box shows its code; the picture shows its sound). On STEP they follow MODE:
HITS toggles step n of the bar for INST, ACC toggles the step's accent, LIVE plays and, while the transport runs,
writes the hit at the nearest sixteenth, ERAS wipes the held instrument as the playhead passes.
HOME held + a white key picks INST and plays it. POLY held clears the pattern; SAVE held brings it back (a PATN
turned over an edited pattern keeps the edit for SAVE the same way).

**The written rhythms.** Ten, as readable strings in `DRM_PRESET`: ROCK, DISC, FUNK, SHFL, BOSA, RMBA, CHA, BGIN,
TNGO, MRCH. They're my arrangements of the styles a preset rhythm box offers, two bars each, a starting point to edit
or vary. A track starts on ROCK.

**SEED, and how a version varies.** This is the part I care most about getting musical. SEED 0 is the pattern as
written. Any other value is a version, and a version is a pure function of (version, bar, step, instrument): a hash
decides each choice, so the same SEED always plays the same thing and the screen can draw it. It isn't noise
sprinkled on the grid. Each instrument varies the way a player would vary that part:

- The backbone (BD, SD, CP) is never dropped. The bass drum picks up a note on the weak sixteenth before one of its
  own hits; the snare adds quiet ghost notes on the sixteenths of a beat it already plays in.
- The time (HH, MA, TB) thins on the weak sixteenths and fills in on the eighths, quieter, and only in a bar where
  it already plays. A closed hat on the last eighth may open. The open hat itself never fills in.
- The colour (RS, the congas and bongos, CL, CB, GU, MB) adds ornaments next to its own hits, and a conga or bongo
  hit can move to another drum of the family the pattern uses: the part's melody changes, not its rhythm.
- VARY scales it, and the first bar strays least (60 %) and the last bar most (100 %), so the loop still reads as
  its pattern.
- FILL is the chance the last bar ends in a fill: a roll on the pattern's snare (else its clap, rim, claves or a
  bongo), a run down the congas it has, or a stutter of the bass drum against the roll's instrument. Then a crash on
  the next downbeat, on the cymbal or else an open hat, only if the pattern has one.
- Nothing the pattern doesn't use comes in (the one exception: a closed hat opening). A rock beat never sprouts a
  cowbell; a bossa never a snare. The tests hold it to that over 200 versions of ROCK, BOSA and RMBA.

Quiet hits (ghosts, fill-ins) play 7 dB down. EVOL moves the version on by one every N passes of the pattern, so a
loop can evolve on its own while SEED stays where it was set.

**Timing.** Steps come from the modulation clock (`mclk.beat`, Q16 beats, exact, reset at PLAY): a sixteenth is
16384, and SWNG pushes the odd steps up to a third of a step late, the same way SEQ swings. A step's hits strike at
the start of the block it lands in (within 0.73 ms). The tests check the hits land on the grid to the block.

**Who owns what.** The main loop owns the pattern (`tp[t].dpat`, `dacc`, `dins`). LIVE and ERAS happen in the ISR,
which sees the key the moment it goes down, so their writes go through a ring of 32 the main loop applies
(`drm_post` / `drm_poll`). A LIVE hit pressed late in a step is written on the next one and struck now, and a skip
mask stops the next step striking it again. The voices (`drm_rt`) are ISR-only and live in the pool.

**Voices.** Five a track. An instrument re-struck takes its own voice (a drum machine's way: a new hit cuts the last
one off), else a quiet voice, else the oldest. A voice that's rung out (-84 dB) stops computing, and inside a voice
a layer that has gone quiet is skipped (the bass drum's 2 ms thud doesn't cost its whole second).

**What it costs** (host instructions per output sample, `checkpoint_sim.sh`): DRUM busy alone (run 26: FUNK at
VARY 100, FILL 100, long decays, the others plain reels) 1,308, against 938 for the reels alone; the four-track groove
with a DRUM track in place of its drum reel (run 27) 1,898; four DRUM tracks at once, every voice (run 28) 2,482 (2026-10-10: the hits
triggering the modulators and SEQ's AMT OFS added about half a percent).
The first version of the voice loop cost 1,473, 2,071 and 3,720: it decided per sample what's fixed for a block
(the filter's mode, the metal's squares, drive) and kept computing layers that had gone silent. Splitting it by
layer, specialising each loop and skipping quiet layers brought the realistic groove back under my 2,000 target.

**Tested** (`tests/bryo_host.c`, `test_drum_voices` and `test_drum`): every instrument rings out, peaks in range,
has no DC and renders the same every hit, and an accent is louder; the chokes; two bars of ROCK play exactly as
written and on the grid; swing; SEED's determinism, SEED 0, the backbone never dropped, nothing foreign coming in
(ROCK, BOSA, RMBA), the congas varying on their own drums; EVOL; DEC's scale; the voice steal; PATN, POLY's clear
and SAVE's undo (and an empty clear); STEP's HITS and ACC, OCT on the bar, HOME + key; an instrument's own knobs;
LIVE (no double strike) and ERAS. The LIVE and ERAS checks were confirmed to fail with their logic taken out. The
modulation audit covers DRUM's targets; FILL and EVOL show "no change" there because they act at the pattern's end,
past the audit's 3 s window (a two-bar loop at 120 BPM is 4 s).

**Open, for later:** the kit is voiced by measurement (pitch, level, decay) and waits on a listen; the hats and
maracas are bright (energy to 17 kHz), the bass drum sits low for a small speaker. The cymbal takes 9 s to ring out
at its designed decay. The patterns aren't saved yet (phase 8, with everything else).

## Consistency review: sources, devices, modulators (2026-10-10)

I went through every page (the descriptor tables in `param.c`), every gesture (`controls.tsv`, `ui_input.c`) and
how the modulators meet each source (`mod.c`, `chain.c`), looking for the same idea done two ways. Here's what I
found and what I did with it.

**Already one rule everywhere** (I checked, nothing to change):

- The destructive holds: POLY (tape, or a DRUM pattern), MONO (modulation) and PATN over an edited pattern all
  take half a second and one level of SAVE-held undo. Now the slot pad + MONO clear joins them.
- A pad held + SELECT picks that pad's thing: HOME the source, a slot its engine.
- A slot's pad + a knob sets a depth on any page that has targets: the four devices, every source's own knobs, the
  channel strip. The things the main loop acts on (the reels, PATN, DRUM's LEN INST BAR MODE, ROUT) are left out
  the same way, by `nomod` or `mod_tdesc`.
- Every source keeps its own knobs when you switch away and back; every source ends on a page with LVL, -24..+6 dB.

**Fixed now:**

- **The same filter list had two names.** RIPPLE's `SLOP` and POLY's `TYPE` both choose LP / BP / HP (the same
  `N_SLOP` names). It's a type, not a slope, so RIPPLE's is `TYPE` now.
- **Drive and decay were spelled two ways.** COLOR said `DRIV` where SYNTH and DRUM say `DRV`; DRUM said `DECY`
  where SYNTH, POLY, RIPPLE, SPACE and the ADSR say `DEC`. Both now use the common spelling (labels only: the
  code's enum names stay, so nothing else moves). The COLOR and RIPPLE pictures had their own copies of the
  labels; they read the descriptors now, so a rename can't leave them behind again.
- **The ADSR modulator showed raw numbers.** Its ATK DEC REL run on the same `TIME_MS_X10` curve as SYNTH's and
  POLY's envelopes, which show milliseconds and seconds; the modulator's showed 0..127. They show times now.
  (FOLLOW's RISE and FALL stay numbers: they're a different curve, `mod_coef`.)
- **A DRUM track's loop, as the modulators saw it, was 4 bars.** The ADSR restarts (one-shot, or LOOP) on the
  track's loop: TAPE's is the tape's, SYNTH's and POLY's every 4 bars. DRUM got the 4 bars too, so with LEN 3 the
  envelope drifted against the pattern. `drm_block` now marks the pattern's start (`mod_seam`), the way `tape.c`
  marks the tape's, and the 4-bar phrase is only SYNTH's and POLY's.
- **OP5 and OP6 flipped a hidden tape.** On a SYNTH, POLY or DRUM track they toggled the TAPE's REV and HALF out of
  sight, which you only found when you switched back to TAPE. They now say "TAPE TRACKS ONLY" and leave it alone.
- **The undo hint read two ways** ("HOLD SAVE: UNDO" for the tape and DRUM clears, "SAVE: UNDO" for MONO's). It's
  "HOLD SAVE: UNDO" everywhere, since a tap of SAVE saves.
- **The controls map** had HOME's hold under the slot-held context and a POLY-only HOME row that the general row
  already covered; both are fixed.

**Decided and built (the six questions this review left open):**

1. **One place for the output knobs.** A source's LVL (and COLOR's) is KNOB 4 of its last page; ROUT sits just
   left of it, or on KNOB 4 where there's no LVL. TAPE 3 reads REEL ROTA . LVL, SYNTH 5 and POLY 4 . . . LVL,
   COLOR 3 . . ROUT LVL, RIPPLE 3 . . . ROUT, DRUM's STEP already ended in LVL. GRAIN's pages 3 and 4 swapped,
   so its ROUT (with PATN SCAL PRND) is on its last page and SCAN WARP OFST FDBK come first. (Superseded for ROUT
   on 2026-10-11: an effect's ROUT and WET are KNOB 3-4 of its first page now, "Every effect opens with its mix".)
2. **SYNTH turns its pages in POLY's order**: OSC, AMP (the envelope), FILTER, VOICE, LEVEL.
3. **AMT and OFS are KNOB 3 and 4 of every modulator's last page.** LFO's page 3 is its timing (SYNC TRIG FADE),
   page 4 PHAS SPRD AMT OFS; ADSR already ended VEL LOOP AMT OFS; FOLLOW's page 2 is HOLD SPRD AMT OFS. SEQ has a
   third page, . SPRD AMT OFS: AMT and OFS place its steps the way they place every engine's output, and SPRD lags
   its right side as ADSR's and FOLLOW's do (0: the right side is the left, exactly).
4. **A DRUM pattern's hits are its notes.** Each step that strikes something sets a bit (`mod_hit`, from
   `drm_block`), and the next block treats it as a key going down on that track: an ADSR plays a one-shot (attack,
   decay, release: a hit has no length to sustain over), and an LFO or SEQ on TRIG KEY restarts. It works on every
   DRUM track, focused or not, since the pattern plays whatever's focused.
5. **HOME turns the page as it's let go, on every source** (the slot pads' way). Held, it does something else
   instead: + SELECT picks the source from any page (and shows it), + a white key picks DRUM's instrument. The cost
   is a page turn on the let-go rather than the press, a few tens of milliseconds.
6. **OCT moves DRUM's bar only on DRUM's own pages.** On the track's device pages it's the octave again, so a
   DRUM track's RIPPLE root can change octave like any other.

How the pages move without anything else moving: the knob tables stay in the order the knobs are stored, and a
layout (`PLAYOUT` in `param.c`) says which stored knob each page position shows. Only the code that turns a page
and a KNOB into a knob goes through it (`ui_page`, `page_target`, the pictogram lookup, the picture's
last-turned knob, `pdesc_pages`), so the DSP, the modulation targets (a depth is stored by knob, not by place),
the pictures and the projects are untouched. A test checks every layout puts each knob on exactly one place and
that the two rules hold. The one stored change is SEQ's three new knobs, so the project format is version 2: a
version 1 project's SEQ slots get AMT 100 (and SPRD, OFS 0) instead of the 0s they stored there, which would
have silenced them.

## The arpeggiator, the keys as a keyboard, SETUP (2026-10-10)

Four things from one round of playing it on the FM-1, built together because they meet at the keys and the pads.

**The pads.** ARP was the fourth modulator slot; the modulators now are three that swap between LFO, ADSR and
FOLLOW, plus the step sequencer. The LFO pad holds the three (M1 M2 M3): held + OP1, OP2 or OP3 picks the one it
shows (`ui.lfo_slot`), and from then a tap opens that one's page and a hold sets its depths. ENV stays M2 straight
away (so M2 is reachable two ways; both pads light on its page). SEQ is M4 and its engine never changes (SELECT with
it held says so). The slots keep their stored numbers so nothing saved moves: slot 0 M1, slot 1 M2, slot 2 the SEQ
(shown M4), slot 3 M3, the old ARP slot, still a random LFO to start (`SLOT_NUM`, `SLOT_OF_NUM` in param.c turn one
into the other for the screen). An old project whose slot 2 isn't a SEQ, or whose other slots are, loads as it was;
only the engine switch keeps to the new rule.

**The arpeggiator** (`arp.c`, one per track, `tp[t].arp`, its page on the ARP pad). It's a filter on the key mask:
chain.c gives it the keys held on a SYNTH or POLY track and the note of bit 0 (`key_base`), and takes back a mask
with at most bit 0 set, `key_base` set to the step's note. Between steps there's always a silent block (GATE is
clamped a block short of the step), so the source sees a new key press each step and starts its envelope, and so do
the modulators' KEY triggers. SYNTH and POLY needed only `key_base` instead of "C of the octave" and 32 key bits
instead of 16. The notes it holds are absolute (0..127), in the order pressed: KEY keeps the keys held, LTCH keeps
the last chord after they're all let go (a press after that starts a new one), ADD keeps every key ever pressed
(past 32 the oldest goes; POLY held on its page, or MODE to OFF, clears it). ORDR walks RNGE octaves of them UP,
DOWN, UPDN (the ends once), RND or PLAY (pressed order); `arp_place` is the walk, shared with the screen's preview.
Its clock is its own: the first note sounds on the key press, each step RATE long at the tempo (60 x 44100 / (BPM
x steps a beat): 1/16 at 120 is 5,512 samples), SWNG moving a third of a step at most from the second of each pair
to the first. It runs whether the transport plays or not. A latched pattern on a track you're not looking at plays
on, and its KEY triggers still fire (`mod_xkeys`). Cost: about 4 host instructions a sample for all four
(checkpoint run 1: 940 to 944, with the new reels). It isn't a modulation target (its knobs are choices acted on per
step) and it isn't in track presets (the music, like the pattern), but it's in projects (version 3).

**The keys as a keyboard.** The 27 keys are a piano's from F3 to G5 (hal/fm1_input.h); Bryo had the white keys as
16 semitones from C so the black keys could be the PRD's macros. SETUP's KEYS on NOTE (or GLO held + OCT+) makes
them a keyboard on SYNTH and POLY: the mask is `fm1_in.notes` itself, `key_base` the F of the octave (53 at OCT 3).
The black keys' shortcuts are off then, all of them (a key can't be a sharp and a mute), except under the LFO pad,
where OP1-OP3 still pick. TAPE's slices, DRUM's kit, GRAIN's cursor and RIPPLE's root keep the white keys as
they were.

**SETUP**, the mixer's last page: KEYS, KNOB, HOLD and LEDS, the instrument's, not a project's. HOLD and LEDS were
Felucca's MENU settings (kept in its fields all along); KEYS and KNOB are a byte of their own in the settings record:
favorites.factory[15][0], an engine Felucca never had, so going back to Felucca reads nothing from it. KNOB is how
far a detent moves a value with more than 32 steps: 1X, 2X, 4X, or ACC, Felucca's acceleration (#52: the time
between detents makes the multiplier, 1 to 4, so a slow turn stays fine). Choices always move one a detent. ACC
also reaches the tempo; 2X and 4X leave it a BPM a detent.

**A blank project.** OCT+ on an EMPTY slot in the project view starts a new project there (`pj_new`): every knob at
its default, every track on its own empty tape (TAPE's and POLY's REEL on TAPE), no drum pattern, no depths, nothing
in the arpeggiators, the mixer and tempo as at power-on. It's the current project from then (SAVE saves it there),
and the slot shows EMPTY until it's saved. Power-on with nothing saved still plays the factory reels, which are now
generative patches (`tools/gen_reels.py`: KEYS a Turing machine on the piano, AIR a sample-and-hold flute pad, PLUK
two Euclidean string rhythms against each other; one bar each, so they loop together with BEAT). They didn't last
the day: on the FM-1 they were annoying, and the piano and flute went for synthesized ones, BELL (FM bells), DRON
(a drone) and FOLD (wavefolded plucks), same length, same keys, the scene unchanged.

**The factory scene** (later the same day). Power-on with nothing saved used to be the four reels and nothing
moving; now it's a scene the modulators keep changing (`tpf_scene` in tpfactory.c, the factory presets' loader on
four track recipes): T1 DRUM on FUNK, VARY 70, FILL 50, EVOL every two loops, M3 (random, two bars a step) moving
SEED 45 % so it picks a new version of the pattern, M4 stepping the kit's TUNE; T2 BELL through grains (WET 55)
whose PTCH M4 steps a beat at a time through 16 steps of A minor pentatonic (0 3 5 7 10 12 semitones: steps of
0 26 43 59 84 100 at 25 % of PTCH's 48), no slew, M3 scattering SPRY, an echo; T3 DRON through strings (WET 45 on A2)
whose root M4 walks A C D E G a bar a step, and grains with PRND 12 held to SCAL MIN, so the random pitches stay in
key; T4 FOLD on half its loop, M3 moving STRT 40 % each bar (a new phrase) into a delay whose feedback M1 breathes over
four bars. FACTORY in the project view (turn past P6) loads it again: `pj_factory` sets the defaults and the scene,
and leaves no slot current, so SAVE tapped on a page says to pick one rather than overwrite the last project. Tested
(test_factory_scene): over eight bars SEED takes 15 versions, T2's grains 6 pitches all on the pentatonic, T3's
strings 5 roots, T4's loop 19 starts, the peak about -10 dBFS; FACTORY loads over a saved P1 and leaves it.

**The scene, arpeggiated** (later still). Two of the scene's tracks became POLY tracks played by their
arpeggiators, which the owner suggested on hearing it: T2 strikes BELL on a latched Am7, RND over two octaves in
eighths, and M4 steps POLY's TUNE +0 +5 +7 +0 a bar at a time, so the chord goes Am7 Dm7 Em7 Am7 and every note stays
in A minor; T4 plucks FOLD up and down A minor pentatonic in swung sixteenths with the same four steps. POLY plays a
reel at its own pitch on ROOT's key, so ROOT is set to the reel's first note (BELL and FOLD each start with one
clean note for it, `TPS_BELL_NOTE`, `TPS_FOLD_NOTE`; until ROOT existed, TUNE was set to 48 minus it). For a scene, or a project, to hold an arpeggio the
arpeggiator's notes had to outlive a power cycle: projects (format 4) save them after its knobs, and loading hands
them to the ISR (`arp_seed`: the notes in a buffer, `RING_PUBLISH`, then the request), latched. And notes held with
no key down now follow the transport: STOP quiets them, PLAY starts them from the first step, a key brings them back
at once; keys held still play whether the transport runs or not. Tested: stopped, the scene's arpeggios wait; eight
bars give T2 64 steps over 16 notes and 3 chords, every one in A minor, T4 10 notes; STOP stops them with their notes
kept; a project saved with them brings them back.

How it's tested (`tests/bryo_host.c` test_arp_keys): KEY UP at 1/16 plays C3 E3 C3 E3 with a silent block between
steps; RNGE 2 UPDN and DOWN walk as they should; KEY stops when the keys go, LTCH keeps a chord and replaces it on a
new press, ADD piles up, POLY held clears; OFF passes the keys through, a TAPE track gets no arpeggio; GLO + OCT+
flips the keys and keeps it, a black key plays F#3 and doesn't mute, white key 1 is F3, OP1-OP3 still pick under
LFO; depths follow the LFO pad's pick; SETUP 4X moves a 0-100 value 4 a detent and a choice 1, ACC goes further on
a fast turn and stays fine on a slow one; a project keeps the arpeggiator. The screens: ARP, ARP in ADD, SETUP, a
new blank project.

## USB record mode (2026-10-08)

Recording from the computer is a mode of its own, not a source a track plays through: you go in, record, put the
take on a track and come out. Nothing else runs meanwhile, which keeps it light and keeps it simple to reason about.

**Going in.** With the transport stopped, hold REC for a second (a hint shows after 0.3 s). REC used to arm on the
way down; now, stopped, it arms on the way up, so a held REC can mean something else. Arming while stopped has no
timing to keep (nothing records until PLAY), so nothing is lost. Playing, REC still arms on the way down, because a
punch-in has to land where you press it; holding it then only says to stop first.

**While it's up.** The transport stops, every track's REC lets go, and the tracks stop rendering: the audio ISR fades
them out over one block and plays the computer instead. Their GRAIN buffers, RIPPLE strings and SPACE lines go back
to the shared memory (their polls see `sys.usbrec`), so the take can use it. The FM-1's own USB input, what the
computer can record from it, sends silence: otherwise whatever the computer plays would come straight back to it.

| Step | REC | HOME | White keys | KNOB 1-4 |
| --- | --- | --- | --- | --- |
| READY | start the take | leave | - | - |
| RECORDING | stop it | stop, throw it away | - | - |
| CHOOSE | keep it on the picked track (its tape replaced) and leave | throw it away (READY) | 1..TRACKS pick the track (lit) | START, LENGTH, GAIN (NORM past +24 dB), FADE |

The screen keeps to what the step needs: the four tracks as numbered boxes with what each holds, the step's word or
the take's seconds large, one level bar, and the room left.

**A long take shouldn't eat the other tracks by surprise.** A 20 s take is most of the shared memory, so what a take
costs is on screen from the moment it records: under the running time, the memory as one ribbon (the other tracks'
tapes dim, the take in ink growing, what's left a dotted line). In CHOOSE the take loops so you hear it, its outline
shows the kept part in ink and the trimmed part dim, the ribbon shows the kept part only (and leaves out the tape it
would replace), and the numbers say KEEP 1.30 and how many seconds the other tracks would have left. Trimming it
gives that room back as you turn.

- **START, LENGTH** move a block (11.6 ms) a detent, four or sixteen when turned fast. Blocks are the unit because
  each decodes on its own: keeping a trimmed take moves its blocks down to the start of its chunks with a plain
  copy and gives back the chunks past the end.
- **GAIN** is -24..+24 dB; one detent past +24 is NORM, the loudest block of the kept part to full scale (from the
  peaks the encoder already stored, so it costs nothing to show).
- **FADE** (0..500 ms, 10 to start with) fades in and out at the cut points, so a cut mid-sound doesn't click; the
  loop's seam in the preview gets the same fades.
- What's kept is decoded and encoded again only where it changed: every block when GAIN isn't 0 dB, otherwise only
  the blocks under a fade. The rest keep their bits, so trimming alone adds no second generation of ADPCM noise.

**How the sound gets in.** `usb.c` (behind `BRYO_UAC_OUT`, on in Bryo, off in the update loader, whose descriptors
stay byte for byte) adds a second UAC1 streaming interface: the computer plays into the FM-1, 16-bit stereo at
44.1 kHz on EP4 OUT, isochronous and adaptive. `uaco_service` takes each 1 ms packet in TIMER5, from `usb_poll` and
nested in the render as the input's `uac_service` already was. It hands the frames to `usbrec.c`, which writes two
rings:

- **The take** goes in as mono at 22,050 Hz, the tape's format: two frames, both sides, into one sample, as a WAV
  over the drive does. The main loop encodes it into ADPCM blocks in chunks of the shared memory it takes as it goes
  (free ones, or cleared and parked tapes'; never a tape in use). The take is written on the computer's clock, so it
  never drops or repeats a sample, whatever the DAC's clock does.
- **The monitor** is the frames as they came. The DAC runs at 44,117.6 Hz against the computer's 44,100, so the
  audio ISR reads the ring a hair slower than it plays, steered by how full the ring is (within 1 %, about 0.04 % in
  practice). A dropped or doubled frame every 57 ms would click; this bends the pitch by an amount nobody hears.

A take is at most a tape's 23.6 s, or what memory there is. When either runs out it stops there and moves on to
CHOOSE with a message. Putting it on a track uses `tape_replace()`, which I pulled out of the drive's WAV import so
both hand chunks over the same way.

**Tests.** `test_usbrec` holds REC playing (a hint, no mode) and stopped (the mode, with no arm on the let-go). The
devices give their memory back. A 441 Hz sine from the computer is heard at 440.8 Hz with no jump, the ring holding
steady against the two clocks. A 3 s take in packets of 44 and 45 frames keeps every frame (357 blocks, none lost),
asks for a track and loops in CHOOSE at 300.0 Hz. Trimmed to 0.50..1.50 s it keeps 6 chunks instead of 23; NORM
on a take peaking at 7,500 is x4.34; at GAIN -6 dB and FADE 100 ms it lands on track 3 as 86 blocks at half level,
faded in and out, and plays back at 300.0 Hz. With 3 chunks free the take stops at 48 blocks, and
HOME throws it away and leaves. `usb_sie_test` (UAC in and out) parses Bryo's descriptors as a host would (five
interfaces, the AC header and its terminals, the stream's endpoint) and runs the endpoint against the emulated
controller: a packet before the computer picks the stream is dropped, then 20 packets polled twice a frame all
arrive in order, and a bus reset turns the stream off. The interrupt stress enters and leaves the mode, records and
places takes (2,720 in 20 s) with packets arriving from the TIMER5 signal, and the memory books balance.

**What only the device can tell.** That the JieLi controller's EP4 OUT works as its register map says (the receive
DMA address, isochronous mode, the receive count): I have the SDK's register names, not a run. And how computers
treat a new sound output. macOS may switch to an output it hasn't seen when it's plugged in; if Bryo grabs the Mac's
sound every time it's connected, the next step is to present the output only while the mode is up (a re-enumeration,
which would also drop and remount the drive).

**Future work: audio out.** The other way, the FM-1 as the computer's input, has existed since Felucca (EP4 IN, the
master output at 44.1 kHz). It hasn't been checked against Bryo's chain yet; it's on the hardware run sheet.

## Memory by usage, TRACKS and REC IN (2026-10-08)

Until now every track had a 36 KB tape whether it used it or not, and the plan reserved a delay and a reverb per
track the same way. That's 4 × 3.3 s of tape however you play, with a third of the RAM sitting idle on tracks doing
nothing. Now there's one pool and each track takes what it uses (`mem.c`).

**Chunks.** The pool is 143 chunks of 16 tape blocks (4,096 samples, 186 ms each): 119 in the pool region (128 until 2026-10-11, then 126: two went back to keep the pool's 8 KiB headroom, seven more to the audio code in RAM on 2026-10-12) and 24 in
main RAM's spare room, 26.6 s of tape-format sound in all. A tape is a list of chunks, and a reader looks a block up
through the list (one more load per block decode). The main loop hands chunks out and takes them back; the audio ISR
only reads the lists. Taking a chunk away shortens the list first, then frees it, and that's safe to reuse at once:
the main loop never runs inside the audio ISR, so a block being rendered finishes before the next step, and every
later block sees the shorter list. Nothing in the ISR holds a pointer into a chunk from one block to the next.

**Tapes grow as they record.** REC on a blank tape starts the head at the loop's start when the transport plays; the
tape grows behind it, the main loop keeping a chunk ready ahead (`tape_poll`). Letting go of REC sets the loop's
length. A reel copied in takes exactly the chunks it needs. A track that never records holds nothing. One tape holds
23.6 s at most (127 chunks): the head's position is kept in 1/4096ths of a sample in 32 bits, which runs out at 23.8 s.

**A WAV over USB never allocates.** It arrives in `usb_poll`, which runs in the TIMER5 interrupt and can cut into
the main loop halfway through handing a chunk out. So while the drive is up the main loop keeps two chunks set aside
(free ones, or a cleared or parked tape's), and the capture takes from those through a ring with one writer on each
side, the main loop topping it up between USB packets. A capture dropped halfway keeps its chunks for the next one
to write over, and only the main loop frees them, with the drive held busy. Those two chunks (372 ms) are the price
of the drive being plugged in.

**When nothing is free**, chunks come off the end of a tape, in this order: a cleared tape (its undo goes), a parked
track's tape (the longest), then the longest tape of all, never the one asking and never one that's growing. A WAV
arriving over USB only takes free chunks, cleared and parked tapes, never a tape in use; what doesn't fit is cut and
the message says so. When there's nothing left to take, a growing tape stops and loops what it has.

**TRACKS** (GLO held + SELECT, 1-4) says how many tracks are in use. It's a held pad and SELECT, like HOME + SELECT
picking the source and a slot + SELECT its engine, so plain SELECT stays the tempo on every view and a tempo turn on
the mixer can never park a track mid-song. The ones above it park: they fade out in 2 ms
and stop rendering, REC on them lets go, the focus and GLO's track keys stay below it. Nothing is erased: a parked
tape is just the first taken when memory runs short, and raising TRACKS brings back what's still there. Fewer
tracks also means more for the ones left: GRAIN's buffer holds more bars and a track sounds more grains. The mixer's
levels page shows it all under the faders: each track's seconds of tape (and GRAIN buffer), OFF for a parked one,
the memory as one ribbon (tapes solid, buffers dim, free dotted), TRACKS and the time still free. EDIT (held, or tapped to latch it) still
gives the channel.

**REC IN** (ALGORITHM): what each track's REC records, KNOB 1-4 a track each, ALGORITHM turned again the focused
track's. AUTO is what REC always did: the others' mix on a TAPE track, its own source on a SYNTH or POLY track. OTHR
is the others' mix whatever the source; T1-T4 one track, after its devices; a track's own number reads SELF and
records it back onto itself, DUB setting how much of the last pass stays. So T1 into T2 into T3 into T4 is three
knob turns, and every bounce can add its own effects. The picture draws the routes you've set as lines between the
tracks (solid while that REC is armed) and names the defaults, with the routing of the last-turned track in words.

**Reels across slots.** The six user reel slots are 40 KiB each. A sound up to 3.3 s fits one, laid out exactly as
reels always were (so old reels still read). A longer one runs into the slots after it, its data in one piece, up to
21.5 s across all six; its header says how many it spans. The slots it covers read as empty but aren't free.
Whether a slot holds a sound is checked (header, CRC) once, at power-on and after every save, and kept: the audio
ISR asks every block, and a long reel's CRC takes longer than a block.

**What the review round found** (a reviewer agent read the new code against the single-core model, and I checked
each finding): a tape over 127 chunks overflowed its Q12 head (now capped at 23.6 s); the WAV capture allocated
from the TIMER5 interrupt (now the spares above); a parked track's grains held their slots forever, since a parked
track isn't rendered (they're let go when it parks); every block the ISR re-ran a user reel's flash CRC (now
cached); a block staged on a tape whose list was replaced could be written into the new one (a generation count
drops it). Smaller: a growing tape keeps two chunks ready ahead instead of one, a blank tape ignores DUB (there's
nothing to keep), and a GRAIN buffer shrunk under its write head keeps the head's place in the bars.

**Tests** (`test_memory`): a blank tape grows while REC records it and stops at its length, looping what it recorded;
stealing takes a cleared tape first, then a parked track's, then the longest, never from a tape in use for an import;
a tape that can't grow loops what it has, and REC with nothing anywhere is refused; a 700-block reel spans three
slots and plays from flash, a one-slot reel keeps the old layout, a save into a covered slot breaks the long reel; a
10 s WAV lands whole on a tape, a 6 s one as a reel across two slots, and one with no memory at all is refused; TRACKS
parks and brings back a track; REC IN T1 records track 1 alone, and a SYNTH track on OTHR records the others.

## Build order

Each phase ends in something you can flash and hear or see, and each is its own commit series.

| Phase | What | Done when |
| --- | --- | --- |
| 1. Skeleton (**done**, host-verified) | `bryo.c` boots on the kept hardware layer; the old app code is removed; silence plus a test tone; the header and an empty strip; install, UBOOT and calibration still work | it installs from the web installer and returns to stock |
| 2. TAPE + reels (**done**, host-verified; see "Phase 2, as built") | tapes play factory reels; the loop window, speed, reverse, half speed, FADE; slices on the white keys; REC and overdub (resampling the other tracks); clear and undo | you can load, slice, record and overdub a loop |
| 3. USB audio in + SYNTH + POLY (**done**, host-verified; see "SYNTH, as built", "POLY, as built" and "USB record mode") | the source_t interface; the UAC OUT endpoint, drift handling, a record mode; the SYNTH source; the POLY source (voices with their own envelope and filter, three HOME pages) | you can record your computer, and play the synth and samples onto a tape |
| 3b. GRAIN (**done**, host-verified; see "GRAIN, as built") | the scheduler, the shared pool of grains, FREEZE (key 0); the live buffer, SCAN, FDBK | grains run on 4 tracks inside the budget |
| 3c. Memory by usage (**done**, host-verified; see "Memory by usage, TRACKS and REC IN") | the shared chunks, growing tapes, TRACKS, REC IN, long reels | memory follows what you use |
| 4. RIPPLE (**done**, host-verified; see "RIPPLE, as built") | strings, chromatic keys, OCT shifts | tuned feedback chords from the keys |
| 5. COLOR + SPACE (**done**, host-verified; see "COLOR, as built" and "SPACE, as built") | drive, crush, noise; delay and reverb | the full chain on 4 tracks inside the budget (on the host it isn't: everything on all four is 5,842; the device decides) |
| 6. Mixer + routing (**done**, host-verified; routing early as REC IN; see "The mixer, as built") | GLO mixer, filters, compressor | the mixer's DSP |
| 7. Modulation (**done** but p-locks, host-verified; see "Modulation, as built") | the 4 engines, hold-and-turn depth, assigning engines, SEQ steps, MONO's clear; p-locks wait on a decision | the PRD's §4 workflow end to end |
| 7b. DRUM (**done**, host-verified; see "DRUM, as built") | the fourth source: sixteen synthesized instruments, patterns of 2 to 4 bars, ten written rhythms, SEED's versions, STEP's HITS ACC LIVE ERAS | a groove from a rhythm, varied, edited, printed onto a tape |
| 8. Projects (**done**, host-verified; see "Projects, as built") | save and recall with reels; quick SAVE; the project view; the last project at power-on (user reels came in phase 2, over USB, so no upload tool) | a power cycle brings a session back |
| 9. Screen (**done**, host-verified; see "Phase 9: the strip shows modulation") | the dot-grid screens (early); a rail under each modulated knob: its reach, where it's set, a block where it is now (every depth summed); LFO RATE as a division | the PRD's §5 |
| 10. Tools + docs | the upload tool for reels, the installer text, a Bryo manual | someone else can use it |

Phases 2 and 7 are the riskiest: the tape format, and whether the modulation sum fits at control rate. If
either needs rethinking, it'll show up early.
