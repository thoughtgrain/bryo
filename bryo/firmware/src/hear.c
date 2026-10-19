/* SPDX-License-Identifier: GPL-3.0-only */
/* Bryo: the hearing floor (2026-10-19; docs/bryo-architecture.md, "The hearing floor").
 *
 * "Could we do some tricky optimizations where the quietest sounds just drop completely when cpu and ram gets
 * overloaded?" Every part that ends its own sound already had a point where it stopped: a voice's envelope under
 * -72 dB of its peak, a drum under -60 dB, the strings and the delay resting on exact silence. Those points were
 * fixed, set against the sound itself. Here they're set against what's heard: a sound under the floor at the output
 * (-72 dBFS, HEAR_BASE) is dropped, wherever it is, after the track's level, MASTER and the master compressor. A
 * release on a track turned down 20 dB ends 20 dB sooner; one at full level ends where it always did.
 *
 * And under load the floor rises: when the audio runs short of time (chain.c chain_shed) the floor goes up 6 dB a
 * time, to -48 dBFS, before anything else is shed, so the quietest sounds anywhere in the mix drop first: tails,
 * faint grains, a delay's last echoes. Only past that are the grains, strings and drum voices capped, as before. A
 * calm second with room takes it down a step (hear_recover).
 *
 * The floor is worked out once a block (chain.c chain_hear) into each part's terms: hear_fl[t] inside track t
 * (where its sources and devices are, before its level), hear_rm for the rooms (after the tracks' levels). A track
 * being recorded is heard before its level (REC takes it there), and what other tracks record hears it before MASTER:
 * with REC armed anywhere, MASTER and the compressor don't count, and the armed track's own level doesn't either.
 * Audio ISR only. Integer only. */

#define HEAR_BASE 8                  /* the floor at the output, Q15: -72.2 dBFS */
#define HEAR_STEPS 4u                /* under load it rises 6 dB a step: -66 -60 -54 -48 */
#define HEAR_DEAF 32767              /* a floor: nothing of it is heard (its level, or MASTER, at 0) */
static uint32_t hear_step;           /* the steps it's risen by now (chain_shed raises it, hear_recover lowers it) */
static uint32_t hear_calm;           /* blocks since the last rise */
static int32_t hear_fl[NTRK] = {HEAR_BASE, HEAR_BASE, HEAR_BASE, HEAR_BASE};   /* track t's floor, Q15 amplitude */
static int32_t hear_rm = HEAR_BASE;  /* the rooms' */

/* the floor at the output now, Q15 */
static inline __attribute__((always_inline)) int32_t hear_thr(void) { return HEAR_BASE << hear_step; }

/* a floor f seen through a gain g (Q12): what's under f x 4096 / g there is under f after it (g 0: never heard) */
static int32_t hear_through(int32_t f, int32_t g)
{
    if (g <= 0)
        return HEAR_DEAF;
    f = (int32_t)(((uint32_t)f << 12) / (uint32_t)g);   /* (f under 2^15: no overflow) */
    return f < HEAR_DEAF ? f : HEAR_DEAF;
}

/* track t's floor for a source at LVL lv (Q10, 1024: 0 dB: chain.c applies it after the source). The last one
 * worked out a track is kept: a divide only when the floor or LVL moved */
static int32_t hear_src(uint32_t t, int32_t lv)
{
    static int32_t kf[NTRK], kl[NTRK], kr[NTRK];
    int32_t f = hear_fl[t];
    if (lv == 1024 || f >= HEAR_DEAF)
        return f;
    if (f != kf[t] || lv != kl[t]) {
        kf[t] = f;
        kl[t] = lv;
        kr[t] = hear_through(f, lv << 2);
    }
    return kr[t];
}

/* an envelope's end, Q24 (SYNTH, POLY: dsp.c env_step), for a voice that peaks at full scale under a floor f: it
 * ends where the envelope takes it under f. At the floor's base and 0 dB, 1 << 12: where it always ended */
static inline __attribute__((always_inline)) int32_t hear_end24(int32_t f)
{
    return f >= 1 << 14 ? 1 << 23 : (f < HEAR_BASE ? HEAR_BASE : f) << 9;   /* (never past half: a deaf track's
                                                                              * release still ramps out) */
}

/* the lowest stored block peak (tape.c: |sample| >> 7, a byte) that may be heard through a gain g (Q15) under floor
 * f: a block's sound is at most (peak + 1) x 128, x 2 for the half level a GRAIN buffer stores (grain.c gr_quiet) */
static uint32_t hear_peak(int32_t f, int32_t g)
{
    uint32_t p;
    if (g <= 0 || f >= HEAR_DEAF)
        return 256u;                                    /* (no block is heard) */
    p = ((uint32_t)f << 7) / (uint32_t)g;               /* (f x 32768 / (g x 256): heard from peak p - 1 up, the
                                                         * divide rounding down keeps it on the side of heard) */
    return p > 2u ? (p > 257u ? 256u : p - 1u) : 1u;    /* (1 at the least: a peak of 0 is silence, as it was) */
}

/* Past the floor's top, the caps (grains, strings, drum voices) take from the quietest track first: each track's
 * peak as heard, after its level (chain.c's meter: the same for every track past that, MASTER and the compressor
 * being the mix's) */
static uint8_t shed_rush;            /* ISR: blocks a rush's short fades still apply to (chain.c's load guard) */
static int32_t hear_pk[NTRK];        /* ISR: each track's peak last block, after its level (chain.c chain_block) */

/* the quietest track of those in mask (-1: none) */
static int32_t hear_quietest(uint32_t mask)
{
    int32_t t, q = -1;
    for (t = 0; t < (int32_t)NTRK; t++)
        if (((mask >> t) & 1u) && (q < 0 || hear_pk[t] < hear_pk[q]))
            q = t;
    return q;
}

/* the loudest track of those in mask (-1: none): the first a cap comes back to */
static int32_t hear_loudest(uint32_t mask)
{
    int32_t t, q = -1;
    for (t = 0; t < (int32_t)NTRK; t++)
        if (((mask >> t) & 1u) && (q < 0 || hear_pk[t] > hear_pk[q]))
            q = t;
    return q;
}
