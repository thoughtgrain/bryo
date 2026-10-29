# Bryo

Bryo is firmware for the M-VAVE FM-1 that turns it into a four-track sound-sculpting instrument. Each track starts
with a source (TAPE, SYNTH, POLY or DRUM) and runs through a fixed chain: **GRAIN, RIPPLE, COLOR, CHRONOS**, then a
channel strip into the mix and a master compressor. Four modulators per track can move any knob, and an
arpeggiator can play what you hold. There are no menus: you hold a pad and turn a knob.

It's a fork of [Felucca](https://github.com/hugelton/Felucca) by Leo Kuroshita / Hügelton Instruments, and it
keeps Felucca's hardware layer, update path and installer, which is why installing it works the same way. Thanks
go upstream: see [ACKNOWLEDGEMENTS.md](ACKNOWLEDGEMENTS.md) and [NOTICE.md](NOTICE.md). Felucca's own README is
kept in [docs/FELUCCA-README.md](docs/FELUCCA-README.md).

**To play it, start with [the manual](docs/manual/README.md):** small steps, each with the screen you should see.
The website, <https://thoughtgrain.github.io/bryo/>, has the short version.

## What's in it

- **Four sources.** TAPE: a loop in RAM (IMA ADPCM, 22.05 kHz) with 16 slices on the white keys, a loop window,
  speed, reverse, RPT, wow and flutter. SYNTH: a small synth, up to three notes, with classic waves and FM, phase distortion, vowel and wavetable ones. POLY: a
  sampler that plays any reel, or the track's own tape, across the keys. DRUM: a CR-78-inspired drum machine of
  Bryo's own synthesized sounds, ten written rhythms, and SEED, which varies a pattern the way a player would.
- **The chain.** GRAIN granulates the track's last bars from a live buffer (and has two other forms, WARBLE and
  VINYL); RIPPLE is strings, combs, resonators or a 12-band bank; COLOR is drive, crush, noise and tilt; CHRONOS is
  a delay and a send to one of two shared rooms. GRAIN, RIPPLE and COLOR can each be an insert or a send.
- **Modulation.** Four slots a track: three behind the LFO pad (each an LFO, ADSR or FOLLOW) and SEQ, a step
  sequencer with p-locks. Hold a slot's pad and turn any knob to set how far it moves it.
- **Arpeggiators.** One a track, on every source, with its own rhythm; RIPPLE has a second one that plucks its
  strings.
- **Recording.** REC prints the other tracks, one track, or the track itself onto its tape; overdub, bounce, freeze
  to tape, and recording what a computer plays over USB.
- **Scenes and projects.** Four scenes a project, recalled with a glide or morphed between; six project slots and
  eight track presets in flash; the last project saved comes back at power-on.
- **One shared memory.** About 20 seconds of sound shared by the four tracks, handed out by what's in use: a tape
  grows as you record it, and the effects take memory only while they're on.
- **Files over USB.** Plugged in, the FM-1 is a small drive called BRYO: copy WAVs on and off.

Every control in every context is in [docs/controls.tsv](docs/controls.tsv) (one row per control and gesture,
checked by `tests/controls_check.py`); the manual's last section is the short version of it.

## Where it stands

Phases 1 to 9 of 10 are built, and Bryo has run on an FM-1 since 2026-10-10. The hardware checkpoint
([docs/hardware-checkpoint.md](docs/hardware-checkpoint.md)) is under way: the CPU readings on the device are the
numbers everything else waits on. There's no release yet, so you build it yourself.

[docs/bryo-status.md](docs/bryo-status.md) is the page to read first if you want to work on it: the state, the
file map, how to build, test and measure, the decisions and what's open. Each part, as built, is in
[docs/bryo-architecture.md](docs/bryo-architecture.md).

## Building and testing

The device build needs JieLi's toolchain and SDK, as Felucca's did: see [BUILDING.md](BUILDING.md), then
`./build.sh`, then `tools/fm1_install.py build/bryo.fwsc` to install it.

The host tests need a C compiler and Python with Pillow and fontTools (clang and node add checks when they're
there). The font generator uses Pillow's font features, so Pillow needs libraqm; a plain `pip3 install Pillow` on
macOS may not have it.

```sh
pip3 install Pillow fonttools
tests/run_tests.sh                 # must end "ALL HOST TESTS PASSED"
```

They type-check the firmware with clang, run the hardware layer's tests, run Bryo's chain, input and screens on the
host, check every screen against its pixel fingerprint, and test the installer. With a device build they also test
the update path against the real package. `tests/checkpoint_sim.sh` measures the load of each checkpoint run
(valgrind), and `tests/mod_audit.sh` (numpy) checks how every knob a modulator can move behaves.

## Licence

GPL-3.0-only, like Felucca. See [LICENSE](LICENSE), [LICENSING.md](LICENSING.md) and [NOTICE.md](NOTICE.md).
