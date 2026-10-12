/* SPDX-License-Identifier: GPL-3.0-only */
/* Bryo: GRAIN, the first device after the source (docs/bryo-architecture.md, "GRAIN, as built"). The S-4's MOSAIC:
 * grains of a live buffer of what the source plays, blended with the source by WET; from here on the track is stereo
 * (SPRD places each grain).
 *
 * The buffer: while GRAIN is on (WET above 0) and SCAN isn't TAPE, it keeps the last few bars of the source (the
 * tape playing, or SYNTH or POLY as you play them) in chunks of the shared memory (mem.c), in the tape's own format.
 * Its length follows TRACKS: 1 bar with 3 or 4 tracks, 2 bars with 2, 4 bars with 1 (whole bars, at most 12 s, so
 * a slow tempo gets fewer). The write head starts at the bar line when the transport starts and wraps at the
 * buffer's end, so the buffer always holds whole bars. It records while the transport plays, or always on a SYNTH
 * or POLY track (you play those without the transport); FDBK keeps that share of what was there (0: the last bars
 * only; 100: layers that never fade). Switching the source carries over: the old sound stays in the buffer until the
 * write head passes, fading at FDBK each pass.
 *
 * Freeze (the 0 black key, held: every track): the buffer stops recording and holds its bars, and the grains go on
 * playing them, a loop locked to the tempo (a tempo change while frozen: the loop's pace follows).
 *
 * SCAN: where grains start (the cursor).
 *   TAPE  grains of the track's tape inside TAPE's loop window, as before the buffer (no memory taken): the cursor
 *         moves at WARP's speed (100 %: the tape's pace; 0 still; below 0 backwards); frozen: it stops.
 *   STR   (stretch) the cursor moves through the buffer at WARP's speed against the write head's: 100 keeps pace
 *         (a fixed distance behind), below falls behind and stretches time, 0 holds a spot.
 *   POS   (position) the cursor stays at OFST of the buffer (0: the bar line).
 *   DLY   (delay) the cursor trails the write head by OFST of the buffer: grains of what played that long ago.
 * On the GRAIN page the white keys move the cursor to their sixteenth of the tape's loop (TAPE) or of the buffer
 * (POS: the spot; DLY: the trail), instead of moving the tape's head; turning OFST takes over again.
 *
 * A grain: starts around the cursor (SPRY scatters it, up to half the window either way), lasts SIZE ms, reads at
 * PTCH semitones plus a random +-PRND (held to SCAL's scale when it isn't OFF), backwards for REV's share of grains,
 * shaped by a window whose ramps grow with CONT (0: square with 2 ms ends, 100: a full Hann; taken once a block and
 * ramped across it), placed in the stereo field at random within +-SPRD, its level 1 / sqrt(the grains expected to
 * overlap). RATE sets how many start a second (1..80, on a square law), PATN their rhythm: EVEN, SWNG (long-short),
 * CLST (bursts of four), RND. Grains start while the transport plays, on a SYNTH or POLY track, while frozen, or
 * while a white key is held on the GRAIN page; those sounding finish either way.
 *
 * Reading: no copy. A grain decodes the ADPCM itself into a 128-sample window, from the stored decoder state at the
 * start of the window's block; a forward grain carries its decoder on (one decode per sample), a reverse grain
 * refills its window backwards. Outside what it reads: silence.
 *
 * Load: the grains that may sound at once are a pool that follows how many tracks have GRAIN on (WET above 0, in
 * use): 40 for one, 30 for two, 18 for three or four. It's shared by what each track asks for (gr_plan, every block):
 * a track's need is the grains a second times SIZE, with room for bunching; a track asking less than an even share
 * gets what it asks, and the rest is split evenly among the ones that want more. So a dense pad beside a sparse grain
 * track keeps most of the pool, and only two dense tracks thin each other. A grain due while all a track's share
 * sounds is skipped (those sounding finish: a share that drops thins a cloud over one grain's length, no cut), and
 * its level counts only the grains it can sound, so a thinned cloud doesn't also get quieter. The audio ISR's
 * shedding (chain_shed) shrinks the pool, a quarter at a time down to half, and it comes back an eighth a second.
 *
 * Audio ISR, except grain_poll (the main loop: the buffers' memory). The main loop writes the knobs. */

#define GR_SLOTS 40u                 /* grains in all (the most one track may sound: GR_POOL[1]) */
#define GR_CAP 8u                    /* the pool's scale: x grain_cap / GR_CAP (shedding lowers grain_cap to 4) */
#define GR_WIN 128u                  /* a grain's decoded window, samples (a reverse refill re-decodes from its
                                      * block's start: the wider the window, the rarer) */
#define GR_BUF_MAX (12u * TAPE_SR)   /* a buffer's most, tape samples (12 s) */
enum { GP_SIZE, GP_RATE, GP_PTCH, GP_SPRD, GP_WET, GP_SPRY, GP_CONT, GP_REV, GP_PATN, GP_SCAL, GP_PRND, GP_ROUT, GP_SCAN,
       GP_WARP, GP_OFST, GP_FDBK, GP_BODY, GP_BMOD, GP_ROOT, GP_BTON,
       GP_GRNS, GP_SYNC };   /* (param.c DEV_P[DEV_GRAIN]) */
enum { BODY_SRC, BODY_TRI, BODY_SIN };
enum { SCAN_TAPE, SCAN_STR, SCAN_POS, SCAN_DLY };

/* sin^2 (pi/2 i/128), Q15: a window's rising ramp */
static const int16_t GR_RAMP[129] = {
    0, 5, 20, 44, 79, 123, 177, 241, 315, 398, 491, 593, 705, 827, 958, 1098,
    1247, 1406, 1573, 1749, 1935, 2128, 2331, 2542, 2761, 2989, 3224, 3468, 3719, 3978, 4244, 4518,
    4799, 5086, 5381, 5682, 5990, 6304, 6624, 6950, 7281, 7618, 7961, 8308, 8660, 9017, 9379, 9744,
    10114, 10487, 10864, 11244, 11628, 12014, 12403, 12794, 13187, 13583, 13980, 14378, 14778, 15178, 15580, 15981,
    16383, 16786, 17187, 17589, 17989, 18389, 18787, 19184, 19580, 19973, 20364, 20753, 21139, 21523, 21903, 22280,
    22653, 23023, 23388, 23750, 24107, 24459, 24806, 25149, 25486, 25817, 26143, 26463, 26777, 27085, 27386, 27681,
    27968, 28249, 28523, 28789, 29048, 29299, 29543, 29778, 30006, 30225, 30436, 30639, 30832, 31018, 31194, 31361,
    31520, 31669, 31809, 31940, 32062, 32174, 32276, 32369, 32452, 32526, 32590, 32644, 32688, 32723, 32747, 32762,
    32767,
};
/* 1 / sqrt(n), Q15: a grain's level when n grains are expected to overlap */
static const int16_t GR_NORM[33] = {32767, 32767, 23170, 18918, 16384, 14654, 13377, 12385, 11585, 10922, 10362, 9880,
                                     9459, 9088, 8757, 8460, 8192, 7947, 7723, 7517, 7327, 7151, 6986, 6833, 6689,
                                     6554, 6426, 6306, 6193, 6085, 5983, 5885, 5793};   /* 1 / sqrt n, Q15 */
/* the pool, by how many tracks have GRAIN on */
static const uint8_t GR_POOL[NTRK + 1u] = {40, 40, 30, 18, 18};   /* (2026-10-11: were 32 24 16 16, before the
                                                                  * grains rendered at half rate) */
/* the scales SCAL holds pitches to (OFF CHR MAJ MIN PEN): the semitones of an octave they keep */
static const uint16_t GR_SCALE[5] = {0, 0xFFF, 0xAB5, 0x5AD, 0x295};

typedef struct {
    int32_t pos;                     /* the read position, Q12 samples */
    int32_t inc;                     /* its step per output sample, Q12 (below 0: backwards) */
    uint32_t left, len, fade;        /* output samples to go, in all, in each ramp */
    int32_t gl, gr;                  /* its level into each side, Q15 */
    int32_t wlo;                     /* the sample at w[0] (-1: nothing decoded yet) */
    int32_t dpos, pred, idx;         /* the decoder: the next sample it decodes, its state */
    int16_t w[GR_WIN];
    uint16_t wver;                   /* the buffer's commit count its window was decoded at (gr_buf_t.cver) */
    int32_t qb;                      /* the block it last found silent or not (gr_quiet's answer, qq), or -1 */
    uint8_t qq;
    int16_t st16;                    /* its pitch, 1/16 semitone (for the tests and the screen) */
    uint8_t rev;
    uint8_t trk;                     /* whose it is */
} grain_t;

typedef struct {                     /* a track's live buffer */
    uint8_t map[MEM_NC];             /* its chunks (the main loop's) */
    volatile uint8_t nch;
    volatile uint8_t bars;           /* the bars it holds */
    volatile uint32_t len;           /* samples it holds: its bars at the tempo it was sized at (0: none) */
    /* the ISR's */
    int32_t w;                       /* the write head, samples */
    int32_t hx[6];                   /* the last six output samples (the half-band filter's, gr_record) */
    int32_t fill;                    /* samples recorded since the head last started over (at most len; STR's cursor
                                      * stays inside them) */
    uint32_t flen;                   /* len when fill was last counted (resized: only what's behind the head counts) */
    uint8_t odd;
    uint8_t staged;                  /* stage holds block sblk, being written */
    uint8_t was_playing;
    uint32_t sblk;
    int16_t stage[TAPE_BLK];
    uint16_t nloud;                  /* ISR: its blocks with sound in them (a peak of 1 or more: above about -48 dB),
                                      * kept as each is written; 0: grains of it would be silence */
    uint32_t loud_len;               /* .. the length and chunks it was counted over (changed: counted again) */
    uint8_t loud_nch;
    uint16_t cver;                   /* ISR: blocks committed (a grain whose window holds the last one decodes again) */
    uint32_t cblk;                   /* .. the last one */
} gr_buf_t;

typedef struct {
    int32_t cur;                     /* the cursor, Q12 samples */
    int32_t wait;                    /* output samples until the next grain */
    uint32_t sb;                     /* SYNC: the beat (Q16) the next grain starts on */
    uint8_t sync_on;                 /* .. sb is set (SYNC was on last block) */
    uint32_t n;                      /* grains started (PATN's beat count) */
    uint32_t keys;                   /* the white keys last block (the GRAIN page) */
    int32_t rng;
    int32_t kofs;                    /* a white key's spot (POS: where; DLY: the trail), samples; -1 none */
    int16_t ofst_seen;               /* OFST when the key was pressed (turning OFST takes over again) */
    uint8_t scan;                    /* SCAN last block (a change: the cursor starts over) */
    uint8_t dset;                    /* dsh holds last block's dry share */
    int32_t dsh;                     /* the dry share last block, Q15 (ROUT switched: ramped across the next) */
    int32_t hl[3], hr[3];            /* the grains' half-rate mix: its last three samples (the 2x interpolator's) */
    int32_t wetq;                    /* WET last block, Q15: the grains' mix ramps from it (0: the grains are off) */
    uint32_t bph;                    /* BODY: its wave's phase (TRI, SIN) */
    int16_t bto;                     /* .. the last grain started: its pitch in the octave over ROOT, 1/16 semitone */
    int16_t bst;                     /* .. the wave's pitch there now (gliding to bto) */
    int32_t blp;                     /* .. TONE's low-pass */
    int32_t bg;                      /* .. its level last block, Q15 (each block ramps from it) */
    int32_t benv;                    /* .. TRI SIN: the gate, Q15: open while grains sound, closing after them */
    uint8_t bmode;                   /* .. the MODE it's sounding (a new one: down to 0 a block, then the new) */
    uint8_t hq;                      /* 1: they're 0 (nothing to carry into the next block) */
} grain_trk_t;

static grain_t gslot[GR_SLOTS] __attribute__((section(".pool")));
static uint32_t gr_used[2];          /* a bit per slot sounding (GR_SLOTS past 32: two words) */
static uint32_t gr_trk[NTRK][2];     /* the same bits by track: a track walks its own slots, not all 40 (kept in
                                      * step with gr_used by gr_tset and gr_tclr: a slot is set with its track) */
static inline uint32_t gr_on(uint32_t k) { return (gr_used[k >> 5] >> (k & 31u)) & 1u; }
static inline void gr_set(uint32_t k) { gr_used[k >> 5] |= 1u << (k & 31u); }
static inline void gr_clr(uint32_t k) { gr_used[k >> 5] &= ~(1u << (k & 31u)); }
static inline void gr_tset(uint32_t t, uint32_t k) { gr_set(k); gr_trk[t][k >> 5] |= 1u << (k & 31u); }
static inline void gr_tclr(uint32_t t, uint32_t k) { gr_clr(k); gr_trk[t][k >> 5] &= ~(1u << (k & 31u)); }
/* the lowest set bit's slot of a track's mask word w (w != 0) */
static inline uint32_t gr_low(uint32_t w)
{
    uint32_t k = 0;
    if (!(w & 0xFFFFu)) { w >>= 16; k += 16u; }
    if (!(w & 0xFFu)) { w >>= 8; k += 8u; }
    if (!(w & 0xFu)) { w >>= 4; k += 4u; }
    if (!(w & 0x3u)) { w >>= 2; k += 2u; }
    return k + (1u - (w & 1u));
}
static grain_trk_t grain[NTRK];
static gr_buf_t gbuf[NTRK];
static uint32_t grain_cap = GR_CAP;  /* the pool's scale, lowered by chain_shed */
static uint8_t gr_share[NTRK];       /* the grains each track may sound at once now (gr_plan) */
static volatile uint8_t grain_frozen;   /* the 0 key held (the ISR writes it; the screen tags FROZEN) */
static uint32_t grain_calm;          /* blocks since the last shed */

static inline uint32_t gr_rnd(grain_trk_t *G) { return noise32(&G->rng); }

/* TRACKS in use (sys.ntrk, read safely) */
static uint32_t gr_ntrk(void) { return sys.ntrk >= 1u && sys.ntrk <= NTRK ? sys.ntrk : NTRK; }

/* track t's devices may hold memory: it's in use (TRACKS) and the USB record mode isn't up (nothing renders then,
 * and the take can have their memory) */
static int trk_live(uint32_t t) { return t < gr_ntrk() && !sys.usbrec; }

/* the grains a second x 100 at RATE r (1..80, on a square law) */
static inline int32_t gr_gps100(int32_t r) { return 100 + r * r * 79 / 100; }

/* SYNC: RATE as the tempo's divisions, a grain each (beats, Q16): 1 bar, 1/2, 1/4, 1/8, 8T, 1/16, 16T, 1/32, 32T,
 * 1/64 (ui.c names them) */
#define GR_NSDIV 10u
static const uint32_t GR_SDIV[GR_NSDIV] = {4u << 16, 2u << 16, 1u << 16, 1u << 15, 21845u, 1u << 14, 10923u, 1u << 13,
                                           5461u, 1u << 12};
static inline uint32_t gr_sdiv(const int16_t *p)
{
    return GR_SDIV[(uint32_t)clamp(p[GP_RATE], 0, 100) * GR_NSDIV / 101u];
}

/* the grains a second x 100 that RATE asks for (SYNC: its division at the tempo) */
static inline int32_t gr_rate_gps(const int16_t *p)
{
    if (!p[GP_SYNC])
        return gr_gps100(p[GP_RATE]);
    return (int32_t)((uint32_t)clamp(sys.bpm, 40, 240) * 109227u / gr_sdiv(p));   /* (100 x bpm / 60 a beat: x 65536
                                                         * / 60 = 109,227 over the division's beats Q16) */
}

/* the grains a track's knobs ask for at once: those a second x SIZE (m), with room for PATN's and chance's
 * bunching (m + m/2 + 2) */
static uint32_t gr_need(const int16_t *p)
{
    int32_t m = gr_rate_gps(p) * p[GP_SIZE] / 100000;
    return (uint32_t)(m + (m >> 1) + 2);               /* (m >= 0: the shift is the divide) */
}

/* track t's grains would be heard now, so it takes a share of the pool: GRAIN on (WET above 0), the track not muted,
 * something to read (a tape: there is one; the buffer: sound has gone into it, gr_buf_loud), and grains able to start
 * (grain_block's rule: the transport playing, or on the GRAIN page; reading the buffer, also a SYNTH or POLY track or
 * frozen). A track that isn't keeps the grains it has sounding (they finish); it starts none. loud: the ISR's
 * gr_buf_loud, or the screen's last copy of it */
static int gr_wants(uint32_t t, int loud)
{
    const int16_t *p = TPD(t, DEV_GRAIN);
    uint32_t scan = (uint32_t)clamp(p[GP_SCAN], 0, 3);
    int keys = t == sys.sel && sys.keys_grain;
    if (!trk_live(t) || p[GP_WET] <= 0 || track[t].mute)
        return 0;
    if (scan == SCAN_TAPE) {
        tape_view_t v;
        tape_view(t, &v);
        return v.len && (sys.playing || keys);
    }
    return loud && (sys.playing || keys || tp[t].src != SRC_TAPE || grain_frozen);
}

/* why track t takes no share now (gr_wants said no), for the GRAIN page; 0: it would take one */
static const char *gr_why(uint32_t t)
{
    const int16_t *p = TPD(t, DEV_GRAIN);
    if (gr_wants(t, gbuf[t].nloud != 0 && gbuf[t].loud_len == gbuf[t].len) || p[GP_WET] <= 0 || !trk_live(t))
        return 0;
    if (track[t].mute)
        return "MUTED: NO GRAINS";
    if (clamp(p[GP_SCAN], 0, 3) == SCAN_TAPE) {
        tape_view_t v;
        tape_view(t, &v);
        return v.len ? "STOPPED: NO GRAINS" : "NO TAPE: NO GRAINS";
    }
    return tp[t].src == SRC_TAPE && !sys.playing ? "STOPPED: NO GRAINS" : "SILENT: NO GRAINS";
}

/* left shared among the tracks in act by what each asks (ask[]), into share[]: those asking less than an even share
 * of what's left get what they ask, round by round; the rest is split evenly among those that want more. Returns
 * what's left over (0 when it ran short) */
static uint32_t gr_split(uint8_t *share, uint32_t act, const uint32_t *ask, uint32_t left)
{
    uint32_t t, k;
    for (k = 0; k < NTRK && act; k++) {
        uint32_t n = 0, even, gave = 0;
        for (t = 0; t < NTRK; t++)
            n += (act >> t) & 1u;
        even = left / n;
        for (t = 0; t < NTRK; t++)
            if (((act >> t) & 1u) && ask[t] <= even) {
                share[t] = (uint8_t)ask[t];
                left -= ask[t];
                act &= ~(1u << t);
                gave = 1;
            }
        if (!gave) {                                    /* everyone left wants more: an even split */
            uint32_t i = 0;
            for (t = 0; t < NTRK; t++)
                if ((act >> t) & 1u)
                    share[t] = (uint8_t)(left / n + (i++ < left % n));
            return 0;
        }
    }
    return left;
}

/* the pool shared out, into share[]. The pool's size follows the tracks whose grains would be heard (gr_wants).
 * A track whose GRNS is a number holds that many: those first, so the AUTO tracks give way to them (more held than
 * the pool: the held ones share it as gr_split does, by what each holds). The AUTO tracks share what's left by need
 * (RATE x SIZE, with room: gr_need); when they all have what they ask and some is left, it's split evenly among
 * them: the need only decides who gives way when the pool runs short, it's no cap (a lone AUTO track has all 40).
 * A held track never gets more than it holds: what it holds is what it keeps, whatever comes and goes. (The ISR's
 * every block, gr_plan; the screen the same, for itself.) */
static void gr_plan_into(uint8_t *share)
{
    uint32_t t, on = 0, held = 0, aut = 0, nauto = 0, left, ask[NTRK];
    for (t = 0; t < NTRK; t++) {
        share[t] = 0;
        ask[t] = 0;
        if (gr_wants(t, gbuf[t].nloud != 0 && gbuf[t].loud_len == gbuf[t].len)) {
            const int16_t *p = TPD(t, DEV_GRAIN);
            on++;
            if (p[GP_GRNS] > 0) {
                held |= 1u << t;
                ask[t] = (uint32_t)clamp(p[GP_GRNS], 1, GR_GRNS_MAX);
            } else {
                aut |= 1u << t;
                nauto++;
                ask[t] = gr_need(p);
            }
        }
    }
    left = GR_POOL[on] * grain_cap / GR_CAP;
    left = gr_split(share, held, ask, left);
    left = gr_split(share, aut, ask, left);
    if (nauto && left) {                                /* the AUTO tracks have what they ask, some left: theirs */
        uint32_t i = 0;
        for (t = 0; t < NTRK; t++)
            if ((aut >> t) & 1u)
                share[t] = (uint8_t)(share[t] + left / nauto + (i++ < left % nauto));
    }
}

/* audio ISR, each block: the shares the grains start under */
static int gr_buf_loud(uint32_t t);
static void gr_plan(void)
{
    uint32_t t;
    for (t = 0; t < NTRK; t++)                          /* (the loud counts current before the plan reads them) */
        (void)gr_buf_loud(t);
    gr_plan_into(gr_share);
}

/* how many grains track t may sound at once now */
static uint32_t gr_allow(uint32_t t) { return gr_share[t]; }

/* the grains sounding on track t (the screen, the tests) */
static uint32_t grain_count(uint32_t t)
{
    uint32_t w, c = 0, j;
    for (j = 0; j < 2u; j++)
        for (w = gr_trk[t][j]; w; w &= w - 1u)
            c++;
    return c;
}

/* every grain of track t stops */
static void grain_kill(uint32_t t)
{
    uint32_t k;
    gr_used[0] &= ~gr_trk[t][0];
    gr_used[1] &= ~gr_trk[t][1];
    gr_trk[t][0] = gr_trk[t][1] = 0;
    for (k = 0; k < 3u; k++)                            /* (the half-rate mix's tail: nothing left to carry) */
        grain[t].hl[k] = grain[t].hr[k] = 0;
    grain[t].hq = 1;
}

/* the bars a buffer holds: by TRACKS, fewer when they'd pass 12 s at this tempo */
static uint32_t gr_bars(void)
{
    uint32_t nt = gr_ntrk(), b = nt >= 3u ? 1u : nt == 2u ? 2u : 4u, bar = 5292000u / (sys.bpm ? sys.bpm : 120u);
    while (b > 1u && b * bar > GR_BUF_MAX)
        b /= 2u;
    return b;
}

/* a bar in output samples at the tempo now (4 beats) */
static uint32_t gr_bar_out(void) { return 10584000u / (sys.bpm ? sys.bpm : 120u); }

/* track t's buffer as a view (what grains read) */
static void gr_buf_view(uint32_t t, tape_view_t *v)
{
    gr_buf_t *B = &gbuf[t];
    uint32_t cap = (uint32_t)B->nch * TAPE_CHS;
    memset(v, 0, sizeof *v);
    v->map = B->map;
    v->len = B->len < cap ? B->len : cap;
    v->ram = 1;
}

/* ----------------------------------------------------------- the buffer --- */
/* the staged block back into its chunk (dropped when the buffer has shrunk past it meanwhile) */
/* a stored block's peak as ima_peak keeps it, of the sound: twice what's stored (gr_record) */
static uint8_t gr_peak(const int16_t *s)
{
    uint32_t i;
    int32_t pk = 0;
    for (i = 0; i < TAPE_BLK; i++) {
        int32_t a = s[i] < 0 ? -s[i] : s[i];
        if (a > pk)
            pk = a;
    }
    return (uint8_t)(pk >> 6 > 255 ? 255 : pk >> 6);
}

static void gr_bcommit(gr_buf_t *B)
{
    mem_chunk_t *c;
    uint32_t b = B->sblk;
    if (!B->staged)
        return;
    B->staged = 0;
    if (b / MEM_CB >= B->nch || b * TAPE_BLK >= B->len)
        return;
    B->cver++;                                          /* (the grains reading it: their windows are stale) */
    B->cblk = b;
    c = mem_at(B->map[b / MEM_CB]);
    ima_fit(B->stage, &c->pred[b % MEM_CB], &c->idx[b % MEM_CB]);
    ima_enc(B->stage, c->pred[b % MEM_CB], c->idx[b % MEM_CB], c->data[b % MEM_CB], TAPE_BLK);
    {
        uint8_t pk = gr_peak(B->stage);
        if (B->loud_len == B->len && B->loud_nch == B->nch)   /* (the count follows the block: loud now, or not) */
            B->nloud = (uint16_t)(B->nloud + (pk != 0) - (c->peak[b % MEM_CB] != 0));
        c->peak[b % MEM_CB] = pk;
    }
}

/* audio ISR: track t's buffer has sound in it (its loud blocks, counted again when it has been resized) */
static int gr_buf_loud(uint32_t t)
{
    gr_buf_t *B = &gbuf[t];
    uint32_t len = B->len, nch = B->nch, b, nb, c = 0;
    if (!len || !nch)
        return 0;
    if (B->loud_len != len || B->loud_nch != nch) {    /* (sized again: once, every block's peak) */
        nb = len / TAPE_BLK;
        nb = nb < nch * MEM_CB ? nb : nch * MEM_CB;
        for (b = 0; b < nb; b++)
            c += mem_at(B->map[b / MEM_CB])->peak[b % MEM_CB] != 0;
        B->nloud = (uint16_t)c;
        B->loud_len = len;
        B->loud_nch = (uint8_t)nch;
    }
    return B->nloud != 0;
}

/* one sample x into the buffer at the write head: what's there x FDBK % + x */
static void gr_bput(gr_buf_t *B, int32_t x, int32_t fb)
{
    uint32_t b = (uint32_t)B->w / TAPE_BLK, o = (uint32_t)B->w % TAPE_BLK;
    if (!B->staged || B->sblk != b) {
        mem_chunk_t *c = mem_at(B->map[b / MEM_CB]);
        gr_bcommit(B);
        ima_dec(c->data[b % MEM_CB], c->pred[b % MEM_CB], c->idx[b % MEM_CB], B->stage, TAPE_BLK);
        B->sblk = b;
        B->staged = 1;
    }
    B->stage[o] = (int16_t)clamp((fb ? B->stage[o] * fb / 100 : 0) + x, -32767, 32767);   /* (FDBK 0: no divide
                                                                                               * a buffer sample) */
    if (B->fill < (int32_t)B->len)
        B->fill++;
    if ((uint32_t)++B->w >= B->len)
        B->w = 0;
}

/* the source's block into the buffer at 22.05 kHz, as the tape, through a half-band low-pass (-1 0 9 16 9 0 -1)/32:
 * a pair of samples averaged let a hi-hat's top fold down into the buffer at -16 dB, a hiss under every grain; this
 * keeps it near -23 dB and folds it to the top, not the middle (three samples late). At half the source's level: a
 * source runs above full scale inside the chain (the factory scene's drums peak near 52,000), and the buffer's 16 bits
 * would clip it hard, every hit. A grain of the buffer plays at twice (gr_start), its peaks are kept at the sound's
 * own (gr_bcommit) */
static void gr_record(gr_buf_t *B, const int32_t *dry, uint32_t n, int32_t fb)
{
    int32_t x[6u + CTL];
    uint32_t i;
    for (i = 0; i < 6u; i++)
        x[i] = B->hx[i];
    for (i = 0; i < n; i++)
        x[6u + i] = dry[i];
    for (i = 0; i < n; i++) {
        B->odd ^= 1u;
        if (!B->odd) {                                  /* (centred three back from the newest, x[6 + i]) */
            const int32_t *c = &x[3u + i];
            gr_bput(B, sdiv2(16 * c[0] + 9 * (c[-1] + c[1]) - c[-3] - c[3], 6), fb);
        }
    }
    for (i = 0; i < 6u; i++)
        B->hx[i] = x[n + i];
}

/* ------------------------------------------------------------- reading --- */
/* decode samples dpos.. of view v into out (n of them; silence past its end), the decoder's state carried in locals;
 * each block starts from its stored state */
static void gr_decode(grain_t *g, const tape_view_t *v, int16_t *out, int32_t n)
{
    int32_t pred = g->pred, idx = g->idx, s = g->dpos, len = (int32_t)v->len, i;
    const uint8_t *d = s < len ? tv_data(v, (uint32_t)s / TAPE_BLK) : 0;
    for (i = 0; i < n; i++, s++) {
        uint32_t o = (uint32_t)s % TAPE_BLK;
        if (s >= len) {
            out[i] = 0;
            continue;
        }
        if (!o) {                                       /* a block's start: its data and its stored state */
            d = tv_data(v, (uint32_t)s / TAPE_BLK);
            pred = tv_pred(v, (uint32_t)s / TAPE_BLK);
            idx = tv_idx(v, (uint32_t)s / TAPE_BLK);
        }
        out[i] = (int16_t)ima_step(&pred, &idx, (d[o >> 1] >> ((o & 1u) * 4u)) & 15u);
    }
    g->pred = pred;
    g->idx = idx;
    g->dpos = s;
}

/* decode the window from sample lo (kept inside the view). Moving forward over what it holds, it keeps the overlap
 * and decodes on from where its decoder is (each sample decoded once); otherwise it starts at lo's block (its stored
 * state) and decodes up to lo first. */
static void gr_fill(grain_t *g, const tape_view_t *v, int32_t lo)
{
    static int16_t skip[TAPE_BLK];                      /* (what's decoded on the way to lo: thrown away) */
    int32_t len = (int32_t)v->len, i, keep;
    if (lo > len - (int32_t)GR_WIN)
        lo = len - (int32_t)GR_WIN;
    if (lo < 0)
        lo = 0;
    if (g->wlo >= 0 && lo > g->wlo && lo <= g->dpos && g->dpos == g->wlo + (int32_t)GR_WIN) {
        keep = g->dpos - lo;                            /* the forward slide: the overlap stays */
        for (i = 0; i < keep; i++)
            g->w[i] = g->w[i + lo - g->wlo];
        gr_decode(g, v, g->w + keep, (int32_t)GR_WIN - keep);
        g->wlo = lo;
        return;
    }
    g->dpos = lo / (int32_t)TAPE_BLK * (int32_t)TAPE_BLK;   /* from the block's start */
    if (lo > g->dpos)
        gr_decode(g, v, skip, lo - g->dpos);
    gr_decode(g, v, g->w, (int32_t)GR_WIN);
    g->wlo = lo;
}

/* the view at Q12 position p for grain g, interpolated (silence outside it) */
static inline int32_t gr_read(grain_t *g, const tape_view_t *v, int32_t p)
{
    int32_t i = p >> 12, f = p & 4095, a, b;
    if (i < 0 || i >= (int32_t)v->len)
        return 0;
    if (g->wlo < 0 || i < g->wlo || i + 1 >= g->wlo + (int32_t)GR_WIN)
        gr_fill(g, v, g->inc >= 0 ? i : i - (int32_t)GR_WIN + 2);
    a = g->w[i - g->wlo];
    b = i + 1 - g->wlo < (int32_t)GR_WIN ? g->w[i + 1 - g->wlo] : a;
    return a + (((b - a) * f) >> 12);
}

/* the window at output sample `at` of grain g (0 outside it), Q15: the ramps at both ends, flat between */
static inline int32_t gr_env(const grain_t *g, uint32_t at)
{
    if (at >= g->len)
        return 0;
    if (at < g->fade)
        return GR_RAMP[at * 128u / g->fade];
    if (g->len - at <= g->fade)
        return GR_RAMP[(g->len - at) * 128u / g->fade];
    return 32767;
}

/* PTCH + a random +-PRND, held to SCAL: 1/16 semitones */
static int32_t gr_pitch(grain_trk_t *G, const int16_t *p)
{
    int32_t st16 = mod_pitch16((uint32_t)(G - grain), FN_GRAIN, p[GP_PTCH]), sc = clamp(p[GP_SCAL], 0, 4), k, best = 0;
    int32_t bd = 1 << 20;                               /* (PTCH to 1/16 semitone when it's modulated: mod.c) */
    if (p[GP_PRND])
        st16 += (int32_t)(gr_rnd(G) % (uint32_t)(p[GP_PRND] * 32 + 1)) - p[GP_PRND] * 16;
    if (!sc)
        return st16;
    for (k = -36; k <= 36; k++) {                       /* the nearest semitone the scale keeps */
        int32_t d = k * 16 - st16;
        d = d < 0 ? -d : d;
        if (((GR_SCALE[sc] >> (uint32_t)((k % 12 + 12) % 12)) & 1u) && d < bd) {
            bd = d;
            best = k * 16;
        }
    }
    return best;
}

/* a grain's step for pitch st16 (1/16 semitone), Q12 samples per output sample: 2048 at the sound's own pitch */
static int32_t gr_rate(int32_t st16)
{
    return (int32_t)(pitch_inc((uint32_t)clamp(60 * 16 + st16, 0, 2047)) / (pitch_inc(60u * 16u) >> 11));
}

/* the gap before the next grain, output samples: RATE 0..100 -> 1..80 grains a second, in PATN's rhythm */
static int32_t gr_gap(grain_trk_t *G, const int16_t *p)
{
    int32_t gps100 = gr_gps100(p[GP_RATE]), t = 4410000 / gps100;
    switch (p[GP_PATN]) {
    case 0: return t;                                   /* EVEN */
    case 1: return G->n & 1u ? t * 2 / 3 : t * 4 / 3;   /* SWNG: long, short */
    case 2: return G->n % 4u == 3u ? t * 13 / 4 : t / 4;   /* CLST: three close, then a gap */
    default: return t / 4 + (int32_t)(gr_rnd(G) % (uint32_t)(t * 3 / 2 + 1));   /* RND */
    }
}

/* SYNC: the first beat (Q16) at or after `at` that a grain starts on: the grid of division dv in PATN's rhythm. EVEN
 * every step; SWNG the odd steps a third late; CLST three a quarter step apart, then the rest of four steps quiet;
 * RND each step's grain somewhere in it (the same place each time round: from the step's number) */
static uint32_t gr_snext(const int16_t *p, uint32_t dv, uint32_t at, uint32_t seed)
{
    uint32_t c, j, best = 0xFFFFFFFFu;
    switch (p[GP_PATN]) {
    case 0: return (at + dv - 1u) / dv * dv;
    case 2: {
        uint32_t g = at / (4u * dv);
        for (c = g; c < g + 2u; c++)
            for (j = 0; j < 3u; j++)
                if (c * 4u * dv + j * dv / 4u >= at && c * 4u * dv + j * dv / 4u < best)
                    best = c * 4u * dv + j * dv / 4u;
        return best;
    }
    default:
        for (c = at / dv; c < at / dv + 3u; c++) {
            uint32_t pt = c * dv + (p[GP_PATN] == 1 ? ((c & 1u) ? dv / 3u : 0u)
                                                    : ((c ^ seed) * 2654435761u >> 8) % dv);
            if (pt >= at && pt < best)
                best = pt;
        }
        return best;
    }
}

/* a new grain of track t around the cursor, in the window [ls, ls + ll) of what it reads */
/* what grains read from lo to hi (samples, either end past the view's: round its loop of ll) is silent: every block's
 * stored peak 0 (under 128: about -48 dB). Not the block being written (busy: its peak is the old sound's), nor more
 * than GR_QUIET_MAX blocks (then it's not worth the looking): those count as sound. A grain there would cost its work
 * to make nothing */
#define GR_QUIET_MAX 256
static int gr_quiet(const tape_view_t *v, int32_t lo, int32_t hi, int32_t ll, int32_t busy)
{
    int32_t len = (int32_t)v->len, b, nb;
    if (!len || ll <= 0 || hi < lo)
        return 0;
    nb = hi / (int32_t)TAPE_BLK - lo / (int32_t)TAPE_BLK + 1;
    if (nb > GR_QUIET_MAX)
        return 0;
    for (b = 0; b < nb; b++) {
        int32_t at = (lo / (int32_t)TAPE_BLK + b) * (int32_t)TAPE_BLK;
        while (at >= len)
            at -= ll;
        while (at < 0)
            at += ll;
        if (at / (int32_t)TAPE_BLK == busy || tv_peak(v, (uint32_t)at / TAPE_BLK))
            return 0;
    }
    return 1;
}

static void gr_start(uint32_t t, grain_trk_t *G, const int16_t *p, int32_t ls, int32_t ll, const tape_view_t *v,
                     int32_t busy)
{
    grain_t *g = 0;
    uint32_t k;
    int32_t at, sp, rate, gps100, ov, pan;
    if (grain_count(t) >= gr_allow(t))
        return;                                         /* all it may sound are sounding: this one is skipped */
    sp = ll * p[GP_SPRY] / 200;                         /* where it starts, and what it would read: all silence, it
                                                         * isn't started (no work, and its slot left to one heard) */
    at = (G->cur >> 12) + (sp ? (int32_t)(gr_rnd(G) % (uint32_t)(2 * sp + 1)) - sp : 0);
    at = ls + ((at - ls) % ll + ll) % ll;
    pan = gr_pitch(G, p);
    rate = gr_rate(pan);
    if (gr_quiet(v, at, at + (int32_t)((uint32_t)rate * ((uint32_t)p[GP_SIZE] * 441u / 10u) >> 12) + 1, ll, busy))
        return;
    for (k = 0; k < GR_SLOTS && !g; k++)                /* any free slot (the shares keep the total in the pool) */
        if (!gr_on(k))
            g = &gslot[k];
    if (!g)
        return;
    gr_tset(t, (uint32_t)(g - gslot));
    g->trk = (uint8_t)t;
    g->st16 = (int16_t)pan;                             /* (SPRY, PTCH, PRND: drawn above) */
    G->bto = (int16_t)(((pan % 192) + 192) % 192);      /* (BODY's wave plays it: its degree over ROOT) */
    g->rev = (uint8_t)((int32_t)(gr_rnd(G) % 100u) < p[GP_REV]);
    g->len = (uint32_t)p[GP_SIZE] * 441u / 10u;        /* SIZE ms -> output samples */
    g->fade = (uint32_t)p[GP_CONT] * g->len / 200u;    /* CONT: the ramps, up to half the grain each */
    if (g->fade < 88u)
        g->fade = 88u < g->len / 2u ? 88u : g->len / 2u;   /* (2 ms ends at the least: no click) */
    g->left = g->len;
    g->inc = g->rev ? -rate : rate;
    g->pos = (g->rev ? at + (int32_t)((uint32_t)rate * g->len >> 12) : at) << 12;   /* (backwards: from its end) */
    g->wlo = -1;
    g->wver = gbuf[t].cver;
    g->qb = -1;
    gps100 = gr_rate_gps(p);                            /* grains overlapping: a second's grains x SIZE, but no */
    ov = clamp(gps100 * p[GP_SIZE] / 100000, 1, clamp((int32_t)gr_allow(t), 1, 32));   /* more than it may sound */
    pan = p[GP_SPRD] ? (int32_t)(gr_rnd(G) % (uint32_t)(2 * p[GP_SPRD] + 1)) - p[GP_SPRD] : 0;
    g->gl = GR_NORM[ov] * (100 - pan) / 200;            /* (centre: each side half) */
    g->gr = GR_NORM[ov] * (100 + pan) / 200;
    if (clamp(p[GP_SCAN], 0, 3) != SCAN_TAPE) {        /* the buffer holds half the sound's level (gr_record): twice
                                                         * (65,534 at most: x 32,767 still fits 32 bits, gr_mix) */
        g->gl *= 2;
        g->gr *= 2;
    }
}

/* the buffer's cursor pace at WARP 100, Q12 samples per output sample: the buffer once round per its bars at the
 * tempo now (2048, as the tape, until the tempo changes under a frozen buffer) */
static int32_t gr_pace(const gr_buf_t *B, int32_t len)
{
    uint32_t bars = B->bars ? B->bars : 1u, bo = bars * gr_bar_out();
    return (int32_t)(((uint32_t)len << 4) / (bo >> 8));   /* (len x 4096 / bo, in 32 bits) */
}

/* audio ISR: GRAIN's BODY, a floor under the grains, into their half-rate mix (xl xr, h2 samples) before WET (the wet
 * side's): the source low-passed (SRC), or a wave (TRI SIN, -12 dBFS at 100) playing the grains' notes, through TONE's
 * one-pole low-pass, its level ramped across the block. The wave follows the pitch of the last grain started (PTCH,
 * PRND, SCAL's note) in the octave over ROOT: the grains' sound at its own pitch is ROOT, a fifth up is ROOT's fifth,
 * so it plays their scale, folded low enough to stay a floor. ROOT RES (its lowest value) takes RESONATOR's root
 * instead (PTCH, a white key, RES ARP's note): strings only ring at the notes they're tuned to, so BODY on its own
 * ROOT under a moving RES ARP rarely rang them (2026-10-11, from playing it: "the body sound is not flowing into the
 * resonator"); on RES it always does. The wave sounds while the grains do (sounding: any of
 * the track's grains this block): with none, stopped or silent, it closes in about 0.3 s (it droned on its own with
 * the transport stopped, 2026-10-11). SRC needs no gate: it is the source, silent with it. It glides there (1/16 of the way a block, about 12 ms):
 * a fast RATE blurs into a moving line, not a buzz of steps. Before any grain, ROOT itself; coming on from 0, straight
 * to the note. A new MODE: the old one down to 0 this block, the new from 0 the next (a 0.7 ms dip, no step). Returns 1 when it added something. Out of line: off (the default), grain_block's loops compile as
 * they did without it */
#define GR_ROOT_RES 23                /* ROOT's lowest, RES: RESONATOR's root (reso.c, after this file) */
static int32_t reso_root16(uint32_t t);
static __attribute__((noinline)) uint32_t gr_body(grain_trk_t *G, const int16_t *p, const int32_t *dry, int32_t *xl,
                                                  int32_t *xr, uint32_t h2, uint32_t sounding)
{
    uint32_t mode = (uint32_t)clamp(p[GP_BMOD], 0, 2), k;
    int32_t g1 = clamp(p[GP_BODY], 0, 100) * 32767 / 100, g0 = G->bg, ev = G->benv;
    ev = sounding ? ev + ((32767 - ev) >> 3) : ev - (ev >> 6);   /* (TRI SIN's gate: open in about 6 ms while the
                                                                  * grains sound, -60 dB about 0.3 s after them) */
    G->benv = ev = ev < 64 ? 0 : ev;
    if (mode != BODY_SRC)
        g1 = (int32_t)(((uint32_t)g1 * (uint32_t)ev) >> 15);
    if (!g0 && !g1 && mode == G->bmode)
        return 0;
    if (mode != G->bmode) {
        if (g0)
            g1 = 0;                                     /* (this block: the old one out) */
        else
            G->bmode = (uint8_t)mode;
    }
    G->bg = g1;
    if (!g0 && !g1)
        return 0;
    {
        int32_t wq = (int32_t)CUTOFF_HZ[clamp(p[GP_BTON], 0, 127)] * 9337 / 1000;   /* 2 pi f / 22050, Q15 */
        int32_t kq = (int32_t)(((uint32_t)wq << 15) / (uint32_t)(32768 + wq));        /* w / (1 + w) */
        int32_t dg = (g1 - g0) / (int32_t)h2, gk = g0, lp = G->blp;
        int32_t d = G->bto - G->bst;
        uint32_t ph = G->bph, inc;
        if (!g0)
            G->bst = G->bto;                            /* (coming on: straight to the note, no glide up to it) */
        else
            G->bst = (int16_t)(G->bst + (d >= 0 ? (d + 15) >> 4 : -((15 - d) >> 4)));
        {                                               /* ROOT, or RES: the strings' root (last block's, 0.7 ms) */
            int32_t r16 = p[GP_ROOT] <= GR_ROOT_RES ? reso_root16((uint32_t)(G - grain)) : clamp(p[GP_ROOT], 0, 127) * 16;
            inc = 2u * pitch_inc((uint32_t)clamp(r16 + G->bst, 0, 2047));
        }
        for (k = 0; k < h2; k++) {
            int32_t x, y;
            if (G->bmode == BODY_SRC) {
                x = clamp(dry[2u * k], -32767, 32767);  /* (held: the low-pass product fits 32 bits) */
            } else {
                ph += inc;
                if (G->bmode == BODY_TRI) {
                    x = (int32_t)(ph >> 16);
                    x = ((x < 32768 ? x : 65535 - x) - 16384) >> 1;
                } else {
                    x = sine_i(ph) >> 2;
                }
            }
            x = (x - lp) * kq;                          /* (TONE's new value toward 0, as RESONATOR's strings: a
                                                         * shift alone left a small offset for good after the sound
                                                         * stopped, and DEC 100's strings rang it up: reso.c rs_settle) */
            lp += x >> 15;
            lp += (lp >> 31) & (int32_t)((uint32_t)-(x & 32767) >> 31);
            gk += dg;
            y = (lp * (gk >> 3)) >> 12;
            xl[k] += y;
            xr[k] += y;
        }
        G->blp = lp;
        G->bph = ph;
    }
    return 1;
}

/* -------------------------------------------------------------- a block --- */
#define GR_DUCK_LOG2 5
#define GR_DUCK (1u << GR_DUCK_LOG2)        /* a seam's duck: a grain fades to 0 over this many samples of what it
                                             * reads either side of a jump in it (1.5 ms at its own pitch) */

/* how far a is from b round a loop of len samples (0..len/2) */
static inline int32_t gr_cdist(int32_t a, int32_t b, int32_t len)
{
    int32_t d = a - b;
    if (len) {                                          /* (as d % len, sign kept, no divide: d is within a few
                                                         * lengths of b, so a step or two) */
        while (d >= len)
            d -= len;
        while (d <= -len)
            d += len;
    }
    d = d < 0 ? -d : d;
    return len && d > len - d ? len - d : d;
}

/* audio ISR: h half-rate samples of grain g into ol and or: its window (e, Q20, moving de a sample), its pan (gl
 * gr), read through its decoded window (round the loop of ll at the ends of the view's len). duck: the read passes
 * near the seam this block, the zone of zh either side of seam where what it reads jumps or is about to (the block the
 * buffer is writing, the tape's end): inside it the grain is silent, and over GR_DUCK either side it fades down and
 * back up, a dip instead of a click; sv: the zone moves a sample a half-rate sample (the write head's pace). Inlined
 * with duck a constant: a grain nowhere near it runs the loop it always did */
static inline __attribute__((always_inline)) void gr_mix(grain_t *g, const tape_view_t *v, int32_t len, int32_t ll,
                                                        int32_t h, int32_t e, int32_t de, int32_t gl, int32_t gr,
                                                        int32_t *ol, int32_t *or, int duck, int32_t seam, int32_t zh,
                                                        int32_t sv)
{
    int32_t pos = g->pos, inc = 2 * g->inc, wlo = g->wlo, half = len / 2, i;
    const int16_t *w = g->w;
    for (i = 0; i < h; i++, e += de, pos += inc) {      /* (the grain's state in locals: the mix can't alias it) */
        int32_t ix = (pos >> 12) - wlo, x, a, b;
        if (wlo >= 0 && ix >= 0 && ix + 1 < (int32_t)GR_WIN) {   /* (the usual case: inside the window) */
            a = w[ix];
            b = w[ix + 1];
            x = a + (((b - a) * (pos & 4095)) >> 12);
        } else {
            if (pos < 0 || (pos >> 12) >= len)          /* (off either end: round the loop, as the buffer and the
                                                         * tape go round, not silence: a grain cut mid-way clicks) */
                pos += pos < 0 ? ll << 12 : -(ll << 12);
            x = gr_read(g, v, pos);
            wlo = g->wlo;
        }
        if (duck) {                                     /* (the seam: silent inside the zone, the window's own curve
                                                         * down to it and up from it, by the distance in Q12) */
            int32_t ds = pos - ((seam + sv * i) << 12);  /* (the zone moving with the head: sv a sample) */
            ds = ds > half << 12 ? ds - (len << 12) : ds < -(half << 12) ? ds + (len << 12) : ds;
            ds = (ds < 0 ? -ds : ds) - (zh << 12);
            if (ds < (int32_t)GR_DUCK << 12)
                x = ds <= 0 ? 0 : (x * GR_RAMP[ds >> (GR_DUCK_LOG2 + 12 - 7)]) >> 15;
        }
        x = (x * (e >> 5)) >> 15;
        ol[i] += (x * gl) >> 15;
        or[i] += (x * gr) >> 15;
    }
    g->pos = pos;
}

/* audio ISR: track t's GRAIN on one block. dry: the source (n samples); keys: the white keys on the GRAIN page (0
 * otherwise); frozen: the 0 key held. The track's stereo out into l and r. */
static void grain_block(uint32_t t, const int32_t *dry, uint32_t keys, int frozen, int32_t *l, int32_t *r, uint32_t n)
{
    grain_trk_t *G = &grain[t];
    gr_buf_t *B = &gbuf[t];
    const int16_t *p = TPD(t, DEV_GRAIN);
    tape_view_t v;
    int32_t ls = 0, ll = 0, wet = p[GP_WET], i, len, live = tp[t].src != SRC_TAPE, playing = sys.playing, rec = 0;
    uint32_t k, j, mw, press = keys & ~G->keys, scan = (uint32_t)clamp(p[GP_SCAN], 0, 3);
    G->keys = keys;
    if (!G->rng)
        G->rng = 0x2545F491 + (int32_t)t * 7919;
    grain_frozen = (uint8_t)frozen;
    {                                                   /* the dry share, centred: INS 1 - WET, SEND all of it */
        int32_t d1 = p[GP_ROUT] ? 32767 : PCT_Q15[100 - clamp(wet, 0, 100)], d0 = G->dset ? G->dsh : d1;
        G->dsh = d1;
        G->dset = 1;
        if (d0 != d1) {                                  /* (ROUT or WET moved: ramped, no click) */
            int32_t dd = (d1 - d0) >> CTL_LOG2;
            for (i = 0; i < (int32_t)n; i++) {          /* (stepped first: the block's last sample at d1, within
                                                         * rounding, so the next block's start doesn't step) */
                d0 = i + 1 == (int32_t)n ? d1 : d0 + dd;
                l[i] = r[i] = (dry[i] * (d0 >> 3)) >> 12;
            }
        } else if (p[GP_ROUT]) {
            for (i = 0; i < (int32_t)n; i++)
                l[i] = r[i] = dry[i];
        } else if (wet) {                               /* (a gain, not a divide a sample: the target's compiler
                                                         * keeps x / 100 a hardware divide, and this runs on every
                                                         * track, always) */
            for (i = 0; i < (int32_t)n; i++)
                l[i] = r[i] = (dry[i] * (d1 >> 3)) >> 12;
        } else {                                        /* (WET 0: the dry as it is) */
            for (i = 0; i < (int32_t)n; i++)
                l[i] = r[i] = dry[i];
        }
    }
    if (B->len != B->flen) {                            /* (resized: what's behind the head, back to 0, is this pass's;
                                                         * past it, the new end holds nothing of it) */
        B->flen = B->len;
        B->fill = B->fill < B->w ? B->fill : B->w;
    }
    if (!B->len)
        B->fill = 0;
    if (scan != SCAN_TAPE && B->len && B->nch) {        /* the buffer: the bar line at the transport's start */
        if (B->w >= (int32_t)B->len)                    /* (shrunk under it: the same place in the bars) */
            B->w %= (int32_t)B->len;
        if (playing && !B->was_playing) {
            gr_bcommit(B);
            B->w = 0;
            B->odd = 0;
            B->fill = 0;
            for (i = 0; i < 6; i++)
                B->hx[i] = 0;
        }
        if (!frozen && wet && (playing || live)) {      /* it records: what's there x FDBK + the source */
            gr_record(B, dry, n, clamp(p[GP_FDBK], 0, 100));
            rec = 1;
        }
        else if (B->staged)
            gr_bcommit(B);
    }
    B->was_playing = (uint8_t)playing;
    if (scan == SCAN_TAPE) {                            /* what grains read: the tape, or the buffer */
        tape_view(t, &v);
        len = (int32_t)v.len;
        if (len)
            tape_window(t, v.len, &ls, &ll);
    } else {
        gr_buf_view(t, &v);
        len = (int32_t)v.len;
        ll = len;
    }
    if (!len) {                                         /* nothing to read: no grains */
        grain_kill(t);
        return;
    }
    if (scan != G->scan) {                              /* a new SCAN: the cursor starts over */
        G->scan = (uint8_t)scan;
        G->cur = ls << 12;
        G->kofs = -1;
    }
    if (G->cur < ls << 12 || G->cur >= (ls + ll) << 12)
        G->cur = ls << 12;
    if (p[GP_OFST] != G->ofst_seen)                     /* OFST turned: it takes over from a key */
        G->kofs = -1;
    for (k = 0; press && k < NWHITE; k++)                /* a key: the cursor to its sixteenth */
        if ((press >> k) & 1u) {
            G->kofs = ll * (int32_t)k / 16;
            G->ofst_seen = p[GP_OFST];
            G->cur = (ls + G->kofs) << 12;
            G->wait = 0;
            break;
        }
    switch (scan) {                                     /* the cursor */
    case SCAN_TAPE:                                     /* WARP 100 %: the tape's own pace; frozen or stopped, still */
        G->cur += frozen || !(playing || keys) ? 0 : 2048 * p[GP_WARP] / 100 * (int32_t)n;
        break;
    case SCAN_STR:                                      /* WARP against the write head; frozen: the loop plays on */
        if (frozen || playing || live || keys)
            G->cur += gr_pace(B, len) * p[GP_WARP] / 100 * (int32_t)n;
        break;
    case SCAN_POS:                                      /* still, at OFST (or a key's spot) */
        G->cur = (G->kofs >= 0 ? G->kofs : len * p[GP_OFST] / 100) << 12;
        break;
    default: {                                          /* DLY: OFST (or a key's sixteenth) behind the write head */
        int32_t lag = G->kofs >= 0 ? G->kofs : len * p[GP_OFST] / 100;
        if (lag < 2 * (int32_t)TAPE_BLK)
            lag = 2 * (int32_t)TAPE_BLK;                /* (not into the block being written) */
        if (frozen)                                     /* (the write head stopped: the trail runs on, a loop) */
            G->cur += gr_pace(B, len) * (int32_t)n;
        else
            G->cur = (B->w - lag) << 12;
        break;
    }
    }
    while (G->cur >= (ls + ll) << 12)
        G->cur -= ll << 12;
    while (G->cur < ls << 12)
        G->cur += ll << 12;
    if (scan == SCAN_STR && rec) {                      /* STR never starts grains at the seam, where the newest sound
                                                         * meets the bars-old: the cursor began on the write head at
                                                         * PLAY, and WARP's pace and the head's drift apart. It keeps
                                                         * dd behind the head: a grain's length, SPRY's scatter ahead
                                                         * of the cursor and two blocks, at most half the buffer, once
                                                         * that much is recorded since PLAY (till then, as it was:
                                                         * nothing older to read). So a grain
                                                         * at its own pitch ends short of the head (one pitched up:
                                                         * the seam's duck, gr_mix). Closer than 3/4 of that, or about
                                                         * to be overrun (WARP under 100), it goes back to dd behind.
                                                         * Only where new grains start moves: those sounding play on */
        int32_t dd = 2 * (int32_t)TAPE_BLK + 22 * p[GP_SIZE] + ll * p[GP_SPRY] / 200, d = B->w - (G->cur >> 12);
                                                        /* (SIZE ms x 22.05, and SPRY's scatter: gr_start) */
        if (dd > len >> 1)
            dd = len >> 1;
        if (d < 0)
            d += len;
        if (B->fill >= dd && (d < dd - (dd >> 2) || d > len - (dd >> 1)))   /* (a quarter's slack: one move in a
                                                         * minute for the drift) */
            G->cur = (B->w >= dd ? B->w - dd : B->w + len - dd) << 12;
    }
    if (!wet && !G->wetq) {                             /* (dry only, the grains faded out last block: they stop,
                                                         * the cursor goes on; the 2x up's last three samples still
                                                         * play out, below, or their cut is a click) */
        grain_kill(t);
        if (G->hq)
            return;
    }
    if (p[GP_SYNC]) {                                   /* SYNC: on the tempo's grid, from the beat clock (mod.c) */
        uint32_t dv = gr_sdiv(p), b0 = mclk.beat, b1 = b0 + mclk.inc + 1u;
        if (!G->sync_on || G->sb + 8u * dv < b0 || G->sb > b1 + 8u * dv) {   /* (from now: started, or the beat
                                                         * jumped: PLAY) */
            G->sb = gr_snext(p, dv, b0, t);
            G->sync_on = 1;
        }
        G->wait = 1;                                    /* (the free clock waits) */
        if (G->sb < b1) {
            if (wet && (playing || keys || (scan != SCAN_TAPE && (live || frozen)))) {
                gr_start(t, G, p, ls, ll, &v, scan != SCAN_TAPE && B->staged ? (int32_t)B->sblk : -1);
                G->n++;
            }
            G->sb = gr_snext(p, dv, G->sb + 1u, t);
            if (G->sb < b1)                             /* (one a block: past it, the next from the next block) */
                G->sb = gr_snext(p, dv, b1, t);
        }
    } else {
        G->sync_on = 0;
        G->wait -= (int32_t)n;
    }
    if (!p[GP_SYNC] && G->wait <= 0 && wet && (playing || keys || (scan != SCAN_TAPE && (live || frozen)))) {
        gr_start(t, G, p, ls, ll, &v, scan != SCAN_TAPE && B->staged ? (int32_t)B->sblk : -1);
        G->n++;
        G->wait += gr_gap(G, p);
        if (G->wait < (int32_t)n)
            G->wait = (int32_t)n;
    }
    {                                                   /* the grains, at half rate (the buffer's and the tape's
                                                         * 22.05 kHz: nothing in them above 11 kHz), into xl xr
                                                         * after the last block's three; then 2x up into l and r */
        int32_t xl[3u + CTL / 2u], xr[3u + CTL / 2u], seam, zh, busy;   /* (rec: the buffer recorded this block) */
        uint32_t h2 = n / 2u, any = 0;
        for (k = 0; k < 3u; k++) {
            xl[k] = G->hl[k];
            xr[k] = G->hr[k];
        }
        for (k = 0; k < h2; k++)
            xl[3u + k] = xr[3u + k] = 0;
        /* where what it reads jumps, or is about to: recording, the TAPE_BLK behind the write head (the block being
         * written is stored when it's full: until then a grain reads a bar ago's sound there, after it the new) up
         * to the head (the newest against the oldest), moving with the head a sample a half-rate sample (sv), from
         * where it was at the block's start; the buffer still, its head; the tape, its end (round to its loop) */
        rec = rec && scan != SCAN_TAPE;
        zh = rec ? (int32_t)(TAPE_BLK / 2u) : 0;
        seam = scan == SCAN_TAPE ? 0 : rec ? B->w - (int32_t)h2 - zh : B->w;
        busy = scan != SCAN_TAPE && B->staged ? (int32_t)B->sblk : -1;   /* (the block being written: its peak
                                                                          * isn't the sound's yet) */
        if (len) {                                      /* (into [0, len): as (seam % len + len) % len, no divide) */
            while (seam < 0)
                seam += len;
            while (seam >= len)
                seam -= len;
        }
        for (j = 0; j < 2u; j++)                        /* the track's own slots, lowest first (as the 40 were) */
        for (mw = gr_trk[t][j]; mw; mw &= mw - 1u) {
            grain_t *g;
            int32_t e0, e1, gl, gr, m, h;
            k = j * 32u + gr_low(mw);
            g = &gslot[k];
            any = 1;
            if (scan != SCAN_TAPE && g->wver != B->cver)  /* (a block stored: its silence looked at again) */
                g->qb = -1;
            if (scan != SCAN_TAPE && g->wver != B->cver) {   /* a block was stored since its window was decoded: if
                                                         * the window holds it, decode again (else a bar ago's sound
                                                         * meets the new inside the window) */
                int32_t c0 = (int32_t)(B->cblk * TAPE_BLK);
                if (g->wlo >= 0 && g->wlo < c0 + (int32_t)TAPE_BLK && g->dpos > c0)
                    g->wlo = -1;
                g->wver = B->cver;
            }
            m = (int32_t)(g->left < n ? g->left : n);
            h = (m + 1) >> 1;                           /* (half-rate samples: a grain ending mid-block rounds up) */
            e0 = gr_env(g, g->len - g->left);           /* the window: its value at the block's ends, ramped */
            e1 = gr_env(g, g->len - g->left + (uint32_t)m);
            gl = g->gl;                                 /* (WET: on the mix, below) */
            gr = g->gr;
            {
                int32_t inc = 2 * g->inc, p0 = g->pos >> 12, p1 = (g->pos + inc * h) >> 12, span = p1 - p0;
                int32_t de = ((e1 - e0) * 32) >> (CTL_LOG2 - 1);
                span = span < 0 ? -span : span;
                int32_t b0 = sdiv2(p0 < p1 ? p0 : p1, 8), b1 = sdiv2((p0 < p1 ? p1 : p0) + 1, 8);   /* (/ TAPE_BLK) */
                int q;
                if (b0 == b1 && b0 == g->qb && b0 != busy && p0 >= 0 && p1 < len)
                    q = g->qq;                          /* (still inside the block it last looked at: the same) */
                else {
                    q = gr_quiet(&v, p0 < p1 ? p0 : p1, (p0 < p1 ? p1 : p0) + 1, ll, busy);
                    g->qb = b0 == b1 && b0 != busy && p0 >= 0 && p1 < len ? b0 : -1;
                    g->qq = (uint8_t)q;
                }
                if (q) {                                /* (silence this block:
                                                                                     * no decoding, no mixing; it
                                                                                     * moves on, its window to be read
                                                                                     * again where there's sound) */
                    int32_t pos = g->pos + inc * h;
                    if (pos < 0 || (pos >> 12) >= len)
                        pos += pos < 0 ? ll << 12 : -(ll << 12);
                    g->pos = pos;
                    g->wlo = -1;
                } else if (gr_cdist(p0, seam, len) <= span + zh + (int32_t)(GR_DUCK + h2) ||
                    gr_cdist(p1, seam, len) <= span + zh + (int32_t)(GR_DUCK + h2))   /* (near the seam this block:
                                                                                     * ducked) */
                    gr_mix(g, &v, len, ll, h, e0 << 5, de, gl, gr, xl + 3, xr + 3, 1, seam, zh, rec);
                else
                    gr_mix(g, &v, len, ll, h, e0 << 5, de, gl, gr, xl + 3, xr + 3, 0, 0, 0, 0);
            }
            g->left -= (uint32_t)m;
            if (!g->left)
                gr_tclr(t, k);
        }
        any |= gr_body(G, p, dry, xl + 3, xr + 3, h2, any);   /* BODY: a floor under the grains (out of line) */
        {                                               /* WET on the track's grains together, ramped across the block
                                                         * from last block's (no step when it's turned or modulated;
                                                         * turned to 0, they fade out over this block, then stop).
                                                         * The ramp ends three half-rate samples early, at w1: the 2x
                                                         * up plays them three late, so the block's last output is
                                                         * at w1, where the next block's starts */
            int32_t w1 = PCT_Q15[clamp(wet, 0, 100)], w0 = G->wetq, gw = w0;
            int32_t dw = w1 == w0 || h2 <= 3u ? 0 : (w1 - w0) / (int32_t)(h2 - 3u);   /* (a divide only while WET
                                                         * moves) */
            if (any && (w0 != 32767 || w1 != 32767))
                for (k = 0; k < h2; k++) {
                    gw = k + 4u > h2 ? w1 : gw + dw;
                    xl[3u + k] = (xl[3u + k] * (gw >> 3)) >> 12;
                    xr[3u + k] = (xr[3u + k] * (gw >> 3)) >> 12;
                }
            G->wetq = w1;
        }
        if (any || !G->hq) {                            /* 2x up, a cubic's midpoint (-1 9 9 -1)/16 between each pair,
                                                         * two half-rate samples late; then their tail carried on */
            for (k = 0; k < h2; k++) {
                const int32_t *a = xl + k, *b = xr + k;
                l[2u * k] += a[1];
                l[2u * k + 1u] += (9 * (a[1] + a[2]) - a[0] - a[3]) >> 4;
                r[2u * k] += b[1];
                r[2u * k + 1u] += (9 * (b[1] + b[2]) - b[0] - b[3]) >> 4;
            }
            for (k = 0; k < 3u; k++) {
                G->hl[k] = xl[h2 + k];
                G->hr[k] = xr[h2 + k];
            }
            G->hq = !any && !xl[h2] && !xl[h2 + 1u] && !xl[h2 + 2u] && !xr[h2] && !xr[h2 + 1u] && !xr[h2 + 2u];
        }
    }
}

/* audio ISR: shedding takes grains first (the pool a quarter smaller at a time, down to half); a second without it
 * gives an eighth back */
static void grain_shed(void)
{
    if (grain_cap > 4u)
        grain_cap -= 2u;
    grain_calm = 0;
}
/* audio ISR, each block: a second after the last shed, an eighth of the pool back, if there's room for it (the
 * heaviest half of the last second under 70 %: chain_room); no room, another second */
static void grain_recover(void)
{
    if (++grain_calm >= 1378u && grain_cap < GR_CAP) {   /* (1378 blocks: a second) */
        if (chain_room())
            grain_cap++;
        grain_calm = 0;
    }
}

/* ------------------------------------------------------ main loop side --- */
/* track t's buffer to n chunks (taking from tapes as tape_steal allows: a device switched on takes from the end of
 * the longest tape) and len samples (no more than its chunks hold); shrinking: the length first, then the chunks */
static void gr_buf_size(uint32_t t, uint32_t n, uint32_t len, uint32_t bars)
{
    gr_buf_t *B = &gbuf[t];
    while (B->nch < n) {
        int32_t c = tape_alloc(MEM_GRAIN + t, NTRK, 1);
        if (c < 0)
            break;
        B->map[B->nch] = (uint8_t)c;
        RING_PUBLISH();
        B->nch++;
    }
    if (len > (uint32_t)B->nch * TAPE_CHS)
        len = (uint32_t)B->nch * TAPE_CHS;
    if (len > n * TAPE_CHS)
        len = n * TAPE_CHS;
    B->bars = (uint8_t)bars;
    B->len = len;
    RING_PUBLISH();
    while (B->nch > n) {
        uint32_t c = B->map[B->nch - 1u];
        B->nch--;
        mem_free(c);
    }
}

/* main loop, every pass: each track's buffer as long as its bars (1, 2 or 4 by TRACKS, at the tempo) while GRAIN is
 * on and SCAN reads it, none otherwise (a parked track, WET 0, SCAN TAPE). A frozen buffer keeps its size. */
static void grain_poll(void)
{
    uint32_t t, bars = gr_bars(), len = bars * (5292000u / (sys.bpm ? sys.bpm : 120u));
    for (t = 0; t < NTRK; t++) {
        const int16_t *p = tp[t].dev[DEV_GRAIN];
        int on = trk_live(t) && mod_peak_dev(t, DEV_GRAIN, GP_WET) > 0 && p[GP_SCAN] != SCAN_TAPE;   /* (WET 0 but
                                                                                                * modulated up: on) */
        if (grain_frozen && on && gbuf[t].len)
            continue;
        if (on) {
            if (gbuf[t].len != len || gbuf[t].nch * TAPE_CHS < len)
                gr_buf_size(t, (len + TAPE_CHS - 1u) / TAPE_CHS, len, bars);
        } else if (gbuf[t].nch) {
            gr_buf_size(t, 0, 0, bars);
        }
    }
}
