/* SPDX-License-Identifier: GPL-3.0-only */
/* Bryo: the parameter table. Every track has five devices (its source, GRAIN, RESONATOR, COLOR, SPACE) with four
 * knobs each, and four modulator slots with four knobs each. KNOB 1..4 always edit the four values of whatever the
 * screen shows (the PRD's "4-value strip"), so a page is just (device or slot, track).
 *
 * The values live here as integers in each parameter's own range; the DSP reads them once per control block and
 * maps them (the device modules own that mapping). The labels are what the strip prints under each pictogram: at
 * most 4 capitals, so four sit apart across the screen in the 5 x 7 face (ui_px.c), the way groovebox screens
 * abbreviate. */

enum { DEV_SRC, DEV_GRAIN, DEV_RESO, DEV_COLOR, DEV_SPACE, NDEV };
#define NSLOT 4u                 /* modulator slots per track */
enum { F_PCT, F_BIPCT, F_ST, F_NOTE, F_MS, F_NUM, F_DB, F_ENUM, F_CT, F_HZ, F_TIME, F_PCT100 };   /* how a value prints
                                                                                         * (F_PCT100: hundredths of a
                                                                                         * percent, shown whole) */

typedef struct {
    const char *label;
    int16_t min, max, def;
    uint8_t fmt;
    const char *const *names;    /* F_ENUM: a name per value, min..max (at most 4 letters) */
    uint8_t nomod;               /* 1: not a modulation target (a choice the main loop acts on, or a view's state) */
} pdesc_t;

/* the names of the switch-like values */
static const char *const N_OFFON[2] = {"OFF", "ON"};
static const char *const N_TRIG[2] = {"FREE", "KEY"};
static const char *const N_CLK[2] = {"FREE", "BPM"};
static const char *const N_MODE[2] = {"KEYS", "FLLW"};
static const char *const N_DIR[4] = {"FWD", "REV", "PING", "RND"};
static const char *const N_BODY[3] = {"SRC", "TRI", "SIN"};   /* GRAIN's BODY: the source, or a wave */
static const char *const N_ROOT[1] = {"RES"};          /* BODY's ROOT at its bottom: RESONATOR's root (F_NOTE's
                                                        * names: the name of its lowest value, if it has one) */
static const char *const N_SHAPE[5] = {"SIN", "TRI", "SQR", "SAW", "RND"};
static const char *const N_SRC[6] = {"SELF", "T1", "T2", "T3", "T4", "USB"};
static const char *const N_OSC[5] = {"SIN", "TRI", "SQR", "SAW", "PLS"};
static const char *const N_HOLD[5] = {"OFF", "1/32", "1/16", "1/8", "1/4"};
static const char *const N_SRATE[6] = {"1", "1/2", "1/4", "1/8", "1/16", "1/32"};   /* SEQ: a step's length, in bars */
#include "bryo_reels.h"              /* the factory reels' names (tools/gen_reels.py) */
#define USLOT_N 6u                   /* user reels: your sounds in flash (reel.c) */
static char uslot_name[USLOT_N][6];  /* their names (reel.c uslot_names) */
static const char *const N_REEL[1u + NREEL + USLOT_N] = {"TAPE", REEL_NAMES_INIT, uslot_name[0], uslot_name[1],
                                                         uslot_name[2], uslot_name[3], uslot_name[4], uslot_name[5]};

#define NPK 24u                  /* knobs a device or engine can have: up to six pages of four (20 until GRAIN's
                                  * GRNS: projects and presets store each array with its count, depths by array
                                  * and knob, so older ones read as they are) */

static const char *const DEV_NAME[NDEV] = {"TAPE", "GRAIN", "RESONATOR", "COLOR", "SPACE"};

/* Page 1 of each device is the PRD's four knobs. The pages after it hold what the sound needs that the PRD left
 * without a knob: wet levels, the switches the black keys flip, the shapes. The names follow the Torso S-4's
 * devices where Bryo's does the same job (docs/s4-alignment.md): TAPE's SOS is DUB, MOSAIC's SPRAY is SPRY, and so
 * on; the PRD's knob positions stay. A device's pages run until one whose first label is empty. */
static const char *const N_PATN[4] = {"EVEN", "SWNG", "CLST", "RND"};
static const char *const N_SCAN[4] = {"TAPE", "STR", "POS", "DLY"};
static const char *const N_GSCAL[5] = {"OFF", "CHR", "MAJ", "MIN", "PEN"};
static const char *const N_ROOMS[2] = {"A", "B"};       /* SPACE's ROOM: the shared room a track sends to */
static const char *const N_QUAL[2] = {"HI", "LO"};      /* a room's QUAL: 22.05 or 11.025 kHz */
#define GR_GRNS_MAX 40                /* GRAIN's GRNS: the most it can hold (the pool for one track, grain.c GR_POOL) */
static const char *const N_GRNS[1 + GR_GRNS_MAX] = {"AUTO", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "13", "14", "15", "16", "17", "18", "19", "20", "21", "22", "23", "24", "25", "26", "27", "28", "29", "30", "31", "32", "33", "34", "35", "36", "37", "38", "39", "40"};
static const char *const N_RSCAL[4] = {"HARM", "MAJ", "MIN", "PEN"};
static const char *const N_SLOP[3] = {"LP", "BP", "HP"};
static const char *const N_CMOD[3] = {"BIT", "RATE", "BOTH"};
static const char *const N_ROUT[2] = {"INS", "SEND"};
/* ROUT (GRAIN, RESONATOR, COLOR): how WET mixes the device in. INS: in the track's line, WET crossfading the dry
 * sound with the device's (dry x (1 - WET) + device x WET); SEND: in parallel, the dry at full and the device's
 * sound added at WET (dry + device x WET), the way an aux send works, per track. SPACE is always a send (DLY and
 * VERB are levels added to the dry). Not a modulation target: a switch flipping at an LFO's rate isn't a sound */
#define P_ROUT {"ROUT", 0, 1, 0, F_ENUM, N_ROUT, 1}
/* the arpeggiator's knobs and names (ARP_P below; RESONATOR's own arpeggio uses them too) */
enum { AR_MODE, AR_ORDR, AR_RNGE, AR_RATE, AR_GATE, AR_SWNG, NARP = 8 };
enum { ARM_OFF, ARM_KEY, ARM_LTCH, ARM_ADD };
enum { ARO_UP, ARO_DOWN, ARO_UPDN, ARO_RND, ARO_PLAY };   /* (PK_DIR's order, then PLAY) */
static const char *const N_ARM[4] = {"OFF", "KEY", "LTCH", "ADD"};
static const char *const N_ARO[5] = {"UP", "DOWN", "UPDN", "RND", "PLAY"};
static const char *const N_ARATE[6] = {"1/4", "1/8", "8T", "1/16", "16T", "1/32"};
static const char *const N_TSYNC[6] = {"OFF", "1BR", "2BR", "4BR", "8BR", "16BR"};   /* TAPE SYNC: the loop, in bars */

/* (each table in the order its knobs are stored; the "1:", "2:" notes are those blocks of four. Where a page shows
 * them otherwise, PLAYOUT below says so) */
static const pdesc_t DEV_P[NDEV][NPK] = {
    {   /* TAPE (the source when it's TAPE: SYNTH and POLY have their own tables) */
        {"STRT", 0, 10000, 0, F_PCT100}, {"LEN", 100, 10000, 10000, F_PCT100},   /* (hundredths: a click is 1 %,
                                                                                * HOME held 0.01 %: the zoom) */
        {"SPD", -200, 200, 100, F_BIPCT}, {"DUB", -100, 100, 0, F_BIPCT},
        /* 2: the crossfade at the loop's seam, reverse and half speed (OP5 and OP6 flip them too), the record gain */
        {"XFAD", 0, 100, 10, F_MS}, {"REV", 0, 1, 0, F_ENUM, N_OFFON},
        {"HALF", 0, 1, 0, F_ENUM, N_OFFON}, {"GAIN", -12, 12, 0, F_DB},
        /* 3: what the track plays: its own tape, a factory reel or one of your reels (in flash; REC copies it onto
         * the tape); ROTATE: where in the loop playing (and slice 1) starts against the transport; LVL: what the
         * track hears of its tape (the S-4's LEVEL, on every source's last page; REC's GAIN is what goes on) */
        {"REEL", 0, NREEL + USLOT_N, 0, F_ENUM, N_REEL}, {"ROTA", 0, 99, 0, F_PCT}, {"LVL", -24, 6, 0, F_DB},
        /* SYNC: the loop lasts that many bars at the tempo, its speed (and pitch, as on a tape machine) following
         * it, its place locked to the beat (tape.c tape_sync). A take of set bars sets it */
        {"SYNC", 0, 5, 0, F_ENUM, N_TSYNC, 1},
        /* the envelope of a slice the keys play (ENV opens it, page 2): stopped, the slice through ATK DEC SUS while
         * the key's held and REL after; playing, a key's jump fades in over ATK (the loop itself isn't gated). The
         * defaults change nothing: REL long enough that a tapped slice plays to its end */
        {"ATK", 0, 127, 0, F_TIME}, {"DEC", 0, 127, 60, F_TIME}, {"SUS", 0, 100, 100, F_PCT},
        {"REL", 0, 127, 127, F_TIME}},
    {   /* GRAIN (the S-4's MOSAIC) */
        {"SIZE", 5, 500, 80, F_MS}, {"RATE", 0, 100, 40, F_PCT},
        {"PTCH", -24, 24, 0, F_ST}, {"SPRD", 0, 100, 30, F_PCT},
        /* 2: dry/wet, SPRAY (the jitter of where grains read), the window's contour (square .. smooth), the chance
         * a grain plays backwards */
        {"WET", 0, 100, 0, F_PCT}, {"SPRY", 0, 100, 20, F_PCT},   /* (WET 0: off until it's turned up) */
        {"CONT", 0, 100, 50, F_PCT}, {"REV", 0, 100, 0, F_PCT},
        /* 3: the order grains fire in; their pitch held to a scale; a random pitch per grain, up to +-PRND
         * semitones */
        {"PATN", 0, 3, 3, F_ENUM, N_PATN}, {"SCAL", 0, 4, 0, F_ENUM, N_GSCAL},
        {"PRND", 0, 12, 0, F_ST}, P_ROUT,
        /* 4: what grains read and where (grain.c), left to right as you set it up: SCAN, the track's TAPE or the
         * live buffer of the last bars, read stretching behind the write head (STR), at a spot (POS) or a delay
         * behind it (DLY); WARP, how fast the read point moves (TAPE, STR; 0 holds it, - backwards); OFST, POS's
         * spot or DLY's delay as a share of the buffer; FDBK, how much of the buffer stays as new sound goes in
         * (the S-4's) */
        {"SCAN", 0, 3, 1, F_ENUM, N_SCAN}, {"WARP", -200, 200, 100, F_BIPCT},
        {"OFST", 0, 100, 25, F_PCT}, {"FDBK", 0, 100, 0, F_PCT},
        /* 5: BODY, a floor under the grains (it helps at WET 100, where the source is gone): the track's own sound
         * low-passed (MODE SRC: always in key and in time with it), or a wave playing the grains' notes (PTCH,
         * PRND, SCAL) in the octave over ROOT (RES, below C1: over RESONATOR's root, PTCH, a key or RES ARP's note, so
         * it always rings the strings; TRI: heard on the small speaker too, its harmonics; SIN: the purest, for
         * headphones), through TONE's low-pass, at BODY's level. It's part of the wet side: WET sets it with the
         * grains (grain.c) */
        {"BODY", 0, 100, 0, F_PCT}, {"MODE", 0, 2, 0, F_ENUM, N_BODY, 1},
        {"ROOT", 23, 72, 45, F_NOTE, N_ROOT}, {"TONE", 0, 100, 38, F_HZ},
        /* 6: GRNS, the grains this track holds of the pool: AUTO, shared by what each track's RATE and SIZE ask
         * (the leftover everyone's); a number, that many kept for it whatever the other tracks do, taken from
         * the AUTO tracks first (grain.c gr_plan_into). Not a modulation target: it's the pool's split */
        {"GRNS", 0, GR_GRNS_MAX, 0, F_ENUM, N_GRNS, 1},
        /* SYNC: RATE in the tempo's divisions (1 bar .. 1/64), the grains on that grid, PATN shaping it (grain.c) */
        {"SYNC", 0, 1, 0, F_ENUM, N_OFFON, 1}},
    {   /* RESONATOR (the S-4's RING) */
        {"PTCH", 33, 81, 45, F_NOTE}, {"DEC", 0, 100, 60, F_PCT},
        {"TONE", 0, 100, 60, F_PCT}, {"WET", 0, 100, 0, F_PCT},
        /* 2: a filter before the strings (cutoff, resonance, its TYPE: low-, band- or high-pass, as POLY's), and the
         * strings' tuning: the root's harmonics, or a scale's chord tones from the root */
        {"CUT", 0, 127, 127, F_HZ}, {"RES", 0, 100, 0, F_PCT},
        {"TYPE", 0, 2, 0, F_ENUM, N_SLOP}, {"SCAL", 0, 3, 0, F_ENUM, N_RSCAL},
        /* 3: how WET mixes the strings in (ROUT: INS or SEND); a pluck's shape (reso.c rs_pluck): PICK, how hard
         * (an offset on TONE for the burst: - a soft mallet, + a pick), STRM, the strings strummed STRM ms apart,
         * VARY, each pluck a little different. 0 on all three: the pluck as it was (a stored 0 from before them) */
        P_ROUT, {"PICK", -100, 100, 0, F_BIPCT}, {"STRM", 0, 100, 0, F_MS}, {"VARY", 0, 100, 0, F_PCT},
        /* its own arpeggio, RES ARP (stored here, 12-15, shown on the ARP pad's page 3, not on RESONATOR's): the
         * keys on that page into it, a pluck of the strings each step at that note's root, whatever the source
         * plays. As the ARP pad's knobs, with no GATE (a pluck rings by DEC) */
        {"MODE", 0, 3, ARM_OFF, F_ENUM, N_ARM, 1}, {"ORDR", 0, 4, ARO_UP, F_ENUM, N_ARO, 1},
        {"RNGE", 1, 3, 1, F_NUM, 0, 1}, {"RATE", 0, 5, 3, F_ENUM, N_ARATE, 1}},
    {   /* COLOR (the S-4's DEFORM) */
        {"DRV", 0, 100, 0, F_PCT}, {"CRSH", 0, 100, 0, F_PCT},
        {"NOIS", 0, 100, 0, F_PCT}, {"TILT", -100, 100, 0, F_BIPCT},
        /* 2: the noise's decay after the sound that opens it, the noise's tone, what CRUSH takes (bits, the sample
         * rate, both), dry/wet */
        {"NDEC", 0, 127, 40, F_TIME}, {"NTON", -100, 100, 0, F_BIPCT},
        {"CMOD", 0, 2, 0, F_ENUM, N_CMOD}, {"WET", 0, 100, 100, F_PCT},
        /* 3: the output level (drive adds loudness), how WET mixes COLOR in (ROUT: INS or SEND; a send of a sound
         * nothing colours adds the dry to itself, +6 dB at WET 100: LVL is there for it) */
        {"LVL", -24, 6, 0, F_DB}, P_ROUT, {""}, {""}},
    {   /* SPACE (the S-4's VAST, its room shared: space.c): the track's delay (TIME FDBK, DLY its level, TONE in its
         * feedback, SPRD the right echo ahead), and its send to a room (ROOM: A or B; VERB how much). SIZE DEC RTON
         * QUAL PRE are the knobs of the room ROOM picks (SP_RMK: the page edits rmp[], shared by every track on
         * that room; the track's own values there are unused), so they're not modulation targets. Stored 3 and 8
         * were the track's own SIZE and PRE until the rooms were shared (project.c reads a version 7 file's raw).
         * (DLY 0, VERB 0: off until turned up, taking no memory) */
        {"TIME", 10, 370, 250, F_MS}, {"FDBK", 0, 100, 30, F_PCT},
        {"ROOM", 0, 1, 0, F_ENUM, N_ROOMS, 1}, {"SIZE", 0, 100, 50, F_PCT, 0, 1},
        {"DLY", 0, 100, 0, F_PCT}, {"VERB", 0, 100, 0, F_PCT},
        {"TONE", -100, 100, 0, F_BIPCT}, {"SPRD", 0, 100, 100, F_PCT},
        {"DEC", 0, 100, 40, F_PCT, 0, 1}, {"RTON", -100, 100, 0, F_BIPCT, 0, 1},
        {"QUAL", 0, 1, 0, F_ENUM, N_QUAL, 1}, {"PRE", 0, 200, 20, F_MS, 0, 1},
        /* 4: SYNC, TIME in the tempo's divisions (space.c sp_sync_time) */
        {"SYNC", 0, 1, 0, F_ENUM, N_OFFON, 1}},
};

/* The source engines: what starts a track's chain (docs/bryo-architecture.md, "Source engines"). TAPE's knobs are
 * DEV_P[DEV_SRC]; each other source has its own table, and each track keeps every source's values, so switching
 * back and forth loses nothing. */
enum { SRC_TAPE, SRC_SYNTH, SRC_POLY, SRC_DRUM, NSRC };
static const char *const SRC_NAME[NSRC] = {"TAPE", "SYNTH", "POLY", "DRUM"};

/* SYNTH (synth.c): a small subtractive voice from Felucca's ANALOG engine, up to three per track. Stored here as
 * the oscillators, the filter, the envelope (one ADSR for the level and, by ENV, the cutoff), how the keys play it
 * and the level; the pages show them in POLY's order (OSC, AMP, FILTER, VOICE, LEVEL: PLAYOUT below). */
enum { SY_WAVE, SY_DTUN, SY_MIX, SY_NOIS, SY_CUT, SY_RES, SY_ENV, SY_KTRK, SY_ATK, SY_DEC, SY_SUS, SY_REL, SY_VOIC,
       SY_GLID, SY_DRV, SY_TUNE, SY_LVL, SY_FINE };
#define SYN_NV 3u                /* voices per track */
static const pdesc_t SYN_P[NPK] = {
    /* OSC: two oscillators of the same wave, the second DTUN cents up and MIX of the blend; white noise */
    {"WAVE", 0, 4, 3, F_ENUM, N_OSC}, {"DTUN", 0, 100, 8, F_CT}, {"MIX", 0, 100, 50, F_PCT}, {"NOIS", 0, 100, 0, F_PCT},
    /* FILTER: a resonant low-pass; ENV opens (or, below 0, closes) it with the envelope; KTRK follows the keys */
    {"CUT", 0, 127, 72, F_HZ}, {"RES", 0, 100, 25, F_PCT}, {"ENV", -100, 100, 45, F_BIPCT}, {"KTRK", 0, 100, 50, F_PCT},
    /* AMP: the envelope's times (1 ms .. 10 s) and its sustain level */
    {"ATK", 0, 127, 0, F_TIME}, {"DEC", 0, 127, 60, F_TIME}, {"SUS", 0, 100, 60, F_PCT}, {"REL", 0, 127, 45, F_TIME},
    /* VOICE: how many keys sound at once (1: one voice, legato), the glide between notes, drive before the filter,
     * the tuning in semitones */
    {"VOIC", 1, SYN_NV, SYN_NV, F_NUM}, {"GLID", 0, 127, 0, F_TIME}, {"DRV", 0, 100, 0, F_PCT},
    {"TUNE", -24, 24, 0, F_ST},
    /* LEVEL: what the track hears of the synth (every source's last page, as the S-4's LEVEL); FINE: the tuning in
     * cents, on top of TUNE (page 4, beside it) */
    {"LVL", -24, 6, 0, F_DB}, {"FINE", -50, 50, 0, F_CT}, {""}, {""}};

/* POLY (poly.c): a sound played at the keys' pitches, up to four at once, each through its own envelope and filter.
 * The sound is any reel choice: a factory reel, one of yours, or this track's own tape (record into it, then play it
 * across the keys). The key ROOT names (A3 at first: BELL's note, the first reel POLY plays) plays it as it was
 * recorded, FINE cents off. */
enum { PL_REEL, PL_STRT, PL_TUNE, PL_VOIC, PL_ATK, PL_DEC, PL_SUS, PL_REL, PL_CUT, PL_RES, PL_TYPE, PL_ENV, PL_LVL,
       PL_END, PL_ROOT, PL_FINE };
#define POL_NV 4u                /* voices per track */
static const pdesc_t POL_P[NPK] = {
    /* SAMPLE: which sound, where in it a note starts, the tuning, how many notes at once */
    {"REEL", 0, NREEL + USLOT_N, 1, F_ENUM, N_REEL}, {"STRT", 0, 9900, 0, F_PCT100}, {"TUNE", -24, 24, 0, F_ST},
    {"VOIC", 1, POL_NV, POL_NV, F_NUM},
    /* ENV: each voice's envelope */
    {"ATK", 0, 127, 0, F_TIME}, {"DEC", 0, 127, 60, F_TIME}, {"SUS", 0, 100, 100, F_PCT}, {"REL", 0, 127, 40, F_TIME},
    /* FILTER: each voice's state-variable filter, and how far its envelope moves the cutoff */
    {"CUT", 0, 127, 127, F_HZ}, {"RES", 0, 100, 0, F_PCT}, {"TYPE", 0, 2, 0, F_ENUM, N_SLOP},
    {"ENV", -100, 100, 0, F_BIPCT},
    /* LEVEL: what the track hears of POLY's voices; END: where in the sound a note stops (page 1, after STRT; as
     * STRT, in hundredths: HOME held turns either a hundredth a click, the zoom) */
    {"LVL", -24, 6, 0, F_DB}, {"END", 100, 10000, 10000, F_PCT100},
    /* TUNING (page 4): ROOT, the note the sound was recorded at (that key plays it as it is; a setting, not a
     * modulation target: it would step by semitones), and FINE, cents, for a sound that sits between two notes */
    {"ROOT", 24, 96, 57, F_NOTE, 0, 1}, {"FINE", -50, 50, 0, F_CT}};   /* (ROOT A3: BELL's, POLY's first reel) */

/* DRUM (drum.c): a drum machine. Sixteen synthesized instruments on the white keys, played from a pattern of 2..4
 * bars of sixteenths. Four pages: the PATTERN (a written rhythm to start from, its length, swing, how hard an
 * accented step hits), how each pass VARIES from it (SEED picks a version; 0 plays it as written), the KIT (every
 * instrument at once) and STEP (the instrument the keys write, the bar they show, what pressing a key does). PATN,
 * LEN, INST, BAR and MODE aren't modulation targets: PATN writes the pattern when turned, the rest are the page's
 * own state. */
enum { DM_PATN, DM_LEN, DM_SWNG, DM_ACNT, DM_SEED, DM_VARY, DM_FILL, DM_EVOL, DM_TUNE, DM_DECY, DM_TONE, DM_DRV,
       DM_INST, DM_BAR, DM_MODE, DM_LVL };
enum { DMODE_HITS, DMODE_ACC, DMODE_LIVE, DMODE_ERAS };
#define DRM_NPRESET 10u
#define DRM_NBAR 4u
static const char *const N_DPATN[DRM_NPRESET] = {"ROCK", "DISC", "FUNK", "SHFL", "BOSA", "RMBA", "CHA", "BGIN",
                                                 "TNGO", "MRCH"};
static const char *const N_DINST[16] = {"BD", "SD", "CP", "RS", "LC", "LB", "HB", "CL",
                                        "CB", "MA", "TB", "GU", "HH", "OH", "MB", "CY"};
static const char *const N_DMODE[4] = {"HITS", "ACC", "LIVE", "ERAS"};
static const char *const N_EVOL[5] = {"OFF", "1", "2", "4", "8"};
static const pdesc_t DRM_P[NPK] = {
    /* PATTERN: a written rhythm (turned: it replaces the pattern; SAVE held undoes), the bars it loops, swing on
     * the off-sixteenths, an accented step's extra punch */
    {"PATN", 0, DRM_NPRESET - 1, 0, F_ENUM, N_DPATN, 1}, {"LEN", 2, DRM_NBAR, 2, F_NUM, 0, 1},
    {"SWNG", 0, 100, 0, F_PCT}, {"ACNT", 0, 100, 60, F_PCT},
    /* VARY: the version (0: as written), how far it strays, the chance of a fill in the last bar, a new version
     * every EVOL loops */
    {"SEED", 0, 200, 0, F_NUM}, {"VARY", 0, 100, 40, F_PCT}, {"FILL", 0, 100, 25, F_PCT},
    {"EVOL", 0, 4, 0, F_ENUM, N_EVOL},
    /* KIT: every instrument's pitch (semitones), decay (-100: a quarter, +100: four times), brightness, drive */
    {"TUNE", -12, 12, 0, F_ST}, {"DEC", -100, 100, 0, F_BIPCT}, {"TONE", -100, 100, 0, F_BIPCT},
    {"DRV", 0, 100, 0, F_PCT},
    /* STEP: the instrument the keys write (HOME held + a white key picks it too), the bar they show (OCT- / OCT+),
     * what a key does (HITS, ACC: toggles that step; LIVE: plays and writes at the nearest step; ERAS: held, wipes
     * its instrument as the playhead passes), and the level (every source's last page) */
    {"INST", 0, 15, 0, F_ENUM, N_DINST, 1}, {"BAR", 1, DRM_NBAR, 1, F_NUM, 0, 1},
    {"MODE", 0, 3, 0, F_ENUM, N_DMODE, 1}, {"LVL", -24, 6, 0, F_DB}};
/* an instrument's own knobs (a white key held on PATTERN, VARY or KIT: KNOB 1..4 are that instrument's) */
enum { DIN_TUNE, DIN_DECY, DIN_LVL, DIN_TONE, NDIN };
static const pdesc_t DRI_P[NDIN] = {
    {"TUNE", -12, 12, 0, F_ST}, {"DEC", -100, 100, 0, F_BIPCT}, {"LVL", -24, 6, 0, F_DB}, {"TONE", -100, 100, 0, F_BIPCT}};

/* an unused knob on a page (no label) */
static int pdesc_empty(const pdesc_t *d) { return !d->label || !d->label[0]; }


/* each track's channel strip, after the chain and before the mix (the mixer's second page: EDIT on the mixer, held or tapped).
 * LOW and HIGH are shelves (+-12 dB); FILT is one knob for two filters, a low-pass turning left of 0 and a
 * high-pass turning right of it (the DJ-mixer way: 0 is open); PAN is the place in the stereo field. The mixer
 * DSP (phase 6) applies them; until then they are values you can set and see. */
enum { CH_LOW, CH_HIGH, CH_FILT, CH_PAN, NCH };
static const pdesc_t CH_P[NCH] = {
    {"LOW", -12, 12, 0, F_DB}, {"HIGH", -12, 12, 0, F_DB}, {"FILT", -100, 100, 0, F_BIPCT}, {"PAN", -100, 100, 0, F_BIPCT},
};

/* the master compressor on the four tracks' sum (mixer.c; the mixer's MASTER page, EDIT tapped twice): how hard, how
 * fast it closes and opens, and the squeezed against the dry. One for the whole instrument. AMT 0: off. */
enum { MS_AMT, MS_ATK, MS_REL, MS_MIX, NMS };
static const pdesc_t MS_P[NMS] = {
    {"AMT", 0, 100, 0, F_PCT}, {"ATK", 1, 100, 10, F_MS}, {"REL", 20, 990, 200, F_MS}, {"MIX", 0, 100, 100, F_PCT},
};
static int16_t mst[NMS];

/* KEY, the mixer's page after MASTER (EDIT tapped three times): the project's key and scale. With SCAL on, a note
 * SYNTH or POLY starts (a key, the arpeggiator's) that's outside the scale plays the nearest one in it (between two,
 * the lower): the keyboard stays a piano, the wrong notes just can't sound. One for the whole project; a new one
 * starts on A MIN, the key every factory reel is in. */
enum { KY_KEY, KY_SCAL, NKY };
static const char *const N_KEYN[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
static const char *const N_KSCAL[8] = {"OFF", "MAJ", "MIN", "DOR", "MIX", "HMIN", "PEN", "MPEN"};
static const uint16_t KSCAL_MASK[8] = {   /* the notes in each scale, bit n: n semitones above the key */
    0xFFFu, 0xAB5u, 0x5ADu, 0x6ADu, 0x6B5u, 0x9ADu, 0x295u, 0x4A9u};
static const pdesc_t KY_P[4] = {
    {"KEY", 0, 11, 9, F_ENUM, N_KEYN, 1}, {"SCAL", 0, 7, 2, F_ENUM, N_KSCAL, 1}, {""}, {""}};   /* (A MIN: the
                                                                                               * factory reels' key) */
static int16_t keyp[NKY];

/* note n (0..127) as the scale lets it sound: itself, or the nearest note in KEY's SCAL (ties: the lower). Called
 * when a note starts (the ISR, once a note), so the loop is fine */
static int32_t key_snap(int32_t n)
{
    uint32_t m = KSCAL_MASK[(uint32_t)keyp[KY_SCAL] & 7u], pc = (uint32_t)(n + 120 - clamp(keyp[KY_KEY], 0, 11)), d;
    while (pc >= 12u)
        pc -= 12u;
    m |= m << 12;                                        /* (two octaves: pc - d and pc + d without a wrap) */
    for (d = 0; d <= 6u; d++) {
        if ((m >> (pc + 12u - d)) & 1u)
            return clamp(n - (int32_t)d, 0, 127);
        if ((m >> (pc + d)) & 1u)
            return clamp(n + (int32_t)d, 0, 127);
    }
    return n;
}

/* The two rooms the tracks share (space.c; edited on SPACE's pages, the room the track's ROOM picks): SIZE, DEC,
 * TONE (RTON on the page: - darker, + a high-pass in front), QUAL (HI 22.05 kHz, LO 11.025: half the work, a darker
 * tail), PRE. One pair for the whole instrument, stored with the project; not modulation targets (no track owns
 * them). */
#define NROOM 2u
enum { RM_SIZE, RM_DEC, RM_TONE, RM_QUAL, RM_PRE, NRM };
enum { RQ_HI, RQ_LO };
static const pdesc_t RM_P[NRM] = {
    {"SIZE", 0, 100, 50, F_PCT}, {"DEC", 0, 100, 40, F_PCT}, {"TONE", -100, 100, 0, F_BIPCT},
    {"QUAL", 0, 1, RQ_HI, F_ENUM, N_QUAL, 1}, {"PRE", 0, 200, 20, F_MS},
};
static int16_t rmp[NROOM][NRM];
/* SPACE's stored knob k as its room's: RM_* + 1 (0: the track's own) */
static const uint8_t SP_RMK[NPK] = {0, 0, 0, RM_SIZE + 1, 0, 0, 0, 0, RM_DEC + 1, RM_TONE + 1, RM_QUAL + 1, RM_PRE + 1};


/* The arpeggiator (arp.c), one per track, on the ARP pad: it plays the notes held on a SYNTH or POLY track one at a
 * time, in time. MODE: OFF; KEY, while keys are held; LTCH, the last chord keeps playing after the keys are let go
 * (a new chord, pressed after letting go, replaces it); ADD, every key pressed joins the notes for good (an endless
 * pattern you pile up; POLY held on the ARP page clears it). ORDR: UP, DOWN, UPDN (up and back, the ends once), RND,
 * PLAY (the order pressed). RNGE spreads them over 1-3 octaves. RATE a step (8T, 16T: triplets), GATE how much of
 * it the note sounds, SWNG the off-beats late. Not modulation targets (choices the ISR acts on per step). */
static const pdesc_t ARP_P[NPK] = {   /* (NPK long: pdesc_pages looks past the last page) */
    {"MODE", 0, 3, ARM_OFF, F_ENUM, N_ARM, 1}, {"ORDR", 0, 4, ARO_UP, F_ENUM, N_ARO, 1}, {"RNGE", 1, 3, 1, F_NUM, 0, 1},
    {"RATE", 0, 5, 3, F_ENUM, N_ARATE, 1},
    {"GATE", 10, 100, 50, F_PCT, 0, 1}, {"SWNG", 0, 100, 0, F_PCT, 0, 1}, {""}, {""},
    /* page 3, RES ARP: RESONATOR's own arpeggio (stored in RESONATOR's knobs, 12-15, not in tp.arp: ui.c's
     * ui_page points there). The same knobs as page 1, the same ranges as RESONATOR's descriptors */
    {"MODE", 0, 3, ARM_OFF, F_ENUM, N_ARM, 1}, {"ORDR", 0, 4, ARO_UP, F_ENUM, N_ARO, 1}, {"RNGE", 1, 3, 1, F_NUM, 0, 1},
    {"RATE", 0, 5, 3, F_ENUM, N_ARATE, 1}};
#define ARP_RES 8u                   /* ARP_P's index of RES ARP's first knob (page 3) */

/* SETUP, the mixer's last page (EDIT tapped six times): how the panel behaves, for the instrument, not a project
 * (settings.c keeps them). KEYS: the black keys as the shortcuts (SHRT: mutes, reverse, clears, freeze) or as notes
 * (NOTE: the 27 keys a keyboard, F to G, on SYNTH and POLY; GLO held + OCT+ flips it too). KNOB: how far a detent
 * moves a value with a range wider than 32 steps: 1X, 2X, 4X, or ACC (a slow turn 1, a fast one up to 4). HOLD: how
 * long a pad is held before it counts as held. LEDS: how the idle pads glow (settings.c LEDS_*, in the menu's
 * order, darkest first). */
enum { ST_KEYS, ST_KNOB, ST_HOLD, ST_LEDS, NSET };
enum { KNOB_1X, KNOB_2X, KNOB_4X, KNOB_ACC };
static const char *const N_KEYS[2] = {"SHRT", "NOTE"};
static const char *const N_KNOB[4] = {"1X", "2X", "4X", "ACC"};
static const char *const N_HOLDMS[4] = {"300", "400", "500", "600"};
static const char *const N_LEDS[4] = {"OFF", "DIML", "DIMH", "INV"};
static const pdesc_t SET_P[NSET] = {
    {"KEYS", 0, 1, 0, F_ENUM, N_KEYS, 1}, {"KNOB", 0, 3, KNOB_1X, F_ENUM, N_KNOB, 1},
    {"HOLD", 0, 3, 1, F_ENUM, N_HOLDMS, 1}, {"LEDS", 0, 3, 2, F_ENUM, N_LEDS, 1}};

/* The modulator engines, lined up with
 * the Torso S-4's modulators (docs/bryo-architecture.md, "Modulators"). The S-4's WAVE and RANDOM are one LFO
 * here: random is one of its shapes (RND), on the same core, so every shape gets the same rate, placement and
 * timing, and the random knobs (SMTH, VAR, LEN) work on every shape. ADSR and FOLLOW are the S-4's; SEQ is the
 * PRD's. Page by page: the shape's character first, then its refinements, its depth and placement (AMT, OFS,
 * PHAS, SPRD), then the timing switches. The PRD's hold-and-turn sets each target's depth; AMT scales the slot as
 * a whole, as the S-4's AMOUNT does. mod.c runs them. */
enum { ME_WAVE, ME_ADSR, ME_SEQ, ME_FOLLOW, NME };
static const char *const ME_NAME[NME] = {"LFO", "ADSR", "SEQ", "FOLLOW"};
#define P_AMT {"AMT", 0, 100, 100, F_PCT}
#define P_OFS {"OFS", -100, 100, 0, F_BIPCT}
#define P_PHAS {"PHAS", 0, 359, 0, F_NUM}
#define P_SPRD {"SPRD", 0, 100, 0, F_PCT}            /* the right channel's phase against the left: the S-4's SPREAD */
#define P_TRIG {"TRIG", 0, 1, 0, F_ENUM, N_TRIG}      /* free-running, or restarted by a key */
#define LFO_RND 4                                     /* the LFO's random shape (N_SHAPE) */
static const pdesc_t ME_P[NME][NPK] = {
    {   /* LFO. 1: RATE and the shape (SIN TRI SQR SAW RND), bent by SKEW (squashed to a side) and FOLD (peaks
         * folded back). 2: CURV widens or narrows the curves; SMTH slews it (RND: glides between its steps); VAR
         * lets each time round drift from the last (0: the same forever); LEN is RND's steps per loop, and for
         * the other shapes the cycles before VAR's drift repeats. 3: depth and placement. 4: the clock (free Hz
         * or the tempo's divisions), the key restart, a fade-in. */
        {"RATE", 0, 127, 64, F_NUM}, {"SHPE", 0, 4, 0, F_ENUM, N_SHAPE}, {"SKEW", -100, 100, 0, F_BIPCT},
        {"FOLD", 0, 100, 0, F_PCT},
        {"CURV", -100, 100, 0, F_BIPCT}, {"SMTH", 0, 100, 0, F_PCT}, {"VAR", 0, 100, 0, F_PCT}, {"LEN", 1, 16, 8, F_NUM},
        P_AMT, P_OFS, P_PHAS, P_SPRD,
        {"SYNC", 0, 1, 1, F_ENUM, N_CLK}, P_TRIG, {"FADE", 0, 100, 0, F_PCT}, {""}},
    {   /* ADSR: the four stages (the same time curve as SYNTH's and POLY's envelopes, shown the same way), then
         * their curves (0 straight, - logarithmic, + exponential) and SPRD, then the key's velocity, looping (attack
         * and decay cycling), depth and offset */
        {"ATK", 0, 127, 10, F_TIME}, {"DEC", 0, 127, 50, F_TIME}, {"SUS", 0, 100, 60, F_PCT}, {"REL", 0, 127, 50, F_TIME},
        {"ACRV", -100, 100, 0, F_BIPCT}, {"DCRV", -100, 100, 0, F_BIPCT}, {"RCRV", -100, 100, 0, F_BIPCT}, P_SPRD,
        {"VEL", 0, 100, 0, F_PCT}, {"LOOP", 0, 1, 0, F_ENUM, N_OFFON}, P_AMT, P_OFS},
    {   /* SEQ (the PRD's): 16 steps of values (a white key held + KNOB 1 sets one), a step a RATE (1 bar .. 1/32),
         * then their order, restart, chance and first step, then spread, depth and offset */
        {"LEN", 1, 16, 16, F_NUM}, {"RATE", 0, 5, 2, F_ENUM, N_SRATE}, {"SLEW", 0, 100, 0, F_PCT}, {"SWNG", 0, 100, 0, F_PCT},
        {"DIR", 0, 3, 0, F_ENUM, N_DIR}, P_TRIG, {"PROB", 0, 100, 100, F_PCT}, {"STRT", 1, 16, 1, F_NUM},
        P_SPRD, P_AMT, P_OFS},   /* (SPRD: the right side lags, as ADSR's and FOLLOW's) */
    {   /* FOLLOW: the envelope of a track's sound (SELF: this track's own, whatever its source) or USB in; GAIN
         * into it, how fast it rises and falls; then sample-and-hold to the tempo, depth, offset, spread */
        {"SRC", 0, 5, 0, F_ENUM, N_SRC}, {"GAIN", -12, 12, 0, F_DB}, {"RISE", 0, 127, 10, F_NUM},
        {"FALL", 0, 127, 50, F_NUM},
        {"HOLD", 0, 4, 0, F_ENUM, N_HOLD}, P_AMT, P_OFS, P_SPRD},
};
/* a SEQ slot's steps before you set them: a pattern with an accent on each beat, so the page shows the idea
 * (the slot's depth is 0 until it's assigned, so this moves nothing) */
static const int8_t SEQ_DEF[16] = {100, 25, 60, 25, 80, 25, 60, 40, 100, 25, 60, 25, 80, 50, 35, 20};

/* Where the knobs sit on the pages. A table above is in the order its knobs are stored (the DSP, the modulation
 * targets and the projects number them that way); a page shows them in the order below. These rules hold for every
 * device and engine: a source's (and COLOR's) LVL is KNOB 4 of its last page; an effect's ROUT and WET are KNOB 3
 * and 4 of its first (SPACE's sends, DLY and VERB, the same); a modulator's AMT and OFS are KNOB 3 and 4 of its last
 * page. at[]: the stored knob at each page
 * position (an empty knob's index where the position is empty). A table not listed shows in stored order. */
typedef struct {
    const pdesc_t *p;
    uint8_t pages;
    uint8_t at[NPK];
} playout_t;
static const playout_t PLAYOUT[] = {
    {DEV_P[DEV_SRC], 4, {0, 1, 2, 3, 12, 13, 14, 15, 4, 5, 6, 7, 8, 9, 11, 10}},   /* TAPE: the envelope second (as
                                                                                     * SYNTH's, POLY's), REEL ROTA SYNC
                                                                                     * LVL */
    /* the effects open with their mix: ROUT and WET KNOB 3-4 of page 1 (SPACE: DLY and VERB, its sends), so the
     * first press of EDIT or FX can bring one in; the sound's knobs follow (2026-10-11) */
    {DEV_P[DEV_GRAIN], 6, {0, 1, 11, 4, 2, 3, 5, 6, 7, 8, 9, 10, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23}},
                                                    /* GRAIN: SIZE RATE ROUT WET, PTCH SPRD SPRY CONT, REV PATN SCAL
                                                     * PRND, SCAN.., BODY.., GRNS */
    {DEV_P[DEV_RESO], 3, {0, 1, 8, 3, 4, 5, 6, 7, 2, 9, 10, 11, 12, 13, 14, 15}},   /* RESONATOR: PTCH DEC ROUT WET,
                                                                                      * CUT RES TYPE SCAL, TONE PICK
                                                                                      * STRM VARY (12-15,
                                                                                      * RES ARP: the ARP pad's) */
    {DEV_P[DEV_COLOR], 3, {0, 1, 9, 7, 2, 3, 4, 5, 6, 10, 11, 8}},              /* COLOR: DRV CRSH ROUT WET, NOIS TILT
                                                                                 * NDEC NTON, CMOD . . LVL */
    {DEV_P[DEV_SPACE], 4, {0, 1, 4, 5, 2, 3, 8, 11, 6, 7, 9, 10, 12, 13, 14, 15}},   /* SPACE: TIME FDBK DLY VERB,
                                                                                 * ROOM SIZE DEC PRE, TONE SPRD RTON
                                                                                 * QUAL, SYNC (2-3 and 8-11: the
                                                                                 * room's, SP_RMK) */
    {SYN_P, 5, {0, 1, 2, 3, 8, 9, 10, 11, 4, 5, 6, 7, 12, 13, 15, 17, 14, 18, 19, 16}},   /* OSC AMP FILTER, VOIC GLID
                                                                                       * TUNE FINE, DRV . . LVL */
    {POL_P, 4, {0, 1, 13, 2, 4, 5, 6, 7, 8, 9, 10, 11, 3, 14, 15, 12}},         /* POLY: REEL STRT END TUNE, ..., VOIC
                                                                                 * ROOT FINE LVL */
    {ME_P[ME_WAVE], 4, {0, 1, 2, 3, 4, 5, 6, 7, 12, 13, 14, 15, 10, 11, 8, 9}},  /* LFO: timing, then PHAS SPRD AMT OFS */
    {ME_P[ME_SEQ], 3, {0, 1, 2, 3, 4, 5, 6, 7, 11, 8, 9, 10}},                  /* SEQ: . SPRD AMT OFS */
    {ME_P[ME_FOLLOW], 2, {0, 1, 2, 3, 4, 7, 5, 6}},                             /* FOLLOW: HOLD SPRD AMT OFS */
};

static const playout_t *pdesc_layout(const pdesc_t *p)
{
    uint32_t i;
    for (i = 0; i < sizeof PLAYOUT / sizeof PLAYOUT[0]; i++)
        if (PLAYOUT[i].p == p)
            return &PLAYOUT[i];
    return 0;
}

/* the stored knob of table p at page position pos (4 x page + knob) */
static uint32_t pdesc_at(const pdesc_t *p, uint32_t pos)
{
    const playout_t *l = pdesc_layout(p);
    return l && pos < NPK ? l->at[pos] : pos;
}

/* table p's knobs up to its last that has a label (what a project or a preset needs to store: past it, every one
 * is empty and reads back as 0 however many are written; NPK 24 written whole would no longer fit the biggest project
 * in its sector) */
static uint32_t pdesc_len(const pdesc_t *p)
{
    uint32_t n = NPK;
    while (n && pdesc_empty(&p[n - 1u]))
        n--;
    return n;
}

/* a device's or an engine's pages: its layout's, else up to the first page whose first knob has no label */
static uint32_t pdesc_pages(const pdesc_t *p)
{
    const playout_t *l = pdesc_layout(p);
    uint32_t n = 1;
    if (l)
        return l->pages;
    while (n < NPK / 4u && !pdesc_empty(&p[4u * n]))
        n++;
    return n;
}

/* The slots' default engines. Four modulators a track: three you can swap between LFO, ADSR and FOLLOW, which the
 * LFO pad holds (LFO held + OP1, OP2, OP3 picks the one it shows; ENV goes straight to the second), and the step
 * sequencer, which keeps the SEQ pad and stays a SEQ. The ARP pad is the arpeggiator (arp.c), not a slot. Stored
 * slots keep their old numbers (projects and depths are by slot): slot 2 is the SEQ (shown M4), slot 3 the third
 * swappable one (shown M3, a random LFO to start, as it was when ARP held it). */
static const uint8_t SLOT_DEF_ENGINE[NSLOT] = {ME_WAVE, ME_ADSR, ME_SEQ, ME_WAVE};   /* slot 3: the LFO, RND */
#define SLOT_SEQ 2u                                   /* the SEQ pad's slot */
static const uint8_t SLOT_NUM[NSLOT] = {1, 2, 4, 3};  /* the number a slot shows as: M1 M2 M4 M3 */
static const uint8_t SLOT_OF_NUM[NSLOT] = {0, 1, 3, 2};   /* .. and back: M1..M4's slot */
/* the engines a swappable slot can run (not SEQ: the SEQ pad has it) */
static const uint8_t SWAP_ENGINE[4] = {ME_WAVE, ME_ADSR, ME_SEQ, ME_FOLLOW};   /* (the LFO pad's M1-M3: any of four) */

typedef struct {
    int16_t dev[NDEV][NPK];
    uint8_t engine[NSLOT];       /* the engine each slot runs */
    int16_t mod[NSLOT][NPK];     /* the slot's own knobs */
    int16_t ch[NCH];             /* the channel strip */
    int8_t steps[NSLOT][16];     /* a SEQ slot's step values, 0..100 (set per step from phase 7: slot + key + knob) */
    uint8_t src;                 /* the source engine (SRC_*): the main loop writes it, the ISR follows */
    uint8_t recin;               /* what REC records onto the tape (RIN_*: the routing view) */
    int16_t syn[NPK];            /* SYNTH's knobs (TAPE's are dev[DEV_SRC]) */
    int16_t pol[NPK];            /* POLY's */
    int16_t drm[NPK];            /* DRUM's */
    uint16_t dpat[DRM_NBAR][16]; /* DRUM's pattern: per bar and step, the instruments that hit (a bit each) */
    uint16_t dacc[DRM_NBAR];     /* .. the accented steps of each bar (a bit each) */
    int16_t dins[16][NDIN];       /* .. each instrument's own TUNE DEC LVL TONE */
    int16_t arp[NARP];           /* the arpeggiator's knobs (arp.c) */
} track_params_t;
static track_params_t tp[NTRK];
/* the value knob k of track t's SPACE edits: the track's own, or (SP_RMK) its room's */
static int16_t *sp_knob(uint32_t t, uint32_t k)
{
    if (k < NPK && SP_RMK[k])
        return &rmp[clamp(tp[t].dev[DEV_SPACE][2], 0, 1)][SP_RMK[k] - 1u];
    return &tp[t].dev[DEV_SPACE][k % NPK];
}

/* What a track's REC records (the routing view: ALGORITHM, KNOB 1..4 a track each). AUTO is what REC always did:
 * on a TAPE track the other tracks' mix, on a SYNTH or POLY track its own source (play, then slice it). OTHR: the
 * others' mix whatever the source; T1..T4: that one track, after its effects (a track's own number reads SELF: it
 * records itself back onto its tape, DUB setting how much of the old pass stays). */
enum { RIN_AUTO, RIN_OTHR, RIN_T1 };
#define NRIN (RIN_T1 + NTRK)
static const char *const N_RIN[NTRK][NRIN] = {
    {"AUTO", "OTHR", "SELF", "T2", "T3", "T4"}, {"AUTO", "OTHR", "T1", "SELF", "T3", "T4"},
    {"AUTO", "OTHR", "T1", "T2", "SELF", "T4"}, {"AUTO", "OTHR", "T1", "T2", "T3", "SELF"}};
static const pdesc_t RIN_P[NTRK] = {
    {"T1", 0, NRIN - 1, RIN_AUTO, F_ENUM, N_RIN[0]}, {"T2", 0, NRIN - 1, RIN_AUTO, F_ENUM, N_RIN[1]},
    {"T3", 0, NRIN - 1, RIN_AUTO, F_ENUM, N_RIN[2]}, {"T4", 0, NRIN - 1, RIN_AUTO, F_ENUM, N_RIN[3]}};

/* device d of track t as the pages see it: the source's knobs are the chosen source's */
static const pdesc_t *dev_p(uint32_t t, uint32_t d)
{
    if (d != DEV_SRC || tp[t].src == SRC_TAPE)
        return DEV_P[d];
    return tp[t].src == SRC_SYNTH ? SYN_P : tp[t].src == SRC_POLY ? POL_P : DRM_P;
}
static int16_t *dev_v(uint32_t t, uint32_t d)
{
    if (d != DEV_SRC || tp[t].src == SRC_TAPE)
        return tp[t].dev[d];
    return tp[t].src == SRC_SYNTH ? tp[t].syn : tp[t].src == SRC_POLY ? tp[t].pol : tp[t].drm;
}
static const char *dev_name(uint32_t t, uint32_t d) { return d == DEV_SRC ? SRC_NAME[tp[t].src % NSRC] : DEV_NAME[d]; }

static void drm_defaults(uint32_t t);   /* drum.c: the pattern a DRUM track starts with */

/* slot s of track t runs engine e: its knobs start from that engine's defaults */
static void param_engine(uint32_t t, uint32_t s, uint32_t e)
{
    uint32_t k;
    tp[t].engine[s] = (uint8_t)e;
    for (k = 0; k < NPK; k++)
        tp[t].mod[s][k] = pdesc_empty(&ME_P[e][k]) ? 0 : ME_P[e][k].def;
}

static void param_defaults(void)
{
    uint32_t t, d, k, s;
    for (k = 0; k < NMS; k++)                            /* the master compressor */
        mst[k] = MS_P[k].def;
    for (k = 0; k < NKY; k++)                            /* the key and scale */
        keyp[k] = KY_P[k].def;
    for (s = 0; s < NROOM; s++)                          /* the rooms */
        for (k = 0; k < NRM; k++)
            rmp[s][k] = RM_P[k].def;
    for (t = 0; t < NTRK; t++) {
        for (d = 0; d < NDEV; d++)
            for (k = 0; k < NPK; k++)                    /* (SPACE's places for its room's knobs: 0, never read) */
                tp[t].dev[d][k] = pdesc_empty(&DEV_P[d][k]) || (d == DEV_SPACE && SP_RMK[k]) ? 0 : DEV_P[d][k].def;
        for (k = 0; k < NCH; k++)
            tp[t].ch[k] = CH_P[k].def;
        tp[t].src = SRC_TAPE;
        tp[t].recin = RIN_AUTO;
        for (k = 0; k < NARP; k++)
            tp[t].arp[k] = pdesc_empty(&ARP_P[k]) ? 0 : ARP_P[k].def;
        for (k = 0; k < NPK; k++) {
            tp[t].syn[k] = SYN_P[k].def;
            tp[t].pol[k] = pdesc_empty(&POL_P[k]) ? 0 : POL_P[k].def;
            tp[t].drm[k] = DRM_P[k].def;
        }
        drm_defaults(t);
        tp[t].pol[PL_REEL] = (int16_t)(t < NREEL ? t + 1u : 1u);       /* POLY starts on the track's reel too */
        tp[t].dev[DEV_SRC][8] = (int16_t)(t < NREEL ? t + 1u : 0u);   /* track n plays reel n to start */
        tp[t].dev[DEV_SPACE][2] = (int16_t)(t >= 2u);                  /* SPACE ROOM: tracks 1-2 room A, 3-4 B */
        for (s = 0; s < NSLOT; s++) {
            param_engine(t, s, SLOT_DEF_ENGINE[s]);
            if (s == 3u) {                               /* ARP's slot: the random LFO, a loop that drifts a little */
                tp[t].mod[s][1] = LFO_RND;
                tp[t].mod[s][6] = 20;
            }
            for (k = 0; k < 16u; k++)
                tp[t].steps[s][k] = SEQ_DEF[k];
        }
    }
}

/* a value step for one detent: fine for small ranges, ~1 % of the range for wide ones (the knobs are detented
 * encoders, so a full sweep of a wide range is about two turns) */
static int32_t param_step(const pdesc_t *d)
{
    int32_t span = d->max - d->min;
    return span > 200 ? span / 100 : 1;
}

static int16_t param_nudge(const pdesc_t *d, int32_t v, int32_t detents)
{
    return (int16_t)clamp(v + detents * param_step(d), d->min, d->max);
}

/* the position of v in its range, 0..1000 (the dial's arc) */
static int32_t param_ratio(const pdesc_t *d, int32_t v)
{
    return d->max > d->min ? (v - d->min) * 1000 / (d->max - d->min) : 0;
}

/* v as the strip prints it: value and unit apart (the unit is drawn smaller) */
static void param_format(const pdesc_t *d, int32_t v, char *val, const char **unit)
{
    static const char NOTE[12][3] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    *unit = "";
    switch (d->fmt) {
    case F_PCT:
        fmt_int(val, v);
        *unit = "%";
        break;
    case F_PCT100:                                      /* (whole: the zoom shows the hundredths, ui.c) */
        fmt_int(val, v / 100);
        *unit = "%";
        break;
    case F_BIPCT:
    case F_ST:
        if (v > 0) {
            val[0] = '+';
            fmt_int(val + 1, v);
        } else {
            fmt_int(val, v);
        }
        *unit = d->fmt == F_ST ? "st" : "%";
        break;
    case F_NOTE:
        if (d->names && v == d->min) {                  /* (its lowest value named: BODY's ROOT RES) */
            str_cpy(val, d->names[0], 6);
            break;
        }
        str_cpy(val, NOTE[(uint32_t)v % 12u], 4);
        fmt_int(val + str_len(val), v / 12 - 1);
        break;
    case F_MS:
        fmt_int(val, v);
        *unit = "ms";
        break;
    case F_DB:
        if (v > 0) {
            val[0] = '+';
            fmt_int(val + 1, v);
        } else {
            fmt_int(val, v);
        }
        *unit = "dB";
        break;
    case F_ENUM:
        str_cpy(val, d->names[v - d->min], 6);
        break;
    case F_CT:
        if (d->min < 0 && v > 0) {                      /* (FINE: signed, as TUNE) */
            val[0] = '+';
            fmt_int(val + 1, v);
            break;
        }
        fmt_int(val, v);
        *unit = "ct";
        break;
    case F_HZ:                                          /* a cutoff (0..127 on CUTOFF_HZ): 820Hz, 2.4k, 12k */
    case F_TIME: {                                      /* a time (0..127 on TIME_MS_X10): 35ms, 1.2s */
        uint32_t x = d->fmt == F_HZ ? CUTOFF_HZ[v & 127] : (TIME_MS_X10[v & 127] + 5u) / 10u;
        if (x < 1000u) {
            fmt_int(val, (int32_t)x);
            *unit = d->fmt == F_HZ ? "Hz" : "ms";
        } else {
            fmt_int(val, (int32_t)(x / 1000u));
            if (x < 10000u) {
                str_cpy(val + str_len(val), ".", 2);
                fmt_int(val + str_len(val), (int32_t)(x % 1000u / 100u));
            }
            *unit = d->fmt == F_HZ ? "k" : "s";
        }
        break;
    }
    default:
        fmt_int(val, v);
        break;
    }
}
