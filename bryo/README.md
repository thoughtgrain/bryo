# Bryo

Bryo is firmware for the M-VAVE FM-1 that turns it into a 4-track generative sound-sculpting instrument. Each
track starts with a source (a tape loop, a synth, more to come) and runs through a fixed chain: GRAIN,
RESONATOR, COLOR and SPACE. Four modulator slots per track shape all of it, set by holding a pad and turning a
knob, with no menus.

It's a fork of [Felucca](https://github.com/hugelton/Felucca) by Leo Kuroshita / Hügelton Instruments, and it
keeps Felucca's hardware layer, update path and installer, which is why installing it is as easy as installing
Felucca. Thanks go upstream: see [ACKNOWLEDGEMENTS.md](ACKNOWLEDGEMENTS.md) and [NOTICE.md](NOTICE.md).
Felucca's own README is kept in [docs/FELUCCA-README.md](docs/FELUCCA-README.md).

**New to Bryo? Start with [the manual](docs/manual/README.md):** step by step, with the screens.

## Where it stands

**Phases 1 to 9 of 10, and running on an FM-1 since 2026-10-10** (p-locks still to decide; the whole picture, with the file map and what's open,
is in [docs/bryo-status.md](docs/bryo-status.md)). Each of the four tracks has a tape: a loop in RAM (IMA ADPCM at 22.05 kHz) that plays when
you press PLAY. The tracks share one memory (28 s in all) and each takes what it records: a blank tape grows while
REC records it, and TRACKS (GLO held + SELECT) switches tracks off to give the rest more. To start, track n plays factory reel n (BEAT, KEYS, AIR, PLUK, made from material in
this repository). The loop window, speed, reverse and half speed work; the white keys play 16 slices of the loop;
REC records the other tracks onto the focused track's tape (or one chosen track, or itself: REC IN, turn ALGORITHM),
with DUB for sound on sound; holding the POLY key clears
a tape and holding SAVE undoes it. A track can also start with SYNTH, POLY or DRUM instead (hold HOME, turn SELECT):
SYNTH is a small subtractive synth, up to three voices; POLY plays any reel (or the track's own tape) across the
keys, up to four notes, each with its own envelope and filter; DRUM is a drum machine (CR-78-inspired, every sound
synthesized and Bryo's own): sixteen instruments on the white keys, a pattern of 2 to 4 bars from ten written
rhythms, and SEED, which plays a varied version of the pattern the way a player would. REC prints any of them onto
the track's tape. GRAIN
makes sound now: turn its WET up and it keeps the last bars of the track's sound in a live buffer and granulates
them, stretched, held at a spot or delayed (SCAN); the 0 black key held freezes the buffer into a loop. RESONATOR
makes sound too: four tuned strings the track rings through (PTCH, SCAL's chord), and on its page the white keys set
the root and pluck them. COLOR
(drive, crush, noise that rides the sound, tilt) and SPACE (a delay and a small room, taking memory only while DLY or
VERB is up) make sound as well. The mixer's channel (LOW, HIGH, FILT, PAN) and a master compressor
(GLO, then EDIT, EDIT again for MASTER) shape the mix. The four modulator slots run (an LFO, an ADSR, a step sequencer,
an envelope follower): hold a slot's pad and turn any knob on a device, source or channel page to set how far that
slot moves it, and the keys, the loop and the tempo drive them. The knobs carry the Torso S-4's names where they do the same job
([docs/s4-alignment.md](docs/s4-alignment.md)). It runs on the FM-1 since 2026-10-10 (the hardware checkpoint is under way). The plan, with what the hardware allows and the order of the work, is in
[docs/bryo-architecture.md](docs/bryo-architecture.md).

What works on the panel now (every control, in every context, with what's planned for it, is in
[docs/controls.tsv](docs/controls.tsv): tab separated, one row per control and gesture, checked by
`tests/controls_check.py`):

| Control | What it does |
| --- | --- |
| HOME | the track's source (TAPE, SYNTH, POLY or DRUM), as it's let go; again for its next page (TAPE 2, TAPE 3: REEL, ROTATE and LVL; SYNTH's AMP, FILTER, VOICE, LEVEL; POLY's ENV, FILTER, LEVEL). Held + SELECT, from any page: the source. LVL, KNOB 4 of every source's last page, is what the track hears of it, and can be modulated |
| HOME held + SELECT | the track's source: TAPE, SYNTH, POLY or DRUM (each keeps its own knobs) |
| EDIT | GRAIN; again: GRAIN 2, GRAIN 3 (SCAN WARP OFST FDBK), GRAIN 4 (PATN SCAL PRND ROUT), RESONATOR, RESONATOR 2, RESONATOR 3 |
| FX | COLOR; again: COLOR 2, COLOR 3, SPACE, SPACE 2, SPACE 3 |
| ROUT (the last page: GRAIN 4, RESONATOR 3, COLOR 3, just left of LVL where there is one) | INS: WET crossfades the dry sound with the device's; SEND: the dry stays at full and the device is added at WET, a send per track (SPACE's DLY and VERB always work that way) |
| LFO ENV SEQ, tapped | the modulators (the page opens as you let go; again for its next page): LFO holds M1, M2, M3 (each an LFO, ADSR or FOLLOW), ENV is M2, SEQ is M4, the step sequencer |
| LFO ENV SEQ, held + a knob | that modulator's depth to the knob on the page shown (-100 to 100 %); the strip shows its depths while held |
| LFO held + OP1, OP2, OP3 | which modulator the LFO pad is: M1, M2, M3 |
| LFO ENV, held + SELECT | the modulator's engine (LFO, ADSR, FOLLOW); its depths stay. M4 stays the sequencer |
| ARP | the arpeggiator (SYNTH and POLY tracks): MODE (OFF, KEY, LTCH, ADD), ORDR (UP, DOWN, UPDN, RND, PLAY), RNGE 1-3 octaves, RATE; page 2 GATE, SWNG. POLY held on its page clears its notes |
| GLO held + OCT+ | the black keys: the shortcuts, or notes (the 27 keys a keyboard, F to G) |
| GLO, then EDIT x3 | SETUP: KEYS, KNOB (a click moves 1X, 2X, 4X, or ACC: faster turns go further), HOLD, LEDS; kept for the instrument |
| White key held + KNOB 1, on a SEQ page | that step's value (the keys pick steps there, they don't play) |
| GLO, held | the mixer while held: white keys 1 to 4 pick the track, KNOB 1 to 4 set the levels, SELECT sets TRACKS (1 to 4 in use) |
| GLO, tapped | the mixer stays up; tap again or press a page pad to leave |
| EDIT on the mixer | KNOB 1 to 4 set the selected track's LOW, HIGH, FILT and PAN: held, while held; tapped, until tapped again |
| KNOB 1 to 4 | the four values on screen |
| SELECT | the tempo, on every view |
| ALGORITHM | REC IN: KNOB 1 to 4 set what each track's REC records (AUTO, the others, one track, itself); turned again, the focused track's |
| PLAY | start and stop: every tape plays its loop from the start |
| REC | arm the focused track: while playing it records onto its tape the other tracks (TAPE) or its own sound (SYNTH, POLY); a reel is copied on first |
| SAVE, tapped | save the project: the whole instrument (every knob, source, pattern, depth, the mixer, the tempo) into the project loaded last (P1 to start with). A track's own recorded tape goes into a user reel named after it (P2T3). The last project saved comes back at power-on |
| PRESETS | the project view: six slots; turn to point at one, OCT+ loads it (an EMPTY one: a new, blank project there), SAVE saves into it, POLY held deletes it, OCT- goes back. EDIT turns to its second page, the track PRESETS (A..H): a track's sound (its source, devices, channel strip, slots and depths, not its pattern or level). OCT+ loads one into the focused track, SAVE saves the focused track there. Past H come eight factory presets, two a source: DRIFT and RINGS (TAPE), BASS and PAD (SYNTH), KEYS and SWELL (POLY), TIGHT and DUB (DRUM) |
| SAVE, held | undo the last clear (the tape, the track's modulation, or a DRUM pattern), or the last track preset load or SCL reset |
| OCT− / OCT+ | the white keys' octave |
| White keys | TAPE: the 16 slices of the focused track's loop (stopped: the slice plays once); SYNTH, POLY: notes, a semitone apart from C; DRUM: the kit (on STEP: steps, accents, live hits, or held to wipe) |
| Black keys OP1 to OP4 | track mutes |
| Black keys OP5, OP6 | the focused tape's reverse and half speed |
| Black key MONO, held 0.5 s | clear the focused track's modulation (every depth; the slots' own knobs stay) |
| Black key POLY, held 0.5 s | clear the focused track's tape (a DRUM track: its pattern) |
| Black key 0 | GRAIN's freeze: held, while it's held; tapped, latched until the next tap |
| SCL, held | turn a knob: it goes back to its default. Press the page's own pad (the lit one): the whole page does. SAVE held puts it back |

The screen is drawn like a groovebox OLED: a grid of 2 x 2 px dots, bitmap type, white on black (your palette
setting is kept, but these screens don't use it for now).
Each knob is a pictogram that shows its value (a dial's pointer, a fader's fill, a slope's length, a keyboard
with the root lit, a feedback ring that wobbles into chaos as it rises), with a 4-letter label under it. Under the four of them, each page plots what it does in the
same dotted language: a reel-to-reel with the sample on the tape and the loop bracketed on it, the sample lit where grains read
it with one block per grain, the resonator's peaks with a node on each harmonic, a sine through COLOR (with an
inset that follows your last knob), SPACE's echoes as stems and its tail, and each modulator's shape (ADSR names
its stages, SEQ numbers its steps). The last knob you turned has its label inverted in both places.

Not there yet, and on purpose: Felucca's MENU (palette, speaker EQ, LEDs, USB serial, ABOUT and credits). Your
settings from Felucca are kept and still apply; the menu to change them comes back in a later phase.

## Files over USB

Plugged in, the FM-1 also shows up as a small drive named BRYO. Copy `TAPE1.WAV` .. `TAPE4.WAV` off to keep what
each track plays; copy a WAV on to load it: named `TAPE2.WAV` it replaces track 2's tape, any other name goes to
one of six user reels in flash (named after the file). Any WAV works (mono; a tape keeps as much as free memory holds, a user reel up to 21.5 s).
New files show up after you eject and plug back in; deleting a file on the computer doesn't delete the sound.
The details, and why it works this way, are in [docs/bryo-architecture.md](docs/bryo-architecture.md) under
"Files over USB".

## Recording from the computer

With the transport stopped, hold REC for a second: the USB record mode. Everything else stops, and the FM-1 plays
whatever the computer sends it (pick Bryo as the computer's sound output). REC starts a take and REC stops it. Then the
take loops while KNOB 1-4 trim it (START, LENGTH), set its level (GAIN, or NORM past +24 dB) and fade its ends
(FADE); the screen shows how much memory it takes and how much the other tracks would have left. White key 1-4 picks
the track, and REC keeps it there and takes you back. HOME goes back a step (a take you don't want, or out). A take
is up to 23.6 s, or what memory is free.

## If it crashes

A crash shows a red BRYO CRASH screen for 4 s and restarts by itself; a hang restarts it after 8 s (the watchdog).
Turning it off and on does the same. What's in RAM is gone (the tapes, the knobs, until projects arrive in phase 8);
the reels saved to flash and the settings stay. If it crashes in the first 30 s twice in a row, it starts in the
chip's update mode instead of looping, and the web installer puts Bryo or the stock firmware back.

## Building and testing

The device build needs JieLi's toolchain and SDK, as Felucca's did: see [BUILDING.md](BUILDING.md), then
`./build.sh`. The host tests need only a C compiler and Python (with Pillow and fontTools; clang and node add
checks when they're there):

```sh
pip3 install Pillow fonttools
tests/run_tests.sh
```

They type-check the firmware with clang, run the hardware layer's tests, run Bryo's chain, input and screens on
the host, check every screen against its pixel fingerprint, and test the installer. With a device build they also
test the update path against the real package.

`tests/mod_audit.sh` (needs numpy; `--cost` adds valgrind) checks how every knob a modulator can move behaves:
whether it changes the sound, whether it steps at the control blocks, and what it costs.

## Licence

GPL-3.0-only, like Felucca. See [LICENSE](LICENSE), [LICENSING.md](LICENSING.md) and [NOTICE.md](NOTICE.md).
