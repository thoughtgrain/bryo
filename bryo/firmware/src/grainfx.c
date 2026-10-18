/* SPDX-License-Identifier: GPL-3.0-only */
/* Bryo: GRAIN's WARBLE and VINYL forms (2026-10-18; docs/bryo-architecture.md, "GRAIN's WARBLE and VINYL").
 *
 * "I'd like the warbler to be something that tapes could use as well. Maybe it's a new granular engine like we have
 * different filters? And we could have a vinyl granular." GRAIN's knob FORM (picked as RIPPLE's form is, EDIT held +
 * SELECT on GRAIN's pages) makes it GRAIN, WARBLE or VINYL; APPL says where the form acts:
 *   FLOW  the whole sound as it flows through the device, at the output rate: WET mixes it in as it does the grains
 *         (ROUT INS crossfades, SEND adds). No grains start.
 *   GRNS  the grains as the GRAIN form sets them (SIZE RATE and the rest keep their values), their mix at half rate,
 *         after BODY and before WET.
 * WARBLE: a line a few milliseconds long read at a moving delay, a tape's wow (a slow wander, a sine and a random
 * glide) and flutter (two fast sines), AGE darker and saturated; SWEL: each hit kicks the wobble deeper and it
 * settles (a tape that lurches as the sound lands), at 100 only the hits wobble.
 * VINYL: the same line at a record's turn (33 1/3: 1.8 s, its warp at WOW), WEAR (darker, thinner, saturated), the
 * surface's hiss (NOIS) and crackle (CRKL: dust ticks and the odd pop) while something plays, and the needle's
 * glitches: at each turn, GLCH's chance of one of TYPE: SKIP (the last turn once more, a jump back a groove), STCK
 * (a stuck groove: the last turn two to four times) or BRAK (a hand on the platter: down to a stop and back). On the
 * whole sound a glitch reads GRAIN's own buffer of the last bars (it records as it always has); on the grains it
 * moves their cursor.
 * The line is one chunk of the shared sound memory (mem.c), taken while the form is on (gfx_poll), as RIPPLE's
 * strings are: 1,056 samples, 24 ms at 44.1 kHz on FLOW, two of 528 at the half rate on GRNS. Integer only. */

#define GX_LINE (sizeof(mem_chunk_t) / 2u)          /* the line: 1,056 samples */
#define GX_TURN 79380                                /* a record's turn at 33 1/3, output samples (1.8 s) */
#define GX_XF 441                                    /* a glitch fades in and out over 10 ms */

typedef struct {
    uint8_t map;                     /* the line's chunk (the main loop's) */
    volatile uint8_t nch;            /* 1: it's there */
    /* the ISR's */
    uint8_t appl;                    /* APPL last block (a change: the line starts silent) */
    uint32_t w;                      /* the line's write index */
    uint32_t wph, fph, rph;          /* the wow's, the flutter's and the record's turn phases */
    int32_t wn, wnt;                 /* the wow's random glide: where it is, where it's going (Q15) */
    int32_t kick, msm;               /* SWEL: the last hit's kick (Q15, settling), the depth it gives (smoothed) */
    int32_t es;                      /* the input's slow level (the hits stand out of it) */
    int32_t d0;                      /* the delay at the block's end, Q8 samples (the next block ramps from it) */
    int32_t lp[2], hp[2];            /* AGE / WEAR: its low-pass, its high-pass (VINYL), per side (Q8) */
    int32_t sh[4];                   /* .. last block's low-pass, curve's share, -, high-pass (a block ramps from them:
                                      * AGE modulated doesn't step) */
    int32_t nl[2];                   /* the hiss's colour */
    int32_t tick, pop;               /* a dust tick ringing, a pop's thump */
    int32_t rng;
    int32_t wetq;                    /* FLOW: WET last block, Q15 */
    uint32_t turn;                   /* output samples into the record's turn */
    uint8_t gk;                      /* a glitch: 0 none, 1 + GT_* */
    uint8_t reps;                    /* .. STCK: turns still to repeat */
    int32_t gleft, glen;             /* .. output samples left in this pass, a pass's length */
    int32_t gpos, gstart, ginc;      /* .. the reader: where (Q12, the buffer's samples), a pass's start, its step */
    int32_t gx;                      /* .. the reader's share, Q15 (faded in and out) */
    int32_t gcur;                    /* .. GRNS: the cursor a stuck groove goes back to */
    grain_t rd;                      /* .. FLOW: the reader's window into GRAIN's buffer */
} gfx_t;
static gfx_t gfx[NTRK];

/* audio ISR, a block: the delay the line is read at by its end (Q8 samples, at the rate the line runs at: half on
 * GRNS); the wow, flutter and turn moving on n samples */
static int32_t HOT_CODE gfx_delay(gfx_t *X, const int16_t *p, uint32_t form, int half, uint32_t n)
{
    int32_t base = half ? 220 : 440, wow, fl, m, d;
    uint32_t sc = half ? 2u : 1u, was = X->wph;
    X->wph += 58450u * n * sc;                          /* (0.6 Hz: 2^32 x 0.6 / 44100) */
    X->fph += 711000u * n * sc;                         /* (7.3 Hz) */
    X->rph += 54105u * n * sc;                          /* (0.5556 Hz: 33 1/3 a minute) */
    if (X->wph < was)                                   /* (once a wow: a new place for the random glide) */
        X->wnt = (int32_t)(noise32(&X->rng) >> 17) - 16384;
    X->wn += (X->wnt - X->wn) >> 9;
    if (form == GF_VINYL) {                             /* a record: its warp, once a turn */
        wow = (sine_i(X->rph) * 3 + X->wn) >> 2;
        wow = (wow * clamp(p[GP_WOW], 0, 100)) / 100;   /* (a block's divide, not a sample's) */
        return (base << 8) + ((wow * (half ? 150 : 300)) >> 7);
    }
    wow = ((sine_i(X->wph) * 7 + X->wn * 3) >> 3) * clamp(p[GP_WOW], 0, 100) / 100;
    fl = ((sine_i(X->fph) * 3 + sine_i(X->fph * 3u / 2u)) >> 2) * clamp(p[GP_FLUT], 0, 100) / 100;
    {                                                   /* SWEL: each hit kicks the depth up, then it settles */
        int32_t sw = clamp(p[GP_SWEL], 0, 100), mt = ((100 - sw) * 32767 + sw * ((X->kick * 3) >> 1)) / 100;
        X->msm += (mt - X->msm) >> 4;
        m = X->msm;
        X->kick -= X->kick >> 8;
    }
    d = (wow * (half ? 200 : 400) + fl * (half ? 11 : 22)) >> 7;   /* (Q8: at 100, +-400 and +-22 samples) */
    d = (base << 8) + ((d * (m >> 4)) >> 11);           /* (m: to 1.5, Q15) */
    return clamp(d, 1 << 8, (int32_t)((half ? GX_LINE / 2u : GX_LINE) - 2u) << 8);   /* (a kick at 1.5 on WOW 100
                                                         * reaches past the base: held at the line's ends, not past) */
}

/* audio ISR: the input's level this block (the hits SWEL kicks on) */
static void HOT_CODE gfx_level(gfx_t *X, const int32_t *x, uint32_t n, uint32_t step)
{
    int32_t pk = 0, i;
    for (i = 0; i < (int32_t)n; i += (int32_t)step) {
        int32_t a = x[i] < 0 ? -x[i] : x[i];
        pk = a > pk ? a : pk;
    }
    if (pk > 1500 && pk > 2 * X->es + 400)              /* (a hit: well over the level so far) */
        X->kick = 32767;
    X->es += (pk - X->es) >> 6;
}

/* the knobs' per-block numbers: sh[], the low-pass, the curve's share (Q15), the high-pass, their step a sample from
 * last block's (lg: the block's samples, log 2) and where the block starts; the surface's chances */
static void gfx_shape(gfx_t *X, const int16_t *p, uint32_t form, int half, uint32_t lg, int32_t *sh, int32_t *dsh,
                      uint32_t *ct, uint32_t *cp)
{
    int32_t a = clamp(p[GP_AGE], 0, 100), c = clamp(p[GP_CRKL], 0, 100), to[4], k;
    to[0] = 32767 - a * (half ? 220 : 270);             /* (AGE 100: about 1.3 kHz) */
    to[1] = a * 327;                                    /* (the curve's share) */
    to[2] = 0;
    to[3] = form == GF_VINYL ? (half ? 400 : 200) + a * (half ? 8 : 4) : 0;   /* (40 to 160 Hz) */
    if (!X->sh[0])                                      /* (the first block: from here) */
        for (k = 0; k < 4; k++)
            X->sh[k] = to[k];
    for (k = 0; k < 4; k++) {                           /* (toward 0: a ramp never passes its end, a coefficient never
                                                         * goes below 0; a block's divide) */
        sh[k] = X->sh[k];
        dsh[k] = (to[k] - X->sh[k]) / (int32_t)(1u << lg);
        X->sh[k] = to[k];
    }
    *ct = (uint32_t)(c * c) * (half ? 2920u : 1460u);   /* (dust: about 150 ticks a second at 100) */
    *cp = (uint32_t)(c * c) * (half ? 156u : 78u);      /* (pops: about 8 a second) */
}
/* BRAK's pace at e output samples in, Q10: 0.35 s down to a stop, 0.15 s still, 0.35 s back up (68/1024: 1/15,435) */
static inline __attribute__((always_inline)) int32_t gfx_brake(int32_t e)
{
    return e < 15435 ? 1024 - ((e * 68) >> 10) : e < 22050 ? 0 : clamp(((e - 22050) * 68) >> 10, 0, 1024);
}

/* audio ISR: a glitch, when the turn comes round (VINYL, GLCH): which (0: none) */
static uint32_t HOT_CODE gfx_roll(gfx_t *X, const int16_t *p, uint32_t n)
{
    uint32_t r;
    X->turn += n;
    if (X->turn < (uint32_t)GX_TURN)
        return 0;
    X->turn -= (uint32_t)GX_TURN;
    r = noise32(&X->rng);
    if (X->gk || (r >> 8) % 100u >= (uint32_t)clamp(p[GP_GLCH], 0, 100))
        return 0;
    return p[GP_GTYP] >= GT_ALL ? 1u + (r >> 4) % 3u : 1u + (uint32_t)clamp(p[GP_GTYP], 0, 2);
}

/* audio ISR: track t's VINYL glitch on its FLOW input x (n samples, in place): the reader of GRAIN's buffer (B, view
 * v: the last bars at 22.05 kHz, half level) crossfaded in, round a turn back or braking, and out again */
static void HOT_CODE gfx_glitch(gfx_t *X, const int16_t *p, const gr_buf_t *B, const tape_view_t *v, int32_t *x,
                                uint32_t n)
{
    int32_t len = (int32_t)v->len, i, back = GX_TURN / 2;   /* (a turn: half as many of the buffer's samples) */
    uint32_t g = gfx_roll(X, p, n);
    if (back > len - 4 * (int32_t)TAPE_BLK)
        back = len - 4 * (int32_t)TAPE_BLK;
    if (g && len && B->fill > back + 2 * (int32_t)TAPE_BLK && back > (int32_t)TAPE_BLK) {   /* (sound to go back to) */
        X->gk = (uint8_t)g;
        X->rd.wlo = -1;
        X->rd.inc = 1;
        X->ginc = 1024;                                 /* (Q12 buffer samples an output sample: its own speed) */
        X->gstart = B->w - (g == 1u + GT_BRAK ? 3 * (int32_t)TAPE_BLK : back);
        while (X->gstart < 0)
            X->gstart += len;
        X->gpos = X->gstart << 12;
        X->glen = g == 1u + GT_BRAK ? 37485 : 2 * back; /* (BRAK: 0.85 s, down, still, up) */
        X->gleft = X->glen;
        X->reps = (uint8_t)(g == 1u + GT_STCK ? 1u + (noise32(&X->rng) >> 8) % 3u : 0u);
    }
    if (!X->gk && !X->gx)
        return;
    for (i = 0; i < (int32_t)n; i++) {
        int32_t y;
        if (X->gk == 1u + GT_BRAK)                      /* (down to a stop, still, back up) */
            X->ginc = gfx_brake(X->glen - X->gleft);
        if (X->gk && --X->gleft <= 0) {                 /* (a pass done: round again (STCK) or out) */
            if (X->reps) {
                X->reps--;
                X->gpos = X->gstart << 12;
                X->gleft = X->glen;
            } else {
                X->gk = 0;
            }
        }
        X->gx += X->gk ? (X->gx < 32767 - 74 ? 74 : 32767 - X->gx) : (X->gx > 74 ? -74 : -X->gx);
        if (!X->gx)
            continue;
        while (X->gpos >= len << 12)
            X->gpos -= len << 12;
        y = gr_read(&X->rd, v, X->gpos) << 1;            /* (the buffer's half level, back to the sound's) */
        X->gpos += X->ginc;
        x[i] = x[i] + ((((y - x[i]) >> 1) * X->gx) >> 14);   /* (halved first: past full scale, x Q15 overflows) */
    }
}

/* the surface's next sample (VINYL): the hiss at NOIS (its top rolled off: about -31 dB at 100) and the crackle (a
 * dust tick ringing a few samples, sign flipping: crisp; a pop's short thump), the generator's state in locals */
#define GX_SURFACE(rng, nl, tick, pop, nois, ct, cp, y) do {                                                        \
        uint32_t rr_ = noise32(&(rng));                                                                         \
        (nl) += (((int32_t)rr_ >> 16) - (nl)) >> 2;                                                             \
        if (rr_ < (ct))                                                                                         \
            (tick) = (int32_t)((rr_ >> 3) & 1023u) + 200;                                                       \
        else if (rr_ > 0xFFFFFFFFu - (cp))                                                                      \
            (pop) = (int32_t)((rr_ >> 7) & 4095u) + 2500;                                                       \
        (tick) = -((tick) * 3) >> 3;                                                                            \
        (pop) = ((pop) * 7) >> 3;                                                                               \
        (y) += (((nl) * (nois)) >> 10) + (tick) + (pop);                                                        \
    } while (0)

/* audio ISR: x into the line at w and read back d (Q8) behind, interpolated (half level stored: the chain runs past
 * full scale); w moves on round size. d is under size: no divide a sample */
#define GX_LINE_RW(ln, size, w, x, d, y) do {                                                                   \
        uint32_t di_ = (uint32_t)((d) >> 8), r0_ = (w) >= di_ ? (w) - di_ : (w) + (size) - di_;                  \
        uint32_t r1_ = r0_ ? r0_ - 1u : (size) - 1u;                                                            \
        int32_t a_, b_;                                                                                         \
        (ln)[w] = (int16_t)clamp((x) >> 1, -32767, 32767);                                                      \
        a_ = (ln)[r0_];                                                                                         \
        b_ = (ln)[r1_];                                                                                         \
        (y) = (a_ + (((b_ - a_) * ((d) & 255)) >> 8)) << 1;                                                     \
    } while (0)

/* audio ISR: AGE or WEAR on y: the low-pass (a, Q15: Q13 inside, the chain runs past full scale; 32767: the sound as
 * it is), VINYL's high-pass (h), the curve's share (m, Q15: linear, so a modulated AGE ramps, it doesn't step). Both
 * filters keep their state in Q8: in Q0 a shift's dead band could leave one stuck off 0 (the high-pass up to 160
 * below), a DC offset on silence */
#define GX_AGE(lp, hp, a, h, m, vin, sat, y) do {                                                               \
        (lp) += (((((y) << 8) - (lp)) >> 8) * (((a) + 1) >> 2)) >> 5;                                           \
        (y) = ((lp) + 128) >> 8;                                                                                \
        if (vin) {                                                                                              \
            (hp) += (((((y) << 8) - (hp)) >> 8) * (h)) >> 7;                                                    \
            (y) -= ((hp) + 128) >> 8;                                                                           \
        }                                                                                                       \
        if (sat)                                                                                                \
            (y) += ((((softclip((y) >> 1) << 1) - (y)) * ((m) >> 3)) >> 12);                                    \
    } while (0)

/* audio ISR: track t's FLOW on one block (WARBLE or VINYL on the whole sound): the source (dry) through the line and
 * the form, added to l and r at WET (the dry share already there, grain_block). B, v: GRAIN's buffer, for VINYL's
 * glitches (v.len 0: none). The filters' and the surface's state in locals for the block (the line's stores can't
 * alias them then). About 100 host instructions a sample in the loop, 165 a sample with the block's setup (run 33) */
static void HOT_CODE gfx_flow(uint32_t t, const int16_t *p, const int32_t *dry, int32_t *l, int32_t *r, uint32_t n,
                              const gr_buf_t *B, const tape_view_t *v)
{
    gfx_t *X = &gfx[t];
    uint32_t form = gfx_form(p), ct, cp, i, w;
    int32_t x[CTL], d1, dd, d, sh[4], dsh[4], nois = clamp(p[GP_NOIS], 0, 100), wet = clamp(p[GP_WET], 0, 100);
    int32_t w1 = PCT_Q15[wet], w0 = X->wetq, dw, gw, surf, vin = form == GF_VINYL, sat;
    int32_t lp, hp, nl, tick, pop, rng;
    int16_t *ln;
    if (!X->nch || !wet) {                              /* (no line yet, or off: nothing of it) */
        X->wetq = 0;
        return;
    }
    if (!X->rng)
        X->rng = 0x6C8E9CF5 + (int32_t)t * 104729;
    ln = (int16_t *)(void *)mem_at(X->map);
    if (X->appl != GA_FLOW + 1u) {                      /* (its first block on FLOW: a silent line, the delay set) */
        X->appl = GA_FLOW + 1u;
        for (i = 0; i < GX_LINE; i++)
            ln[i] = 0;
        X->d0 = 440 << 8;
        X->w = 0;
    }
    for (i = 0; i < n; i++)
        x[i] = dry[i];
    if (vin && v->len)
        gfx_glitch(X, p, B, v, x, n);
    gfx_level(X, x, n, vin ? 4u : 1u);                  /* (WARBLE: the hits SWEL kicks on; VINYL: the needle down) */
    d1 = gfx_delay(X, p, form, 0, n);
    dd = (d1 - X->d0) >> CTL_LOG2;
    d = X->d0;
    gfx_shape(X, p, form, 0, CTL_LOG2, sh, dsh, &ct, &cp);
    surf = vin && (sys.playing || X->es > 200 || tp[t].src != SRC_TAPE);   /* (the needle down) */
    sat = sh[1] || dsh[1];
    dw = w1 == w0 ? 0 : (w1 - w0) >> CTL_LOG2;
    gw = w0;
    lp = X->lp[0], hp = X->hp[0], nl = X->nl[0], tick = X->tick, pop = X->pop, rng = X->rng, w = X->w;
    for (i = 0; i < n; i++) {
        int32_t y;
        d = i + 1u == n ? d1 : d + dd;
        GX_LINE_RW(ln, GX_LINE, w, x[i], d, y);
        w = w + 1u < GX_LINE ? w + 1u : 0u;
        sh[0] += dsh[0], sh[1] += dsh[1], sh[3] += dsh[3];
        GX_AGE(lp, hp, sh[0], sh[3], sh[1], vin, sat, y);
        if (surf)
            GX_SURFACE(rng, nl, tick, pop, nois, ct, cp, y);
        if (dw)
            gw = i + 1u == n ? w1 : gw + dw;
        y = (y * (gw >> 3)) >> 12;
        l[i] += y;
        r[i] += y;
    }
    X->lp[0] = lp, X->hp[0] = hp, X->nl[0] = nl, X->tick = tick, X->pop = pop, X->rng = rng, X->w = w;
    X->d0 = d1;
    X->wetq = w1;
}

/* audio ISR: track t's GRNS (WARBLE or VINYL on the grains): their half-rate mix xl xr (h samples), in place, before
 * WET, each side its own half of the line. Returns 1 while it has sound of its own to give (the line's tail, the
 * surface) */
static int HOT_CODE gfx_grns(uint32_t t, const int16_t *p, int32_t *xl, int32_t *xr, uint32_t h, int any)
{
    gfx_t *X = &gfx[t];
    uint32_t form = gfx_form(p), ct, cp, i, half = GX_LINE / 2u, w;
    int32_t d1, dd, d, sh[4], dsh[4], nois = clamp(p[GP_NOIS], 0, 100), surf, vin = form == GF_VINYL, sat;
    int32_t lpl, lpr, hpl, hpr, nll, nlr, tick, pop, rng;
    int16_t *ln;
    if (!X->nch)
        return 0;
    if (!X->rng)
        X->rng = 0x6C8E9CF5 + (int32_t)t * 104729;
    ln = (int16_t *)(void *)mem_at(X->map);
    if (X->appl != GA_GRNS + 1u) {
        X->appl = GA_GRNS + 1u;
        for (i = 0; i < GX_LINE; i++)
            ln[i] = 0;
        X->d0 = 220 << 8;
        X->w = 0;
    }
    if (!vin)
        gfx_level(X, xl, h, 1u);
    d1 = gfx_delay(X, p, form, 1, h);
    dd = h ? (d1 - X->d0) / (int32_t)h : 0;             /* (a block's divide) */
    d = X->d0;
    gfx_shape(X, p, form, 1, CTL_LOG2 - 1u, sh, dsh, &ct, &cp);   /* (h: half a block) */
    surf = vin && any;                                  /* (the needle on the grains: while they sound) */
    sat = sh[1] || dsh[1];
    lpl = X->lp[0], lpr = X->lp[1], hpl = X->hp[0], hpr = X->hp[1], nll = X->nl[0], nlr = X->nl[1];
    tick = X->tick, pop = X->pop, rng = X->rng, w = X->w;
    for (i = 0; i < h; i++) {
        int32_t yl, yr;
        d = i + 1u == h ? d1 : d + dd;
        GX_LINE_RW(ln, half, w, xl[i], d, yl);
        GX_LINE_RW(ln + half, half, w, xr[i], d, yr);
        w = w + 1u < half ? w + 1u : 0u;
        sh[0] += dsh[0], sh[1] += dsh[1], sh[3] += dsh[3];
        GX_AGE(lpl, hpl, sh[0], sh[3], sh[1], vin, sat, yl);
        GX_AGE(lpr, hpr, sh[0], sh[3], sh[1], vin, sat, yr);
        if (surf) {
            int32_t c = 0;                              /* (one needle: the crackle on both sides, the hiss each its
                                                         * own) */
            GX_SURFACE(rng, nll, tick, pop, nois, ct, cp, c);
            yl += c;
            nlr += (((int32_t)noise32(&rng) >> 16) - nlr) >> 2;
            yr += c - ((nll * nois) >> 10) + ((nlr * nois) >> 10);
        }
        xl[i] = yl;
        xr[i] = yr;
    }
    X->lp[0] = lpl, X->lp[1] = lpr, X->hp[0] = hpl, X->hp[1] = hpr, X->nl[0] = nll, X->nl[1] = nlr;
    X->tick = tick, X->pop = pop, X->rng = rng, X->w = w;
    X->d0 = d1;
    return 1;
}

/* audio ISR: GRNS's glitches (VINYL) on the grains' cursor (Q12, inside [ls, ls + ll)): moved, from `was`, by this
 * block; SKIP takes it back a turn, STCK keeps sending it back to where the groove stuck, BRAK slows its pace to a
 * stop and back */
static void HOT_CODE gfx_cursor(uint32_t t, const int16_t *p, int32_t *cur, int32_t was, int32_t ls, int32_t ll,
                                uint32_t n)
{
    gfx_t *X = &gfx[t];
    int32_t back = GX_TURN / 2 < ll / 2 ? GX_TURN / 2 : ll / 2;
    uint32_t g = gfx_roll(X, p, n);
    if (g) {
        X->gk = (uint8_t)g;
        X->glen = g == 1u + GT_BRAK ? 37485 : g == 1u + GT_STCK ? 2 * back : 0;
        X->gleft = X->glen;
        X->reps = (uint8_t)(g == 1u + GT_STCK ? 1u + (noise32(&X->rng) >> 8) % 3u : 0u);
        X->gcur = *cur - (back << 12);
        if (g == 1u + GT_SKIP) {                        /* (back a groove, at once) */
            *cur = X->gcur;
            X->gk = 0;
        }
    } else if (X->gk == 1u + GT_STCK) {                 /* (from the block after it starts) */
        X->gleft -= (int32_t)n;
        if (X->gleft <= 0) {
            if (X->reps) {
                X->reps--;
                *cur = X->gcur;
                X->gleft = X->glen;
            } else {
                X->gk = 0;
            }
        }
    } else if (X->gk == 1u + GT_BRAK) {                 /* (its pace: down to still and back) */
        *cur = was + (((*cur - was) * gfx_brake(X->glen - X->gleft)) >> 10);
        X->gleft -= (int32_t)n;
        if (X->gleft <= 0)
            X->gk = 0;
    }
    while (*cur < ls << 12)
        *cur += ll << 12;
    while (*cur >= (ls + ll) << 12)
        *cur -= ll << 12;
}

/* ------------------------------------------------------ main loop side --- */
/* every pass: each track's line while WARBLE or VINYL is on (WET up, modulated or not), none otherwise */
static void gfx_poll(void)
{
    uint32_t t;
    for (t = 0; t < NTRK; t++) {
        gfx_t *X = &gfx[t];
        int on = trk_live(t) && gfx_form(tp[t].dev[DEV_GRAIN]) != GF_GRAIN && mod_peak_dev(t, DEV_GRAIN, GP_WET) > 0;
        if (on && !X->nch) {
            int32_t c = tape_alloc(MEM_GFX + t, NTRK, 1);
            if (c >= 0) {
                X->map = (uint8_t)c;
                X->appl = 0;                            /* (a fresh line: silent before it's read) */
                RING_PUBLISH();
                X->nch = 1;
            }
        } else if (!on && X->nch) {
            X->nch = 0;
            RING_PUBLISH();
            mem_free(X->map);
        }
    }
}
