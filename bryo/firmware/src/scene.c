/* SPDX-License-Identifier: GPL-3.0-only */
/* Bryo: scenes (2026-10-13). Four a project: each a snapshot of the sound, so a performance can jump between sounds
 * or morph from one to another while the same material plays.
 *
 * What a scene holds: each track's source (TAPE SYNTH POLY DRUM) and every knob of its sound (mod.c's numbering of
 * the knobs: the devices, every source's knobs, the channel strip, the DRUM instruments), each track's level, and
 * the rooms. Not the tapes and reels, the patterns, steps, modulation depths, mutes, arpeggios, the tempo or the
 * key: a scene is how it sounds, the playing stays the playing.
 *
 * Between two scenes, track by track: the same source, and its knobs morph (every value on the way; a choice that
 * can't be halfway, a form, ROUT, a filter's TYPE, switches at the midpoint, each softened as when it's turned by
 * hand). Another source: no morph, the track fades out over the first half, switches whole while silent, and the
 * scene's sound fades in over the second ("the same track type would be on each scene. If they aren't the same we
 * don't morph, but the first scene fades out and the next scene fades in"). The fades go through the track's level,
 * which the ISR ramps block to block (chain.c rt->gv).
 *
 * Gestures (GLO held: the global pad; its white keys 1-4 pick the track, 5-8 are the scenes):
 *   key 5-8 tapped     recall: SCN_GLIDE_MS from where it is to the scene (a jump of every knob at once would
 *                      click; a glide is a fast hand on all of them)
 *   key held + SELECT  morph: from the sound as it was at the first detent toward the scene, 5 % a detent; let go
 *                      anywhere and the sound stays there
 *   key held + SAVE    store the sound as it is into the scene
 *
 * Memory: a scene in RAM is whole (2.1 KB: four of them and the morph's start, 10.6 KB of .bss); in a project it's
 * only what differs from the project's own (project.c), so scenes cost the save what they change. Main loop only:
 * the ISR reads the knobs as it always does. */

#define NSCN 4u
#define SCN_GLIDE_MS 300u            /* a recall's glide */
#define SCN_FULL 1000                /* the way from one to the other */
#define SCN_STEP 50                  /* a SELECT detent of the morph (5 %) */

typedef struct {
    uint8_t used;
    uint8_t src[NTRK];               /* the tracks' sources */
    uint8_t lvl[NTRK];               /* .. levels */
    int16_t v[NTRK][MOD_NTGT];       /* every knob, by mod.c's numbering (scn_kind 0: not kept, 0) */
    int16_t rm[NROOM][NRM];          /* the rooms' */
} scene_t;
static scene_t scn[NSCN];
static scene_t scn_from;             /* the morph's (or the glide's) start */
static uint32_t scn_ver;             /* bumped at each store or clear (the project's fingerprint: project.c) */
static uint8_t scn_cur;              /* the scene last recalled, stored or morphed to the end, 1..NSCN (0: none) */
static struct {
    uint8_t on;                      /* 1: a recall's glide, 2: a morph by hand */
    uint8_t to;                      /* the scene, 0..NSCN-1 */
    int32_t pos;                     /* 0..SCN_FULL along the way */
    uint32_t t0;                     /* the glide's start, fm1_ms */
} scm;

/* knob g's descriptor (mod.c's numbering, whether it's a target or not; 0: none) */
static const pdesc_t *scn_desc(uint32_t g)
{
    uint32_t k = g % NPK, s = (g - MOD_TSRC) / NPK;
    if (g < MOD_TSRC)
        return &DEV_P[1u + g / NPK][k];
    if (g < MOD_TCH)
        return s == SRC_TAPE ? &DEV_P[DEV_SRC][k] : s == SRC_SYNTH ? &SYN_P[k] : s == SRC_POLY ? &POL_P[k] : &DRM_P[k];
    if (g < MOD_TDIN)
        return &CH_P[g - MOD_TCH];
    return g < MOD_NTGT ? &DRI_P[(g - MOD_TDIN) % NDIN] : 0;
}

/* what a scene does with knob g: 0 not its (none there; the reels, the material; RIPL ARP, an arpeggio; SPACE's
 * places for its room's knobs, never read; DRUM's pattern and editing choices), 1 a value that morphs, 2 a choice
 * that switches at the midpoint */
static uint8_t scn_kinds[MOD_NTGT];
static uint32_t scn_kind(uint32_t g)
{
    static uint8_t ready;
    if (!ready) {
        uint32_t i;
        for (i = 0; i < MOD_NTGT; i++) {
            const pdesc_t *d = scn_desc(i);
            uint32_t k = i % NPK;
            uint8_t v = !d || pdesc_empty(d) || d->names == N_REEL ? 0 : d->nomod ? 2 : 1;
            if (i >= MOD_TG(DEV_RESO, RP_AMODE) && i <= MOD_TG(DEV_RESO, RP_ARAT))
                v = 0;
            if (i >= MOD_TG(DEV_SPACE, 0) && i < MOD_TG(DEV_SPACE, 0) + NPK && SP_RMK[k])
                v = 0;
            if (i >= MOD_TSRC + SRC_DRUM * NPK && i < MOD_TSRC + (SRC_DRUM + 1u) * NPK && d && d->nomod)
                v = 0;
            if (i == MOD_TSRC + SRC_SYNTH * NPK + SY_WAVE && v)
                v = 2;                                  /* (SYNTH's WAVE a choice here: VOX to SAW isn't through FM
                                                         * and PD, 2026-10-14) */
            scn_kinds[i] = v;
        }
        ready = 1;
    }
    return g < MOD_NTGT ? scn_kinds[g] : 0u;
}

/* knob g on track t */
static int16_t *scn_knob(uint32_t t, uint32_t g)
{
    uint32_t k, a = mod_tarr(g, &k);
    return &mod_base(t, a)[k];
}

/* the sound as it is, into s */
static void scn_take(scene_t *s)
{
    uint32_t t, g, i, k;
    for (t = 0; t < NTRK; t++) {
        s->src[t] = tp[t].src;
        s->lvl[t] = track[t].level;
        for (g = 0; g < MOD_NTGT; g++)
            s->v[t][g] = scn_kind(g) ? *scn_knob(t, g) : 0;
    }
    for (i = 0; i < NROOM; i++)
        for (k = 0; k < NRM; k++)
            s->rm[i][k] = rmp[i][k];
}

/* the sound at pos (0..SCN_FULL) of the way from a to b */
static void scn_mix(const scene_t *a, const scene_t *b, int32_t pos)
{
    uint32_t t, g, i, k;
    int half = pos >= SCN_FULL / 2;
    for (t = 0; t < NTRK; t++) {
        if (a->src[t] != b->src[t]) {                    /* another source: out, switched whole, in */
            const scene_t *w = half ? b : a;
            int32_t f = half ? pos - SCN_FULL / 2 : SCN_FULL / 2 - pos;
            tp[t].src = w->src[t];
            for (g = 0; g < MOD_NTGT; g++)
                if (scn_kind(g))
                    *scn_knob(t, g) = w->v[t][g];
            track[t].level = (uint8_t)((w->lvl[t] * f + SCN_FULL / 4) / (SCN_FULL / 2));
            continue;
        }
        {
            int32_t x = a->lvl[t], y = b->lvl[t];
            track[t].level = (uint8_t)(x + ((y - x) * pos + (y > x ? SCN_FULL / 2 : -SCN_FULL / 2)) / SCN_FULL);
        }
        for (g = 0; g < MOD_NTGT; g++) {
            uint32_t kd = scn_kind(g);
            int32_t x = a->v[t][g], y = b->v[t][g];
            if (!kd)
                continue;
            *scn_knob(t, g) = (int16_t)(kd == 2 || x == y ? (half ? y : x)
                                        : x + ((y - x) * pos + (y > x ? SCN_FULL / 2 : -SCN_FULL / 2)) / SCN_FULL);
        }
    }
    for (i = 0; i < NROOM; i++)
        for (k = 0; k < NRM; k++)
            rmp[i][k] = (int16_t)(RM_P[k].nomod ? (half ? b->rm[i][k] : a->rm[i][k])
                                                : a->rm[i][k] + ((b->rm[i][k] - a->rm[i][k]) * pos) / SCN_FULL);
}

/* every scene empty (a new project, a load before its own are read) */
static void scn_clear(void)
{
    uint32_t n;
    for (n = 0; n < NSCN; n++)
        scn[n].used = 0;
    scm.on = 0;
    scn_cur = 0;
    scn_ver++;
}

/* scene n (0..NSCN-1): the knobs as they are */
static void scn_store(uint32_t n)
{
    if (n >= NSCN)
        return;
    scm.on = 0;
    scn_take(&scn[n]);
    scn[n].used = 1;
    scn_cur = (uint8_t)(n + 1u);
    scn_ver++;
}

/* scene n recalled: the glide toward it starts (0, or -1: nothing stored there) */
static int scn_recall(uint32_t n)
{
    if (n >= NSCN || !scn[n].used)
        return -1;
    scn_take(&scn_from);
    scm.on = 1;
    scm.to = (uint8_t)n;
    scm.pos = 0;
    scm.t0 = fm1_ms;
    scn_cur = (uint8_t)(n + 1u);
    return 0;
}

/* SELECT turned d detents with scene n's key held: the morph toward it (from where the knobs were at its first
 * detent); its position now, 0..100 %, or -1 (nothing stored) */
static int32_t scn_morph(uint32_t n, int32_t d)
{
    if (n >= NSCN || !scn[n].used)
        return -1;
    if (scm.on != 2 || scm.to != n) {                    /* (a new morph: from here) */
        scn_take(&scn_from);
        scm.on = 2;
        scm.to = (uint8_t)n;
        scm.pos = 0;
    }
    scm.pos = clamp(scm.pos + d * SCN_STEP, 0, SCN_FULL);
    scn_mix(&scn_from, &scn[n], scm.pos);
    if (scm.pos == SCN_FULL)
        scn_cur = (uint8_t)(n + 1u);
    return (scm.pos * 100 + SCN_FULL / 2) / SCN_FULL;
}

/* the morph's hand let go (the scene's key): the sound stays where it is; a turn after starts a new one */
static void scn_morph_end(void)
{
    if (scm.on == 2)
        scm.on = 0;
}

/* main loop, every pass: a recall's glide */
static void scn_poll(void)
{
    uint32_t e;
    if (scm.on != 1)
        return;
    e = fm1_ms - scm.t0;
    scm.pos = e >= SCN_GLIDE_MS ? SCN_FULL : (int32_t)(e * SCN_FULL / SCN_GLIDE_MS);
    scn_mix(&scn_from, &scn[scm.to], scm.pos);
    if (scm.pos == SCN_FULL)
        scm.on = 0;
}
