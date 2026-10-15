/* SPDX-License-Identifier: GPL-3.0-only */
/* Bryo: RIPPLE, the device after GRAIN (docs/bryo-architecture.md, "RIPPLE, as built"): four tuned strings the
 * track's sound rings through, the S-4's RING with the PRD's strings. Its picture (ui_viz.c viz_reso) draws the
 * same mappings this plays.
 *
 * A string is a Karplus-Strong loop: a delay line one period long, a one-pole low-pass in the loop (TONE: 0 dark,
 * 100 bright), and a loop gain set so it rings DEC long (DEC 0: 80 ms .. 100: 20 s, to -60 dB). The four strings sit
 * at the first four partials SCAL keeps above the root: HARM the root's harmonics 1 2 3 4 (each string adds its own
 * overtones on top, so all the harmonics ring), MAJ root, third, fifth, octave; MIN the same with a minor third; PEN
 * root, second, third, fifth. What excites them is the track's sound after GRAIN, through a filter first (CUT RES
 * TYPE: the same state-variable filter as POLY's, low-, band- or high-pass), and WET blends the strings with it. The
 * strings sit across the stereo field, 1 and 3 to the left, 2 and 4 to the right.
 *
 * The root is PTCH, or the last white key played on the RIPPLE page (until PTCH is turned again). On a TAPE track
 * those keys pluck the strings too, a burst of noise one period long (a sound from nothing: a blank tape, RIPPLE
 * and REC); on a SYNTH or POLY track the keys still play the source and the strings follow its notes.
 *
 * Memory: four chunks of the shared memory (mem.c, a string's delay line in each: 1,056 samples, so the lowest
 * string is F1, 43.7 Hz), taken while WET is above 0 on a track in use and given back otherwise.
 *
 * Load: about 15 instructions a string a sample. Shedding (chain_shed) takes strings after grains: one at a time down
 * to two, one back a second later. Audio ISR, except reso_poll (the main loop: the memory). Integer only. */

#define RS_N 4u                      /* strings a track */
#define RS_NB 12u                    /* BANK's bands (three a string's chunk: shedding takes them three at a time) */
#define RS_BANK_TOP (120 * 16)       /* BANK: no band above C9 (8.4 kHz) */
#define RS_LEN (sizeof(mem_chunk_t) / 2u)   /* a string's delay line: a chunk as samples (1,056) */
enum { RP_PTCH, RP_DEC, RP_TONE, RP_WET, RP_CUT, RP_RES, RP_SLOP, RP_SCAL, RP_ROUT, RP_PICK, RP_STRM, RP_VARY,
       RP_AMODE, RP_AORD, RP_ARNG, RP_ARAT, RP_MODE, RP_DRV, RP_RIPL, RP_KEYS, RP_PLVL, RP_SPRD, RP_EVEN };
                                                    /* (12-15: its arpeggio; 16: its form, EDIT held + SELECT) */
/* the form, MODE (2026-10-13: picked as a source is, EDIT held + SELECT on RIPPLE's pages, each with its own pages
 * and picture): STRG the strings; COMB, combs (half the line, fed back inverted: the odd harmonics only, hollow);
 * RESO, four resonators (a resonant band-pass a partial, no line: a sitar's strings); BANK, twelve of them over the
 * chord's partials (a Polivoks-like filter bank: what comes in is sorted into the chord) */
enum { RM_STRG, RM_COMB, RM_RESO, RM_BANK, NRMODE };
/* KEYS: what the keys on RIPPLE's page and RIPL ARP's steps do. PLCK: set the root and pluck (a sound of its own,
 * on a TAPE or DRUM track; the arpeggio on any); TUNE: set the root only, so what rings is the track's sound, played
 * in the keys' tune */
enum { RK_PLCK, RK_TUNE };

/* the partials above the root, 1/16 semitones, by SCAL: the strings take the first four (RS_N), the picture
 * (ui_viz.c viz_reso) draws every one. HARM is the root's harmonics (12 log2 k); the rest are a chord's tones
 * stacked over three octaves (2026-10-13: were HARM MAJ MIN PEN; the chord sets are what a second knob would have
 * been for: a string set is a chord, and sus, sevenths, diminished and augmented are the chords a key has). The
 * first four are what sounds: MAJ root third fifth octave, DOM7 root third fifth seventh (no octave: the seventh's
 * rub is the point), 5TH root fifth octave twelfth (a power chord), OCT four octaves (one note, four registers). */
#define RS_PARTS 24u
static const int16_t RS_PART16[13][RS_PARTS] = {
    {0, 192, 304, 384, 446, 496, 539, 576, 609, 638, 664, 688, 710, 731, 750, 768, 785, 801, 816, 830, 843, 856, 869,
     880},                                                                   /* HARM */
    {0, 64, 112, 192, 256, 304, 384, 448, 496, 576},                         /* MAJ */
    {0, 48, 112, 192, 240, 304, 384, 432, 496, 576},                         /* MIN */
    {0, 32, 64, 112, 144, 192, 224, 256, 304, 336, 384, 416, 448, 496, 528, 576},   /* PEN */
    {0, 32, 112, 192, 224, 304, 384, 416, 496, 576},                         /* SUS2 */
    {0, 80, 112, 192, 272, 304, 384, 464, 496, 576},                         /* SUS4 */
    {0, 64, 112, 160, 192, 256, 304, 352, 384, 448, 496, 544, 576},          /* DOM7 */
    {0, 64, 112, 176, 192, 256, 304, 368, 384, 448, 496, 560, 576},          /* MAJ7 */
    {0, 48, 112, 160, 192, 240, 304, 352, 384, 432, 496, 544, 576},          /* MIN7 */
    {0, 48, 96, 144, 192, 240, 288, 336, 384, 432, 480, 528, 576},           /* DIM (a diminished seventh) */
    {0, 64, 128, 192, 256, 320, 384, 448, 512, 576},                         /* AUG */
    {0, 112, 192, 304, 384, 496, 576},                                       /* 5TH */
    {0, 192, 384, 576, 768}};                                                /* OCT */
static const uint8_t RS_PART_N[13] = {24, 10, 10, 16, 10, 10, 13, 13, 13, 13, 10, 7, 5};
/* partial k of set sc, 1/16 semitones over the root: the row's, then (BANK's twelve) the row again, its span up */
static inline __attribute__((always_inline)) int32_t rs_part16(int32_t sc, uint32_t k)
{
    uint32_t n = RS_PART_N[sc] - 1u;
    return k <= n ? RS_PART16[sc][k] : RS_PART16[sc][k % n] + (int32_t)(k / n) * RS_PART16[sc][n];
}
#define RS_SCALES 12                 /* SCAL's last index */
/* DEC as a ring time to -60 dB, ms, every 10 */
static const uint16_t RS_T60[11] = {80, 140, 250, 440, 780, 1400, 2400, 4300, 7600, 13000, 20000};
/* 2^(-i/16), Q15: a loop gain from its exponent */
static const uint16_t RS_EXP2N[17] = {32767, 31379, 30048, 28774, 27554, 26386, 25268, 24196, 23170,
                                      22188, 21247, 20347, 19484, 18658, 17867, 17110, 16384};

typedef struct {
    uint8_t map[RS_N];               /* each string's chunk (the main loop's) */
    volatile uint8_t nch;
    /* the ISR's */
    uint32_t w;                      /* the write index, shared by the strings */
    int32_t lp[RS_NB];               /* each loop's low-pass (a band's last output) */
    uint32_t quiet;                  /* samples in a row of silence in and next to none out (the strings resting) */
    volatile uint16_t lvl[RS_NB];    /* ISR -> the screen: each string's level, a peak of its loop's sample taken once
                                      * a block, falling an eighth a block (ui_viz.c: a string ringing, in orange) */
    int32_t ic1, ic2;                /* the filter in front */
    int32_t root16;                  /* a key's root, 1/16 semitone (0: none, PTCH's; a key's is F1 at least) */
    int32_t rs16;                    /* the root the strings sound, gliding to the root (0: not yet) */
    uint32_t dq[RS_N];               /* each string's delay last block, Q8 (0: none): a new one is reached sample by
                                      * sample across the block, so a moving pitch never jumps the read point */
    int16_t ptch_seen;               /* PTCH when the key was played (turning PTCH takes over again) */
    int32_t wet;                     /* WET last block (ramped across the next), 0..100 */
    int32_t dry;                     /* the track's share last block + 1, Q15 (INS: 1 - WET; SEND: all; 0: not set) */
    uint32_t keys;
    int32_t rng;
    uint32_t pend[RS_NB];            /* a strum: samples until string k is plucked (0: not waiting) */
    int32_t proot, pa, plv[RS_NB];    /* .. the pluck it belongs to: its root, brightness, each string's level */
    int32_t psc;                     /* .. and its SCAL */
    int32_t bs1[RS_NB], bs2[RS_NB];  /* RESO, BANK: each band's filter state */
    uint32_t burst[RS_NB];           /* .. a pluck's noise still to feed band k, samples (0: none) */
    int32_t by[RS_NB], blv[RS_NB];     /* .. its one-pole (the pluck's brightness) and its level */
    uint32_t rph;                    /* RIPL's phase (string k a quarter turn on from string k - 1) */
    int32_t rc[RS_N];                /* RIPL: each string's pitch offset this block, cents */
    uint8_t mode_seen;               /* MODE last block: a change clears the lines (what rang in the old one would
                                      * be read wrong by the new) */
} reso_t;

static reso_t reso[NTRK];
static uint32_t reso_cap = RS_N;     /* strings that sound, lowered by chain_shed */
static uint32_t reso_calm;

/* a string's delay line */
static inline __attribute__((always_inline)) int16_t *rs_line(const reso_t *R, uint32_t k) { return (int16_t *)(void *)mem_at(R->map[k]); }

/* the root now, 1/16 semitone */
static int32_t reso_root16(uint32_t t)
{
    return reso[t].root16 > 0 ? reso[t].root16 : mod_pitch16(t, FN_RESO, TPD(t, DEV_RESO)[RP_PTCH]);
}

/* a period, Q8 samples, for pitch n16 (kept inside a string's line) */
static uint32_t rs_period_q8(int32_t n16)
{
    uint32_t inc = pitch_inc((uint32_t)clamp(n16, 29 * 16, 2047)) >> 8, p;
    p = inc ? 0xFFFFFFFFu / inc : (RS_LEN - 2u) << 8;
    return p > (RS_LEN - 2u) << 8 ? (RS_LEN - 2u) << 8 : p;
}

/* a period, Q8 samples, for pitch n16 and ct cents more (RIPL: the strings' slow drift; cents_inc has two divides,
 * so rs_period_q8 is kept for the still case) */
static uint32_t rs_period_q8c(int32_t n16, int32_t ct)
{
    uint32_t inc = cents_inc(clamp(n16, 29 * 16, 2047), ct, 0) >> 8, p;
    p = inc ? 0xFFFFFFFFu / inc : (RS_LEN - 2u) << 8;
    return p > (RS_LEN - 2u) << 8 ? (RS_LEN - 2u) << 8 : p;
}

/* MODE now (a choice the main loop makes: not modulated) */
static inline __attribute__((always_inline)) int32_t rs_mode(const reso_t *R)
{
    return clamp(tp[R - reso].dev[DEV_RESO][RP_MODE], 0, NRMODE - 1);
}

/* DRV's shaper: x through a cubic, x - x^3 / 3, after a gain of 1 + DRV / 50 (gq, Q8) and before its inverse (ig,
 * Q14: at Q8 the round trip was 0.4 % off, enough to shorten a ring), so a small signal passes as it is and a big
 * one rounds off (the top at 2/3 of full scale over the gain):
 * what a Polivoks does to its feedback, the op amps running out of swing. In the loop (STRG, COMB) it's on what's
 * fed back, so a loud string compresses and gains its odd harmonics while it rings and a quiet one rings clean;
 * in BANK it's on what reaches the bands */
static inline __attribute__((always_inline)) int32_t rs_shape(int32_t x, int32_t gq, int32_t ig)
{
    int32_t u = (x * gq) >> 8, s;
    u = u > 32767 ? 32767 : u < -32767 ? -32767 : u;
    s = u - (((((u * u) >> 15) * u) >> 15) * 10923 >> 15);   /* (10923: 1/3, Q15) */
    return (s * ig) >> 14;
}

/* what the loop feeds back for its low-passed sample lp: times the gain g (Q15; settle: rounded toward 0,
 * rs_settle, at both of the shaper's steps too, so a residue shrinks every pass), through DRV's shaper when it's
 * on: gq its gain (Q8), g3 the loop gain over three times it, since the cubic is taken as 3u - u^3 (one multiply
 * fewer a sample than u - u^3 / 3). COMB passes g and g3 negated: the toward-zero rounding goes by the product's
 * sign, so it holds either way. DRV 0 in STRG is the loop as it always was. Every term is a value, not a field: a
 * store into the line or the sum would make the compiler read a field back each sample */
static inline __attribute__((always_inline)) int32_t rs_fb(int32_t lp, int32_t g, int32_t g3, int32_t drv, int32_t gq,
                                                           int settle)
{
    int32_t t;
    if (drv) {
        int32_t u = settle ? (lp * gq + ((lp >> 31) & 255)) >> 8 : (lp * gq) >> 8;
        u = u > 32767 ? 32767 : u < -32767 ? -32767 : u;
        t = (u * 3 - ((((u * u) >> 15) * u) >> 15)) * g3;   /* (|3u - u^3| < 2^17, g3 < 2^14: under 2^31) */
    } else {
        t = lp * g;
    }
    return settle ? (t + ((t >> 31) & 32767)) >> 15 : t >> 15;
}

/* the loop gain for a period of p samples to ring t60 ms to -60 dB: 2^-(9.97 p / (44.1 t60)), Q15 */
static int32_t rs_gain(uint32_t p, uint32_t t60)
{
    uint32_t e = p * 14817u / t60, ip = e >> 16, fr = e & 0xFFFFu, i = fr >> 12, r = fr & 0xFFFu;
    int32_t g = RS_EXP2N[i] + (((int32_t)RS_EXP2N[i + 1] - (int32_t)RS_EXP2N[i]) * (int32_t)r >> 12);
    return ip > 14u ? 0 : g >> ip;
}

/* DEC 0..100 as a ring time, ms */
/* TONE's damping, 32767 * (15 + 85 * TONE / 100) / 100 (the same arithmetic: two divides a block spared) */
static const int16_t RS_TONE_A[101] = {
    4915, 4915, 5242, 5570, 5898, 6225, 6553, 6553, 6881, 7208, 7536, 7864, 8191, 8519, 8519, 8847, 9174, 9502, 9830,
    10157, 10485, 10485, 10813, 11140, 11468, 11796, 12123, 12123, 12451, 12779, 13106, 13434, 13762, 14089, 14089,
    14417, 14745, 15072, 15400, 15728, 16055, 16055, 16383, 16711, 17038, 17366, 17694, 17694, 18021, 18349, 18677,
    19004, 19332, 19660, 19660, 19987, 20315, 20643, 20970, 21298, 21626, 21626, 21953, 22281, 22609, 22936, 23264,
    23264, 23592, 23919, 24247, 24575, 24902, 25230, 25230, 25558, 25885, 26213, 26541, 26868, 27196, 27196, 27524,
    27851, 28179, 28507, 28834, 28834, 29162, 29490, 29817, 30145, 30473, 30800, 30800, 31128, 31456, 31783, 32111,
    32439, 32767};

static uint32_t rs_t60(int32_t dec)
{
    static int32_t last = -1;                           /* (ISR only: the answer kept for the same DEC, three
                                                         * divides a block spared while it holds) */
    static uint32_t lastv;
    int32_t d = clamp(dec, 0, 100), i, f;
    if (d == last)
        return lastv;
    i = d / 10;
    f = d % 10;
    last = d;
    lastv = i >= 10 ? RS_T60[10] : RS_T60[i] + (uint32_t)((RS_T60[i + 1] - RS_T60[i]) * f / 10);
    return lastv;
}

/* string k plucked: one period of noise into its line, behind the write index, softened by a (the pluck's
 * brightness, as TONE's damping), at level lv (Q15) */
static void HOT_CODE rs_pluck1(reso_t *R, uint32_t k, int32_t root16, int32_t a, int32_t lv, int32_t sc)
{
    int16_t *d = rs_line(R, k);
    int32_t mode = rs_mode(R);
    uint32_t p = rs_period_q8(root16 + rs_part16(sc, k)) >> (mode == RM_COMB ? 9 : 8), at = R->w, i;
    int32_t y = 0;
    R->quiet = 0;                                       /* (awake: something to ring) */
    if (mode >= RM_RESO) {                              /* no line: the burst goes into the band, sample by sample */
        R->burst[k] = p + 2u;
        R->by[k] = 0;
        R->blv[k] = lv;
        R->bs1[k] = R->bs2[k] = 0;
        return;
    }
    for (i = 0; i < p + 2u; i++) {
        int32_t x = (int32_t)(noise32(&R->rng) >> 17) - 16384;
        y += (a * (x - y)) >> 15;
        at = at ? at - 1u : RS_LEN - 1u;
        d[at] = (int16_t)clamp(d[at] + (lv >= 32767 ? y : (y * lv) >> 15), -32767, 32767);
    }
    R->lp[k] = 0;
}

/* a pluck of the strings (a key, or RIPL ARP's step), shaped by RIPPLE 3: PICK, how hard (an offset on TONE for
 * the burst alone: below 0 a soft mallet, dark and round; above, a pick, bright and snappy); STRM, a strum (the
 * strings one after another, root first, STRM ms apart; 0 all at once); VARY, each pluck a little different (its
 * brightness up to VARY x 0.3 points either way, its level down to 1 - VARY / 2, each string's a little more). All
 * three at 0: the pluck as it always was. A new pluck drops a strum still under way (its strings not yet reached) */
static void HOT_CODE rs_pluck(reso_t *R, uint32_t nstr, int32_t root16, const int16_t *p, int32_t sc)
{
    int32_t pick = clamp(p[RP_TONE] + p[RP_PICK], 0, 100), vary = clamp(p[RP_VARY], 0, 100);
    int32_t lv = PCT_Q15[clamp(p[RP_PLVL], 0, 100)];    /* (PLVL: the pluck's level) */
    uint32_t k, gap = (uint32_t)clamp(p[RP_STRM], 0, 100) * 441u / 10u;   /* (ms -> samples) */
    if (!R->rng)
        R->rng = 0x51ED27;
    if (vary) {
        int32_t sw = vary * 3 / 10;
        pick = clamp(pick + (int32_t)(noise32(&R->rng) % (uint32_t)(2 * sw + 1)) - sw, 0, 100);
        lv -= (int32_t)((noise32(&R->rng) >> 17) * (uint32_t)vary / 200u);
        lv = lv < 0 ? 0 : lv;
    }
    R->proot = root16;
    R->pa = RS_TONE_A[pick];
    R->psc = sc;
    for (k = 0; k < RS_NB; k++) {
        R->pend[k] = 0;
        if (k >= nstr)
            continue;
        R->plv[k] = vary ? lv - (int32_t)((noise32(&R->rng) >> 17) * (uint32_t)vary / 400u) : lv;
        if (k && gap)
            R->pend[k] = gap * (rs_mode(R) == RM_BANK ? k / 3u : k);   /* (BANK: its bands three a string) */
        else
            rs_pluck1(R, k, root16, R->pa, R->plv[k], sc);
    }
}

/* audio ISR: a strum's strings, as their time comes (block by block: 0.7 ms) */
static void rs_strum(reso_t *R, uint32_t nstr, uint32_t n)
{
    uint32_t k;
    for (k = 0; k < RS_NB; k++)
        if (R->pend[k]) {
            if (R->pend[k] > n) {
                R->pend[k] -= n;
            } else {
                R->pend[k] = 0;
                if (k < nstr)
                    rs_pluck1(R, k, R->proot, R->pa, R->plv[k], R->psc);
            }
        }
}

/* audio ISR: one block of track t's RIPPLE arpeggio (RIPL ARP, the ARP pad's page 3: MODE ORDR RNGE RATE; a pluck
 * has no gate): keys, the white keys on that page over base. Returns 1 while a step's note sounds, the note in *note */
static volatile uint8_t reso_arp_clear_req[NTRK];   /* main loop -> ISR: forget RIPL ARP's notes (POLY held on it) */
static uint32_t HOT_CODE reso_arp_block(uint32_t t, uint32_t keys, uint32_t base, uint32_t n, uint32_t *note)
{
    const int16_t *p = tp[t].dev[DEV_RESO];
    int16_t a[NARP] = {0};
    if (reso_arp_clear_req[t]) {
        reso_arp_clear_req[t] = 0;
        reso_arp[t].n = 0;
    }
    a[AR_MODE] = p[RP_AMODE];
    a[AR_ORDR] = p[RP_AORD];
    a[AR_RNGE] = p[RP_ARNG];
    a[AR_RATE] = p[RP_ARAT];
    a[AR_GATE] = 50;
    return arp_run(&reso_arp[t], a, p[RP_AMODE] != ARM_OFF, keys, base, n, note, t + 4u) & 1u;
}

/* audio ISR: track t's RIPPLE on its stereo block, in place. keys: the white keys on the RIPPLE page (0
 * otherwise); pluck: they pluck (a TAPE track) or only retune (a SYNTH or POLY track, the keys playing the source) */
static uint8_t reso_arp_on[NTRK];   /* ISR: the arpeggio's note sounding last block (a new step: a pluck) */
/* Why a string's damping and its loop gain round toward 0 on a silent block (rs_settle): a right shift rounds down, so with g near 1
 * (DEC high) a string a little below 0 never got smaller, (lp x g) >> 15 giving lp back: below about
 * 32768 / (32768 - g) it stuck there for good. The damping's new value rounds toward 0 too (its step can't: a step of
 * -0.9 rounded toward 0 is none, and a string a little above 0 stalls instead). Four strings stuck that way left a constant offset of about 740 (-33 dB) in the track after it went quiet.
 * Unheard itself (the master takes DC away), but COLOR's NOIS follows its input's level, so it held the noise open:
 * a steady hiss with the transport stopped (2026-10-11, from playing it). Rounding toward 0 makes every string's
 * level shrink each time round, to 0. */
static void HOT_CODE reso_block(uint32_t t, int32_t *l, int32_t *r, uint32_t keys, int pluck, int32_t anote, uint32_t n)
{
    reso_t *R = &reso[t];
    const int16_t *p = TPD(t, DEV_RESO);
    int32_t wet1 = clamp(p[RP_WET], 0, 100), wet0 = R->wet, e[CTL], acc_e[CTL], acc_o[CTL], heard = 0;
    int32_t a = RS_TONE_A[clamp(p[RP_TONE], 0, 100)], sc = clamp(p[RP_SCAL], 0, RS_SCALES), root16;
    uint32_t k, i, press = keys & ~R->keys, nstr = R->nch < reso_cap ? R->nch : reso_cap, t60 = rs_t60(p[RP_DEC]);
    int32_t mode = rs_mode(R), drv = clamp(p[RP_DRV], 0, 100), ripl = clamp(p[RP_RIPL], 0, 100), ig = 16384;
    int32_t comb = mode == RM_COMB, gq = 256 + ((drv * 1311) >> 8);   /* (DRV's gain, 1 + DRV / 50: to 3 x) */
    int tune = p[RP_KEYS] == RK_TUNE;
    int32_t inq = 16384;                                /* (the input's share this block: COMB makes it up after) */                   /* (KEYS TUNE: a key or a step retunes, never plucks) */
    if (mode == RM_BANK)
        nstr *= 3u;                                     /* (BANK: its bands, three a string's chunk) */
    if (drv)
        ig = (int32_t)((1u << 22) / (uint32_t)gq);      /* (1 / gq, Q14) */
    if (mode != R->mode_seen) {                         /* MODE changed: the lines and the states cleared */
        R->mode_seen = (uint8_t)mode;
        for (k = 0; k < R->nch; k++)
            memset(rs_line(R, k), 0, RS_LEN * 2u);
        for (k = 0; k < RS_NB; k++) {
            R->lp[k] = R->bs1[k] = R->bs2[k] = 0;
            R->burst[k] = R->pend[k] = 0;
        }
        for (k = 0; k < RS_N; k++)
            R->dq[k] = 0;
    }
    R->keys = keys;
    R->wet = wet1;
    rs_strum(R, nstr, n);
    if (p[RP_PTCH] != R->ptch_seen)                     /* PTCH turned: it takes over from a key */
        R->root16 = 0;
    for (k = 0; press && k < NWHITE; k++)                /* a key: the root (the lowest new one) */
        if ((press >> k) & 1u) {
            R->root16 = clamp(12 * ((int32_t)track[t].octave + 1) + (int32_t)k, 29, 108) * 16;
            R->ptch_seen = p[RP_PTCH];
            if (pluck && !tune && nstr)
                rs_pluck(R, nstr, R->root16, p, sc);
            break;
        }
    if (anote >= 0) {                                    /* its arpeggio's step: that note the root, plucked */
        R->root16 = clamp(anote, 29, 108) * 16;
        R->ptch_seen = p[RP_PTCH];
        if (nstr && !tune)
            rs_pluck(R, nstr, R->root16, p, sc);
    }
    if (!nstr || (!wet0 && !wet1)) {                     /* off: the track as it was */
        R->dry = 32768;
        R->rs16 = 0;
        for (k = 0; k < RS_NB; k++)
            R->lvl[k] = 0;
        for (k = 0; k < RS_N; k++)
            R->dq[k] = 0;
        return;
    }
    root16 = reso_root16(t);
    if (!R->rs16 || R->root16 > 0)                      /* a key's root lands at once (a pluck is a new note); */
        R->rs16 = root16;                               /* PTCH, turned or modulated, glides: half the way a block */
    else
        R->rs16 += sdiv2(root16 - R->rs16, 1);
    root16 = R->rs16;
    {   /* what excites the strings: the track, mono, through the filter in front (when it does anything) */
        int32_t slope = clamp(p[RP_SLOP], 0, 2);
        tsvf_t flt = {0};                                 /* (set when filt; zero keeps the compiler's warning off) */
        int32_t c8 = mod_fine8(t, FN_RCUT, p[RP_CUT]);   /* (CUT finer than its semitones when modulated: mod.c) */
        int filt = c8 < 127 << 8 || p[RP_RES] || slope;
        /* the input's share, by the root string's ring: less as the ring gets longer, so a long DEC rings louder but
         * not without bound (the loop clamps besides); applied as the input is read, not in a pass of its own */
        int32_t in = mode >= RM_RESO ? 16384              /* (the resonators scale their own input by their loss) */
                   : clamp((32768 - rs_gain(rs_period_q8(root16) >> 8, t60)) * 16, 4096, 16384);
        inq = in;
        int32_t ic1 = R->ic1, ic2 = R->ic2, any = 0;    /* (the filter's state in locals across the block) */
        if (filt) {
            for (i = 0; i < n; i++)
                any |= l[i] | r[i];
            if (any || ic1 || ic2)
                tsvf_coef(&flt, c8, PCT_127[clamp(p[RP_RES], 0, 100)]);
            else
                filt = 0;                               /* silence into a filter at rest: silence out of it, the
                                                         * state still 0 (what running it gives; resting tracks) */
        }
        for (i = 0; i < n; i++) {
            int32_t x = (l[i] + r[i]) >> 1;
            heard |= x;
            e[i] = ((filt ? tsvf_mode(&flt, x >> 1, &ic1, &ic2, (uint32_t)slope) << 1 : x) * in) >> 15;
            acc_e[i] = acc_o[i] = 0;
        }
        if (mode >= RM_RESO && drv)                     /* (RESO, BANK: DRV on what reaches the bands) */
            for (i = 0; i < n; i++)
                e[i] = rs_shape(e[i], gq, ig);
        R->ic1 = ic1;
        R->ic2 = ic2;
        if (!heard && R->ic1 > -256 && R->ic1 < 256 && R->ic2 > -256 && R->ic2 < 256)
            R->ic1 = R->ic2 = 0;                        /* (silence in: the filter's integer residue let go. With RES
                                                         * up it held a few dozen for good, and DEC 100's strings rang
                                                         * it up to -16 dB with nothing playing: rs_settle) */
    }
    if (!heard && R->quiet > RS_LEN + CTL)              /* resting: silence in, and the strings have rung down to
                                                         * next to nothing (|out| <= 2) for longer than a line is
                                                         * long: nothing left to ring, so none of their work (2026-10-11:
                                                         * half the CPU with nothing playing). A pluck or sound in
                                                         * wakes them, carrying on from their lines as they are */
        nstr = 0;
    {   /* each string's cents off its partial: RIPL, a slow triangle of up to +-50 cents at 100, 0.2 Hz + RIPL / 100
         * Hz, a quarter turn a string, so the drift runs across the strings like a wave (the lines follow sample by
         * sample, the moving loop); and SPRD, on the strings and combs, a detune across them, -50 .. +50 cents from
         * the lowest to the highest at 100 (a chorus: the resonators take SPRD as their spacing instead, below) */
        int32_t sprd = mode < RM_RESO ? clamp(p[RP_SPRD], -100, 100) : 0, rp = ripl && nstr;
        if (rp)
            R->rph += 623306u + (uint32_t)ripl * 31165u;   /* (2^32 x 32 / 44100 a Hz) */
        for (k = 0; k < RS_N && (rp || sprd || R->rc[k]); k++) {   /* (both 0: the cents stay 0, no work) */
            int32_t tri = (int32_t)((R->rph + (uint32_t)k * 0x40000000u) >> 16), c = 0;
            tri = tri < 32768 ? tri * 2 - 32768 : 98304 - tri * 2;
            if (rp)
                c = (tri * ripl) >> 16;
            R->rc[k] = c + (((2 * (int32_t)k - 3) * sprd * 43) >> 8);   /* (/ 6, near enough, no divide) */
        }
    }
    if (mode >= RM_RESO) {                               /* the resonators: each partial a band (below), DEC its
                                                         * ring (the same ms as a string's), TONE a tilt over the
                                                         * bands (the picture's), a pluck a burst of noise into it.
                                                         * RESO: four bands; BANK: twelve. Both every sample
                                                         * (2026-10-14: BANK ran at half rate with its output stepped
                                                         * between, images of the top bands up to 13 kHz: "glitches
                                                         * when played through") */
        uint32_t tone = (uint32_t)clamp(p[RP_TONE], 0, 100), rt;
        int32_t gb = rs_gain(CTL, t60), dq = 32768 - gb, sq;   /* (the ring's loss a block, Q15; a sample's is
                                                                * dq / 2^20) */
        int32_t tstep = (100 - (int32_t)tone) * (mode == RM_BANK ? 3 : 9);
        int32_t sprd = clamp(p[RP_SPRD], -100, 100), even = clamp(p[RP_EVEN], -100, 100);
        for (rt = 0, i = 1u << 7; i; i >>= 1)           /* (the square root of the loss: the input's scale) */
            if ((rt | i) * (rt | i) <= (uint32_t)dq)
                rt |= i;
        sq = 12 * (int32_t)rt;
        for (k = 0; k < nstr; k++) {
            int32_t part = rs_part16(sc, k), rc = R->rc[mode == RM_BANK ? k / 3u : k];
            int32_t n16 = root16 + part + ((part * sprd) >> 8);   /* (SPRD: the spacing stretched, up to -40 % or +40 %
                                                                   * from the root: an inharmonic set) */
            int32_t tilt = 1000 - (int32_t)k * tstep;
            uint32_t inc, burst = R->burst[k];
            int32_t th, th2, eps, eq, sh = 14, s1 = R->bs1[k], s2 = R->bs2[k], by = R->by[k], last = 0, gain, blv;
            int32_t *acc = k & 1u ? acc_o : acc_e;
            if (tilt <= 0 || n16 > RS_BANK_TOP) {        /* (tilted away, or past the top: silent) */
                R->bs1[k] = R->bs2[k] = R->lp[k] = 0;
                R->burst[k] = 0;
                R->lvl[k] = 0;
                continue;
            }
            inc = rc ? cents_inc(clamp(n16, 29 * 16, 2047), rc, 0) : pitch_inc((uint32_t)clamp(n16, 29 * 16, 2047));
            th = (int32_t)((inc >> 16) * 1608u) + (int32_t)((((inc >> 8) & 255u) * 1608u) >> 8);   /* 2 pi f / fs,
                                                                                                   * Q24 */
            th2 = (th >> 12) * (th >> 12);              /* (th^2, then th^3: Q24, under 2^25) */
            th2 = (th2 >> 12) * (th >> 12);
            eps = th - (((th2 >> 8) * 2731) >> 8);     /* 2 sin(th/2) = th - th^3 / 24 (2731 / 2^16; th^3 x 2731
                                                         * whole overflowed, 2026-10-14: found by the sanitizer
                                                         * when two machines' goldens disagreed) */
            while ((eps >> (14 - sh)) >= 32768 && sh)   /* (eps in 15 bits: the shift takes the rest) */
                sh--;
            eq = eps >> (14 - sh);
            gain = tilt << 5;                           /* (Q15: TONE's tilt, 1.0 at the top) */
            gain = gain * (100 + (k & 1u ? even : -even)) / 100;   /* (EVEN: the 2nd 4th 6th.. partials against the
                                                                    * 1st 3rd 5th..; once a band a block) */
            blv = R->blv[k];
            for (i = 0; i < n; i++) {
                int32_t x = e[i];
                if (burst) {                            /* (a period of noise: the resonator stores it) */
                    int32_t nz = (int32_t)(noise32(&R->rng) >> 17) - 16384;
                    by += (R->pa * (nz - by)) >> 15;
                    s1 += (by * blv) >> 8;
                    burst--;
                }
                /* the resonator: a rotation of (s1, s2) by th a sample, the "magic circle" (area-preserving: the
                 * second step uses the first's result), the states Q23 so the small steps keep their precision.
                 * The track goes in scaled by the square root of the loss, so a tone held at a band's pitch builds
                 * up about 3 x at DEC 60 as heard (1 x at 0, 8 x at 100) and noise keeps one level whatever the ring (it built
                 * up 2^20 / dq, thousands of times, into a hard clamp: the glitch of 2026-10-14); the loss comes
                 * off one state a sample (the rotation shares it out: twice a block's, halved). Products stay
                 * under 2^31: eq < 2^15, the state read 10 bits down, |s1| clamped at 2^26 (a pluck's burst on a
                 * tone could pass it) */
                s1 += ((s2 >> 10) * eq) >> sh;
                s2 -= ((s1 >> 10) * eq) >> sh;
                s1 += (x * sq) >> 8;
                s1 -= ((s1 >> 10) * dq) >> 9;
                s1 = s1 > (1 << 26) ? 1 << 26 : s1 < -(1 << 26) ? -(1 << 26) : s1;
                last = ((s2 >> 12) * gain) >> 7;       /* (x 16, +24 dB: a band's a sliver of a broad sound, so drums
                                                         * through the bank came out ~28 dB under themselves, 2026-10-15:
                                                         * "filter band doesn't do much"; now about their own level, a
                                                         * held tone louder, the mix's soft knee its limit) */
                acc[i] += last;
            }
            R->bs1[k] = s1;
            R->bs2[k] = s2;
            R->by[k] = by;
            R->burst[k] = burst;
            R->lp[k] = last;
            {                                           /* (its level for the screen, as a string's) */
                uint32_t lv = (uint32_t)(last < 0 ? -last : last), l0 = R->lvl[k];
                l0 -= (l0 + 7u) >> 3;                   /* (to 0: l0 >> 3 alone stalled under 8) */
                R->lvl[k] = (uint16_t)(lv > l0 ? (lv > 65535u ? 65535u : lv) : l0);
            }
        }
        for (k = nstr; k < RS_NB; k++)                   /* (bands shed: quiet, their meters down) */
            R->lvl[k] = 0;
        for (k = 0; k < RS_N; k++)
            R->dq[k] = 0;
    } else {
    uint32_t comp = ((uint32_t)(32768 - a) << 8) / (uint32_t)a;   /* (the same for every string: once a block) */
    int settle = !heard;                                 /* (silence in: the moving loop rounds toward 0 as the
                                                          * settling one does; RIPL keeps the delay moving) */
    for (k = 0; k < nstr; k++) {                         /* each string: read a period back, damp, ring, write */
        int16_t *d = rs_line(R, k);
        int32_t n16 = root16 + rs_part16(sc, k);
        uint32_t pq8 = (R->rc[k] ? rs_period_q8c(n16, R->rc[k]) : rs_period_q8(n16)) >> comb;
        uint32_t dq8 = pq8 > comp + 512u ? pq8 - comp : 512u, di = dq8 >> 8, df = dq8 & 255u, w = R->w;
        int32_t g = rs_gain(pq8 >> 8, t60), lp = R->lp[k], *acc = k & 1u ? acc_o : acc_e, g3 = (g * ig) >> 14;
        uint32_t rd = w >= di ? w - di : w + RS_LEN - di, d0 = R->dq[k] ? R->dq[k] : dq8;
        int32_t dd = (int32_t)dq8 - (int32_t)d0;
        g3 = (g3 * 10923) >> 15;                        /* (over 3: the cubic's) */
        if (comb) {
            g = -g;
            g3 = -g3;
        }
        R->dq[k] = dq8;
        if (dd) {                                       /* the delay moving: each sample's own read point */
            int32_t cq = (int32_t)d0 << CTL_LOG2;       /* (n is CTL) */
            for (i = 0; i < n; i++) {
                uint32_t cur, rp;
                int32_t x, y, z;
                cq += dd;
                cur = (uint32_t)(cq >> CTL_LOG2);
                di = cur >> 8;
                rd = w >= di ? w - di : w + RS_LEN - di;
                rp = rd ? rd - 1u : RS_LEN - 1u;
                x = d[rd] + (((d[rp] - d[rd]) * (int32_t)(cur & 255u)) >> 8);
                z = a * (x - lp);
                lp += z >> 15;
                if (settle)
                    lp += (lp >> 31) & (int32_t)((uint32_t)-(z & 32767) >> 31);
                y = rs_fb(lp, g, g3, drv, gq, settle);
                z = y + e[i];
                d[w] = (int16_t)(z > 32767 ? 32767 : z < -32767 ? -32767 : z);
                acc[i] += y;
                if (++w == RS_LEN)
                    w = 0;
            }
        } else if (heard) {                             /* the usual loop, in wrap-free runs: the two wrap tests a
                                                         * sample leave the loop, and the sample read a step back is
                                                         * the one read last time (rd moves by one a sample, and the
                                                         * write head is at least two ahead of it, so that cell is
                                                         * as it was) */
            int32_t prev = d[rd ? rd - 1u : RS_LEN - 1u];
            i = 0;
            while (i < n) {
                uint32_t run = n - i, j;
                if (run > RS_LEN - w)
                    run = RS_LEN - w;
                if (run > RS_LEN - rd)
                    run = RS_LEN - rd;
                for (j = 0; j < run; j++, i++) {
                    int32_t cur = d[rd], x = cur + (((prev - cur) * (int32_t)df) >> 8), y, z;
                    lp += (a * (x - lp)) >> 15;
                    y = rs_fb(lp, g, g3, drv, gq, 0);
                    z = y + e[i];
                    d[w] = (int16_t)(z > 32767 ? 32767 : z < -32767 ? -32767 : z);
                    acc[i] += y;
                    prev = cur;
                    w++;
                    rd++;
                }
                if (w == RS_LEN)
                    w = 0;
                if (rd == RS_LEN)
                    rd = 0;
            }
        } else {                                        /* silence in: the same, rounding toward 0 (rs_settle), so
                                                         * the strings die away to 0 and not to an offset (a block of
                                                         * sound hides the round-down; this costs about 8 more a string
                                                         * a sample, so only here) */
            for (i = 0; i < n; i++) {
                uint32_t rp = rd ? rd - 1u : RS_LEN - 1u;
                int32_t x = d[rd] + (((d[rp] - d[rd]) * (int32_t)df) >> 8), y, z;
                z = a * (x - lp);
                lp += z >> 15;
                lp += (lp >> 31) & (int32_t)((uint32_t)-(z & 32767) >> 31);
                y = rs_fb(lp, g, g3, drv, gq, 1);
                z = y + e[i];
                d[w] = (int16_t)(z > 32767 ? 32767 : z < -32767 ? -32767 : z);
                acc[i] += y;
                if (++w == RS_LEN)
                    w = 0;
                if (++rd == RS_LEN)
                    rd = 0;
            }
        }
        R->lp[k] = lp;
        {                                               /* (its level for the screen: a block's last sample, held) */
            uint32_t a = (uint32_t)(lp < 0 ? -lp : lp), l0 = R->lvl[k];
            l0 -= (l0 + 7u) >> 3;                   /* (to 0: l0 >> 3 alone stalled under 8) */
            R->lvl[k] = (uint16_t)(a > l0 ? (a > 65535u ? 65535u : a) : l0);
        }
    }
    if (comb && nstr) {                                  /* COMB: its output made up for the input's share (the
                                                          * longer FDBK, the less went in: it came out 4 dB under at
                                                          * 40, 12.5 under at 100, quieter the more it rang) */
        int32_t mk = (int32_t)(268435456u / (uint32_t)inq);   /* (Q14: 1 .. 4) */
        for (i = 0; i < n; i++) {
            acc_e[i] = (acc_e[i] * mk) >> 14;
            acc_o[i] = (acc_o[i] * mk) >> 14;
        }
    }
    }
    if (nstr) {
        R->w += n;                                       /* (w < RS_LEN, n a block: one wrap at most) */
        if (R->w >= RS_LEN)
            R->w -= RS_LEN;
        for (i = 0; i < n && !heard; i++)
            if (acc_e[i] > 2 || acc_e[i] < -2 || acc_o[i] > 2 || acc_o[i] < -2)
                heard = 1;
        if (heard)
            R->quiet = 0;
        else if ((R->quiet += n) > RS_LEN + CTL)
            for (k = 0; k < RS_NB; k++)
                R->lp[k] = R->bs1[k] = R->bs2[k] = 0;
    } else {
        for (k = 0; k < RS_NB; k++)                     /* (resting, or off: nothing ringing) */
            R->lvl[k] = 0;
    }
    {   /* WET: the strings against the track, ramped across the block (Q15: no division a sample). INS crossfades;
         * SEND keeps the track at full and adds the strings (ROUT: the track's share ramped when it's switched) */
        int32_t w0 = PCT_Q15[wet0], dw = wet1 == wet0 ? 0 : ((wet1 - wet0) * 32767 / 100) >> CTL_LOG2;
        int32_t d1 = p[RP_ROUT] ? 32767 : 32767 - PCT_Q15[wet1], d0 = R->dry ? R->dry - 1 : d1;
        R->dry = d1 + 1;
        if (p[RP_ROUT] || d0 != 32767 - PCT_Q15[wet0]) {   /* a send, or switching: the two shares apart */
            int32_t dd = (d1 - d0) >> CTL_LOG2;
            for (i = 0; i < n; i++, w0 += dw, d0 += dd) {
                int32_t wl = soft_knee(((acc_e[i] * 3) >> 2) + ((acc_o[i] * 3) >> 3), 24000);
                int32_t wr = soft_knee(((acc_o[i] * 3) >> 2) + ((acc_e[i] * 3) >> 3), 24000);
                l[i] = (int32_t)(((l[i] >> 1) * (d0 >> 1)) >> 13) + (int32_t)(((wl >> 1) * (w0 >> 1)) >> 13);
                r[i] = (int32_t)(((r[i] >> 1) * (d0 >> 1)) >> 13) + (int32_t)(((wr >> 1) * (w0 >> 1)) >> 13);
            }
            return;
        }
        for (i = 0; i < n; i++, w0 += dw) {
            int32_t wl = soft_knee(((acc_e[i] * 3) >> 2) + ((acc_o[i] * 3) >> 3), 24000);   /* (strings 1 and 3 2 : 1
                                                                                             * left, 2 and 4 right) */
            int32_t wr = soft_knee(((acc_o[i] * 3) >> 2) + ((acc_e[i] * 3) >> 3), 24000);
            l[i] += (int32_t)(((wl - l[i]) >> 1) * (w0 >> 1) >> 13);   /* (halves: |a - b| < 2^17, w0 < 2^15) */
            r[i] += (int32_t)(((wr - r[i]) >> 1) * (w0 >> 1) >> 13);
        }
    }
}

/* audio ISR: shedding takes strings after grains (one at a time, down to two); a second without it gives one back */
static void reso_shed(void)
{
    if (reso_cap > 2u)
        reso_cap--;
    reso_calm = 0;
}
static void HOT_CODE reso_recover(void)
{
    if (++reso_calm >= 1378u && reso_cap < RS_N) {     /* (a string back, if there's room: chain_room) */
        if (chain_room())
            reso_cap++;
        reso_calm = 0;
    }
}

/* main loop, every pass: a track's strings while its RIPPLE is on (WET above 0, the track in use), taken from the
 * shared memory (from the end of the longest tape when nothing is free); none otherwise */
static void reso_poll(void)
{
    uint32_t t;
    for (t = 0; t < NTRK; t++) {
        reso_t *R = &reso[t];
        int on = trk_live(t) && mod_peak_dev(t, DEV_RESO, RP_WET) > 0;   /* (WET modulated up: on) */
        while (on && R->nch < RS_N) {
            int32_t c = tape_alloc(MEM_RESO + t, NTRK, 1);
            if (c < 0)
                break;
            R->map[R->nch] = (uint8_t)c;
            RING_PUBLISH();
            R->nch++;
        }
        while (!on && R->nch) {
            uint32_t c = R->map[R->nch - 1u];
            R->nch--;
            mem_free(c);
        }
    }
}
