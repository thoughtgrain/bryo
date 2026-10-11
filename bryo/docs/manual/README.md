<!-- SPDX-License-Identifier: GPL-3.0-only -->
# The Bryo manual

Bryo turns the M-VAVE FM-1 into a four-track instrument for sculpting sound. Each track has a **source** (a tape
loop, a synth, a sampler or a drum machine) and runs through four effects in a fixed order: **GRAIN, RESONATOR,
COLOR, SPACE**. Four **modulators** per track can move any knob, and an **arpeggiator** can play your chords for
you. There are no menus: you hold a pad and turn a knob.

This manual is a series of small steps. Each one does one thing, shows the screen you should see, and says what you
should hear. Do them in order the first time; after that, jump to what you need.

> The pictures are the real screens, drawn by the same code that runs on the FM-1. The values on them are the ones
> the tests set, so yours may differ a little.

**Contents**

0. [Before you start](#0-before-you-start): the panel, reading the screen, how pages work
1. [First sounds](#1-first-sounds): power on, play, tempo, volume, mutes, picking a track
2. [Playing with a tape](#2-playing-with-a-tape): slices, the loop, speed, reels
3. [The mixer](#3-the-mixer): levels, tone and pan, the compressor, SETUP, the CPU meter
4. [Recording](#4-recording): clear a track, record onto it, overdub, undo
5. [Other sources](#5-other-sources): SYNTH, POLY and DRUM; the arpeggiator; the black keys as notes
6. [The effects](#6-the-effects): GRAIN, RESONATOR, COLOR, SPACE
7. [Modulation](#7-modulation): make knobs move by themselves
8. [Getting back](#8-getting-back): reset a knob or a page, undo
9. [Saving](#9-saving): projects, a blank project, the factory scene, track presets
10. [With a computer](#10-with-a-computer): files over USB, recording the computer
11. [When something goes wrong](#11-when-something-goes-wrong)
12. [Quick reference](#12-quick-reference)

---

## 0. Before you start

### 0.1 The panel

![The FM-1's panel, labelled for Bryo](img/panel.svg)

- **Top left**, four labelled knobs: **MASTER** (volume), **SELECT** (tempo), **PRESETS** (projects and presets),
  **ALGORITHM** (what REC records). Under them, **OCT−** and **OCT+**.
- **Top right**, **KNOB 1 to 4**: they always change the four values on the screen.
- **The pads**, two rows:
  - **Blue: the pages.** HOME (the track's source), EDIT (GRAIN, RESONATOR), FX (COLOR, SPACE), GLO (the mixer).
  - **Purple: the modulators and the envelope.** LFO holds three you can swap around (M1, M2, M3), SEQ is M4, the
    step sequencer. **ENV** is the track's own envelope: SYNTH's and POLY's, TAPE's slices', DRUM's KIT.
  - **ARP** is the arpeggiator.
  - **Green: the transport.** PLAY and REC.
  - **SAVE** saves (held: undo). **SCL** held resets things.
- **The keys:** 16 white keys play the track. The 11 black keys above them have their own jobs (mutes, reverse,
  clears, freeze), written above each one in the picture.

### 0.2 Reading the screen

<img src="img/screen.png" width="380" alt="The TAPE page with numbered marks">

1. **The track** you're on: T1 to T4. (On a modulator's page it says M1 to M4, the slot.)
2. **The page's name**, and dots for which of its pages you're on (TAPE has four: `▪···` is the first).
3. **The transport and the tempo.** ■ stopped, ▶ playing, ● armed to record; then the tempo (120 BPM). A **✱**
   before them means you've changed something since you last saved (see 0.3); most pictures in this manual show it.
4. **The four knobs.** A little picture, a name, a value. KNOB 1 is the left one, KNOB 4 the right one. The last
   one you turned has its name inverted.
5. **The picture** of what the page does: here the tape, with the loop bracketed (IN and OUT) and the play head.
6. **What the white keys do** on this page (here: play slices).
7. **The track again, and the octave** of the white keys.

### 0.3 How pages work

Every pad opens a page, and **pressing the same pad again goes to its next page**. Then it goes round to the first.

| Pad | Pages, in order |
| --- | --- |
| HOME | the source's pages (TAPE has 4; SYNTH 5; POLY 4; DRUM 4) |
| EDIT | GRAIN 1-6, then RESONATOR 1-3 |
| FX | COLOR 1-3, then SPACE 1-3 |
| ENV | the source's envelope: SYNTH's AMP, POLY's ENV, TAPE's slice envelope (each the second page), DRUM's KIT |
| LFO, SEQ | a modulator's pages (LFO: M1, M2 or M3; SEQ: M4) |
| ARP | the arpeggiator: 2 pages, then RES ARP (RESONATOR's own arpeggio) |
| GLO | the mixer (EDIT then steps CHANNEL, MASTER, SETUP) |

KNOB 1 to 4 always change the four values you see. There's nothing hidden.

**Jump straight to a page: hold its pad and press a white key.** While you hold a pad (HOME, EDIT, FX, ENV, LFO,
SEQ or ARP), the white keys show its pages: one glows for each page, the page on screen is lit, and every other key
goes dark. Press white key 3 to go to the third page. EDIT's keys run through GRAIN's pages and then RESONATOR's
(key 8 is RESONATOR 2); FX's through COLOR's and then SPACE's. Every effect's first page ends with its mix:
**ROUT** on KNOB 3 and **WET** on KNOB 4 (SPACE: **DLY** and **VERB**, its two sends), so one press of EDIT or FX
and KNOB 4 brings an effect in. Holding LFO also lights OP1-OP3, the one it's on
bright. Let go without pressing a key and the pad turns its page as usual.

### 0.3 Saving, in short

- **Nothing is kept until you tap SAVE.** The **✱** in the header says there are changes you haven't saved; it goes
  away when you save.
- **Tap SAVE:** everything goes into the current project (**P1** to start with). Turn the FM-1 off and on and it's
  all back.
- **Hold SAVE:** undo. It brings back the last thing you cleared, reset or loaded (a tape, a drum pattern, a
  track's modulation, a page you reset, a preset you loaded).
- **Holding the black POLY key for half a second clears things:** the track's tape (on DRUM, its pattern; on an ARP
  page, the notes it holds). The screen says "KEEP HOLDING POLY TO CLEAR..." first; let go then and nothing happens.
- **Lost something that SAVE held can't bring back?** If you haven't saved since, go back to your last save: turn
  **PRESETS**, point at your project, press **OCT+**, and **OCT+** again to confirm. Everything since that save
  is gone, the lost track comes back.
- **Projects and presets:** a project (P1 to P6) is everything at once; a track preset (A to H) is one track's
  sound. Both are in [9](#9-saving).

---

## 1. First sounds

### 1.1 Turn it on

1. Turn the FM-1 on.
2. **BRYO** shows for a moment.
3. Then you're on **track 1's TAPE page**.

<img src="img/boot.png" width="240" alt="The boot screen"> <img src="img/tape.png" width="240" alt="TAPE, track 1">

> **Don't hold OCT− and OCT+ while you turn it on.** That starts the hardware calibration (see
> [11.3](#113-a-calibration-screen)).

### 1.2 Press PLAY

1. Press **PLAY**.
2. The ■ in the header turns into ▶, and the play head moves along the tape.

**You'll hear:** the factory scene, four tracks that keep changing by themselves:
- **Track 1:** a drum machine that varies its pattern as it goes.
- **Track 2:** FM bells (**BELL**) played by the arpeggiator: a chord at random over two octaves, the chord moving
  every bar (Am7, Dm7, Em7).
- **Track 3:** a warm drone (**DRON**) ringing strings that walk from note to note, with a shimmer of grains.
- **Track 4:** wavefolded plucks (**FOLD**) arpeggiated up and down in swung sixteenths, following track 2's chords,
  into an echo.

It's all in A minor; the modulators and the two arpeggiators do the changing.

> If you've saved a project, it comes back at power-on instead. To hear the factory scene again, see
> [9.4](#94-back-to-the-factory-scene).

<img src="img/tape_playing.png" width="240" alt="TAPE while playing">

3. Press **PLAY** again to stop. Every loop starts from the top when you play again.

### 1.3 Change the tempo

1. Turn **SELECT**. It works on every page.
2. The number in the header changes (40 to 240 BPM).

The tapes keep their own speed (that's SPD, in [2.3](#23-speed-and-backwards)). The tempo drives everything that
keeps time: the drum machine, the modulators, and how many bars GRAIN holds. You'll hear it once you get there.

### 1.4 Volume

Turn **MASTER**. It's the volume of everything, on every page.

### 1.5 Mute a track

1. Press black key **OP1**: track 1 goes silent.
2. Press it again: back.
3. **OP2, OP3, OP4** do the same for tracks 2, 3 and 4.

### 1.6 Pick a track

Almost everything you do changes **the track you're on** (T1 in the header).

1. **Hold GLO.** The mixer appears.
2. While holding it, **press white key 2**. The box moves to T2.
3. Let go of GLO. You're back on the page you were on, now for **track 2** (the header says T2).

<img src="img/mixer_held.png" width="240" alt="The mixer while GLO is held">

---

## 2. Playing with a tape

A **TAPE** track plays a loop. These steps work on any track whose source is TAPE (all four, to start with). Press
**PLAY** first.

### 2.1 Play slices with the white keys

The loop is cut into 16 slices, one per white key.

1. Press **HOME** to be on the TAPE page.
2. Press **white key 1**, then **white key 9**.

**You'll hear:** the loop jumps to the start (key 1), then to halfway (key 9). Tap keys in rhythm to remix it.
Stopped, a key plays its slice once.

### 2.2 Make the loop shorter

1. On the **TAPE** page, turn **KNOB 2 (LEN)** left to about 50 %.
2. The brackets on the tape close in: the loop is now half as long.
3. Turn **KNOB 1 (STRT)** to slide the loop along the tape.

<img src="img/tape_loop.png" width="240" alt="A shorter loop, moved along the tape">

### 2.3 Speed, and backwards

1. On the **TAPE** page, turn **KNOB 3 (SPD)**. 100 % is normal speed.
2. Turn it below 0 and the tape plays **backwards**. −100 % is backwards at normal speed.

<img src="img/tape_speed.png" width="240" alt="SPD at -140 %">

**You'll hear:** the pitch follows the speed, like a real tape.

### 2.4 The quick ones: reverse and half speed

- Black key **OP5**: the track's tape backwards, on or off.
- Black key **OP6**: half speed, on or off.

They're the same switches as REV and HALF on TAPE's page 3.

<img src="img/tape2.png" width="240" alt="TAPE 3: XFAD REV HALF GAIN">

### 2.4b Shape the slices: TAPE's envelope

Press **ENV** (or HOME to the second page): **ATK DEC SUS REL**, an envelope for the slices you play from the keys.

- **Stopped**, a key's slice rises over ATK, falls over DEC to SUS while you hold the key, and fades over REL when you
  let go. Out of the box it changes nothing: a tapped slice plays to its end, as before.
- **Playing**, the loop keeps going as it is; a key's jump fades in over ATK (a long ATK makes the jumps swell in).

<img src="img/tape_env.png" width="240" alt="TAPE 2: the slice envelope">

### 2.5 Play a different reel

1. Press **HOME** until the dots show the last page, **TAPE 4**.
2. Turn **KNOB 1 (REEL)**: BEAT, BELL, DRON, FOLD, then any reels you've added. **TAPE** means the track's own tape
   (what you record onto it).
3. **KNOB 2 (ROTA)** moves where the loop starts; **KNOB 4 (LVL)** is how loud the track hears its source.

<img src="img/tape3.png" width="240" alt="TAPE 4: REEL, ROTA, LVL">

---

## 3. The mixer

### 3.1 Set the levels

1. **Hold GLO.**
2. **KNOB 1 to 4** set track 1 to 4's levels (0 to 127; 100 is the default). Each fader's fill moves with the
   sound.
3. Let go of GLO to go back.

<img src="img/mixer.png" width="240" alt="The mixer">

### 3.2 Keep the mixer open

**Tap GLO** (press and let go quickly). The mixer stays up, so both hands are free. Tap GLO again, or press any
page pad, to leave.

### 3.3 Tone and pan for one track

1. Open the mixer (tap GLO) and pick the track (hold GLO + white key 1-4).
2. **Tap EDIT.** The **CHANNEL** page: **LOW** and **HIGH** (bass and treble, ±12 dB), **FILT** (left of centre
   a low-pass, right a high-pass), **PAN**.

<img src="img/mixer_channel.png" width="240" alt="CHANNEL: LOW HIGH FILT PAN">

### 3.4 The master compressor

1. In the mixer, **tap EDIT twice**: the **MASTER** page.
2. **AMT** is how much it squeezes, **ATK** and **REL** how fast, **MIX** how much of it you hear.
3. Tap EDIT once more to get back to the levels.

<img src="img/mixer_master.png" width="240" alt="MASTER: AMT ATK REL MIX">

### 3.5 Fewer tracks, more memory

The four tracks share one memory (28 seconds of sound in all). You can switch tracks off to give the others more:

1. **Hold GLO** and turn **SELECT**: TRACKS 4, 3, 2 or 1.
2. The tracks above the number go silent and **OFF**, and their memory is free for the others.

<img src="img/mixer_tracks3.png" width="240" alt="TRACKS 3: track 4 off">

### 3.6 SETUP: how the panel behaves

The mixer has a fourth page with your preferences. They belong to the FM-1, not to a project, and they're kept when
you turn it off.

1. Tap **GLO**, then tap **EDIT three times** (CHANNEL, MASTER, SETUP).
2. **KEYS**: **SHRT** (the black keys are the shortcuts: mutes, reverse, clears, freeze) or **NOTE** (the black keys
   play notes: see [5.9](#59-play-the-black-keys-as-notes)).
3. **KNOB**: how far a knob moves a value per click. **1X**, **2X**, **4X**, or **ACC** (turn slowly for fine
   steps, quickly to cover ground). Small choices, like a filter's type, always move one at a time.
4. **HOLD**: how long a pad must be held to count as held. **LEDS**: how the idle pads glow.

<img src="img/setup.png" width="240" alt="SETUP: KEYS KNOB HOLD LEDS">

### 3.7 The CPU meter

On the mixer, bottom right: **CPU**, how hard Bryo is working, in 5 % steps. It turns bright at 85 %. Past that,
Bryo thins out grains and strings to keep the sound clean.

---

## 4. Recording

REC records onto **the track you're on**. On a TAPE track it records **the other tracks** playing (you can change
that: [4.5](#45-choose-what-rec-records)).

### 4.1 Clear a track's tape

1. Pick track 2 (hold GLO + white key 2).
2. **Hold black key POLY for half a second.** Track 2 is now an empty tape.

(Changed your mind? **Hold SAVE**: it comes back. See [4.4](#44-undo-a-clear).)

### 4.2 Record the others onto it

1. Press **PLAY**.
2. Press **REC**. The header shows ● and the tape grows while it records.
3. Let it run for 10 to 15 seconds, then press **REC** again.

<img src="img/tape_rec.png" width="240" alt="Recording">

**You'll hear:** track 2 now loops what the other three played: a new loop, as long as you recorded.

### 4.3 Record over a loop (overdub)

**DUB** on the TAPE page (KNOB 4) sets how much of the old recording stays when you record over it: 0 replaces it,
higher keeps more of it under the new sound.

1. On a track with a recording, set **DUB** to about +60 %.
2. Press **REC** while it plays, and **REC** again to stop.

### 4.4 Undo a clear

**Hold SAVE.** The last clear comes back: a tape, a track's modulation, or a drum pattern, whichever you cleared
last. (It only works while nothing has been recorded over it.)

### 4.5 Choose what REC records

1. Turn **ALGORITHM**. The **REC IN** page opens.
2. **KNOB 1 to 4** set what tracks 1 to 4 record:
   - **AUTO**: a TAPE track records the others; a SYNTH or POLY track records itself.
   - **OTHR**: the other tracks' mix.
   - **T1 to T4**: one track. Its own number shows as **SELF**: it records itself (resampling).
3. The lines show where each track's recording comes from. Press **HOME** to leave.

<img src="img/route.png" width="240" alt="REC IN, every track AUTO"> <img src="img/route_chain.png" width="240" alt="REC IN with a chain">

---

## 5. Other sources

Any track can be a **TAPE**, a **SYNTH**, a **POLY** (a sampler) or a **DRUM** machine. Each keeps its own knobs, so
you can switch back and forth.

### 5.1 Change a track's source

1. **Hold HOME.**
2. While holding it, **turn SELECT**: TAPE, SYNTH, POLY, DRUM.
3. Let go. You're on the new source's first page.

### 5.2 SYNTH: a small synth

Up to three notes at once. **The white keys play notes**, a semitone apart, starting from C; **OCT− / OCT+** move
them down or up an octave.

Press **HOME** again to go through its five pages:

| Page | Knobs |
| --- | --- |
| 1, the oscillator | **WAVE** (SIN TRI SQR SAW PLS), **DTUN** (a second oscillator, detuned), **MIX** of the two, **NOIS** |
| 2, AMP | **ATK DEC SUS REL**: how a note starts, falls, holds and fades |
| 3, FILTER | **CUT** (cutoff), **RES** (resonance), **ENV** (how much the envelope opens it), **KTRK** (follows the note) |
| 4, VOICE | **VOIC** (1-3 notes), **GLID** (slides between notes), **DRV** (drive), **TUNE** |
| 5, LEVEL | **LVL** |

<img src="img/synth.png" width="240" alt="SYNTH: WAVE DTUN MIX NOIS"> <img src="img/synth_filter.png" width="240" alt="SYNTH's FILTER">

**Try:** play a note, then turn **CUT** on the FILTER page while it sounds.

### 5.3 POLY: play a sound across the keys

POLY plays a reel (or the track's own tape) at the pitch of each key, up to four notes at once.

1. Switch the track to **POLY** (hold HOME, turn SELECT).
2. **KNOB 1 (REEL)** picks the sound. **STRT** moves where in it each note starts, **TUNE** shifts the pitch,
   **VOIC** is how many notes at once.
3. Play the white keys.

<img src="img/poly.png" width="240" alt="POLY: REEL STRT TUNE VOIC">

Its other pages: **ENV** (ATK DEC SUS REL), **FILTER** (CUT RES TYPE ENV), **LEVEL**.

### 5.4 DRUM: a drum machine

1. Switch the track to **DRUM** (hold HOME, turn SELECT), and press **PLAY**.
2. The **PATTERN** page: **PATN** picks one of ten rhythms (ROCK, DISC, FUNK, SHFL, BOSA, RMBA, CHA, BGIN, TNGO,
   MRCH). **LEN** is 2 to 4 bars, **SWNG** the swing, **ACNT** how strong the accents are.
3. The dots below show the pattern, a row per instrument.

<img src="img/drum.png" width="240" alt="DRUM: PATTERN">

**The white keys play the kit**, left to right: bass drum, snare, clap, rim shot, low conga, low bongo, high bongo,
claves, cowbell, maracas, tambourine, guiro, hi-hat, open hat, metal beat, cymbal.

### 5.5 DRUM: let it play like a drummer

1. Press **HOME** for the **VARY** page.
2. Turn **SEED** away from 0. Every number is a different version of the pattern: ghost notes, the hats thinning
   and filling, fills at the end. The same SEED always plays the same version.
3. **VARY** is how far it strays, **FILL** how often it fills, **EVOL** how often it moves on to a new version.

<img src="img/drum_vary.png" width="240" alt="DRUM: VARY">

### 5.6 DRUM: write your own steps

1. Press **HOME** until the **STEP** page (the fourth).
2. Turn **KNOB 1 (INST)** to choose the instrument.
3. Make sure **MODE** (KNOB 3) says **HITS**.
4. Now **white key n switches step n** of the bar on or off. **OCT− / OCT+** pick the bar, and the page stays on
   that bar while the pattern plays, so you can write into it as it goes round.
5. The white keys show the bar: a step that's on is **lit**, the others glow dim. While it plays, the playhead
   flips the key it's on (a lit step blinks dim, an empty one lights), so you can see it go round. In **ACC** mode
   the lit keys are the accents.

<img src="img/drum_step.png" width="240" alt="DRUM: STEP">

The other MODEs: **ACC** sets accents; **LIVE** lets you play along and records what you play at the nearest step;
**ERAS** wipes the instrument's hits as the play head passes while you hold a key.

To clear the whole pattern: **hold black key POLY** half a second. **Hold SAVE** to undo.

### 5.7 DRUM: shape one instrument

1. On PATTERN, VARY or KIT, **hold a white key**.
2. While holding it, KNOB 1 to 4 are **that instrument's** TUNE, DEC (length), LVL and TONE. The header shows which
   one.
3. **To modulate one of them**, keep the key held, then also hold **LFO** (or SEQ) and turn the knob: that's the
   modulator's depth to this drum's TUNE, DEC, LVL or TONE. Hold the key first; pressed after the pad, a white key
   jumps to a page instead. Each hit takes the value the modulator has at that moment, so an LFO on the hi-hat's LVL
   makes the hats swell and fade, a SEQ on the snare's TUNE plays a little melody.

<img src="img/drum_inst.png" width="240" alt="One instrument's own knobs">

The **KIT** page (the third) does the same for the whole kit at once: TUNE, DEC, TONE, DRV.

<img src="img/drum_kit.png" width="240" alt="DRUM: KIT">

### 5.8 ARP: the arpeggiator

The arpeggiator plays the keys you hold **one note at a time, in time**. It works on every track: on **SYNTH and
POLY** it plays notes, on **TAPE** it steps through the slices you hold, and on **DRUM** it walks the drums you hold up
and down (hold BD, SD and CH with UPDN and you get BD SD CH SD BD...).

1. Press **ARP**.
2. Turn **KNOB 1 (MODE)** to **KEY**.
3. Hold a chord on the white keys.

<img src="img/arp.png" width="240" alt="ARP: two notes, the one playing filled">

**You'll hear:** the notes of the chord one after another, a sixteenth each. The bars show the pattern; the solid one
is playing.

The knobs:

| Knob | Does |
| --- | --- |
| **MODE** | **OFF**; **KEY** plays while you hold keys; **LTCH** keeps playing the last chord after you let go (press a new chord to replace it); **ADD** keeps every key you press, so you can pile up an endless pattern |
| **ORDR** | **UP**, **DOWN**, **UPDN** (up and back down), **RND** (random), **PLAY** (the order you pressed them) |
| **RNGE** | 1, 2 or 3 octaves: the notes, then the same an octave up, and so on (on TAPE and DRUM there's no octave of a slice or a drum, so it stays at 1) |
| **RATE** | how long each step is: 1/4, 1/8, 8T, 1/16, 16T, 1/32 (T is a triplet) |
| **GATE** (page 2) | how much of each step the note sounds: short and plucky, or long |
| **SWNG** (page 2) | pushes every other step late |

**To build an endless pattern:** set MODE to **ADD**, then press keys one at a time. Each one joins the pattern and
stays. **Hold black key POLY** (on the ARP page) to clear it and start again.

<img src="img/arp_add.png" width="240" alt="ARP in ADD mode, four notes piled up"> <img src="img/arp_drum.png" width="240" alt="ARP on a DRUM track: three drums, up and down">

**RES ARP (the ARP pad's third page): an arpeggio for the strings.** Press ARP three times (or hold ARP and press
white key 3). It's RESONATOR's own arpeggio, separate from the track's: each step plucks RESONATOR's four strings at
that note, whatever the track plays. **MODE** turns it on (OFF, KEY, LTCH, ADD); **ORDR**, **RNGE** and **RATE** work
as above. With MODE on, the white keys on this page play the strings' pattern and the track itself stays quiet; with
MODE off they play the track as on the other ARP pages. A latched or piled-up pattern keeps plucking when you leave
the page, and **hold POLY** here to clear it. You'll want RESONATOR's **WET** up (EDIT seven times, KNOB 4) to hear it.

<img src="img/arp_res.png" width="240" alt="RES ARP: two notes held, the strings plucked in turn">

Keys you hold play straight away, transport or not. A latched or piled-up pattern keeps playing when you move to
another track, and follows the transport: **STOP** quiets it (the notes are kept), **PLAY** starts it again from the
top. Each track has its own, and a project you save keeps the notes it holds.

### 5.9 Play the black keys as notes

The FM-1's keys are laid out like a piano, F to G. Normally the black keys are shortcuts, and the white keys play
semitones in a row. You can make it a real keyboard instead:

1. **Hold GLO and press OCT+.** The screen says **BLACK KEYS PLAY NOTES**.
2. On a SYNTH or POLY track, the white keys now play the natural notes from F, and the black keys the sharps, like a
   piano. **OCT− / OCT+** still move it.
3. **Hold GLO and press OCT+** again to get the shortcuts back.

While the black keys are notes, **hold GLO** to use their shortcuts (mutes, reverse, clears, freeze): with GLO held
they're shortcuts again, and the OP keys light up for the muted tracks. Holding LFO and tapping
OP1–OP3 still works ([7.4](#74-choose-which-modulator-the-lfo-pad-controls)). The setting is also on the SETUP page
([3.6](#36-setup-how-the-panel-behaves)) and is kept when you turn the FM-1 off.

---

## 6. The effects

Every track goes through GRAIN, RESONATOR, COLOR and SPACE, in that order. They start switched off (WET at 0, DLY
and VERB at 0), so turn them up to hear them.

### 6.1 GRAIN: turn it on

GRAIN keeps the last bars of the track's sound and plays tiny pieces of it (grains), scattered and layered.

1. Press **PLAY**.
2. Press **EDIT**: **GRAIN 1**.
3. Turn **KNOB 4 (WET)** up to 80 %.

<img src="img/grain_busy.png" width="240" alt="GRAIN 1: SIZE RATE ROUT WET, WET 80%"> <img src="img/grain_playing.png" width="240" alt="Grains playing">

**You'll hear:** a cloud of the track's sound. Each block in the picture is a grain.

**How many grains:** under the picture, "5 OF 40 GRAINS" is how many are sounding and how many this track may have
right now. There's a pool of 40 for one track, 30 between two and 18 between three or four, and only tracks whose
grains you'd hear share it. A muted track doesn't, and neither does one with nothing in its buffer yet (a SYNTH or
POLY track nobody has played), or one reading the tape while the transport is stopped. A track alone has all 40; how
many actually sound is up to **RATE** and **SIZE** (more grains a second, or longer ones, overlap more), so turn those
up for a thicker cloud. To give a track a set number, whatever the others do, use **GRNS** on GRAIN 6 (6.4.2). When a
track has no grains, the page says why: **MUTED**, **STOPPED**, **SILENT**, or **TAKEN** (other tracks hold the
whole pool).

### 6.2 GRAIN: shape the grains

On **GRAIN 1** (press EDIT once):

- **SIZE**: how long each grain is.
- **RATE**: how many there are.
- **ROUT** and **WET**: how the grains mix in (6.8).

On **GRAIN 2**: **PTCH** their pitch in semitones, **SPRD** how wide in stereo, **SPRY** scatters where they
start, **CONT** softens their edges.

On **GRAIN 3**: **REV** plays some backwards, **PATN** the rhythm they fire in, **SCAL** holds their pitch to a scale
and **PRND** gives each a random pitch.

<img src="img/grain.png" width="240" alt="GRAIN 1: SIZE RATE ROUT WET"> <img src="img/grain2.png" width="240" alt="GRAIN 2: PTCH SPRD SPRY CONT">

### 6.3 GRAIN: freeze

- **Hold black key 0**: the grains loop the bars they have and stop listening. Let go to carry on.
- **Tap black key 0**: it stays frozen (its light is on) until you tap again.

<img src="img/grain_frozen.png" width="240" alt="FROZEN">

**Try:** freeze, switch the track's source or mute it, then let go.

### 6.4 GRAIN: where the grains read

On **GRAIN 4**, **SCAN** chooses:

| SCAN | The grains read |
| --- | --- |
| **STR** (the default) | what just played, moving at **WARP**'s speed: below 100 % it falls behind and stretches time |
| **POS** | one spot, set by **OFST** |
| **DLY** | what played a moment ago (OFST sets how long ago): a granular delay |
| **TAPE** | straight from the track's tape |

**FDBK** feeds the grains back into themselves. On any GRAIN page, the **white keys move the reading spot** to their
sixteenth of the loop.

<img src="img/grain4.png" width="240" alt="GRAIN 4: SCAN WARP OFST FDBK">

### 6.4.1 GRAIN: BODY, a floor under the grains

At WET 100 you hear only grains: the original sound is gone, and with it what held the cloud down. **GRAIN 5** puts a
floor back under them:

- **BODY**: how loud the floor is (0 is off). It goes with the grains, so WET turns it down too.
- **MODE**: what the floor is.
  - **SRC**: the track's own sound with its top taken off. Always in key and in time with what you're playing.
  - **TRI**: a triangle wave that plays the grains' notes. It's heard on the FM-1's own speaker too.
  - **SIN**: a sine that plays the grains' notes: the purest, best on headphones (the speaker barely plays it when
    it's low).
- The wave (TRI and SIN) sounds while the grains do: stop the transport, and it fades with them.
- **ROOT**: where the wave sits (TRI and SIN), C1 to C5. Set it to the key you're in. The wave follows each grain's
  note, so when **PTCH** (GRAIN 2), **PRND** and **SCAL** (GRAIN 3) send the grains around a scale, the wave walks the same
  scale over ROOT, gliding from note to note. It always stays in the octave above ROOT, so it's a floor however high
  or low the grains go. With PTCH and PRND at 0 it holds ROOT.
- **ROOT RES** (turn ROOT all the way down, below C1): the wave takes RESONATOR's root instead (its PTCH, the white
  key you played on its page, or RES ARP's note), so BODY always rings the strings. RESONATOR's strings only answer
  the notes they're tuned to: a BODY a third away from them passes through without ringing them.
- **TONE**: how much top end the floor keeps, from a deep rumble to a full low-mid.

<img src="img/grain_body.png" width="240" alt="GRAIN 5: BODY 40%, MODE TRI, ROOT A2, TONE 392 Hz">

### 6.4.2 GRAIN: how many grains a track holds

**GRAIN 6** has **GRNS** (KNOB 1).

- **GRNS AUTO** (the default): the track shares the pool with the others, by what each one's RATE and SIZE ask for.
  Whatever nobody needs is shared out too, so a track alone has all 40.
- **GRNS 1 to 40**: the track holds that many. It takes them from the AUTO tracks first, and keeps them however the
  other tracks come and go, so its cloud doesn't thin when another track's GRAIN comes on. It never gets more than
  that number either. The page then reads "5 OF 12 HELD".
- The pool still shrinks as more tracks use grains (40, 30, 18): that's what the FM-1's processor can run. If the
  tracks holding numbers ask for more than the pool, they share it: the ones asking less keep theirs, the rest
  split what's left. An AUTO track left with nothing says **TAKEN: NO GRAINS**.
- GRNS sets how many may sound; **RATE** and **SIZE** still decide how many start. For a full 12, turn those up
  until the page reads "12 OF 12 HELD".

<img src="img/grain6.png" width="240" alt="GRAIN 6: GRNS 12; 4 OF 12 HELD">

### 6.5 RESONATOR: four strings

The track rings four tuned strings, like a sitar's sympathetic strings.

1. Press **EDIT seven times** for **RESONATOR**.
2. Turn **KNOB 4 (WET)** up (KNOB 3 is ROUT, see 6.8).
3. **The white keys set the strings' root note** (on a TAPE or DRUM track they pluck the strings alone, without
   playing a slice or a drum). **PTCH** does the same with a knob. **DEC** is how long they ring.
4. **RESONATOR 2**: a filter (CUT, RES, TYPE) and **SCAL**, the chord the four strings make (HARM, MAJ, MIN, PEN).
5. **RESONATOR 3**: **TONE**, how bright they ring.
6. To make the strings play a pattern of their own, use **RES ARP**, the ARP pad's third page ([5.8](#58-arp-the-arpeggiator)).

<img src="img/resonator_wet.png" width="240" alt="RESONATOR ringing"> <img src="img/resonator2.png" width="240" alt="RESONATOR 2: CUT RES TYPE SCAL">

### 6.6 COLOR: dirt and character

1. Press **FX**: **COLOR**.
2. **DRV** drives it, **CRSH** crushes it (lo-fi), and **ROUT** and **WET** mix it in.
3. **COLOR 2**: **NOIS** adds noise that rides on the sound, **TILT** makes it darker or brighter, then the noise's
   length and tone (NDEC, NTON).
4. **COLOR 3**: what CRSH crushes (CMOD: BIT, RATE, BOTH) and **LVL**, its level.

<img src="img/color.png" width="240" alt="COLOR: DRV CRSH ROUT WET"> <img src="img/color2.png" width="240" alt="COLOR 2: NOIS TILT NDEC NTON">

### 6.7 SPACE: echoes and a room

1. Press **FX four times**: **SPACE**. **TIME** is the echo's time, **FDBK** how many echoes; turn up **DLY** (the
   echoes) and **VERB** (the room), its two sends.
2. Press **FX** again for **SPACE 2**: **SIZE** and **DEC** shape the room, **TONE** darkens or brightens both.
3. **SPACE 3**: **PRE**, a short gap before the room starts.

<img src="img/space_long.png" width="240" alt="SPACE: TIME FDBK DLY VERB"> <img src="img/space2.png" width="240" alt="SPACE 2: SIZE DEC TONE SPRD">

### 6.8 Insert or send

GRAIN, RESONATOR and COLOR have **ROUT** on KNOB 3 of their first page, next to WET:

- **INS** (insert): **WET** fades from the dry sound to the effect.
- **SEND**: the dry sound stays at full and WET adds the effect on top.

SPACE always works as a send.

<img src="img/color.png" width="240" alt="COLOR 1: DRV CRSH ROUT WET">

---

## 7. Modulation

Each track has **four modulators**. A modulator makes knobs move by themselves.

- **M1, M2, M3** live on the **LFO** pad (hold LFO and tap OP1, OP2 or OP3 to choose). Each can be an **LFO** (a
  wave), an **ADSR** (an envelope), a **SEQ** (steps) or a **FOLLOW** (follows how loud a track is). Out of the box:
  M1 is a slow LFO, M2 an envelope, M3 a random LFO.
- (**ENV** isn't a modulator: it opens the track's own envelope page.)
- **M4** is the step sequencer, on the **SEQ** pad.

### 7.1 Make a knob move

Let's make COLOR's crush breathe.

1. Press **PLAY**, then **FX** (COLOR).
2. **Hold LFO.** The knobs now show depths (how much M1 moves each one).
3. While holding it, **turn KNOB 2 (CRSH)** to about **+40 %**.
4. Let go.

<img src="img/mod_depth.png" width="240" alt="Holding LFO: the depths">

**You'll hear:** the crush coming and going, slowly. That's it: **hold a slot, turn a knob**. It works on any page
of the track: the source, the effects, the channel strip.

### 7.2 See what's moving

After you let go, a modulated knob has a **mark under it**: a rail showing how far it reaches, and where it is now.

<img src="img/mod_marks.png" width="240" alt="Modulated knobs, with their rails">

### 7.3 Change how the modulator moves

1. **Tap LFO** (press and let go). M1's own page opens.
2. **RATE** is how fast, in time with the tempo (1/2: once every half bar), **SHPE** the shape (SIN TRI SQR SAW RND), **SKEW** and **FOLD** bend
   it.
3. **Tap LFO again** for its other pages. The last one has **AMT** (how much it moves everything) and **OFS** (an
   offset).

<img src="img/mod1.png" width="240" alt="M1: LFO"> <img src="img/mod1_p4.png" width="240" alt="LFO's last page: PHAS SPRD AMT OFS">

### 7.4 Choose which modulator the LFO pad controls

1. **Hold LFO.**
2. While holding it, **tap black key OP1, OP2 or OP3**: M1, M2 or M3. The screen says which.
3. Turn a knob now to set that modulator's depth, or let go of LFO to see its page.

From then on, LFO is that modulator, until you pick another.

### 7.5 Change a modulator's engine

M1, M2 and M3 can each be **LFO**, **ADSR** (an envelope the keys, the arpeggiator or the loop set off), **SEQ** or
**FOLLOW**.

1. **Hold LFO** (with the modulator you want chosen: hold LFO and tap OP1-OP3).
2. **Turn SELECT** to pick the engine.

Its depths stay where they were. M4 always stays the step sequencer.

<img src="img/mod2.png" width="240" alt="ADSR"> <img src="img/mod4_follow.png" width="240" alt="FOLLOW">

### 7.6 SEQ: set the steps

1. **Tap SEQ** for M4's page.
2. **Hold a white key** (key 1 is step 1) and **turn KNOB 1**: that step's value.
3. Hold several keys to set them together.
4. **LEN** is how many steps; **RATE** how long each one is.
5. The white keys show it: the steps inside LEN glow, and while it plays the step playing is **lit** (held keys are
   lit too). It runs while its page is on screen even before it modulates anything, so you can watch it as you
   write the steps.

<img src="img/mod3_steps.png" width="240" alt="SEQ with steps being set">

### 7.7 Clear modulation

- **Hold black key MONO** half a second: every depth on the track is cleared (the slots' own settings stay).
- **Hold a modulator's pad and MONO**: only that modulator's depths on the page you're on.
- **Hold SAVE** to undo.
- Or turn one depth back to 0: it stops there, so it's easy to find.

---

## 8. Getting back

### 8.1 Reset one knob

**Hold SCL and turn a knob.** It goes back to its default.

<img src="img/scl_held.png" width="240" alt="SCL held">

### 8.2 Reset a whole page

**Hold SCL and press the page's own pad** (the lit one: HOME on a source page, EDIT on GRAIN, FX on COLOR...).
Every knob on that page goes back to its default.

### 8.3 Undo

**Hold SAVE.** It puts back the last thing you reset, cleared or loaded.

---

## 9. Saving

### 9.1 Save everything

**Tap SAVE.** Everything is saved into the current project (**P1** to start with): every knob, source, pattern,
depth, the mixer and the tempo. A tape you recorded goes into a user reel named after the project and the track
(**P1T2** is project 1, track 2); if all six user reels are taken, SAVE tells you which tapes didn't fit. The last
project you saved comes back when you turn the FM-1 on.

**Saving while it plays:** the sound carries on. When there's a new recording to keep, the sound fades out for a
moment while it's written, then fades back in. (Bryo gets the next save ready in a quiet moment, when the transport
is stopped and nothing sounds, so a save mid-performance doesn't have to stop the sound.)

### 9.2 Projects: save, load, delete

There are six project slots, P1 to P6.

1. Turn **PRESETS**. The **PROJECTS** view opens.
2. Turn **PRESETS** to point at a slot.
3. Then:
   - **OCT+** loads it. (If you have changes you haven't saved, press OCT+ again within 3 seconds to confirm.)
   - **SAVE** saves into it. (Over another project, press SAVE again within 3 seconds.)
   - **Hold POLY** half a second to delete it.
   - **OCT−** to leave.

<img src="img/projects.png" width="240" alt="PROJECTS">

### 9.3 Start a new, blank project

1. Turn **PRESETS** and point at an **EMPTY** slot.
2. Press **OCT+**. (If you have changes you haven't saved, press it again within 3 seconds.)
3. Everything starts from nothing: every track an empty tape, no reels, no drum pattern, no modulation, 120 BPM.
4. **Tap SAVE** when you want to keep it: it saves into that slot.

<img src="img/project_new.png" width="240" alt="A new, blank project in P3">

### 9.4 Back to the factory scene

The scene Bryo starts with is always there, even after you've saved over it:

1. Turn **PRESETS**, and keep turning past **P6**: **F, FACTORY**.
2. Press **OCT+**. (Changes not saved: press it again within 3 seconds.)
3. The factory scene plays. Your projects in P1 to P6 are left exactly as they were.
4. To keep your changes to it, turn **PRESETS** to a slot and press **SAVE**.

<img src="img/projects_factory.png" width="240" alt="FACTORY in the project view">

### 9.5 Track presets: keep one track's sound

A track preset is **a track's sound**: its source and knobs, the effects, the channel strip and the modulation. Not
its pattern, level, REC IN or arpeggiator.

1. Turn **PRESETS**, then press **EDIT**: the **PRESETS** page, eight slots **A to H**.
2. Turn **PRESETS** to a slot.
3. **SAVE** saves the track you're on into it. **OCT+** loads it into the track you're on.
4. Changed your mind after loading? **Hold SAVE** to put the track back.

<img src="img/presets.png" width="240" alt="Track PRESETS A to H">

### 9.6 Factory presets

Past H come eight factory presets, two for each source: **DRIFT** and **RINGS** (TAPE), **BASS** and **PAD**
(SYNTH), **KEYS** and **SWELL** (POLY), **TIGHT** and **DUB** (DRUM). Load one into a track of that source with
**OCT+**.

<img src="img/presets_factory.png" width="240" alt="Factory presets">

---

## 10. With a computer

### 10.1 Copy sounds on and off

Plugged into a computer, the FM-1 shows up as a small drive called **BRYO**.

- **Copy a track off:** `TAPE1.WAV` to `TAPE4.WAV` are what each track plays.
- **Replace a track's tape:** copy a WAV on, named `TAPE2.WAV` for track 2 (and so on).
- **Add a reel:** a WAV with any other name becomes one of six user reels (up to 21.5 s). Pick it with REEL
  ([2.5](#25-play-a-different-reel)).
- **Eject the drive and plug it back in** to see new files.

### 10.2 Record what the computer plays

1. With the FM-1 stopped, **hold REC for a second**. The **USB RECORD** screen opens.
2. On the computer, pick **Bryo** as the sound output and play something.
3. Press **REC** to start the take and **REC** to stop it.
4. Trim it while it loops: **STRT** and **LEN** (the part you keep), **GAIN**, **FADE** (the ends).
5. Press **white key 1 to 4** for the track it goes on, then **REC** to keep it.
6. **HOME** goes back a step (throws the take away, or leaves).

<img src="img/usbrec.png" width="240" alt="USB RECORD: READY"> <img src="img/usbrec_rec.png" width="240" alt="Recording a take"> <img src="img/usbrec_trim.png" width="240" alt="Trimming the take">

---

## 11. When something goes wrong

### 11.1 A red BRYO CRASH screen

<img src="img/crash.png" width="240" alt="BRYO CRASH">

It restarts by itself after 4 seconds. What was only in memory (anything unsaved) is gone; saved projects, reels
and settings stay. If it freezes instead, it restarts after 8 seconds. If it crashes twice within 30 seconds of
turning on, it starts in update mode so it can be reinstalled.

Please write down what you were doing: it helps find the bug.

### 11.2 UPDATE MODE IN 3...

<img src="img/uboot.png" width="240" alt="UPDATE MODE countdown">

You're holding **OCT− and OCT+ together**. Holding them five seconds puts the FM-1 in update mode, to install
firmware. **Let go** to cancel.

### 11.3 A calibration screen

<img src="img/calibration.png" width="240" alt="HARDWARE CALIBRATION">

OCT− and OCT+ were held while it turned on. Follow the screen (press each button, turn each knob), or turn it off
and on again.

### 11.4 Going back to the original firmware

M-VAVE's updater puts the stock firmware back. Bryo's projects and tapes can't be opened there.

---

## 12. Quick reference

| Do this | To |
| --- | --- |
| **PLAY** | start / stop |
| **SELECT** | tempo |
| **MASTER** | volume |
| **GLO** held + white key 1-4 | pick a track |
| **GLO** held + KNOB 1-4 | track levels |
| **GLO** held + SELECT | how many tracks are on |
| **GLO** tapped, then **EDIT** | CHANNEL, then MASTER |
| **OP1-OP4** | mute tracks |
| **OP5 / OP6** | the tape backwards / half speed |
| **HOME** | the source's pages |
| **HOME** held + SELECT | change the source |
| **EDIT** | GRAIN, then RESONATOR |
| **FX** | COLOR, then SPACE |
| **ENV** | the source's envelope page |
| **LFO SEQ** tapped | that modulator's pages (LFO: M1-M3, SEQ: M4) |
| **LFO SEQ** held + a knob | how much that modulator moves the knob |
| **LFO** held + OP1 / OP2 / OP3 | which modulator LFO is: M1, M2, M3 |
| **LFO** held + SELECT | the modulator's engine (LFO, ADSR, SEQ, FOLLOW) |
| **ARP** | the arpeggiator (notes on SYNTH and POLY, slices on TAPE, drums on DRUM); its page 3, RES ARP, plays RESONATOR's strings |
| **GLO** held + OCT+ | black keys: shortcuts or notes |
| **GLO** tapped, **EDIT** ×3 | SETUP: keys, knob speed, hold time, LEDs |
| **REC** | record onto the track |
| **REC** held 1 s, stopped | record the computer |
| **ALGORITHM** | what REC records |
| black **POLY** held | clear the tape (DRUM: the pattern) |
| black **MONO** held | clear the track's modulation |
| black **0** | freeze GRAIN (held, or tapped to latch) |
| **SCL** held + a knob | that knob to its default |
| **SCL** held + the lit pad | the page to its defaults |
| **SAVE** | save the project |
| **SAVE** held | undo the last clear, reset or load |
| **PRESETS** | projects; then EDIT for track presets |
| **PRESETS**, an empty slot, **OCT+** | a new, blank project |
| **PRESETS** past P6 (FACTORY), **OCT+** | the factory scene back |
| **OCT− / OCT+** | octave (DRUM's pages: the bar) |
