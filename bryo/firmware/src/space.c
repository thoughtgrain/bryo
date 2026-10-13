/* SPDX-License-Identifier: GPL-3.0-only */
/* Bryo: SPACE (docs/bryo-architecture.md, "SPACE, as built"): each track's delay, and two rooms the tracks share, the
 * S-4's VAST split in two: a delay that belongs to the track, a reverb that belongs to the mix. All of it at half
 * the rate (22.05 kHz) or below.
 *
 * The delay, a track's own: TIME (10..370 ms) back, gliding when TIME turns (a tape echo's pitch bend, no clicks);
 * FDBK what each echo feeds back (100: nearly forever); TONE in the feedback (- a low-pass, + a high-pass, so each
 * echo is darker or thinner than the last); DLY its level (100: the first echo at 0.8 of the sound); SPRD the right
 * echo up to 10 ms ahead of the left (0: mono).
 *
 * The rooms (2026-10-11, from playing it: four rooms were the default project's largest cost, and "let me configure
 * the quality of rooms, and drop from 4 rooms to 2 ... share rooms with tracks"): A and B, each Felucca's ROOM (four
 * damped combs and two pairs of allpasses, Freeverb's shape) with its own SIZE (the combs' lengths, 15..46 ms), DEC
 * (the time to fall 60 dB, 0.2..4.2 s), TONE (- darker combs, + a high-pass in front), PRE (a pre-delay, 0..200 ms)
 * and QUAL: HI runs it at 22.05 kHz, LO at 11.025 kHz (half the work, a darker tail). Their knobs are the mixer's
 * ROOMS pages (rmp[], stored with the project, not modulation targets: no track owns them). A track's ROOM picks A or
 * B and its VERB is how much it sends there: its sound after COLOR, mono, times VERB and the track's level (so a
 * track turned down or muted leaves its room too). The rooms come back after the tracks, stereo, into the master.
 *
 * Memory, from the shared chunks (mem.c), only while used: a track's delay line while DLY is above 0 (8 chunks,
 * 8,192 samples); a room (a chunk per comb, the allpasses in a fifth) and its pre-delay (5 chunks) while a track in
 * use sends to it. A line is 16-bit when there's room for it with 3 s still free; when memory is short it starts
 * 8-bit (companded like A-law: half the chunks, the noise riding under the sound), and when nothing is free it takes
 * from the end of the longest tape. A line never changes format while it sounds. The rooms' own buffers are always
 * 16-bit (a feedback loop would grow 8-bit's noise).
 *
 * Audio ISR, except space_poll (the main loop: the memory). Integer only. */

#define SP_CH 1024u                  /* 16-bit samples a line keeps in a chunk (of its 1,056: a power of two) */
#define SP_DLY16 8u                  /* the delay's line, chunks: 8,192 samples, TIME's 370 ms at 22.05 kHz */
#define SP_DLY8 4u                   /* .. 8-bit */
#define SP_PRE16 5u                  /* the pre-delay's line: 5,120 samples, PRE's 200 ms */
#define SP_PRE8 3u                   /* .. 8-bit (6,144) */
#define SP_REV 5u                    /* a room: a comb in each of four chunks, the four allpasses in the fifth */
#define SP_KEEP 16u                  /* chunks (3 s) a 16-bit line leaves free, or it starts 8-bit */
#define SP_LMAX 8u
/* (3 and 8: the track's own SIZE and PRE until the rooms were shared; 3 and 8-11 the room's knobs now: SP_RMK) */
enum { SP_TIME, SP_FDBK, SP_ROOM, SP_X3, SP_DLY, SP_VERB, SP_TONE, SP_SPRD, SP_X8, SP_SYNC = 12, SP_AGE, SP_DIFF,
       SP_DUCK };

/* the combs' longest (SIZE 100), and the allpasses (left pair, right pair), samples at 22.05 kHz: Freeverb's
 * lengths, the combs half again as long for a room rather than a box */
static const uint16_t SP_COMB[4] = {837, 891, 958, 1017};
static const uint16_t SP_AP[4] = {278, 220, 290, 232};
static const uint16_t SP_APO[4] = {0, 278, 498, 788};   /* where each allpass sits in the fifth chunk */
/* TYPE (2026-10-13): the room's tunings, by ROOM PLAT HALL SPRG. The combs' lengths (Q8, of SIZE's), their ring
 * (Q8, of DEC's T60), the allpasses' gain (Q15; 0: the room's 1/2, as it always was) and the combs' damping (Q8).
 * PLAT dense and bright, HALL long and dark, SPRG short, bright, ringing. ROOM is the room as it was, bit for bit */
static const uint16_t SP_TY_LEN[4] = {256, 159, 256, 90}, SP_TY_T60[4] = {256, 320, 461, 230};
static const uint16_t SP_TY_APG[4] = {0, 22938, 19661, 24576}, SP_TY_DAMP[4] = {256, 128, 410, 102};
/* MOD: each comb's own slow LFO (0.37 0.53 0.71 0.89 Hz), phase steps a sample at 22.05 kHz */
static const uint32_t SP_MOD_INC[4] = {72071u, 103236u, 138302u, 173367u};

typedef struct {                     /* a line in chunks of the shared memory */
    uint8_t map[SP_LMAX];            /* (the main loop's) */
    uint8_t nch;
    uint8_t bits8;                   /* 8-bit companded (set before ready, never while it's ready) */
    volatile uint8_t ready;          /* every chunk is in: the ISR may use it */
    volatile uint8_t gen;            /* counts each time it's handed over (the ISR starts it from silence) */
} sp_line_t;

typedef struct {                     /* a track's SPACE */
    sp_line_t dly;                   /* the main loop's (ready: published to the ISR) */
    /* the ISR's */
    uint8_t dlive;                   /* the delay running (its state reset when it starts) */
    uint8_t dgen;                    /* the line's gen it started on */
    uint32_t dw;                     /* the line's write index */
    int32_t dq;                      /* the delay's distance, Q8 samples (glides toward TIME) */
    int32_t dlp;                     /* TONE's filter in the feedback */
    int32_t gd, gv;                  /* DLY's gain and the send's (VERB x the track's level) last block, Q15 */
    int32_t pl, pr;                  /* the delay's last half-rate sample, each side (the step back up to 44.1 kHz) */
    uint32_t quiet;                  /* half-rate samples in a row of silence in and next to none out (sp_quiet) */
    int16_t mdly, mfb;               /* DLY and FDBK as their gains were last worked out (0 at power-on: their gains */
    int32_t mgd, mfbg;               /* .. 0, right for 0), and the gains: a divide each only when the knob moves */
    /* the echoes' character (sp_delay; all at rest at 0) */
    uint32_t wph, fph, dph;          /* AGE: the wow's, the flutter's and the drift's phases */
    int32_t wob;                     /* .. the read point's offset last block, Q8 samples (each block ramps from it) */
    int32_t alp;                     /* .. its low-pass in the feedback */
    int32_t dpk, denv;               /* DUCK: the input's peak last block, and its envelope */
    uint16_t dj[2];                  /* DIFF: the two allpasses' indices */
    int32_t ages, difs;              /* AGE (Q8) and DIFF's mix (Q15) as they glide to their knobs: turned, they move
                                      * ~70 ms and ~50 ms end to end, not in one block (a jump was a glitch) */
    uint8_t dfdirty;                 /* DIFF's lines hold sound (cleared once it's back at 0) */
} space_t;
#define SP_DF0 113u                  /* DIFF's allpasses, samples at 22.05 kHz (5.1 and 9.0 ms: prime, apart) */
#define SP_DF1 199u
static int16_t sp_dfa[NTRK][SP_DF0 + SP_DF1];   /* (main RAM: 2.5 KB) */

typedef struct {                     /* a room, A or B */
    sp_line_t pre, rev;              /* the main loop's */
    volatile uint8_t users;          /* main loop -> screen: the tracks sending to it (a bit each) */
    /* the ISR's */
    uint8_t live, gen;               /* running (its state reset when it starts), on the lines' gen */
    uint8_t qual;                    /* the QUAL it runs at now */
    uint32_t pw;                     /* the pre-delay's write index */
    uint16_t ci[4], ai[4];           /* the combs' and allpasses' indices */
    uint16_t clen[4], alen[4];       /* their lengths now (SIZE, QUAL) */
    uint8_t type_seen;               /* the TYPE clen, cg and apg were set for */
    int32_t tyg;                     /* the output's gain, Q15: a new SIZE, QUAL or TYPE fades it out, retunes (its
                                      * lines cleared), fades it in, once the knob has stood still ~18 ms */
    uint32_t pk;                     /* .. what SIZE QUAL TYPE asked for last block */
    uint8_t pcnt;                    /* .. blocks it's stood still */
    int32_t ing;                     /* .. the combs' input after a retune, Q15, rising from 0 (~23 ms): from cleared
                                      * lines they build up, their first echoes not arriving mid-waveform */
    int32_t mds;                     /* MOD's depth as it glides to the knob (Q8 samples) */
    int32_t apg;                     /* the allpasses' gain, Q15 (0: 1/2, ROOM's own form) */
    uint32_t mph[4];                 /* MOD: each comb's LFO phase */
    int32_t clp[4], cg[4];           /* the combs' damping filters, their loop gains (DEC), Q15 */
    int16_t size_seen, dec_seen;     /* what clen and cg were set for (-1: nothing yet) */
    int32_t rhp;                     /* TONE's high-pass in front */
    int32_t in[CTL / 2u];            /* this block's sends, mono at 22.05 kHz (space_block adds, space_rooms takes) */
    int32_t pl, pr;                  /* its last output sample, each side (the step back up to 44.1 kHz) */
    uint32_t quiet;                  /* samples (at its rate) of silence in and next to none out (sp_quiet) */
} room_t;

static space_t space[NTRK];
static room_t room[NROOM];

/* SPACE resting (2026-10-11, from playing it: half the CPU with nothing playing). With silence going in and next to
 * nothing coming out (|wet| <= 16 both sides, -66 dB: a room's combs, in integers, cycle at about that for good
 * once the tail is gone, a faint sound of their own) for longer than its lines are long (the delay's; a room's
 * pre-delay plus its longest comb and allpasses), everything in them has gone round and come out as next to
 * nothing: it stops working until sound comes in again, where it carries on from its lines as they are. The
 * filters in its loops are let go to 0 as it rests (a residue under the threshold can't hold it awake). */
#define SP_QUIET_OUT 16
static inline __attribute__((always_inline)) int32_t sp_loud(int32_t x) { return (uint32_t)(x + SP_QUIET_OUT) > 2u * SP_QUIET_OUT; }   /* |x| > 16 */

/* SYNC: TIME as the tempo's divisions (96ths of a beat): 1/64 1/32 16T 1/16 8T 16D 1/8 4T 8D 1/4 4D 1/2. One the line
 * can't hold at the tempo (370 ms) plays halved until it fits (1/4 at 120 BPM: 1/8), so it stays on the grid */
static const uint8_t SP_SDIV[12] = {6, 12, 16, 24, 32, 36, 48, 64, 72, 96, 144, 192};
static uint32_t sp_sync_d96(int32_t time)
{
    uint32_t d = SP_SDIV[(uint32_t)(clamp(time, 10, 370) - 10) * 12u / 361u], bpm = (uint32_t)clamp(sys.bpm, 40, 240);
    while (d > 3u && d * 3528000u / bpm > 370u * 5645u)
        d >>= 1;
    return d;
}
/* .. a division's name (the strip's TIME under SYNC) */
static const char *sp_div_name(uint32_t d96)
{
    static const uint8_t D[16] = {3, 6, 8, 9, 12, 16, 18, 24, 32, 36, 48, 64, 72, 96, 144, 192};
    static const char *const N[16] = {"1/128", "1/64", "32T", "64D", "1/32", "16T", "32D", "1/16", "8T", "16D", "1/8",
                                      "4T", "8D", "1/4", "4D", "1/2"};
    uint32_t i;
    for (i = 0; i < 16u && D[i] != d96; i++)
        ;
    return i < 16u ? N[i] : "?";
}

/* .. the delay it gives, Q8 samples at 22.05 kHz (a beat at 1 BPM is 1,323,000 samples: x 256 / 96 = 3,528,000) */
static int32_t sp_sync_q8(int32_t time)
{
    return (int32_t)(sp_sync_d96(time) * 3528000u / (uint32_t)clamp(sys.bpm, 40, 240));
}

/* ------------------------------------------------- 8-bit companding --- */
/* sign, a 3-bit segment, a 4-bit step inside it: 16 steps of 16 below 256, then each segment twice the last up to
 * 32,767 (A-law's shape). The segment of |x| >> 8: */
static const uint8_t SP_SEG[128] = {
    0, 1, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
    6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
    7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
    7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7};

static inline uint8_t sp_enc8(int32_t x)
{
    uint32_t a = (uint32_t)(x < 0 ? -x : x), s, m;
    if (a > 32767u)
        a = 32767u;
    s = SP_SEG[a >> 8];
    m = s ? (a >> (s + 3u)) & 15u : a >> 4;
    return (uint8_t)((x < 0 ? 128u : 0u) | (s << 4) | m);
}

static inline int32_t sp_dec8(uint8_t c)
{
    uint32_t s = (c >> 4) & 7u, m = c & 15u;
    int32_t a = s ? (int32_t)(((16u + m) << (s + 3u)) + (1u << (s + 2u))) : (int32_t)(m << 4);
    return c & 128u ? -a : a;
}

/* a * b, Q15, toward zero: the loops here run for seconds, and a shift's floor would keep them ringing on a small
 * DC of their own after the sound has gone (-36 at the output, measured; rounding to nearest still left a limit
 * cycle of 28). Cutting the magnitude only ever takes energy out, so a loop with nothing in it falls to 0. */
static inline __attribute__((always_inline)) int32_t sp_mul(int32_t a, int32_t b) { return sdiv2(a * b, 15); }   /* (toward zero: the tails settle) */

/* ----------------------------------------------------------- lines --- */
/* a line's chunks as pointers, for a block */
static inline __attribute__((always_inline)) void sp_bases(const sp_line_t *ln, uint8_t **b)
{
    uint32_t k;
    for (k = 0; k < ln->nch; k++)
        b[k] = (uint8_t *)(void *)mem_at(ln->map[k]);
}

static inline __attribute__((always_inline)) uint32_t sp_len(const sp_line_t *ln) { return (uint32_t)ln->nch * SP_CH * (ln->bits8 ? 2u : 1u); }

static inline int32_t sp_rd(uint8_t *const *b, int bits8, uint32_t i)
{
    if (bits8)
        return sp_dec8(b[i >> 11][i & 2047u]);
    return ((const int16_t *)(void *)b[i >> 10])[i & 1023u];
}

static inline void sp_wr(uint8_t *const *b, int bits8, uint32_t i, int32_t x)
{
    if (bits8)
        b[i >> 11][i & 2047u] = sp_enc8(x);
    else
        ((int16_t *)(void *)b[i >> 10])[i & 1023u] = (int16_t)clamp(x, -32767, 32767);
}

/* main loop: line ln for owner o holds n16 chunks at 16 bits, or n8 at 8 bits when memory is short (n8 0: 16-bit
 * only); returns 1 once it's all in and handed to the ISR. When nothing more can be had, a 16-bit line holding
 * enough for 8 bits becomes one (its chunks are silent either way: a zero sample is a zero code). */
static int sp_line_fill(sp_line_t *ln, uint32_t o, uint32_t n16, uint32_t n8)
{
    uint32_t need;
    if (ln->ready)
        return 1;
    if (!ln->nch)
        ln->bits8 = n8 && mem_count(MEM_FREE) < n16 + SP_KEEP;
    need = ln->bits8 ? n8 : n16;
    while (ln->nch < need) {
        int32_t c = tape_alloc(o, NTRK, 1);
        if (c < 0)
            break;
        ln->map[ln->nch++] = (uint8_t)c;
    }
    if (ln->nch < need && !ln->bits8 && n8 && ln->nch >= n8) {
        while (ln->nch > n8)
            mem_free(ln->map[--ln->nch]);
        ln->bits8 = 1;
        need = n8;
    }
    if (ln->nch < need)
        return 0;
    ln->gen++;
    RING_PUBLISH();                                     /* (the map before the flag that hands it to the ISR) */
    ln->ready = 1;
    return 1;
}

/* main loop: the ISR stops using line ln (from its next block), then its chunks go back */
static void sp_line_free(sp_line_t *ln)
{
    ln->ready = 0;
    RING_PUBLISH();
    while (ln->nch)
        mem_free(ln->map[--ln->nch]);
}

/* main loop, every pass: each track's delay line while its DLY is up (the track in use); each room's lines while a
 * track in use sends to it (VERB up, its ROOM that room) */
static void space_poll(void)
{
    uint32_t t, k, users[NROOM] = {0, 0};
    for (t = 0; t < NTRK; t++) {
        space_t *S = &space[t];
        int use = trk_live(t);                          /* (DLY or VERB at 0 but modulated up: on) */
        if (use && mod_peak_dev(t, DEV_SPACE, SP_DLY) > 0)
            sp_line_fill(&S->dly, MEM_SPACE + t, SP_DLY16, SP_DLY8);
        else if (S->dly.nch)
            sp_line_free(&S->dly);
        if (use && mod_peak_dev(t, DEV_SPACE, SP_VERB) > 0)
            users[clamp(tp[t].dev[DEV_SPACE][SP_ROOM], 0, 1)] |= 1u << t;
    }
    for (k = 0; k < NROOM; k++) {
        room_t *R = &room[k];
        R->users = (uint8_t)users[k];
        if (users[k]) {
            if (sp_line_fill(&R->rev, MEM_ROOM + k, SP_REV, 0))
                sp_line_fill(&R->pre, MEM_ROOM + k, SP_PRE16, SP_PRE8);
        } else {
            if (R->pre.nch)
                sp_line_free(&R->pre);
            if (R->rev.nch)
                sp_line_free(&R->rev);
        }
    }
}

/* ------------------------------------------------------------ audio --- */
/* a room's combs and allpasses for SIZE, DEC and QUAL (LO: every length half, each sample twice as long) */
static void sp_room(room_t *R, int32_t size, int32_t dec, uint32_t lo, uint32_t type)
{
    uint32_t k, t60 = (uint32_t)(200 + dec * 40);
    if (type) {                                         /* TYPE: its ring (ROOM: as it was) */
        t60 = t60 * SP_TY_T60[type] >> 8;
        R->apg = SP_TY_APG[type];
    } else
        R->apg = 0;
    R->type_seen = (uint8_t)type;
    for (k = 0; k < 4u; k++) {
        uint32_t len = (SP_COMB[k] * (uint32_t)(40 + size * 60 / 100) / 100u) >> lo;
        if (type) {                                     /* (its lengths, inside the comb's chunk) */
            len = len * SP_TY_LEN[type] >> 8;
            len = len < 8u ? 8u : len > (1052u >> lo) ? (1052u >> lo) : len;
        }
        R->clen[k] = (uint16_t)len;
        if (R->ci[k] >= len)
            R->ci[k] = 0;
        R->alen[k] = (uint16_t)(SP_AP[k] >> lo);
        if (R->ai[k] >= R->alen[k])
            R->ai[k] = 0;
        R->cg[k] = rs_gain((2u << lo) * len, t60);      /* (rs_gain counts samples at 44.1 kHz) */
    }
    R->size_seen = (int16_t)size;
    R->dec_seen = (int16_t)dec;
    R->qual = (uint8_t)lo;
}

/* saturate to the lines' 16 bits (one compare when it's inside) */
static inline __attribute__((always_inline)) int32_t sp_sat(int32_t v)
{
    return (uint32_t)(v + 32767) <= 65534u ? v : v < 0 ? -32767 : 32767;
}

/* the delay over a block at half the rate: m in, its echoes added to wl, wr (the state in locals throughout: the
 * compiler can't know the buffers don't alias it) */
/* the echoes' character for a block (sp_delay): AGE's drive, its inverse, its low-pass, the read point's offset at
 * the block's end (wow and flutter), DIFF's allpass gain. All 0: the delay as it was, bit for bit */
typedef struct {
    int32_t dr, dri, ka, wob1, dg;   /* (dg: DIFF's mix, Q15; its allpasses' own gain is SP_DFG) */
} sp_char_t;
#define SP_DFG 19661                 /* DIFF's allpasses' gain, 0.6: fixed, so 0 isn't a bare delay (a mix, not a gain,
                                      * fades it in and out without a jump) */

static void HOT_CODE sp_delay(space_t *S, uint8_t *const *db, int dlb, uint32_t dl, int32_t tgt, int32_t off, int32_t kl,
                     int32_t kh, int32_t fbg, int32_t v0, int32_t v1, const int32_t *m, int32_t *wl, int32_t *wr,
                     const sp_char_t *ch, int16_t *dfa)
{
    int32_t q = S->dq, lp = S->dlp, w = (int32_t)S->dw, gq = v0 << (CTL_LOG2 - 1), dg = v1 - v0;
    int32_t wob = S->wob, dwob = (ch->wob1 - S->wob) >> (CTL_LOG2 - 1), alp = S->alp;
    uint32_t i, j0 = S->dj[0], j1 = S->dj[1];
    for (i = 0; i < CTL / 2u; i++, gq += dg) {
        int32_t e, er, f, s0, s1, ix, g = gq >> (CTL_LOG2 - 1), qq = q;
        q += clamp((tgt - q) >> 11, -96, 96);           /* TIME glides: ~90 ms, at most 3/8 of a sample a sample */
        if (ch->wob1 || wob) {                          /* AGE: the read point wavers (wow and flutter), ramped */
            wob += dwob;
            qq = q - wob < (2 << 8) ? 2 << 8 : q - wob;
            qq = qq > (int32_t)(dl - 4u) << 8 ? (int32_t)(dl - 4u) << 8 : qq;   /* (TIME at its longest: the
                                                                                 * swing stays inside the line) */
        } else
            qq = q;
        ix = w - (qq >> 8);                             /* (a tape echo's bend: within a fifth) */
        if (ix < 0)
            ix += (int32_t)dl;
        s0 = sp_rd(db, dlb, (uint32_t)ix);
        s1 = sp_rd(db, dlb, ix ? (uint32_t)ix - 1u : dl - 1u);
        e = s0 + (((s1 - s0) * (qq & 255)) >> 8);
        er = e;
        if (off) {                                      /* SPRD: the right echo a little ahead */
            int32_t qr = qq - off < (2 << 8) ? 2 << 8 : qq - off;
            ix = w - (qr >> 8);
            if (ix < 0)
                ix += (int32_t)dl;
            s0 = sp_rd(db, dlb, (uint32_t)ix);
            s1 = sp_rd(db, dlb, ix ? (uint32_t)ix - 1u : dl - 1u);
            er = s0 + (((s1 - s0) * (qr & 255)) >> 8);
        }
        if (kh) {                                       /* TONE in the feedback */
            lp += sp_mul(e - lp, kh);
            f = e - lp;
        } else if (kl < 32767) {
            lp += sp_mul(e - lp, kl);
            f = lp;
        } else
            f = e;
        if (ch->dr) {                                   /* AGE: each repeat driven into the tanh (the small signal
                                                         * at the same level) and darker */
            f = (softclip((f * ch->dr) >> 12) * ch->dri) >> 12;
            alp += sp_mul(f - alp, ch->ka);
            f = alp;
        }
        if (ch->dg) {                                   /* DIFF: two allpasses in the feedback, mixed in by DIFF,
                                                         * each repeat smeared more than the last */
            int32_t o = dfa[j0], u = sp_sat(f + sp_mul(o, SP_DFG)), y;
            dfa[j0] = (int16_t)u;
            y = o - sp_mul(u, SP_DFG);
            if (++j0 >= SP_DF0)
                j0 = 0;
            o = dfa[SP_DF0 + j1];
            u = sp_sat(y + sp_mul(o, SP_DFG));
            dfa[SP_DF0 + j1] = (int16_t)u;
            y = o - sp_mul(u, SP_DFG);
            if (++j1 >= SP_DF1)
                j1 = 0;
            f += sp_mul(y - f, ch->dg);
        }
        sp_wr(db, dlb, (uint32_t)w, m[i] + sp_mul(f, fbg));
        if (++w >= (int32_t)dl)
            w = 0;
        wl[i] += (e * g) >> 15;
        wr[i] += (er * g) >> 15;
    }
    S->dq = q;
    S->dlp = lp;
    S->dw = (uint32_t)w;
    S->wob = ch->wob1;
    S->alp = alp;
    S->dj[0] = (uint16_t)j0;
    S->dj[1] = (uint16_t)j1;
}

/* one comb over the block: read, damp, feed back, step; what it read into o[] (the combs are independent: each one
 * hears only the input, so running one after another over the block is what running them side by side was) */
#define SP_COMB_RUN(k)                                                                                    \
    do {                                                                                                  \
        int32_t lp_ = lp##k, cg_ = cg##k;                                                                 \
        uint32_t i_ = i##k, n_ = n##k;                                                                    \
        int16_t *c_ = c##k;                                                                               \
        for (i = 0; i < ns; i++) {                                                                        \
            int32_t o_ = c_[i_];                                                                          \
            if (mo##k) {                    /* MOD: read mo (Q8 samples) newer, between two samples */    \
                uint32_t r_ = i_ + (uint32_t)(mo##k >> 8), r1_;                                           \
                if (r_ >= n_)                                                                             \
                    r_ -= n_;                                                                             \
                r1_ = r_ + 1u < n_ ? r_ + 1u : 0u;                                                        \
                o_ = c_[r_] + (((c_[r1_] - c_[r_]) * (mo##k & 255)) >> 8);                                \
            }                                                                                             \
            lp_ = o_ + sp_mul(lp_ - o_, damp);                                                            \
            c_[i_] = (int16_t)sp_sat(in[i] + sp_mul(lp_, cg_));                                           \
            if (++i_ >= n_)                                                                               \
                i_ = 0;                                                                                   \
            o[i] = o_;                                                                                    \
        }                                                                                                 \
        lp##k = lp_;                                                                                      \
        i##k = i_;                                                                                        \
    } while (0)
/* one allpass over the block on a[] (in series: the second hears all of the first's output, as it did sample by
 * sample) */
#define SP_AP_RUN(k, a)                                                                                   \
    do {                                                                                                  \
        uint32_t j_ = j##k, m_ = m##k;                                                                    \
        int16_t *p_ = ap##k;                                                                              \
        for (i = 0; i < ns; i++) {                                                                        \
            int32_t o_ = p_[j_];                                                                          \
            p_[j_] = (int16_t)sp_sat((a)[i] + (apg ? sp_mul(o_, apg) : sdiv2(o_, 1)));                    \
            (a)[i] = o_ - (a)[i];                                                                         \
            if (++j_ >= m_)                                                                               \
                j_ = 0;                                                                                   \
        }                                                                                                 \
        j##k = j_;                                                                                        \
    } while (0)

/* a room over ns samples at its rate (at most CTL / 2): x in (mono), through the pre-delay (pre samples), its tail
 * into wl, wr (stereo, the right side from its own pair of allpasses fed a different mix of the combs). Stage by
 * stage over the block, not every stage a sample: one loop holding four combs and four allpasses kept about 38
 * values live and spilled on every comb on the target (about 16 registers); each stage on its own keeps its few in
 * registers. The same arithmetic in the same order for every value: the output bit for bit (2026-10-12) */
static void HOT_CODE sp_room_run(room_t *R, uint8_t *const *pb, int plb, uint32_t pl, int32_t pre, uint8_t *const *rb,
                        int32_t kh, int32_t damp, const int32_t *xin, uint32_t ns, int32_t *wl, int32_t *wr,
                        int32_t md, int32_t ig0, int32_t ig1)
{
    int16_t *c0 = (int16_t *)(void *)rb[0], *c1 = (int16_t *)(void *)rb[1], *c2 = (int16_t *)(void *)rb[2];
    int16_t *c3 = (int16_t *)(void *)rb[3], *ap0 = (int16_t *)(void *)rb[4] + SP_APO[0];
    int16_t *ap1 = (int16_t *)(void *)rb[4] + SP_APO[1], *ap2 = (int16_t *)(void *)rb[4] + SP_APO[2];
    int16_t *ap3 = (int16_t *)(void *)rb[4] + SP_APO[3];
    uint32_t i0 = R->ci[0], i1 = R->ci[1], i2 = R->ci[2], i3 = R->ci[3], n0 = R->clen[0], n1 = R->clen[1];
    uint32_t n2 = R->clen[2], n3 = R->clen[3], j0 = R->ai[0], j1 = R->ai[1], j2 = R->ai[2], j3 = R->ai[3];
    uint32_t m0 = R->alen[0], m1 = R->alen[1], m2 = R->alen[2], m3 = R->alen[3];
    int32_t lp0 = R->clp[0], lp1 = R->clp[1], lp2 = R->clp[2], lp3 = R->clp[3], cg0 = R->cg[0], cg1 = R->cg[1];
    int32_t cg2 = R->cg[2], cg3 = R->cg[3], hp = R->rhp, w = (int32_t)R->pw;
    int32_t in[CTL], o[CTL], apg = R->apg, mo0 = 0, mo1 = 0, mo2 = 0, mo3 = 0;
    uint32_t i;
    if (md) {                                           /* MOD: each comb read up to md (Q8) samples newer, by its
                                                         * own LFO, set once a block (it moves a few thousandths of a
                                                         * sample a block: no steps heard) */
        uint32_t k;
        int32_t mo[4];
        for (k = 0; k < 4u; k++) {
            R->mph[k] += SP_MOD_INC[k] * ns;
            mo[k] = (md + ((sine_i(R->mph[k]) * md) >> 15)) >> 1;
        }
        mo0 = mo[0];
        mo1 = mo[1];
        mo2 = mo[2];
        mo3 = mo[3];
    }
    for (i = 0; i < ns; i++) {                          /* the pre-delay, TONE's high-pass, the level in */
        int32_t x, ix = w - pre;
        if (ix < 0)
            ix += (int32_t)pl;
        sp_wr(pb, plb, (uint32_t)w, xin[i]);
        x = pre ? sp_rd(pb, plb, (uint32_t)ix) : xin[i];
        if (++w >= (int32_t)pl)
            w = 0;
        if (kh) {                                       /* TONE +: a high-pass in front */
            hp += sp_mul(x - hp, kh);
            x -= hp;
        }
        in[i] = (x * 2580) >> 15;                       /* (Felucca's ROOM level) */
    }
    if (ig0 != 32767 || ig1 != 32767) {                 /* (after a retune: the combs' input rising) */
        int32_t ig = ig0, dig = (ig1 - ig0) / (int32_t)ns;
        for (i = 0; i < ns; i++, ig += dig)
            in[i] = (in[i] * (ig >> 3)) >> 12;
    }
    SP_COMB_RUN(0);                                     /* the combs: damped feedback loops; the right side's mix */
    for (i = 0; i < ns; i++)                            /* .. alternates their signs (decorrelated from the left) */
        wl[i] = wr[i] = o[i];
    SP_COMB_RUN(1);
    for (i = 0; i < ns; i++) {
        wl[i] += o[i];
        wr[i] -= o[i];
    }
    SP_COMB_RUN(2);
    for (i = 0; i < ns; i++) {
        wl[i] += o[i];
        wr[i] += o[i];
    }
    SP_COMB_RUN(3);
    for (i = 0; i < ns; i++) {
        wl[i] += o[i];
        wr[i] -= o[i];
    }
    SP_AP_RUN(0, wl);                                   /* the allpasses: two each side */
    SP_AP_RUN(1, wl);
    SP_AP_RUN(2, wr);
    SP_AP_RUN(3, wr);
    R->ci[0] = (uint16_t)i0;
    R->ci[1] = (uint16_t)i1;
    R->ci[2] = (uint16_t)i2;
    R->ci[3] = (uint16_t)i3;
    R->ai[0] = (uint16_t)j0;
    R->ai[1] = (uint16_t)j1;
    R->ai[2] = (uint16_t)j2;
    R->ai[3] = (uint16_t)j3;
    R->clp[0] = lp0;
    R->clp[1] = lp1;
    R->clp[2] = lp2;
    R->clp[3] = lp3;
    R->rhp = hp;
    R->pw = (uint32_t)w;
}
#undef SP_COMB_RUN
#undef SP_AP_RUN

/* audio ISR: track t's SPACE on its stereo block (n even): its delay added to it, and what it sends to its room
 * (VERB x lvl, the track's level, Q12) added to the room's input for space_rooms */
static void HOT_CODE space_block(uint32_t t, int32_t *l, int32_t *r, uint32_t n, int32_t lvl)
{
    space_t *S = &space[t];
    const int16_t *v = TPD(t, DEV_SPACE);
    uint8_t *db[SP_LMAX];
    room_t *R = &room[clamp(v[SP_ROOM], 0, 1)];
    int32_t tone = v[SP_TONE], sprd = clamp(v[SP_SPRD], 0, 100), gd1, gv1, gd0 = S->gd, gv0 = S->gv;
    int32_t kl = 32767, kh = 0, fbg, off;               /* (worked out past the "nothing on" return: two divides) */
    int32_t tgt = v[SP_SYNC] ? sp_sync_q8(v[SP_TIME])   /* (SYNC: the tempo's division) */
                             : clamp(v[SP_TIME], 10, 370) * 5645;   /* ms -> Q8 samples at 22.05 kHz (22.05 x 256) */
    uint32_t i, dl = 0, h = n / 2u;
    int dlb = 0;
    if (S->dly.ready) {
        if (!S->dlive || S->dgen != S->dly.gen) {      /* starting: from silence, at TIME already */
            S->dgen = S->dly.gen;
            S->dw = 0;
            S->dq = tgt;
            S->dlp = 0;
            S->dlive = 1;
            S->quiet = 0;
            S->wob = S->alp = S->dpk = S->denv = S->ages = S->difs = 0;
            S->dfdirty = 0;
            S->dj[0] = S->dj[1] = 0;
            for (i = 0; i < SP_DF0 + SP_DF1; i++)
                sp_dfa[t][i] = 0;
        }
        sp_bases(&S->dly, db);
        dl = sp_len(&S->dly);
        dlb = S->dly.bits8;
    } else
        S->dlive = 0;
    if (v[SP_DLY] != S->mdly) {
        S->mdly = v[SP_DLY];
        S->mgd = clamp(v[SP_DLY], 0, 100) * 26214 / 100;   /* DLY 100: the first echo at 0.8 */
    }
    gd1 = S->dlive ? S->mgd : 0;
    if (v[SP_DUCK] > 0 && gd1) {                        /* DUCK: the echoes down by the track's level (last block's
                                                         * peak: fast up, ~190 ms down), all the way at -12 dBFS */
        int32_t k;
        S->denv = S->dpk > S->denv ? S->dpk : S->denv - (S->denv >> 8);
        k = S->denv > 8191 ? 32767 : S->denv << 2;
        gd1 = (gd1 * (32767 - ((PCT_Q15[clamp(v[SP_DUCK], 0, 100)] * k) >> 15))) >> 15;
    }
    gd1 = gd0 + clamp(gd1 - gd0, -2048, 2048);          /* (DLY, and DUCK's quick dip, glide: ~10 ms end to end;
                                                         * across one block it was a click, 2026-10-13) */
    gv1 = R->rev.ready && R->pre.ready ? PCT_Q15[clamp(v[SP_VERB], 0, 100)] * clamp(lvl, 0, 4096) >> 12 : 0;
    S->gd = gd1;
    S->gv = gv1;
    if (!gd0 && !gd1 && !gv0 && !gv1 && !(S->dlive && S->mgd)) {   /* nothing on (or nothing yet; DUCK all the way
                                                         * down is still on: the line goes on taking the sound, for
                                                         * the echoes to come back) */
        S->pl = S->pr = 0;
        return;
    }
    if (v[SP_FDBK] != S->mfb) {
        S->mfb = v[SP_FDBK];
        S->mfbg = clamp(v[SP_FDBK], 0, 100) * 32112 / 100;
    }
    fbg = S->mfbg;
    off = sprd ? sprd * 220 * 256 / 100 : 0;
    if (tone < 0)                                       /* - : a low-pass in the feedback */
        kl = 32767 + tone * 287;                        /* (to 0.12 at -100: ~430 Hz) */
    else if (tone > 0)                                  /* + : a high-pass in the feedback */
        kh = tone * 60;                                 /* (to 0.18 at 100: ~650 Hz) */
    if (dl && tgt > (int32_t)(dl - 4u) << 8)
        tgt = (int32_t)(dl - 4u) << 8;
    {
        int32_t m[CTL / 2], wl[CTL / 2], wr[CTL / 2], ol = S->pl, or_ = S->pr, in = 0, loud = 0;
        int32_t pk = 0;
        for (i = 0; i < h; i++) {                       /* mono, at half the rate */
            m[i] = sp_sat((l[2 * i] + r[2 * i] + l[2 * i + 1] + r[2 * i + 1]) >> 2);
            in |= m[i];
            pk = m[i] > pk ? m[i] : -m[i] > pk ? -m[i] : pk;
        }
        S->dpk = pk;                                    /* (DUCK's, next block) */
        if (gv0 || gv1) {                               /* the send, ramped across the block */
            int32_t gq = gv0 << (CTL_LOG2 - 1), dg = gv1 - gv0;
            for (i = 0; i < h; i++, gq += dg)
                R->in[i] += (m[i] * (gq >> (CTL_LOG2 - 1))) >> 15;
        }
        if (!S->dlive || (!in && S->quiet > dl)) {      /* no delay, or resting: silence in, nothing left in it */
            S->pl = S->pr = 0;
            return;
        }
        for (i = 0; i < h; i++)
            wl[i] = wr[i] = 0;
        {
            sp_char_t ch = {0, 0, 0, 0, 0};
            int32_t a8;
            S->ages += clamp((clamp(v[SP_AGE], 0, 100) << 8) - S->ages, -256, 256);   /* (a unit a block) */
            S->difs += clamp(PCT_Q15[clamp(v[SP_DIFF], 0, 100)] - S->difs, -656, 656);   /* (2 % a block) */
            a8 = S->ages;
            if (a8) {                                   /* AGE: up to 2x drive into the tanh, a low-pass down to
                                                         * ~1.4 kHz a pass; the warble, at 100: wow (0.7 Hz) to +-80
                                                         * samples, ~27 cents; a drift under it (0.27 Hz, +-40, ~5
                                                         * cents: the wow wanders, not a clean cycle); flutter (6.3
                                                         * Hz) to +-5, ~15 cents (2026-10-13: "warble a bit more";
                                                         * were 16 and 7 cents) */
                ch.dr = 4096 + ((a8 * 41) >> 8);
                ch.dri = (4096 << 12) / ch.dr;
                ch.ka = 32767 - ((a8 * 200) >> 8);
                S->wph += 136348u * h;
                S->fph += 1227135u * h;
                S->dph += 52590u * h;
                ch.wob1 = (sine_i(S->wph) * ((a8 * 204) >> 8) + sine_i(S->dph) * ((a8 * 102) >> 8) +
                           sine_i(S->fph) * ((a8 * 13) >> 8)) >> 15;
            }
            if (S->difs) {                              /* DIFF: the allpasses mixed in */
                ch.dg = S->difs;
                S->dfdirty = 1;
            } else if (S->dfdirty) {                    /* (back at 0: their lines cleared, once, so turning it up
                                                         * again starts from silence, not old sound) */
                for (i = 0; i < SP_DF0 + SP_DF1; i++)
                    sp_dfa[t][i] = 0;
                S->dfdirty = 0;
            }
            sp_delay(S, db, dlb, dl, tgt, off, kl, kh, fbg, gd0, gd1, m, wl, wr, &ch, sp_dfa[t]);
        }
        for (i = 0; i < h; i++) {                       /* back up to 44.1 kHz: halfway, then the sample */
            loud |= sp_loud(wl[i]) | sp_loud(wr[i]);
            l[2 * i] += (ol + wl[i]) >> 1;
            r[2 * i] += (or_ + wr[i]) >> 1;
            l[2 * i + 1] += wl[i];
            r[2 * i + 1] += wr[i];
            ol = wl[i];
            or_ = wr[i];
        }
        S->pl = ol;
        S->pr = or_;
        if (in || loud)
            S->quiet = 0;
        else if ((S->quiet += h) > dl)                  /* settled: its feedback's filter let go as it rests */
            S->dlp = 0;
    }
}

/* audio ISR, after the tracks: the rooms on what the tracks sent them this block, into the mix (l r, n samples);
 * their inputs cleared for the next block. HI at 22.05 kHz, LO at 11.025 kHz (the sends averaged in pairs, the tail
 * stepped back up across four samples) */
static void HOT_CODE space_rooms(int32_t *l, int32_t *r, uint32_t n)
{
    uint32_t k, i, h = n / 2u;
    for (k = 0; k < NROOM; k++) {
        room_t *R = &room[k];
        const int16_t *q = rmp[k];
        uint8_t *pb[SP_LMAX], *rb[SP_REV];
        uint32_t qw = (uint32_t)clamp(q[RM_QUAL], 0, 1), tw = (uint32_t)clamp(q[RM_TYPE], 0, 3);
        uint32_t lo = R->live ? R->qual : qw, ns = h >> lo, pl, settle;   /* (a new QUAL waits for the fade) */
        int32_t tone = q[RM_TONE], kh = 0, damp = 6554, pre, in = 0, loud = 0, wl[CTL / 2], wr[CTL / 2], x[CTL / 2];
        int plb;
        if (!(R->rev.ready && R->pre.ready)) {
            R->live = 0;
            for (i = 0; i < h; i++)
                R->in[i] = 0;
            continue;
        }
        if (!R->live || R->gen != (uint8_t)(R->rev.gen + R->pre.gen)) {   /* starting: from silence */
            R->gen = (uint8_t)(R->rev.gen + R->pre.gen);
            R->pw = 0;
            R->rhp = 0;
            for (i = 0; i < 4u; i++) {
                R->ci[i] = R->ai[i] = 0;
                R->clp[i] = 0;
            }
            R->size_seen = -1;
            R->tyg = 32767;
            R->ing = 32767;
            R->qual = (uint8_t)lo;
            R->live = 1;
            R->quiet = 0;
            R->pl = R->pr = 0;
        }
        sp_bases(&R->pre, pb);
        sp_bases(&R->rev, rb);
        pl = sp_len(&R->pre);
        plb = R->pre.bits8;
        if (R->clen[0] == 0 || R->size_seen < 0)        /* starting: SIZE and TYPE as asked */
            sp_room(R, clamp(q[RM_SIZE], 0, 100), clamp(q[RM_DEC], 0, 100), lo, tw);
        else if (q[RM_DEC] != R->dec_seen)              /* DEC: at once (only the loops' gains); a new SIZE or TYPE
                                                         * waits for the fade, below */
            sp_room(R, R->size_seen, clamp(q[RM_DEC], 0, 100), lo, R->type_seen);
        if (tone < 0)                                   /* - : darker combs */
            damp = 6554 - tone * 200;
        else if (tone > 0) {                            /* + : a high-pass in front, the combs a little brighter */
            kh = (tone * 60) << lo;
            damp = 6554 - tone * 49;
        }
        if (R->type_seen)                               /* TYPE: its damping */
            damp = clamp(damp * SP_TY_DAMP[R->type_seen] >> 8, 0, 30000);
        pre = (clamp(q[RM_PRE], 0, 200) * 2205 / 100) >> lo;
        if (pre > (int32_t)pl - 2)
            pre = (int32_t)pl - 2;
        settle = (uint32_t)pre + (1017u >> lo) + (1020u >> lo);
        for (i = 0; i < ns; i++) {                      /* what came in (LO: two by two) */
            x[i] = lo ? (R->in[2 * i] + R->in[2 * i + 1]) >> 1 : R->in[i];
            x[i] = sp_sat(x[i]);
            in |= x[i];
        }
        for (i = 0; i < h; i++)
            R->in[i] = 0;
        {                                               /* a new SIZE, QUAL or TYPE (changing the lines' lengths
                                                         * or rate at once was a crunch): once the knob has stood
                                                         * still ~18 ms, faded out (~12 ms), its lines cleared, retuned
                                                         * (a QUAL: started again at its rate), faded back in; resting,
                                                         * at once */
            uint32_t key = (uint32_t)(uint16_t)q[RM_SIZE] | qw << 16 | tw << 18;
            int pend = q[RM_SIZE] != R->size_seen || qw != R->qual || tw != R->type_seen;
            if (key != R->pk) {
                R->pk = key;
                R->pcnt = 0;
            } else if (R->pcnt < 255u)
                R->pcnt++;
            if (pend && (!R->tyg || (!in && R->quiet > settle))) {
                uint32_t k2, c2;
                for (k2 = 0; k2 < SP_REV; k2++)
                    for (c2 = 0; c2 < RS_LEN; c2++)
                        ((int16_t *)(void *)rb[k2])[c2] = 0;
                for (k2 = 0; k2 < 4u; k2++)
                    R->clp[k2] = 0;
                R->ing = 0;
                if (qw != R->qual) {                    /* (QUAL: from silence at its rate, next block, the
                                                         * pre-delay's line cleared too: its rate changes. SIZE and
                                                         * TYPE keep it, so the room's input runs on unbroken) */
                    for (c2 = 0; c2 < pl; c2++)
                        sp_wr(pb, plb, c2, 0);
                    R->live = 0;
                    R->qual = (uint8_t)qw;
                    continue;
                }
                sp_room(R, clamp(q[RM_SIZE], 0, 100), clamp(q[RM_DEC], 0, 100), lo, tw);
            }
        }
        if (!in && R->quiet > settle) {                 /* resting (sp_quiet) */
            R->tyg = 32767;
            continue;
        }
        R->mds += clamp((clamp(q[RM_MOD], 0, 100) * 6 >> lo) - R->mds, -8, 8);   /* (MOD: to 2.3 samples at 22.05
                                                         * kHz, 0.1 ms, the same time at 11.025; gliding ~55 ms end to
                                                         * end) */
        {
            int32_t ig0 = R->ing, ig1 = ig0 + 1024 > 32767 ? 32767 : ig0 + 1024;
            R->ing = ig1;
            sp_room_run(R, pb, plb, pl, pre, rb, kh, damp, x, ns, wl, wr, R->mds, ig0, ig1);
        }
        {
            int pend = q[RM_SIZE] != R->size_seen || qw != R->qual || tw != R->type_seen;
            int32_t g0 = R->tyg, g1 = pend && R->pcnt >= 24u ? g0 - 2048 : g0 + 512;   /* (out in 16 blocks, ~12
                                                         * ms; back in 64, ~47 ms: longer than the combs, so their first
                                                         * echoes from the cleared lines come in under the fade) */
            g1 = clamp(g1, 0, 32767);
            R->tyg = g1;
            if (g0 != 32767 || g1 != 32767) {           /* (TYPE's fade, across the block: one divide) */
                int32_t g = g0, dg2 = (g1 - g0) / (int32_t)ns;
                for (i = 0; i < ns; i++, g += dg2) {
                    wl[i] = (wl[i] * (g >> 3)) >> 12;
                    wr[i] = (wr[i] * (g >> 3)) >> 12;
                }
            }
        }
        {
            int32_t ol = R->pl, or_ = R->pr;
            for (i = 0; i < ns; i++) {
                loud |= sp_loud(wl[i]) | sp_loud(wr[i]);
                if (lo) {                               /* 11.025 -> 44.1 kHz: a quarter of the way at a time */
                    int32_t dl = wl[i] - ol, dr = wr[i] - or_;
                    l[4 * i] += ol + (dl >> 2);
                    r[4 * i] += or_ + (dr >> 2);
                    l[4 * i + 1] += ol + (dl >> 1);
                    r[4 * i + 1] += or_ + (dr >> 1);
                    l[4 * i + 2] += ol + ((dl * 3) >> 2);
                    r[4 * i + 2] += or_ + ((dr * 3) >> 2);
                    l[4 * i + 3] += wl[i];
                    r[4 * i + 3] += wr[i];
                } else {                                /* 22.05 -> 44.1 kHz: halfway, then the sample */
                    l[2 * i] += (ol + wl[i]) >> 1;
                    r[2 * i] += (or_ + wr[i]) >> 1;
                    l[2 * i + 1] += wl[i];
                    r[2 * i + 1] += wr[i];
                }
                ol = wl[i];
                or_ = wr[i];
            }
            R->pl = ol;
            R->pr = or_;
        }
        if (in || loud)
            R->quiet = 0;
        else if ((R->quiet += ns) > settle) {           /* settled: its loops' filters let go as it rests */
            R->rhp = 0;
            for (i = 0; i < 4u; i++)
                R->clp[i] = 0;
        }
    }
}
